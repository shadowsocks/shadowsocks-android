/* error.c - Error handling

   Copyright (C) 2000, 2001 Thomas Moestl
   Copyright (C) 2003, 2004, 2005, 2011 Paul A. Rombouts

  This file is part of the pdnsd package.

  pdnsd is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  pdnsd is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with pdnsd; see the file COPYING. If not, see
  <http://www.gnu.org/licenses/>.
*/

#include <config.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include "error.h"
#include "helpers.h"
#include "conff.h"


pthread_mutex_t loglock = PTHREAD_MUTEX_INITIALIZER;
volatile short int use_log_lock=0;

/*
 * Initialize a mutex for io-locking in order not to produce gibberish on
 * multiple simultaneous errors.
 */
/* This is now defined as an inline function in error.h */
#if 0
void init_log_lock(void)
{
	use_log_lock=1;
}
#endif

/* We crashed? Ooops... */
void crash_msg(char *msg)
{
	log_error("%s", msg);
	log_error("pdnsd probably crashed due to a bug. Please consider sending a bug");
	log_error("report to p.a.rombouts@home.nl or tmoestl@gmx.net");
}

/* Log a warning, error or info message.
 * If we are a daemon, use the syslog. s is a format string like in printf,
 * the optional following arguments are the arguments like in printf */
void log_message(int prior, const char *s, ...)
{
#ifndef ANDROID
	int gotlock=0;
	va_list va;
	FILE *f;

	if (use_log_lock) {
		gotlock=softlock_mutex(&loglock);
		/* If we failed to get the lock and the type of the
		   message is "info" or less important, then don't bother. */
		if(!gotlock && prior>=LOG_INFO)
			return;
	}
	if (global.daemon) {
		openlog("pdnsd",LOG_PID,LOG_DAEMON);
		va_start(va,s);
		vsyslog(prior,s,va);
		va_end(va);
		closelog();
	}
	else {
		f=stderr;
#if DEBUG > 0
		goto printtofile;
	}
	if(debug_p) {
		f=dbg_file;
	printtofile:
#endif
		{
			char ts[sizeof "* 12/31 23:59:59| "];
			time_t tt = time(NULL);
			struct tm tm;

			if(!localtime_r(&tt, &tm) || strftime(ts, sizeof(ts), "* %m/%d %T| ", &tm) <=0)
				ts[0]=0;
			fprintf(f,"%spdnsd: %s: ", ts,
				prior<=LOG_CRIT?"critical":
				prior==LOG_ERR?"error":
				prior==LOG_WARNING?"warning":
				"info");
		}
		va_start(va,s);
		vfprintf(f,s,va);
		va_end(va);
		{
			const char *p=strchr(s,0);
			if(!p || p==s || *(p-1)!='\n')
				fputc('\n',f);
		}
	}
	if (gotlock)
		pthread_mutex_unlock(&loglock);
#endif
}


#if DEBUG > 0
/* XXX: The timestamp generation makes this a little heavy-weight */
void debug_msg(int c, const char *fmt, ...)
{
	va_list va;

	if (!c) {
		char ts[sizeof "12/31 23:59:59"];
		time_t tt = time(NULL);
		struct tm tm;
		unsigned *id;

		if(localtime_r(&tt, &tm) && strftime(ts, sizeof(ts), "%m/%d %T", &tm) > 0) {
			if((id = (unsigned *)pthread_getspecific(thrid_key)))
				fprintf(dbg_file,"%u %s| ", *id, ts);
			else
				fprintf(dbg_file,"- %s| ", ts);
		}
	}
	va_start(va,fmt);
	vfprintf(dbg_file,fmt,va);
	va_end(va);
	fflush(dbg_file);
}
#endif /* DEBUG */
