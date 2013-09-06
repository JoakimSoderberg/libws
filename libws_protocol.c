
#include "libws_config.h"
#include "libws_types.h"
#include "libws_protocol.h"

int ws_read_header(const unsigned char *data, size_t len)
{
	assert(data);

	if (len < WS_MIN_HEADER_SIZE)
	{
		return -1;
	}

	// TODO: Unpack header

	return 0;
}

void ws_pack_header_first_byte(ws_header_t *h, uint8_t *b)
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

void ws_pack_header_rest(ws_header_t *h, uint8_t *b, size_t len, size_t *header_len)
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
}

void ws_pack_header(ws_header_t *h, uint8_t *b, size_t len, size_t *header_len)
{
	ws_pack_header_first_byte(h, b);
	ws_pack_header_rest(h, b, len, header_len);
}


