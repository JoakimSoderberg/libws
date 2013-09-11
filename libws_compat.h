
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

#endif // __LIBWS_COMPAT_H__
