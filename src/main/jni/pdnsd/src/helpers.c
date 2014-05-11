/* helpers.c - Various helper functions

   Copyright (C) 2000, 2001 Thomas Moestl
   Copyright (C) 2002, 2003, 2005, 2006, 2008, 2011 Paul A. Rombouts

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
#include <sys/time.h>
#include <signal.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include "ipvers.h"
#include "thread.h"
#include "error.h"
#include "helpers.h"
#include "cache.h"
#include "conff.h"


/*
 * This is to exit pdnsd from any thread.
 */
void pdnsd_exit()
{
	pthread_kill(main_thrid,SIGTERM);
	pthread_exit(NULL);
}

/*
 * Try to grab a mutex. If we can't, fail. This will loop until we get the
 * mutex or fail. This is only used in debugging code or at exit, otherwise
 * we might run into lock contention problems.
 */
int softlock_mutex(pthread_mutex_t *mutex)
{
	unsigned int tr=0;
	while(pthread_mutex_trylock(mutex)) {
		if (++tr>=SOFTLOCK_MAXTRIES)
			return 0;
		usleep_r(10000);
	}
	return 1;
}

/*
 * setuid() and setgid() for a specified user.
 */
int run_as(const char *user)
{
	if (user[0]) {
#ifdef HAVE_GETPWNAM_R
		struct passwd pwdbuf, *pwd;
		size_t buflen;
		int err;

		for(buflen=128;; buflen*=2) {
			char buf[buflen];  /* variable length array */

			/* Note that we use getpwnam_r() instead of getpwnam(),
			   which returns its result in a statically allocated buffer and
			   cannot be considered thread safe.
			   Doesn't use NSS! */
			err=getpwnam_r(user, &pwdbuf, buf, buflen, &pwd);
			if(err==0 && pwd) {
				/* setgid first, because we may not be allowed to do it anymore after setuid */
				if (setgid(pwd->pw_gid)!=0) {
					log_error("Could not change group id to that of run_as user '%s': %s",
						  user,strerror(errno));
					return 0;
				}

				/* initgroups uses NSS, so we can disable it,
				   i.e. we might need DNS for LDAP lookups, which times out */
				if (global.use_nss && (initgroups(user, pwd->pw_gid)!=0)) {
					log_error("Could not initialize the group access list of run_as user '%s': %s",
						  user,strerror(errno));
					return 0;
				}
				if (setuid(pwd->pw_uid)!=0) {
					log_error("Could not change user id to that of run_as user '%s': %s",
						  user,strerror(errno));
					return 0;
				}
				break;
			}
			else if(err!=ERANGE) {
				if(err)
					log_error("run_as user '%s' could not be found: %s",user,strerror(err));
				else
					log_error("run_as user '%s' could not be found.",user);
				return 0;
			}
			else if(buflen>=16*1024) {
				/* If getpwnam_r() seems defective, call it quits rather than
				   keep on allocating ever larger buffers until we crash. */
				log_error("getpwnam_r() requires more than %u bytes of buffer space.",(unsigned)buflen);
				return 0;
			}
			/* Else try again with larger buffer. */
		}
#else
		/* No getpwnam_r() :-(  We'll use getpwnam() and hope for the best. */
		struct passwd *pwd;

		if (!(pwd=getpwnam(user))) {
			log_error("run_as user %s could not be found.",user);
			return 0;
		}
		/* setgid first, because we may not allowed to do it anymore after setuid */
		if (setgid(pwd->pw_gid)!=0) {
			log_error("Could not change group id to that of run_as user '%s': %s",
				  user,strerror(errno));
			return 0;
		}
		/* initgroups uses NSS, so we can disable it,
		   i.e. we might need DNS for LDAP lookups, which times out */
		if (global.use_nss && (initgroups(user, pwd->pw_gid)!=0)) {
			log_error("Could not initialize the group access list of run_as user '%s': %s",
				  user,strerror(errno));
			return 0;
		}
		if (setuid(pwd->pw_uid)!=0) {
			log_error("Could not change user id to that of run_as user '%s': %s",
				  user,strerror(errno));
			return 0;
		}
#endif
	}

	return 1;
}

/*
 * returns whether c is allowed in IN domain names
 */
