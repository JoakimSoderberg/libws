
#include "libws_config.h"

#include "libws.h"
#include "libws_private.h"
#include "libws_log.h"

void libws_log(int prio, const char *file, 
	const char *func, int line, const char *fmt, ...);
{
	va_list va_args;

	va_start(va_args);

	switch (prio)
	{
		case LIBWS_CRIT:
		case LIBWS_ERR:
		case LIBWS_WARN:
			vfprintf(stderr, fmt, va_args);
			break;
		case LIBWS_INFO:
		case LIBWS_DEBUG:
		case LIBWS_DEBUG2:
		case LIBWS_TRACE:
			vprintf(fmt, va_args);
			break;
	}
	
	va_end(va_args);
}

void libws_set_log_level(ws_t ws, int prio)
{
	assert(ws != NULL);

	ws->debug_level = prio;

	return 0;
}

