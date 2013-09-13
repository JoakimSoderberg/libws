
#include "libws_config.h"
#include "libws_log.h"
#include "libws_types.h"
#include "libws_handshake.h"
#include "libws_private.h"
#include "libws_base64.h"

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

int _ws_read_http_upgrade_response(ws_t ws)
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

	return 0;
}


