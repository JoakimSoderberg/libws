
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

int ws_pack_header(ws_header_t *header, uint8_t *data, size_t len)
{
	assert(header);
	assert(data);
	// TODO: Pack the header in network byte order.

	return 0;
}

int ws_pack_header_payload_len(uint64_t payload_len, uint8_t *data, size_t len)
{
	assert(data);

	return 0;
}


