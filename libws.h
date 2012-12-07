
#ifndef __LIBWS_H__
#define __LIBWS_H__

#include <stdio.h>
#include <event2/event.h>

typedef struct _ws_t *ws_t;

int ws_global_init();

int ws_global_destroy();

int ws_init(ws_t *ws);

int ws_destroy(ws_t *ws);

int ws_connect_advanced(ws_t ws, const char *server, int port, const char *uri);

int ws_connect(ws_t ws, const char *uri);

int ws_close(ws_t ws);

int ws_send_msg(ws_t ws, const char *msg, size_t len);

int ws_send_ping(ws_t ws);

typedef int (*ws_msg_callback_f)(ws_t, const char *msg, size_t len, void *arg);
typedef int (*ws_err_callback_f)(ws_t, int errcode, const char *errmsg, void *arg);
typedef int (*ws_close_callback_f)(ws_t, int reason, void *arg);

int ws_set_onmsg_cb(ws_t ws, ws_msg_callback_f func, void *arg);

int ws_set_onerr_cb(ws_t ws, ws_msg_callback_f func, void *arg);

int ws_set_onclose_cb(ws_t ws, ws_close_callback_f func, void *arg);

int ws_set_origin(ws_t ws, const char *origin);

int ws_set_onpong_cb(ws_t ws, ws_msg_callback_f, void *arg);

int ws_set_ssl(ws_t ws, int ssl);

int ws_set_binary(ws_t ws, int binary);

int ws_add_header(ws_t ws, const char *header, const char *value);

int ws_remove_header(ws_t ws, const char *header);

int ws_is_connected(ws_t ws);


#endif // __LIBWS_H__
