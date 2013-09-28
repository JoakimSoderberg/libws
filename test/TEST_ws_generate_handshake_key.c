#include "libws_test_helpers.h"
#include "libws.h"
#include "libws_private.h"
#include "libws_handshake.h"
#include "libws_log.h"
#ifndef WIN32
#include <unistd.h> // TODO: System introspection.
#endif
#include <errno.h>

static int do_test(ws_t ws, int success_expected)
{
	if (_ws_generate_handshake_key(ws))
	{
		if (success_expected)
		{
			libws_test_FAILURE("Failed to generate handshake key");
			return -1;
		}
		else
		{
			libws_test_SUCCESS("Failed to generate handshake key as expected");
		}
	}
	else
	{
		if (success_expected)
		{
			libws_test_SUCCESS("Generated handshake key");
		}
		else
		{
			libws_test_FAILURE("Failed to generate handshake key");
			return -1;
		}
	}

	return 0;
}

int TEST_ws_generate_handshake_key(int argc, char *argv[])
{
	int ret = 0;
	ws_base_t base = NULL;
	ws_t ws = NULL;

	libws_test_HEADLINE("TEST_ws_generate_handshake_key");
	
	if (libws_test_init(argc, argv)) return -1;

	if (ws_global_init(&base))
	{
		libws_test_FAILURE("Failed to init global state");
		return -1;
	}

	if (ws_init(&ws, base))
	{
		libws_test_FAILURE("Failed to init websocket state");
		ret = -1;
		goto fail;
	}

	libws_test_STATUS("Run the same test twice to check for memory leaks");
	ret |= do_test(ws, 1);
	ret |= do_test(ws, 1);

	#ifndef WIN32
	libws_test_STATUS("Close file descriptor to random source and check fail");
	
	if (close(base->random_fd))
	{
		libws_test_FAILURE("Failed to close random source: %s (%d)", 
							strerror(errno), errno);
		ret |= -1;
	}
	else
	{
		ret |= do_test(ws, 0);
	}
	#endif

fail:
	ws_destroy(&ws);
	ws_global_destroy(&base);

	return ret;
}
