
#include "libws_config.h"

#include <stdio.h>
#include <assert.h>

#ifdef WIN32
#define _CRT_RAND_S
#include <stdlib.h>
#endif

#include <sys/time.h>
#include <unistd.h>
#include <string.h>

#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include "libws_log.h"
#include "libws_types.h"
#include "libws_header.h"
#include "libws_private.h"
#include "libws.h"
#include "libws_handshake.h"

#ifdef LIBWS_WITH_OPENSSL
#include "libws_openssl.h"
#endif 

static ws_malloc_replacement_f 	replaced_ws_malloc = NULL;
static ws_free_replacement_f	replaced_ws_free = NULL;
static ws_realloc_replacement_f	replaced_ws_realloc = NULL;

void *_ws_malloc(size_t size)
{
	if (size == 0)
		return NULL;

	if (replaced_ws_malloc)
		return replaced_ws_malloc(size);
	
	return malloc(size);
}

void *_ws_realloc(void *ptr, size_t size)
{
	return replaced_ws_realloc ? replaced_ws_realloc(ptr, size) : realloc(ptr, size);
}

void _ws_free(void *ptr)
{
	if (replaced_ws_free)
		replaced_ws_free(ptr);
	else
		free(ptr);
}

void *_ws_calloc(size_t count, size_t size)
{
	void *p = NULL;

	if (!count || !size)
		return NULL;

	if (replaced_ws_malloc)
	{
		size_t sz = count * size;
		// TODO: If count > (size_t max / size), goto fail.
		p = replaced_ws_malloc(sz);

		if (p)
			return memset(p, 0, sz);
	}

	p = calloc(count, size);

	#ifdef WIN32
	// Windows doesn't set ENOMEM properly.
	if (!p)
		goto fail;
	#endif

	return p;
fail:
	errno = ENOMEM;
	return NULL;
}

char *_ws_strdup(const char *str)
{
	if (!str)
	{
		errno = EINVAL;
		return NULL;
	}

	if (replaced_ws_malloc)
	{
		size_t len = strlen(str);
		void *p = NULL;
		
		// TODO: goto fail if len == size_t max.

		if ((p = replaced_ws_malloc(len + 1)))
		{
			return memcpy(p, str, len + 1);
		}
	}
	else
	{	
		#ifdef WIN32
		return _strdup(str);
		#else
		return strdup(str);
		#endif
	}
fail:
	errno = ENOMEM;
	return NULL;
}

void _ws_set_memory_functions(ws_malloc_replacement_f malloc_replace,
							 ws_free_replacement_f free_replace,
							 ws_realloc_replacement_f realloc_replace)
{

	replaced_ws_malloc = malloc_replace;
	replaced_ws_free = free_replace;
	replaced_ws_realloc = realloc_replace;

	event_set_mem_functions(malloc_replace, realloc_replace, free_replace);

	#ifdef LIBWS_WITH_OPENSSL
	CRYPTO_set_mem_functions(malloc_replace, realloc_replace, free_replace);
	#endif
}

///
/// Event for when a connection attempt times out.
///
static void _ws_connection_timeout_event(evutil_socket_t fd, short what, void *arg)
{
	char buf[256];
	ws_t ws = (ws_t)arg;
	assert(ws);

	LIBWS_LOG(LIBWS_ERR, "Websocket connection timed out after %ld seconds "
						 "for %s", ws->connect_timeout.tv_sec, 
						 ws_get_uri(ws, buf, sizeof(buf)));

	if (ws->connect_timeout_cb)
	{
		ws->connect_timeout_cb(ws, ws->connect_timeout, ws->connect_timeout_arg);
	}
}

static void _ws_pong_timeout_event(evutil_socket_t fd, short what, void *arg)
{
	ws_t ws = (ws_t)arg;
	assert(ws);

	// TODO: Make sure we delete this event if the ws->pong_timeout_cb is set to NULL while waiting for event to time out.

	if (ws->pong_timeout_cb)
	{
		ws->pong_timeout_cb(ws, ws->pong_timeout, ws->pong_arg);
	}
}

