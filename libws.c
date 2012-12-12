
#include "libws_config.h"

#include <stdio.h>
#include <sys/socket.h>
#include <assert.h>
#include "libws.h"
#include "libws_private.h"
#include "libws_log.h"

int ws_global_init(ws_base_t *base)
{
	ws_base_t b;

	assert(base != NULL);

	if (!(*base = (ws_base_s *)calloc(1, sizeof(ws_base_s))))
	{
		LIBWS_LOG(LIBWS_CRIT, "Out of memory!");
		return -1;
	}

	b = *base;

	b->debug_level = LIBWS_NONE;

	#ifdef LIBWS_WITH_OPENSSL

	SSL_library_init();
	SSL_load_error_strings();

	if (!RAND_poll())
	{
		LIBWS_LOG(LIBWS_CRIT, "Got no random source, cannot init OpenSSL");
		return -1;
	}

	// Setup the SSL context.
	{
		SSL_METHOD *ssl_method = SSLvs23_client_method();

		if (!(b->ssl_ctx = SSL_CTX_new(ssl_method)))
		{
			LIBWS_LOG(LIBWS_ERR, "Failed to create OpenSSL context");
			return -1;
		}
	}

	#endif // LIBWS_WITH_OPENSSL

	return 0;
}

void ws_global_destroy(ws_base_t *base)
{
	ws_base_t b;

	assert(base != NULL);

	b = *base;

	#ifdef LIBWS_WITH_OPENSSL

	if (b->ssl_ctx)
	{		
		SSL_CTX_free(b->ssl_ctx);
		b->ssl_ctx = NULL;
	}

	#endif // LIBWS_WITH_OPENSSL

	return 0;
}

int ws_init(ws_t *ws, ws_base_t ws_base)
{
	ws_t w = NULL;

	assert(ws != NULL);
	assert(ws_base != NULL);

	if (!(*ws = (ws_s *)calloc(1, sizeof(ws_s))))
	{
		LIBWS_LOG(LIBWS_CRIT, "Out of memory!");
		return -1;
	}

	// Just for convenience.
	w = *ws;

	w->ws_base = ws_base;

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

	return 0;
fail:
	if (ws->base)
	{
		event_base_free(ws->base);
		ws->base = NULL;
	}

	return -1;
}

void ws_destroy(ws_t *ws)
{
	ws_t w;

	if (!ws)
		return 0;
	
	w = *ws;

	if (w->bev)
	{
		bufferevent_free(w->bev);
		w->bev = NULL;
	}

	if (w->base)
	{
		event_base_free(w->base);
		w->base = NULL;
	}

	#ifdef LIBWS_WITH_OPENSSL

	if (w->ssl_ctx)
	{
		SSL_CTX_free(w->ssl_ctx);
		w->ssl_ctx = NULL;
	}

	#endif // LIBWS_WITH_OPENSSL

	free(w);
	*ws = NULL;
}

int ws_connect(ws_t ws, const char *server, int port, const char *uri)
{
	assert(ws != NULL);

	if (!ws->base)
	{
		LIBWS_LOG(LIBWS_ERR, "Websocket instance not properly initialized");
		return -1;
	}

	if (ws->state != WS_STATE_DISCONNECTED)
	{
		if (ws_close(ws))
		{
			LIBWS_LOG(LIBWS_ERR, "")
			return -1;
		}
	}

	if (!server)
	{
		LIBWS_LOG(LIBWS_ERR, "NULL server given");
		return -1;
	}

	if (_create_bufferevent_socket(ws))
	{
		LIBWS_LOG(LIBWS_ERR, "Failed to create bufferevent socket");
		return -1;
	}

	// TODO: Add Websocket magic stuff to buf.
	evbuffer_add_printf(bufferevent_get_output(ws->bev), "");
	
	if (bufferevent_socket_connect_hostname(ws->base, ws->dns_base, AF_UNSPEC, server, port))
	{
		LIBWS_LOG(LIBWS_ERR, "Failed to create connect event");
		return -1;
	}

	return 0;
}

/*
int ws_connect(ws_t ws, const char *uri)
{
	// http://www.w3.org/Library/src/HTParse
	ws_parse_uri()

	ws_connect_advanced(ws)
}
*/

int ws_close(ws_t ws)
{
	assert(ws != NULL);

	ws->state = WS_STATE_DISCONNECTING;

	#ifdef LIBWS_WITH_OPENSSL

	//
	// SSL_RECEIVED_SHUTDOWN tells SSL_shutdown to act as if we had already
	// received a close notify from the other end.  SSL_shutdown will then
	// send the final close notify in reply.  The other end will receive the
	// close notify and send theirs.  By this time, we will have already
	// closed the socket and the other end's real close notify will never be
	// received.  In effect, both sides will think that they have completed a
	// clean shutdown and keep their sessions valid.  This strategy will fail
	// if the socket is not ready for writing, in which case this hack will
	// lead to an unclean shutdown and lost session on the other end.
	//
	if (ws->ssl)
	{
		SSL_set_shutdown(ws->ssl, SSL_RECEIVED_SHUTDOWN);
		SSL_shutdown(ws->ssl);
		ws->ssl = NULL;
	}

	#endif // LIBWS_WITH_OPENSSL

	if (ws->bev)
	{
		bufferevent_free(ws->bev);
		ws->bev = NULL;
	}

	return 0;
}

int ws_service(ws_t ws)
{
	assert(ws != NULL);

	if (event_base_loop(ws->base, EVLOOP_NONBLOCK))
	{
		LIBWS_LOG(LIBWS_ERR, "Failed to feed event loop");
		return -1;
	}

	return 0;
}

int ws_service_until_quit(ws_t ws)
{
	assert(ws != NULL);

	return event_base_dispatch(ws->base);
}

int ws_quit(ws_t ws, int let_running_events_complete)
{
	assert(ws != NULL);

	if (!ws->base)
	{
		LIBWS_LOG(LIBWS_ERR, "Websocket event base not initialized");
		return -1;
	}

	if (let_running_events_complete)
	{
		return event_base_loopexit(ws->base, NULL);
	}
	
	return event_base_loopbreak(ws->base);
}

char *ws_get_uri(ws_t ws, char *buf, size_t bufsize)
{
	assert(ws != NULL);

	if (!buf)
	{
		LIBWS_LOG(LIBWS_ERR, "buf is NULL");
		return NULL;
	}

	// TODO: Check return value?
	evutil_vsnprintf(buf, bufsize, "%s://%s:%d/%s", 
		(ws->use_ssl != LIBWS_SSL_OFF) ? "wss" : "ws", 
		ws->server,
		ws->port,
		ws->uri);

	return buf;
}

void ws_set_user_state(ws_t ws, void *user_state)
{
	assert(ws != NULL);

	ws->user_state = user_state;
}

void *ws_get_user_state(ws_t ws)
{
	assert(ws != NULL);

	return ws->user_state;
}


