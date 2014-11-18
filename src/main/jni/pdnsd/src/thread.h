/* thread.h - Threading helpers

   Copyright (C) 2000 Thomas Moestl
   Copyright (C) 2002, 2003, 2005 Paul A. Rombouts

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


#ifndef _THREAD_H_
#define _THREAD_H_

#include <config.h>
#include <pthread.h>
#include <signal.h>

/* --- from main.c */
extern sigset_t sigs_msk;
/* --- */

#if (TARGET==TARGET_LINUX) && !defined(THREADLIB_NPTL)
extern volatile short int waiting;
void thread_sig(int sig);
#endif

/* These are macros for setting up the signal handling of a new thread. They
 * are needed because the LinuxThreads implementation obviously has some
 * problems in signal handling, which makes the recommended solution (doing
 * sigwait() in one thread and blocking the signals in all threads) impossible.
 * So, for Linux, we have to install the fatal_sig handler.
 * It seems to me that signal handlers in fact aren't shared between threads
 * under Linux. Also, sigwait() does not seem to work as indicated in the docs */

/* Note added by Paul Rombouts: In the new Native POSIX Thread Library for Linux (NPTL)
   signal handling has changed from per-thread signal handling to POSIX process signal handling,
   which makes the recommended solution mentioned by Thomas Moestl possible.
   In this case I can simply define THREAD_SIGINIT to be empty.
   The signals are blocked in main() before any threads are created,
   and we simply never unblock them except by calling sigwait() in main(). */

#if (TARGET==TARGET_LINUX)
# ifdef THREADLIB_NPTL
# define THREAD_SIGINIT
# else
#  ifdef THREADLIB_LINUXTHREADS2
#  define THREAD_SIGINIT   {				\
	struct sigaction action;			\
	pthread_sigmask(SIG_UNBLOCK,&sigs_msk,NULL);	\
	action.sa_handler = thread_sig;			\
	action.sa_mask = sigs_msk;			\
	action.sa_flags = 0;				\
	sigaction(SIGINT,&action,NULL);			\
	sigaction(SIGILL,&action,NULL);			\
	sigaction(SIGABRT,&action,NULL);		\
	sigaction(SIGFPE,&action,NULL);			\
	sigaction(SIGSEGV,&action,NULL);		\
	sigaction(SIGTSTP,&action,NULL);		\
	sigaction(SIGTTOU,&action,NULL);		\
	sigaction(SIGTTIN,&action,NULL);		\
	sigaction(SIGTERM,&action,NULL);		\
	action.sa_handler = SIG_IGN;			\
	sigemptyset(&action.sa_mask);			\
	action.sa_flags = 0;				\
	sigaction(SIGPIPE,&action,NULL);		\
   }
#  else
#  define THREAD_SIGINIT   {				\
	struct sigaction action;			\
	pthread_sigmask(SIG_UNBLOCK,&sigs_msk,NULL);	\
	action.sa_handler = thread_sig;			\
	action.sa_mask = sigs_msk;			\
	action.sa_flags = 0;				\
	sigaction(SIGILL,&action,NULL);			\
	sigaction(SIGABRT,&action,NULL);		\
	sigaction(SIGFPE,&action,NULL);			\
	sigaction(SIGSEGV,&action,NULL);		\
	sigaction(SIGTSTP,&action,NULL);		\
	sigaction(SIGTTOU,&action,NULL);		\
	sigaction(SIGTTIN,&action,NULL);		\
	action.sa_handler = SIG_IGN;			\
	sigemptyset(&action.sa_mask);			\
	action.sa_flags = 0;				\
	sigaction(SIGPIPE,&action,NULL);		\
   }
#  endif
# endif
#elif (TARGET==TARGET_BSD) || (TARGET==TARGET_CYGWIN)
#define THREAD_SIGINIT pthread_sigmask(SIG_BLOCK,&sigs_msk,NULL)
#else
# error Unsupported platform!
#endif


/* This is a thread-safe usleep().
   Implementation of the BSD usleep function using nanosleep.
*/
inline static int usleep_r(unsigned long useconds)
  __attribute__((always_inline));
inline static int usleep_r(unsigned long useconds)
{
  struct timespec ts = { tv_sec: (useconds / 1000000),
			 tv_nsec: (useconds % 1000000) * 1000ul };

  return nanosleep(&ts, NULL);
}

/* This is a thread-safe sleep().
   The semantics are somewhat different from the POSIX sleep function,
   but it suits our purposes.
*/
inline static int sleep_r (unsigned int seconds)
  __attribute__((always_inline));
inline static int sleep_r (unsigned int seconds)
{
  struct timespec ts = { tv_sec: seconds, tv_nsec: 0 };

  return nanosleep(&ts, NULL);
}


/* Used for creating detached threads */
extern pthread_attr_t attr_detached;

#if DEBUG>0
/* Key for storing private thread ID's */
extern pthread_key_t thrid_key;
#endif

#endif
