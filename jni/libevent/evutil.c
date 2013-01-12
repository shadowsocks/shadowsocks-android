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

#include "event2/event-config.h"

#define _GNU_SOURCE

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#include <io.h>
#include <tchar.h>
#endif

#include <sys/types.h>
#ifdef _EVENT_HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef _EVENT_HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef _EVENT_HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef _EVENT_HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#ifdef _EVENT_HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef _EVENT_HAVE_NETINET_IN6_H
#include <netinet/in6.h>
#endif
#ifdef _EVENT_HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifndef _EVENT_HAVE_GETTIMEOFDAY
#include <sys/timeb.h>
#include <time.h>
#endif
#include <sys/stat.h>

#include "event2/util.h"
#include "util-internal.h"
#include "log-internal.h"
#include "mm-internal.h"

#include "strlcpy-internal.h"
#include "ipv6-internal.h"

#ifdef WIN32
#define open _open
#define read _read
#define close _close
#define fstat _fstati64
#define stat _stati64
#define mode_t int
#endif

int
evutil_open_closeonexec(const char *pathname, int flags, unsigned mode)
{
	int fd;

#ifdef O_CLOEXEC
	flags |= O_CLOEXEC;
#endif

	if (flags & O_CREAT)
		fd = open(pathname, flags, (mode_t)mode);
	else
		fd = open(pathname, flags);
	if (fd < 0)
		return -1;

#if !defined(O_CLOEXEC) && defined(FD_CLOEXEC)
	if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0)
		return -1;
#endif

	return fd;
}

/**
   Read the contents of 'filename' into a newly allocated NUL-terminated
   string.  Set *content_out to hold this string, and *len_out to hold its
   length (not including the appended NUL).  If 'is_binary', open the file in
   binary mode.

   Returns 0 on success, -1 if the open fails, and -2 for all other failures.

   Used internally only; may go away in a future version.
 */
int
evutil_read_file(const char *filename, char **content_out, size_t *len_out,
    int is_binary)
{
	int fd, r;
	struct stat st;
	char *mem;
	size_t read_so_far=0;
	int mode = O_RDONLY;

	EVUTIL_ASSERT(content_out);
	EVUTIL_ASSERT(len_out);
	*content_out = NULL;
	*len_out = 0;

#ifdef O_BINARY
	if (is_binary)
		mode |= O_BINARY;
#endif

	fd = evutil_open_closeonexec(filename, mode, 0);
	if (fd < 0)
		return -1;
	if (fstat(fd, &st) || st.st_size < 0 ||
	    st.st_size > EV_SSIZE_MAX-1 ) {
		close(fd);
		return -2;
	}
	mem = mm_malloc((size_t)st.st_size + 1);
	if (!mem) {
		close(fd);
		return -2;
	}
	read_so_far = 0;
#ifdef WIN32
#define N_TO_READ(x) ((x) > INT_MAX) ? INT_MAX : ((int)(x))
#else
#define N_TO_READ(x) (x)
#endif
	while ((r = read(fd, mem+read_so_far, N_TO_READ(st.st_size - read_so_far))) > 0) {
		read_so_far += r;
		if (read_so_far >= (size_t)st.st_size)
			break;
		EVUTIL_ASSERT(read_so_far < (size_t)st.st_size);
	}
	close(fd);
	if (r < 0) {
		mm_free(mem);
		return -2;
	}
	mem[read_so_far] = 0;

	*len_out = read_so_far;
	*content_out = mem;
	return 0;
}

int
evutil_socketpair(int family, int type, int protocol, evutil_socket_t fd[2])
{
#ifndef WIN32
	return socketpair(family, type, protocol, fd);
#else
	return evutil_ersatz_socketpair(family, type, protocol, fd);
#endif
}

int
evutil_ersatz_socketpair(int family, int type, int protocol,
    evutil_socket_t fd[2])
{
	/* This code is originally from Tor.  Used with permission. */

	/* This socketpair does not work when localhost is down. So
	 * it's really not the same thing at all. But it's close enough
	 * for now, and really, when localhost is down sometimes, we
	 * have other problems too.
	 */
#ifdef WIN32
#define ERR(e) WSA##e
#else
#define ERR(e) e
#endif
	evutil_socket_t listener = -1;
	evutil_socket_t connector = -1;
	evutil_socket_t acceptor = -1;
	struct sockaddr_in listen_addr;
	struct sockaddr_in connect_addr;
	ev_socklen_t size;
	int saved_errno = -1;

	if (protocol
		|| (family != AF_INET
#ifdef AF_UNIX
		    && family != AF_UNIX
#endif
		)) {
		EVUTIL_SET_SOCKET_ERROR(ERR(EAFNOSUPPORT));
		return -1;
	}
	if (!fd) {
		EVUTIL_SET_SOCKET_ERROR(ERR(EINVAL));
		return -1;
	}

	listener = socket(AF_INET, type, 0);
	if (listener < 0)
		return -1;
	memset(&listen_addr, 0, sizeof(listen_addr));
	listen_addr.sin_family = AF_INET;
	listen_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	listen_addr.sin_port = 0;	/* kernel chooses port.	 */
	if (bind(listener, (struct sockaddr *) &listen_addr, sizeof (listen_addr))
		== -1)
		goto tidy_up_and_fail;
	if (listen(listener, 1) == -1)
		goto tidy_up_and_fail;

	connector = socket(AF_INET, type, 0);
	if (connector < 0)
		goto tidy_up_and_fail;
	/* We want to find out the port number to connect to.  */
	size = sizeof(connect_addr);
	if (getsockname(listener, (struct sockaddr *) &connect_addr, &size) == -1)
		goto tidy_up_and_fail;
	if (size != sizeof (connect_addr))
		goto abort_tidy_up_and_fail;
	if (connect(connector, (struct sockaddr *) &connect_addr,
				sizeof(connect_addr)) == -1)
		goto tidy_up_and_fail;

	size = sizeof(listen_addr);
	acceptor = accept(listener, (struct sockaddr *) &listen_addr, &size);
	if (acceptor < 0)
		goto tidy_up_and_fail;
	if (size != sizeof(listen_addr))
		goto abort_tidy_up_and_fail;
	evutil_closesocket(listener);
	/* Now check we are talking to ourself by matching port and host on the
	   two sockets.	 */
	if (getsockname(connector, (struct sockaddr *) &connect_addr, &size) == -1)
		goto tidy_up_and_fail;
	if (size != sizeof (connect_addr)
		|| listen_addr.sin_family != connect_addr.sin_family
		|| listen_addr.sin_addr.s_addr != connect_addr.sin_addr.s_addr
		|| listen_addr.sin_port != connect_addr.sin_port)
		goto abort_tidy_up_and_fail;
	fd[0] = connector;
	fd[1] = acceptor;

	return 0;

 abort_tidy_up_and_fail:
	saved_errno = ERR(ECONNABORTED);
 tidy_up_and_fail:
	if (saved_errno < 0)
		saved_errno = EVUTIL_SOCKET_ERROR();
	if (listener != -1)
		evutil_closesocket(listener);
	if (connector != -1)
		evutil_closesocket(connector);
	if (acceptor != -1)
		evutil_closesocket(acceptor);

	EVUTIL_SET_SOCKET_ERROR(saved_errno);
	return -1;
#undef ERR
}

int
evutil_make_socket_nonblocking(evutil_socket_t fd)
{
#ifdef WIN32
	{
		u_long nonblocking = 1;
		if (ioctlsocket(fd, FIONBIO, &nonblocking) == SOCKET_ERROR) {
			event_sock_warn(fd, "fcntl(%d, F_GETFL)", (int)fd);
			return -1;
		}
	}
#else
	{
		int flags;
		if ((flags = fcntl(fd, F_GETFL, NULL)) < 0) {
			event_warn("fcntl(%d, F_GETFL)", fd);
			return -1;
		}
		if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
			event_warn("fcntl(%d, F_SETFL)", fd);
			return -1;
		}
	}
#endif
	return 0;
}

int
evutil_make_listen_socket_reuseable(evutil_socket_t sock)
{
#ifndef WIN32
	int one = 1;
	/* REUSEADDR on Unix means, "don't hang on to this address after the
	 * listener is closed."  On Windows, though, it means "don't keep other
	 * processes from binding to this address while we're using it. */
	return setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void*) &one,
	    (ev_socklen_t)sizeof(one));
#else
	return 0;
#endif
}

int
evutil_make_socket_closeonexec(evutil_socket_t fd)
{
#if !defined(WIN32) && defined(_EVENT_HAVE_SETFD)
	int flags;
	if ((flags = fcntl(fd, F_GETFD, NULL)) < 0) {
		event_warn("fcntl(%d, F_GETFD)", fd);
		return -1;
	}
	if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1) {
		event_warn("fcntl(%d, F_SETFD)", fd);
		return -1;
	}
#endif
	return 0;
}

int
evutil_closesocket(evutil_socket_t sock)
{
#ifndef WIN32
	return close(sock);
#else
	return closesocket(sock);
#endif
}

