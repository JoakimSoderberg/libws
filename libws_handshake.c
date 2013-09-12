
#include "libws_config.h"
#include "libws_log.h"
#include "libws_types.h"
#include "libws_handshake.h"
#include "libws_private.h"

int _ws_send_http_upgrade(ws_t ws)
{
	struct evbuffer *out = NULL;
	assert(ws);
	assert(ws->bev);

	if (!ws->server)
	{
		return -1;
	}

	out = bufferevent_get_output(ws->bev);

	evbuffer_add_printf(out,
		"GET /%s HTTP/1.1\r\n"
		"Host: %s:%d\r\n"
		"Connection: Upgrade\r\n"
		"Upgrade: websocket\r\n"
		"Sec-Websocket-Version: 13\r\n",
		(ws->uri ? ws->uri : ""),
		ws->server,
		ws->port);

	//evbuffer_add_printf(out)

	return 0;
}