static int _ws_setup_timeout_event(ws_t ws, event_callback_fn func, 
									struct event **ev, struct timeval *tv)
{
	assert(ws);
	assert(ev);
	assert(func);
	assert(tv);

	LIBWS_LOG(LIBWS_TRACE, "Setting up new timeout event");

	if (*ev)
	{
		event_free(*ev);
		*ev = NULL;
	}

	if (!(*ev = evtimer_new(ws->ws_base->ev_base, 
							func, (void *)ws)))
	{
		LIBWS_LOG(LIBWS_ERR, "Failed to create timeout event");
		return -1;
	}

	if (evtimer_add(*ev, tv))
	{
		LIBWS_LOG(LIBWS_ERR, "Failed to add timeout event");
		event_free(*ev);
		*ev = NULL;
		return -1;
	}

	return 0;
}

int _ws_setup_pong_timeout(ws_t ws)
{
	assert(ws);
	return _ws_setup_timeout_event(ws, _ws_pong_timeout_event,
				&ws->pong_timeout_event, &ws->pong_timeout);
}

int _ws_setup_connection_timeout(ws_t ws)
{	
	struct timeval tv = {WS_DEFAULT_CONNECT_TIMEOUT, 0};
	assert(ws);
	 
	if (ws->connect_timeout.tv_sec > 0)
	{
		tv = ws->connect_timeout;
	}

	return _ws_setup_timeout_event(ws, _ws_connection_timeout_event, 
									&ws->connect_timeout_event, &tv);
}

static int _ws_handle_close_frame(ws_t ws)
{
	ws_header_t *h;
	assert(ws);
	LIBWS_LOG(LIBWS_TRACE, "Close frame");

	h = &ws->header;

	ws->server_close_status = (uint16_t)WS_CLOSE_STATUS_NORMAL_1000;
	ws->server_reason = NULL;
	ws->server_reason_len = 0;

	if (ws->close_timeout_event)
	{
		event_free(ws->close_timeout_event);
		ws->close_timeout_event = NULL;
	}

	ws->state = WS_STATE_CLOSING;
	ws->received_close = 1;

	// The Close frame MAY contain a body (the "Application data" portion of
		// the frame) that indicates a reason for closing.
	// If there is a body, the first two bytes of
	// the body MUST be a 2-byte unsigned integer (in network byte order)
	// representing a status code
	if (ws->ctrl_len > 0)
	{
		if (ws->ctrl_len < 2)
		{
			LIBWS_LOG(LIBWS_ERR, "Close frame application data lacking "
								 "status code");
			// TODO: We should shutdown immediately here.
		}
		else
		{
			// Read status and reason.
			if (h->mask_bit)
			{
				ws_unmask_payload(h->mask, ws->ctrl_payload, ws->ctrl_len);
			}

			ws->server_close_status = 
				(ws_close_status_t)ntohs(*((uint16_t *)ws->ctrl_payload));
			ws->server_reason = &ws->ctrl_payload[2];
			ws->server_reason_len = ws->ctrl_len - 2;
		}
	}

	// If an endpoint receives a Close frame and did not previously send a
	// Close frame, the endpoint MUST send a Close frame in response.  (When
	// sending a Close frame in response, the endpoint typically echos the
	// status code it received.)  It SHOULD do so as soon as practical.  An
	// endpoint MAY delay sending a Close frame until its current message is
	// sent (for instance, if the majority of a fragmented message is
	// already sent, an endpoint MAY send the remaining fragments before
	// sending a Close frame).  However, there is no guarantee that the
	// endpoint that has already sent a Close frame will continue to process
	// data.
	if (!ws->sent_close)
	{
		ws_close_with_status_reason(ws, 
			ws->server_close_status, 
			ws->server_reason, 
			ws->server_reason_len);
	}	
}

int _ws_handle_control_frame(ws_t ws)
{
	ws_header_t *h;
	assert(ws);
	LIBWS_LOG(LIBWS_TRACE, "Control frame");

	h = &ws->header;

	assert(WS_OPCODE_IS_CONTROL(h->opcode));

	if (h->opcode == WS_OPCODE_CLOSE_0X8)
	{
		_ws_handle_close_frame(ws);
	}

	return 0;
}

