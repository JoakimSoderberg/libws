
#include "libws_config.h"

#include "libws_types.h"
#include "libws_log.h"
#include "libws_handshake.h"
#include "libws_private.h"
#include "libws_base64.h"
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <assert.h>

int _ws_generate_handshake_key(ws_t ws)
{
	char rand_key[16];
	int key_len;
	assert(ws);

	if (ws->handshake_key_base64)
		free(ws->handshake_key_base64);

	// Randomize 16 bytes and base64 encode them for the
	// Sec-WebSocket-Key field.
	if (_ws_get_random_mask(ws, rand_key, sizeof(rand_key)))
	{
		LIBWS_LOG(LIBWS_ERR, "Failed to get random byte sequence "
							 "for Websocket upgrade handshake key");
		return -1;
	}

	ws->handshake_key_base64 = libws_base64((const void *)rand_key, sizeof(rand_key), &key_len);

	if (!ws->handshake_key_base64)
	{
		LIBWS_LOG(LIBWS_ERR, "Failed to base64 encode websocket key");
		return -1;
	}

	return 0;
}

int _ws_send_http_upgrade(ws_t ws)
{
	struct evbuffer *out = NULL;
	size_t key_len = 0;
	size_t i;
	assert(ws);
	assert(ws->bev);

	if (!ws->server)
	{
		return -1;
	}

	out = bufferevent_get_output(ws->bev);

	if (_ws_generate_handshake_key(ws))
	{
		LIBWS_LOG(LIBWS_ERR, "Failed to send HTTP upgrade handshake");
		return -1;
	}

	evbuffer_add_printf(out,
		"GET /%s HTTP/1.1\r\n"
		"Host: %s:%d\r\n"
		"Connection: Upgrade\r\n"
		"Upgrade: websocket\r\n"
		"Sec-Websocket-Version: 13\r\n"
		"Sec-WebSocket-Key: %s\r\n",
		(ws->uri ? ws->uri : ""),
		ws->server,
		ws->port,
		ws->handshake_key_base64);

	if (ws->origin)
	{
		evbuffer_add_printf(out, "Origin: %s\r\n", ws->origin);
	}

	if (ws->num_subprotocols > 0)
	{
		evbuffer_add_printf(out, "Sec-WebSocket-Protocol: %s", 
			ws->subprotocols[0]);

		for (i = 1; i < ws->num_subprotocols; i++)
		{
			evbuffer_add_printf(out, ", %s", ws->subprotocols[i]);
		}
	}

	// TODO: Sec-WebSocket-Extensions

	// TODO: Add custom headers.

	return 0;
}

int _ws_parse_http_header(const char *line, char **header_name, 
						char **header_val)
{
	int ret = 0;
	const char *start = line;
	char *end;
	size_t len;

	assert(header_name);
	assert(header_val);

	*header_name = NULL;
	*header_val = NULL;

	if (!line)
		return -1;

	// TODO: Replace with strsep instead.
	if (!(end = strchr(line, ':')))
		return -1;

	len = (end - start) * sizeof(char);
	*header_name = (char *)malloc(len + 1);

	if (!(*header_name))
		return -1;

	memcpy(*header_name, line, len);
	(*header_name)[len] = '\0';

	end++;
	// TODO: Replace with strspn instead end += strspn(end, " \t");
	while ((*end == ' ') || (*end == '\t')) end++;
	*header_val = strdup(end);

	// TODO: Trim the right side of header value as well.

	if (!(*header_val))
		goto fail;

	return ret;
fail:
	if (*header_name)
	{
		free(*header_name);
		*header_name = NULL;
	}

	return ret;
}

int _ws_parse_http_status(const char *line, 
						int *http_major_version, int *http_minor_version,
						int *status_code)
{
	const char *s = line;
	int n;

	*status_code = -1;
	*http_major_version = -1;
	*http_minor_version = -1;

	if (!line)
		return -1;

	n = sscanf(line, "HTTP/%d.%d %d", 
		http_major_version, http_minor_version, status_code);

	if (n != 3)
		return -1;

	return 0;
}

ws_parse_state_t _ws_read_http_status(ws_t ws, 
				struct evbuffer *in, 
				int *http_major_version, 
				int *http_minor_version,
				int *status_code)
{
	char *line = NULL;
	size_t len;
	assert(ws);
	assert(in);

	line = evbuffer_readln(in, &len, EVBUFFER_EOL_CRLF);

	if (!line)
		return WS_PARSE_STATE_NEED_MORE;

	if (_ws_parse_http_status(line, 
		http_major_version, http_minor_version, status_code))
	{
		free(line);
		return WS_PARSE_STATE_ERROR;
	}

	free(line);
	return WS_PARSE_STATE_SUCCESS;
}

