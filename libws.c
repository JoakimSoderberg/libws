
#include "libws_config.h"

#include <event2/event.h>
#include <stdio.h>
#include <sys/socket.h>
#include <assert.h>
#include "libws_types.h"
#include "libws_log.h"
#include "libws.h"
#include "libws_protocol.h"
#include "libws_private.h"
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

	#ifndef WIN32
	if ((b->random_fd = open(WS_RANDOM_PATH, O_RDONLY)) < 0)
	{
		LIBWS_LOG(LIBWS_ERR, "Failed to open random source %s , %d",
							WS_RANDOM_PATH, b->random_fd);
		return -1;
	}
	#endif

	#ifdef LIBWS_WITH_OPENSSL
	if (_ws_global_openssl_init(b))
	{
		LIBWS_LOG(LIBWS_CRIT, "Failed to init OpenSSL");
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

	#ifndef WIN32
	close(b->random_fd);
	#endif

	#ifdef LIBWS_WITH_OPENSSL
	_ws_global_openssl_destroy(b);
	#endif

	free(*base);
	*base = NULL;

	// TODO: Should we destroy all connections here as well?
}

int ws_init(ws_t *ws, ws_base_t ws_base)
{
	struct ws_s *w = NULL;

	assert(ws != NULL);
	assert(ws_base != NULL);

	if (!(*ws = (struct ws_s *)calloc(1, sizeof(struct ws_s))))
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

		if (!(w->dns_base = evdns_base_new(w->base, 1)))
		{
			LIBWS_LOG(LIBWS_CRIT, "Out of memory!");
			goto fail;
		}
	}

	return 0;
fail:
	if (w->base)
	{
		event_base_free(w->base);
		w->base = NULL;
	}

	return -1;
}

void ws_destroy(ws_t *ws)
{
	struct ws_s *w;

	if (!ws || !(*ws))
		return;
	
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
	_ws_openssl_destroy(w);
	#endif

	free(w);
	*ws = NULL;
}

