
#include "libws.h"
#include "libws_log.h"
#include "libws_handshake.c"
#include "libws_test_helpers.h"

int TEST_ws_parse_http_header(int argc, char **argv)
{
	int ret = 0;
	char *line = "Origin:    arne weise    ";
	char *invalid_line = "Blarg";
	char *header_name;
	char *header_val;
	int i;

	libws_test_HEADLINE("TEST_ws_parse_http_header");

	if (libws_test_init(argc, argv)) return -1;

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

	libws_test_STATUS("Parse invalid line:");

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

	libws_test_STATUS("Parse NULL line:");

	if (_ws_parse_http_header(NULL, &header_name, &header_val))
	{
		libws_test_SUCCESS("Failed on NULL input");
	}
	else
	{
		libws_test_FAILURE("Did not fail o NULL input");
		ret |= -1;
	}

	{
		int http_major_version;
		int http_minor_verson;
		int http_status;

		if (_ws_parse_http_status(NULL, &http_major_version,
							 &http_minor_verson, &http_status))
		{
			libws_test_SUCCESS("Failed to parse HTTP status with NULL input");
		}
		else
		{
			libws_test_FAILURE("Success parsing HTTP status with NULL input");
			ret |= -1;
		}

		if (_ws_parse_http_status("HTTP/1.1 abc", &http_major_version,
							 &http_minor_verson, &http_status))
		{
			libws_test_SUCCESS("Failed to parse HTTP status with NULL input");
		}
		else
		{
			libws_test_FAILURE("Success parsing HTTP status with NULL input");
			ret |= -1;
		}
	}

	ws_set_memory_functions(libws_test_malloc,
							 free,
							 libws_test_realloc);

	for (i = 1; i <= 2; i++)
	{
		libws_test_STATUS("Test failing malloc on allocation: %d", i);
		libws_test_set_malloc_fail_count(i);

		if (_ws_parse_http_header(line, &header_name, &header_val))
		{
			libws_test_SUCCESS("Failed as expected on NULL memory allocation "
								"after %d allocations", i);
		}
		else
		{
			libws_test_FAILURE("Did not fail on NULL memory allocation "
								"after %d allocations", i);
			ret |= -1;
		}
	}

	ws_set_memory_functions(NULL, NULL, NULL);

	return ret;
}