int _ws_handle_frame_begin(ws_t ws)
{
	assert(ws);

	LIBWS_LOG(LIBWS_TRACE, "Frame begin, opcode = %d", ws->header.opcode);

	ws->recv_frame_len = 0;

	if (WS_OPCODE_IS_CONTROL(ws->header.opcode))
	{
		LIBWS_LOG(LIBWS_DEBUG, "  Control frame");
		memset(ws->ctrl_payload, 0, sizeof(ws->ctrl_payload));
		return 0;
	}

	LIBWS_LOG(LIBWS_DEBUG, "  Normal frame");

	// Normal frame.
	if (!ws->in_msg)
	{
		ws->in_msg = 1;

		if (ws->msg_begin_cb)
		{
			LIBWS_LOG(LIBWS_DEBUG, "Call message begin callback");
			ws->msg_begin_cb(ws, ws->msg_begin_arg);
		}
		else
		{
			ws_default_msg_begin_cb(ws, ws->msg_begin_arg);
		}
	}

	if (ws->msg_frame_begin_cb)
	{
		LIBWS_LOG(LIBWS_DEBUG, "Call frame begin callback");
		ws->msg_frame_begin_cb(ws, ws->msg_frame_begin_arg);
	}
	else
	{
		ws_default_msg_frame_begin_cb(ws, ws->msg_frame_begin_arg);
	}

	return 0;
}

int _ws_handle_frame_data(ws_t ws, char *buf, size_t len)
{
	int ret = 0;
	assert(ws);

	if (WS_OPCODE_IS_CONTROL(ws->header.opcode))
	{
		size_t total_len = (ws->ctrl_len + len);

		if (total_len > WS_CONTROL_MAX_PAYLOAD_LEN)
		{
			LIBWS_LOG(LIBWS_ERR, "Control payload too big %u, only %u allowed",
						total_len, WS_CONTROL_MAX_PAYLOAD_LEN);
			
			// Copy the remaining data into the buf.
			len = WS_CONTROL_MAX_PAYLOAD_LEN - ws->ctrl_len;
			// TODO: Set protocol violation error status here. (This will then be handled in the read callback)
			ret = -1;
		}

		memcpy(&ws->ctrl_payload[ws->ctrl_len], buf, len);
		ws->ctrl_len += len;
		
		return ret;
	}

	if (ws->msg_frame_data_cb)
	{
		ws->msg_frame_data_cb(ws, buf, len, ws->msg_frame_data_arg);
	}
	else
	{
		ws_default_msg_frame_data_cb(ws, buf, len, ws->msg_frame_data_arg);
	}

	return ret;
}

int _ws_handle_frame_end(ws_t ws)
{
	assert(ws);
	LIBWS_LOG(LIBWS_DEBUG2, "Frame end, opcode = %d", ws->header.opcode);

	if (WS_OPCODE_IS_CONTROL(ws->header.opcode))
	{
		return _ws_handle_control_frame(ws);
	}

	if (ws->msg_frame_end_cb)
	{
		ws->msg_frame_end_cb(ws, ws->msg_frame_end_arg);
	}
	else
	{
		ws_default_msg_frame_end_cb(ws, ws->msg_frame_end_arg);
	}

	if (ws->header.fin)
	{
		if (ws->msg_end_cb)
		{
			ws->msg_end_cb(ws, ws->msg_end_arg);
		}
		else
		{
			ws_default_msg_end_cb(ws, ws->msg_end_arg);
		}

		ws->in_msg = 0;
	}

	ws->has_header = 0;

	return 0;
}

