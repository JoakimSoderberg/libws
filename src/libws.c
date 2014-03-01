
#include "libws_config.h"

#include <event2/dns.h>
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include <stdio.h>
#ifdef _WIN32
#include <WinSock2.h>
#else
#include <sys/socket.h>
#endif
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <stdint.h>
#include <inttypes.h>
#ifndef _WIN32
#include <unistd.h> // TODO: System introspection.
#endif
#include <string.h>
#include <signal.h>

#include "libws_types.h"
#include "libws_log.h"
#include "libws_header.h"
#include "libws_private.h"
#ifdef LIBWS_WITH_OPENSSL
#include "libws_openssl.h"
#endif
#include "libws.h"
#include "libws_handshake.h"
#include "libws_utf8.h"

void ws_set_memory_functions(ws_malloc_replacement_f malloc_replace,
							 ws_free_replacement_f free_replace,
							 ws_realloc_replacement_f realloc_replace)
{
	_ws_set_memory_functions(malloc_replace, free_replace, realloc_replace);
}

int ws_global_init(ws_base_t *base)
{
	ws_base_t b;

	assert(base);

	LIBWS_LOG(LIBWS_TRACE, "Global init");

	LIBWS_LOG(LIBWS_INFO, "Libevent version %s", event_get_version());

	if (!(*base = (ws_base_s *)_ws_calloc(1, sizeof(ws_base_s))))
	{
		LIBWS_LOG(LIBWS_CRIT, "Out of memory!");
		return -1;
	}

	b = *base;

	// Don't crash on Broken pipe for a socket.
	#ifndef _WIN32
	signal(SIGPIPE, SIG_IGN);

	if ((b->random_fd = open(WS_RANDOM_PATH, O_RDONLY)) < 0)
	{
		LIBWS_LOG(LIBWS_ERR, "Failed to open random source %s , %d",
							WS_RANDOM_PATH, b->random_fd);
		goto fail;
	}
	#endif

	// Create Libevent context.
	{
		if (!(b->ev_base = event_base_new()))
		{
			LIBWS_LOG(LIBWS_CRIT, "Out of memory!");
			goto fail;
		}

		// TODO: Passing 1 here fails on windows... dns/server regress test in Libevent does not.
		if (!(b->dns_base = evdns_base_new(b->ev_base, 1)))
		{
			LIBWS_LOG(LIBWS_CRIT, "Out of memory!");
			goto fail;
		}
	}

	#ifdef LIBWS_WITH_OPENSSL
	if (_ws_global_openssl_init(b))
	{
		LIBWS_LOG(LIBWS_CRIT, "Failed to init OpenSSL");
		goto fail;
	}
	#endif

	return 0;
fail:
	if (b->ev_base)
	{
		event_base_free(b->ev_base);
		b->ev_base = NULL;
	}

	if (*base)
	{
		_ws_free(*base);
		*base = NULL;
	}

	return -1;
}

void ws_global_destroy(ws_base_t *base)
{
	ws_base_t b;

	assert(base);

	b = *base;

	#ifndef WIN32
	if (close(b->random_fd))
	{
		LIBWS_LOG(LIBWS_ERR, "Failed to close random source: %s (%d)", 
							strerror(errno), errno);
	}
	#endif

	if (b->dns_base)
	{
		evdns_base_free(b->dns_base, 1);
		b->dns_base = NULL;
	}	

	if (b->ev_base)
	{
		event_base_free(b->ev_base);
		b->ev_base = NULL;
	}

	#ifdef LIBWS_WITH_OPENSSL
	_ws_global_openssl_destroy(b);
	#endif

	_ws_free(*base);
	*base = NULL;

	// TODO: Should we destroy all connections here as well?
}

int ws_init(ws_t *ws, ws_base_t ws_base)
{
	struct ws_s *w = NULL;

	assert(ws);
	assert(ws_base);

	if (!(*ws = (struct ws_s *)_ws_calloc(1, sizeof(struct ws_s))))
	{
		LIBWS_LOG(LIBWS_CRIT, "Out of memory!");
		return -1;
	}

	// Just for convenience.
	w = *ws;

	w->ws_base = ws_base;

	#ifdef LIBWS_WITH_OPENSSL
	if (_ws_openssl_init(w, ws_base))
	{
		LIBWS_LOG(LIBWS_ERR, "Failed to init OpenSSL");
		_ws_free(*ws);
		*ws = NULL;
		return -1;
	}
	#endif // LIBWS_WITH_OPENSSL

	w->state = WS_STATE_CLOSED_CLEANLY;

	return 0;
}

