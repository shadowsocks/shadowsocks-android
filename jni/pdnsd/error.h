/* error.h - Error handling

   Copyright (C) 2000, 2001 Thomas Moestl
   Copyright (C) 2003, 2004, 2011 Paul A. Rombouts

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


#ifndef ERROR_H
#define ERROR_H

#include <config.h>
#include <time.h>
#include <pthread.h>
#include <stdio.h>
#include <signal.h>
#include <syslog.h>

#include "thread.h"
#include "helpers.h"
#include "pdnsd_assert.h"

/* --- from error.c */
extern volatile short int use_log_lock;
/* --- */

void crash_msg(char *msg);

inline static void init_log_lock(void) __attribute__((always_inline));
inline static void init_log_lock(void)
{
	use_log_lock=1;
}

void log_message(int prior,const char *s, ...) printfunc(2, 3);
#if !defined(CPP_C99_VARIADIC_MACROS)
/* GNU C Macro Varargs style. */
#define log_error(args...) log_message(LOG_ERR,args)
#define log_warn(args...) log_message(LOG_WARNING,args)
#define log_info(level,args...) {if((level)<=global.verbosity) log_message(LOG_INFO,args);}
#else
/* ANSI C99 style. */
#define log_error(...) log_message(LOG_ERR,__VA_ARGS__)
#define log_warn(...) log_message(LOG_WARNING,__VA_ARGS__)
#define log_info(level,...) {if((level)<=global.verbosity) log_message(LOG_INFO,__VA_ARGS__);}
#endif

/* Following are some ugly macros for debug messages that
 * should inhibit any code generation when DEBUG is not defined.
 * Of course, those messages could be done in a function, but I
 * want to save the overhead when DEBUG is not defined.
 * debug_p needs to be defined (by including conff.h), or you
 * will get strange errors.
 * A macro call expands to a complete statement, so a semicolon after
 * the macro call is redundant.
 * The arguments are normal printfs, so you know how to use the args
 */
#if DEBUG>0
void debug_msg(int c, const char *fmt, ...) printfunc(2, 3);
/* from main.c */
extern FILE *dbg_file;
#endif

#if !defined(CPP_C99_VARIADIC_MACROS)
/* GNU C Macro Varargs style. */
# if DEBUG > 0
#  define DEBUG_MSG(args...)	{if (debug_p) debug_msg(0,args);}
#  define DEBUG_MSGC(args...)	{if (debug_p) debug_msg(1,args);}
#  define DEBUG_PDNSDA_MSG(args...) {char _debugsockabuf[ADDRSTR_MAXLEN]; DEBUG_MSG(args);}
#  define PDNSDA2STR(a)		pdnsd_a2str(a,_debugsockabuf,sizeof(_debugsockabuf))
#  define DEBUG_RHN_MSG(args...) {unsigned char _debugstrbuf[DNSNAMEBUFSIZE]; DEBUG_MSG(args);}
#  define RHN2STR(a)		rhn2str(a,_debugstrbuf,sizeof(_debugstrbuf))
# else
#  define DEBUG_MSG(args...)
#  define DEBUG_MSGC(args...)
#  define DEBUG_PDNSDA_MSG(args...)
#  define DEBUG_RHN_MSG(args...)
# endif	/* DEBUG > 0 */
#else
/* ANSI C99 style. */
# if DEBUG > 0
/*
 * XXX: The ANSI and GCC variadic macros should be merged as far as possible, but that
 *      might make things even more messy...
 */
#  define DEBUG_MSG(...)	{if (debug_p) debug_msg(0,__VA_ARGS__);}
#  define DEBUG_MSGC(...)	{if (debug_p) debug_msg(1,__VA_ARGS__);}
#  define DEBUG_PDNSDA_MSG(...)	{char _debugsockabuf[ADDRSTR_MAXLEN]; DEBUG_MSG(__VA_ARGS__);}
#  define PDNSDA2STR(a)		pdnsd_a2str(a,_debugsockabuf,ADDRSTR_MAXLEN)
#  define DEBUG_RHN_MSG(...)	{unsigned char _debugstrbuf[DNSNAMEBUFSIZE]; DEBUG_MSG(__VA_ARGS__);}
#  define RHN2STR(a)		rhn2str(a,_debugstrbuf,sizeof(_debugstrbuf))
# else
#  define DEBUG_MSG(...)
#  define DEBUG_MSGC(...)
#  define DEBUG_PDNSDA_MSG(...)
#  define DEBUG_RHN_MSG(...)
# endif	/* DEBUG > 0 */
#endif

#endif
