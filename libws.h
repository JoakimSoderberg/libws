
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
int ws_send_msg(ws_t ws, char *msg, uint64_t len);

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
int ws_msg_frame_send(ws_t ws, char *frame_data, uint64_t datalen);

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
int ws_msg_frame_data_begin(ws_t ws, uint64_t datalen);

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
int ws_msg_frame_data_send(ws_t ws, char *data, uint64_t datalen);

///
/// Sends a ping. To receive the pong reply use 
/// #ws_set_onpong_cb to set a callback.
///
/// @param[in]	ws 		The websocket session context.
///
/// @returns			0 on success.
///
int ws_send_ping(ws_t ws);

///
/// Sends a ping with a payload. Same as #ws_send_ping but with
/// a specified payload.
/// @param[in]	ws 		The websocket session context.
/// @param[in]	data 	The frame payload data to send.
/// @param[in]	datalen	The length of the frame payload.
///
/// @returns			0 on success.
///
int ws_send_ping_ex(ws_t ws, char *msg, uint64_t len);

///
/// Sends a pong reply to the websocket.
///
/// @note This should only be called as a reply to a ping
///		  message received in the callback set by #ws_set_onping_cb.
///
/// @see ws_send_pong_ex, ws_set_onpong_cb
///
/// @param[in]	ws 		The websocket session context.
/// @param[in]	arg		User context passed to the callback.
///
/// @returns			0 on success.
/// 
void ws_send_pong(ws_t ws);

///
/// Sends a pong reply with a payload to the websocket.
/// 
/// @see ws_send_pong, ws_set_onpong_cb
///
/// @param[in]	ws 		The websocket session context.
/// @param[in]	arg		User context passed to the callback.
///
/// @returns			0 on success.
/// 
void ws_send_pong_ex(ws_t ws, char *msg, uint64_t len);

///
/// Sets the max allowed frame size. If any frame size exceeds
/// this, it will automatically be broken up into a separate
/// frame.
///
/// @param[in]	ws 				The websocket session context.
/// @param[in]	max_frame_size 	The max frame size. 0 means no limit.
///
/// @returns					0 on success.
///
int ws_set_max_frame_size(ws_t ws, uint64_t max_frame_size);

///
/// Gets the current max frame size set. See #ws_set_max_frame_size
/// on how to set it.
///
/// @param[in]	ws 	The websocket session context.
///
/// @returns		The current max frame size.
///
uint64_t ws_get_max_frame_size(ws_t ws);

// TODO: Add onmsg_begin, onmsg_frame, onmsg_end

///
/// Sets the on connect callback function.
///
/// @param[in]	ws 		The websocket session context.
/// @param[in]	arg		User context passed to the callback.
///
/// @returns			0 on success.
/// 
void ws_set_onconnect_cb(ws_t ws, connect_callback_f func, void *arg);

///
/// Sets the on msg callback function.
///
/// @param[in]	ws 		The websocket session context.
/// @param[in]	arg		User context passed to the callback.
///
/// @returns			0 on success.
/// 
void ws_set_onmsg_cb(ws_t ws, ws_msg_callback_f func, void *arg);

///
/// Sets the on error callback function.
///
/// @param[in]	ws 		The websocket session context.
/// @param[in]	arg		User context passed to the callback.
///
/// @returns			0 on success.
/// 
void ws_set_onerr_cb(ws_t ws, ws_msg_callback_f func, void *arg);

///
/// Sets the on close callback function.
///
/// @param[in]	ws 		The websocket session context.
/// @param[in]	arg		User context passed to the callback.
///
/// @returns			0 on success.
/// 
void ws_set_onclose_cb(ws_t ws, ws_close_callback_f func, void *arg);

///
/// Sets the origin to use when doing the websocket connection handshake.
///
/// @param[in]	ws 		The websocket session context.
///
/// @returns			0 on success.
/// 
int ws_set_origin(ws_t ws, const char *origin);

///
/// The default ping callback handler. This will send a pong
/// message in reply.
///
/// @note This should only ever be called from within the
///       ping callback set using #ws_set_onping_cb.
///
/// @see ws_set_onping_cb
///
/// @param[in]	ws 		The websocket session context.
/// @param[in]	arg		User context passed to the callback.
///
void ws_onping_default_cb(ws_t ws, char *msg, uint64_t len);

///
/// Sets the on ping callback function for when a ping websocket
/// frame is received. If this is set, it is up to the user callback
/// to reply with a pong, or call the #ws_onping_default_cb handler.
///
/// @see ws_send_ping, ws_send_ping_ex, ws_onping_default_cb
///
/// @param[in]	ws 		The websocket session context.
/// @param[in]	arg		User context passed to the callback.
///
void ws_set_onping_cb(ws_t ws, ws_msg_callback_f, void *arg);

