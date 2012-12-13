
#ifndef __LIBWS_COMPAT_H__
#define __LIBWS_COMPAT_H__

#include "libws_config.h"
#include <stdarg.h>

#ifndef va_copy
#define va_copy(dest, src) memcpy(&(dest), &(src), sizeof(va_list));
#endif // va_copy

#endif // __LIBWS_COMPAT_H__
