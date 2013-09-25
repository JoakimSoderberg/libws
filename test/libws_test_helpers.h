
#ifndef __LIBWS_TEST_HELPERS_H__
#define __LIBWS_TEST_HELPERS_H__

int libws_test_verbose();
int libws_test_log_on();
int libws_test_parse_cmdline(int argc, char **argv);
int libws_test_init(int argc, char **argv);

void libws_test_SUCCESS(const char *fmt, ...);
void libws_test_FAILURE(const char *fmt, ...);
void libws_test_STATUS(const char *fmt, ...);
void libws_test_STATUS_ex(const char *fmt, ...);
void libws_test_SKIPPED(const char *fmt, ...);
void libws_test_HEADLINE(const char *headline);

#endif // __LIBWS_TEST_HELPERS_H__