void ws_destroy(ws_t *ws)
{
	struct ws_s *w;
	
	if (!ws || !(*ws))
		return; 

	w = *ws;

	// TODO: If connected, send close frame with status WS_CLOSE_STATUS_GOING_AWAY_1001

	if (w->bev)
	{
		bufferevent_free(w->bev);
		w->bev = NULL;
	}

	if (w->origin)
	{
		_ws_free(w->origin);
	}

	_ws_destroy_event(&w->connect_timeout_event);
	_ws_destroy_event(&w->close_timeout_event);
	_ws_destroy_event(&w->pong_timeout_event);

	// Must be done after the bufferevent is freed.
	if (w->rate_limits)
	{
		ev_token_bucket_cfg_free(w->rate_limits);
		w->rate_limits = NULL;
	}

	ws_clear_subprotocols(w);

	if (w->handshake_key_base64) _ws_free(w->handshake_key_base64);
	if (w->server) _ws_free(w->server);
	if (w->uri) _ws_free(w->uri);


	#ifdef LIBWS_WITH_OPENSSL
	_ws_openssl_destroy(w);
	#endif

	_ws_free(w);
	*ws = NULL;
}

ws_base_t ws_get_base(ws_t ws)
{
	assert(ws);
	return ws->ws_base;
}

int ws_connect(ws_t ws, const char *server, int port, const char *uri)
{
	int ret = 0;
	struct evbuffer *out = NULL;
	assert(ws);

	LIBWS_LOG(LIBWS_DEBUG, "Connect start");

	if ((ws->state != WS_STATE_CLOSED_CLEANLY)
	 && (ws->state != WS_STATE_CLOSED_UNCLEANLY))
	{
		LIBWS_LOG(LIBWS_ERR, "Already connected or connecting");
		return -1;
	}

	if (!server)
	{
		LIBWS_LOG(LIBWS_ERR, "NULL server given");
		return -1;
	}

	if (ws->server) _ws_free(ws->server);
	ws->server = _ws_strdup(server);

	if (ws->uri) _ws_free(ws->uri);
	ws->uri = _ws_strdup(uri);

	ws->port = port;
	ws->received_close = 0;
	ws->sent_close = 0;
	ws->in_msg = 0;

	if (_ws_create_bufferevent_socket(ws))
	{
		LIBWS_LOG(LIBWS_ERR, "Failed to create bufferevent socket");
		ret = -1;
		goto fail;
	}

	out = bufferevent_get_output(ws->bev);
	
	if (bufferevent_socket_connect_hostname(ws->bev, 
				ws->ws_base->dns_base, AF_UNSPEC, ws->server, ws->port))
	{
		LIBWS_LOG(LIBWS_ERR, "Failed to create connect event");
		ret = -1;
		goto fail;
	}

	ws->state = WS_STATE_CONNECTING;

	// Setup a timeout event for the connection attempt.
	if (_ws_setup_connection_timeout(ws))
	{
		LIBWS_LOG(LIBWS_ERR, "Failed to setup connection timeout event");
		ret = -1;
		goto fail;
	}

	return 0;
fail:
	if (ws->server) _ws_free(ws->server);
	if (ws->uri) _ws_free(ws->uri);

	return -1;
}

