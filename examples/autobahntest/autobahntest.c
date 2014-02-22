
#include <libws.h>
#include <libws_log.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void onmsg(ws_t ws, char *msg, uint64_t len, int binary, void *arg)
{	
	if (!binary)
		printf("%s\n", msg ? msg : "NULL");
	else
		printf("%d length binary message received\n", len);

	ws_send_msg_ex(ws, msg, len);
	ws_close(ws);
}

void onclose(ws_t ws, ws_close_status_t status,
			const char *reason, size_t reason_len, void *arg)
{
	printf("Closing %u\n", (uint16_t)status);
	ws_base_quit(ws_get_base(ws), 1);
}

void onconnect(ws_t ws, void *arg)
{
	//char *msg = strdup("hello");
	printf("Connected!\n");
	//ws_send_msg(ws, msg);
	//free(msg);
}

int main(int argc, char **argv)
{
	int ret = 0;
	int i;
	ws_base_t base = NULL;
	ws_t ws = NULL;
	int ssl = 0;
	int port = 9500;
	int testcase = 1;
	char url[1024];
	char *server = "localhost";
	memset(url, 0, sizeof(url));

	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "--ssl"))
		{
			ssl = 1;
		}
		else if (!strcmp(argv[i], "--port") && (i + 1 < argc))
		{
			i++;
			port = atoi(argv[i]);
		}
		else if (!strcmp(argv[i], "--test") && (i + 1 < argc))
		{
			i++;
			testcase = atoi(argv[i]);
					}
		else if (!strcmp(argv[i], "--reports"))
		{
			snprintf(url, sizeof(url), "updateReports?agent=libws");
		}
		else
		{
			server = argv[i];
		}
	}

	if (strlen(url) == 0)
	{
		snprintf(url, sizeof(url), "runCase?case=%d&agent=libws", testcase);
	}

	printf("SSL: %s\n", ssl ? "ON" : "OFF");
	printf("Server: %s:%d\n", server, port);
	printf("Test case: %d\n", testcase);

	ws_set_log_cb(ws_default_log_cb);
	ws_set_log_level(-1);

	printf("AutobahnTestSuite client\n\n");

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
	ws_set_onclose_cb(ws, onclose, NULL);

	if (ssl)
	{
		ws_set_ssl_state(ws, LIBWS_SSL_SELFSIGNED);
	}

	printf("Connect to server %s\n", server);

	if (ws_connect(ws, server, port, url))
	{
		ret = -1;
		goto fail;
	}

	ws_base_service_blocking(base);

fail:
	ws_destroy(&ws);
	ws_global_destroy(&base);
	printf("Bye bye!\n");
	return ret;
}

