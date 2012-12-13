
#ifndef __LIBWS_H_OPENSSL__
#define __LIBWS_H_OPENSSL__

void _ws_global_openssl_destroy(ws_base_t ws_base);

void _ws_global_openssl_init(ws_base_t ws_base);

int _ws_openssl_init(ws_t ws, ws_base_t ws_base);

void _ws_openssl_destroy(ws_t ws);

int _ws_openssl_close(ws_t ws);

#endif // __LIBWS_H_OPENSSL__
