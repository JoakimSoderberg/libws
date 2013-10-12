
#ifndef __LIBWS_H_OPENSSL__
#define __LIBWS_H_OPENSSL__

void _ws_global_openssl_destroy(struct ws_base_s *ws_base);

int _ws_global_openssl_init(struct ws_base_s *ws_base);

int _ws_openssl_init(struct ws_s *ws, struct ws_base_s *ws_base);

void _ws_openssl_destroy(struct ws_s *ws);

int _ws_openssl_close(struct ws_s *ws);

struct bufferevent *_ws_create_bufferevent_openssl_socket(struct ws_s *ws);

#endif // __LIBWS_H_OPENSSL__
