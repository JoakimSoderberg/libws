
#ifndef __LIBWS_H__
#define __LIBWS_H__

#include "libws_config.h"

#include <stdio.h>
#include <event2/event.h>

typedef struct ws_s *ws_t;
typedef struct ws_base_s *ws_base_t;

typedef enum ws_state_e
{
	WS_STATE_DNS_LOOKUP,
	WS_STATE_DISCONNECTING,
	WS_STATE_DISCONNECTED,
	WS_STATE_CONNECTING,
	WS_STATE_CONNECTED
} ws_state_t;

typedef void (*ws_msg_callback_f)(ws_t ws, const char *msg, size_t len, void *arg);
typedef void (*ws_err_callback_f)(ws_t ws, int errcode, const char *errmsg, void *arg);
typedef void (*ws_close_callback_f)(ws_t ws, int reason, void *arg);
typedef void (*ws_connect_callback_f)(ws_t ws, void *arg);
typedef void (*ws_timeout_callback_f)(ws_t ws, void *arg);

int ws_global_init();

void ws_global_destroy();

int ws_init(ws_t *ws, ws_base_t ws_base);

void ws_destroy(ws_t *ws);

int ws_connect(ws_t ws, const char *server, int port, const char *uri);

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

int ws_service_until_quit(ws_t ws);

int ws_quit(ws_t ws, int let_running_events_complete);

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

int ws_set_recv_timeout_cb(ws_t ws, ws_timeout_callback_f func, struct timeval recv_timeout, void *arg);

int ws_set_send_timeout_cb(ws_t ws, ws_timeout_callback_f func, struct timeval send_timeout, void *arg);

int ws_set_connect_timeout_cb(ws_t ws, ws_timeout_callback_f func, struct timeval connect_timeout, void *arg);

void ws_set_user_state(ws_t ws, void *user_state);

void *ws_get_user_state(ws_t ws);

int ws_get_uri(ws_t ws, char *buf, size_t bufsize);

ws_state_t ws_get_state(ws_t ws);

#ifdef LIBWS_WITH_OPENSSL

typedef enum libws_ssl_state_e
{
	LIBWS_SSL_OFF,
	LIBWS_SSL_ON,
	LIBWS_SSL_SELFSIGNED
} libws_ssl_state_t;

int ws_set_ssl_state(ws_t ws, libws_ssl_state_t ssl);

#endif // LIBWS_WITH_OPENSSL

#endif // __LIBWS_H__