ev_int64_t
evutil_strtoll(const char *s, char **endptr, int base)
{
#ifdef _EVENT_HAVE_STRTOLL
	return (ev_int64_t)strtoll(s, endptr, base);
#elif _EVENT_SIZEOF_LONG == 8
	return (ev_int64_t)strtol(s, endptr, base);
#elif defined(WIN32) && defined(_MSC_VER) && _MSC_VER < 1300
	/* XXXX on old versions of MS APIs, we only support base
	 * 10. */
	ev_int64_t r;
	if (base != 10)
		return 0;
	r = (ev_int64_t) _atoi64(s);
	while (isspace(*s))
		++s;
	if (*s == '-')
		++s;
	while (isdigit(*s))
		++s;
	if (endptr)
		*endptr = (char*) s;
	return r;
#elif defined(WIN32)
	return (ev_int64_t) _strtoi64(s, endptr, base);
#elif defined(_EVENT_SIZEOF_LONG_LONG) && _EVENT_SIZEOF_LONG_LONG == 8
	long long r;
	int n;
	if (base != 10 && base != 16)
		return 0;
	if (base == 10) {
		n = sscanf(s, "%lld", &r);
	} else {
		unsigned long long ru=0;
		n = sscanf(s, "%llx", &ru);
		if (ru > EV_INT64_MAX)
			return 0;
		r = (long long) ru;
	}
	if (n != 1)
		return 0;
	while (EVUTIL_ISSPACE(*s))
		++s;
	if (*s == '-')
		++s;
	if (base == 10) {
		while (EVUTIL_ISDIGIT(*s))
			++s;
	} else {
		while (EVUTIL_ISXDIGIT(*s))
			++s;
	}
	if (endptr)
		*endptr = (char*) s;
	return r;
#else
#error "I don't know how to parse 64-bit integers."
#endif
}

#ifndef _EVENT_HAVE_GETTIMEOFDAY
/* No gettimeofday; this muse be windows. */
int
evutil_gettimeofday(struct timeval *tv, struct timezone *tz)
{
	struct _timeb tb;

	if (tv == NULL)
		return -1;

	/* XXXX
	 * _ftime is not the greatest interface here; GetSystemTimeAsFileTime
	 * would give us better resolution, whereas something cobbled together
	 * with GetTickCount could maybe give us monotonic behavior.
	 *
	 * Either way, I think this value might be skewed to ignore the
	 * timezone, and just return local time.  That's not so good.
	 */
	_ftime(&tb);
	tv->tv_sec = (long) tb.time;
	tv->tv_usec = ((int) tb.millitm) * 1000;
	return 0;
}
#endif

#ifdef WIN32
int
evutil_socket_geterror(evutil_socket_t sock)
{
	int optval, optvallen=sizeof(optval);
	int err = WSAGetLastError();
	if (err == WSAEWOULDBLOCK && sock >= 0) {
		if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (void*)&optval,
					   &optvallen))
			return err;
		if (optval)
			return optval;
	}
	return err;
}
#endif

/* XXX we should use an enum here. */
/* 2 for connection refused, 1 for connected, 0 for not yet, -1 for error. */
int
evutil_socket_connect(evutil_socket_t *fd_ptr, struct sockaddr *sa, int socklen)
{
	int made_fd = 0;

	if (*fd_ptr < 0) {
		if ((*fd_ptr = socket(sa->sa_family, SOCK_STREAM, 0)) < 0)
			goto err;
		made_fd = 1;
		if (evutil_make_socket_nonblocking(*fd_ptr) < 0) {
			goto err;
		}
	}

	if (connect(*fd_ptr, sa, socklen) < 0) {
		int e = evutil_socket_geterror(*fd_ptr);
		if (EVUTIL_ERR_CONNECT_RETRIABLE(e))
			return 0;
		if (EVUTIL_ERR_CONNECT_REFUSED(e))
			return 2;
		goto err;
	} else {
		return 1;
	}

err:
	if (made_fd) {
		evutil_closesocket(*fd_ptr);
		*fd_ptr = -1;
	}
	return -1;
}

/* Check whether a socket on which we called connect() is done
   connecting. Return 1 for connected, 0 for not yet, -1 for error.  In the
   error case, set the current socket errno to the error that happened during
   the connect operation. */
int
evutil_socket_finished_connecting(evutil_socket_t fd)
{
	int e;
	ev_socklen_t elen = sizeof(e);

	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (void*)&e, &elen) < 0)
		return -1;

	if (e) {
		if (EVUTIL_ERR_CONNECT_RETRIABLE(e))
			return 0;
		EVUTIL_SET_SOCKET_ERROR(e);
		return -1;
	}

	return 1;
}

#if (EVUTIL_AI_PASSIVE|EVUTIL_AI_CANONNAME|EVUTIL_AI_NUMERICHOST| \
     EVUTIL_AI_NUMERICSERV|EVUTIL_AI_V4MAPPED|EVUTIL_AI_ALL| \
     EVUTIL_AI_ADDRCONFIG) != \
    (EVUTIL_AI_PASSIVE^EVUTIL_AI_CANONNAME^EVUTIL_AI_NUMERICHOST^ \
     EVUTIL_AI_NUMERICSERV^EVUTIL_AI_V4MAPPED^EVUTIL_AI_ALL^ \
     EVUTIL_AI_ADDRCONFIG)
#error "Some of our EVUTIL_AI_* flags seem to overlap with system AI_* flags"
#endif

/* We sometimes need to know whether we have an ipv4 address and whether we
   have an ipv6 address. If 'have_checked_interfaces', then we've already done
   the test.  If 'had_ipv4_address', then it turns out we had an ipv4 address.
   If 'had_ipv6_address', then it turns out we had an ipv6 address.   These are
   set by evutil_check_interfaces. */
static int have_checked_interfaces, had_ipv4_address, had_ipv6_address;

/* Macro: True iff the IPv4 address 'addr', in host order, is in 127.0.0.0/8
 */
#define EVUTIL_V4ADDR_IS_LOCALHOST(addr) (((addr)>>24) == 127)

/* Macro: True iff the IPv4 address 'addr', in host order, is a class D
 * (multiclass) address.
 */
#define EVUTIL_V4ADDR_IS_CLASSD(addr) ((((addr)>>24) & 0xf0) == 0xe0)

/* Test whether we have an ipv4 interface and an ipv6 interface.  Return 0 if
 * the test seemed successful. */
static int
evutil_check_interfaces(int force_recheck)
{
	const char ZEROES[] = "\x00\x00\x00\x00\x00\x00\x00\x00"
	    "\x00\x00\x00\x00\x00\x00\x00\x00";
	evutil_socket_t fd = -1;
	struct sockaddr_in sin, sin_out;
	struct sockaddr_in6 sin6, sin6_out;
	ev_socklen_t sin_out_len = sizeof(sin_out);
	ev_socklen_t sin6_out_len = sizeof(sin6_out);
	int r;
	char buf[128];
	if (have_checked_interfaces && !force_recheck)
		return 0;

	/* To check whether we have an interface open for a given protocol, we
	 * try to make a UDP 'connection' to a remote host on the internet.
	 * We don't actually use it, so the address doesn't matter, but we
	 * want to pick one that keep us from using a host- or link-local
	 * interface. */
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(53);
	r = evutil_inet_pton(AF_INET, "18.244.0.188", &sin.sin_addr);
	EVUTIL_ASSERT(r);

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	sin6.sin6_port = htons(53);
	r = evutil_inet_pton(AF_INET6, "2001:4860:b002::68", &sin6.sin6_addr);
	EVUTIL_ASSERT(r);

	memset(&sin_out, 0, sizeof(sin_out));
	memset(&sin6_out, 0, sizeof(sin6_out));

	/* XXX some errnos mean 'no address'; some mean 'not enough sockets'. */
	if ((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) >= 0 &&
	    connect(fd, (struct sockaddr*)&sin, sizeof(sin)) == 0 &&
	    getsockname(fd, (struct sockaddr*)&sin_out, &sin_out_len) == 0) {
		/* We might have an IPv4 interface. */
		ev_uint32_t addr = ntohl(sin_out.sin_addr.s_addr);
		if (addr == 0 ||
		    EVUTIL_V4ADDR_IS_LOCALHOST(addr) ||
		    EVUTIL_V4ADDR_IS_CLASSD(addr)) {
			evutil_inet_ntop(AF_INET, &sin_out.sin_addr,
			    buf, sizeof(buf));
			/* This is a reserved, ipv4compat, ipv4map, loopback,
			 * link-local or unspecified address.  The host should
			 * never have given it to us; it could never connect
			 * to sin. */
			event_warnx("Got a strange local ipv4 address %s",buf);
		} else {
			event_debug(("Detected an IPv4 interface"));
			had_ipv4_address = 1;
		}
	}
	if (fd >= 0)
		evutil_closesocket(fd);

	if ((fd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP)) >= 0 &&
	    connect(fd, (struct sockaddr*)&sin6, sizeof(sin6)) == 0 &&
	    getsockname(fd, (struct sockaddr*)&sin6_out, &sin6_out_len) == 0) {
		/* We might have an IPv6 interface. */
		const unsigned char *addr =
		    (unsigned char*)sin6_out.sin6_addr.s6_addr;
		if (!memcmp(addr, ZEROES, 8) ||
		    (addr[0] == 0xfe && (addr[1] & 0xc0) == 0x80)) {
			/* This is a reserved, ipv4compat, ipv4map, loopback,
			 * link-local or unspecified address.  The host should
			 * never have given it to us; it could never connect
			 * to sin6. */
			evutil_inet_ntop(AF_INET6, &sin6_out.sin6_addr,
			    buf, sizeof(buf));
			event_warnx("Got a strange local ipv6 address %s",buf);
		} else {
			event_debug(("Detected an IPv4 interface"));
			had_ipv6_address = 1;
		}
	}

	if (fd >= 0)
		evutil_closesocket(fd);

	return 0;
}

