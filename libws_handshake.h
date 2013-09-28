
#ifndef __LIBWS_HANDSHAKE_H__
#define __LIBWS_HANDSHAKE_H__

#include "libws.h"
#include <event2/buffer.h>

#define HTTP_STATUS_SWITCHING_PROTOCOLS_101 101

typedef enum ws_http_header_flags_e
{
	WS_HAS_VALID_UPGRADE_HEADER 	= (1 << 0), ///< A valid Upgrade header received.
	WS_HAS_VALID_CONNECTION_HEADER 	= (1 << 1), ///< A valid Connection header received.
	WS_HAS_VALID_WS_ACCEPT_HEADER 	= (1 << 2), ///< A valid Sec-WebSocket-Accept header received.
	WS_HAS_VALID_WS_EXT_HEADER 		= (1 << 3), ///< A valid Sec-WebSocket-Extensions header received.
	WS_HAS_VALID_WS_PROTOCOL_HEADER = (1 << 4)  ///< A valid Sec-WebSocket-Protocol header received.
} ws_http_header_flags_t;

int _ws_generate_handshake_key(ws_t ws);

int _ws_send_handshake(ws_t ws, struct evbuffer *out);

int _ws_parse_http_header(char *line, char **header_name, char **header_val);

int _ws_read_server_handshake_reply(ws_t ws, struct evbuffer *in);

int _ws_check_server_protocol_list(ws_t ws, const char *val);

int _ws_calculate_key_hash(const char *handshake_key_base64, 
							char *key_hash, size_t len);

#endif // __LIBWS_HANDSHAKE_H__

