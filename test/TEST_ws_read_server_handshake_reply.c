#include "libws_test_helpers.h"
#include "libws.h"
#include "libws_private.h"
#include "libws_handshake.h"
#include "libws_log.h"
#include <event2/buffer.h>

static int show_result_status(const char *msg,
						ws_parse_state_t state, ws_parse_state_t expected)
{
	if (state != expected)
	{
		libws_test_FAILURE("%s: Incorrect state \"%s\", expected \"%s\"", msg, 
					ws_parse_state_to_string(state), 
					ws_parse_state_to_string(expected));
		return -1;
	}
	else
	{
		libws_test_SUCCESS("%s: State \"%s\" as expected", msg, 
							ws_parse_state_to_string(state));
		return 0;
	}
}
static int run_header_test(const char *msg, ws_t ws, struct evbuffer *in, 
							ws_parse_state_t expected)
{
	ws_parse_state_t state;

	// Print the HTTP reponse we're testing.
	if (libws_test_verbose())
	{
		libws_test_STATUS("%s", evbuffer_pullup(in, evbuffer_get_length(in)));
	}

	state = _ws_read_server_handshake_reply(ws, in);
	return show_result_status(msg, state, expected);
}

int TEST_ws_read_server_handshake_reply(int argc, char *argv[])
{
	int ret = 0;
	ws_base_t base = NULL;
	ws_t ws = NULL;
	char key_hash[256];
	struct evbuffer *in = evbuffer_new();
	ws_parse_state_t state;

	libws_test_HEADLINE("TEST_ws_read_server_handshake_reply");
	if (libws_test_init(argc, argv)) return -1;

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

	libws_test_STATUS("Test valid full response:");
	{
		// Emulate having sent a request.
		ws->connect_state = WS_CONNECT_STATE_SENT_REQ;
		evbuffer_add_printf(in, 
							"HTTP/1.1 101\r\n"
							"Upgrade: websocket\r\n"
							"Connection: upgrade\r\n"
							"Sec-WebSocket-Accept: %s\r\n"
							"Sec-WebSocket-Protocol: echo\r\n"
							"\r\n", key_hash);
	
		ret |= run_header_test("  Valid full response", ws, in, 
							WS_PARSE_STATE_SUCCESS);
	}

	libws_test_STATUS("Test valid partial response:");
	{
		ws->connect_state = WS_CONNECT_STATE_SENT_REQ;

		// Part 1.
		evbuffer_add_printf(in, 
							"HTTP/1.1 101\r\n"
							"Upgrade: websocket\r\n"
							"Connection: upg");

		libws_test_STATUS("	 Parse part 1:");
		ret |= run_header_test("  Valid partial response, part 1", ws, in, 
							WS_PARSE_STATE_NEED_MORE);
		// Part 2.
		evbuffer_add_printf(in,
							"rade\r\n"
							"Sec-WebSocket-Accept: %s\r\n"
							"Sec-WebSocket-Protocol: echo\r\n"
							"\r\n", key_hash);

		libws_test_STATUS("	 Parse part 2:");
		ret |= run_header_test("  Valid partial response, part 2", ws, in, 
							WS_PARSE_STATE_SUCCESS);
	}

	libws_test_STATUS("Test invalid HTTP version:");
	{
		// Emulate having sent a request.
		ws->connect_state = WS_CONNECT_STATE_SENT_REQ;
		evbuffer_add_printf(in, 
							"HTTP/1.0 101\r\n"
							"Upgrade: websocket\r\n"
							"Connection: upgrade\r\n"
							"Sec-WebSocket-Accept: %s\r\n"
							"Sec-WebSocket-Protocol: echo\r\n"
							"\r\n", key_hash);
		
		ret |= run_header_test("Invalid HTTP version response", ws, in, 
						WS_PARSE_STATE_ERROR);
	}

fail:
	evbuffer_free(in);
	ws_destroy(&ws);
	ws_global_destroy(&base);

	return ret;
}

