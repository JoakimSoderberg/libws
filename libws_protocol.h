#ifndef __LIBWS_PROTOCOL_H__
#define __LIBWS_PROTOCOL_H__

#include <inttypes.h>

typedef enum ws_opcode_e
{
	WS_OPCODE_CONTINUATION = 0x0,
	WS_OPCODE_TEXT = 0x1,
	WS_OPCODE_BINARY = 0x2,
	// 0x3 - 0x7 Reserved for further non-control frames.
	WS_OPCODE_CLOSE = 0x8,
	WS_OPCODE_PING = 0x9,
	WS_OPCODE_PONG = 0xA
	// 0xB - 0xF are reserved for further control frames.
} ws_opcode_t;

typedef struct ws_header_s
{
	uint8_t fin;
	uint8_t rsv1;
	uint8_t rsv2;
	uint8_t rsv3;
	ws_opcode_t opcode;
	uint8_t mask_bit;
	uint64_t payload_len;
	uint32_t mask;
} ws_header_t;

#endif // __LIBWS_PROTOCOL_H__
