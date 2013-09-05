
#include "libws_config.h"

#include <stdio.h>

#include "libws_log.h"
#include "libws_private.h"

static void _ws_connected_event(struct bufferevent *bev, short events, ws_t ws)
{
	char buf[1024];
	LIBWS_LOG(LIBWS_DEBUG, "Connected to %s", ws_get_uri(ws, buf, sizeof(buf)));

	if (ws->connect_cb)
	{
		ws->connect_cb(ws, ws->connect_arg);
	}
}

static void _ws_eof_event(struct bufferevent *bev, short events, ws_t ws)
{
	if (ws->close_cb)
	{
		// TODO: Pass a reason.
		ws->close_cb(ws, 0, ws->close_arg);
	}

	if (ws_close(ws))
	{
		LIBWS_LOG(LIBWS_ERR, "Error on websocket quit");
	}
}

static void _ws_error_event(struct bufferevent *bev, short events, ws_t ws)
{
	const char *err_msg;
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
}

void _ws_event_callback(struct bufferevent *bev, short events, void *ptr)
{
	ws_t ws = (ws_t)ptr;
	assert(ws);

	if (events & BEV_EVENT_CONNECTED)
	{
		_ws_connected_event(bev, events, ws);
		return;
	}

	if (events & BEV_EVENT_EOF)
	{
		_ws_eof_event(bev, events, ws);
		return;
	}
	
	if (events & BEV_EVENT_ERROR)
	{
		_ws_error_event(bev, events, ws);
		return;
	}
}

void _ws_read_callback(struct bufferevent *bev, void *ptr)
{
	ws_t ws = (ws_t)ptr;
	assert(ws);

	char *buf = NULL;
}

void _ws_write_callback(struct bufferevent *bev, void *ptr)
{
	ws_t ws = (ws_t)ptr;
	assert(ws);


}

int _ws_create_bufferevent_socket(ws_t ws)
{
	assert(ws);

	#ifdef LIBWS_WITH_OPENSSL
	if (ws->use_ssl)
	{
		if (_ws_create_bufferevent_openssl_socket(ws)) 
		{
			return -1;
		}
	}
	else
	#endif // LIBWS_WITH_OPENSSL
	{
		if (!(ws->bev = bufferevent_socket_new(ws->base, -1, 
										BEV_OPT_CLOSE_ON_FREE)))
		{
			LIBWS_LOG(LIBWS_ERR, "Failed to create socket");
			return -1;
		}
	}

	bufferevent_setcb(ws->bev, _ws_read_callback, _ws_write_callback, 
					_ws_event_callback, (void *)ws);

	bufferevent_enable(ws->bev, EV_READ | EV_WRITE);

	return 0;
fail:
	if (ws->bev)
	{
		bufferevent_free(ws->bev);
	}

	return -1;
}

int _ws_send_data(ws_t ws, const char *msg, uint64_t len)
{
	assert(ws);

	if (ws->state != WS_STATE_CONNECTED)
	{
		return -1;
	}

	// TODO: Send data.

	return 0;
}


uint32_t _ws_get_random_bits()
{
	uint32_t b;
	return b;
}

int _ws_mask_payload(uint32_t mask, char *msg, uint64_t len)
{

}


