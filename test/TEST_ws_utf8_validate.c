#include "libws_test_helpers.h"
#include "libws_utf8.h"
#include <stdio.h>
#include <string.h>

static void print_utf8_str(const unsigned char *s, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++)
	{
		printf("%x", s[i]);
	}

	printf("\n");
}

static const char *utf8_state_str(ws_utf8_parse_state_t s)
{
	switch (s)
	{
		case WS_UTF8_FAILURE: return "fail";
		case WS_UTF8_SUCCESS: return "success";
		case WS_UTF8_NEED_MORE: return "need more";
	}

	return NULL;
}

static const char *utf8_overlong[] =
{
	"\xc0\xaf",
	"\xe0\x80\xaf",
	"\xf0\x80\x80\xaf",
	"\xf8\x80\x80\x80\xaf",
	"\xfc\x80\x80\x80\x80\xaf"
};
#define UTF8_OVERLONG_COUNT (sizeof(utf8_overlong) / sizeof(char *))

static int test_utf8_overlong()
{
	int ret = 0;
	int i;
	ws_utf8_parse_state_t s;

	libws_test_STATUS("Test overlong strings:");

	for (i = 0; i < UTF8_OVERLONG_COUNT; i++)
	{
		print_utf8_str((unsigned char *)utf8_overlong[i], 
			strlen(utf8_overlong[i]));
		
		s = ws_utf8_validate((unsigned char *)utf8_overlong[i], 
				strlen(utf8_overlong[i]));

		if (s == WS_UTF8_FAILURE)
		{
			libws_test_SUCCESS("Got expected %s", utf8_state_str(s));
		}
		else
		{
			ret |= 1;
			libws_test_FAILURE("Got unexpected %s", utf8_state_str(s));
		}
	}

	return ret;
}

int TEST_ws_utf8_validate(int argc, char **argv)
{
	int ret = 0;

	libws_test_HEADLINE("TEST_ws_utf8_validate");

	ret |= test_utf8_overlong();

	

	return 0;
}

