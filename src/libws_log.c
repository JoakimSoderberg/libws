
#include "libws_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "libws_log.h"
#include "libws.h"
#include "libws_private.h"

#ifdef _WIN32
#include <WinSock2.h>
#endif // _WIN32

static int _ws_log_level = LIBWS_NONE;
static ws_log_callback_f _ws_log_cb;

///
/// Returns a string representation of the current time.
///
/// @param[in]	buf		A buffer to write the resulting string to.
/// @param[in]	bufsize	The size of #buf.
///
/// @returns A pointer to the buffer passed in.
///
char *_ws_get_time_str(char *buf, size_t bufsize)
{
	const char *fmt = "%Y-%m-%d %H:%M:%S";

	#ifdef _WIN32
	struct tm *now;
	time_t timenow;
	time(&timenow);
	now = localtime(&timenow);
	strftime(buf, bufsize, fmt, now);
	#else
	int ret = 0;
	struct timeval now;
	evutil_gettimeofday(&now, NULL);
	ret = strftime(buf, bufsize, fmt, localtime(&now.tv_sec));
	if (ret != 0)
	{
		snprintf(&buf[ret], bufsize, ".%ld", now.tv_usec);
	}
	#endif

	return buf;
}

const char *ws_log_get_prio_str(int prio)
{
	switch (prio)
	{
		case LIBWS_CRIT:	return "CRITICAL";
		case LIBWS_ERR:		return "ERROR";
		case LIBWS_WARN:	return "WARNING";
		case LIBWS_INFO:	return "INFO";
		case LIBWS_DEBUG:	return "DEBUG";
		case LIBWS_DEBUG2:	return "DEBUG2";
		case LIBWS_TRACE:	return "TRACE";
	}

	return "UNKNOWN";
}

void ws_default_log_cb(int prio, const char *file, 
	const char *func, int line, const char *fmt, va_list args)
{
	char timebuf[256];
	FILE *fd = stdout;

	switch (prio)
	{
		case LIBWS_CRIT:
		case LIBWS_ERR:
		case LIBWS_WARN:
			fd = stderr;
			break;
		case LIBWS_INFO:
		case LIBWS_DEBUG:
		case LIBWS_DEBUG2:
		case LIBWS_TRACE:
			fd = stdout;
			break;
	}

	fprintf(fd, "[%s] %6s, %4d:%-32s: ", 
			_ws_get_time_str(timebuf, sizeof(timebuf)), 
			ws_log_get_prio_str(prio), line, func);
	vfprintf(fd, fmt, args);
	fprintf(fd, "\n");
}

void libws_log(int prio, const char *file, 
	const char *func, int line, const char *fmt, ...)
{
	va_list args;

	if (!(_ws_log_level & prio))
	{
		return;
	}

	// Call the user callback.
	if (_ws_log_cb)
	{
		va_start(args, fmt);
		_ws_log_cb(prio, file, func, line, fmt, args);
		va_end(args);
	}
}

void ws_set_log_level(int prio)
{
	_ws_log_level = prio;
}

int ws_get_log_level()
{
	return _ws_log_level;
}

void ws_set_log_cb(ws_log_callback_f func)
{
	_ws_log_cb = func;
}


