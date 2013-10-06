
#ifndef __LIBWS_H__
#define __LIBWS_H__

#include "libws_config.h"
#include "libws_types.h"
#include "libws_header.h"
#include "libws_compat.h"

#include <stdio.h>
#include <event2/event.h>
#include <stdint.h>
#include <inttypes.h>

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
void ws_global_destroy(ws_base_t *base);

///
/// Replaces the libraries memory handling functions.
/// If any of the values are set to NULL, the default will be used.
///
/// @note These functions must allocate memory with the same alignment
///		  as the C library. 
///		  realloc(NULL, size) is the same as malloc(size).
///		  realloc(ptr, 0) is the same as free(ptr).
///		  They must be threadsafe if you're using them from several threads.
///
/// @param[in]	malloc_replace 	The malloc replacement function.
/// @param[in]	free_replace 	The free replacement function.
/// @param[in]	realloc_replace The realloc replamcent function.
///
void ws_set_memory_functions(ws_malloc_replacement_f malloc_replace,
							 ws_free_replacement_f free_replace,
							 ws_realloc_replacement_f realloc_replace);

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
/// @param[in]	ws 	The websocket context.
///
void ws_destroy(ws_t *ws);

///
/// Gets the websocket base context that the given websocket 
/// session belongs to.
///
/// @param[in]	ws 	The websocket context.
/// 
/// @returns A websocket base context.
///
ws_base_t ws_get_base(ws_t ws);

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
/// Closes the websocket connection with the "1000 normal closure" status.
///
/// @see ws_close_with_status, ws_close_with_status_reason
///
/// @param ws 	The websocket context to close.
///
/// @returns 0 on success. -1 on failure.
///
int ws_close(ws_t ws);

///
/// Closes the websocket connection, with a specified status code.
///
/// @see ws_close, ws_close_with_status_reason
///
/// @param[in] ws 		Websocket context.
/// @param[in] status 	Status code.
///
/// @returns 0 on success. -1 on failure.
///
int ws_close_with_status(ws_t ws, ws_close_status_t status);

int ws_close_with_status_reason(ws_t ws, ws_close_status_t status, 
				const char *reason, size_t reason_len);

///
/// Service the websocket base context. This needs to be done
/// periodically to keep the websocket connections running.
///
/// @param[in] base The websocket base context.
///
/// @returns 0 on success.
///
int ws_base_service(ws_base_t base);

///
/// Service the websocket base context blocking the current thread
/// until #ws_base_quit is called from one of the callbacks.
///
/// @param[in] base The websocket base context.
///
/// @returns 0 on success.
///
int ws_base_service_blocking(ws_base_t base);

///
/// Quits the given websocket event loop.
///
/// @param[in] base 						The websocket base context.
/// @param[in] let_running_events_complete	Let any pending events run.
///
/// @returns 0 on success.
///
int ws_base_quit(ws_base_t base, int let_running_events_complete);

#if 0
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
#endif

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
///
/// @returns			0 on success.
///
int ws_msg_begin(ws_t ws);

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
int ws_send_ping_ex(ws_t ws, char *msg, size_t len);

///
/// Sends a pong reply with a payload to the websocket.
///
/// @note A pong should only be sent as reply to a ping
///		  in the callback set using #ws_set_onping_cb.
///		  It is also a requirement in the Websocket RFC
///		  that the payload is the same as the one received in
///		  the ping message.
///
/// @see ws_set_onpong_cb, ws_set_onping_cb, ws_onpong_default_cb
///
/// @param[in]	ws 		The websocket session context.
/// @param[in]	msg 	Pong payload (MUST be same as ping payload).
/// @param[in]	len 	Length of pong payload.
///
/// @returns 			0 on success.
/// 
int ws_send_pong(ws_t ws, char *msg, size_t len);

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

///
/// Get the header for the current websocket frame being read.
///
/// @note This only makes sense to use in the frame callback functions.
///
/// @param[in]	ws 	The websocket session context.
///
/// @returns The header for the current websocket frame being read.
///
ws_header_t *ws_get_header(ws_t ws);

///
/// Sets the on connect callback function.
///
/// @param[in]	ws 		The websocket session context.
/// @param[in]	func 	The callback function.
/// @param[in]	arg		User context passed to the callback.
///
/// @returns			0 on success.
/// 
void ws_set_onconnect_cb(ws_t ws, ws_connect_callback_f func, void *arg);


///
/// Sets the message callback function. Once a full websocket message
/// has been received and assembeled this function will be called.
/// The message itself could have been split in several frames.
///
/// @ingroup MessageAPI Message based API
///
/// @param[in]	ws 		The websocket session context.
/// @param[in]	func 	The callback function.
/// @param[in]	arg		User context passed to the callback.
///
/// @returns			0 on success.
/// 
void ws_set_onmsg_cb(ws_t ws, ws_msg_callback_f func, void *arg);

/// @defgroup FrameAPI Frame based API
/// @{

///
/// Default callback for a message begin event. 
///
/// @note This should only be called from within a #ws_msg_begin_callback_f
///		  set using #ws_set_onmsg_begin_cb.
///
/// @see ws_msg_begin_callback_f
///
void ws_default_msg_begin_cb(ws_t ws, void *arg);

