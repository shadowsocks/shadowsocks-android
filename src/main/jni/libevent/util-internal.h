/*
 * Copyright (c) 2007-2012 Niels Provos and Nick Mathewson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _EVENT_UTIL_INTERNAL_H
#define _EVENT_UTIL_INTERNAL_H

#include "event2/event-config.h"
#include <errno.h>

/* For EVUTIL_ASSERT */
#include "log-internal.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef _EVENT_HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#include "event2/util.h"

#include "ipv6-internal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* If we need magic to say "inline", get it for free internally. */
#ifdef _EVENT_inline
#define inline _EVENT_inline
#endif
#ifdef _EVENT___func__
#define __func__ _EVENT___func__
#endif

/* A good no-op to use in macro definitions. */
#define _EVUTIL_NIL_STMT ((void)0)
/* A no-op that tricks the compiler into thinking a condition is used while
 * definitely not making any code for it.  Used to compile out asserts while
 * avoiding "unused variable" warnings.  The "!" forces the compiler to
 * do the sizeof() on an int, in case "condition" is a bitfield value.
 */
#define _EVUTIL_NIL_CONDITION(condition) do { \
	(void)sizeof(!(condition));  \
} while(0)

/* Internal use only: macros to match patterns of error codes in a
   cross-platform way.  We need these macros because of two historical
   reasons: first, nonblocking IO functions are generally written to give an
   error on the "blocked now, try later" case, so sometimes an error from a
   read, write, connect, or accept means "no error; just wait for more
   data," and we need to look at the error code.  Second, Windows defines
   a different set of error codes for sockets. */

#ifndef WIN32

/* True iff e is an error that means a read/write operation can be retried. */
#define EVUTIL_ERR_RW_RETRIABLE(e)				\
	((e) == EINTR || (e) == EAGAIN)
/* True iff e is an error that means an connect can be retried. */
#define EVUTIL_ERR_CONNECT_RETRIABLE(e)			\
	((e) == EINTR || (e) == EINPROGRESS)
/* True iff e is an error that means a accept can be retried. */
#define EVUTIL_ERR_ACCEPT_RETRIABLE(e)			\
	((e) == EINTR || (e) == EAGAIN || (e) == ECONNABORTED)

/* True iff e is an error that means the connection was refused */
#define EVUTIL_ERR_CONNECT_REFUSED(e)					\
	((e) == ECONNREFUSED)

#else

#define EVUTIL_ERR_RW_RETRIABLE(e)					\
	((e) == WSAEWOULDBLOCK ||					\
	    (e) == WSAEINTR)

#define EVUTIL_ERR_CONNECT_RETRIABLE(e)					\
	((e) == WSAEWOULDBLOCK ||					\
	    (e) == WSAEINTR ||						\
	    (e) == WSAEINPROGRESS ||					\
	    (e) == WSAEINVAL)

#define EVUTIL_ERR_ACCEPT_RETRIABLE(e)			\
	EVUTIL_ERR_RW_RETRIABLE(e)

#define EVUTIL_ERR_CONNECT_REFUSED(e)					\
	((e) == WSAECONNREFUSED)

#endif

#ifdef _EVENT_socklen_t
#define socklen_t _EVENT_socklen_t
#endif

/* Arguments for shutdown() */
#ifdef SHUT_RD
#define EVUTIL_SHUT_RD SHUT_RD
#else
#define EVUTIL_SHUT_RD 0
#endif
#ifdef SHUT_WR
#define EVUTIL_SHUT_WR SHUT_WR
#else
#define EVUTIL_SHUT_WR 1
#endif
#ifdef SHUT_BOTH
#define EVUTIL_SHUT_BOTH SHUT_BOTH
#else
#define EVUTIL_SHUT_BOTH 2
#endif

/* Locale-independent replacements for some ctypes functions.  Use these
 * when you care about ASCII's notion of character types, because you are about
 * to send those types onto the wire.
 */
