
#include "libws_test_helpers.h"
#include "libws_handshake.h"

static int do_test(ws_t ws, int valid, const char *val)
{
	if (_ws_check_server_protocol_list(ws, val))
	{
		if (valid)
		{
			libws_test_FAILURE("Failed to validate the valid list: "
								"\"%s\"", val);
			return -1;
		}
		else
		{
			libws_test_SUCCESS("Failed to validate the invalid list: "
								"\"%s\"", val);
			return 0;
		}
	}
	
	if (valid)
	{
		libws_test_SUCCESS("Validated the valid list: "
								"\"%s\"", val);
		return 0;
	}
	else
	{
		libws_test_FAILURE("Failed to invalidate the invalid list: "
								"\"%s\"", val);
		return -1;
	}
}

int TEST_ws_check_server_protocol_list(int argc, char *argv[])
{
	int ret = 0;
	ws_base_t base = NULL;
	ws_t ws = NULL;
	size_t i;
	size_t count = 0;
	char *protocols[] = {"arne", "weises", "julafton"};

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

	libws_test_STATUS("Adding subprotocols to ws instance.");
	for (i = 0; i < sizeof(protocols) / sizeof(char *); i++)
	{
		if (ws_add_subprotocol(ws, protocols[i]))
		{
			libws_test_FAILURE("Failed to add sub protocols to ws_t");
			ret = -1;
			goto fail;
		}	
	}

	libws_test_STATUS("Start tests:");
	ret |= do_test(ws, 1, "arne, weises, julafton");
	ret |= do_test(ws, 1, "weises, ARNE, julafton");
	ret |= do_test(ws, 1, "    arne   , julafton,     weiseS");
	ret |= do_test(ws, 1, "    arne   ,      julafton,     weiseS");
	ret |= do_test(ws, 1, "arne");
	ret |= do_test(ws, 1, "JULAFTON");
	ret |= do_test(ws, 1, "WEISes     ");
	ret |= do_test(ws, 1, "arne, julafton");
	ret |= do_test(ws, 0, "");
	ret |= do_test(ws, 0, "    ");
	ret |= do_test(ws, 0, ",,arne");
	ret |= do_test(ws, 0, "completely, wrong, arne");
	ret |= do_test(ws, 0, "arne, fel, julafton");

fail:
	ws_destroy(&ws);
	ws_global_destroy(&base);

	return ret;
}