void _ws_read_websocket(ws_t ws, struct evbuffer *in)
{
	assert(ws);
	assert(ws->bev);
	assert(in);

	LIBWS_LOG(LIBWS_DEBUG2, "Read websocket data");

	// First read the websocket header.
	if (!ws->has_header)
	{
		size_t header_len;
		ev_ssize_t bytes_read;
		char header_buf[WS_HDR_MAX_SIZE];
		ws_parse_state_t state;

		LIBWS_LOG(LIBWS_DEBUG2, "Read websocket header");

		bytes_read = evbuffer_copyout(in, (void *)header_buf, 
										sizeof(header_buf));

		state = ws_unpack_header(&ws->header, &header_len, 
				(unsigned char *)header_buf, sizeof(header_buf));

		assert(state != WS_PARSE_STATE_USER_ABORT);

		switch (state)
		{
			case WS_PARSE_STATE_SUCCESS: 
			{
				ws_header_t *h = &ws->header;
				ws->has_header = 1;

				LIBWS_LOG(LIBWS_DEBUG2, "Got header:\n"
					"fin = %d, rsv = {%d,%d,%d}, mask_bit = %d, opcode = %d, "
					"mask = %x, len = %d", 
					h->fin, h->rsv1, h->rsv2, h->rsv3, h->mask_bit, 
					h->opcode, h->mask, h->payload_len);

				if (evbuffer_drain(in, header_len))
				{
					// TODO: Error! close
				}
				break;
			}
			case WS_PARSE_STATE_NEED_MORE: return;
			case WS_PARSE_STATE_ERROR:
				// TODO: raise error callback with websocket error.
				break;
		}

		_ws_handle_frame_begin(ws);
	}
	
	if (ws->has_header)
	{
		// We're in a frame.
		size_t recv_len = evbuffer_get_length(in);
		size_t remaining = ws->header.payload_len - ws->recv_frame_len;

		LIBWS_LOG(LIBWS_DEBUG2, "In frame (remaining %u bytes)", remaining);

		if (recv_len > remaining) 
			recv_len = remaining;

		if (remaining == 0)
		{
			_ws_handle_frame_end(ws);
		}
		else
		{
			int bytes_read;
			char *buf = (char *)_ws_malloc(recv_len);

			// TODO: Maybe we should only do evbuffer_pullup here instead and pass that pointer on instead.
			bytes_read = evbuffer_remove(in, buf, recv_len);
			ws->recv_frame_len += bytes_read;

			if (bytes_read != recv_len)
			{
				LIBWS_LOG(LIBWS_ERR, "Wanted to read %u but only got %d", 
						recv_len, bytes_read);
			}

			if (ws->header.mask_bit)
			{
				ws_unmask_payload(ws->header.mask, buf, bytes_read);
			}

			LIBWS_LOG(LIBWS_DEBUG2, "Read %u bytes of the %u total. "
					"Expecting %u payload bytes", 
					bytes_read, ws->recv_frame_len, 
					ws->header.payload_len);

			if (_ws_handle_frame_data(ws, buf, bytes_read))
			{
				// TODO: Raise protocol error via error cb.
				// TODO: Close connection.
				LIBWS_LOG(LIBWS_ERR, "Failed to handle frame data");
			}
			else
			{
				// TODO: This is not hit in some cases.
				LIBWS_LOG(LIBWS_DEBUG2, "recv_frame_len = %u, payload_len = %u", ws->recv_frame_len, ws->header.payload_len);
				// The entire frame has been received.
				if (ws->recv_frame_len == ws->header.payload_len)
				{
					_ws_handle_frame_end(ws);
				}
			}

			_ws_free(buf);
		}
	}
}

///
/// Libevent bufferevent callback for when there is data to be read
/// on the websocket socket.
///
static void _ws_read_callback(struct bufferevent *bev, void *ptr)
{
	ws_t ws = (ws_t)ptr;
	struct evbuffer *in;
	assert(ws);
	assert(bev);
	assert(ws->bev == bev);

	LIBWS_LOG(LIBWS_DEBUG, "Read callback");

	in = bufferevent_get_input(ws->bev);

	if (ws->connect_state != WS_CONNECT_STATE_HANDSHAKE_COMPLETE)
	{
		// Complete the connection handshake.
		ws_parse_state_t state;

		LIBWS_LOG(LIBWS_DEBUG, "Look for handshake reply");

		switch ((state = _ws_read_server_handshake_reply(ws, in)))
		{
			case WS_PARSE_STATE_ERROR: break; // TODO: Close connection.
			case WS_PARSE_STATE_NEED_MORE: return;
			case WS_PARSE_STATE_SUCCESS:
			{
				ws->state = WS_STATE_CONNECTED;

				if (ws->connect_cb)
				{
					LIBWS_LOG(LIBWS_DEBUG, "Calling connect callback");
					ws->connect_cb(ws, ws->connect_arg);
				}
				
				return;
			}
		}
	}
	else
	{
		// Connected and completed handshake we can now expect websocket data.
		_ws_read_websocket(ws, in);
	}
}