int ws_connect(ws_t ws, const char *server, int port, const char *uri)
{
	assert(ws);

	if (!ws->base)
	{
		LIBWS_LOG(LIBWS_ERR, "Websocket instance not properly initialized");
		return -1;
	}

	if (ws->state != WS_STATE_DISCONNECTED)
	{
		if (ws_close(ws))
		{
			LIBWS_LOG(LIBWS_ERR, "Already connected");
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
	
	if (bufferevent_socket_connect_hostname(ws->bev, 
				ws->dns_base, AF_UNSPEC, server, port))
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

	if (ws->close_cb)
	{
		// TODO: Make up some reasons for closing :D
		int reason = 0;
		ws->close_cb(ws, reason, ws->close_arg);
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
	evutil_snprintf(buf, bufsize, "%s://%s:%d/%s", 
		(ws->use_ssl != LIBWS_SSL_OFF) ? "wss" : "ws", 
		ws->server,
		ws->port,
		ws->uri);

	return buf;
}

void ws_set_no_copy_cb(ws_t ws, ws_no_copy_cleanup_f cleanup_f, void *extra)
{
	assert(ws);
	ws->no_copy_cleanup_cb = cleanup_f;
	ws->no_copy_extra = extra;
}

void ws_set_user_state(ws_t ws, void *user_state)
{
	assert(ws);
	ws->user_state = user_state;
}

void *ws_get_user_state(ws_t ws)
{
	assert(ws);
	return ws->user_state;
}

#define _WS_MUST_BE_CONNECTED(__ws__, err_msg) \
	if (__ws__->state != WS_STATE_CONNECTED) \
	{ \
		LIBWS_LOG(LIBWS_ERR, "Not connected on " err_msg); \
		return -1; \
	} 

int ws_msg_begin(ws_t ws, ws_frame_type_t type)
{
	assert(ws);
	_WS_MUST_BE_CONNECTED(ws, "message begin");

	if (ws->send_state != WS_SEND_STATE_NONE)
	{
		return -1;
	}

	memset(&ws->header, 0, sizeof(ws_header_t));
	ws->header.opcode = ws->binary_mode ? 
						WS_OPCODE_BINARY : WS_OPCODE_TEXT;

	ws->send_state = WS_SEND_STATE_MESSAGE_BEGIN;
	
	return 0;
}

int ws_msg_frame_data_begin(ws_t ws, uint64_t datalen)
{
	uint8_t header_buf[WS_MAX_HEADER_SIZE];
	size_t header_len = 0;

	assert(ws);
	_WS_MUST_BE_CONNECTED(ws, "frame data begin");

	if ((ws->send_state != WS_SEND_STATE_MESSAGE_BEGIN)
	 && (ws->send_state != WS_SEND_STATE_IN_MESSAGE))
	{
		LIBWS_LOG(LIBWS_ERR, "Incorrect state for frame data begin");
		return -1;
	)
	
	if (datalen > WS_MAX_PAYLOAD_LEN)
	{
		LIBWS_LOG(LIBWS_ERR, "Payload length (0x%x) larger than max allowed "
							 "websocket payload (0x%x)",
							ws->header.payload_len, WS_MAX_PAYLOAD_LEN);
		return -1;
	}

	ws->header.mask_bit = 0x1;
	ws->header.payload_len = datalen;

	if (_ws_get_random_mask(ws, &ws->header.mask, sizeof(uint32_t)) 
		!= sizeof(uint32_t))
	 {
	 	return -1;
	 }

	if (ws->send_state == WS_SEND_STATE_MESSAGE_BEGIN)
	{
		// Opcode will be set to either TEXT or BINARY here.
		assert((ws->header.opcode == WS_OPCODE_TEXT) 
			|| (ws->header.opcode == WS_OPCODE_BINARY));

		ws->send_state = WS_SEND_STATE_IN_MESSAGE;
	}
	else
	{
		// We've already sent frames.
		ws->header.opcode = WS_OPCODE_CONTINUATION;
	}

	ws_pack_header(&ws->header, header_buf, sizeof(header_buf), &header_len);
	
	if (_ws_send_data(ws, header_buf, (uint64_t)header_len, 0))
	{
		LIBWS_LOG(LIBWS_ERR, "Failed to send frame header");
		return -1;
	}

	return 0;
}

int ws_msg_frame_data_send(ws_t ws, char *data, uint64_t datalen)
{
	assert(ws);
	_WS_MUST_BE_CONNECTED(ws, "frame data send");

	if (ws->send_state != WS_SEND_STATE_IN_MESSAGE)
	{
		LIBWS_LOG(LIBWS_ERR, "Incorrect send state in frame data send");
		return -1;
	}

	// TODO: Don't touch original buffer as an option?	
	if (_ws_mask_payload(ws->header.mask, data, datalen))
	{
		LIBWS_LOG(LIBWS_ERR, "Failed to mask payload");
		return -1;
	}

	if (_ws_send_data(ws, data, datalen, 1))
	{
		LIBWS_LOG(LIBWS_ERR, "Failed to send frame data");
		return -1;
	}

	return 0;
}

int ws_msg_frame_send(ws_t ws, char *frame_data, uint64_t datalen)
{
	assert(ws);
	assert(frame_data);
	_WS_MUST_BE_CONNECTED(ws, "message frame send");

	if ((ws->send_state != WS_SEND_STATE_MESSAGE_BEGIN)
	 && (ws->send_state != WS_SEND_STATE_IN_MESSAGE))
	{
		LIBWS_LOG(LIBWS_ERR, "Incorrect send state in message frame send");
		return -1;
	}
	
	if (ws_msg_frame_data_begin(ws, datalen))
	{
		return -1;
	}

	// TODO: This can be chunked.
	if (ws_msg_frame_data_send(ws, frame_data, datalen))
	{
		return -1;
	}

	return 0;
}

int ws_msg_end(ws_t ws)
{
	assert(ws);
	_WS_MUST_BE_CONNECTED(ws, "message end");

	if (ws->send_state != WS_SEND_STATE_IN_MESSAGE)
	{
		LIBWS_LOG(LIBWS_ERR, "Incorrect send state in message end");
		return -1;
	}

	// TODO: Write a frame with FIN bit set.

	ws->send_state = WS_SEND_STATE_NONE;

	return 0;
}

int ws_send_msg(ws_t ws, char *msg, uint64_t len)
{
	uint64_t frame_len;
	assert(ws);
	_WS_MUST_BE_CONNECTED(ws, "send message");

	if (ws_msg_begin(ws))
	{
		return -1;
	}

	if (ws_msg_frame_send(ws, msg, len))
	{
		return -1;
	}

	/*
	TODO: Send in chunks based on max_frame_size
	while (1)
	{

		if (ws_msg_frame_send(ws, msg, len))
		{
			return -1;
		}

		if (ws->max_frame_size)
		{
			if (len > ws->max_frame_size)
			{
				break;
			}
		}
	}
	*/

	if (ws_msg_end(ws))
	{
		return -1;
	}

	return 0;
}

