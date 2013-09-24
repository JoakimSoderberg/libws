#include "libws_test_helpers.h"
#include "libws.h"
#include "libws_private.h"
#include "libws_handshake.h"
#include "libws_log.h"
#include <event2/buffer.h>

int TEST_ws_read_server_handshake_reply(int argc, char *argv[])
{
	int ret = 0;
	ws_base_t base = NULL;
	ws_t ws = NULL;
	char key_hash[256];
	struct evbuffer *in = evbuffer_new();

	ws_set_log_cb(ws_default_log_cb);
	ws_set_log_level(-1);

	if (!in)
	{
		libws_test_FAILURE("Failed to init evbuffer");
		return -1;
	}

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

	if (ws_add_subprotocol(ws, "echo"))
	{
		libws_test_FAILURE("Failed to add sub protocol \"echo\"");
		ret = -1;
		goto fail;
	}

	if (_ws_generate_handshake_key(ws))
	{
		libws_test_FAILURE("Failed to generate handshake base64 key");
		ret = -1;
		goto fail;
	}
	else
	{
		libws_test_SUCCESS("Generated handshake base64 key: %s",
							ws->handshake_key_base64);
	}

	if (_ws_calculate_key_hash(ws->handshake_key_base64, 
								key_hash, sizeof(key_hash)))
	{
		libws_test_FAILURE("Failed to calculate key hash");
		ret = -1;
		goto fail;
	}
	else
	{
		libws_test_SUCCESS("Calculated key hash: %s", key_hash);
	}

	libws_test_STATUS("Adding test data to buffer:");
	evbuffer_add_printf(in, 
						"HTTP/1.1 101\r\n"
						"Upgrade: websocket\r\n"
						"Connection: upgrade\r\n"
						"Sec-WebSocket-Accept: %s\r\n"
						"Sec-WebSocket-Protocol: echo\r\n"
						"\r\n"
						"\r\n", key_hash);


	libws_test_STATUS("Call _ws_read_server_handshake_reply:");

	// Emulate having sent a request.
	ws->connect_state = WS_CONNECT_STATE_SENT_REQ;

	if (_ws_read_server_handshake_reply(ws, in))
	{
		libws_test_FAILURE("Failed to read valid server handshake reply");
	}
	else
	{
		libws_test_SUCCESS("Read valid server handshake reply");
	}

fail:
	ws_destroy(&ws);
	ws_global_destroy(&base);

	return ret;
}

