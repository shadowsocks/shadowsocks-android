/* Copyright 2011 Nick Mathewson, George Kadianakis
   See LICENSE for other credits and copying information
*/

/**
 * \file util.c
 * \headerfile util.h
 * \brief Utility functions.
 *
 * \details Contains:
 *      - wrappers of malloc() et al.
 *      - network functions
 *      - string functions
 *      - functions used by obfsproxy's logging subsystem
 **/

#include "util.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>

#include <event2/dns.h>
#ifndef _WIN32
#include <arpa/inet.h>
#endif
#ifdef AF_LOCAL
#include <sys/un.h>
#endif

/** Any size_t larger than this amount is likely to be an underflow. */
#define SIZE_T_CEILING  (SIZE_MAX/2 - 16)

/**************************** Memory Allocation ******************************/

static void ATTR_NORETURN
die_oom(void)
{
  log_error("Memory allocation failed: %s",strerror(errno));
}

void *
xmalloc(size_t size)
{
  void *result;

  obfs_assert(size < SIZE_T_CEILING);

  /* Some malloc() implementations return NULL when the input argument
     is zero. We don't bother detecting whether the implementation we're
     being compiled for does that, because it should hardly ever come up,
     and avoiding it unconditionally does no harm. */
  if (size == 0)
    size = 1;

  result = malloc(size);
  if (result == NULL)
    die_oom();

  return result;
}

void *
xrealloc(void *ptr, size_t size)
{
  void *result;
  obfs_assert (size < SIZE_T_CEILING);
  if (size == 0)
    size = 1;

  result = realloc(ptr, size);
  if (result == NULL)
    die_oom();

  return result;
}

void *
xzalloc(size_t size)
{
  void *result = xmalloc(size);
  memset(result, 0, size);
  return result;
}

void *
xmemdup(const void *ptr, size_t size)
{
  void *copy = xmalloc(size);
  memcpy(copy, ptr, size);
  return copy;
}

char *
xstrdup(const char *s)
{
  return xmemdup(s, strlen(s) + 1);
}

char *
xstrndup(const char *s, size_t maxsize)
{
  char *copy;
  size_t size;
  /* strnlen is not in any standard :-( */
  for (size = 0; size < maxsize; size++)
    if (s[size] == '\0')
      break;

  copy = xmalloc(size + 1);
  memcpy(copy, s, size);
  copy[size] = '\0';
  return copy;
}

/******************************** Mathematics ********************************/

unsigned int
ui64_log2(uint64_t u64)
{
  unsigned int r = 0;
  if (u64 >= (((uint64_t)1)<<32)) {
    u64 >>= 32;
    r = 32;
  }
  if (u64 >= (((uint64_t)1)<<16)) {
    u64 >>= 16;
    r += 16;
  }
  if (u64 >= (((uint64_t)1)<<8)) {
    u64 >>= 8;
    r += 8;
  }
  if (u64 >= (((uint64_t)1)<<4)) {
    u64 >>= 4;
    r += 4;
  }
  if (u64 >= (((uint64_t)1)<<2)) {
    u64 >>= 2;
    r += 2;
  }
  if (u64 >= (((uint64_t)1)<<1)) {
    u64 >>= 1;
    r += 1;
  }
  return r;
}

/************************ Obfsproxy Network Routines *************************/