///
/// Libevent bufferevent callback for when a write is done on
/// the websocket socket.
///
static void _ws_write_callback(struct bufferevent *bev, void *ptr)
{
	ws_t ws = (ws_t)ptr;
	assert(ws);
	assert(bev);

	LIBWS_LOG(LIBWS_DEBUG, "Write callback");
}

static void _ws_connected_event(struct bufferevent *bev, short events, void *arg)
{
	ws_t ws = (ws_t)arg;
	assert(ws);
	char buf[1024];
	LIBWS_LOG(LIBWS_DEBUG, "Connected to %s", ws_get_uri(ws, buf, sizeof(buf)));

	if (ws->connect_timeout_event)
	{
		LIBWS_LOG(LIBWS_DEBUG, "Freeing connect timeout event");
		event_free(ws->connect_timeout_event);
		ws->connect_timeout_event = NULL;
	}

	bufferevent_enable(ws->bev, EV_READ | EV_WRITE);

	#ifdef LIBWS_WITH_OPENSSL
	{
		int rc = SSL_get_verify_result(ws->ssl);

		if(rc != X509_V_OK) 
		{
  			if (rc == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT 
  			 || rc == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN)
  			{
  				LIBWS_LOG(LIBWS_DEBUG, "Server using a self-signed certificate");
  				// TODO: Fail if use_ssl is not set to allow self-signed.
  			}
  		}
	}
	#endif

	// Add the handshake to the send buffer, this will
	// be sent as soon as we're connected.
	if (_ws_send_handshake(ws, bufferevent_get_output(ws->bev)))
	{
		LIBWS_LOG(LIBWS_ERR, "Failed to assemble handshake");
		return;
	}
}

static void _ws_eof_event(struct bufferevent *bev, short events, void *ptr)
{
	ws_t ws = (ws_t)ptr;
	ws_close_status_t status;
	struct evbuffer *in;
	assert(ws);

	LIBWS_LOG(LIBWS_TRACE, "EOF event");

	in = bufferevent_get_input(ws->bev);

	if (evbuffer_get_length(in) > 0)
	{
		LIBWS_LOG(LIBWS_DEBUG, "Left %u bytes at EOF", evbuffer_get_length(in));

		_ws_read_websocket(ws, in);
	}

	status = ws->server_close_status;

	LIBWS_LOG(LIBWS_DEBUG, "Sent close frame %d, received close frame %d", 
							ws->sent_close, ws->received_close);

	_ws_shutdown(ws);

	if (!ws->received_close)
	{
		ws->state = WS_STATE_CLOSED_UNCLEANLY;
		status = WS_CLOSE_STATUS_ABNORMAL_1006;
	}

	if (ws->close_cb)
	{
		LIBWS_LOG(LIBWS_DEBUG, "Call close callback");

		ws->close_cb(ws, 
			status,
			ws->server_reason,
			ws->server_reason_len,
			ws->close_arg);
	}
	else
	{
		LIBWS_LOG(LIBWS_DEBUG, "No close callback");
	}
}

static void _ws_error_event(struct bufferevent *bev, short events, void *ptr)
{
	const char *err_msg;
	int err;
	ws_t ws = (ws_t)ptr;
	assert(ws);

	LIBWS_LOG(LIBWS_DEBUG, "Error raised");

	if (ws->state == WS_STATE_DNS_LOOKUP)
	{
		err = bufferevent_socket_get_dns_error(ws->bev);
		err_msg = evutil_gai_strerror(err);

		LIBWS_LOG(LIBWS_ERR, "DNS error %d: %s", err, err_msg);
	}
	else
	{
		err = EVUTIL_SOCKET_ERROR();
		err_msg = evutil_socket_error_to_string(err);

		LIBWS_LOG(LIBWS_ERR, "%s (%d)", err_msg, err);
	}

	// TODO: Should there even be an erro callback?
	if (ws->err_cb)
	{
		ws->err_cb(ws, err, err_msg, ws->err_arg);
	}
	else
	{
		ws_close(ws);
	}
}