///
/// Default callback for a message frame event. 
///
/// @note This should only be called from within a #ws_msg_frame_callback_f
///		  set using #ws_set_onmsg_frame_cb.
///
/// @see ws_msg_frame_callback_f
///
void ws_default_msg_frame_cb(ws_t ws, const char *payload, 
							uint64_t len, void *arg);

///
/// Default callback for a message frame event. 
///
/// @note This should only be called from within a #ws_msg_end_callback_f
///		  set using #ws_set_onmsg_end_cb.
///
/// @see ws_msg_end_callback_f
///
void ws_default_msg_end_cb(ws_t ws, void *arg);

///
/// Sets the message begin callback function. When the first frame of a new
/// message is received, this function is called.
///
/// @see ws_get_header, ws_set_onmsg_frame_cb, ws_set_onmsg_end_cb
///
/// @param[in]	ws 		The websocket session context.
/// @param[in]	func 	The callback function.
/// @param[in]	arg		User context passed to the callback.
///
/// @returns			0 on success.
/// 
void ws_set_onmsg_begin_cb(ws_t ws, ws_msg_begin_callback_f func, void *arg);

///
/// Sets the message frame callback function. When a new frame in a message
/// has been received, this function will be called.
///
/// @see ws_get_header, ws_set_onmsg_begin_cb, ws_set_onmsg_end_cb,
///		 ws_set_onmsg_frame_begin_cb, ws_set_onmsg_frame_data_cb, 
///		 ws_set_onmsg_frame_end_cb
///	
///
/// @param[in]	ws 		The websocket session context.
/// @param[in]	func 	The callback function.
/// @param[in]	arg		User context passed to the callback.
///
/// @returns			0 on success.
/// 
void ws_set_onmsg_frame_cb(ws_t ws, ws_msg_frame_callback_f func, void *arg);

///
/// Sets the message end callback function. When a message has received its
/// final frame, this function will be called.
///
/// @see ws_get_header, ws_set_onmsg_begin_cb, ws_set_onmsg_frame_cb
///
/// @param[in]	ws 		The websocket session context.
/// @param[in]	func 	The callback function.
/// @param[in]	arg		User context passed to the callback.
///
/// @returns			0 on success.
/// 
void ws_set_onmsg_end_cb(ws_t ws, ws_msg_end_callback_f func, void *arg);

// TODO: Add a note about how overriding these functions will result in message not being assembled unless the default handlers are called from within the overriden versions.
/// @defgroup StreamAPI Stream based API
/// @{

///
/// Default callback for a message frame begin event. 
///
/// @note This should only be called from within a #ws_msg_frame_begin_callback_f
///		  set using #ws_set_onmsg_frame_begin_cb.
///
/// @see ws_msg_frame_begin_callback_f
///
void ws_default_msg_frame_begin_cb(ws_t ws, void *arg);

///
/// Default callback for a message frame data event. 
///
/// @note This should only be called from within a #ws_msg_frame_data_callback_f
///		  set using #ws_set_onmsg_frame_data_cb.
///
/// @see ws_msg_frame_data_callback_f
///
void ws_default_msg_frame_data_cb(ws_t ws, const char *payload, 
								uint64_t len, void *arg);

///
/// Default callback for a message frame end event. 
///
/// @note This should only be called from within a #ws_msg_frame_end_callback_f
///		  set using #ws_set_onmsg_frame_end_cb.
///
/// @see ws_msg_frame_end_callback_f
///
void ws_default_msg_frame_end_cb(ws_t ws, void *arg);

///
/// Sets the message frame begin callback function. When the header of a new
/// frame inside a message is received this function is called.
///
/// @see ws_get_header, ws_set_onmsg_frame_cb,
///		 ws_set_onmsg_frame_data_cb, ws_set_onmsg_frame_end_cb
///
/// @param[in]	ws 		The websocket session context.
/// @param[in]	func 	The callback function.
/// @param[in]	arg		User context passed to the callback.
///
/// @returns			0 on success.
/// 
void ws_set_onmsg_frame_begin_cb(ws_t ws, ws_msg_frame_begin_callback_f func, 
								void *arg);

///
/// Sets the message frame data callback function. When data within a frame
/// is received this function is called.
///
/// @see ws_get_header, ws_set_onmsg_frame_cb,
///		 ws_set_onmsg_frame_data_cb, ws_set_onmsg_frame_end_cb
///
/// @param[in]	ws 		The websocket session context.
/// @param[in]	func 	The callback function.
/// @param[in]	arg		User context passed to the callback.
///
/// @returns			0 on success.
/// 
void ws_set_onmsg_frame_data_cb(ws_t ws, ws_msg_frame_data_callback_f func, 
								void *arg);

///
/// Sets the message frame end callback function. When all the data in a
/// frame has been received, this function will be called.
///
/// @see ws_get_header, ws_set_onmsg_frame_cb,
///		 ws_set_onmsg_frame_data_cb, ws_set_onmsg_frame_begin_cb
///
/// @param[in]	ws 		The websocket session context.
/// @param[in]	func 	The callback function.
/// @param[in]	arg		User context passed to the callback.
///
/// @returns			0 on success.
/// 
void ws_set_onmsg_frame_end_cb(ws_t ws, ws_msg_frame_end_callback_f func, 
								void *arg);