/**
   Accepts a string 'address' of the form ADDRESS:PORT and attempts to
   parse it into an 'evutil_addrinfo' structure.

   If 'nodns' is set it means that 'address' was an IP address.
   If 'passive' is set it means that the address is destined for
   listening and not for connecting.

   If no port was given in 'address', we set 'default_port' as the
   port.
*/
struct evutil_addrinfo *
resolve_address_port(const char *address, int nodns, int passive,
                     const char *default_port)
{
  struct evutil_addrinfo *ai = NULL;
  struct evutil_addrinfo ai_hints;
  int ai_res, ai_errno;
  char *a, *cp;
  const char *portstr;

  if (!address)
    return NULL;

  a = xstrdup(address);

  if ((cp = strchr(a, ':'))) {
    portstr = cp+1;
    *cp = '\0';
  } else if (default_port) {
    portstr = default_port;
  } else {
    log_debug("Error in address %s: port required.", address);
    free(a);
    return NULL;
  }

  memset(&ai_hints, 0, sizeof(ai_hints));
  ai_hints.ai_family = AF_UNSPEC;
  ai_hints.ai_socktype = SOCK_STREAM;
  ai_hints.ai_flags = EVUTIL_AI_ADDRCONFIG | EVUTIL_AI_NUMERICSERV;
  if (passive)
    ai_hints.ai_flags |= EVUTIL_AI_PASSIVE;
  if (nodns)
    ai_hints.ai_flags |= EVUTIL_AI_NUMERICHOST;

  ai_res = evutil_getaddrinfo(a, portstr, &ai_hints, &ai);
  ai_errno = errno;

  free(a);

  if (ai_res) {
    if (ai_res == EVUTIL_EAI_SYSTEM)
      log_warn("Error resolving %s: %s [%s]",
               address, evutil_gai_strerror(ai_res), strerror(ai_errno));
    else
      log_warn("Error resolving %s: %s", address, evutil_gai_strerror(ai_res));

    if (ai) {
      evutil_freeaddrinfo(ai);
      ai = NULL;
    }
  } else if (ai == NULL) {
    log_warn("No result for address %s", address);
  }

  return ai;
}

const char *
safe_str(const char *address)
{
  if (safe_logging)
    return "[scrubbed]";
  else
    return address;
}

char *
printable_address(struct sockaddr *addr, socklen_t addrlen)
{
  char apbuf[INET6_ADDRSTRLEN + 8]; /* []:65535 is 8 characters */

  switch (addr->sa_family) {
#ifndef _WIN32 /* Windows XP doesn't have inet_ntop. Fix later. */
  case AF_INET: {
    char abuf[INET6_ADDRSTRLEN];
    struct sockaddr_in *sin = (struct sockaddr_in*)addr;
    if (!inet_ntop(AF_INET, &sin->sin_addr, abuf, INET6_ADDRSTRLEN))
      break;
    obfs_snprintf(apbuf, sizeof apbuf, "%s:%d", abuf, ntohs(sin->sin_port));
    return xstrdup(apbuf);
  }

  case AF_INET6: {
    char abuf[INET6_ADDRSTRLEN];
    struct sockaddr_in6 *sin6 = (struct sockaddr_in6*)addr;
    if (!inet_ntop(AF_INET, &sin6->sin6_addr, abuf, INET6_ADDRSTRLEN))
      break;
    obfs_snprintf(apbuf, sizeof apbuf, "[%s]:%d", abuf,
                  ntohs(sin6->sin6_port));
    return xstrdup(apbuf);
  }
#endif

#ifdef AF_LOCAL
  case AF_LOCAL:
    return xstrdup(((struct sockaddr_un*)addr)->sun_path);
#endif
  default:
    break;
  }

  obfs_snprintf(apbuf, sizeof apbuf,
                "<addr family %d>", addr->sa_family);
  return xstrdup(apbuf);
}

static struct evdns_base *the_evdns_base = NULL;

struct evdns_base *
get_evdns_base(void)
{
  return the_evdns_base;
}

int
init_evdns_base(struct event_base *base)
{
  the_evdns_base = evdns_base_new(base, 1);
  return the_evdns_base == NULL ? -1 : 0;
}

/************************ String Functions *************************/
/** The functions in this section were carbon copied off tor. Thank you tor! */

/** Replacement for snprintf.  Differs from platform snprintf in two
 * ways: First, always NUL-terminates its output.  Second, always
 * returns -1 if the result is truncated.  (Note that this return
 * behavior does <i>not</i> conform to C99; it just happens to be
 * easier to emulate "return -1" with conformant implementations than
 * it is to emulate "return number that would be written" with
 * non-conformant implementations.) */