int EVUTIL_ISALPHA(char c);
int EVUTIL_ISALNUM(char c);
int EVUTIL_ISSPACE(char c);
int EVUTIL_ISDIGIT(char c);
int EVUTIL_ISXDIGIT(char c);
int EVUTIL_ISPRINT(char c);
int EVUTIL_ISLOWER(char c);
int EVUTIL_ISUPPER(char c);
char EVUTIL_TOUPPER(char c);
char EVUTIL_TOLOWER(char c);

/** Helper macro.  If we know that a given pointer points to a field in a
    structure, return a pointer to the structure itself.  Used to implement
    our half-baked C OO.  Example:

    struct subtype {
	int x;
	struct supertype common;
	int y;
    };
    ...
    void fn(struct supertype *super) {
	struct subtype *sub = EVUTIL_UPCAST(super, struct subtype, common);
	...
    }
 */
#define EVUTIL_UPCAST(ptr, type, field)				\
	((type *)(((char*)(ptr)) - evutil_offsetof(type, field)))

/* As open(pathname, flags, mode), except that the file is always opened with
 * the close-on-exec flag set. (And the mode argument is mandatory.)
 */
int evutil_open_closeonexec(const char *pathname, int flags, unsigned mode);

int evutil_read_file(const char *filename, char **content_out, size_t *len_out,
    int is_binary);

int evutil_socket_connect(evutil_socket_t *fd_ptr, struct sockaddr *sa, int socklen);

int evutil_socket_finished_connecting(evutil_socket_t fd);

int evutil_ersatz_socketpair(int, int , int, evutil_socket_t[]);

int evutil_resolve(int family, const char *hostname, struct sockaddr *sa,
    ev_socklen_t *socklen, int port);

const char *evutil_getenv(const char *name);

long _evutil_weakrand(void);

/* Evaluates to the same boolean value as 'p', and hints to the compiler that
 * we expect this value to be false. */
#if defined(__GNUC__) && __GNUC__ >= 3         /* gcc 3.0 or later */
#define EVUTIL_UNLIKELY(p) __builtin_expect(!!(p),0)
#else
#define EVUTIL_UNLIKELY(p) (p)
#endif

