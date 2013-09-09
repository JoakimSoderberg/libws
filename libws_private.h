
#ifndef __LIBWS_PRIVATE_H__
#define __LIBWS_PRIVATE_H__

///
/// @internal
/// @file libws_private.h
///
/// @author Joakim Söderberg <joakim.soderberg@gmail.com>
///
///

#include "libws_config.h"
#include "libws_log.h"
#include "libws_types.h"
#include "libws_protocol.h"

#include <event2/event.h>
#include <event2/bufferevent.h>

#ifdef LIBWS_WITH_OPENSSL
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <event2/bufferevent_ssl.h>
#endif // LIBWS_WITH_OPENSSL

typedef enum ws_send_state_e
{
	WS_SEND_STATE_NONE,
	WS_SEND_STATE_MESSAGE_BEGIN,
	WS_SEND_STATE_IN_MESSAGE,
	WS_SEND_STATE_IN_MESSAGE_PAYLOAD
} ws_send_state_t;

///
/// Global context for the library.
///
typedef struct ws_base_s
{
	#ifndef WIN32
	int random_fd;
	#endif

	#ifdef LIBWS_WITH_OPENSSL

	SSL_CTX *ssl_ctx;

	#endif // LIBWS_WITH_OPENSSL
} ws_base_s;

///
/// Context for a websocket connection.
///
typedef struct ws_s
{
	ws_state_t				state;				///< Websocket state.

	// TODO: Move the bases into the ws_base_t instead?
	struct event_base		*base;				///< Libevent event base.
	struct evdns_base 		*dns_base;			///< Libevent DNS base.
	struct bufferevent 		*bev;				///< Buffer event socket.

	ws_msg_callback_f		msg_cb;				///< Callback for when a message is received on the websocket.
	void 					*msg_arg;			///< The user supplied argument to pass to the ws_s#msg_cb callback.

	ws_err_callback_f 		err_cb;				///< Callback for when an error occurs on the websocket connection.
	void					*err_arg;			///< The user supplied argument to pass to the ws_s#error_cb callback.

	ws_close_callback_f		close_cb;			///< Callback for when the websocket connection is closed.
	void					*close_arg;			///< The user supplied argument to pass to the ws_s#close_cb callback.

	ws_connect_callback_f	connect_cb;
	void					*connect_arg;

	ws_timeout_callback_f	connect_timeout_cb;
	struct timeval			connect_timeout;
	void					*connect_timeout_arg;
	struct event 			*connect_timeout_event;

	ws_timeout_callback_f	recv_timeout_cb;
	struct timeval			recv_timeout;
	void					*recv_timeout_arg;

	ws_timeout_callback_f	send_timeout_cb;
	struct timeval			send_timeout;
	void					*send_timeout_arg;

	ws_msg_callback_f		pong_cb;
	void					*pong_arg;

	ws_timeout_callback_f	pong_timeout_cb;
	void					*pong_timeout_arg;
	struct timeval			pong_timeout;
	struct event 			*pong_timeout_event;

	ws_msg_callback_f		ping_cb;
	void					*ping_arg;

	void					*user_state;

	char 					*server;
	char					*uri;
	int						port;

	char 					*origin;

	int						binary_mode;

	int						debug_level;
	struct ws_base_s		*ws_base;

	uint64_t				max_frame_size;		///< The max frame size to allow before chunking.
	ws_header_t				header;				///< Header that's being sent for the current p
	uint64_t				frame_size;			///< The frame size of the frame currently being sent.
	uint64_t				frame_data_sent;	///< The number of bytes sent so far of the current frame.
	ws_send_state_t			send_state;			///< The state for sending data.
	ws_no_copy_cleanup_f	no_copy_cleanup_cb;	///< If set, any data written to the websocket will be freed using this callback.
	void 					*no_copy_extra;

	#ifdef LIBWS_WITH_OPENSSL

	int 					use_ssl;
	SSL 					*ssl;

	#endif // LIBWS_WITH_OPENSSL
} ws_s;


///
/// Creates a timeout event for when connecting.
///
/// @param[in]	ws 	The websocket context.
///
/// @returns		0 on success.
///
int _ws_setup_connection_timeout(ws_t ws);

///
/// Creates a timeout event for an expected pong reply.
///
/// @param[in]	ws 	The websocket context.
///
/// @returns		0 on success.
///
int _ws_setup_pong_timeout(ws_t ws);

/// 
/// Creates the libevent bufferevent socket.
///
/// @param[in]	ws 	The websocket context.
///
/// @returns		0 on success.
///
int _ws_create_bufferevent_socket(ws_t ws);

///
/// Sends data over the bufferevent socket.
///
/// @param[in]	ws 		The websocket context.
/// @param[in]	msg 	The buffer to send.
/// @param[in]	len 	Length of the buffer to send.
/// @param[in]	no_copy Should we copy the contents of the buffer
///						or simply use a reference? Used for zero copy.
///						This requires that the buffer is still valid
///						until the bufferevent sends it. To enable this
///						you have to set a cleanup function using
///						#ws_set_no_copy_cb that frees the data.
///
/// @returns			0 on success.
/// 
int _ws_send_data(ws_t ws, char *msg, uint64_t len, int no_copy);

///
/// Sends a raw websocket frame.
///
/// @param[in]	ws 		The websocket context.
/// @param[in]	opcode	The websocket operation code.
/// @param[in]	data 	Application data for the websocket frame.
/// @param[in]	datalen Length of the data.
///
/// @returns			0 on success.
///
int _ws_send_frame_raw(ws_t ws, ws_opcode_t opcode, char *data, uint64_t datalen);

///
/// Randomizes the contents of #buf. This is used for generating
/// the 32-bit payload mask.
///
/// @param[in]	ws 		The websocket context.
/// @param[in]	buf 	The buffer to randomize.
/// @param[in]	len 	Size of #buf.
///
/// @returns			The number of bytes successfully randomized.
///						Negative on error.
///
int _ws_get_random_mask(ws_t ws, char *buf, size_t len);

///
/// Masks the data in a given payload message.
///
/// @param[in]	mask 	The payload mask.
/// @param[in]	msg 	The payload message to mask.
/// @param[in]	len 	Length of the payload #msg.
///
/// @returns			0 on success.
///
int _ws_mask_payload(uint32_t mask, char *msg, uint64_t len);

///
/// Sets timeouts for read and write events of the underlying bufferevent.
///
/// @param[in]	ws 		The websocket context.
/// 
void _ws_set_timeouts(ws_t ws);



#endif // __LIBWS_PRIVATE_H__
