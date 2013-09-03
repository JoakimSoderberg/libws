
#ifndef __LIBWS_TYPES_H__
#define __LIBWS_TYPES_H__

#include <stdio.h>

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

#ifdef LIBWS_WITH_OPENSSL
typedef enum libws_ssl_state_e
{
	LIBWS_SSL_OFF,
	LIBWS_SSL_ON,
	LIBWS_SSL_SELFSIGNED
} libws_ssl_state_t;
#endif // LIBWS_WITH_OPENSSL

typedef void (*ws_msg_callback_f)(ws_t ws, const char *msg, size_t len, void *arg);
typedef void (*ws_err_callback_f)(ws_t ws, int errcode, const char *errmsg, void *arg);
typedef void (*ws_close_callback_f)(ws_t ws, int reason, void *arg);
typedef void (*ws_connect_callback_f)(ws_t ws, void *arg);
typedef void (*ws_timeout_callback_f)(ws_t ws, void *arg);

#endif // __LIBWS_TYPES_H__
