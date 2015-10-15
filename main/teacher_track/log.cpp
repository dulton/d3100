#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "runtime.h"

static int _level = 3;
static double _0 = now();
static const char *stamp_prefix()
{
	static char buf[64];
	snprintf(buf, sizeof(buf), "%.3f ", now() - _0);
	return buf;
}

void set_log_level(int l)
{
	_level = l;
}

const char *_level_prefix[] = {
	"FATAL:", "ERR:", "WARNING:", "INFO:", "DEBUG:",
};

static void _log(int level, const char *mod, const char *info)
{
	fprintf(stdout, "%s: %s: [%s]: %s", stamp_prefix(), 
			_level_prefix[level], mod,
			info);
}

void fatal(const char *mod, const char *fmt, ...)
{
	if (_level < 0) return;
	char buf[1024];
	va_list args;

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	_log(0, mod, buf);
	exit(-1);	// 
}

void error(const char *mod, const char *fmt, ...)
{
	if (_level < 1) return;
	char buf[1024];
	va_list args;

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	_log(1, mod, buf);
}

void warning(const char *mod, const char *fmt, ...)
{
	if (_level < 2) return;
	char buf[1024];
	va_list args;

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	_log(2, mod, buf);
}

void info(const char *mod, const char *fmt, ...)
{
	if (_level < 3) return;
	char buf[1024];
	va_list args;

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	_log(3, mod, buf);
}

void debug(const char *mod, const char *fmt, ...)
{
	if (_level < 4) return;
	char buf[1024];
	va_list args;

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	_log(4, mod, buf);
}