///
/// The default pong callback handler. This notes that a pong
/// was received in reply to the ping that was sent.
///
/// @param[in]	ws 		The websocket session context.
/// @param[in]	arg		User context passed to the callback.
///
void ws_onpong_default_cb(ws_t ws, char *msg, uint64_t len);

///
/// Sets the on pong callback function for when a ping websocket
/// frame is received. 
///
/// @param[in]	ws 		The websocket session context.
/// @param[in]	arg		User context passed to the callback.
/// 
void ws_set_onpong_cb(ws_t ws, ws_msg_callback_f, void *arg);

///
/// Sets binary mode for the messages that are sent 
/// to the websocket connection.
///
/// @param[in]	ws 		The websocket session context.
/// @param[in]	binary	Set to 1 if binary mode should be used.
///
void ws_set_binary(ws_t ws, int binary);

///
/// Adds a HTTP header to the initial connection handshake.
///
/// @param[in]	ws 		The websocket session context.
/// @param[in]	header 	The name of the header to add.
/// @param[in]	value	The value of the header.
///
/// @returns			0 on success.
/// 
int ws_add_header(ws_t ws, const char *header, const char *value);

///
/// Removes a HTTP header for the initial connection handshake. 
///
/// @param[in]	ws 		The websocket session context.
/// @param[in]	header	The name of the header to be removed.
///
/// @returns			0 on success.
/// 
int ws_remove_header(ws_t ws, const char *header);

///
/// Checks if the websocket is connected.
///
/// @param[in]	ws 		The websocket session context.
///
/// @returns			Returns 1 if the websocket is connected.
/// 
int ws_is_connected(ws_t ws);

///
/// Sets the callback for when a receive operation has timed out.
///
/// @param[in]	ws 				The websocket session context.
/// @param[in]	func			The callback function.
/// @param[in]	recv_timeout 	The timeout for the receive operation.
/// @param[in]	arg 			User context passed to callback.
///
/// @returns			0 on success.
/// 
void ws_set_recv_timeout_cb(ws_t ws, ws_timeout_callback_f func, struct timeval recv_timeout, void *arg);

///
/// Sets the callback for when a send operation has timed out.
///
/// @param[in]	ws 				The websocket session context.
/// @param[in]	func			The callback function.
/// @param[in]	send_timeout 	The timeout for the send operation.
/// @param[in]	arg 			User context passed to callback.
///
/// @returns			0 on success.
/// 
void ws_set_send_timeout_cb(ws_t ws, ws_timeout_callback_f func, struct timeval send_timeout, void *arg);

///
/// Sets the callback for when a connection attempt times out.
///
/// @param[in]	ws 				The websocket session context.
/// @param[in]	func			The callback function.
/// @param[in]	connect_timeout The timeout for the receive operation.
/// @param[in]	arg 			User context passed to callback.
///
/// @returns			0 on success.
/// 
void ws_set_connect_timeout_cb(ws_t ws, ws_timeout_callback_f func, struct timeval connect_timeout, void *arg);

///
/// Sets the user context/state for this websocket connection.
///
/// @see ws_get_user_state
///
/// @param[in]	ws 				The websocket session context.
/// @param[in]	user_state 		The user context/state.
/// 
void ws_set_user_state(ws_t ws, void *user_state);

///
/// Gets the user context/state for this websocket connection.
///
/// @see ws_set_user_state
///
/// @param[in]	ws 				The websocket session context.
/// 
void *ws_get_user_state(ws_t ws);

///
/// Returns the URI of the current websocket connection.
///
/// @see ws_get_user_state
///
/// @param[in]	ws 			The websocket session context.
/// @param[out]	buf 		A buffer to put the result in.
/// @param[in]	bufsize		The size of the buffer.
///
/// @returns 				NULL on failure.
/// 
char *ws_get_uri(ws_t ws, char *buf, size_t bufsize);

///
/// Sets the cleanup callback function to use when freeing
/// data that is passed to the send msg function in no copy mode.
///
/// @param[in]	ws 			The websocket session context.
/// @param[in]	func 		Callback function.
/// @param[in]	extra 		Argument that is passed to the callback function.
///
void ws_set_no_copy_cb(ws_t ws, ws_no_copy_cleanup_f func, void *extra);

///
/// Gets the websocket state.
///
/// @param[in]	ws 	The websocket context.
///
ws_state_t ws_get_state(ws_t ws);

#ifdef LIBWS_WITH_OPENSSL

///
/// Sets what type of SSL connections to allow.
///
/// @see libws_ssl_state_t
/// 
/// @param[in]	ws 			The websocket session context.
///
int ws_set_ssl_state(ws_t ws, libws_ssl_state_t ssl);

#endif // LIBWS_WITH_OPENSSL

#endif // __LIBWS_H__


