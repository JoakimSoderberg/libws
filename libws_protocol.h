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

LIBWS_INLINE void ws_pack_header_first_byte(ws_header_t *h, uint8_t *b)
{
	assert(h);
	assert(b);

	// TODO: Pack the header in network byte order.
	//  7 6 5 4 3 2 1 0
	// +-+-+-+-+-------+
	// |F|R|R|R| opcode|
	// |I|S|S|S|  (4)  |
	// |N|V|V|V|       |
	// | |1|2|3|       |
	// +-+-+-+-+-------+
	b[0] = 0;
	b[0] |= ((!!h->fin)  << 7);
	b[0] |= ((!!h->rsv1) << 6);
	b[0] |= ((!!h->rsv2) << 5);
	b[0] |= ((!!h->rsv3) << 4);
	b[0] |= (h->opcode  & 0xF);
}

LIBWS_INLINE void ws_pack_header_rest(ws_header_t *h, uint8_t *b, size_t len, size_t *header_len)
{
	assert(h);
	assert(b);
	assert(len >= WS_MAX_HEADER_SIZE);
	assert(h->payload_len <= WS_MAX_PAYLOAD_LEN);
	assert(header_len);

	// +----------------+---------------+-------------------------------+
	// |    Byte 0      |     Byte 1    |     Byte 2+                   |
	// +----------------+---------------+-------------------------------+
	//					 7 6 5 4 3 2 1 0
    //                  +-+-------------+-------------------------------+
    //                  |M| Payload len |    Extended payload length    |
    //                  |A|     (7)     |             (16/64)           |
    //                  |S|             |   (if payload len==126/127)   |
    //                  |K|             |                               |
    //  +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
    //  |     Extended payload length continued, if payload len == 127  |
    //  + - - - - - - - - - - - - - - - +-------------------------------+
    //  |                               |Masking-key, if MASK set to 1  |
    //  +-------------------------------+-------------------------------+
    //  | Masking-key (continued)       |
    //  +--------------------------------

	b[1] = 0;

	// Masking bit. 
	// (This MUST be set for a client according to RFC).
	b[1] |= ((!!h->mask_bit) << 7); 

	*header_len = 2;

	if (h->payload_len < 126)
	{
		// Use 1 byte for payload len.
		b[1] |= h->payload_len;
	}
	else if (h->payload_len == 126)
	{
		// 2 byte extra payload length.
		*header_len += 2;

		b[1] = 126;
		*((uint16_t *)&b[2]) = htons((uint16_t)h->payload_len);
	}
	else
	{
		// 8 byte extra payload length.
		*header_len += 8;

		b[1] = 127;

		//b[2] = ...
		// TODO: Pack uint64_t in network byte order. Use htonll (not portable)? 
	}

	if (h->mask_bit)
	{
		*((uint32_t *)&buf[header_len]) = htonl(h->mask);
		*header_len += 4;
	}
}

LIBWS_INLINE void ws_pack_header(ws_header_t *h, uint8_t *b, size_t len, size_t *header_len)
{
	ws_pack_header_first_byte(h, b);
	ws_pack_header_rest(h, b, len, header_len);
}


#endif // __LIBWS_PROTOCOL_H__
