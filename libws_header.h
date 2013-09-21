#ifndef __LIBWS_HEADER_H__
#define __LIBWS_HEADER_H__

#include <inttypes.h>

typedef enum ws_opcode_e
{
	WS_OPCODE_CONTINUATION = 0x0,
	WS_OPCODE_TEXT = 0x1,
	WS_OPCODE_BINARY = 0x2,

	// 0x3 - 0x7 Reserved for further non-control frames.
	WS_OPCODE_NON_CONTROL_RSV_0X3 = 0x3,
	WS_OPCODE_NON_CONTROL_RSV_0X4 = 0x4,
	WS_OPCODE_NON_CONTROL_RSV_0X5 = 0x5,
	WS_OPCODE_NON_CONTROL_RSV_0X6 = 0x6,
	WS_OPCODE_NON_CONTROL_RSV_0X7 = 0x7,

	WS_OPCODE_CLOSE = 0x8,
	WS_OPCODE_PING = 0x9,
	WS_OPCODE_PONG = 0xA,

	// 0xB - 0xF are reserved for further control frames.
	WS_OPCODE_CONTROL_RSV_0XB = 0xB,
	WS_OPCODE_CONTROL_RSV_0XC = 0xC,
	WS_OPCODE_CONTROL_RSV_0XD = 0xD,
	WS_OPCODE_CONTROL_RSV_0XE = 0xE,
	WS_OPCODE_CONTROL_RSV_0XF = 0xF,
} ws_opcode_t;

#define WS_OPCODE_IS_CONTROL(opcode) \
	((opcode >= WS_OPCODE_CLOSE) && (opcode <= WS_OPCODE_CONTROL_RSV_0XF))

#define WS_MAX_PAYLOAD_LEN 0x7FFFFFFFFFFFFFFF
#define WS_CONTROL_MAX_PAYLOAD_LEN 125

#define WS_HDR_BASE_SIZE 2
#define WS_HDR_PAYLOAD_LEN_SIZE 8
#define WS_HDR_MASK_SIZE 4
#define WS_HDR_MIN_SIZE WS_HDR_BASE_SIZE
#define WS_HDR_MAX_SIZE (WS_HDR_BASE_SIZE + WS_HDR_PAYLOAD_LEN_SIZE + WS_HDR_MASK_SIZE)

///
/// Websocket header structure.
///
/// 
/// Websocket protocol RFC 6455: http://tools.ietf.org/html/rfc6455
///
///      0                   1                   2                   3
///      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
///     +-+-+-+-+-------+-+-------------+-------------------------------+
///     |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
///     |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
///     |N|V|V|V|       |S|             |   (if payload len==126/127)   |
///     | |1|2|3|       |K|             |                               |
///     +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
///     |     Extended payload length continued, if payload len == 127  |
///     + - - - - - - - - - - - - - - - +-------------------------------+
///     |                               |Masking-key, if MASK set to 1  |
///     +-------------------------------+-------------------------------+
///     | Masking-key (continued)       |          Payload Data         |
///     +-------------------------------- - - - - - - - - - - - - - - - +
///     :                     Payload Data continued ...                :
///     + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
///     |                     Payload Data continued ...                |
///     +---------------------------------------------------------------+
///
typedef struct ws_header_s
{
	uint8_t fin;			///< Final frame bit.
	uint8_t rsv1;			///< Reserved bit 1 for extensions.
	uint8_t rsv2;			///< Reserved bit 2 for extensions. 
	uint8_t rsv3;			///< Reserved bit 3 for extensions.
	ws_opcode_t opcode;		///< Operation code.
	uint8_t mask_bit;		///< If this frame is masked this bit is set.
	uint64_t payload_len;	///< Length of the payload.
	uint32_t mask;			///< Masking key for the payload.
} ws_header_t;

int ws_unpack_header(ws_header_t *h, size_t *header_len, 
					const unsigned char *b, size_t len);

///
/// Packs a websocket header struct into a network byte order
/// octet stream.
///
/// @param[in]	h 			A pointer to the header to pack.
/// @param[in]	b 			The byte buffer to pack the header in. 
///							Must be at least #WS_HDR_MAX_SIZE big.
/// @param[in]	len 		The size of the buffer.
/// @param[out] header_len	Will contain the final size of the packed header.
///
void ws_pack_header(ws_header_t *h, uint8_t *b, size_t len, size_t *header_len);



#endif // __LIBWS_HEADER_H__
