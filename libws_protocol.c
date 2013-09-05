
#include "libws_config.h"
#include "libws_types.h"
#include "libws_protocol.h"

int ws_read_header(const unsigned char *data, size_t len)
{
	if (len < WS_MIN_HEADER_SIZE)
	{
		return -1;
	}

	return 0;
}
