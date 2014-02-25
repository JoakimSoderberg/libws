
#include <libws.h>
#include <libws_log.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "jansson.h"
#include "cargo/cargo.h"
#include "libws_test_helpers.h"

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
	int cases[2];
	size_t case_count;
	char *agentname;
	char *server;
	int reports;
	int help;
	int debug;
	int nocolor;

	char **extras;
	size_t extra_count;
} libws_autobahn_args_t;

libws_autobahn_state_t state;
libws_autobahn_args_t args;
int server_case_count = 0;
int current_case = -1;
int global_return = 0;

void draw_line()
{
	int i;

	for (i = 0; i < 80; i++)
		printf("-");

	printf("\n");
}

void parse_test_info(char *msg)
{
	char headline[128];
	json_error_t error;
	json_t *json = NULL;
	char *id = NULL;
	char *description = NULL;

	if (!(json = json_loads(msg, 0, &error)))
	{
		fprintf(stderr, "Failed to load test info json: %s\n", error.text);
		return;
	}

	if (json_unpack_ex(json, &error, 0,
		"{"
			"s:s"
			"s:s"
		"}",
		"id", &id,
		"description", &description))
	{
		fprintf(stderr, "Failed to parse test info: %s\n", error.text);
		json_decref(json);
		return;
	}

	snprintf(headline, sizeof(headline), "[%d] - %s", current_case, id);

	libws_test_HEADLINE(headline);
	libws_test_STATUS("%s", description);
}

void parse_test_status(char *msg)
{
	json_error_t error;
	json_t *json = NULL;
	char *behavior = NULL;

	if (!(json = json_loads(msg, 0, &error)))
	{
		fprintf(stderr, "Failed to load test info json: %s\n", error.text);
		return;
	}

	if (json_unpack_ex(json, &error, 0,
		"{"
			"s:s"
		"}",
		"behavior", &behavior))
	{
		fprintf(stderr, "Failed to parse test info: %s\n", error.text);
		json_decref(json);
		return;
	}

	if (!strcmp(behavior, "OK"))
	{
		libws_test_SUCCESS("");
	}
	else
	{
		libws_test_FAILURE("");
		global_return = -1;
	}
}

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
			parse_test_info(msg);
			break;
		}
		case LIBWS_AUTOBAHN_STATE_TESTSTATUS:
		{
			parse_test_status(msg);
			break;
		}
		case LIBWS_AUTOBAHN_STATE_COUNT: 
		{
			server_case_count = atoi(msg);
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

	current_case = testcase;

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
	printf("\n");

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
		ret = server_case_count;
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

	draw_line();
	printf("Running test case %d to %d (of %d)\n", 
			args.cases[0], args.cases[1], max_case);
	draw_line();
	printf("\n");


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

		ret |= cargo_add(cargo, "--nocolor", &args.nocolor, CARGO_BOOL,
					"Turn off fancy color output.");

		ret |= cargo_add(cargo, "--port", &args.port, CARGO_INT,
					"The websocket port to use.");
		cargo_add_alias(cargo, "--port", "-p");

		ret |= cargo_addv(cargo, "--test", (void **)&args.cases, 
				&args.case_count, 2, CARGO_INT,
				"A test case to run (1 argument), or a range by specifying "
				"start and stop (2 arguments). Note that these are simply "
				"specified as an integer. Not the 1.1.1 name format the "
				"tests use.");

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
			cargo_print_usage(cargo);
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

	if (args.nocolor)
	{
		libws_test_nocolor(1);
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

	draw_line();
	printf("Agent: %s\n", args.agentname);
	printf("SSL: %s\n", args.ssl ? "ON" : "OFF");
	printf("Server: %s:%d\n", args.server, args.port);
	printf("Test cases: ");
	if (args.case_count == 1)
	{
		printf("%d\n", args.cases[0]);
	}
	else if (args.case_count == 2)
	{
		printf("%d to %d\n", args.cases[0], args.cases[1]);
	}
	else
	{
		printf("-\n");
	}

	draw_line();
	printf("\n");

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
			if (args.cases[0] > args.cases[1])
			{
				fprintf(stderr, "First test case must be smaller than the "
					"second\n");
				goto done;
			}

			ret = run_cases(args.cases[0], args.cases[1]+1);
		}
	}

	if (ret)
	{
		fprintf(stderr, "Failure!\n");
	}

	if (global_return)
	{
		libws_test_FAILURE("One or more tests failed!");
	}

done:
	cargo_destroy(&cargo);
	printf("Bye bye!\n");
	return ret | global_return;
}

