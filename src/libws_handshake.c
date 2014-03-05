
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
	{
		_ws_free(ws->handshake_key_base64);
		ws->handshake_key_base64 = NULL;
	}

	// Randomize 16 bytes and base64 encode them for the
	// Sec-WebSocket-Key field.
	if (_ws_get_random_mask(ws, rand_key, sizeof(rand_key)) < 0)
	{
		LIBWS_LOG(LIBWS_ERR, "Failed to get random byte sequence "
							 "for Websocket upgrade handshake key");
		return -1;
	}

	ws->handshake_key_base64 = libws_base64((const void *)rand_key, 
											sizeof(rand_key), &key_len);

	if (!ws->handshake_key_base64)
	{
		LIBWS_LOG(LIBWS_ERR, "Failed to base64 encode websocket key");
		return -1;
	}

	return 0;
}

int _ws_send_handshake(ws_t ws, struct evbuffer *out)
{
	size_t key_len = 0;
	size_t i;
	assert(ws);
	assert(out);

	LIBWS_LOG(LIBWS_DEBUG, "Start sending websocket handshake");

	if (!ws->server)
	{
		return -1;
	}

	LIBWS_LOG(LIBWS_DEBUG, "Generate handshake key");

	if (_ws_generate_handshake_key(ws))
	{
		LIBWS_LOG(LIBWS_ERR, "Failed to generate handshake key");
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
	
	evbuffer_add_printf(out, "\r\n");

	ws->connect_state = WS_CONNECT_STATE_SENT_REQ;

	LIBWS_LOG(LIBWS_DEBUG, "%s", evbuffer_pullup(out, evbuffer_get_length(out)));

	// TODO: Sec-WebSocket-Extensions

	// TODO: Add custom headers.

	return 0;
}

int _ws_parse_http_header(char *line, char **header_name, 
						char **header_val)
{
	int ret = 0;
 	char *start = line;
 	char *end;
 	size_t len;

	assert(header_name);
	assert(header_val);

	*header_name = NULL;
	*header_val = NULL;

	if (!line)
		return -1;

	if (!(end = strchr(line, ':')))
		return -1;

	len = (end - start) * sizeof(char);
	*header_name = (char *)_ws_malloc(len + 1);

	if (!(*header_name))
		return -1;

	memcpy(*header_name, line, len);
	(*header_name)[len] = '\0';

	end++;
	*header_val = _ws_strdup(end + strspn(end, " \t"));

	if (!(*header_val))
	{
		ret = -1;
		goto fail;
	}

	ws_rtrim(*header_val);

	return ret;
fail:
	if (*header_name)
	{
		_ws_free(*header_name);
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
		_ws_free(line);
		return WS_PARSE_STATE_ERROR;
	}

	_ws_free(line);
	return WS_PARSE_STATE_SUCCESS;
}

static int _ws_validate_http_header(ws_t ws, ws_http_header_flags_t flag,
				const char *name, const char *val, 
				const char *expected_name, const char *expected_val,
				int must_appear_only_once)
{
	assert(ws);

	if (!strcasecmp(expected_name, name))
	{
		if (must_appear_only_once && (ws->http_header_flags & flag))
		{
			LIBWS_LOG(LIBWS_ERR, "%s must only appear once", 
								expected_name);
			return -1;
		}

		if (strcasecmp(expected_val, val))
		{
			LIBWS_LOG(LIBWS_ERR, "%s header must contain \"%s\" "
								"but got \"%s\"", name, expected_val, val);
			return -1;
		}

		ws->http_header_flags |= flag;
	}

	return 0;
}

int _ws_check_server_protocol_list(ws_t ws, const char *val)
{
	int ret = 0;
	size_t i;
	char *s = _ws_strdup(val);
	char *v = s;
	char *prot = NULL;
	char *end = NULL;
	int found = 0;
	
	while ((prot = libws_strsep(&v, ",")) != NULL)
	{
		// Trim start.
		prot += strspn(prot, " ");
		
		// Trim end.
		ws_rtrim(prot);

		found = 0;

		for (i = 0; i < ws->num_subprotocols; i++)
		{
			if (!strcasecmp(ws->subprotocols[i], prot))
			{
				// TODO: Add subprotocol to negotiated list of sub protocols.
				// TODO: Maybe give the user a list of these in the connection callback?
				found = 1;
			}
		}

		if (!found)
		{
			ret = -1;
			break;
		}
	}

	_ws_free(s);

	return ret;
}

int _ws_calculate_key_hash(const char *handshake_key_base64, 
							char *key_hash, size_t len)
{
	char key_hash_sha1[20]; // SHA1, 160 bits = 20 bytes.
	char *key_hash_sha1_base64;
	int base64_len;
	char accept_key[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	strcpy(key_hash, handshake_key_base64);
	strcat(key_hash, accept_key);
	
	SHA1((unsigned char *)key_hash, 
		strlen(key_hash), (unsigned char *)key_hash_sha1);

	key_hash_sha1_base64 = libws_base64((const void *)key_hash_sha1, 
									sizeof(key_hash_sha1), &base64_len);

	if (!key_hash_sha1_base64)
		return -1;

	strcpy(key_hash, key_hash_sha1_base64);
	free(key_hash_sha1_base64);

	return 0;
}

///
/// Validates that a HTTP header is correct.
///
/// @param[in]	ws 		The websocket session context.
/// @param[in]	name 	Header name.
/// @param[in]	val 	Header value.
///
/// @returns If a required header has an incorrect value -1 is returned
///			 which means that the connection must fail.
///
int _ws_validate_http_headers(ws_t ws, const char *name, const char *val)
{
	assert(ws);

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
	if (_ws_validate_http_header(ws, WS_HAS_VALID_UPGRADE_HEADER, name, val,
							"Upgrade", "websocket", 1))
	{
		return -1;
	}

	// 3. If the response lacks a |Connection| header field or the
	//    |Connection| header field doesn't contain a token that is an
	//    ASCII case-insensitive match for the value "Upgrade", the client
	//    MUST _Fail the WebSocket Connection_.
	if (_ws_validate_http_header(ws, WS_HAS_VALID_CONNECTION_HEADER, name, val,
							"Connection", "upgrade", 1))
	{
		return -1;
	}

	// 4. If the response lacks a |Sec-WebSocket-Accept| header field or
	//    the |Sec-WebSocket-Accept| contains a value other than the
	//    base64-encoded SHA-1 of the concatenation of the |Sec-WebSocket-
	//    Key| (as a string, not base64-decoded) with the string "258EAFA5-
	//    E914-47DA-95CA-C5AB0DC85B11" but ignoring any leading and
	//    trailing whitespace, the client MUST _Fail the WebSocket
	//    Connection_.
	{
		char key_hash[256];

		if (_ws_calculate_key_hash(ws->handshake_key_base64, 
								key_hash, sizeof(key_hash)))
		{
			return -1;
		}

		if (_ws_validate_http_header(ws, WS_HAS_VALID_WS_ACCEPT_HEADER, 
								name, val, "Sec-WebSocket-Accept", key_hash, 1))
		{
			return -1;
		}
	}

	// 5.  If the response includes a |Sec-WebSocket-Extensions| header
	//    field and this header field indicates the use of an extension
	//    that was not present in the client's handshake (the server has
	//    indicated an extension not requested by the client), the client
	//    MUST _Fail the WebSocket Connection_.  (The parsing of this
	//    header field to determine which extensions are requested is
	//    discussed in Section 9.1.)
	//
	// Note that like other HTTP header fields, this header field MAY be
    // split or combined across multiple lines.  Ergo, the following are
    // equivalent:
    // 
    //      Sec-WebSocket-Extensions: foo
    //      Sec-WebSocket-Extensions: bar; baz=2
    // 
    // is exactly equivalent to
	// 
    //      Sec-WebSocket-Extensions: foo, bar; baz=2
    // 
    // Note that the order of extensions is significant.  Any interactions
   	// between multiple extensions MAY be defined in the documents defining
   	// the extensions.  In the absence of such definitions, the
   	// interpretation is that the header fields listed by the client in its
   	// request represent a preference of the header fields it wishes to use,
   	// with the first options listed being most preferable.  The extensions
   	// listed by the server in response represent the extensions actually in
   	// use for the connection.  Should the extensions modify the data and/or
   	// framing, the order of operations on the data should be assumed to be
   	// the same as the order in which the extensions are listed in the
   	// server's response in the opening handshake.
   	//
	if (!strcasecmp("Sec-WebSocket-Extensions", name))
	{
		//
	   	// The |Sec-WebSocket-Extensions| header field MAY appear multiple times
		// in an HTTP request (which is logically the same as a single
		// |Sec-WebSocket-Extensions| header field that contains all values.
		// However, the |Sec-WebSocket-Extensions| header field MUST NOT appear
		// more than once in an HTTP response.
	   	//
		{
			// TODO: Parse extension list here. Right now no extensions are supported, so fail by default.
			LIBWS_LOG(LIBWS_ERR, "The server wants to use an extension "
								 "we didn't request: %s", val);
			// TODO: Set close reason here.
			return -1;
		}

		ws->http_header_flags |= WS_HAS_VALID_WS_EXT_HEADER;
	}

	// 6. If the response includes a |Sec-WebSocket-Protocol| header field
	//    and this header field indicates the use of a subprotocol that was
	//    not present in the client's handshake (the server has indicated a
	//    subprotocol not requested by the client), the client MUST _Fail
	//    the WebSocket Connection_.
	if (!strcasecmp("Sec-WebSocket-Protocol", name))
	{
	   	// The |Sec-WebSocket-Protocol| header field MAY appear multiple times
		// in an HTTP request (which is logically the same as a single
		// |Sec-WebSocket-Protocol| header field that contains all values).
		// However, the |Sec-WebSocket-Protocol| header field MUST NOT appear
		// more than once in an HTTP response.
		if (ws->http_header_flags & WS_HAS_VALID_WS_PROTOCOL_HEADER)
		{
			LIBWS_LOG(LIBWS_ERR, "Got more than one \"Sec-WebSocket-Protocol\""
								 " header in HTTP response");

			ws->http_header_flags &= ~WS_HAS_VALID_WS_PROTOCOL_HEADER;
			return -1;
		}

		if (_ws_check_server_protocol_list(ws, val))
		{
			LIBWS_LOG(LIBWS_ERR, "Server wanted to use a subprotocol we "
								 "didn't request: %s", val);

			ws->http_header_flags &= ~WS_HAS_VALID_WS_PROTOCOL_HEADER;
			return -1;
		}

		ws->http_header_flags |= WS_HAS_VALID_WS_PROTOCOL_HEADER;
	}

	return 0;	
}

ws_parse_state_t _ws_read_http_headers(ws_t ws, struct evbuffer *in)
{
	char *line = NULL;
	char *header_name = NULL;
	char *header_val = NULL;
	ws_parse_state_t state = WS_PARSE_STATE_NEED_MORE;
	size_t len;
	assert(ws);
	assert(in);

	// TODO: Set a max header size to allow.

	while ((line = evbuffer_readln(in, &len, EVBUFFER_EOL_CRLF)) != NULL)
	{
		// Check for end of HTTP response (empty line).
		if (*line == '\0')
		{
			LIBWS_LOG(LIBWS_DEBUG2, "End of HTTP request");
			state = WS_PARSE_STATE_SUCCESS;
			break;	
		}

		if (_ws_parse_http_header(line, &header_name, &header_val))
		{
			LIBWS_LOG(LIBWS_ERR, "Failed to parse HTTP upgrade "
								 "repsonse line: %s", line);
			
			state = WS_PARSE_STATE_ERROR;
			break;
		}

		LIBWS_LOG(LIBWS_DEBUG2, "%s: %s", header_name, header_val);
		
		// Let the user get the header.
		if (ws->header_cb)
		{
			LIBWS_LOG(LIBWS_DEBUG2, "	Call header callback");

			if (ws->header_cb(ws, header_name, header_val, ws->header_arg))
			{
				LIBWS_LOG(LIBWS_DEBUG, "User header callback cancelled "
										"handshake");
				state = WS_PARSE_STATE_USER_ABORT;
				break;
			}
		}

		if (_ws_validate_http_headers(ws, header_name, header_val))
		{
			LIBWS_LOG(LIBWS_ERR, "	invalid");
			state = WS_PARSE_STATE_ERROR;
			break;
		}

		LIBWS_LOG(LIBWS_DEBUG2, "	valid");

		_ws_free(line);
		line = NULL;

		if (header_name) 
		{
			_ws_free(header_name);
			header_name = NULL;
		}

		if (header_val)
		{
			_ws_free(header_val);
			header_val = NULL;
		}
	}

	if (line) _ws_free(line);
	if (header_name) _ws_free(header_name);
	if (header_val) _ws_free(header_val);

	return state;
}

ws_parse_state_t _ws_read_server_handshake_reply(ws_t ws, struct evbuffer *in)
{
	char *line = NULL;
	char *header_name = NULL;
	char *header_val = NULL;
	int major_version;
	int minor_version;
	int status_code;
	ws_parse_state_t parse_state;
	assert(ws);
	assert(in);

	LIBWS_LOG(LIBWS_DEBUG, "Reading server handshake reply");

	switch (ws->connect_state)
	{
		default: 
		{
			LIBWS_LOG(LIBWS_ERR, "Incorrect connect state in server handshake "
								 "response handler %d", ws->connect_state);
			return WS_PARSE_STATE_ERROR; 
		}
		case WS_CONNECT_STATE_SENT_REQ:
		{
			ws->http_header_flags = 0;

			// Parse status line HTTP/1.1 200 OK...
			if ((parse_state = _ws_read_http_status(ws, in, 
							&major_version, &minor_version, &status_code)) 
							!= WS_PARSE_STATE_SUCCESS)
			{
				return parse_state;
			}

			LIBWS_LOG(LIBWS_DEBUG, "HTTP/%d.%d %d", 
					major_version, minor_version, status_code);

			if ((major_version != 1) || (minor_version != 1))
			{
				LIBWS_LOG(LIBWS_ERR, "Server using unsupported HTTP "
									 "version %d.%d",
										major_version, minor_version);
				return WS_PARSE_STATE_ERROR;
			}

			if (status_code != HTTP_STATUS_SWITCHING_PROTOCOLS_101)
			{
				// TODO: This must not be invalid. Could be a redirect for instance (add callback for this, so the user can decide to follow it... and also add auto follow support).

				LIBWS_LOG(LIBWS_ERR, "Invalid HTTP status code (%d)", 
									status_code);
				return WS_PARSE_STATE_ERROR;
			}

			ws->connect_state = WS_CONNECT_STATE_PARSED_STATUS;
			// Fall through.
		} 
		case WS_CONNECT_STATE_PARSED_STATUS:
		{
			LIBWS_LOG(LIBWS_DEBUG, "Reading headers");

			// Read the HTTP headers.
			if ((parse_state = _ws_read_http_headers(ws, in)) 
				!= WS_PARSE_STATE_SUCCESS)
			{
				return parse_state;
			}

			LIBWS_LOG(LIBWS_DEBUG, "Successfully parsed HTTP headers");

			ws->connect_state = WS_CONNECT_STATE_PARSED_HEADERS;
			// Fall through.
		}
		case WS_CONNECT_STATE_PARSED_HEADERS:
		{
			ws_http_header_flags_t f = ws->http_header_flags;
			LIBWS_LOG(LIBWS_DEBUG, "Checking if we have all required headers:");

			if (!(f & WS_HAS_VALID_UPGRADE_HEADER))
			{
				LIBWS_LOG(LIBWS_ERR, "Missing Upgrade header");
				return WS_PARSE_STATE_ERROR;
			}

			if (!(f & WS_HAS_VALID_CONNECTION_HEADER))
			{
				LIBWS_LOG(LIBWS_ERR, "Missing Connection header");
				return WS_PARSE_STATE_ERROR;
			}
			
			if (!(f & WS_HAS_VALID_WS_ACCEPT_HEADER))
			{
				LIBWS_LOG(LIBWS_ERR, "Missing Sec-WebSocket-Accept header");
				return WS_PARSE_STATE_ERROR;
			}

			if (!(f & WS_HAS_VALID_WS_PROTOCOL_HEADER))
			{
				if (ws->num_subprotocols > 0)
				{
					LIBWS_LOG(LIBWS_WARN, "Server did not reply to my "
											"subprotocol request");
				}
			}

			LIBWS_LOG(LIBWS_DEBUG, "Handshake complete");
			ws->connect_state = WS_CONNECT_STATE_HANDSHAKE_COMPLETE;
		}
	}

	return WS_PARSE_STATE_SUCCESS;
}


