
#ifndef __LIBWS_UTF8__
#define __LIBWS_UTF8__ 

#include <stdlib.h>
#include <inttypes.h>

typedef enum ws_utf8_parse_state_e
{
	WS_UTF8_FAILURE = -1,
	WS_UTF8_SUCCESS = 0,
	WS_UTF8_NEED_MORE = 1,
	WS_UTF8_NEED_MORE2 = 2,
	WS_UTF8_NEED_MORE3 = 3,
	WS_UTF8_NEED_MORE4 = 4
} ws_utf8_parse_state_t;

ws_utf8_parse_state_t ws_utf8_validate(const unsigned char *value, 
										size_t len, size_t *offset);

#define WS_UTF8_ACCEPT 0
#define WS_UTF8_REJECT 1
typedef uint32_t ws_utf8_state_t;

ws_utf8_state_t ws_utf8_decode(ws_utf8_state_t *state, uint32_t *codep, uint32_t byte);

ws_utf8_state_t ws_utf8_validate2(ws_utf8_state_t *state, char *str, size_t len);


#endif // __LIBWS_UTF8__