int ws_close_with_status_reason(ws_t ws, ws_close_status_t status, 
							const char *reason, size_t reason_len)
{
	struct timeval tv;
	assert(ws);

	LIBWS_LOG(LIBWS_TRACE, "Sending close frame");

	ws->state = WS_STATE_CLOSING;

	// The underlying TCP connection, in most normal cases, SHOULD be closed
	// first by the server, so that it holds the TIME_WAIT state and not the
	// client (as this would prevent it from re-opening the connection for 2
	// maximum segment lifetimes (2MSL), while there is no corresponding
	// server impact as a TIME_WAIT connection is immediately reopened upon
	// a new SYN with a higher seq number).  In abnormal cases (such as not
	// having received a TCP Close from the server after a reasonable amount
	// of time) a client MAY initiate the TCP Close.  As such, when a server
	// is instructed to _Close the WebSocket Connection_ it SHOULD initiate
	// a TCP Close immediately, and when a client is instructed to do the
	// same, it SHOULD wait for a TCP Close from the server. 
	
	// Send a websocket close frame to the server.
	if (!ws->sent_close)
	{
		if (_ws_send_close(ws, status, reason, reason_len))
		{
			LIBWS_LOG(LIBWS_ERR, "Failed to send close frame");
			goto fail;
		}
	}

	ws->sent_close = 1;

	// Give the server time to initiate the closing of the
	// TCP session. Otherwise we'll force an unclean shutdown
	// ourselves.
	if (ws->close_timeout_event) event_free(ws->close_timeout_event);

	if (!(ws->close_timeout_event = evtimer_new(ws->ws_base->ev_base, 
									_ws_close_timeout_cb, (void *)ws)))
	{
		LIBWS_LOG(LIBWS_ERR, "Failed to create close timeout event");
		goto fail;
	}

	tv.tv_sec = 3; // TODO: Let the user set this.
	tv.tv_usec = 0;

	if (evtimer_add(ws->close_timeout_event, &tv))
	{
		LIBWS_LOG(LIBWS_ERR, "Failed to add close timeout event");
		goto fail;
	}

	return 0;

fail:

	// If we fail to send the close frame, we do a TCP close
	// right away (unclean websocket close).

	if (ws->close_timeout_event)
	{
		event_free(ws->close_timeout_event);
		ws->close_timeout_event = NULL;
	}

	LIBWS_LOG(LIBWS_ERR, "Failed to send close frame, "
						 "forcing unclean close");

	_ws_shutdown(ws);

	if (ws->close_cb)
	{
		char reason[] = "Problem sending close frame";
		ws->close_cb(ws, WS_CLOSE_STATUS_ABNORMAL_1006, 
					reason, sizeof(reason), ws->close_arg);
	}

	return -1;
}

int ws_close_with_status(ws_t ws, ws_close_status_t status)
{
	return ws_close_with_status_reason(ws, status, NULL, 0);
}

int ws_close(ws_t ws)
{
	return ws_close_with_status_reason(ws, 
			WS_CLOSE_STATUS_NORMAL_1000, NULL, 0);
}

int ws_base_service(ws_base_t base)
{
	assert(base);

	if (event_base_loop(base->ev_base, EVLOOP_NONBLOCK))
	{
		LIBWS_LOG(LIBWS_ERR, "Failed to feed event loop");
		return -1;
	}

	return 0;
}

int ws_base_service_blocking(ws_base_t base)
{
	int ret;
	assert(base);

	ret = event_base_dispatch(base->ev_base);

	return ret;
}

int ws_base_quit_delay(ws_base_t base, int let_running_events_complete, 
					const struct timeval *delay)
{
	int ret;
	assert(base);

	LIBWS_LOG(LIBWS_TRACE, "Websocket base quit");

	if (let_running_events_complete)
	{
		ret = event_base_loopexit(base->ev_base, delay);
	}
	else
	{
		ret = event_base_loopbreak(base->ev_base);
	}

	return ret;
}

int ws_base_quit(ws_base_t base, int let_running_events_complete)
{
	return ws_base_quit_delay(base, let_running_events_complete, NULL);
}

#if 0
int ws_service(ws_t ws)
{
	assert(ws);

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
	assert(ws);

	ret = event_base_dispatch(ws->base);

	ws->state = WS_STATE_DISCONNECTED;

	return ret;
}