/* Internal addrinfo flag.  This one is set when we allocate the addrinfo from
 * inside libevent.  Otherwise, the built-in getaddrinfo() function allocated
 * it, and we should trust what they said.
 **/
#define EVUTIL_AI_LIBEVENT_ALLOCATED 0x80000000

/* Helper: construct a new addrinfo containing the socket address in
 * 'sa', which must be a sockaddr_in or a sockaddr_in6.  Take the
 * socktype and protocol info from hints.  If they weren't set, then
 * allocate both a TCP and a UDP addrinfo.
 */
struct evutil_addrinfo *
evutil_new_addrinfo(struct sockaddr *sa, ev_socklen_t socklen,
    const struct evutil_addrinfo *hints)
{
	struct evutil_addrinfo *res;
	EVUTIL_ASSERT(hints);

	if (hints->ai_socktype == 0 && hints->ai_protocol == 0) {
		/* Indecisive user! Give them a UDP and a TCP. */
		struct evutil_addrinfo *r1, *r2;
		struct evutil_addrinfo tmp;
		memcpy(&tmp, hints, sizeof(tmp));
		tmp.ai_socktype = SOCK_STREAM; tmp.ai_protocol = IPPROTO_TCP;
		r1 = evutil_new_addrinfo(sa, socklen, &tmp);
		if (!r1)
			return NULL;
		tmp.ai_socktype = SOCK_DGRAM; tmp.ai_protocol = IPPROTO_UDP;
		r2 = evutil_new_addrinfo(sa, socklen, &tmp);
		if (!r2) {
			evutil_freeaddrinfo(r1);
			return NULL;
		}
		r1->ai_next = r2;
		return r1;
	}

	/* We're going to allocate extra space to hold the sockaddr. */
	res = mm_calloc(1,sizeof(struct evutil_addrinfo)+socklen);
	if (!res)
		return NULL;
	res->ai_addr = (struct sockaddr*)
	    (((char*)res) + sizeof(struct evutil_addrinfo));
	memcpy(res->ai_addr, sa, socklen);
	res->ai_addrlen = socklen;
	res->ai_family = sa->sa_family; /* Same or not? XXX */
	res->ai_flags = EVUTIL_AI_LIBEVENT_ALLOCATED;
	res->ai_socktype = hints->ai_socktype;
	res->ai_protocol = hints->ai_protocol;

	return res;
}

/* Append the addrinfo 'append' to the end of 'first', and return the start of
 * the list.  Either element can be NULL, in which case we return the element
 * that is not NULL. */
struct evutil_addrinfo *
evutil_addrinfo_append(struct evutil_addrinfo *first,
    struct evutil_addrinfo *append)
{
	struct evutil_addrinfo *ai = first;
	if (!ai)
		return append;
	while (ai->ai_next)
		ai = ai->ai_next;
	ai->ai_next = append;

	return first;
}

static int
parse_numeric_servname(const char *servname)
{
	int n;
	char *endptr=NULL;
	n = (int) strtol(servname, &endptr, 10);
	if (n>=0 && n <= 65535 && servname[0] && endptr && !endptr[0])
		return n;
	else
		return -1;
}

/** Parse a service name in 'servname', which can be a decimal port.
 * Return the port number, or -1 on error.
 */
static int
evutil_parse_servname(const char *servname, const char *protocol,
    const struct evutil_addrinfo *hints)
{
	int n = parse_numeric_servname(servname);
	if (n>=0)
		return n;
#if defined(_EVENT_HAVE_GETSERVBYNAME) || defined(WIN32)
	if (!(hints->ai_flags & EVUTIL_AI_NUMERICSERV)) {
		struct servent *ent = getservbyname(servname, protocol);
		if (ent) {
			return ntohs(ent->s_port);
		}
	}
#endif
	return -1;
}

/* Return a string corresponding to a protocol number that we can pass to
 * getservyname.  */
static const char *
evutil_unparse_protoname(int proto)
{
	switch (proto) {
	case 0:
		return NULL;
	case IPPROTO_TCP:
		return "tcp";
	case IPPROTO_UDP:
		return "udp";
#ifdef IPPROTO_SCTP
	case IPPROTO_SCTP:
		return "sctp";
#endif
	default:
#ifdef _EVENT_HAVE_GETPROTOBYNUMBER
		{
			struct protoent *ent = getprotobynumber(proto);
			if (ent)
				return ent->p_name;
		}
#endif
		return NULL;
	}
}

static void
evutil_getaddrinfo_infer_protocols(struct evutil_addrinfo *hints)
{
	/* If we can guess the protocol from the socktype, do so. */
	if (!hints->ai_protocol && hints->ai_socktype) {
		if (hints->ai_socktype == SOCK_DGRAM)
			hints->ai_protocol = IPPROTO_UDP;
		else if (hints->ai_socktype == SOCK_STREAM)
			hints->ai_protocol = IPPROTO_TCP;
	}

	/* Set the socktype if it isn't set. */
	if (!hints->ai_socktype && hints->ai_protocol) {
		if (hints->ai_protocol == IPPROTO_UDP)
			hints->ai_socktype = SOCK_DGRAM;
		else if (hints->ai_protocol == IPPROTO_TCP)
			hints->ai_socktype = SOCK_STREAM;
#ifdef IPPROTO_SCTP
		else if (hints->ai_protocol == IPPROTO_SCTP)
			hints->ai_socktype = SOCK_STREAM;
#endif
	}
}

#if AF_UNSPEC != PF_UNSPEC
#error "I cannot build on a system where AF_UNSPEC != PF_UNSPEC"
#endif

/** Implements the part of looking up hosts by name that's common to both
 * the blocking and nonblocking resolver:
 *   - Adjust 'hints' to have a reasonable socktype and protocol.
 *   - Look up the port based on 'servname', and store it in *portnum,
 *   - Handle the nodename==NULL case
 *   - Handle some invalid arguments cases.
 *   - Handle the cases where nodename is an IPv4 or IPv6 address.
 *
 * If we need the resolver to look up the hostname, we return
 * EVUTIL_EAI_NEED_RESOLVE.  Otherwise, we can completely implement
 * getaddrinfo: we return 0 or an appropriate EVUTIL_EAI_* error, and
 * set *res as getaddrinfo would.
 */
int
evutil_getaddrinfo_common(const char *nodename, const char *servname,
    struct evutil_addrinfo *hints, struct evutil_addrinfo **res, int *portnum)
{
	int port = 0;
	const char *pname;

	if (nodename == NULL && servname == NULL)
		return EVUTIL_EAI_NONAME;

	/* We only understand 3 families */
	if (hints->ai_family != PF_UNSPEC && hints->ai_family != PF_INET &&
	    hints->ai_family != PF_INET6)
		return EVUTIL_EAI_FAMILY;

	evutil_getaddrinfo_infer_protocols(hints);

	/* Look up the port number and protocol, if possible. */
	pname = evutil_unparse_protoname(hints->ai_protocol);
	if (servname) {
		/* XXXX We could look at the protocol we got back from
		 * getservbyname, but it doesn't seem too useful. */
		port = evutil_parse_servname(servname, pname, hints);
		if (port < 0) {
			return EVUTIL_EAI_NONAME;
		}
	}

	/* If we have no node name, then we're supposed to bind to 'any' and
	 * connect to localhost. */
	if (nodename == NULL) {
		struct evutil_addrinfo *res4=NULL, *res6=NULL;
		if (hints->ai_family != PF_INET) { /* INET6 or UNSPEC. */
			struct sockaddr_in6 sin6;
			memset(&sin6, 0, sizeof(sin6));
			sin6.sin6_family = AF_INET6;
			sin6.sin6_port = htons(port);
			if (hints->ai_flags & EVUTIL_AI_PASSIVE) {
				/* Bind to :: */
			} else {
				/* connect to ::1 */
				sin6.sin6_addr.s6_addr[15] = 1;
			}
			res6 = evutil_new_addrinfo((struct sockaddr*)&sin6,
			    sizeof(sin6), hints);
			if (!res6)
				return EVUTIL_EAI_MEMORY;
		}

		if (hints->ai_family != PF_INET6) { /* INET or UNSPEC */
			struct sockaddr_in sin;
			memset(&sin, 0, sizeof(sin));
			sin.sin_family = AF_INET;
			sin.sin_port = htons(port);
			if (hints->ai_flags & EVUTIL_AI_PASSIVE) {
				/* Bind to 0.0.0.0 */
			} else {
				/* connect to 127.0.0.1 */
				sin.sin_addr.s_addr = htonl(0x7f000001);
			}
			res4 = evutil_new_addrinfo((struct sockaddr*)&sin,
			    sizeof(sin), hints);
			if (!res4) {
				if (res6)
					evutil_freeaddrinfo(res6);
				return EVUTIL_EAI_MEMORY;
			}
		}
		*res = evutil_addrinfo_append(res4, res6);
		return 0;
	}

