#ifndef __LIBWS_PROTOCOL_H__
#define __LIBWS_PROTOCOL_H__

#include <inttypes.h>

// 
// Websocket protocol RFC 6455: http://tools.ietf.org/html/rfc6455
//
//      0                   1                   2                   3
//      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//     +-+-+-+-+-------+-+-------------+-------------------------------+
//     |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
//     |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
//     |N|V|V|V|       |S|             |   (if payload len==126/127)   |
//     | |1|2|3|       |K|             |                               |
//     +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
//     |     Extended payload length continued, if payload len == 127  |
//     + - - - - - - - - - - - - - - - +-------------------------------+
//     |                               |Masking-key, if MASK set to 1  |
//     +-------------------------------+-------------------------------+
//     | Masking-key (continued)       |          Payload Data         |
//     +-------------------------------- - - - - - - - - - - - - - - - +
//     :                     Payload Data continued ...                :
//     + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
//     |                     Payload Data continued ...                |
//     +---------------------------------------------------------------+
//

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

#define WS_MAX_PAYLOAD_LEN 0x7FFFFFFFFFFFFFFF

#define WS_HDR_BASE_SIZE 2
#define WS_HDR_PAYLOAD_LEN_SIZE 8
#define WS_HDR_MASK_SIZE 4
#define WS_HDR_MIN_SIZE WS_HDR_BASE_SIZE
#define WS_HDR_MAX_SIZE (WS_HDR_BASE_SIZE + WS_HDR_PAYLOAD_LEN_SIZE + WS_HDR_MASK_SIZE)

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
