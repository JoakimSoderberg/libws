
#include <libws.h>
#include <libws_log.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "jansson.h"
#include "cargo/cargo.h"

typedef enum libws_autobahn_state_e
{
	LIBWS_AUTOBAHN_STATE_TEST = 0,
	LIBWS_AUTOBAHN_STATE_REPORT = 1,
	LIBWS_AUTOBAHN_STATE_TESTINFO = 2,
	LIBWS_AUTOBAHN_STATE_TESTSTATUS = 3,
	LIBWS_AUTOBAHN_STATE_COUNT = 4,
} libws_autobahn_state_t;

typedef struct libws_autobahn_args_s
{
	int ssl;
	int port;
	int testcase;
	int cases[2];
	size_t case_count;
	char *agentname;
	char *server;
	int reports;
	int help;
	int debug;

	char **extras;
	size_t extra_count;
} libws_autobahn_args_t;

libws_autobahn_state_t state;
libws_autobahn_args_t args;
int case_count = 0;

void onmsg(ws_t ws, char *msg, uint64_t len, int binary, void *arg)
{
	switch (state)
	{
		case LIBWS_AUTOBAHN_STATE_REPORT:
		{
			printf("Got report! %s\n", msg);
			break;
		}
		case LIBWS_AUTOBAHN_STATE_TESTINFO:
		{
			printf("Test info: %s\n", msg);
			break;
		}
		case LIBWS_AUTOBAHN_STATE_TESTSTATUS:
		{
			printf("Test status: %s\n", msg);
			break;
		}
		case LIBWS_AUTOBAHN_STATE_COUNT: 
		{
			case_count = atoi(msg);
			break;
		}
		default:
		case LIBWS_AUTOBAHN_STATE_TEST:
		{
			// Echo any message.
			if (!binary)
				printf("%s\n", msg ? msg : "NULL");
			else
				printf("%llu length binary message received\n", len);

			ws_send_msg_ex(ws, msg, len, binary);
			break;
		}
	}

	ws_close(ws);
}

void onping(ws_t ws, char *payload, uint64_t len, int binary, void *arg)
{
	ws_onping_default_cb(ws, payload, len, binary, arg);
	printf("Ping! (%llu byte payload)\n", len);
}

void onclose(ws_t ws, ws_close_status_t status,
			const char *reason, size_t reason_len, void *arg)
{
	if (state == LIBWS_AUTOBAHN_STATE_TEST)
		printf("Close status: %u. %s\n", (uint16_t)status, reason);
	ws_base_quit(ws_get_base(ws), 1);
}

void onconnect(ws_t ws, void *arg)
{
	if (state == LIBWS_AUTOBAHN_STATE_TEST)
		printf("Connected!\n");
}

int do_connect(char *url)
{
	int ret = 0;
	ws_base_t base = NULL;
	ws_t ws = NULL;

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
	ws_set_onping_cb(ws, onping, NULL);

	if (args.ssl)
	{
		ws_set_ssl_state(ws, LIBWS_SSL_SELFSIGNED);
	}

	if (ws_connect(ws, args.server, args.port, url))
	{
		ret = -1;
		goto fail;
	}

	ws_base_service_blocking(base);

fail:
	ws_destroy(&ws);
	ws_global_destroy(&base);

	return ret;
}

int update_reports()
{
	char url[1024];
	int ret = 0;

	snprintf(url, sizeof(url), 
			"updateReports?agent=%s", args.agentname);
	state = LIBWS_AUTOBAHN_STATE_REPORT;

	if (do_connect(url))
	{
		ret = -1;
	}

	return ret;
}

int run_case(int testcase)
{
	char url[1024];
	int ret = 0;

	// Get test info.
	snprintf(url, sizeof(url), "getCaseInfo?case=%d&agent=%s",
			testcase, args.agentname);
	state = LIBWS_AUTOBAHN_STATE_TESTINFO;

	if (do_connect(url))
	{
		ret = -1;
		goto fail;
	}

	// Run test.
	snprintf(url, sizeof(url), "runCase?case=%d&agent=%s",
			testcase, args.agentname);
	state = LIBWS_AUTOBAHN_STATE_TEST;

	if (do_connect(url))
	{
		ret = -1;
		goto fail;
	}

	// Print status.
	snprintf(url, sizeof(url), "getCaseStatus?case=%d&agent=%s",
			testcase, args.agentname);
	state = LIBWS_AUTOBAHN_STATE_TESTSTATUS;

	if (do_connect(url))
	{
		ret = -1;
		goto fail;
	}

fail:
	return ret;
}

