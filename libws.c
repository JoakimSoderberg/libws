
#include "libws_config.h"

#include <stdio.h>
#include <sys/socket.h>
#include <assert.h>
#include "libws.h"
#include "libws_private.h"
#include "libws_log.h"

// ------------------------------------------------------------------------------


int ws_global_init()
{
	#ifdef LIBWS_WITH_OPENSSL
	SSL_library_init();
	SSL_load_error_strings();

	if (!RAND_poll())
	{
		LIBWS_LOG(LIBWS_CRIT, "Got no random source, cannot init OpenSSL");
		return -1;
	}

	#endif // LIBWS_WITH_OPENSSL
	return 0;
}

int ws_global_destroy()
{
	return 0;
}

int ws_init(ws_t *ws)
{
	ws_t w = NULL;

	*ws = (ws_s *)calloc(1, sizeof(ws_s));

	if (*ws)
	{
		LIBWS_LOG(LIBWS_CRIT, "Out of memory!");
		return -1;
	}

	// Just for convenience.
	w = *ws;

	// Create Libevent context.
	{
		if (!(w->base = event_base_new()))
		{
			LIBWS_LOG(LIBWS_CRIT, "Out of memory!");
			goto fail;
		}

		if (!(w->dns_base = evdns_base_new()))
		{
			LIBWS_LOG(LIBWS_CRIT, "Out of memory!");
			goto fail;
		}
	}

	#ifdef LIBWS_WITH_OPENSSL
	// Setup the SSL context.
	{
		SSL_METHOD *ssl_method = SSLvs23_client_method();

		if (!(w->ssl_ctx = SSL_CTX_new(ssl_method)))
		{
			LIBWS_LOG(LIBWS_ERR, "Failed to create OpenSSL context");
			goto fail;
		}
	}
	#endif // LIBWS_WITH_OPENSSL

	return 0;
fail:
	if (ws->base)
	{
		event_base_free(ws->base);
	}

	return -1;
}

int ws_destroy(ws_t *ws)
{
	if (!ws)
		return 0;
	
	if (ws->bev)
	{
		bufferevent_free(ws->bev);
		ws->bev = NULL;
	}

	if (ws->base)
	{
		event_base_free(ws->base);
		ws->base = NULL;
	}

	#ifdef LIBWS_WITH_OPENSSL
	if (ws->ssl_ctx)
	{
		SSL_CTX_free(ws->ssl_ctx);
		ws->ssl_ctx = NULL;
	}
	#endif // LIBWS_WITH_OPENSSL

	free(ws);

	return 0;
}

int ws_connect_advanced(ws_t ws, const char *server, int port, const char *uri)
{
	assert(ws != NULL);
	assert(ws->base != NULL);

	if (!server)
	{
		LIBWS_LOG(LIBWS_ERR, "NULL server given");
		return -1;
	}

	// Copy arguments.
	{
		if (ws->server) free(ws->server);
		if (ws->uri) free(ws->uri);
		ws->port = 0;

		ws->server = strdup(server);
		ws->uri = strdup(uri);
		ws->port = port;
	}


	if (_create_bufferevent_socket(ws))
	{
		LIBWS_LOG(LIBWS_ERR, "Failed to create bufferevent socket");
		return -1;
	}
	

	//  int bufferevent_socket_connect_hostname (struct buffer-event *, struct evdns_base *, int, const char *, int)
	//bufferevent_socket_connect_hostname(ws->bev, )

	// TODO: Add Websocket magic stuff to buf.
	evbuffer_add_printf(bufferevent_get_output(ws->bev), "");
	
	if (bufferevent_socket_connect_hostname(ws->bev, 
		ws->base, ws->dns_base, AF_UNSPEC, server, port))
	{
		LIBWS_LOG(LIBWS_ERR, "Failed to call connect");
		goto fail;
	}

	return 0;
fail:

	bufferevent_free(ws->bev);
	return -1;
}

/*
int ws_connect(ws_t ws, const char *uri)
{
	// http://www.w3.org/Library/src/HTParse
	ws_parse_uri()

	ws_connect_advanced(ws)
}*/

int ws_close(ws_t ws)
{
	assert(ws != NULL);

	if (ws->server) free(ws->server);
	if (ws->uri) free(ws->uri);
	ws->port = 0;

	#ifdef LIBWS_WITH_OPENSSL
	// TODO: Shutdown SSL connection.
	#endif // LIBWS_WITH_OPENSSL	

	bufferevent_free(ws->bev);

	return 0;
}

int ws_service(ws_t ws)
{
	assert(ws != NULL);
	return event_base_loop(ws->base, EVLOOP_NONBLOCK);
}

int ws_send_msg(ws_t ws, const char *msg, size_t len)
{
	assert(ws != NULL);
	return 0;
}

int ws_send_ping(ws_t ws)
{
	assert(ws != NULL);
	return 0;
}

int ws_set_onmsg_cb(ws_t ws, ws_msg_callback_f func, void *arg)
{
	assert(ws != NULL);
	return 0;
}

int ws_set_onerr_cb(ws_t ws, ws_msg_callback_f func, void *arg)
{
	assert(ws != NULL);
	return 0;
}

int ws_set_onclose_cb(ws_t ws, ws_close_callback_f func, void *arg)
{
	assert(ws != NULL);
	return 0;
}

int ws_set_origin(ws_t ws, const char *origin)
{
	assert(ws != NULL);
	return 0;
}

int ws_set_onpong_cb(ws_t ws, ws_msg_callback_f func, void *arg)
{
	assert(ws != NULL);

	ws->pong_cb = func;
	ws->pong_arg = arg;

	return 0;
}

int ws_set_binary(ws_t ws, int binary)
{
	assert(ws != NULL);
	return 0;
}

int ws_add_header(ws_t ws, const char *header, const char *value)
{
	assert(ws != NULL);
	return 0;
}

int ws_remove_header(ws_t ws, const char *header)
{
	assert(ws != NULL);
	return 0;
}

int ws_is_connected(ws_t ws)
{
	assert(ws != NULL);
	return 0;
}

int ws_set_recv_timeout_cb(ws_t ws, ws_timeout_callback_f func, struct timeval recv_timeout, void *arg)
{
	assert(ws != NULL);

	ws->ws_set_recv_timeout_cb = func;
	ws->recv_timeout_arg = arg;
	ws->recv_timeout = recv_timeout;

	return 0;
}

int ws_set_send_timeout_cb(ws_t ws, ws_timeout_callback_f func, struct timeval send_timeout, void *arg)
{
	assert(ws != NULL);

	ws->send_timeout_cb = func;
	ws->send_timeout = send_timeout;
	ws->send_timeout_arg = arg;

	return 0;
}

int ws_set_connect_timeout_cb(ws_t ws, ws_timeout_callback_f func, struct timeval connect_timeout, void *arg)
{
	assert(ws != NULL);

	ws->ws_set_connect_timeout_cb = func;
	ws->connect_timeout_arg;
	ws->connect_timeout = connect_timeout;

	return 0;
}

#ifdef LIBWS_WITH_OPENSSL
int ws_set_ssl(ws_t ws, int ssl)
{
	assert(ws != NULL);
	
	ws->use_ssl = ssl;

	return 0;
}
#endif // LIBWS_WITH_OPENSSL