int
obfs_snprintf(char *str, size_t size, const char *format, ...)
{
  va_list ap;
  int r;
  va_start(ap,format);
  r = obfs_vsnprintf(str,size,format,ap);
  va_end(ap);
  return r;
}

/** Replacement for vsnprintf; behavior differs as obfs_snprintf differs from
 * snprintf.
 */
int
obfs_vsnprintf(char *str, size_t size, const char *format, va_list args)
{
  int r;
  if (size == 0)
    return -1; /* no place for the NUL */
  if (size > SIZE_T_CEILING)
    return -1;
  r = vsnprintf(str, size, format, args);
  str[size-1] = '\0';
  if (r < 0 || r >= (ssize_t)size)
    return -1;
  return r;
}
/** Remove from the string <b>s</b> every character which appears in
 * <b>strip</b>. */
void
ascii_strstrip(char *s, const char *strip)
{
  char *read = s;
  while (*read) {
    if (strchr(strip, *read)) {
      ++read;
    } else {
      *s++ = *read++;
    }
  }
  *s = '\0';
}

void
ascii_strlower(char *s)
{
  while (*s) {
    if (*s >= 'A' && *s <= 'Z')
      *s = *s - 'A' + 'a';
    ++s;
  }
}

/**
 * Portable asprintf implementation.  Does a printf() into a newly malloc'd
 * string.  Sets *<b>strp</b> to this string, and returns its length (not
 * including the terminating NUL character).
 *
 * You can treat this function as if its implementation were something like
   <pre>
     char buf[_INFINITY_];
     obfs_snprintf(buf, sizeof(buf), fmt, args);
     *strp = xstrdup(buf);
     return strlen(*strp):
   </pre>
 * Where _INFINITY_ is an imaginary constant so big that any string can fit
 * into it.
 */
int
obfs_asprintf(char **strp, const char *fmt, ...)
{
  int r;
  va_list args;
  va_start(args, fmt);
  r = obfs_vasprintf(strp, fmt, args);
  va_end(args);
  if (!*strp || r < 0)
    log_error("Internal error in asprintf");

  return r;
}

/**
 * Portable vasprintf implementation.  Does a printf() into a newly malloc'd
 * string.  Differs from regular vasprintf in the same ways that
 * obfs_asprintf() differs from regular asprintf.
 */
int
obfs_vasprintf(char **strp, const char *fmt, va_list args)
{
  /* use a temporary variable in case *strp is in args. */
  char *strp_tmp=NULL;
#ifdef HAVE_VASPRINTF
  /* If the platform gives us one, use it. */
  int r = vasprintf(&strp_tmp, fmt, args);
  if (r < 0)
    *strp = NULL;
  else
    *strp = strp_tmp;
  return r;
#elif defined(_MSC_VER)
  /* On Windows, _vsnprintf won't tell us the length of the string if it
   * overflows, so we need to use _vcsprintf to tell how much to allocate */
  int len, r;
  char *res;
  len = _vscprintf(fmt, args);
  if (len < 0) {
    *strp = NULL;
    return -1;
  }
  strp_tmp = xmalloc(len + 1);
  r = _vsnprintf(strp_tmp, len+1, fmt, args);
  if (r != len) {
    free(strp_tmp);
    *strp = NULL;
    return -1;
  }
  *strp = strp_tmp;
  return len;
#else
  /* Everywhere else, we have a decent vsnprintf that tells us how many
   * characters we need.  We give it a try on a short buffer first, since
   * it might be nice to avoid the second vsnprintf call.
   */
  char buf[128];
  int len, r;
  va_list tmp_args;
  va_copy(tmp_args, args);
  len = vsnprintf(buf, sizeof(buf), fmt, tmp_args);
  va_end(tmp_args);
  if (len < (int)sizeof(buf)) {
    *strp = xstrdup(buf);
    return len;
  }
  strp_tmp = xmalloc(len+1);
  r = vsnprintf(strp_tmp, len+1, fmt, args);
  if (r != len) {
    free(strp_tmp);
    *strp = NULL;
    return -1;
  }
  *strp = strp_tmp;
  return len;
#endif
}

