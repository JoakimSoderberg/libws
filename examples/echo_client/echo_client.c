
#include <libws.h>
#include <libws_log.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void onmsg(ws_t ws, const char *msg, uint64_t len, int binary, void *arg)
{
	printf("Message: \"%s\"\n", msg);

	ws_base_quit(ws_get_base(ws), 1);
}

void onconnect(ws_t ws, void *arg)
{
	char *msg = strdup("hello");
	printf("Connected!\n");
	ws_send_msg(ws, msg, strlen(msg));
	free(msg);
}

int main(int argc, char **argv)
{
	int ret = 0;
	ws_base_t base = NULL;
	ws_t ws = NULL;

	ws_set_log_cb(ws_default_log_cb);
	ws_set_log_level(-1);

	printf("Echo client\n\n");

	if (ws_global_init(&base))
	{
		fprintf(stderr, "Failed to init global state.\n");
		return -1;
	}

	if (ws_init(&ws, base))
	{
		fprintf(stderr, "Failed to init websocket state.\n");
		ret = -1;
		goto fail;
	}

	ws_set_onmsg_cb(ws, onmsg, NULL);
	ws_set_onconnect_cb(ws, onconnect, NULL);

	if (ws_connect(ws, "localhost", 9500, ""))
	{
		return -1;
	}

	ws_base_service_blocking(base);

fail:
	ws_destroy(&ws);
	ws_global_destroy(&base);
	return ret;
}

