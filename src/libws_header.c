
#include "libws_config.h"
#include "libws_log.h"
#include "libws_compat.h"
#include "libws_types.h"
#include "libws_header.h"
#include "libws_private.h"
#include <assert.h>
#include <string.h>

#ifndef _WIN32
#include <arpa/inet.h>
#endif

const char *opcodes[] =
{
	"continuation", 		// 0x0
	"text",					// 0x1
	"binary",				// 0x2
	"non control reserved",	// 0x3
	"non control reserved", // 0x4
	"non control reserved", // 0x5
	"non control reserved", // 0x6
	"non control reserved", // 0x7
	"close",				// 0x8
	"ping",					// 0x9
	"pong",					// 0xA
	"control reserved",		// 0xB
	"control reserved",		// 0xC
	"control reserved",		// 0xD
	"control reserved",		// 0xE
	"control reserved"		// 0xF
};

const char *ws_opcode_str(ws_opcode_t opcode)
{
	if (((int)opcode >= 0) && ((int)opcode <= 0xF))
		return opcodes[opcode];

	return NULL;
}

ws_parse_state_t ws_unpack_header(ws_header_t *h, size_t *header_len, 
									const unsigned char *b, size_t len)
{
	assert(b);
	assert(h);
	assert(header_len);

	*header_len = 0;
	memset(h, 0, sizeof(ws_header_t));	

	if (len < WS_HDR_MIN_SIZE)
	{
		goto need_more;
	}

	// First byte.
	h->fin		= (b[0] >> 7) & 0x1;
	h->rsv1		= (b[0] >> 6) & 0x1;
	h->rsv2		= (b[0] >> 5) & 0x1;
	h->rsv3		= (b[0] >> 4) & 0x1;
	h->opcode	= (b[0]       & 0xF);

	// Second byte.
	h->mask_bit = (b[1] >> 7) & 0x1;
	h->payload_len = (b[1] & 0x7F);

	*header_len = 2;

	// Get extended payload size if any (2 or 8 bytes).
	if (h->payload_len == 126)
	{
		if (len < (*header_len + 2))
		{
			goto need_more;
		}
		else
		{
			// 2 byte payload length.
			uint16_t *size_ptr = (uint16_t *)&b[2];
			h->payload_len = (uint64_t)ntohs(*size_ptr);
			*header_len += 2;
		}
	}
	else if (h->payload_len == 127)
	{
		if (len < (*header_len + 8))
		{
			goto need_more;
		}
		else
		{
			// 8 byte payload length.
			uint64_t *size_ptr = (uint64_t *)&b[2];
			h->payload_len = libws_ntoh64(*size_ptr);
			*header_len += 8;
		}
	}

	// Read masking key.
	if (h->mask_bit)
	{
		uint32_t *mask_ptr = (uint32_t *)&b[*header_len];
		// TODO: Hmm shouldn't it be ntohl here? (doesn't work with RFC examples though).
		h->mask = (*mask_ptr);
		*header_len += 4;
	}

	return WS_PARSE_STATE_SUCCESS;
need_more:
	return WS_PARSE_STATE_NEED_MORE;
}

static void _ws_pack_header_first_byte(ws_header_t *h, uint8_t *b)
{
	assert(h);
	assert(b);

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

static void _ws_pack_header_rest(ws_header_t *h, uint8_t *b, size_t len, size_t *header_len)
{
	assert(h);
	assert(b);
	assert(len >= WS_HDR_MAX_SIZE);
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

	if (h->payload_len <= 125)
	{
		// Use 1 byte for payload len.
		b[1] |= h->payload_len;
	}
	else if (h->payload_len <= 0xFFFF)
	{
		// 2 byte extra payload length.
		uint16_t *size_ptr = (uint16_t *)&b[2];
		*header_len += 2;

		b[1] |= 126;
		*size_ptr = htons((uint16_t)h->payload_len);
	}
	else
	{
		// 8 byte extra payload length.
		uint64_t *size_ptr = (uint64_t *)&b[2];
		*header_len += 8;

		b[1] |= 127;
		*size_ptr = libws_hton64(h->payload_len);
	}

	if (h->mask_bit)
	{
		uint32_t *mask_ptr = (uint32_t *)&b[*header_len];
		*mask_ptr = (h->mask);
		*header_len += 4;
	}
}

void ws_pack_header(ws_header_t *h, uint8_t *b, size_t len, size_t *header_len)
{
	_ws_pack_header_first_byte(h, b);
	_ws_pack_header_rest(h, b, len, header_len);
}


