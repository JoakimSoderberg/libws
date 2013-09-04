
#ifndef __LIBWS_H__
#define __LIBWS_H__

#include "libws_config.h"
#include "libws_types.h"
#include "libws_protocol.h"

#include <stdio.h>
#include <event2/event.h>

///
/// Initializes the global context for the library that's common
/// for all connections.
///
/// @param[out]	base 	A pointer to a #ws_base_t to use as global context.
///
/// @returns 			0 on success.
///
int ws_global_init(ws_base_t *base);

///
/// Destroys the global context of the library.
///
/// @param[in]	base 	The global context to destroy.
///
void ws_global_destroy(ws_base_t base);

///
/// Initializes a new Websocket connection context and ties
/// it to a global context.
///
/// @param[out]	ws 		Websocket context.
/// @param[in]	base 	The global websocket context.
///
/// @returns			0 on success.
///
int ws_init(ws_t *ws, ws_base_t base);

///
/// Destroys a Websocket connecton context.
///
/// @param[in]	ws 	The context.
///
void ws_destroy(ws_t *ws);

///
/// Connects to a Websocket on a specified server.
///
/// @param[in]	ws 		Websocket context.
/// @param[in]	server	Websocket server hostname.
/// @param[in]	port	Websocket server port.
/// @param[in]	uri 	The websocket uri.
///
/// @returns			0 on success.
///
int ws_connect(ws_t ws, const char *server, int port, const char *uri);

///
/// Closes the websocket connection.
///
/// @param ws 	The websocket context to close.
///
/// @returns 0 on success. -1 on failure.
///
int ws_close(ws_t ws);

///
/// Services the Websocket connection.
///
/// @param[in] ws 	The websocket context to be serviced.
///
/// @returns 0 on success. -1 on failure.
///
int ws_service(ws_t ws);

///
/// Services the Websocket connection blocking until
/// the connection is quit.
///
/// @param[in] ws 	The websocket session context.
///
/// @returns		0 on success, -1 otherwise.
///
int ws_service_until_quit(ws_t ws);

///
/// Quit/disconnect from a websocket connection.
///
/// @param[in] ws 	The websocket session context.
/// @param[in] let_running_events_complete
///					Should we let pending events run, or quit
///					right away?
///
/// @returns		0 on success.
///
int ws_quit(ws_t ws, int let_running_events_complete);

///
/// Send a websocket message.
///
/// @param[in]	ws 		The websocket session context.
/// @param[in]	msg 	The message payload.
/// @param[in]	len 	The message length in octets.
///
/// @returns			0 on success.
///
int ws_send_msg(ws_t ws, const char *msg, size_t len);

//
// Based on this API:
// http://autobahn.ws/python/tutorials/streaming
//

///
/// Begin sending a websocket message of the given type.
///
/// @param[in]	ws 		The websocket session context.
/// @param[in]	type 	The type of frame to send.
///
/// @returns			0 on success.
///
int ws_msg_begin(ws_t ws, ws_frame_type_t type);

///
/// Starts sending a websocket frame.
///
/// @param[in]	ws 			The websocket session context.
/// @param[in]	frame_data 	The data for the frame.
/// @param[in]	datalen		The size of the data to send.
///
/// @returns				0 on success.
///
int ws_msg_frame_send(ws_t ws, const char *frame_data, size_t datalen);

///
/// Ends the sending of a websocket message.
///
/// @param[in]	ws 		The websocket session context.
///
/// @returns			0 on success.
///
int ws_msg_end(ws_t ws);

///
/// Starts sending the data for a websocket frame.
/// Note that there is no ws_msg_frame_data_end, since the length
/// is given at the beginning of the frame.
///
/// @param[in]	ws 			The websocket session context.
/// @param[in]	datalen		The total size of the frame data.
///
/// @returns				0 on success.
///
int ws_msg_frame_data_begin(ws_t ws, size_t datalen);

///
/// Sends frame data. Note that you're not allowed to send more
/// data than was specified in the #ws_msg_frame_data_begin call.
/// You can send in chunks if you wish though.
///
/// @param[in]	ws 		The websocket session context.
/// @param[in]	data 	The frame payload data to send.
/// @param[in]	datalen	The length of the frame payload.
///
/// @returns			0 on success.
///
int ws_msg_frame_data_send(ws_t ws, const char *data, size_t datalen);

int ws_send_ping(ws_t ws);

int ws_set_max_frame_size(ws_t ws, size_t max_frame_size);

// TODO: Add onmsg_begin, onmsg_frame, onmsg_end

int ws_set_onmsg_cb(ws_t ws, ws_msg_callback_f func, void *arg);

int ws_set_onerr_cb(ws_t ws, ws_msg_callback_f func, void *arg);

int ws_set_onclose_cb(ws_t ws, ws_close_callback_f func, void *arg);

int ws_set_origin(ws_t ws, const char *origin);

int ws_set_onpong_cb(ws_t ws, ws_msg_callback_f, void *arg);

int ws_set_binary(ws_t ws, int binary);

int ws_add_header(ws_t ws, const char *header, const char *value);

int ws_remove_header(ws_t ws, const char *header);

int ws_is_connected(ws_t ws);

int ws_set_recv_timeout_cb(ws_t ws, ws_timeout_callback_f func, struct timeval recv_timeout, void *arg);

int ws_set_send_timeout_cb(ws_t ws, ws_timeout_callback_f func, struct timeval send_timeout, void *arg);

int ws_set_connect_timeout_cb(ws_t ws, ws_timeout_callback_f func, struct timeval connect_timeout, void *arg);

void ws_set_user_state(ws_t ws, void *user_state);

void *ws_get_user_state(ws_t ws);

int ws_get_uri(ws_t ws, char *buf, size_t bufsize);

///
/// Gets the websocket state.
///
/// @param[in]	ws 	The websocket context.
///
ws_state_t ws_get_state(ws_t ws);

#ifdef LIBWS_WITH_OPENSSL

int ws_set_ssl_state(ws_t ws, libws_ssl_state_t ssl);

#endif // LIBWS_WITH_OPENSSL

#endif // __LIBWS_H__
