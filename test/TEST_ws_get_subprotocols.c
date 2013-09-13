#include "libws.h"
#include "libws_test_helpers.h"

void add_to_list(ws_t ws, char *subprotocol, size_t index, size_t expected_count)
{
	char **prots = NULL;
	size_t count = 0;
	size_t was = 0;

	libws_test_STATUS("Adding subprotocol \"%s\"", subprotocol);

	if (ws_add_subprotocol(ws, subprotocol))
	{
		libws_test_FAILURE("Could not add subprotocol \"%s\"", subprotocol);
	}
	else
	{
		libws_test_SUCCESS("Added \"%s\" subprotocol", subprotocol);
	}

	if ((was = ws_get_subprotocol_count(ws) != expected_count))
	{
		libws_test_FAILURE("Subprotocol count expected to be %u but was %u", 
			expected_count, was);
	}
	else
	{
		libws_test_SUCCESS("Subprotocol count %u as expected", expected_count);
	}

	libws_test_STATUS("Get subprotocols, expecting %u in list", expected_count);
	prots = NULL;
	prots = ws_get_subprotocols(ws, &count);

	if (!prots)
	{
		libws_test_FAILURE("NULL subprotocol list returned with count %u", count);
	}
	else
	{
		libws_test_SUCCESS("Subprotocol list returned with count %u", count);
	}

	libws_test_STATUS("Validate list contents:");

	if (strcmp(prots[index], subprotocol))
	{
		libws_test_FAILURE("\"%s\" expected, got \"%s\"", 
			subprotocol, prots[index]);
	}
	else
	{
		libws_test_SUCCESS("\"%s\" returned", subprotocol);
	}
}

int TEST_ws_get_subprotocols(int argc, char **argv)
{
	int ret = 0;
	ws_base_t base;
	ws_t ws;
	size_t i;
	char **prots = NULL;
	size_t count = 0;

	if (ws_global_init(&base))
	{
		libws_test_FAILURE("Failed to init global state");
		return -1;
	}

	if (ws_init(&ws, base))
	{
		libws_test_FAILURE("Failed to init websocket state");
		return -1;
	}

	if (ws_get_subprotocol_count(ws) != 0)
	{
		libws_test_FAILURE("Subprotocol count != 0 after init");
		ret = -1;
	}
	else
	{
		libws_test_SUCCESS("Subprotocol count == 0 after init");
	}

	libws_test_STATUS("Get subprotocols");
	prots = ws_get_subprotocols(ws, &count);

	if (prots)
	{
		libws_test_FAILURE("Subprotocol list not empty on init");
	}
	else
	{
		libws_test_SUCCESS("Empty subprotocol list as expected");
	}

	add_to_list(ws, "test1", 0, 1);
	add_to_list(ws, "test2", 1, 2);

	for (i = 0; i < count; i++)
	{
		libws_test_STATUS("\"%s\"", prots[i]);
	}

	libws_test_STATUS("Freeing subprotocol list");
	ws_free_subprotocols_list(prots, count);

	libws_test_STATUS("Clearing subprotocol list");
	ws_clear_subprotocols(ws);

	if (ws_get_subprotocol_count(ws) != 0)
	{
		libws_test_FAILURE("Subprotocol count != 0 after clear");
		ret = -1;
	}
	else
	{
		libws_test_SUCCESS("Subprotocol count == 0 after clear");
	}


	return 0;
}

