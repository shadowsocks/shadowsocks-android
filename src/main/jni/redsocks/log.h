#ifndef LOG_H_WED_JAN_24_18_21_27_2007
#define LOG_H_WED_JAN_24_18_21_27_2007

#include <stdarg.h>
#include <stdbool.h>
#include <syslog.h>

#define log_errno(prio, msg...) _log_write(__FILE__, __LINE__, __func__, 1, prio, ## msg)
#define log_error(prio, msg...) _log_write(__FILE__, __LINE__, __func__, 0, prio, ## msg)

int log_preopen(const char *dst, bool log_debug, bool log_info);
void log_open();

void _log_vwrite(const char *file, int line, const char *func, int do_errno, int priority, const char *fmt, va_list ap);

void _log_write(const char *file, int line, const char *func, int do_errno, int priority, const char *fmt, ...)
#if defined(__GNUC__)
	__attribute__ (( format (printf, 6, 7) ))
#endif
;

/* vim:set tabstop=4 softtabstop=4 shiftwidth=4: */
/* vim:set foldmethod=marker foldlevel=32 foldmarker={,}: */
#endif /* LOG_H_WED_JAN_24_18_21_27_2007 */

