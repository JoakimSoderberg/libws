
#ifndef __LIBWS_COMPAT_H__
#define __LIBWS_COMPAT_H__

#include "libws_config.h"
#include <stdarg.h>
#include <stdint.h>
#include <inttypes.h>

#ifndef va_copy
#define va_copy(dest, src) memcpy(&(dest), &(src), sizeof(va_list));
#endif // va_copy

uint64_t libws_ntoh64(const uint64_t input);
uint64_t libws_hton64(const uint64_t input);

char *libws_strsep(char **s, const char *del);
char *ws_rtrim(char *s);

#ifdef _WIN32
#define strcasecmp _stricmp 
#define strncasecmp _strnicmp 
#endif

#endif // __LIBWS_COMPAT_H__
