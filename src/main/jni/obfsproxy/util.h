/* Copyright 2011 Nick Mathewson, George Kadianakis
   See LICENSE for other credits and copying information
*/

/**
 * \file util.h
 * \brief Headers for util.c.
 **/

#ifndef UTIL_H
#define UTIL_H

#include "config.h"

#include <limits.h>
#include <stdarg.h> /* va_list */
#include <stddef.h> /* size_t, ptrdiff_t, offsetof, NULL */
#include <stdint.h> /* intN_t, uintN_t */
#include <stdlib.h>
#include <string.h>

#include <event2/util.h> /* evutil_addrinfo */
#ifdef _WIN32
#include <ws2tcpip.h> /* addrinfo (event2/util.h should do this,
                         but it doesn't) */
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

struct bufferevent;
struct evbuffer;
struct evconnlistener;
struct evdns_base;
struct event_base;

/***** Type annotations. *****/

#ifndef __GNUC__
#define __attribute__(x) /* nothing */
#endif
#define ATTR_MALLOC   __attribute__((malloc))
#define ATTR_NORETURN __attribute__((noreturn))
#define ATTR_PRINTF_1 __attribute__((format(printf, 1, 2)))
#define ATTR_PRINTF_3 __attribute__((format(printf, 3, 4)))
#define ATTR_PURE     __attribute__((pure))

/***** Memory allocation. *****/

/* Because this isn't Tor and functions named "tor_whatever" would be
   confusing, I am instead following the GNU convention of naming
   allocate-memory-or-crash functions "xwhatever". Also, at this time
   I do not see a need for a free() wrapper. */

void *xmalloc(size_t size) ATTR_MALLOC; /* does not clear memory */
void *xzalloc(size_t size) ATTR_MALLOC; /* clears memory */
void *xrealloc(void *ptr, size_t size);
void *xmemdup(const void *ptr, size_t size) ATTR_MALLOC;
char *xstrdup(const char *s) ATTR_MALLOC;
char *xstrndup(const char *s, size_t maxsize) ATTR_MALLOC;

/***** Pseudo-inheritance. *****/

#define DOWNCAST(container_type, element, ptr) \
  (container_type*)( ((char*)ptr) - offsetof(container_type, element) )

/***** Math. *****/

unsigned int ui64_log2(uint64_t u64);

/***** Network types and functions. *****/

typedef struct circuit_t circuit_t;
typedef struct config_t config_t;
typedef struct conn_t conn_t;
typedef struct protocol_vtable protocol_vtable;
typedef struct socks_state_t socks_state_t;

enum recv_ret {
  /* Everything went fine. */
  RECV_GOOD=0,
  /* Something went bad. */
  RECV_BAD,
  /* ...need...more...data... */
  RECV_INCOMPLETE,

  /* Originally needed by the obfs2 protocol but it might get other
     users in the future.
     It means:
     "We have pending data that we have to send. You should do that by
     calling proto_send() immediately." */
  RECV_SEND_PENDING
};

enum listen_mode {
  LSN_SIMPLE_CLIENT = 1,
  LSN_SIMPLE_SERVER,
  LSN_SOCKS_CLIENT
};

struct evutil_addrinfo *resolve_address_port(const char *address,
                                             int nodns, int passive,
                                             const char *default_port);

/** Produce a printable name for this sockaddr.  The result is in
    malloced memory. */
char *printable_address(struct sockaddr *addr, socklen_t addrlen);

struct evdns_base *get_evdns_base(void);
int init_evdns_base(struct event_base *base);

/***** String functions. *****/

static inline int ascii_isspace(unsigned char c)
{
  return (c == ' ' ||
          c == '\t' ||
          c == '\r' ||
          c == '\n' ||
          c == '\v' ||
          c == '\f');
}
void ascii_strstrip(char *s, const char *kill);
void ascii_strlower(char *s);

int obfs_vsnprintf(char *str, size_t size,
                   const char *format, va_list args);
int obfs_snprintf(char *str, size_t size,
                  const char *format, ...)
  ATTR_PRINTF_3;

int obfs_asprintf(char **strp, const char *fmt, ...);
int obfs_vasprintf(char **strp, const char *fmt, va_list args);

/***** Logging. *****/

/** Log destinations */

/** Spit log messages on stderr. */
#define LOG_METHOD_STDERR 1
/** Place log messages in a file. */
#define LOG_METHOD_FILE 2
/** We don't want no logs. */
#define LOG_METHOD_NULL 3

/** Length of the date-time format that we use in log messages, which is
    "yyyy-mm-dd hh:mm:ss" (without quotes). */
#define ISO_TIME_LEN 19

/** Safely log an address, scrubbing if necessary. */
extern int safe_logging; /* defined in main.c */
const char * safe_str(const char *address);

/** Set the log method, and open the logfile 'filename' if appropriate. */
int log_set_method(int method, const char *filename);

/** Set the minimum severity that will be logged.
    'sev_string' may be "warn", "notice", "info", or "debug"
    (case-insensitively). */
int log_set_min_severity(const char* sev_string);

/** True if debug messages are being logged. */
int log_do_debug(void);

/** Close the logfile if it's open.  Ignores errors. */
void close_obfsproxy_logfile(void);

/** The actual log-emitting functions */

/** Fatal errors: the program cannot continue and will exit. */
void log_error(const char *format, ...)
  ATTR_PRINTF_1 ATTR_NORETURN;

/** Fatal errors: the program cannot continue and will abort */
void log_error_abort(const char *format, ...)
  ATTR_PRINTF_1 ATTR_NORETURN;

/** Warn-level severity: for messages that only appear when something
    has gone wrong. */
void log_warn(const char *format, ...)
  ATTR_PRINTF_1;

/** Notice-level severity: for messages that most users would like to
    learn about during normal operation. */
void log_notice(const char *format, ...)
  ATTR_PRINTF_1;

/** Info-level severity: for rather verbose messages of some interest to
    users during normal operation. */
void log_info(const char *format, ...)
  ATTR_PRINTF_1;

/** Debug-level severity: for hyper-verbose messages of no interest to
    anybody but developers. */
void log_debug(const char *format, ...)
  ATTR_PRINTF_1;

void log_debug_raw(const char *format, va_list ap);

/** Assertion checking.  We don't ever compile assertions out, and we
    want precise control over the error messages, so we use our own
    assertion macros. */
#define obfs_assert(expr)                                     \
  do {                                                        \
    if (!(expr))                                              \
      log_error_abort("assertion failure at %s:%d: %s",       \
                      __FILE__, __LINE__, #expr);             \
  } while (0)

#define obfs_abort()                                            \
  do {                                                          \
    log_error_abort("aborted at %s:%d", __FILE__, __LINE__);    \
  } while (0)

#endif