///
/// Libevent bufferevent callback for when an event occurs on
/// the websocket socket.
///
static void _ws_event_callback(struct bufferevent *bev, short events, void *ptr)
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

int _ws_create_bufferevent_socket(ws_t ws)
{
	int ret = 0;
	assert(ws);

	LIBWS_LOG(LIBWS_DEBUG, "Create bufferevent socket");

	#ifdef LIBWS_WITH_OPENSSL
	if (ws->use_ssl)
	{
		if (!(ws->bev = _ws_create_bufferevent_openssl_socket(ws))) 
		{
			LIBWS_LOG(LIBWS_ERR, "Failed to create SSL socket");
			ret = -1;
			goto fail;
		}
	}
	else
	#endif // LIBWS_WITH_OPENSSL
	{
		if (!(ws->bev = bufferevent_socket_new(ws->ws_base->ev_base, -1, 
										BEV_OPT_CLOSE_ON_FREE)))
		{
			LIBWS_LOG(LIBWS_ERR, "Failed to create socket");
			ret = -1;
			goto fail;
		}
	}

	bufferevent_setcb(ws->bev, _ws_read_callback, _ws_write_callback, 
					_ws_event_callback, (void *)ws);

	return ret;
fail:
	if (ws->bev)
	{
		bufferevent_free(ws->bev);
		ws->bev = NULL;
	}

	return ret;
}

