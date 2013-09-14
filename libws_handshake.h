
#ifndef __LIBWS_HANDSHAKE_H__
#define __LIBWS_HANDSHAKE_H__

#define LIBWS_SEC_WEBSOCKET_ACCEPT_KEY "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

int _ws_generate_handshake_key(ws_t ws);

int _ws_send_http_upgrade(ws_t ws);

int _ws_parse_http_header(char *line, char **header_name, char **header_val);

int _ws_read_http_upgrade_response(ws_t ws);


#endif // __LIBWS_HANDSHAKE_H__

