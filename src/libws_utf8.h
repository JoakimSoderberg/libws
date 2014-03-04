
#ifndef __LIBWS_UTF8__
#define __LIBWS_UTF8__ 

#include <stdlib.h>
#include <inttypes.h>

#define WS_UTF8_ACCEPT 0
#define WS_UTF8_REJECT 1
#define WS_UTF8_NEEDMORE 2
typedef uint32_t ws_utf8_state_t;

ws_utf8_state_t ws_utf8_decode(ws_utf8_state_t *state, uint32_t *codep, uint32_t byte);

ws_utf8_state_t ws_utf8_validate(ws_utf8_state_t *state, const char *str, size_t len);


#endif // __LIBWS_UTF8__