	/* If we can, we should try to parse the hostname without resolving
	 * it. */
	/* Try ipv6. */
	if (hints->ai_family == PF_INET6 || hints->ai_family == PF_UNSPEC) {
		struct sockaddr_in6 sin6;
		memset(&sin6, 0, sizeof(sin6));
		if (1==evutil_inet_pton(AF_INET6, nodename, &sin6.sin6_addr)) {
			/* Got an ipv6 address. */
			sin6.sin6_family = AF_INET6;
			sin6.sin6_port = htons(port);
			*res = evutil_new_addrinfo((struct sockaddr*)&sin6,
			    sizeof(sin6), hints);
			if (!*res)
				return EVUTIL_EAI_MEMORY;
			return 0;
		}
	}

	/* Try ipv4. */
	if (hints->ai_family == PF_INET || hints->ai_family == PF_UNSPEC) {
		struct sockaddr_in sin;
		memset(&sin, 0, sizeof(sin));
		if (1==evutil_inet_pton(AF_INET, nodename, &sin.sin_addr)) {
			/* Got an ipv6 address. */
			sin.sin_family = AF_INET;
			sin.sin_port = htons(port);
			*res = evutil_new_addrinfo((struct sockaddr*)&sin,
			    sizeof(sin), hints);
			if (!*res)
				return EVUTIL_EAI_MEMORY;
			return 0;
		}
	}


	/* If we have reached this point, we definitely need to do a DNS
	 * lookup. */
	if ((hints->ai_flags & EVUTIL_AI_NUMERICHOST)) {
		/* If we're not allowed to do one, then say so. */
		return EVUTIL_EAI_NONAME;
	}
	*portnum = port;
	return EVUTIL_EAI_NEED_RESOLVE;
}

#ifdef _EVENT_HAVE_GETADDRINFO
#define USE_NATIVE_GETADDRINFO
#endif

#ifdef USE_NATIVE_GETADDRINFO
/* A mask of all the flags that we declare, so we can clear them before calling
 * the native getaddrinfo */
static const unsigned int ALL_NONNATIVE_AI_FLAGS =
#ifndef AI_PASSIVE
    EVUTIL_AI_PASSIVE |
#endif
#ifndef AI_CANONNAME
    EVUTIL_AI_CANONNAME |
#endif
#ifndef AI_NUMERICHOST
    EVUTIL_AI_NUMERICHOST |
#endif
#ifndef AI_NUMERICSERV
    EVUTIL_AI_NUMERICSERV |
#endif
#ifndef AI_ADDRCONFIG
    EVUTIL_AI_ADDRCONFIG |
#endif
#ifndef AI_ALL
    EVUTIL_AI_ALL |
#endif
#ifndef AI_V4MAPPED
    EVUTIL_AI_V4MAPPED |
#endif
    EVUTIL_AI_LIBEVENT_ALLOCATED;

static const unsigned int ALL_NATIVE_AI_FLAGS =
#ifdef AI_PASSIVE
    AI_PASSIVE |
#endif
#ifdef AI_CANONNAME
    AI_CANONNAME |
#endif
#ifdef AI_NUMERICHOST
    AI_NUMERICHOST |
#endif
#ifdef AI_NUMERICSERV
    AI_NUMERICSERV |
#endif
#ifdef AI_ADDRCONFIG
    AI_ADDRCONFIG |
#endif
#ifdef AI_ALL
    AI_ALL |
#endif
#ifdef AI_V4MAPPED
    AI_V4MAPPED |
#endif
    0;
#endif

#ifndef USE_NATIVE_GETADDRINFO
/* Helper for systems with no getaddrinfo(): make one or more addrinfos out of
 * a struct hostent.
 */
