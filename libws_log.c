
#include "libws_config.h"

#include "libws.h"
#include "libws_private.h"
#include "libws_log.h"

static int _ws_log_level = LIBWS_NONE;
static ws_log_callback_f _ws_log_cb = NULL;

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

void libws_default_log_cb(int prio, const char *file, 
	const char *func, int line, const char *fmt, va_list args)
{
	int fd = stdio;

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
			fd = stdio;
			break;
	}

	fprintf(fd, "%s, %s(),%d: ", ws_log_get_prio_str(prio), func, line);
	vfprintf(fd, fmt, va_args);
	fprintf(fd, "\n");
}

void libws_log(int prio, const char *file, 
	const char *func, int line, const char *fmt, ...);
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
		va_end(args)
	}
}

void libws_set_log_level(int prio)
{
	_ws_log_level = prio;
}

int libws_get_log_level()
{
	return _ws_log_level;
}

void ws_set_log_cb(ws_log_callback_f func)
{
	_ws_log_cb = func;
}


