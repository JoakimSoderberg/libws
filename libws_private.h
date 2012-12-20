
#ifndef __LIBWS_PRIVATE_H__
#define __LIBWS_PRIVATE_H__

///
/// @internal
/// @file libws_private.h
///
/// @author Joakim Söderberg <joakim.soderberg@gmail.com>
///
///

#include "libws_config.h"

#include "libws.h"
#include <event2/event.h>
#include <event2/bufferevent.h>

#ifdef LIBWS_WITH_OPENSSL
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <event2/bufferevent_ssl.h>
#endif // LIBWS_WITH_OPENSSL

///
/// Global context for the library.
///
typedef struct ws_base_s
{
	#ifdef LIBWS_WITH_OPENSSL

	SSL_CTX 				*ssl_ctx;

	#endif // LIBWS_WITH_OPENSSL
} ws_base_s;

///
/// Context for a websocket connection.
///
typedef struct ws_s
{
	ws_state_t				state;				///< Websocket state.

	struct event_base		*base;				///< Libevent event base.
	struct evdns_base 		*dns_base;			///< Libevent DNS base.

	ws_msg_callback_f		msg_cb;				///< Callback for when a message is received on the websocket.
	void 					*msg_arg			///< The user supplied argument to pass to the ws_s#msg_cb callback.

	ws_err_callback_f 		err_cb;				///< Callback for when an error occurs on the websocket connection.
	void					*err_arg;			///< The user supplied argument to pass to the ws_s#error_cb callback.

	ws_close_callback_f		close_cb;			///< Callback for when the websocket connection is closed.
	void					*close_arg;			///< The user supplied argument to pass to the ws_s#close_cb callback.

	ws_connect_callback_f	connect_cb;
	void					*connect_arg;

	ws_timeout_callback_f	connect_timeout_cb;
	struct timeval			connect_timeout;
	void					*connect_timeout_arg;

	ws_timeout_callback_f	recv_timeout_cb;
	void					*recv_timeout_arg;
	struct timeval			recv_timeout;

	ws_timeout_callback_f	send_timeout_cb;
	struct timeval			send_timeout;
	void					*send_timeout_arg;

	ws_msg_callback_f		pong_cb;
	void					*poing_arg;

	void					*user_state;

	char 					*server;
	char					*uri;
	int						port;

	int						binary_mode;

	int						debug_level;
	ws_base_t				*ws_base;

	#ifdef LIBWS_WITH_OPENSSL

	int 					use_ssl;

	#endif // LIBWS_WITH_OPENSSL
} ws_s;

///
/// Libevent bufferevent callback for when an event occurs on
/// the websocket socket.
///
void _ws_event_callback(struct bufferevent *bev, short events, void *ptr);

///
/// Libevent bufferevent callback for when there is datata to be read
/// on the websocket socket.
///
void _ws_read_callback(struct bufferevent *bev, short events, void *ptr);

///
/// Libevent bufferevent callback for when a write is done on
/// the websocket socket.
///
void _ws_write_callback(struct bufferevent *bev, short events, void *ptr);

///
/// Helper function for creating a bufferevent socket.
///
int _ws_create_bufferevent_socket(ws_t ws);



#endif // __LIBWS_PRIVATE_H__