/// @}
/// @}

///
/// Sets the on error callback function.
///
/// @param[in]	ws 		The websocket session context.
/// @param[in]	func 	The callback function.
/// @param[in]	arg		User context passed to the callback.
///
/// @returns			0 on success.
/// 
void ws_set_onerr_cb(ws_t ws, ws_err_callback_f func, void *arg);

///
/// Sets the on close callback function.
///
/// @param[in]	ws 		The websocket session context.
/// @param[in]	func 	The callback function.
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
void ws_onping_default_cb(ws_t ws, const char *msg, uint64_t len, 
							int binary, void *arg);

///
/// Sets the on ping callback function for when a ping websocket
/// frame is received. If this is set, it is up to the user callback
/// to reply with a pong, or call the #ws_onping_default_cb handler.
///
/// @note A Pong frame sent in response to a Ping frame must have 
///		  identical "Application data" as found in the message body
///		  of the Ping frame being replied to.
///
/// @see ws_send_ping, ws_send_ping_ex, ws_onping_default_cb
///
/// @param[in]	ws 		The websocket session context.
/// @param[in]	arg		User context passed to the callback.
///
void ws_set_onping_cb(ws_t ws, ws_msg_callback_f func, void *arg);

///
/// Sets the on pong callback function for when a ping websocket
/// frame is received. 
///
/// @param[in]	ws 		The websocket session context.
/// @param[in]	arg		User context passed to the callback.
/// 
void ws_set_onpong_cb(ws_t ws, ws_msg_callback_f func, void *arg);

///
/// Sets the callback that is triggered when an expected pong
/// wasn't received in the given time. If this isn't set nothing
/// is done by default.
///
/// @param[in]	ws 		The websocket session context.
/// @param[in]	timeout	The time to wait for a pong reply.
/// @param[in]	arg		User context passed to the callback.
/// 
void ws_set_pong_timeout_cb(ws_t ws, ws_timeout_callback_f func, 
							struct timeval timeout, void *arg);

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
void ws_set_recv_timeout_cb(ws_t ws, ws_timeout_callback_f func, 
							struct timeval recv_timeout, void *arg);

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
void ws_set_send_timeout_cb(ws_t ws, ws_timeout_callback_f func, 
							struct timeval send_timeout, void *arg);

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
void ws_set_connect_timeout_cb(ws_t ws, ws_timeout_callback_f func, 
								struct timeval connect_timeout, void *arg);

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
/// @returns 				NULL on failure, #buf otherwise.
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

///
/// Masks a given payload.
///
/// @param[in]	mask 	The mask to use.
/// @param[in]	msg 	The message to mask.
/// @param[in]	len 	Length of the message buffer.
///
void ws_mask_payload(uint32_t mask, char *msg, uint64_t len);

///
/// Unmasks a given payload.
///
/// @param[in]	mask 	The mask to use.
/// @param[in]	msg 	The message to mask.
/// @param[in]	len 	Length of the message buffer.
///
void ws_unmask_payload(uint32_t mask, char *msg, uint64_t len);

///
/// Add a subprotocol that we can speak over the Websocket.
///
/// @param[in]	ws 	The websocket context.
/// @param[in]	subprotocol 	The name of the subprotocol to add.
///
/// @returns 					0 on success.
///
int ws_add_subprotocol(ws_t ws, const char *subprotocol);

///
/// Returns the number of subprotocols
///
size_t ws_get_subprotocol_count(ws_t ws);

///
/// Returns a list of current subprotocols.
///
/// @param[in]	ws 		The websocket context.
/// @param[out] count 	The number of subprotocols.
///
/// @returns 			A list of strings containing the subprotocols.
///						NULL if the list is empty.
///						It is up to the caller to free this list.
///
char **ws_get_subprotocols(ws_t ws, size_t *count);

///
/// Can be used to free a list of sub protocols.
///
/// @param[in]	subprotocols 	A list returned from #ws_get_subprotocols.
/// @param[in]	count 			The number of items in the list.
///
void ws_free_subprotocols_list(char **subprotocols, size_t count);

///
/// Clear the list of subprotocols.
///
/// @param[in]	ws 	The websocket context.
///
/// @returns 					0 on success.
///
int ws_clear_subprotocols(ws_t ws);

#ifdef LIBWS_WITH_OPENSSL

///
/// Sets what type of SSL connections to allow. 
/// If self signed certificates should be allowed.
///
/// @see libws_ssl_state_t
/// 
/// @param[in]	ws 			The websocket session context.
///
void ws_set_ssl_state(ws_t ws, libws_ssl_state_t ssl);

#endif // LIBWS_WITH_OPENSSL

///
/// Convert a parse state enum value into a readable string.
///
/// @param[in]	state 	The state.
///
/// @returns A string representation of the state enum value.
///
const char *ws_parse_state_to_string(ws_parse_state_t state);

#endif // __LIBWS_H__


