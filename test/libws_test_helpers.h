
#ifndef __LIBWS_TEST_HELPERS_H__
#define __LIBWS_TEST_HELPERS_H__

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

enum libws_test_color_e
{
	NORMAL,
	GREEN,
	RED,
	YELLOW,
	CYAN,
	MAGNETA,
	BRIGHT,
	CSTATUS
};

int libws_test_verbose();
int libws_test_log_on();
void libws_test_nocolor(int nocolor);
int libws_test_parse_cmdline(int argc, char **argv);
int libws_test_init(int argc, char **argv);

void libws_test_vprintf(FILE *target,
		enum libws_test_color_e test_color,
		const char *fmt, va_list args);

void libws_test_printf(FILE *target,
		enum libws_test_color_e test_color,
		const char *fmt, ...);

void libws_test_SUCCESS(const char *fmt, ...);
void libws_test_FAILURE(const char *fmt, ...);
void libws_test_STATUS(const char *fmt, ...);
void libws_test_STATUS_ex(const char *fmt, ...);
void libws_test_SKIPPED(const char *fmt, ...);
void libws_test_HEADLINE(const char *headline);

void libws_test_set_malloc_fail_count(int count);
void *libws_test_malloc(size_t sz);
void libws_test_set_realloc_fail_count(int count);
void *libws_test_realloc(void *ptr, size_t sz);

#endif // __LIBWS_TEST_HELPERS_H__