ws_parse_state_t _ws_read_http_headers(ws_t ws, struct evbuffer *in)
{
	char *line;
	char *header_name;
	char *header_val;
	size_t len;
	assert(ws);
	assert(in);

	// TODO: Set a max header size to allow.

	while ((line = evbuffer_readln(in, &len, EVBUFFER_EOL_CRLF)) != NULL)
	{
		// Check for end of HTTP response (empty line).
		if (*line == '\0')
		{
			return WS_PARSE_STATE_SUCCESS;
		}

		if (_ws_parse_http_header(line, &header_name, &header_val))
		{
			LIBWS_LOG(LIBWS_ERR, "Failed to parse HTTP upgrade "
								 "repsonse line: %s", line);
			return WS_PARSE_STATE_ERROR;
		}
		
		// Let the user get the header.
		if (ws->header_cb)
		{
			if (ws->header_cb(ws, header_name, header_val, ws->header_arg))
			{
				LIBWS_LOG(LIBWS_DEBUG, "User header callback cancelled handshake");
				return WS_PARSE_STATE_USER_ABORT;
			}
		}



		free(line);
		line = NULL;
	}

	return WS_PARSE_STATE_NEED_MORE;
}



ws_parse_state_t _ws_read_http_upgrade_response(ws_t ws)
{
	struct evbuffer *in;
	size_t len;
	char *line = NULL;
	char *header_name = NULL;
	char *header_val = NULL;
	int major_version;
	int minor_version;
	int status_code;
	ws_parse_state_t parse_state;
	assert(ws);
	assert(ws->bev);

	in = bufferevent_get_input(ws->bev);

	switch (ws->connect_state)
	{
		default: 
		{
			LIBWS_LOG(LIBWS_ERR, "Incorrect connect state in HTTP upgrade "
								 "response handler");
			return WS_PARSE_STATE_ERROR; 
		}
		case WS_CONNECT_STATE_SENT_REQ:
		{
			// Parse status line HTTP/1.1 200 OK...
			if ((parse_state = _ws_read_http_status(ws, in, 
							&major_version, &minor_version, &status_code)) 
							!= WS_PARSE_STATE_SUCCESS)
			{
				return parse_state;
			}

			ws->connect_state = WS_CONNECT_STATE_PARSED_STATUS;
			// Fall through.
		} 
		case WS_CONNECT_STATE_PARSED_STATUS:
		{
			// Read the HTTP headers.
			if ((parse_state = _ws_read_http_headers(ws, in)) 
				!= WS_PARSE_STATE_SUCCESS)
			{
				return parse_state;
			}

			ws->connect_state = WS_CONNECT_STATE_PARSED_HEADERS;
			// Fall through.
		}
		case WS_CONNECT_STATE_PARSED_HEADERS:
		{

		}
	}

	

	// TODO: Verify HTTP version / status. Redirect for instance?

	// Read all headers.
	

	// 1.  If the status code received from the server is not 101, the
    //    client handles the response per HTTP [RFC2616] procedures.  In
    //    particular, the client might perform authentication if it
    //    receives a 401 status code; the server might redirect the client
    //    using a 3xx status code (but clients are not required to follow
    //    them), etc.  Otherwise, proceed as follows.

	// 2. If the response lacks an |Upgrade| header field or the |Upgrade|
	//    header field contains a value that is not an ASCII case-
	//    insensitive match for the value "websocket", the client MUST
	//    _Fail the WebSocket Connection_.

	// 3. If the response lacks a |Connection| header field or the
	//    |Connection| header field doesn't contain a token that is an
	//    ASCII case-insensitive match for the value "Upgrade", the client
	//    MUST _Fail the WebSocket Connection_.

	// 4. If the response lacks a |Sec-WebSocket-Accept| header field or
	//    the |Sec-WebSocket-Accept| contains a value other than the
	//    base64-encoded SHA-1 of the concatenation of the |Sec-WebSocket-
	//    Key| (as a string, not base64-decoded) with the string "258EAFA5-
	//    E914-47DA-95CA-C5AB0DC85B11" but ignoring any leading and
	//    trailing whitespace, the client MUST _Fail the WebSocket
	//    Connection_.

	// 5.  If the response includes a |Sec-WebSocket-Extensions| header
	//    field and this header field indicates the use of an extension
	//    that was not present in the client's handshake (the server has
	//    indicated an extension not requested by the client), the client
	//    MUST _Fail the WebSocket Connection_.  (The parsing of this
	//    header field to determine which extensions are requested is
	//    discussed in Section 9.1.)

	// 6. If the response includes a |Sec-WebSocket-Protocol| header field
	//    and this header field indicates the use of a subprotocol that was
	//    not present in the client's handshake (the server has indicated a
	//    subprotocol not requested by the client), the client MUST _Fail
	//    the WebSocket Connection_.

	return WS_PARSE_STATE_SUCCESS;
fail:
	return WS_PARSE_STATE_ERROR;
}


