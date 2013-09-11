
#ifndef __LIBWS_TEST_HELPERS_H__
#define __LIBWS_TEST_HELPERS_H__

int  libws_test_parse_cmdline(int argc, char **argv);
void libws_test_SUCCESS(const char *fmt, ...);
void libws_test_FAILURE(const char *fmt, ...);
void libws_test_STATUS(const char *fmt, ...);
void libws_test_STATUS_ex(const char *fmt, ...);

#endif // __LIBWS_TEST_HELPERS_H__
