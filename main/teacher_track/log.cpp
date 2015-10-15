#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

static double now()
{
	struct timeval tv;
	gettimeofday(&tv, 0);
	return tv.tv_sec + tv.tv_usec/1000000.0;
}

static double _0 = now();

static const char *stamp_prefix()
{
	static char buf[64];
	snprintf(buf, sizeof(buf), "%.3f ", now() - _0);
	return buf;
}

const char *_level_prefix[] = {
	"ERR:", "WARNING:", "INFO:",
};

static void _log(int level, const char *mod, const char *info)
{
	fprintf(stdout, "%s: %s: [%s]: %s", stamp_prefix(), 
			_level_prefix[level], mod,
			info);
}

void error(const char *mod, const char *fmt, ...)
{
	char buf[1024];
	va_list args;

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	_log(0, mod, buf);
}

void warning(const char *mod, const char *fmt, ...)
{
	char buf[1024];
	va_list args;

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	_log(1, mod, buf);
}

void info(const char *mod, const char *fmt, ...)
{
	char buf[1024];
	va_list args;

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	_log(2, mod, buf);
}