/************************ Logging Subsystem *************************/
/** The code of this section was to a great extent shamelessly copied
    off tor. It's basicaly a stripped down version of tor's logging
    system. Thank you tor. */

/* Note: obfs_assert and obfs_abort cannot be used anywhere in the
   logging system, as they will recurse into the logging system and
   cause an infinite loop.  We use plain old abort(3) instead. */

/* Size of maximum log entry, including newline and NULL byte. */
#define MAX_LOG_ENTRY 1024
/* String to append when a log entry doesn't fit in MAX_LOG_ENTRY. */
#define TRUNCATED_STR "[...truncated]"
/* strlen(TRUNCATED_STR) */
#define TRUNCATED_STR_LEN 14

/** Logging severities */

#define LOG_SEV_ERR     5
#define LOG_SEV_WARN    4
#define LOG_SEV_NOTICE  3
#define LOG_SEV_INFO    2
#define LOG_SEV_DEBUG   1

/* logging method */
static int logging_method=LOG_METHOD_STDERR;
/* minimum logging severity */
static int logging_min_sev=LOG_SEV_NOTICE;
/* logfile fd */
static int logging_logfile=-1;

/** Helper: map a log severity to descriptive string. */
static const char *
sev_to_string(int severity)
{
  switch (severity) {
  case LOG_SEV_ERR:     return "error";
  case LOG_SEV_WARN:    return "warn";
  case LOG_SEV_NOTICE:  return "notice";
  case LOG_SEV_INFO:    return "info";
  case LOG_SEV_DEBUG:   return "debug";
  default:
    abort();
  }
}

/** If 'string' is a valid log severity, return the corresponding
 * numeric value.  Otherwise, return -1. */
static int
string_to_sev(const char *string)
{
  if (!strcasecmp(string, "error"))
    return LOG_SEV_ERR;
  else if (!strcasecmp(string, "warn"))
    return LOG_SEV_WARN;
  else if (!strcasecmp(string, "notice"))
    return LOG_SEV_NOTICE;
  else if (!strcasecmp(string, "info"))
    return LOG_SEV_INFO;
  else if (!strcasecmp(string, "debug"))
    return LOG_SEV_DEBUG;
  else
    return -1;
}

/**
   Returns True if 'severity' is a valid obfsproxy logging severity.
   Otherwise, it returns False.
*/
static int
sev_is_valid(int severity)
{
  return (severity == LOG_SEV_ERR    ||
          severity == LOG_SEV_WARN   ||
          severity == LOG_SEV_NOTICE ||
          severity == LOG_SEV_INFO   ||
          severity == LOG_SEV_DEBUG);
}

/**
   Helper: Opens 'filename' and sets it as the obfsproxy logfile.
   On success it returns 0, on fail it returns -1.
*/
static int
open_and_set_obfsproxy_logfile(const char *filename)
{
  if (!filename)
    return -1;
  logging_logfile = open(filename,
                         O_WRONLY|O_CREAT|O_APPEND,
                         0644);
  if (logging_logfile < 0)
    return -1;
  return 0;
}

/**
   Closes the obfsproxy logfile if it exists.
   Ignores errors.
*/
void
close_obfsproxy_logfile(void)
{
  if (logging_logfile >= 0)
    close(logging_logfile);
}

/**
   Sets the global logging 'method' and also sets and open the logfile
   'filename' in case we want to log into a file.
   It returns 1 on success and -1 on fail.
*/
int
log_set_method(int method, const char *filename)
{
  if (method == LOG_METHOD_FILE) {
    if (open_and_set_obfsproxy_logfile(filename) < 0)
      return -1;
  }

  logging_method = method;

  return 0;
}

/**
   Sets the minimum logging severity of obfsproxy to the severity
   described by 'sev_string', then it returns 0.  If 'sev_string' is
   not a valid severity, it returns -1.
*/
int
log_set_min_severity(const char* sev_string)
{
  int severity = string_to_sev(sev_string);
  if (!sev_is_valid(severity)) {
    log_warn("Severity '%s' makes no sense.", sev_string);
    return -1;
  }
  logging_min_sev = severity;
  return 0;
}