static struct evutil_addrinfo *
addrinfo_from_hostent(const struct hostent *ent,
    int port, const struct evutil_addrinfo *hints)
{
	int i;
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	struct sockaddr *sa;
	int socklen;
	struct evutil_addrinfo *res=NULL, *ai;
	void *addrp;

	if (ent->h_addrtype == PF_INET) {
		memset(&sin, 0, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_port = htons(port);
		sa = (struct sockaddr *)&sin;
		socklen = sizeof(struct sockaddr_in);
		addrp = &sin.sin_addr;
		if (ent->h_length != sizeof(sin.sin_addr)) {
			event_warnx("Weird h_length from gethostbyname");
			return NULL;
		}
	} else if (ent->h_addrtype == PF_INET6) {
		memset(&sin6, 0, sizeof(sin6));
		sin6.sin6_family = AF_INET6;
		sin6.sin6_port = htons(port);
		sa = (struct sockaddr *)&sin6;
		socklen = sizeof(struct sockaddr_in);
		addrp = &sin6.sin6_addr;
		if (ent->h_length != sizeof(sin6.sin6_addr)) {
			event_warnx("Weird h_length from gethostbyname");
			return NULL;
		}
	} else
		return NULL;

	for (i = 0; ent->h_addr_list[i]; ++i) {
		memcpy(addrp, ent->h_addr_list[i], ent->h_length);
		ai = evutil_new_addrinfo(sa, socklen, hints);
		if (!ai) {
			evutil_freeaddrinfo(res);
			return NULL;
		}
		res = evutil_addrinfo_append(res, ai);
	}

	if (res && ((hints->ai_flags & EVUTIL_AI_CANONNAME) && ent->h_name)) {
		res->ai_canonname = mm_strdup(ent->h_name);
		if (res->ai_canonname == NULL) {
			evutil_freeaddrinfo(res);
			return NULL;
		}
	}

	return res;
}
#endif

/* If the EVUTIL_AI_ADDRCONFIG flag is set on hints->ai_flags, and
 * hints->ai_family is PF_UNSPEC, then revise the value of hints->ai_family so
 * that we'll only get addresses we could maybe connect to.
 */
void
evutil_adjust_hints_for_addrconfig(struct evutil_addrinfo *hints)
{
	if (!(hints->ai_flags & EVUTIL_AI_ADDRCONFIG))
		return;
	if (hints->ai_family != PF_UNSPEC)
		return;
	if (!have_checked_interfaces)
		evutil_check_interfaces(0);
	if (had_ipv4_address && !had_ipv6_address) {
		hints->ai_family = PF_INET;
	} else if (!had_ipv4_address && had_ipv6_address) {
		hints->ai_family = PF_INET6;
	}
}

#ifdef USE_NATIVE_GETADDRINFO
static int need_numeric_port_hack_=0;
static int need_socktype_protocol_hack_=0;
static int tested_for_getaddrinfo_hacks=0;

/* Some older BSDs (like OpenBSD up to 4.6) used to believe that
   giving a numeric port without giving an ai_socktype was verboten.
   We test for this so we can apply an appropriate workaround.  If it
   turns out that the bug is present, then:

    - If nodename==NULL and servname is numeric, we build an answer
      ourselves using evutil_getaddrinfo_common().

    - If nodename!=NULL and servname is numeric, then we set
      servname=NULL when calling getaddrinfo, and post-process the
      result to set the ports on it.

   We test for this bug at runtime, since otherwise we can't have the
   same binary run on multiple BSD versions.

   - Some versions of Solaris believe that it's nice to leave to protocol
     field set to 0.  We test for this so we can apply an appropriate
     workaround.
*/
static void
test_for_getaddrinfo_hacks(void)
{
	int r, r2;
	struct evutil_addrinfo *ai=NULL, *ai2=NULL;
	struct evutil_addrinfo hints;

	memset(&hints,0,sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_flags =
#ifdef AI_NUMERICHOST
	    AI_NUMERICHOST |
#endif
#ifdef AI_NUMERICSERV
	    AI_NUMERICSERV |
#endif
	    0;
	r = getaddrinfo("1.2.3.4", "80", &hints, &ai);
	hints.ai_socktype = SOCK_STREAM;
	r2 = getaddrinfo("1.2.3.4", "80", &hints, &ai2);
	if (r2 == 0 && r != 0) {
		need_numeric_port_hack_=1;
	}
	if (ai2 && ai2->ai_protocol == 0) {
		need_socktype_protocol_hack_=1;
	}

	if (ai)
		freeaddrinfo(ai);
	if (ai2)
		freeaddrinfo(ai2);
	tested_for_getaddrinfo_hacks=1;
}

static inline int
need_numeric_port_hack(void)
{
	if (!tested_for_getaddrinfo_hacks)
		test_for_getaddrinfo_hacks();
	return need_numeric_port_hack_;
}

static inline int
need_socktype_protocol_hack(void)
{
	if (!tested_for_getaddrinfo_hacks)
		test_for_getaddrinfo_hacks();
	return need_socktype_protocol_hack_;
}

static void
apply_numeric_port_hack(int port, struct evutil_addrinfo **ai)
{
	/* Now we run through the list and set the ports on all of the
	 * results where ports would make sense. */
	for ( ; *ai; ai = &(*ai)->ai_next) {
		struct sockaddr *sa = (*ai)->ai_addr;
		if (sa && sa->sa_family == AF_INET) {
			struct sockaddr_in *sin = (struct sockaddr_in*)sa;
			sin->sin_port = htons(port);
		} else if (sa && sa->sa_family == AF_INET6) {
			struct sockaddr_in6 *sin6 = (struct sockaddr_in6*)sa;
			sin6->sin6_port = htons(port);
		} else {
			/* A numeric port makes no sense here; remove this one
			 * from the list. */
			struct evutil_addrinfo *victim = *ai;
			*ai = victim->ai_next;
			victim->ai_next = NULL;
			freeaddrinfo(victim);
		}
	}
}

static int
apply_socktype_protocol_hack(struct evutil_addrinfo *ai)
{
	struct evutil_addrinfo *ai_new;
	for (; ai; ai = ai->ai_next) {
		evutil_getaddrinfo_infer_protocols(ai);
		if (ai->ai_socktype || ai->ai_protocol)
			continue;
		ai_new = mm_malloc(sizeof(*ai_new));
		if (!ai_new)
			return -1;
		memcpy(ai_new, ai, sizeof(*ai_new));
		ai->ai_socktype = SOCK_STREAM;
		ai->ai_protocol = IPPROTO_TCP;
		ai_new->ai_socktype = SOCK_DGRAM;
		ai_new->ai_protocol = IPPROTO_UDP;

		ai_new->ai_next = ai->ai_next;
		ai->ai_next = ai_new;
	}
	return 0;
}
#endif

int
evutil_getaddrinfo(const char *nodename, const char *servname,
    const struct evutil_addrinfo *hints_in, struct evutil_addrinfo **res)
{
#ifdef USE_NATIVE_GETADDRINFO
	struct evutil_addrinfo hints;
	int portnum=-1, need_np_hack, err;

	if (hints_in) {
		memcpy(&hints, hints_in, sizeof(hints));
	} else {
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = PF_UNSPEC;
	}

#ifndef AI_ADDRCONFIG
	/* Not every system has AI_ADDRCONFIG, so fake it. */
	if (hints.ai_family == PF_UNSPEC &&
	    (hints.ai_flags & EVUTIL_AI_ADDRCONFIG)) {
		evutil_adjust_hints_for_addrconfig(&hints);
	}
#endif

#ifndef AI_NUMERICSERV
	/* Not every system has AI_NUMERICSERV, so fake it. */
	if (hints.ai_flags & EVUTIL_AI_NUMERICSERV) {
		if (servname && parse_numeric_servname(servname)<0)
			return EVUTIL_EAI_NONAME;
	}
#endif

	/* Enough operating systems handle enough common non-resolve
	 * cases here weirdly enough that we are better off just
	 * overriding them.  For example:
	 *
	 * - Windows doesn't like to infer the protocol from the
	 *   socket type, or fill in socket or protocol types much at
	 *   all.  It also seems to do its own broken implicit
	 *   always-on version of AI_ADDRCONFIG that keeps it from
	 *   ever resolving even a literal IPv6 address when
	 *   ai_addrtype is PF_UNSPEC.
	 */
#ifdef WIN32
	{
		int tmp_port;
		err = evutil_getaddrinfo_common(nodename,servname,&hints,
		    res, &tmp_port);
		if (err == 0 ||
		    err == EVUTIL_EAI_MEMORY ||
		    err == EVUTIL_EAI_NONAME)
			return err;
		/* If we make it here, the system getaddrinfo can
		 * have a crack at it. */
	}
#endif

	/* See documentation for need_numeric_port_hack above.*/
	need_np_hack = need_numeric_port_hack() && servname && !hints.ai_socktype
	    && ((portnum=parse_numeric_servname(servname)) >= 0);
	if (need_np_hack) {
		if (!nodename)
			return evutil_getaddrinfo_common(
				NULL,servname,&hints, res, &portnum);
		servname = NULL;
	}

	if (need_socktype_protocol_hack()) {
		evutil_getaddrinfo_infer_protocols(&hints);
	}

	/* Make sure that we didn't actually steal any AI_FLAGS values that
	 * the system is using.  (This is a constant expression, and should ge
	 * optimized out.)
	 *
	 * XXXX Turn this into a compile-time failure rather than a run-time
	 * failure.
	 */
	EVUTIL_ASSERT((ALL_NONNATIVE_AI_FLAGS & ALL_NATIVE_AI_FLAGS) == 0);

	/* Clear any flags that only libevent understands. */
	hints.ai_flags &= ~ALL_NONNATIVE_AI_FLAGS;

	err = getaddrinfo(nodename, servname, &hints, res);
	if (need_np_hack)
		apply_numeric_port_hack(portnum, res);

	if (need_socktype_protocol_hack()) {
		if (apply_socktype_protocol_hack(*res) < 0) {
			evutil_freeaddrinfo(*res);
			*res = NULL;
			return EVUTIL_EAI_MEMORY;
		}
	}
	return err;
#else
	int port=0, err;
	struct hostent *ent = NULL;
	struct evutil_addrinfo hints;

	if (hints_in) {
		memcpy(&hints, hints_in, sizeof(hints));
	} else {
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = PF_UNSPEC;
	}

	evutil_adjust_hints_for_addrconfig(&hints);

	err = evutil_getaddrinfo_common(nodename, servname, &hints, res, &port);
	if (err != EVUTIL_EAI_NEED_RESOLVE) {
		/* We either succeeded or failed.  No need to continue */
		return err;
	}

	err = 0;
	/* Use any of the various gethostbyname_r variants as available. */
	{
#ifdef _EVENT_HAVE_GETHOSTBYNAME_R_6_ARG
		/* This one is what glibc provides. */
		char buf[2048];
		struct hostent hostent;
		int r;
		r = gethostbyname_r(nodename, &hostent, buf, sizeof(buf), &ent,
		    &err);
#elif defined(_EVENT_HAVE_GETHOSTBYNAME_R_5_ARG)
		char buf[2048];
		struct hostent hostent;
		ent = gethostbyname_r(nodename, &hostent, buf, sizeof(buf),
		    &err);
#elif defined(_EVENT_HAVE_GETHOSTBYNAME_R_3_ARG)
		struct hostent_data data;
		struct hostent hostent;
		memset(&data, 0, sizeof(data));
		err = gethostbyname_r(nodename, &hostent, &data);
		ent = err ? NULL : &hostent;
#else
		/* fall back to gethostbyname. */
		/* XXXX This needs a lock everywhere but Windows. */
		ent = gethostbyname(nodename);
#ifdef WIN32
		err = WSAGetLastError();
#else
		err = h_errno;
#endif
#endif

		/* Now we have either ent or err set. */
		if (!ent) {
			/* XXX is this right for windows ? */
			switch (err) {
			case TRY_AGAIN:
				return EVUTIL_EAI_AGAIN;
			case NO_RECOVERY:
			default:
				return EVUTIL_EAI_FAIL;
			case HOST_NOT_FOUND:
				return EVUTIL_EAI_NONAME;
			case NO_ADDRESS:
#if NO_DATA != NO_ADDRESS
			case NO_DATA:
#endif
				return EVUTIL_EAI_NODATA;
			}
		}

		if (ent->h_addrtype != hints.ai_family &&
		    hints.ai_family != PF_UNSPEC) {
			/* This wasn't the type we were hoping for.  Too bad
			 * we never had a chance to ask gethostbyname for what
			 * we wanted. */
			return EVUTIL_EAI_NONAME;
		}

		/* Make sure we got _some_ answers. */
		if (ent->h_length == 0)
			return EVUTIL_EAI_NODATA;

		/* If we got an address type we don't know how to make a
		   sockaddr for, give up. */
		if (ent->h_addrtype != PF_INET && ent->h_addrtype != PF_INET6)
			return EVUTIL_EAI_FAMILY;

		*res = addrinfo_from_hostent(ent, port, &hints);
		if (! *res)
			return EVUTIL_EAI_MEMORY;
	}

	return 0;
#endif
}

void
evutil_freeaddrinfo(struct evutil_addrinfo *ai)
{
#ifdef _EVENT_HAVE_GETADDRINFO
	if (!(ai->ai_flags & EVUTIL_AI_LIBEVENT_ALLOCATED)) {
		freeaddrinfo(ai);
		return;
	}
#endif
	while (ai) {
		struct evutil_addrinfo *next = ai->ai_next;
		if (ai->ai_canonname)
			mm_free(ai->ai_canonname);
		mm_free(ai);
		ai = next;
	}
}

static evdns_getaddrinfo_fn evdns_getaddrinfo_impl = NULL;

void
evutil_set_evdns_getaddrinfo_fn(evdns_getaddrinfo_fn fn)
{
	if (!evdns_getaddrinfo_impl)
		evdns_getaddrinfo_impl = fn;
}

/* Internal helper function: act like evdns_getaddrinfo if dns_base is set;
 * otherwise do a blocking resolve and pass the result to the callback in the
 * way that evdns_getaddrinfo would.
 */
int
evutil_getaddrinfo_async(struct evdns_base *dns_base,
    const char *nodename, const char *servname,
    const struct evutil_addrinfo *hints_in,
    void (*cb)(int, struct evutil_addrinfo *, void *), void *arg)
{
	if (dns_base && evdns_getaddrinfo_impl) {
		evdns_getaddrinfo_impl(
			dns_base, nodename, servname, hints_in, cb, arg);
	} else {
		struct evutil_addrinfo *ai=NULL;
		int err;
		err = evutil_getaddrinfo(nodename, servname, hints_in, &ai);
		cb(err, ai, arg);
	}
	return 0;
}

const char *
evutil_gai_strerror(int err)
{
	/* As a sneaky side-benefit, this case statement will get most
	 * compilers to tell us if any of the error codes we defined
	 * conflict with the platform's native error codes. */
	switch (err) {
	case EVUTIL_EAI_CANCEL:
		return "Request canceled";
	case 0:
		return "No error";

	case EVUTIL_EAI_ADDRFAMILY:
		return "address family for nodename not supported";
	case EVUTIL_EAI_AGAIN:
		return "temporary failure in name resolution";
	case EVUTIL_EAI_BADFLAGS:
		return "invalid value for ai_flags";
	case EVUTIL_EAI_FAIL:
		return "non-recoverable failure in name resolution";
	case EVUTIL_EAI_FAMILY:
		return "ai_family not supported";
	case EVUTIL_EAI_MEMORY:
		return "memory allocation failure";
	case EVUTIL_EAI_NODATA:
		return "no address associated with nodename";
	case EVUTIL_EAI_NONAME:
		return "nodename nor servname provided, or not known";
	case EVUTIL_EAI_SERVICE:
		return "servname not supported for ai_socktype";
	case EVUTIL_EAI_SOCKTYPE:
		return "ai_socktype not supported";
	case EVUTIL_EAI_SYSTEM:
		return "system error";
	default:
#if defined(USE_NATIVE_GETADDRINFO) && defined(WIN32)
		return gai_strerrorA(err);
#elif defined(USE_NATIVE_GETADDRINFO)
		return gai_strerror(err);
#else
		return "Unknown error code";
#endif
	}
}

#ifdef WIN32
#define E(code, s) { code, (s " [" #code " ]") }
static struct { int code; const char *msg; } windows_socket_errors[] = {
  E(WSAEINTR, "Interrupted function call"),
  E(WSAEACCES, "Permission denied"),
  E(WSAEFAULT, "Bad address"),
  E(WSAEINVAL, "Invalid argument"),
  E(WSAEMFILE, "Too many open files"),
  E(WSAEWOULDBLOCK,  "Resource temporarily unavailable"),
  E(WSAEINPROGRESS, "Operation now in progress"),
  E(WSAEALREADY, "Operation already in progress"),
  E(WSAENOTSOCK, "Socket operation on nonsocket"),
  E(WSAEDESTADDRREQ, "Destination address required"),
  E(WSAEMSGSIZE, "Message too long"),
  E(WSAEPROTOTYPE, "Protocol wrong for socket"),
  E(WSAENOPROTOOPT, "Bad protocol option"),
  E(WSAEPROTONOSUPPORT, "Protocol not supported"),
  E(WSAESOCKTNOSUPPORT, "Socket type not supported"),
  /* What's the difference between NOTSUPP and NOSUPPORT? :) */
  E(WSAEOPNOTSUPP, "Operation not supported"),
  E(WSAEPFNOSUPPORT,  "Protocol family not supported"),
  E(WSAEAFNOSUPPORT, "Address family not supported by protocol family"),
  E(WSAEADDRINUSE, "Address already in use"),
  E(WSAEADDRNOTAVAIL, "Cannot assign requested address"),
  E(WSAENETDOWN, "Network is down"),
  E(WSAENETUNREACH, "Network is unreachable"),
  E(WSAENETRESET, "Network dropped connection on reset"),
  E(WSAECONNABORTED, "Software caused connection abort"),
  E(WSAECONNRESET, "Connection reset by peer"),
  E(WSAENOBUFS, "No buffer space available"),
  E(WSAEISCONN, "Socket is already connected"),
  E(WSAENOTCONN, "Socket is not connected"),
  E(WSAESHUTDOWN, "Cannot send after socket shutdown"),
  E(WSAETIMEDOUT, "Connection timed out"),
  E(WSAECONNREFUSED, "Connection refused"),
  E(WSAEHOSTDOWN, "Host is down"),
  E(WSAEHOSTUNREACH, "No route to host"),
  E(WSAEPROCLIM, "Too many processes"),

  /* Yes, some of these start with WSA, not WSAE. No, I don't know why. */
  E(WSASYSNOTREADY, "Network subsystem is unavailable"),
  E(WSAVERNOTSUPPORTED, "Winsock.dll out of range"),
  E(WSANOTINITIALISED, "Successful WSAStartup not yet performed"),
  E(WSAEDISCON, "Graceful shutdown now in progress"),
#ifdef WSATYPE_NOT_FOUND
  E(WSATYPE_NOT_FOUND, "Class type not found"),
#endif
  E(WSAHOST_NOT_FOUND, "Host not found"),
  E(WSATRY_AGAIN, "Nonauthoritative host not found"),
  E(WSANO_RECOVERY, "This is a nonrecoverable error"),
  E(WSANO_DATA, "Valid name, no data record of requested type)"),

  /* There are some more error codes whose numeric values are marked
   * <b>OS dependent</b>. They start with WSA_, apparently for the same
   * reason that practitioners of some craft traditions deliberately
   * introduce imperfections into their baskets and rugs "to allow the
   * evil spirits to escape."  If we catch them, then our binaries
   * might not report consistent results across versions of Windows.
   * Thus, I'm going to let them all fall through.
   */
  { -1, NULL },
};
#undef E
/** Equivalent to strerror, but for windows socket errors. */
const char *
evutil_socket_error_to_string(int errcode)
{
  /* XXXX Is there really no built-in function to do this? */
  int i;
  for (i=0; windows_socket_errors[i].code >= 0; ++i) {
    if (errcode == windows_socket_errors[i].code)
      return windows_socket_errors[i].msg;
  }
  return strerror(errcode);
}
#endif

int
evutil_snprintf(char *buf, size_t buflen, const char *format, ...)
{
	int r;
	va_list ap;
	va_start(ap, format);
	r = evutil_vsnprintf(buf, buflen, format, ap);
	va_end(ap);
	return r;
}

int
evutil_vsnprintf(char *buf, size_t buflen, const char *format, va_list ap)
{
	int r;
	if (!buflen)
		return 0;
#ifdef _MSC_VER
	r = _vsnprintf(buf, buflen, format, ap);
	if (r < 0)
		r = _vscprintf(format, ap);
#elif defined(sgi)
	/* Make sure we always use the correct vsnprintf on IRIX */
	extern int      _xpg5_vsnprintf(char * __restrict,
		__SGI_LIBC_NAMESPACE_QUALIFIER size_t,
		const char * __restrict, /* va_list */ char *);

	r = _xpg5_vsnprintf(buf, buflen, format, ap);
#else
	r = vsnprintf(buf, buflen, format, ap);
#endif
	buf[buflen-1] = '\0';
	return r;
}

#define USE_INTERNAL_NTOP
#define USE_INTERNAL_PTON

const char *
evutil_inet_ntop(int af, const void *src, char *dst, size_t len)
{
#if defined(_EVENT_HAVE_INET_NTOP) && !defined(USE_INTERNAL_NTOP)
	return inet_ntop(af, src, dst, len);
#else
	if (af == AF_INET) {
		const struct in_addr *in = src;
		const ev_uint32_t a = ntohl(in->s_addr);
		int r;
		r = evutil_snprintf(dst, len, "%d.%d.%d.%d",
		    (int)(ev_uint8_t)((a>>24)&0xff),
		    (int)(ev_uint8_t)((a>>16)&0xff),
		    (int)(ev_uint8_t)((a>>8 )&0xff),
		    (int)(ev_uint8_t)((a    )&0xff));
		if (r<0||(size_t)r>=len)
			return NULL;
		else
			return dst;
#ifdef AF_INET6
	} else if (af == AF_INET6) {
		const struct in6_addr *addr = src;
		char buf[64], *cp;
		int longestGapLen = 0, longestGapPos = -1, i,
			curGapPos = -1, curGapLen = 0;
		ev_uint16_t words[8];
		for (i = 0; i < 8; ++i) {
			words[i] =
			    (((ev_uint16_t)addr->s6_addr[2*i])<<8) + addr->s6_addr[2*i+1];
		}
		if (words[0] == 0 && words[1] == 0 && words[2] == 0 && words[3] == 0 &&
		    words[4] == 0 && ((words[5] == 0 && words[6] && words[7]) ||
			(words[5] == 0xffff))) {
			/* This is an IPv4 address. */
			if (words[5] == 0) {
				evutil_snprintf(buf, sizeof(buf), "::%d.%d.%d.%d",
				    addr->s6_addr[12], addr->s6_addr[13],
				    addr->s6_addr[14], addr->s6_addr[15]);
			} else {
				evutil_snprintf(buf, sizeof(buf), "::%x:%d.%d.%d.%d", words[5],
				    addr->s6_addr[12], addr->s6_addr[13],
				    addr->s6_addr[14], addr->s6_addr[15]);
			}
			if (strlen(buf) > len)
				return NULL;
			strlcpy(dst, buf, len);
			return dst;
		}
		i = 0;
		while (i < 8) {
			if (words[i] == 0) {
				curGapPos = i++;
				curGapLen = 1;
				while (i<8 && words[i] == 0) {
					++i; ++curGapLen;
				}
				if (curGapLen > longestGapLen) {
					longestGapPos = curGapPos;
					longestGapLen = curGapLen;
				}
			} else {
				++i;
			}
		}
		if (longestGapLen<=1)
			longestGapPos = -1;

		cp = buf;
		for (i = 0; i < 8; ++i) {
			if (words[i] == 0 && longestGapPos == i) {
				if (i == 0)
					*cp++ = ':';
				*cp++ = ':';
				while (i < 8 && words[i] == 0)
					++i;
				--i; /* to compensate for loop increment. */
			} else {
				evutil_snprintf(cp,
								sizeof(buf)-(cp-buf), "%x", (unsigned)words[i]);
				cp += strlen(cp);
				if (i != 7)
					*cp++ = ':';
			}
		}
		*cp = '\0';
		if (strlen(buf) > len)
			return NULL;
		strlcpy(dst, buf, len);
		return dst;
#endif
	} else {
		return NULL;
	}
#endif
}

int
evutil_inet_pton(int af, const char *src, void *dst)
{
#if defined(_EVENT_HAVE_INET_PTON) && !defined(USE_INTERNAL_PTON)
	return inet_pton(af, src, dst);
#else
	if (af == AF_INET) {
		int a,b,c,d;
		char more;
		struct in_addr *addr = dst;
		if (sscanf(src, "%d.%d.%d.%d%c", &a,&b,&c,&d,&more) != 4)
			return 0;
		if (a < 0 || a > 255) return 0;
		if (b < 0 || b > 255) return 0;
		if (c < 0 || c > 255) return 0;
		if (d < 0 || d > 255) return 0;
		addr->s_addr = htonl((a<<24) | (b<<16) | (c<<8) | d);
		return 1;
#ifdef AF_INET6
	} else if (af == AF_INET6) {
		struct in6_addr *out = dst;
		ev_uint16_t words[8];
		int gapPos = -1, i, setWords=0;
		const char *dot = strchr(src, '.');
		const char *eow; /* end of words. */
		if (dot == src)
			return 0;
		else if (!dot)
			eow = src+strlen(src);
		else {
			int byte1,byte2,byte3,byte4;
			char more;
			for (eow = dot-1; eow >= src && EVUTIL_ISDIGIT(*eow); --eow)
				;
			++eow;

			/* We use "scanf" because some platform inet_aton()s are too lax
			 * about IPv4 addresses of the form "1.2.3" */
			if (sscanf(eow, "%d.%d.%d.%d%c",
					   &byte1,&byte2,&byte3,&byte4,&more) != 4)
				return 0;

			if (byte1 > 255 || byte1 < 0 ||
				byte2 > 255 || byte2 < 0 ||
				byte3 > 255 || byte3 < 0 ||
				byte4 > 255 || byte4 < 0)
				return 0;

			words[6] = (byte1<<8) | byte2;
			words[7] = (byte3<<8) | byte4;
			setWords += 2;
		}

		i = 0;
		while (src < eow) {
			if (i > 7)
				return 0;
			if (EVUTIL_ISXDIGIT(*src)) {
				char *next;
				long r = strtol(src, &next, 16);
				if (next > 4+src)
					return 0;
				if (next == src)
					return 0;
				if (r<0 || r>65536)
					return 0;

				words[i++] = (ev_uint16_t)r;
				setWords++;
				src = next;
				if (*src != ':' && src != eow)
					return 0;
				++src;
			} else if (*src == ':' && i > 0 && gapPos==-1) {
				gapPos = i;
				++src;
			} else if (*src == ':' && i == 0 && src[1] == ':' && gapPos==-1) {
				gapPos = i;
				src += 2;
			} else {
				return 0;
			}
		}

		if (setWords > 8 ||
			(setWords == 8 && gapPos != -1) ||
			(setWords < 8 && gapPos == -1))
			return 0;

		if (gapPos >= 0) {
			int nToMove = setWords - (dot ? 2 : 0) - gapPos;
			int gapLen = 8 - setWords;
			/* assert(nToMove >= 0); */
			if (nToMove < 0)
				return -1; /* should be impossible */
			memmove(&words[gapPos+gapLen], &words[gapPos],
					sizeof(ev_uint16_t)*nToMove);
			memset(&words[gapPos], 0, sizeof(ev_uint16_t)*gapLen);
		}
		for (i = 0; i < 8; ++i) {
			out->s6_addr[2*i  ] = words[i] >> 8;
			out->s6_addr[2*i+1] = words[i] & 0xff;
		}

		return 1;
#endif
	} else {
		return -1;
	}
#endif
}

int
evutil_parse_sockaddr_port(const char *ip_as_string, struct sockaddr *out, int *outlen)
{
	int port;
	char buf[128];
	const char *cp, *addr_part, *port_part;
	int is_ipv6;
	/* recognized formats are:
	 * [ipv6]:port
	 * ipv6
	 * [ipv6]
	 * ipv4:port
	 * ipv4
	 */

	cp = strchr(ip_as_string, ':');
	if (*ip_as_string == '[') {
		int len;
		if (!(cp = strchr(ip_as_string, ']'))) {
			return -1;
		}
		len = (int) ( cp-(ip_as_string + 1) );
		if (len > (int)sizeof(buf)-1) {
			return -1;
		}
		memcpy(buf, ip_as_string+1, len);
		buf[len] = '\0';
		addr_part = buf;
		if (cp[1] == ':')
			port_part = cp+2;
		else
			port_part = NULL;
		is_ipv6 = 1;
	} else if (cp && strchr(cp+1, ':')) {
		is_ipv6 = 1;
		addr_part = ip_as_string;
		port_part = NULL;
	} else if (cp) {
		is_ipv6 = 0;
		if (cp - ip_as_string > (int)sizeof(buf)-1) {
			return -1;
		}
		memcpy(buf, ip_as_string, cp-ip_as_string);
		buf[cp-ip_as_string] = '\0';
		addr_part = buf;
		port_part = cp+1;
	} else {
		addr_part = ip_as_string;
		port_part = NULL;
		is_ipv6 = 0;
	}

	if (port_part == NULL) {
		port = 0;
	} else {
		port = atoi(port_part);
		if (port <= 0 || port > 65535) {
			return -1;
		}
	}

	if (!addr_part)
		return -1; /* Should be impossible. */
#ifdef AF_INET6
	if (is_ipv6)
	{
		struct sockaddr_in6 sin6;
		memset(&sin6, 0, sizeof(sin6));
#ifdef _EVENT_HAVE_STRUCT_SOCKADDR_IN6_SIN6_LEN
		sin6.sin6_len = sizeof(sin6);
#endif
		sin6.sin6_family = AF_INET6;
		sin6.sin6_port = htons(port);
		if (1 != evutil_inet_pton(AF_INET6, addr_part, &sin6.sin6_addr))
			return -1;
		if ((int)sizeof(sin6) > *outlen)
			return -1;
		memset(out, 0, *outlen);
		memcpy(out, &sin6, sizeof(sin6));
		*outlen = sizeof(sin6);
		return 0;
	}
	else
#endif
	{
		struct sockaddr_in sin;
		memset(&sin, 0, sizeof(sin));
#ifdef _EVENT_HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
		sin.sin_len = sizeof(sin);
#endif
		sin.sin_family = AF_INET;
		sin.sin_port = htons(port);
		if (1 != evutil_inet_pton(AF_INET, addr_part, &sin.sin_addr))
			return -1;
		if ((int)sizeof(sin) > *outlen)
			return -1;
		memset(out, 0, *outlen);
		memcpy(out, &sin, sizeof(sin));
		*outlen = sizeof(sin);
		return 0;
	}
}

const char *
evutil_format_sockaddr_port(const struct sockaddr *sa, char *out, size_t outlen)
{
	char b[128];
	const char *res=NULL;
	int port;
	if (sa->sa_family == AF_INET) {
		const struct sockaddr_in *sin = (const struct sockaddr_in*)sa;
		res = evutil_inet_ntop(AF_INET, &sin->sin_addr,b,sizeof(b));
		port = ntohs(sin->sin_port);
		if (res) {
			evutil_snprintf(out, outlen, "%s:%d", b, port);
			return out;
		}
	} else if (sa->sa_family == AF_INET6) {
		const struct sockaddr_in6 *sin6 = (const struct sockaddr_in6*)sa;
		res = evutil_inet_ntop(AF_INET6, &sin6->sin6_addr,b,sizeof(b));
		port = ntohs(sin6->sin6_port);
		if (res) {
			evutil_snprintf(out, outlen, "[%s]:%d", b, port);
			return out;
		}
	}

	evutil_snprintf(out, outlen, "<addr with socktype %d>",
	    (int)sa->sa_family);
	return out;
}

int
evutil_sockaddr_cmp(const struct sockaddr *sa1, const struct sockaddr *sa2,
    int include_port)
{
	int r;
	if (0 != (r = (sa1->sa_family - sa2->sa_family)))
		return r;

	if (sa1->sa_family == AF_INET) {
		const struct sockaddr_in *sin1, *sin2;
		sin1 = (const struct sockaddr_in *)sa1;
		sin2 = (const struct sockaddr_in *)sa2;
		if (sin1->sin_addr.s_addr < sin2->sin_addr.s_addr)
			return -1;
		else if (sin1->sin_addr.s_addr > sin2->sin_addr.s_addr)
			return 1;
		else if (include_port &&
		    (r = ((int)sin1->sin_port - (int)sin2->sin_port)))
			return r;
		else
			return 0;
	}
#ifdef AF_INET6
	else if (sa1->sa_family == AF_INET6) {
		const struct sockaddr_in6 *sin1, *sin2;
		sin1 = (const struct sockaddr_in6 *)sa1;
		sin2 = (const struct sockaddr_in6 *)sa2;
		if ((r = memcmp(sin1->sin6_addr.s6_addr, sin2->sin6_addr.s6_addr, 16)))
			return r;
		else if (include_port &&
		    (r = ((int)sin1->sin6_port - (int)sin2->sin6_port)))
			return r;
		else
			return 0;
	}
#endif
	return 1;
}

/* Tables to implement ctypes-replacement EVUTIL_IS*() functions.  Each table
 * has 256 bits to look up whether a character is in some set or not.  This
 * fails on non-ASCII platforms, but so does every other place where we
 * take a char and write it onto the network.
 **/
static const ev_uint32_t EVUTIL_ISALPHA_TABLE[8] =
  { 0, 0, 0x7fffffe, 0x7fffffe, 0, 0, 0, 0 };
static const ev_uint32_t EVUTIL_ISALNUM_TABLE[8] =
  { 0, 0x3ff0000, 0x7fffffe, 0x7fffffe, 0, 0, 0, 0 };
static const ev_uint32_t EVUTIL_ISSPACE_TABLE[8] = { 0x3e00, 0x1, 0, 0, 0, 0, 0, 0 };
static const ev_uint32_t EVUTIL_ISXDIGIT_TABLE[8] =
  { 0, 0x3ff0000, 0x7e, 0x7e, 0, 0, 0, 0 };
static const ev_uint32_t EVUTIL_ISDIGIT_TABLE[8] = { 0, 0x3ff0000, 0, 0, 0, 0, 0, 0 };
static const ev_uint32_t EVUTIL_ISPRINT_TABLE[8] =
  { 0, 0xffffffff, 0xffffffff, 0x7fffffff, 0, 0, 0, 0x0 };
static const ev_uint32_t EVUTIL_ISUPPER_TABLE[8] = { 0, 0, 0x7fffffe, 0, 0, 0, 0, 0 };
static const ev_uint32_t EVUTIL_ISLOWER_TABLE[8] = { 0, 0, 0, 0x7fffffe, 0, 0, 0, 0 };
/* Upper-casing and lowercasing tables to map characters to upper/lowercase
 * equivalents. */
static const unsigned char EVUTIL_TOUPPER_TABLE[256] = {
  0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
  16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
  32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,
  48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,
  64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,
  80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,
  96,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,
  80,81,82,83,84,85,86,87,88,89,90,123,124,125,126,127,
  128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
  144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
  160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
  176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
  192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
  208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
  224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
  240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255,
};
static const unsigned char EVUTIL_TOLOWER_TABLE[256] = {
  0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
  16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
  32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,
  48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,
  64,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,
  112,113,114,115,116,117,118,119,120,121,122,91,92,93,94,95,
  96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,
  112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
  128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
  144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
  160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
  176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
  192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
  208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
  224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
  240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255,
};

#define IMPL_CTYPE_FN(name)						\
	int EVUTIL_##name(char c) {					\
		ev_uint8_t u = c;					\
		return !!(EVUTIL_##name##_TABLE[(u >> 5) & 7] & (1 << (u & 31))); \
	}
IMPL_CTYPE_FN(ISALPHA)
IMPL_CTYPE_FN(ISALNUM)
IMPL_CTYPE_FN(ISSPACE)
IMPL_CTYPE_FN(ISDIGIT)
IMPL_CTYPE_FN(ISXDIGIT)
IMPL_CTYPE_FN(ISPRINT)
IMPL_CTYPE_FN(ISLOWER)
IMPL_CTYPE_FN(ISUPPER)

char EVUTIL_TOLOWER(char c)
{
	return ((char)EVUTIL_TOLOWER_TABLE[(ev_uint8_t)c]);
}
char EVUTIL_TOUPPER(char c)
{
	return ((char)EVUTIL_TOUPPER_TABLE[(ev_uint8_t)c]);
}
int
evutil_ascii_strcasecmp(const char *s1, const char *s2)
{
	char c1, c2;
	while (1) {
		c1 = EVUTIL_TOLOWER(*s1++);
		c2 = EVUTIL_TOLOWER(*s2++);
		if (c1 < c2)
			return -1;
		else if (c1 > c2)
			return 1;
		else if (c1 == 0)
			return 0;
	}
}
int evutil_ascii_strncasecmp(const char *s1, const char *s2, size_t n)
{
	char c1, c2;
	while (n--) {
		c1 = EVUTIL_TOLOWER(*s1++);
		c2 = EVUTIL_TOLOWER(*s2++);
		if (c1 < c2)
			return -1;
		else if (c1 > c2)
			return 1;
		else if (c1 == 0)
			return 0;
	}
	return 0;
}

static int
evutil_issetugid(void)
{
#ifdef _EVENT_HAVE_ISSETUGID
	return issetugid();
#else

#ifdef _EVENT_HAVE_GETEUID
	if (getuid() != geteuid())
		return 1;
#endif
#ifdef _EVENT_HAVE_GETEGID
	if (getgid() != getegid())
		return 1;
#endif
	return 0;
#endif
}

const char *
evutil_getenv(const char *varname)
{
	if (evutil_issetugid())
		return NULL;

	return getenv(varname);
}

long
_evutil_weakrand(void)
{
#ifdef WIN32
	return rand();
#else
	return random();
#endif
}

int
evutil_sockaddr_is_loopback(const struct sockaddr *addr)
{
	static const char LOOPBACK_S6[16] =
	    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\1";
	if (addr->sa_family == AF_INET) {
		struct sockaddr_in *sin = (struct sockaddr_in *)addr;
		return (ntohl(sin->sin_addr.s_addr) & 0xff000000) == 0x7f000000;
	} else if (addr->sa_family == AF_INET6) {
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)addr;
		return !memcmp(sin6->sin6_addr.s6_addr, LOOPBACK_S6, 16);
	}
	return 0;
}

#define MAX_SECONDS_IN_MSEC_LONG \
	(((LONG_MAX) - 999) / 1000)

long
evutil_tv_to_msec(const struct timeval *tv)
{
	if (tv->tv_usec > 1000000 || tv->tv_sec > MAX_SECONDS_IN_MSEC_LONG)
		return -1;

	return (tv->tv_sec * 1000) + ((tv->tv_usec + 999) / 1000);
}

int
evutil_hex_char_to_int(char c)
{
	switch(c)
	{
		case '0': return 0;
		case '1': return 1;
		case '2': return 2;
		case '3': return 3;
		case '4': return 4;
		case '5': return 5;
		case '6': return 6;
		case '7': return 7;
		case '8': return 8;
		case '9': return 9;
		case 'A': case 'a': return 10;
		case 'B': case 'b': return 11;
		case 'C': case 'c': return 12;
		case 'D': case 'd': return 13;
		case 'E': case 'e': return 14;
		case 'F': case 'f': return 15;
	}
	return -1;
}

#ifdef WIN32
HANDLE
evutil_load_windows_system_library(const TCHAR *library_name)
{
  TCHAR path[MAX_PATH];
  unsigned n;
  n = GetSystemDirectory(path, MAX_PATH);
  if (n == 0 || n + _tcslen(library_name) + 2 >= MAX_PATH)
    return 0;
  _tcscat(path, TEXT("\\"));
  _tcscat(path, library_name);
  return LoadLibrary(path);
}
#endif

