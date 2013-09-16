
#include "libws.h"
#include "libws_log.h"
#include "libws_handshake.c"
#include "libws_test_helpers.h"

int TEST_ws_parse_http_header(int argc, char **argv)
{
	int ret = 0;
	char *line = "Origin: arne weise";
	char *invalid_line = "Blarg";
	char *header_name;
	char *header_val;

	libws_test_STATUS("Parse header \"%s\"", line);

	if (_ws_parse_http_header(line, &header_name, &header_val))
	{
		libws_test_FAILURE("Failed to parse header \"%s\"", line);
		ret = -1;
	}
	else
	{
		if (!header_name || !header_val)
		{
			if (!header_name)
			{
				libws_test_FAILURE("Header name NULL after parse");
				ret = -1;
			}

			if (!header_name)
			{
				libws_test_FAILURE("Header val NULL after parse");
				ret = -1;
			}
		}
		else
		{
			libws_test_SUCCESS("Parsed header \"%s\". "
						"Name = \"%s\", Value = \"%s\"", 
						line, header_name, header_val);
		}
	}

	free(header_name);
	free(header_val);

	if (_ws_parse_http_header(invalid_line, &header_name, &header_val))
	{
		if (header_name || header_val)
		{
			if (header_name)
			{
				libws_test_FAILURE("Header name not NULL after parse failure");
				ret = -1;
			}

			if (header_name)
			{
				libws_test_FAILURE("Header value not NULL after parse failure");
				ret = -1;
			}
		}
		else
		{
			libws_test_SUCCESS("Header name and value NULL after failed parse");
		}
	}

	return ret;
}
