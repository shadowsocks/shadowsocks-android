/* redsocks - transparent TCP-to-proxy redirector
 * Copyright (C) 2007-2011 Leonid Evdokimov <leon@darkk.net.ru>
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License.  You may obtain a copy
 * of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <event.h>
#include "utils.h"
#include "log.h"

static const char *lowmem = "<Can't print error, not enough memory>";

typedef void (*log_func)(const char *file, int line, const char *func, int priority, const char *message, const char *appendix);

static void fprint_timestamp(
		FILE* fd,
		const char *file, int line, const char *func, int priority, const char *message, const char *appendix)
{
	struct timeval tv = { };
	gettimeofday(&tv, 0);

	/* XXX: there is no error-checking, IMHO it's better to lose messages
	 *      then to die and stop service */
	if (appendix)
		fprintf(fd, "%lu.%6.6lu %s:%u %s(...) %s: %s\n", tv.tv_sec, tv.tv_usec, file, line, func, message, appendix);
	else
		fprintf(fd, "%lu.%6.6lu %s:%u %s(...) %s\n", tv.tv_sec, tv.tv_usec, file, line, func, message);
}

static void stderr_msg(const char *file, int line, const char *func, int priority, const char *message, const char *appendix)
{
	fprint_timestamp(stderr, file, line, func, priority, message, appendix);
}

static FILE *logfile = NULL;

static void logfile_msg(const char *file, int line, const char *func, int priority, const char *message, const char *appendix)
{
	fprint_timestamp(logfile, file, line, func, priority, message, appendix);
	fflush(logfile);
}

static void syslog_msg(const char *file, int line, const char *func, int priority, const char *message, const char *appendix)
{
	if (appendix)
		syslog(priority, "%s: %s\n", message, appendix);
	else
		syslog(priority, "%s\n", message);
}

static log_func log_msg = stderr_msg;
static log_func log_msg_next = NULL;


int log_preopen(const char *dst, bool log_debug, bool log_info)
{
	const char *syslog_prefix = "syslog:";
	const char *file_prefix = "file:";
	if (strcmp(dst, "stderr") == 0) {
		log_msg_next = stderr_msg;
	}
	else if (strncmp(dst, syslog_prefix, strlen(syslog_prefix)) == 0) {
		const char *facility_name = dst + strlen(syslog_prefix);
		int facility = -1;
		int logmask;
		struct {
			char *name; int value;
		} *ptpl, tpl[] = {
			{ "daemon", LOG_DAEMON },
			{ "local0", LOG_LOCAL0 },
			{ "local1", LOG_LOCAL1 },
			{ "local2", LOG_LOCAL2 },
			{ "local3", LOG_LOCAL3 },
			{ "local4", LOG_LOCAL4 },
			{ "local5", LOG_LOCAL5 },
			{ "local6", LOG_LOCAL6 },
			{ "local7", LOG_LOCAL7 },
		};

		FOREACH(ptpl, tpl)
			if (strcmp(facility_name, ptpl->name) == 0) {
				facility = ptpl->value;
				break;
			}
		if (facility == -1) {
			log_error(LOG_ERR, "log_preopen(%s, ...): unknown syslog facility", dst);
			return -1;
		}

		openlog("redsocks", LOG_NDELAY | LOG_PID, facility);

		logmask = setlogmask(0);
		if (!log_debug)
			logmask &= ~(LOG_MASK(LOG_DEBUG));
		if (!log_info)
			logmask &= ~(LOG_MASK(LOG_INFO));
		setlogmask(logmask);

		log_msg_next = syslog_msg;
	}
	else if (strncmp(dst, file_prefix, strlen(file_prefix)) == 0) {
		const char *filename = dst + strlen(file_prefix);
		if ((logfile = fopen(filename, "a")) == NULL) {
			log_error(LOG_ERR, "log_preopen(%s, ...): %s", dst, strerror(errno));
			return -1;
		}
		log_msg_next = logfile_msg;
		/* TODO: add log rotation */
	}
	else {
		log_error(LOG_ERR, "log_preopen(%s, ...): unknown destination", dst);
		return -1;
	}
	return 0;
}

void log_open()
{
	log_msg = log_msg_next;
	log_msg_next = NULL;
}

void _log_vwrite(const char *file, int line, const char *func, int do_errno, int priority, const char *fmt, va_list ap)
{
	int saved_errno = errno;
	struct evbuffer *buff = evbuffer_new();
	const char *message;

	if (buff) {
		evbuffer_add_vprintf(buff, fmt, ap);
		message = (const char*)EVBUFFER_DATA(buff);
	}
	else
		message = lowmem;

	log_msg(file, line, func, priority, message, do_errno ? strerror(saved_errno) : NULL);

	if (buff)
		evbuffer_free(buff);
}

void _log_write(const char *file, int line, const char *func, int do_errno, int priority, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	_log_vwrite(file, line, func, do_errno, priority, fmt, ap);
	va_end(ap);
}

/* vim:set tabstop=4 softtabstop=4 shiftwidth=4: */
/* vim:set foldmethod=marker foldlevel=32 foldmarker={,}: */
