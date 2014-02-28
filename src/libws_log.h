
#ifndef __LIBWS_LOG_H__
#define __LIBWS_LOG_H__

#include "libws_config.h"
#include <stdarg.h>

#define LIBWS_NONE		(0 << 0)
#define LIBWS_CRIT		(1 << 0)
#define LIBWS_ERR		(1 << 1)
#define LIBWS_WARN		(1 << 2)
#define LIBWS_INFO		(1 << 4)
#define LIBWS_DEBUG		(1 << 5)
#define LIBWS_DEBUG2	(1 << 6)
#define LIBWS_TRACE		(1 << 7)

///
/// Function name for debugging and logging.
///
#ifdef WIN32
#define _LIBWS_FUNC_	__FUNCTION__
#else
#define _LIBWS_FUNC_	__func__
#endif

#ifdef LIBWS_WITH_LOG

// http://stackoverflow.com/questions/5588855/standard-alternative-to-gccs-va-args-trick
#define LIBWS_LOG(prio, fmt, ...) \
	libws_log(prio, __FILE__, _LIBWS_FUNC_, __LINE__, fmt, ##__VA_ARGS__)

void libws_log(int prio, const char *file, 
	const char *func, int line, const char *fmt, ...);

#else

#define LIBWS_LOG(prio, fmt, ...)
#define LIBWS_LOGTRACE(fmt, ...) LIBWS_LOG(LIBWS_TRACE, fmt, ...)

#endif // LIBWS_WITH_LOG

#define LIBWS_LOG_MASK(priority) (1 << (priority))
#define LIBWS_LOG_UPTO(priority) ((priority) | ((priority) - 1))

void ws_set_log_level(int prio);
int ws_get_log_level();

typedef void (*ws_log_callback_f)(int prio, const 
				char *file, const char *func, int line, 
				const char *fmt, va_list args);

void ws_set_log_cb(ws_log_callback_f func);

void ws_default_log_cb(int prio, const char *file, 
	const char *func, int line, const char *fmt, va_list args);

const char *ws_log_get_prio_str(int prio);

#endif // __LIBWS_LOG_H__