int get_case_count()
{
	char url[1024];
	int ret = 0;

	snprintf(url, sizeof(url), "getCaseCount");
	state = LIBWS_AUTOBAHN_STATE_COUNT;

	if (do_connect(url))
	{
		ret = -1;
	}
	else
	{
		ret = case_count;
	}

	return ret;
}

int run_cases(int start, int stop)
{
	int i;
	int max_case = 0;
	if ((max_case = get_case_count()) < 0)
	{
		fprintf(stderr, "Failed to get case count!\n");
		return -1;
	}

	printf("-----------------------------------\n");
	printf("Running test case %d to %d (of %d)\n", 
			args.cases[0], args.cases[1], max_case);
	printf("-----------------------------------\n");


	if (stop > max_case) stop = max_case;

	for (i = start; i < stop; i++)
	{
		if (run_case(i))
		{
			return -1;
		}
	}

	return 0;
}

int main(int argc, char **argv)
{
	int ret = 0;
	int i;
	cargo_t cargo;

	printf("\nlibws AutobahnTestSuite client (C) Joakim Soderberg 2014\n\n");
	printf("  jansson v%s\n\n", JANSSON_VERSION);

	// Parse command line arguments.
	{
		args.ssl = 0;
		args.port = 9001;
		args.testcase = 1;
		args.agentname = "libws";
		args.server = "localhost";
		args.reports = 0;
		args.help = 0;
		args.debug = 0;

		cargo_init(&cargo, 32, argv[0],
			"An AutobahnTestSuite client that can be "
			"used to verify websocket compliance.");

		ret |= cargo_add(cargo, "--help", &args.help, CARGO_BOOL,
					"Show this help.");
		cargo_add_alias(cargo, "--help", "-h");

		ret |= cargo_add(cargo, "--debug", &args.debug, CARGO_BOOL,
					"Show websocket debug output.");
		cargo_add_alias(cargo, "--debug", "-d");

		ret |= cargo_add(cargo, "--ssl", &args.ssl, CARGO_BOOL,
					"Use SSL for the websocket connection.");
		cargo_add_alias(cargo, "--ssl", "-s");

		ret |= cargo_add(cargo, "--port", &args.port, CARGO_INT,
					"The websocket port to use.");
		cargo_add_alias(cargo, "--port", "-p");

		ret |= cargo_addv(cargo, "--test", (void **)&args.cases, 
				&args.case_count, 2, CARGO_INT,
				"A test case to run, or a range by specifying start and stop.");
		cargo_add_alias(cargo, "--test", "-t");

		ret |= cargo_addv(cargo, "--agent", (void **)&args.agentname, NULL , 1,
					CARGO_STRING,
					"The name of the user agent. Default is 'libws'.");
		cargo_add_alias(cargo, "--agent", "-a");

		ret |= cargo_add(cargo, "--reports", &args.reports, CARGO_BOOL,
				"Tell the server to update the Autobahn Test Suites "
				"reports manually.");
		cargo_add_alias(cargo, "--reports", "-r");

		if (ret != 0)
		{
			fprintf(stderr, "Failed to add argument\n");
			return -1;
		}

		if (cargo_parse(cargo, 1, argc, argv))
		{
			fprintf(stderr, "Error parsing!\n");
			ret = -1;
			goto done;
		}

		args.extras = cargo_get_args(cargo, &args.extra_count);
	}

	if (args.extra_count >= 1)
	{
		args.server = args.extras[0];
	}

	if (args.help)
	{
		cargo_print_usage(cargo);
		return -1;
	}

	if (args.debug)
	{
		ws_set_log_cb(ws_default_log_cb);
		ws_set_log_level(-1);
	}

	printf("-----------------------------------\n");
	printf("Agent: %s\n", args.agentname);
	printf("SSL: %s\n", args.ssl ? "ON" : "OFF");
	printf("Server: %s:%d\n", args.server, args.port);
	printf("Test cases: %d\n", args.testcase);
	printf("-----------------------------------\n\n");

	if (args.reports)
	{
		ret = update_reports();
	}
	else
	{
		if (args.case_count == 0)
		{
			fprintf(stderr, "You must specify at least one test case!\n");
		}
		else if (args.case_count == 1)
		{
			ret = run_cases(args.cases[0], args.cases[0] + 1);
		}
		else
		{
			ret = run_cases(args.cases[0], args.cases[1]+1);
		}
	}

done:
	cargo_destroy(&cargo);
	printf("Bye bye!\n");
	return ret;
}

