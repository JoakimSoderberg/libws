#include "libws_test_helpers.h"
#include "libws.h"
#include "libws_private.h"
#include "libws_handshake.h"
#include "libws_log.h"
#include <string.h>

int TEST_ws_calculate_key_hash(int argc, char *argv[])
{
	int ret = 0;
	
	char key_hash[1024];

	const char sec_websocket_key[] = "dGhlIHNhbXBsZSBub25jZQ==";
	const char expected_key_hash[] = "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";

	libws_test_HEADLINE("TEST_ws_calculate_key_hash");
	if (libws_test_init(argc, argv)) return -1;

	if (_ws_calculate_key_hash(sec_websocket_key, key_hash, sizeof(key_hash)))
	{
		libws_test_FAILURE("Failed to call _ws_calculate_key_hash");
		ret = -1;
	}

	if (strcmp(key_hash, expected_key_hash))
	{
		libws_test_FAILURE("Expected %s but got %s", 
						expected_key_hash, key_hash);
	}
	else
	{
		libws_test_SUCCESS("Got expected key hash: %s", key_hash);
	}

	return ret;
}