/* Replacement for assert() that calls event_errx on failure. */
#ifdef NDEBUG
#define EVUTIL_ASSERT(cond) _EVUTIL_NIL_CONDITION(cond)
#define EVUTIL_FAILURE_CHECK(cond) 0
#else
#define EVUTIL_ASSERT(cond)						\
	do {								\
		if (EVUTIL_UNLIKELY(!(cond))) {				\
			event_errx(_EVENT_ERR_ABORT,			\
			    "%s:%d: Assertion %s failed in %s",		\
			    __FILE__,__LINE__,#cond,__func__);		\
			/* In case a user-supplied handler tries to */	\
			/* return control to us, log and abort here. */	\
			(void)fprintf(stderr,				\
			    "%s:%d: Assertion %s failed in %s",		\
			    __FILE__,__LINE__,#cond,__func__);		\
			abort();					\
		}							\
	} while (0)
#define EVUTIL_FAILURE_CHECK(cond) EVUTIL_UNLIKELY(cond)
#endif

#ifndef _EVENT_HAVE_STRUCT_SOCKADDR_STORAGE
/* Replacement for sockaddr storage that we can use internally on platforms
 * that lack it.  It is not space-efficient, but neither is sockaddr_storage.
 */
struct sockaddr_storage {
	union {
		struct sockaddr ss_sa;
		struct sockaddr_in ss_sin;
		struct sockaddr_in6 ss_sin6;
		char ss_padding[128];
	} ss_union;
};
#define ss_family ss_union.ss_sa.sa_family
#endif

/* Internal addrinfo error code.  This one is returned from only from
 * evutil_getaddrinfo_common, when we are sure that we'll have to hit a DNS
 * server. */
#define EVUTIL_EAI_NEED_RESOLVE      -90002

struct evdns_base;
struct evdns_getaddrinfo_request;
typedef struct evdns_getaddrinfo_request* (*evdns_getaddrinfo_fn)(
    struct evdns_base *base,
    const char *nodename, const char *servname,
    const struct evutil_addrinfo *hints_in,
    void (*cb)(int, struct evutil_addrinfo *, void *), void *arg);

void evutil_set_evdns_getaddrinfo_fn(evdns_getaddrinfo_fn fn);

struct evutil_addrinfo *evutil_new_addrinfo(struct sockaddr *sa,
    ev_socklen_t socklen, const struct evutil_addrinfo *hints);
struct evutil_addrinfo *evutil_addrinfo_append(struct evutil_addrinfo *first,
    struct evutil_addrinfo *append);
void evutil_adjust_hints_for_addrconfig(struct evutil_addrinfo *hints);
int evutil_getaddrinfo_common(const char *nodename, const char *servname,
    struct evutil_addrinfo *hints, struct evutil_addrinfo **res, int *portnum);

int evutil_getaddrinfo_async(struct evdns_base *dns_base,
    const char *nodename, const char *servname,
    const struct evutil_addrinfo *hints_in,
    void (*cb)(int, struct evutil_addrinfo *, void *), void *arg);

/** Return true iff sa is a looback address. (That is, it is 127.0.0.1/8, or
 * ::1). */
int evutil_sockaddr_is_loopback(const struct sockaddr *sa);


/**
    Formats a sockaddr sa into a string buffer of size outlen stored in out.
    Returns a pointer to out.  Always writes something into out, so it's safe
    to use the output of this function without checking it for NULL.
 */
const char *evutil_format_sockaddr_port(const struct sockaddr *sa, char *out, size_t outlen);

long evutil_tv_to_msec(const struct timeval *tv);

int evutil_hex_char_to_int(char c);

#ifdef WIN32
HANDLE evutil_load_windows_system_library(const TCHAR *library_name);
#endif

#ifndef EV_SIZE_FMT
#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
#define EV_U64_FMT "%I64u"
#define EV_I64_FMT "%I64d"
#define EV_I64_ARG(x) ((__int64)(x))
#define EV_U64_ARG(x) ((unsigned __int64)(x))
#else
#define EV_U64_FMT "%llu"
#define EV_I64_FMT "%lld"
#define EV_I64_ARG(x) ((long long)(x))
#define EV_U64_ARG(x) ((unsigned long long)(x))
#endif
#endif

#ifdef _WIN32
#define EV_SOCK_FMT EV_I64_FMT
#define EV_SOCK_ARG(x) EV_I64_ARG((x))
#else
#define EV_SOCK_FMT "%d"
#define EV_SOCK_ARG(x) (x)
#endif

#if defined(__STDC__) && defined(__STDC_VERSION__)
#if (__STDC_VERSION__ >= 199901L)
#define EV_SIZE_FMT "%zu"
#define EV_SSIZE_FMT "%zd"
#define EV_SIZE_ARG(x) (x)
#define EV_SSIZE_ARG(x) (x)
#endif
#endif

#ifndef EV_SIZE_FMT
#if (_EVENT_SIZEOF_SIZE_T <= _EVENT_SIZEOF_LONG)
#define EV_SIZE_FMT "%lu"
#define EV_SSIZE_FMT "%ld"
#define EV_SIZE_ARG(x) ((unsigned long)(x))
#define EV_SSIZE_ARG(x) ((long)(x))
#else
#define EV_SIZE_FMT EV_U64_FMT
#define EV_SSIZE_FMT EV_I64_FMT
#define EV_SIZE_ARG(x) EV_U64_ARG(x)
#define EV_SSIZE_ARG(x) EV_I64_ARG(x)
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif
