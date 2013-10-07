
#include "libws_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "libws_log.h"
#include "libws.h"
#include "libws_private.h"

static int _ws_log_level = LIBWS_NONE;
static ws_log_callback_f _ws_log_cb;

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

	fprintf(fd, "%s, %d:%-35s: ", ws_log_get_prio_str(prio), line, func);
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