int ws_quit(ws_t ws, int let_running_events_complete)
{
	int ret;
	assert(ws);

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
#endif

char *ws_get_uri(ws_t ws, char *buf, size_t bufsize)
{
	assert(ws);

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

void ws_mask_payload(uint32_t mask, char *msg, uint64_t len)
{
	size_t i;
	uint8_t *m = (uint8_t *)&mask;
	uint8_t *p = (uint8_t *)msg;
	
	if (!msg || !len)
		return;

	for (i = 0; i < len; i++)
	{
		p[i] ^= m[i % 4];
	}
}

void ws_unmask_payload(uint32_t mask, char *msg, uint64_t len)
{
	ws_mask_payload(mask, msg, len);
}

void ws_set_no_copy_cb(ws_t ws, ws_no_copy_cleanup_f func, void *extra)
{
	assert(ws);
	ws->no_copy_cleanup_cb = func;
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

int ws_msg_begin(ws_t ws)
{
	assert(ws);
	_WS_MUST_BE_CONNECTED(ws, "message begin");

	LIBWS_LOG(LIBWS_DEBUG, "Message begin");

	if (ws->send_state != WS_SEND_STATE_NONE)
	{
		return -1;
	}

	memset(&ws->header, 0, sizeof(ws_header_t));
	// FIN, RSVx bits are 0.
	ws->header.opcode = ws->binary_mode ? 
						WS_OPCODE_BINARY_0X2 : WS_OPCODE_TEXT_0X1;

	ws->send_state = WS_SEND_STATE_MESSAGE_BEGIN;
	
	return 0;
}

int ws_msg_frame_data_begin(ws_t ws, uint64_t datalen)
{
	uint8_t header_buf[WS_HDR_MAX_SIZE];
	size_t header_len = 0;

	assert(ws);
	_WS_MUST_BE_CONNECTED(ws, "frame data begin");

	LIBWS_LOG(LIBWS_DEBUG, "Message frame data begin, opcode 0x%x "
			"(send header)", ws->header.opcode, datalen);

	if ((ws->send_state != WS_SEND_STATE_MESSAGE_BEGIN)
	 && (ws->send_state != WS_SEND_STATE_IN_MESSAGE))
	{
		LIBWS_LOG(LIBWS_ERR, "Incorrect state for frame data begin");
		return -1;
	}

	if (datalen > WS_MAX_PAYLOAD_LEN)
	{
		LIBWS_LOG(LIBWS_ERR, "Payload length (0x%x) larger than max allowed "
							 "websocket payload (0x%x)",
							datalen, WS_MAX_PAYLOAD_LEN);
		return -1;
	}

	ws->header.mask_bit = 0x1;
	ws->header.payload_len = datalen;

	if (_ws_get_random_mask(ws, (char *)&ws->header.mask, sizeof(uint32_t)) 
		!= sizeof(uint32_t))
	{
	 	return -1;
	}

	// TODO: Use this function for WS_OPCODE_PING/PONG as well?
	if (ws->send_state == WS_SEND_STATE_MESSAGE_BEGIN)
	{
		// Opcode will be set to either TEXT or BINARY here.
		assert((ws->header.opcode == WS_OPCODE_TEXT_0X1) 
			|| (ws->header.opcode == WS_OPCODE_BINARY_0X2));

		ws->send_state = WS_SEND_STATE_IN_MESSAGE;
	}
	else
	{
		// We've already sent frames.
		ws->header.opcode = WS_OPCODE_CONTINUATION_0X0;
	}

	ws_pack_header(&ws->header, header_buf, sizeof(header_buf), &header_len);
	
	if (_ws_send_data(ws, (char *)header_buf, (uint64_t)header_len, 0))
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

	LIBWS_LOG(LIBWS_DEBUG, "Message frame data send");

	if (ws->send_state != WS_SEND_STATE_IN_MESSAGE)
	{
		LIBWS_LOG(LIBWS_ERR, "Incorrect send state in frame data send");
		return -1;
	}

	// TODO: Don't touch original buffer as an option?
	if (ws->header.mask_bit)
	{	
		ws_mask_payload(ws->header.mask, data, datalen);
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
	assert(frame_data || (!frame_data && (datalen == 0)));
	_WS_MUST_BE_CONNECTED(ws, "message frame send");

	LIBWS_LOG(LIBWS_DEBUG, "Message frame send");

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

	LIBWS_LOG(LIBWS_DEBUG, "Message end");

	if (ws->send_state != WS_SEND_STATE_IN_MESSAGE)
	{
		LIBWS_LOG(LIBWS_ERR, "Incorrect send state in message end");
		return -1;
	}

	// Write a frame with FIN bit set.
	ws->header.fin = 0x1;

	if (ws_msg_frame_send(ws, NULL, 0))
	{
		LIBWS_LOG(LIBWS_ERR, "Failed to send end of message frame");
		return -1;
	}

	ws->send_state = WS_SEND_STATE_NONE;

	return 0;
}

int ws_send_msg_ex(ws_t ws, char *msg, uint64_t len, int binary)
{
	int saved_binary_mode;
	assert(ws);
	_WS_MUST_BE_CONNECTED(ws, "send message");

	LIBWS_LOG(LIBWS_TRACE, "Send message start");

	saved_binary_mode = ws->binary_mode;
	ws->binary_mode = binary;

	// TODO: Use _ws_send_frame_raw if we're not fragmenting the message.
	if (len <= ws->max_frame_size)
	{
		return _ws_send_frame_raw(ws, 
				binary ? WS_OPCODE_BINARY_0X2 : WS_OPCODE_TEXT_0X1, 
				msg, len);
	}

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

	ws->binary_mode = saved_binary_mode;

	LIBWS_LOG(LIBWS_TRACE, "Send message end");

	return 0;
}

int ws_send_msg(ws_t ws, char *msg)
{
	int ret = 0;
	size_t len = 0;

	if (msg)
		len = strlen(msg);

	ret = ws_send_msg_ex(ws, msg, (uint64_t)len, 0);

	return ret;
}

int ws_set_max_frame_size(ws_t ws, uint64_t max_frame_size)
{
	assert(ws);

	if (max_frame_size > WS_MAX_PAYLOAD_LEN)
	{
		LIBWS_LOG(LIBWS_ERR, "Max frame size cannot exceed max payload length");
		return -1;
	}

	ws->max_frame_size = max_frame_size;

	return 0;
}

uint64_t ws_get_max_frame_size(ws_t ws)
{
	assert(ws);
	return ws->max_frame_size;
}

void ws_set_onconnect_cb(ws_t ws, ws_connect_callback_f func, void *arg)
{
	assert(ws);

	ws->connect_cb = func;
	ws->connect_arg = arg;
}

void ws_set_onmsg_cb(ws_t ws, ws_msg_callback_f func, void *arg)
{
	assert(ws);

	ws->msg_cb = func;
	ws->msg_arg = arg;
}

void ws_set_onmsg_begin_cb(ws_t ws, ws_msg_begin_callback_f func, void *arg)
{
	assert(ws);

	ws->msg_begin_cb = func;
	ws->msg_begin_arg = arg;
}

void ws_set_onmsg_frame_cb(ws_t ws, ws_msg_frame_callback_f func, void *arg)
{
	assert(ws);

	ws->msg_frame_cb = func;
	ws->msg_frame_arg = arg;
}

void ws_set_onmsg_end_cb(ws_t ws, ws_msg_end_callback_f func, void *arg)
{
	assert(ws);

	ws->msg_end_cb = func;
	ws->msg_end_arg = arg;
}

void ws_set_onmsg_frame_begin_cb(ws_t ws, ws_msg_frame_begin_callback_f func, 
								void *arg)
{
	assert(ws);

	ws->msg_frame_begin_cb = func;
	ws->msg_frame_begin_arg = arg;
}

void ws_set_onmsg_frame_data_cb(ws_t ws, ws_msg_frame_data_callback_f func, 
								void *arg)
{
	assert(ws);

	ws->msg_frame_data_cb = func;
	ws->msg_frame_data_arg = arg;
}

void ws_set_onmsg_frame_end_cb(ws_t ws, ws_msg_frame_end_callback_f func, 
								void *arg)
{
	assert(ws);

	ws->msg_frame_end_cb = func;
	ws->msg_frame_end_arg = arg;
}

void ws_set_onerr_cb(ws_t ws, ws_err_callback_f func, void *arg)
{
	assert(ws);

	ws->err_cb = func;
	ws->err_arg = arg;
}

void ws_set_onclose_cb(ws_t ws, ws_close_callback_f func, void *arg)
{
	assert(ws);

	ws->close_cb = func;
	ws->close_arg = arg;
}

int ws_set_origin(ws_t ws, const char *origin)
{
	assert(ws);

	// TODO: Verify that origin is a valid value.
	if (ws->origin)
	{
		_ws_free(ws->origin);
	}

	if (!(ws->origin = _ws_strdup(origin)))
	{
		LIBWS_LOG(LIBWS_ERR, "Could not copy origin string. Out of memory!");
		return -1;
	}

	return 0;
}

void ws_onping_default_cb(ws_t ws, char *msg, uint64_t len, 
						int binary, void *arg)
{
	assert(ws);

	if (ws_send_pong(ws, msg, (size_t)len, binary))
	{
		LIBWS_LOG(LIBWS_ERR, "Failed to send pong");
	}
}

void ws_set_onping_cb(ws_t ws, ws_msg_callback_f func, void *arg)
{
	assert(ws);

	ws->ping_cb = func ? func : ws_onping_default_cb;
	ws->ping_arg = arg;
}

void ws_onpong_default_cb(ws_t ws, char *msg, uint64_t len, 
						int binary, void *arg)
{
	assert(ws);

	// Does nothing.
	// TODO: In the future we maybe want to make sure the reply is the same as the ping.
}

void ws_set_onpong_cb(ws_t ws, ws_msg_callback_f func, void *arg)
{
	assert(ws);

	ws->pong_cb = func ? func : ws_onpong_default_cb;
	ws->pong_arg = arg;
}

void ws_set_pong_timeout_cb(ws_t ws, ws_timeout_callback_f func, 
							struct timeval timeout, void *arg)
{
	assert(ws);

	ws->pong_timeout_cb = func;
	ws->pong_timeout_arg = arg;
}

void ws_set_binary(ws_t ws, int binary)
{
	assert(ws);
	ws->binary_mode = binary;
}

int ws_add_header(ws_t ws, const char *header, const char *value)
{
	assert(ws);
	assert(header);
	assert(value);

	// TODO: Add http header add code.
	return 0;
}

int ws_remove_header(ws_t ws, const char *header)
{
	assert(ws);

	if (!header)
	{
		return -1;
	}

	// TODO: Add http header remove code.
	return 0;
}

int ws_is_connected(ws_t ws)
{
	assert(ws);
	return  (ws->state == WS_STATE_CONNECTED);
}

void ws_set_recv_timeout_cb(ws_t ws, ws_timeout_callback_f func, 
						struct timeval recv_timeout, void *arg)
{
	assert(ws);
	
	ws->recv_timeout_cb = func;
	ws->recv_timeout = recv_timeout;
	ws->recv_timeout_arg = arg;

	_ws_set_timeouts(ws);
}

void ws_set_send_timeout_cb(ws_t ws, ws_timeout_callback_f func, 
						struct timeval send_timeout, void *arg)
{
	assert(ws);

	ws->send_timeout_cb = func;
	ws->send_timeout = send_timeout;
	ws->send_timeout_arg = arg;

	_ws_set_timeouts(ws);
}

void ws_set_connect_timeout_cb(ws_t ws, ws_timeout_callback_f func,
						struct timeval connect_timeout, void *arg)
{
	assert(ws);

	ws->connect_timeout_cb = func;
	ws->connect_timeout = connect_timeout;
	ws->connect_timeout_arg = arg;
}

int ws_send_ping_ex(ws_t ws, char *msg, size_t len)
{
	assert(ws);

	if (_ws_send_frame_raw(ws, WS_OPCODE_PING_0X9, msg, (uint64_t)len))
	{
		return -1;
	}

	if (ws->pong_timeout_cb && _ws_setup_pong_timeout(ws))
	{
		LIBWS_LOG(LIBWS_ERR, "Failed to setup pong timeout callback");
	}

	return 0;
}

int ws_send_ping(ws_t ws)
{
	assert(ws);
	return ws_send_ping_ex(ws, NULL, 0);
}

int ws_send_pong(ws_t ws, char *msg, size_t len, int binary)
{
	assert(ws);

	LIBWS_LOG(LIBWS_DEBUG, "Send pong %*s", (int)len, msg);

	if (_ws_send_frame_raw(ws, WS_OPCODE_PONG_0XA, msg, (uint64_t)len))
	{
		return -1;
	}

	return 0;
}

int ws_add_subprotocol(ws_t ws, const char *subprotocol)
{
	assert(ws);

	if (!(ws->subprotocols = (char **)_ws_realloc(ws->subprotocols, 
								(ws->num_subprotocols + 1) * sizeof(char *))))
	{
		LIBWS_LOG(LIBWS_ERR, "Out of memory!");
		return -1;
	}

	ws->subprotocols[ws->num_subprotocols] = strdup(subprotocol);
	ws->num_subprotocols++;

	return 0;
}

size_t ws_get_subprotocol_count(ws_t ws)
{
	assert(ws);
	return ws->num_subprotocols;
}

char **ws_get_subprotocols(ws_t ws, size_t *count)
{
	assert(ws);
	size_t i;
	char **ret = NULL;
	size_t lengths = 0;

	if (!ws->subprotocols)
		return NULL;

	if (!(ret = (char **)_ws_malloc(ws->num_subprotocols * sizeof(char *))))
	{
		LIBWS_LOG(LIBWS_ERR, "Out of memory!"); 
		return NULL;
	}

	for (i = 0; i < ws->num_subprotocols; i++)
	{
		if (!(ret[i] = _ws_strdup(ws->subprotocols[i])))
		{
			goto fail;
		}
	}

	*count = ws->num_subprotocols;

	return ret;
fail:
	ws_free_subprotocols_list(ret, i);

	return NULL;
}

void ws_free_subprotocols_list(char **subprotocols, size_t count)
{
	size_t i;

	if (!subprotocols)
		return;

	for (i = 0; i < count; i++)
	{
		_ws_free(subprotocols[i]);
	}

	_ws_free(subprotocols);
}

int ws_clear_subprotocols(ws_t ws)
{
	size_t i;
	assert(ws);

	if (!ws->subprotocols)
		return 0;

	for (i = 0; i < ws->num_subprotocols; i++)
	{
		if (ws->subprotocols[i])
			_ws_free(ws->subprotocols[i]);
	}

	_ws_free(ws->subprotocols);
	ws->subprotocols = NULL;
	ws->num_subprotocols = 0;

	return 0;
}

void ws_default_msg_begin_cb(ws_t ws, void *arg)
{
	assert(ws);

	LIBWS_LOG(LIBWS_TRACE, "Default message begin callback "
							"(setup message buffer)");

	if (ws->msg)
	{
		if (evbuffer_get_length(ws->msg) != 0)
		{
			LIBWS_LOG(LIBWS_WARN, "Non-empty message buffer on new message");
		}
	
		evbuffer_free(ws->msg);
		ws->msg = NULL;
	}

	if (!(ws->msg = evbuffer_new()))
	{
		// TODO: Close connection. Internal error error code.
		return;
	}
}

void ws_default_msg_frame_cb(ws_t ws, char *payload, 
							uint64_t len, void *arg)
{
	assert(ws);

	LIBWS_LOG(LIBWS_TRACE, "Default message frame callback "
							"(append data to message)");
	
	// Finally we append the frame payload to the message payload.
	evbuffer_add_buffer(ws->msg, ws->frame_data);
}

void ws_default_msg_end_cb(ws_t ws, void *arg)
{
	size_t len;
	unsigned char *payload;
	assert(ws);

	LIBWS_LOG(LIBWS_TRACE, "Default message end callback "
							"(Calls the on message callback)");
	
	// Finalize the message by adding a null char.
	// TODO: No null for binary?
	evbuffer_add_printf(ws->msg, "");

	len = evbuffer_get_length(ws->msg);
	payload = evbuffer_pullup(ws->msg, len);

	if (!ws_utf8_isvalid((unsigned char *)payload))
	{
		// TODO: Do this earlier (needs to support incomplete UTF8 however)
		LIBWS_LOG(LIBWS_ERR, "Invalid UTF8");
		ws_close_with_status(ws, WS_CLOSE_STATUS_INCONSISTENT_DATA_1007);
	}
	else
	{
		LIBWS_LOG(LIBWS_DEBUG2, "Message received of length %lu:\n%s", len, payload);

		if (ws->msg_cb)
		{
			LIBWS_LOG(LIBWS_DEBUG, "Calling message callback");
			ws->msg_cb(ws, (char *)payload, len,
				(ws->header.opcode == WS_OPCODE_BINARY_0X2), ws->msg_arg);
		}
		else
		{
			LIBWS_LOG(LIBWS_DEBUG, "No message callback set, drop message");
		}
	}

	evbuffer_free(ws->msg);
	ws->msg = NULL;
}

void ws_default_msg_frame_begin_cb(ws_t ws, void *arg)
{
	assert(ws);

	LIBWS_LOG(LIBWS_TRACE, "Default message frame begin callback "
							"(Sets up the frame data buffer)");

	// Setup the frame payload buffer.
	if (ws->frame_data)
	{
		if (evbuffer_get_length(ws->frame_data) != 0)
		{
			LIBWS_LOG(LIBWS_WARN, "Non-empty message buffer on new frame");
			// TODO: This should probably fail somehow...
		}

		// TODO: Should we really free it? Not just do evbuffer_remove with a NULL dest?
		evbuffer_free(ws->frame_data);
		ws->frame_data = NULL;
	}

	if (!(ws->frame_data = evbuffer_new()))
	{
		// TODO: Close connection. Internal error reason.
		return;
	}
}

void ws_default_msg_frame_data_cb(ws_t ws, char *payload, 
								uint64_t len, void *arg)
{
	assert(ws);

	LIBWS_LOG(LIBWS_TRACE, "Default message frame data callback "
							"(Append data to frame data buffer)");

	evbuffer_add(ws->frame_data, payload, (size_t)len);
}

void ws_default_msg_frame_end_cb(ws_t ws, void *arg)
{
	assert(ws);

	LIBWS_LOG(LIBWS_TRACE, "Default message frame end callback "
							"(Calls the message frame callback)");

	if (ws->msg_frame_cb)
	{
		size_t frame_len = evbuffer_get_length(ws->frame_data);
		const unsigned char *payload = evbuffer_pullup(ws->frame_data, frame_len);
		
		LIBWS_LOG(LIBWS_DEBUG, "Calling message frame callback");
		ws->msg_frame_cb(ws, (char *)payload, frame_len, arg);
	}
	else
	{
		// This will put the frame data in the message
		// don't bother with passing any payload, 
		// the internal buffers are used anyway.
		ws_default_msg_frame_cb(ws, NULL, 0, arg);
	}

	evbuffer_free(ws->frame_data);
	ws->frame_data = NULL;
}

#ifdef LIBWS_WITH_OPENSSL

void ws_set_ssl_state(ws_t ws, libws_ssl_state_t ssl)
{
	assert(ws);
	ws->use_ssl = ssl;
}

#endif // LIBWS_WITH_OPENSSL

const char *ws_parse_state_to_string(ws_parse_state_t state)
{
	switch (state)
	{
		case WS_PARSE_STATE_ERROR: return "Error";
		case WS_PARSE_STATE_SUCCESS: return "Success";
		case WS_PARSE_STATE_NEED_MORE: return "Need more";
		case WS_PARSE_STATE_USER_ABORT: return "User abort";
		default: return "Unknown";
	}
}

void ws_set_rate_limits(ws_t ws, size_t read_rate, size_t read_burst, 
						size_t write_rate, size_t write_burst)
{
	struct timeval tv;
	assert(ws);
	assert(ws->bev);

	// Get rid of any old rate limiting.
	if (bufferevent_set_rate_limit(ws->bev, NULL))
	{
		LIBWS_LOG(LIBWS_ERR, "Failed to turn off rate limit");
	}

	if (ws->rate_limits)
	{
		ev_token_bucket_cfg_free(ws->rate_limits);
		ws->rate_limits = NULL;
	}

	// Turn off rate limiting.
	if ((read_rate == 0) && (read_burst == 0)
	 && (write_rate == 0) && (write_burst == 0))
	{
		return;
	}

	// Create a rate limiting token bucket.
	tv.tv_sec = 1;
	ws->rate_limits = ev_token_bucket_cfg_new(read_rate, read_burst, 
											  write_rate, write_burst, &tv);


}

