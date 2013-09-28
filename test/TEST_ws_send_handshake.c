#include "libws_test_helpers.h"
#include "libws.h"
#include "libws_log.h"
#include "libws_handshake.h"
#include "libws_private.h"
#include <event2/event.h>
#include <event2/buffer.h>

static int do_test(ws_t ws, struct evbuffer *out, int success_expected)
{
	int ret = 0;
	evbuffer_drain(out, evbuffer_get_length(out));

	if (_ws_send_handshake(ws, out))
	{
		if (success_expected)
		{
			libws_test_FAILURE("Failed to send handshake");
		}
		else
		{
			libws_test_SUCCESS("Failed to send handshake as expected");
		}
		return -1;
	}
	else
	{
		if (success_expected)
		{
			libws_test_SUCCESS("Sent handshake");
		}
		else
		{
			libws_test_FAILURE("Failed to send handshake");
		}
	}

	return 0;
}

int TEST_ws_send_handshake(int argc, char *argv[])
{
	int ret = 0;
	struct evbuffer *out = NULL;
	ws_base_t base = NULL;
	ws_t ws = NULL;

	libws_test_HEADLINE("TEST_ws_send_handshake");

	if (libws_test_init(argc, argv)) return -1;

	if (ws_global_init(&base))
	{
		libws_test_FAILURE("Failed to init global state");
		return -1;
	}

	if (!(out = evbuffer_new()))
	{
		libws_test_FAILURE("Failed to allocate evbuffer");
		ret |= -1;
		goto fail;
	}

	libws_test_STATUS("Test with all settings set");
	{
		if (ws_init(&ws, base))
		{
			libws_test_FAILURE("Failed to init websocket state");
			ret |= -1;
			goto fail;
		}

		ws->server = strdup("example.com");
		ws->port = 123;
		ws->uri = strdup("some_uri/path");
		ws->origin = strdup("example.com");

		ws_add_subprotocol(ws, "echo");
		ws_add_subprotocol(ws, "tut");

		ret |= do_test(ws, out, 1);

		ws_destroy(&ws);
	}

	libws_test_STATUS("Test with no subprotocols or origin");
	{
		if (ws_init(&ws, base))
		{
			libws_test_FAILURE("Failed to init websocket state");
			ret |= -1;
			goto fail;
		}

		ws->server = strdup("example.com");
		ws->port = 123;
		ws->uri = strdup("some_uri/path");

		ret |= do_test(ws, out, 1);

		ws_destroy(&ws);
	}

	libws_test_STATUS("Test nothing set");
	{
		if (ws_init(&ws, base))
		{
			libws_test_FAILURE("Failed to init websocket state");
			ret |= -1;
			goto fail;
		}

		ret |= do_test(ws, out, 0);

		ws_destroy(&ws);
	}

	libws_test_STATUS("Test with failing malloc");
	{
		if (ws_init(&ws, base))
		{
			libws_test_FAILURE("Failed to init websocket state");
			ret |= -1;
			goto fail;
		}

		ws->server = strdup("example.com");
		ws->port = 123;
		ws->uri = strdup("some_uri/path");

		ws_set_memory_functions(libws_test_malloc,
							 free,
							 libws_test_realloc);

		libws_test_set_malloc_fail_count(1);

		ret |= do_test(ws, out, 0);

		ws_destroy(&ws);
	}

fail:
	evbuffer_free(out);
	ws_global_destroy(&base);

	return ret;
}

