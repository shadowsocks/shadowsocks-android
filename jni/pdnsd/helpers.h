/* helpers.h - Various helper functions

   Copyright (C) 2000, 2001 Thomas Moestl
   Copyright (C) 2002, 2003, 2004, 2007, 2009, 2011 Paul A. Rombouts

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


#ifndef HELPERS_H
#define HELPERS_H

#include <config.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include "cache.h"
#include "pdnsd_assert.h"

#define SOFTLOCK_MAXTRIES 1000

int run_as(const char *user);
void pdnsd_exit(void);
int softlock_mutex(pthread_mutex_t *mutex);

#if 0
inline static int isdchar (unsigned char c)
{
  return ((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='-'
#ifdef UNDERSCORE
	  || c=='_'
#endif
	  );
}
#endif

const unsigned char *rhn2str(const unsigned char *rhn, unsigned char *str, unsigned int size);
int  str2rhn(const unsigned char *str, unsigned char *rhn);
const char *parsestr2rhn(const unsigned char *str, unsigned int len, unsigned char *rhn);

/* Note added by Paul Rombouts:
   Compared to the definition used by Thomas Moestl (strlen(rhn)+1), the following definition of rhnlen
   may yield a different result in certain error situations (when a domain name segment contains a null byte).
*/
inline static unsigned int rhnlen(const unsigned char *rhn)
  __attribute__((always_inline));
inline static unsigned int rhnlen(const unsigned char *rhn)
{
	unsigned int i=0,lb;

	while((lb=rhn[i++]))
		i+=lb;
	return i;
}

/* Skip k segments in a name in length-byte string notation. */
inline static const unsigned char *skipsegs(const unsigned char *nm, unsigned k)
  __attribute__((always_inline));
inline static const unsigned char *skipsegs(const unsigned char *nm, unsigned k)
{
	unsigned lb;
	for(;k && (lb= *nm); --k) {
		nm += lb+1;
	}
	return nm;
}

/* Skip a name in length-byte string notation and return a pointer to the
   position right after the terminating null byte.
*/
inline static unsigned char *skiprhn(unsigned char *rhn)
  __attribute__((always_inline));
inline static unsigned char *skiprhn(unsigned char *rhn)
{
	unsigned lb;

	while((lb= *rhn++))
		rhn += lb;
	return rhn;
}

/* count the number of name segments of a name in length-byte string notation. */
inline static unsigned int rhnsegcnt(const unsigned char *rhn)
  __attribute__((always_inline));
inline static unsigned int rhnsegcnt(const unsigned char *rhn)
{
	unsigned int res=0,lb;

	while((lb= *rhn)) {
		++res;
		rhn += lb+1;
	}
	return res;
}

unsigned int rhncpy(unsigned char *dst, const unsigned char *src);
int isnormalencdomname(const unsigned char *rhn, unsigned maxlen);

inline static int is_inaddr_any(pdnsd_a *a)  __attribute__((always_inline));
inline static int is_inaddr_any(pdnsd_a *a)
{
  return SEL_IPVER( a->ipv4.s_addr==INADDR_ANY,
		    IN6_IS_ADDR_UNSPECIFIED(&a->ipv6) );
}

/* Same as is_inaddr_any(), but for the pdnsd_a2 type. */
inline static int is_inaddr2_any(pdnsd_a2 *a)  __attribute__((always_inline));
inline static int is_inaddr2_any(pdnsd_a2 *a)
{
  return SEL_IPVER( a->ipv4.s_addr==INADDR_ANY,
		    IN6_IS_ADDR_UNSPECIFIED(&a->ipv6) );
}


inline static int same_inaddr(pdnsd_a *a, pdnsd_a *b)
  __attribute__((always_inline));
inline static int same_inaddr(pdnsd_a *a, pdnsd_a *b)
{
  return SEL_IPVER( a->ipv4.s_addr==b->ipv4.s_addr,
		    IN6_ARE_ADDR_EQUAL(&a->ipv6,&b->ipv6) );
}

/* Compare a pdnsd_a*  with a pdnsd_a2*. */
inline static int same_inaddr2(pdnsd_a *a, pdnsd_a2 *b)
  __attribute__((always_inline));
inline static int same_inaddr2(pdnsd_a *a, pdnsd_a2 *b)
{
  return SEL_IPVER( a->ipv4.s_addr==b->ipv4.s_addr,
		    IN6_ARE_ADDR_EQUAL(&a->ipv6,&b->ipv6) && b->ipv4.s_addr==INADDR_ANY );
}

inline static int equiv_inaddr2(pdnsd_a *a, pdnsd_a2 *b)
  __attribute__((always_inline));
inline static int equiv_inaddr2(pdnsd_a *a, pdnsd_a2 *b)
{
  return SEL_IPVER( a->ipv4.s_addr==b->ipv4.s_addr,
		    IN6_ARE_ADDR_EQUAL(&a->ipv6,&b->ipv6) ||
		     (b->ipv4.s_addr!=INADDR_ANY && ADDR_EQUIV6_4(&a->ipv6,&b->ipv4)) );
}