#if 0
int isdchar (unsigned char c)
{
	if ((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='-'
#ifdef UNDERSCORE
	    || c=='_'
#endif
	   )
		return 1;
	return 0;
}
#endif

/*
 * Convert a string given in dotted notation to the transport format (length byte prepended
 * domain name parts, ended by a null length sequence)
 * The memory areas referenced by str and rhn may not overlap.
 * The buffer rhn points to is assumed to be at least DNSNAMEBUFSIZE bytes in size.
 *
 * Returns 1 if successful, otherwise 0.
 */
int str2rhn(const unsigned char *str, unsigned char *rhn)
{
	unsigned int i,j;

	if(*str=='.' && !*(str+1)) {
		/* Special case: root domain */
		rhn[0]=0;
		return 1;
	}

	for(i=0;;) {
		unsigned int jlim= i+63;
		if(jlim>DNSNAMEBUFSIZE-2) jlim=DNSNAMEBUFSIZE-2; /* DNSNAMEBUFSIZE-2 because the termination 0 has to follow */
		for(j=i; str[j] && str[j]!='.'; ++j) {
			if(j>=jlim) return 0;
			rhn[j+1]=str[j];
		}
		if(!str[j])
			break;
		if(j<=i)
			return 0;
		rhn[i]=(unsigned char)(j-i);
		i = j+1;
	}

	rhn[i]=0;
	if (j>i || i==0)
		return 0;
	return 1;
}

/*
  parsestr2rhn is essentially the same as str2rhn, except that it doesn't look beyond
  the first len chars in the input string. It also tolerates strings
  not ending in a dot and returns a message in case of an error.
 */
const char *parsestr2rhn(const unsigned char *str, unsigned int len, unsigned char *rhn)
{
	unsigned int i,j;

	if(len>0 && *str=='.' && (len==1 || !*(str+1))) {
		/* Special case: root domain */
		rhn[0]=0;
		return NULL;
	}

	i=0;
	do {
		unsigned int jlim= i+63;
		if(jlim>DNSNAMEBUFSIZE-2) jlim=DNSNAMEBUFSIZE-2;
		for(j=i; j<len && str[j] && str[j]!='.'; ++j) {
			/* if(!isdchar(str[j]))
				return "Illegal character in domain name"; */
			if(j>=jlim)
				return "Domain name element too long";
			rhn[j+1]=str[j];
		}

		if(j<=i) {
			if(j<len && str[j])
				return "Empty name element in domain name";
			else
				break;
		}

		rhn[i]=(unsigned char)(j-i);
		i = j+1;
	} while(j<len && str[j]);

	rhn[i]=0;
	if(i==0)
		return "Empty domain name not allowed";
	return NULL;
}

/*
 * Take a host name as used in the dns transfer protocol (a length byte,
 * followed by the first part of the name, ..., followed by a 0 length byte),
 * and return a string in the usual dotted notation. Length checking is done
 * elsewhere (in decompress_name), this takes names from the cache which are
 * validated. No more than "size" bytes will be written to the buffer str points to
 * (but size must be positive).
 * If the labels in rhn contains non-printable characters, these are represented
 * in the result by a backslash followed three octal digits. Additionally some
 * special characters are preceded by a backslash. Normally the result should
 * fit within DNSNAMEBUFSIZE bytes, but if there are many non-printable characters, the
 * receiving buffer may not be able to contain the full result. In that case the
 * truncation in the result is indicated by series of trailing dots. This
 * function is only used for diagnostic purposes, thus a truncated result should
 * not be a very serious problem (and should only occur under pathological
 * circumstances).
 */
const unsigned char *rhn2str(const unsigned char *rhn, unsigned char *str, unsigned int size)
{
	unsigned int i,j,lb;

	if(size==0) return NULL;

	i=0; j=0;
	lb=rhn[i++];
	if (!lb) {
		if(size>=2)
			str[j++]='.';
	}
	else {
		do {
			for (;lb;--lb) {
				unsigned char c;
				if(j+2>=size)
					goto overflow;
				c=rhn[i++];
				if(isgraph(c)) {
					if(c=='.' || c=='\\' || c=='"') {
						str[j++]='\\';
						if(j+2>=size)
							goto overflow;
					}
					str[j++]=c;
				}
				else {
					unsigned int rem=size-1-j;
					int n=snprintf(charp &str[j],rem,"\\%03o",c);
					if(n<0 || n>=rem) {
						str[j++]='.';
						goto overflow;
					}
					j+=n;
				}
			}
			str[j++]='.';
			lb=rhn[i++];
		} while(lb);
	}
	str[j]=0;
	return str;

 overflow:
	j=size;
	str[--j]=0;
	if(j>0) {
		str[--j]='.';
		if(j>0) {
			str[--j]='.';
			if(j>0)
				str[--j]='.';
		}
	}
	return str;
}

/* Return the length of a domain name in transport format.
   The definition has in fact been moved to helpers.h as an inline function.
   Note added by Paul Rombouts:
   Compared to the definition used by Thomas Moestl (strlen(rhn)+1), the following definition of rhnlen
   may yield a different result in certain error situations (when a domain name segment contains null byte).
*/
#if 0
unsigned int rhnlen(const unsigned char *rhn)
{
	unsigned int i=0,lb;

	while((lb=rhn[i++]))
		i+=lb;
	return i;
}
#endif

/*
 * Non-validating rhn copy (use with checked or generated data only).
 * Returns number of characters copied. The buffer dst points to is assumed to be DNSNAMEBUFSIZE (or
 * at any rate large enough) bytes in size.
 * The answer assembly code uses this; it is guaranteed to not clobber anything
 * after the name.
 */
unsigned int rhncpy(unsigned char *dst, const unsigned char *src)
{
	unsigned int len = rhnlen(src);

	PDNSD_ASSERT(len<=DNSNAMEBUFSIZE,"rhncpy: src too long!");
	memcpy(dst,src,len>DNSNAMEBUFSIZE?DNSNAMEBUFSIZE:len);
	return len;
}


/* Check whether a name is a normal wire-encoded domain name,
   i.e. is not compressed, doesn't use extended labels and is not
   too long.
*/
int isnormalencdomname(const unsigned char *rhn, unsigned maxlen)
{
	unsigned int i,lb;

	if(maxlen>DNSNAMEBUFSIZE)
		maxlen=DNSNAMEBUFSIZE;
	for(i=0;;) {
		if(i>=maxlen) return 0;
		lb=rhn[i++];
		if(lb==0)    break;
		if(lb>0x3f)  return 0;
		i += lb;
	}

	return 1;
}

int str2pdnsd_a(const char *addr, pdnsd_a *a)
{
#ifdef ENABLE_IPV4
	if (run_ipv4) {
		return inet_aton(addr,&a->ipv4);
	}
#endif
#ifdef ENABLE_IPV6
	ELSE_IPV6 {
		/* Try to map an IPv4 address to IPv6 */
		struct in_addr a4;
		if(inet_aton(addr,&a4)) {
			a->ipv6=global.ipv4_6_prefix;
			((uint32_t *)(&a->ipv6))[3]=a4.s_addr;
			return 1;
		}
		return inet_pton(AF_INET6,addr,&a->ipv6)>0;
	}
#endif
	/* return 0; */
}

/* definition moved to helpers.h */
#if 0
int is_inaddr_any(pdnsd_a *a)
{
  return SEL_IPVER( a->ipv4.s_addr==INADDR_ANY,
		    IN6_IS_ADDR_UNSPECIFIED(&a->ipv6) );
}
#endif

/*
 * This is used for user output only, so it does not matter when an error occurs.
 */
const char *pdnsd_a2str(pdnsd_a *a, char *buf, int maxlen)
{
	const char *res= SEL_IPVER( inet_ntop(AF_INET,&a->ipv4,buf,maxlen),
				    inet_ntop(AF_INET6,&a->ipv6,buf,maxlen) );
	if (!res) {
		log_error("inet_ntop: %s", strerror(errno));
		return "?.?.?.?";
	}

	return res;
}


/* Appropriately set our random device */
#ifdef R_DEFAULT
# if (TARGET == TARGET_BSD) && !defined(__NetBSD__)
#  define R_ARC4RANDOM 1
# else
#  define R_RANDOM 1
# endif
#endif

#ifdef RANDOM_DEVICE
FILE *rand_file;
#endif

#ifdef R_RANDOM
void init_crandom()
{
	struct timeval tv;
	struct timezone tz;

	gettimeofday(&tv,&tz);
	srandom(tv.tv_sec^tv.tv_usec); /* not as guessable as time() */
}
#endif

/* initialize the PRNG */
int init_rng()
{
#ifdef RANDOM_DEVICE
	if (!(rand_file=fopen(RANDOM_DEVICE,"r"))) {
		log_error("Could not open %s.",RANDOM_DEVICE);
		return 0;
	}
#endif
#ifdef R_RANDOM
	init_crandom();
#endif
	return 1;
}

/* The following function is now actually defined as a macro in helpers.h */
#if 0
void free_rng()
{
#ifdef RANDOM_DEVICE
	if (rand_file)
		fclose(rand_file);
#endif
}
#endif

/* generate a (more or less) random number 16 bits long. */
unsigned short get_rand16()
{
#ifdef RANDOM_DEVICE
	unsigned short rv;

	if (rand_file) {
		if (fread(&rv,sizeof(rv),1, rand_file)!=1) {
			log_error("Error while reading from random device: %s", strerror(errno));
			pdnsd_exit();
		}
		return rv&0xffff;
	} else
		return random()&0xffff;
#endif
#ifdef R_RANDOM
	return random()&0xffff;
#endif
#ifdef R_ARC4RANDOM
	return arc4random()&0xffff;
#endif
}

/* fsprintf does formatted output to a file descriptor.
   The functionality is similar to fprintf, but note that fd
   is of type int instead of FILE*.
   This function has been rewritten by Paul Rombouts */
int fsprintf(int fd, const char *format, ...)
{
	int n;
	va_list va;

	{
		char buf[256];

		va_start(va,format);
		n=vsnprintf(buf,sizeof(buf),format,va);
		va_end(va);

		if(n<(int)sizeof(buf)) {
			if(n>0) n=write_all(fd,buf,n);
			return n;
		}
	}
	/* retry with a right sized buffer, needs glibc 2.1 or higher to work */
	{
		unsigned bufsize=n+1;
		char buf[bufsize];

		va_start(va,format);
		n=vsnprintf(buf,bufsize,format,va);
		va_end(va);

		if(n>0) n=write_all(fd,buf,n);
	}
	return n;
}

/* Convert data into a hexadecimal representation (for debugging purposes)..
   The result is stored in the character array buf.
   If buf is not large enough to hold the result, the
   truncation is indicated by trailing dots.
*/
void hexdump(const void *data, int dlen, char *buf, int buflen)
{
	const unsigned char *p=data;
	int i,j=0;
	for(i=0;i<dlen;++i) {
		int rem=buflen-j;
		int n=snprintf(buf+j, rem, i==0?"%02x":" %02x", p[i]);
		if(n<0 || n>=rem) goto truncated;
		j += n;
	}
	goto done;

 truncated:
	if(j>=6) {
		j -= 3;
		if(j+4>=buflen)
			j -= 3;
		buf[j++]=' ';
		buf[j++]='.';
		buf[j++]='.';
		buf[j++]='.';
	}
	else {
		int ndots=buflen-1;
		if(ndots>3) ndots=3;
		j=0;
		while(j<ndots) buf[j++]='.';
	}
 done:
	buf[j]=0;
}

/* Copy string "in" to "str" buffer, converting non-printable characters
   to escape sequences.
   Returns the length of the output string, or -1 if the result does not
   fit in the output buffer.
*/
int escapestr(const char *in, int ilen, char *str, int size)
{
	int i,j=0;
	for(i=0;i<ilen;++i) {
		unsigned char c;
		if(j+1>=size)
			return -1;
		c=in[i];
		if(!isprint(c)) {
			int rem=size-j;
			int n=snprintf(&str[j],rem,"\\%03o",c);
			if(n<0 || n>=rem) {
				return -1;
			}
			j+=n;
		}
		else {
			if(c=='\\' || c=='"') {
				str[j++]='\\';
				if(j+1>=size)
					return -1;
			}
			str[j++]=c;
		}
	}
	str[j]=0;
	return j;
}

/*
 * This is not like strcmp, but will return 1 on match or 0 if the
 * strings are different.
 */
/* definition moved to helpers.h as an inline function. */
#if 0
int stricomp(char *a, char *b)
{
	int i;
	if (strlen(a) != strlen(b))
		return 0;
	for (i=0;i<strlen(a);i++) {
		if (tolower(a[i])!=tolower(b[i]))
			return 0;
	}
	return 1;
}
#endif

/* Bah. I want strlcpy */
/* definition moved to helpers.h */
#if 0
int strncp(char *dst, char *src, int dstsz)
{
	char o;

	strncpy(dst,src,dstsz);
	o=dst[dstsz-1];
	dst[dstsz-1]='\0';
	if (strlen(dst) >= dstsz-1 && o!='\0')
		return 0;
	return 1;
}
#endif

#ifndef HAVE_GETLINE
/* Note by Paul Rombouts: I know that getline is a GNU extension and is not really portable,
   but the alternative standard functions have some real problems.
   The following substitute does not have exactly the same semantics as the GNU getline,
   but it should be good enough, as long as the stream doesn't contain any null chars.
   This version is actually based on fgets_realloc() that I found in the WWWOFFLE source.
*/

#define BUFSIZE 256
int getline(char **lineptr, size_t *n, FILE *stream)
{
	char *line=*lineptr;
	size_t sz=*n,i;

	if(!line || sz<BUFSIZE) {
		sz=BUFSIZE;
		line = realloc(line,sz);
		if(!line) return -1;
		*lineptr=line;
		*n=sz;
	}

	for (i=0;;) {
		char *lni;

		if(!(lni=fgets(line+i,sz-i,stream))) {
			if(i && feof(stream))
				break;
			else
				return -1;
		}

		i += strlen(lni);

		if(i<sz-1 || line[i-1]=='\n')
			break;

		sz += BUFSIZE;
		line = realloc(line,sz);
		if(!line) return -1;
		*lineptr=line;
		*n=sz;
	}

	return i;
}
#undef BUFSIZE
#endif


#ifndef HAVE_VASPRINTF
/* On systems where the macro va_copy is not available, hopefully simple assignment will work.
   Otherwise, I don't see any portable way of defining vasprintf() (without using a completely
   different algorithm).
*/
#ifndef va_copy
# ifdef __va_copy
#  define va_copy __va_copy
# else
#  warning "No va_copy or __va_copy macro found, trying simple assignment."
#  define va_copy(dst,src) ((dst)=(src))
# endif
#endif

int vasprintf (char **lineptr, const char *format, va_list va)
{
	int sz=128,n;
	char *line=malloc(sz);
	va_list vasave;

	if(!line) return -1;

	va_copy(vasave,va);
	n=vsnprintf(line,sz,format,va);

	if(n>=sz) {
		/* retry with a right sized buffer, needs glibc 2.1 or higher to work */
		sz=n+1;
		{
			char *tmp=realloc(line,sz);
			if(tmp) {
				line=tmp;
				n=vsnprintf(line,sz,format,vasave);
			}
			else
				n= -1;
		}
	}
	va_end(vasave);

	if(n>=0)
		*lineptr=line;
	else
		free(line);
	return n;
}
#endif

#ifndef HAVE_ASPRINTF
int asprintf (char **lineptr, const char *format, ...)
{
	int n;
	va_list va;

	va_start(va,format);
	n=vasprintf(lineptr,format,va);
	va_end(va);

	return n;
}
#endif

#ifndef HAVE_INET_NTOP
const char *inet_ntop(int af, const void *src, char *dst, size_t size)
{
	const char *rc = NULL;

	if (src != NULL && dst != NULL && size > 0) {
		switch (af) {
		case AF_INET:
		{
			const unsigned char *p=src;
			int n = snprintf(dst, size, "%u.%u.%u.%u",
					 p[0],p[1],p[2],p[3]);
			if (n >= 0 && n < size) rc = dst;
		}
		break;

#ifdef AF_INET6
		case AF_INET6:
		{
			const unsigned char *p=src;
			unsigned int i,offs=0;
			for (i=0;i<16;i+=2) {
				int n=snprintf(dst+offs, size-offs,i==0?"%x":":%x", ((unsigned)p[i]<<8)|p[i+1]);
				if(n<0) return NULL;
				offs+=n;
				if(offs>=size) return NULL;
			}
			rc = dst;
		}
		break;
#endif
		}
	}

	return rc;
}
#endif
