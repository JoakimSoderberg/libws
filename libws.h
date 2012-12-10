
#ifndef __LIBWS_H__
#define __LIBWS_H__

#include "libws_config.h"

#include <stdio.h>
#include <event2/event.h>

typedef struct ws_s *ws_t;

typedef int (*ws_msg_callback_f)(ws_t ws, const char *msg, size_t len, void *arg);
typedef int (*ws_err_callback_f)(ws_t ws, int errcode, const char *errmsg, void *arg);
typedef int (*ws_close_callback_f)(ws_t ws, int reason, void *arg);
typedef int (*ws_connect_callback_f)(ws_t ws, void *arg);
typedef int (*ws_timeout_callback_f)(ws_t ws, void *arg);

int ws_global_init();

int ws_global_destroy();

int ws_init(ws_t *ws);

int ws_destroy(ws_t *ws);

int ws_connect_advanced(ws_t ws, const char *server, int port, const char *uri);

//int ws_connect(ws_t ws, const char *uri);

int ws_close(ws_t ws);

///
/// Services the Websocket connection.
///
/// @param[in] ws 	The websocket context to be serviced.
///
/// @returns 0 on success. -1 on failure.
///
int ws_service(ws_t ws);

int ws_send_msg(ws_t ws, const char *msg, size_t len);

int ws_send_ping(ws_t ws);

int ws_set_onmsg_cb(ws_t ws, ws_msg_callback_f func, void *arg);

int ws_set_onerr_cb(ws_t ws, ws_msg_callback_f func, void *arg);

int ws_set_onclose_cb(ws_t ws, ws_close_callback_f func, void *arg);

int ws_set_origin(ws_t ws, const char *origin);

int ws_set_onpong_cb(ws_t ws, ws_msg_callback_f, void *arg);

int ws_set_binary(ws_t ws, int binary);

int ws_add_header(ws_t ws, const char *header, const char *value);

int ws_remove_header(ws_t ws, const char *header);

int ws_is_connected(ws_t ws);

int ws_parse_uri(const char *uri, char **protocol, char **hostname, int *port, char **path);

int ws_set_recv_timeout_cb(ws_t ws, ws_timeout_callback_f func, struct timeval recv_timeout, void *arg);

int ws_set_send_timeout_cb(ws_t ws, ws_timeout_callback_f func, struct timeval send_timeout, void *arg);

int ws_set_connect_timeout_cb(ws_t ws, ws_timeout_callback_f func, struct timeval connect_timeout, void *arg);

#ifdef LIBWS_WITH_OPENSSL
int ws_set_ssl(ws_t ws, int ssl);
#endif // LIBWS_WITH_OPENSSL

#endif // __LIBWS_H__