static void _ws_builtin_no_copy_cleanup_wrapper(const void *data, 
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

	if (!ws->bev)
	{
		LIBWS_LOG(LIBWS_ERR, "Null bufferevent on send");
		return -1;
	}

	// If in no copy mode we only add a reference to the passed
	// buffer to the underlying bufferevent, and let it use the
	// user supplied cleanup function when it has sent the data.
	// (Note that the header will never be sent like this).
	if (no_copy && ws->no_copy_cleanup_cb)
	{
		if (evbuffer_add_reference(bufferevent_get_output(ws->bev), 
			(void *)msg, len, _ws_builtin_no_copy_cleanup_wrapper, (void *)ws))
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

int _ws_send_frame_raw(ws_t ws, ws_opcode_t opcode, char *data, uint64_t datalen)
{
	uint8_t header_buf[WS_HDR_MAX_SIZE];
	size_t header_len = 0;

	assert(ws);

	if (ws->send_state != WS_SEND_STATE_NONE)
	{
		LIBWS_LOG(LIBWS_ERR, "Send state not none");
		return -1;
	}

	// All control frames MUST have a payload length of 125 bytes or less
	// and MUST NOT be fragmented.
	if (WS_OPCODE_IS_CONTROL(opcode) && (datalen > 125))
	{
		LIBWS_LOG(LIBWS_ERR, "Control frame payload cannot be "
							 "larger than 125 bytes");
		return -1;
	}

	// Pack and send header.
	{
		memset(&ws->header, 0, sizeof(ws_header_t));

		ws->header.fin = 0x1;
		ws->header.opcode = opcode;
		
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

		ws_pack_header(&ws->header, header_buf, sizeof(header_buf), &header_len);
		
		if (_ws_send_data(ws, (char *)header_buf, (uint64_t)header_len, 0))
		{
			LIBWS_LOG(LIBWS_ERR, "Failed to send frame header");
			return -1;
		}
	}

	// Send the data.
	{
		ws_mask_payload(ws->header.mask, data, datalen);

		if (_ws_send_data(ws, data, datalen, 1))
		{
			LIBWS_LOG(LIBWS_ERR, "Failed to send frame data");
			return -1;
		}
	}

	return 0;
}

void _ws_shutdown(ws_t ws)
{
	assert(ws);

	LIBWS_LOG(LIBWS_TRACE, "Websocket shutdown");

	if (ws->connect_timeout_event)
	{
		event_free(ws->connect_timeout_event);
		ws->connect_timeout_event = NULL;
	}

	#ifdef LIBWS_WITH_OPENSSL
	_ws_openssl_close(ws);
	#endif

	if (ws->bev)
	{
		bufferevent_free(ws->bev);
		ws->bev = NULL;
		LIBWS_LOG(LIBWS_DEBUG, "Freed bufferevent");
	}

	// TODO: Only quit when the base has no more connections.
	ws_base_quit(ws->ws_base, 1);

	LIBWS_LOG(LIBWS_TRACE, "End");
}

void _ws_close_timeout_cb(evutil_socket_t fd, short what, void *arg)
{
	ws_t ws = (ws_t)arg;
	assert(ws);

	LIBWS_LOG(LIBWS_TRACE, "Close timeout");

	// This callback should only ever be called after sending a close frame.
	assert(ws->sent_close);

	// We sent a close frame to the server but it hasn't initiated
	// the TCP close.
	if (ws->received_close)
	{
		LIBWS_LOG(LIBWS_ERR, "Timeout! Server sent a Websocket close frame "
							 "but did not close the TCP session");
	}
	else
	{
		LIBWS_LOG(LIBWS_ERR, "Timeout! Server did not reply to Websocket "
							 "close frame");
	}

	LIBWS_LOG(LIBWS_ERR, "Initiating an unclean close");

	_ws_shutdown(ws);
}

int _ws_send_close(ws_t ws, ws_close_status_t status_code, 
					const char *reason, size_t reason_len)
{
	char close_payload[WS_CONTROL_MAX_PAYLOAD_LEN];
	assert(ws);

	if (WS_IS_CLOSE_STATUS_NOT_USED(status_code))
	{
		LIBWS_LOG(LIBWS_ERR, "Invalid websocket close status code. "
							 "Must be between 1000 and 4999. %u given", 
							 (uint16_t)status_code);
		return -1;
	}

	// (Status code is a uint16_t == 2 bytes)
	if ((reason_len + 2) >= WS_CONTROL_MAX_PAYLOAD_LEN)
	{
		LIBWS_LOG(LIBWS_ERR, "Close reason too big to fit max control "
							 "frame payload size %u + 2 byte status (max %d)", 
							 reason_len, WS_CONTROL_MAX_PAYLOAD_LEN);
		return -1;
	}

	*((uint16_t *)close_payload) = htons((uint16_t)status_code);
	memcpy(&close_payload[2], reason, reason_len);

	if (_ws_send_frame_raw(ws, WS_OPCODE_CLOSE_0X8, 
							close_payload, reason_len + 2))
	{
		LIBWS_LOG(LIBWS_ERR, "Failed to send close frame");
		return -1;
	}

	return 0;
}

int _ws_get_random_mask(ws_t ws, char *buf, size_t len)
{
	int i;

	#ifdef WIN32
	// http://msdn.microsoft.com/en-us/library/sxtz2fa8(VS.80).aspx
	for (i = 0; i < len)
	{
		if (rand_s((unsigned int)&buf[i]))
		{
			return -1;
		}
	}
	#else
	i = read(ws->ws_base->random_fd, buf, len);
	#endif 

	return i;
}

void _ws_set_timeouts(ws_t ws)
{
	assert(ws);
	assert(ws->bev);

	// TODO: Maybe a workaround for this problem?:
	// Setting a timeout to NULL is supposed to remove it; 
	// however before Libevent 2.1.2-alpha this wouldn’t work 
	// with all event types. (As a workaround for older versions, 
	// you can try setting the timeout to a multi-day interval 
	// and/or having your eventcb function ignore BEV_TIMEOUT 
	// events when you don’t want them.)

	bufferevent_set_timeouts(ws->bev, &ws->recv_timeout, &ws->send_timeout);
}

void _ws_destroy_event(struct event **event)
{
	assert(event);

	if (*event)
	{
		event_free(*event);
		*event = NULL;
	}
}
