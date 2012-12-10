
#ifndef __LIBWS_PRIVATE_H__
#define __LIBWS_PRIVATE_H__

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

typedef struct ws_s
{
	struct event_base		*base;
	struct evdns_base 		*dns_base;

	ws_msg_callback_f		msg_cb;
	void 					*msg_arg

	ws_err_callback_f 		err_cb;
	void					*err_arg;

	ws_close_callback_f		close_cb;
	void					*close_arg;

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

	char 					*server;
	char					*uri;
	int						port;

	int						binary_mode;

	#ifdef LIBWS_WITH_OPENSSL

	int 					use_ssl;
	SSL 					*ssl_ctx;
	
	#endif // LIBWS_WITH_OPENSSL
} ws_s;

void _ws_event_callback(struct bufferevent *bev, short events, void *ptr);
void _ws_read_callback(struct bufferevent *bev, short events, void *ptr);
void _ws_write_callback(struct bufferevent *bev, short events, void *ptr);
int _create_bufferevent_socket(ws_t ws);

#endif // __LIBWS_PRIVATE_H__
