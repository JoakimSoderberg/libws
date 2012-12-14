
#include "libws_config.h"

#include "libws_log.h"
#include "libws_private.h"

void _ws_event_callback(struct bufferevent *bev, short events, void *ptr)
{
	ws_t ws = (ws_t)ptr;
	assert(ws != NULL);

	if (events & BEV_EVENT_CONNECTED)
	{
		char buf[1024];
		LIBWS_LOG(LIBWS_DEBUG, "Connected to %s", ws_get_uri(ws, buf, sizeof(buf)));

		if (ws->connect_cb)
		{
			ws->connect_cb(ws, ws->connect_arg);
		}

		return;
	}

	if (events & BEV_EVENT_EOF)
	{
		if (ws->close_cb)
		{
			ws->close_cb(ws, close_arg);
		}

		if (ws_close(ws))
		{
			LIBWS_LOG(LIBWS_ERR, "Error on websocket quit");
		}

		return;
	}
	
	if (events & BEV_EVENT_ERROR)
	{
		char *err_msg;
		int err;
		LIBWS_LOG(LIBWS_DEBUG, "Error raised");

		if (ws->state == WS_STATE_DNS_LOOKUP)
		{
			err = bufferevent_socket_get_dns_error(ws->bev);
			err_msg = evutil_gai_strerror(err);

			LIBWS_LOG(LIBWS_ERR, "DNS error %d: %s", err, err_msg);

			if (ws->err_cb)
			{
				ws->err_cb(ws, err, err_msg, ws->err_arg);
			}
			else
			{
				ws_close(ws);
			}
		}

		return;
	}
}

void _ws_read_callback(struct bufferevent *bev, short events, void *ptr)
{
	ws_t ws = (ws_t)ptr;
	assert(ws != NULL);


}

void _ws_write_callback(struct bufferevent *bev, short events, void *ptr)
{
	ws_t ws = (ws_t)ptr;
	assert(ws != NULL);


}

int _ws_create_bufferevent_socket(ws_t ws)
{

	#ifdef LIBWS_WITH_OPENSSL
	if (ws->use_ssl)
	{
		if (_ws_create_bufferevent_openssl_socket(ws)) 
			return -1;
	}
	else
	#endif // LIBWS_WITH_OPENSSL
	{
		if (!(ws->bev = bufferevent_socket_new(ws->base, -1, BEV_OPT_CLOSE_ON_FREE))
		{
			LIBWS_LOG(LIBWS_ERR, "Failed to create socket");
			return -1;
		}
	}

	bufferevent_setcb(ws->bev, _ws_read_callback, _ws_write_callback, _ws_event_callback, (void *)ws);

	bufferevent_enable(ws->bev, EV_READ | EV_WRITE);

	return 0;
fail:
	if (w->bev)
	{
		bufferevent_free(w->bev);
	}

	return -1;
}

