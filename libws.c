
#include "libws_config.h"

#include <stdio.h>
#include <sys/socket.h>
#include <assert.h>
#include "libws.h"
#include "libws_private.h"
#include "libws_log.h"
#ifdef LIBWS_WITH_OPENSSL
#include "libws_openssl.h"
#endif

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

	#ifdef LIBWS_WITH_OPENSSL
	if (_ws_global_openssl_init(b)) 
	{
		return -1;
	}
	#endif

	return 0;
}

void ws_global_destroy(ws_base_t *base)
{
	ws_base_t b;

	assert(base != NULL);

	b = *base;

	#ifdef LIBWS_WITH_OPENSSL
	_ws_global_openssl_destroy(b);
	#endif

	free(*base);
	*base = NULL;

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
	_ws_openssl_destroy(ws);
	#endif

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

	if (_ws_create_bufferevent_socket(ws))
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
	_ws_openssl_close(ws);
	#endif

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
	int ret;
	assert(ws != NULL);

	ret = event_base_dispatch(ws->base);

	ws->state = WS_STATE_DISCONNECTED;

	return ret;
}

int ws_quit(ws_t ws, int let_running_events_complete)
{
	int ret;
	assert(ws != NULL);

	if (!ws->base)
	{
		LIBWS_LOG(LIBWS_ERR, "Websocket event base not initialized");
		return -1;
	}

	ws_close(ws);

	if (let_running_events_complete)
	{
		ret = event_base_loopexit(ws->base, NULL);
	}
	else
	{
		ret = event_base_loopbreak(ws->base);
	}

	return ret;
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


