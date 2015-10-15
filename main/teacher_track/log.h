#pragma once

void fatal(const char *mod, const char *fmt, ...);
void error(const char *mod, const char *fmt, ...);
void warning(const char *mod, const char *fmt, ...);
void info(const char *mod, const char *fmt, ...);
void debug(const char *mod, const char *fmt, ...);

// level: 0 FATAL, 1 ERROR, 2 WARNING, 3 INFO, 4 DEBUG
void set_log_level(int level);