int str2pdnsd_a(const char *addr, pdnsd_a *a);
const char *pdnsd_a2str(pdnsd_a *a, char *buf, int maxlen);

int init_rng(void);
#ifdef RANDOM_DEVICE
extern FILE *rand_file;
/* Because this is usually empty, it is now defined as a macro to save overhead.*/
#define free_rng() {if (rand_file) fclose(rand_file);}
#else
#define free_rng()
#endif

unsigned short get_rand16(void);

int fsprintf(int fd, const char *format, ...) printfunc(2, 3);
#if !defined(CPP_C99_VARIADIC_MACROS)
/* GNU C Macro Varargs style. */
# define fsprintf_or_return(args...) {int _retval; if((_retval=fsprintf(args))<0) return _retval;}
#else
/* ANSI C99 style variadic macro. */
# define fsprintf_or_return(...) {int _retval; if((_retval=fsprintf(__VA_ARGS__))<0) return _retval;}
#endif

/* Added by Paul Rombouts */
inline static ssize_t write_all(int fd,const void *data,size_t n)
  __attribute__((always_inline));
inline static ssize_t write_all(int fd,const void *data,size_t n)
{
  ssize_t written=0;

  while(written<n) {
      ssize_t m=write(fd,(const void*)(((const char*)data)+written),n-written);

      if(m<0)
	return m;

      written+=m;
    }

  return written;
}

void hexdump(const void *data, int dlen, char *buf, int buflen);
int escapestr(const char *in, int ilen, char *str, int size);

#if 0
inline static int stricomp(const char *a, const char *b)
{
  return !strcasecmp(a,b);
}
#endif

/* compare two names in length byte - string format
   rhnicmp returns 1 if the names are the same (ignoring upper/lower case),
   0 otherwise.
 */
inline static int rhnicmp(const unsigned char *a, const unsigned char *b)
  __attribute__((always_inline));
inline static int rhnicmp(const unsigned char *a, const unsigned char *b)
{
	unsigned int i=0;
	unsigned char lb;
	for(;;) {
		lb=a[i];
		if(lb!=b[i]) return 0;
		if(!lb) break;
		++i;
		do {
			if(tolower(a[i])!=tolower(b[i])) return 0;
			++i;
		} while(--lb);
	}
	return 1;
}

/* Bah. I want strlcpy. */
inline static int strncp(char *dst, const char *src, size_t dstsz)
  __attribute__((always_inline));
inline static int strncp(char *dst, const char *src, size_t dstsz)
{
#ifdef HAVE_STRLCPY
	return (strlcpy(dst,src,dstsz)<dstsz);
#else
#ifdef HAVE_STPNCPY
	char *p=stpncpy(dst,src,dstsz);
	if(p<dst+dstsz) return 1;
	*(p-1)='\0';
	return 0;
#else
	strncpy(dst,src,dstsz);
	if(strlen(src)<dstsz) return 1;
	dst[dstsz-1]='\0';
	return 0;
#endif
#endif
}

#ifndef HAVE_STRDUP
inline static char *strdup(const char *s)
  __attribute__((always_inline));
inline static char *strdup(const char *s)
{
	size_t sz=strlen(s)+1;
	char *cp=malloc(sz);
	if(cp)
		memcpy(cp,s,sz);
	return cp;
}
#endif

#ifndef HAVE_STRNDUP
/* This version may allocate a buffer that is unnecessarily large,
   but I'm always going to use it with n<strlen(s)
*/
inline static char *strndup(const char *s, size_t n)
  __attribute__((always_inline));
inline static char *strndup(const char *s, size_t n)
{
	char *cp;
	cp=malloc(n+1);
	if(cp) {
		memcpy(cp,s,n);
		cp[n]='\0';
	}
	return cp;
}
#endif

#ifndef HAVE_STPCPY
inline static char *stpcpy (char *dest, const char *src)
  __attribute__((always_inline));
inline static char *stpcpy (char *dest, const char *src)
{
  register char *d = dest;
  register const char *s = src;

  while ((*d++ = *s++) != '\0');

  return d - 1;
}
#endif

#ifndef HAVE_MEMPCPY
inline static void *mempcpy(void *dest, const void *src, size_t len)
  __attribute__((always_inline));
inline static void *mempcpy(void *dest, const void *src, size_t len)
{
  memcpy(dest,src,len);
  return ((char *)dest)+len;
}
#endif

#ifndef HAVE_GETLINE
int getline(char **lineptr, size_t *n, FILE *stream);
#endif

#ifndef HAVE_ASPRINTF
int asprintf (char **lineptr, const char *format, ...);
#endif

#ifndef HAVE_VASPRINTF
#include <stdarg.h>
int vasprintf (char **lineptr, const char *format, va_list va);
#endif

#ifndef HAVE_INET_NTOP
const char *inet_ntop(int af, const void *src, char *dst, size_t size);
#endif

#define strlitlen(strlit) (sizeof(strlit)-1)

#endif /* HELPERS_H */
