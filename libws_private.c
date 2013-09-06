
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
	assert(bev);

	char *buf = NULL;

	// TODO: Read from the bufferevent.
	// TODO: Parse the websocket header.
	// TODO: Based on op code forward data to appropriate callback.
}

void _ws_write_callback(struct bufferevent *bev, void *ptr)
{
	ws_t ws = (ws_t)ptr;
	assert(ws);
	assert(bev);


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

void _ws_builtin_no_copy_cleanup_wrapper(const void *data, 
										size_t datalen, void *extra)
{
	ws_t ws = (ws_t)extra;
	assert(ws);
	assert(ws->no_copy_cleanup_cb);

	// We wrap this so we can pass the websocket context.
	// (Also, we don't want to expose any bufferevent types to the
	//  external API so we're free to replace it).
	ws->no_copy_cleanup_cb(ws, data, datalen, ws->no_copy_extra);
}

int _ws_send_data(ws_t ws, char *msg, uint64_t len, int no_copy)
{
	// TODO: We supply a len of uint64_t, evbuffer_add uses size_t...
	assert(ws);

	if (ws->state != WS_STATE_CONNECTED)
	{
		return -1;
	}

	if (!ws->bev)
	{
		LIBWS_LOG(LIBWS_ERR, "Null bufferevent on send");
		return -1;
	}

	// If in no copy mode we only add a reference to the passed
	// buffer to the underlying bufferevent, and let it use the
	// user supplied cleanup function when it has sent the data.
	if (no_copy && ws->no_copy_cleanup_cb)
	{
		if (evbuffer_add_reference(bufferevent_get_output(bev), 
			(void *)msg, len, _ws_builtin_cleanup_wrapper, (void *)ws))
		{
			LIBWS_LOG(LIBWS_ERR, "Failed to write reference to send buffer");
			return -1;
		}
	}
	else
	{
		// Send like normal (this will copy the data).
		if (evbuffer_add(bufferevent_get_output(ws->bev), 
						msg, (size_t)len))
		{
			LIBWS_LOG(LIBWS_ERR, "Failed to write to send buffer");
			return -1;
		}
	}

	return 0;
}


uint32_t _ws_get_random_mask()
{
	uint32_t b;
	return b;
}

int _ws_mask_payload(uint32_t mask, char *msg, uint64_t len)
{
	assert(msg);
}