/** True if the minimum log severity is "debug".  Used in a few places
    to avoid some expensive formatting work if we are going to ignore the
    result. */
int
log_do_debug(void)
{
  return logging_min_sev == LOG_SEV_DEBUG;
}

/**
    Logging worker function.
    Accepts a logging 'severity' and a 'format' string and logs the
    message in 'format' according to the configured obfsproxy minimum
    logging severity and logging method.

    Beware! Because die_oom() calls this function, we must avoid mallocing
    any memory here, or we could get into an infinite loop. I believe it
    is currently the case that we don't malloc anything. -RD
*/
static void
logv(int severity, const char *format, va_list ap)
{
  if (!sev_is_valid(severity))
    abort();

  if (logging_method == LOG_METHOD_NULL)
    return;

  /* See if the user is interested in this log message. */
  if (severity < logging_min_sev)
    return;

  size_t n=0;
  int r=0;
  char buf[MAX_LOG_ENTRY];

  time_t now = time(NULL);
  struct tm *nowtm = localtime(&now);
  char ts[ISO_TIME_LEN + 2];
  size_t buflen = MAX_LOG_ENTRY-2;

  if (strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S ", nowtm) !=
      ISO_TIME_LEN + 1)
    ts[0] = '\0';
  r = obfs_snprintf(buf, buflen, "%s[%s] ", ts,
                    sev_to_string(severity));
  if (r < 0)
    n = strlen(buf);
  else
    n=r;

  r = obfs_vsnprintf(buf+n, buflen-n, format, ap);
  if (r < 0) {
    if (buflen >= TRUNCATED_STR_LEN) {
      size_t offset = buflen-TRUNCATED_STR_LEN;
      r = obfs_snprintf(buf+offset, TRUNCATED_STR_LEN+1,
                        "%s", TRUNCATED_STR);
      if (r < 0)
        abort();
    }
    n = buflen;
  } else
    n+=r;

  buf[n]='\n';
  buf[n+1]='\0';

  if (logging_method == LOG_METHOD_STDERR)
    fprintf(stderr, "%s", buf);
  else if (logging_method == LOG_METHOD_FILE) {
    assert(logging_logfile >= 0);
    assert(!(write(logging_logfile, buf, strlen(buf)) < 0));
  } else
    assert(0);
}

/**** Public logging API. ****/

/** Public function for logging an error and then exiting. */
void
log_error(const char *format, ...)
{
  va_list ap;
  va_start(ap,format);

  logv(LOG_SEV_ERR, format, ap);

  va_end(ap);
  exit(1);
}

/** Public function for logging an error message then aborting. */
void
log_error_abort(const char *format, ...)
{
  va_list ap;
  va_start(ap,format);

  logv(LOG_SEV_ERR, format, ap);

  va_end(ap);
  abort();
}

/** Public function for logging a warning. */
void
log_warn(const char *format, ...)
{
  va_list ap;
  va_start(ap,format);

  logv(LOG_SEV_WARN, format, ap);

  va_end(ap);
}

/** Public function for logging a notice message. */
void
log_notice(const char *format, ...)
{
  va_list ap;
  va_start(ap,format);

  logv(LOG_SEV_NOTICE, format, ap);

  va_end(ap);
}

/** Public function for logging an informative message. */
void
log_info(const char *format, ...)
{
  va_list ap;
  va_start(ap,format);

  logv(LOG_SEV_INFO, format, ap);

  va_end(ap);
}

/** Public function for logging a debugging message. */
void
log_debug(const char *format, ...)
{
  va_list ap;
  va_start(ap,format);

  logv(LOG_SEV_DEBUG, format, ap);

  va_end(ap);
}

/** Public function for logging a debugging message, given a format
    string in <b>format</b>, and a va_list <b>ap</b>. */
void
log_debug_raw(const char *format, va_list ap)
{
  logv(LOG_SEV_DEBUG, format, ap);
}
