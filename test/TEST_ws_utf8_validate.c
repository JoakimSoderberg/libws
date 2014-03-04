#include "libws_test_helpers.h"
#include "libws_utf8.h"
#include <stdio.h>
#include <string.h>

static void print_utf8_str(const char *str, size_t len)
{
	size_t i;
	unsigned char *s = (unsigned char *)str;

	for (i = 0; i < len; i++)
	{
		printf("%x", s[i]);
	}

	printf("\n");
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
	ws_utf8_state_t s;

	libws_test_STATUS("Test overlong strings:");

	for (i = 0; i < UTF8_OVERLONG_COUNT; i++)
	{
		print_utf8_str(utf8_overlong[i], strlen(utf8_overlong[i]));

		s = WS_UTF8_ACCEPT;
		ws_utf8_validate(&s, utf8_overlong[i], strlen(utf8_overlong[i]));

		if (s == WS_UTF8_REJECT)
		{
			libws_test_SUCCESS("Got expected ACCEPT");
		}
		else
		{
			ret |= -1;
			libws_test_FAILURE("Got unexpected REJECT");
		}
	}

	return ret;
}

static int test_utf8_valid()
{
	int ret = 0;
	size_t j;
	ws_utf8_state_t s = WS_UTF8_ACCEPT;
	const char *valids[] =
	{
		"\x48\x65\x6c\x6c\x6f\x2d\xc2\xb5\x40\xc3\x9f\xc3\xb6"
		"\xc3\xa4\xc3\xbc\xc3\xa0\xc3\xa1\x2d\x55\x54\x46\x2d\x38\x21\x21",

		"\xce\xba\xe1\xbd\xb9\xcf\x83\xce\xbc\xce\xb5"
	};

	ws_utf8_state_t expvalid0[] =
	{
		WS_UTF8_ACCEPT, 	// 0
		WS_UTF8_ACCEPT, 	// 1
		WS_UTF8_ACCEPT, 	// 2
		WS_UTF8_ACCEPT, 	// 3
		WS_UTF8_ACCEPT, 	// 4
		WS_UTF8_ACCEPT, 	// 5
		WS_UTF8_ACCEPT, 	// 6
		WS_UTF8_NEEDMORE,	// 7
		WS_UTF8_ACCEPT,		// 8
		WS_UTF8_ACCEPT,		// 9
		WS_UTF8_NEEDMORE,	// 10
		WS_UTF8_ACCEPT,		// 11
		WS_UTF8_NEEDMORE,	// 12
		WS_UTF8_ACCEPT,		// 13
		WS_UTF8_NEEDMORE,	// 14
		WS_UTF8_ACCEPT,		// 15
		WS_UTF8_NEEDMORE,	// 16
		WS_UTF8_ACCEPT,		// 17
		WS_UTF8_NEEDMORE,	// 18
		WS_UTF8_ACCEPT,		// 19
		WS_UTF8_NEEDMORE,	// 20
		WS_UTF8_ACCEPT,		// 21
		WS_UTF8_ACCEPT,		// 22
		WS_UTF8_ACCEPT,		// 23
		WS_UTF8_ACCEPT,		// 24
		WS_UTF8_ACCEPT,		// 25
		WS_UTF8_ACCEPT,		// 26
		WS_UTF8_ACCEPT,		// 27
		WS_UTF8_ACCEPT,		// 28
		WS_UTF8_ACCEPT		// 29
	};

	ws_utf8_state_t expvalid1[] =
	{
		WS_UTF8_ACCEPT,		// 0
		WS_UTF8_NEEDMORE,	// 1
		WS_UTF8_ACCEPT,		// 2
		WS_UTF8_NEEDMORE+1,	// 3
		WS_UTF8_NEEDMORE,	// 4
		WS_UTF8_ACCEPT,		// 5
		WS_UTF8_NEEDMORE,	// 6
		WS_UTF8_ACCEPT,		// 7
		WS_UTF8_NEEDMORE,	// 8
		WS_UTF8_ACCEPT,		// 9
		WS_UTF8_NEEDMORE,	// 10
		WS_UTF8_ACCEPT		// 11
	};

	ws_utf8_state_t *expected[] =
	{
		expvalid0,
		expvalid1
	};

	for (j = 0; j < sizeof(valids) / sizeof(char *); j++)
	{
		libws_test_STATUS("Testing full valid string:");
		{
			print_utf8_str(valids[j], strlen(valids[j]));

			s = WS_UTF8_ACCEPT;
			ws_utf8_validate(&s, valids[j], strlen(valids[j]));

			if (s == WS_UTF8_ACCEPT)
			{
				libws_test_SUCCESS("Full valid utf8 string parsed");
			}
			else
			{
				libws_test_FAILURE("Full valid utf8 string was "
									"incorrectly failed");
				ret |= -1;
			}
		}

		libws_test_STATUS("Testing full valid string, 1 octet at a time:");
		{
			size_t len = strlen(valids[j]);
			size_t i;

			for (i = 0; i <= len; i++)
			{
				print_utf8_str(valids[j], i);

				s = WS_UTF8_ACCEPT;
				ws_utf8_validate(&s, valids[j], i);

				if (s == expected[j][i])
				{
					libws_test_SUCCESS("[%i] Got expected \"%u\"", i, s);
				}
				else
				{
					libws_test_FAILURE("[%i] Expected \"%u\" but got \"%u\"", i,
									expected[j][i], s);
				}
			}
		}
	}
	return ret;
}

int TEST_ws_utf8_validate(int argc, char **argv)
{
	int ret = 0;

	libws_test_HEADLINE("TEST_ws_utf8_validate");

	ret |= test_utf8_overlong();
	ret |= test_utf8_valid();

	return ret;
}

