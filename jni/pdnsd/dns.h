/* dns.h - Declarations for dns handling and generic dns functions

   Copyright (C) 2000, 2001 Thomas Moestl
   Copyright (C) 2002, 2003, 2004, 2005, 2009, 2011 Paul A. Rombouts

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


#ifndef DNS_H
#define DNS_H

#include <config.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/types.h>
#include <inttypes.h>
#include "rr_types.h"
#include "list.h"
#include "ipvers.h"

#if (TARGET==TARGET_BSD)
# if !defined(__BIG_ENDIAN)
#  if defined(BIG_ENDIAN)
#   define __BIG_ENDIAN BIG_ENDIAN
#  elif defined(_BIG_ENDIAN)
#   define __BIG_ENDIAN _BIG_ENDIAN
#  endif
# endif
# if !defined(__LITTLE_ENDIAN)
#  if defined(LITTLE_ENDIAN)
#   define __LITTLE_ENDIAN LITTLE_ENDIAN
#  elif defined(_LITTLE_ENDIAN)
#   define __LITTLE_ENDIAN _LITTLE_ENDIAN
#  endif
# endif
# if !defined(__BYTE_ORDER)
#  if defined(BYTE_ORDER)
#   define __BYTE_ORDER BYTE_ORDER
#  elif defined(_BYTE_ORDER)
#   define __BYTE_ORDER _BYTE_ORDER
#  endif
# endif
#endif

/* Deal with byte orders */
#ifndef __BYTE_ORDER
# if defined(__LITTLE_ENDIAN) && defined(__BIG_ENDIAN)
#  error Fuzzy endianness system! Both __LITTLE_ENDIAN and __BIG_ENDIAN have been defined!
# endif
# if !defined(__LITTLE_ENDIAN) && !defined(__BIG_ENDIAN)
#  error Strange Endianness-less system! Neither __LITTLE_ENDIAN nor __BIG_ENDIAN has been defined!
# endif
# if defined(__LITTLE_ENDIAN)
#  define __BYTE_ORDER __LITTLE_ENDIAN
# elif defined(__BIG_ENDIAN)
#  define __BYTE_ORDER __BIG_ENDIAN
# endif
#endif

/* special rr type codes for queries */
#define QT_MIN    251
#define QT_IXFR   251
#define QT_AXFR   252
#define QT_MAILB  253
#define QT_MAILA  254
#define QT_ALL    255
#define QT_MAX    255
#define QT_NUM      5

/* rr classes */
#define C_MIN       1
#define C_IN        1
#define C_CS        2
#define C_CH        3
#define C_HS        4
#define C_MAX       4
#define C_NUM       4

/* special classes for queries */
#define QC_ALL    255

/* status codes */
#define RC_OK       0
#define RC_FORMAT   1
#define RC_SERVFAIL 2
#define RC_NAMEERR  3
#define RC_NOTSUPP  4
#define RC_REFUSED  5
#define RC_BADVERS 16

/*
 * special internal retvals
 */
#define RC_NOTCACHED  0xfffa
#define RC_CACHED     0xfffb
#define RC_STALE      0xfffc
#define RC_TCPREFUSED 0xfffd
#define RC_TRUNC      0xfffe
#define RC_FATALERR   0xffff

/* query/response */
#define QR_QUERY    0
#define QR_RESP     1

/*opcodes */
#define OP_QUERY    0
#define OP_IQUERY   1
#define OP_STATUS   2

#if 0
typedef struct {
	/* the name is the first field. It has variable length, so it can't be put in the struct */
	uint16_t type;
	uint16_t class;
	uint32_t ttl;
	uint16_t rdlength;
	/* rdata follows */
} __attribute__((packed)) rr_hdr_t;

#define sizeof_rr_hdr_t (sizeof rr_hdr_t)
#else

/* We will not actually use the rr_hdr_t type, only its size:
   sizeof(rr_hdr_t) = 2 + 2 + 4 + 2 */
#define sizeof_rr_hdr_t 10
#endif

#define sizeof_opt_pseudo_rr  (1+sizeof_rr_hdr_t)

#if 0
typedef struct {
	/* The server name and maintainer mailbox are the first two fields. It has variable length, */
	/* so they can't be put in the struct */
	uint32_t serial;
	uint32_t refresh;
	uint32_t retry;
	uint32_t expire;
	uint32_t minimum;
} __attribute__((packed)) soa_r_t;


typedef struct {
/*	char           qname[];*/
	uint16_t qtype;
	uint16_t qclass;
} __attribute__((packed)) std_query_t;
#endif


typedef struct {
	uint16_t id;
#if __BYTE_ORDER == __LITTLE_ENDIAN
	unsigned int   rd:1;
	unsigned int   tc:1;
	unsigned int   aa:1;
	unsigned int   opcode:4;
	unsigned int   qr:1;
	unsigned int   rcode:4;
	unsigned int   cd:1;
	unsigned int   ad:1;
	unsigned int   z :1;
	unsigned int   ra:1;
#elif __BYTE_ORDER == __BIG_ENDIAN
	unsigned int   qr:1;
	unsigned int   opcode:4;
	unsigned int   aa:1;
	unsigned int   tc:1;
	unsigned int   rd:1;
	unsigned int   ra:1;
	unsigned int   z :1;
	unsigned int   ad:1;
	unsigned int   cd:1;
	unsigned int   rcode:4;
#else
# error	"Please define __BYTE_ORDER to be __LITTLE_ENDIAN or __BIG_ENDIAN"
#endif
	uint16_t qdcount;
	uint16_t ancount;
	uint16_t nscount;
	uint16_t arcount;
} __attribute__((packed)) dns_hdr_t;


/* A structure that can also be used for DNS messages over TCP. */
typedef struct {
#ifndef NO_TCP_QUERIES
	uint16_t len;
#endif
	dns_hdr_t hdr;
} __attribute__((packed)) dns_msg_t;

#ifdef NO_TCP_QUERIES
# define dnsmsghdroffset 0
#else
# define dnsmsghdroffset 2
#endif


/* Structure for storing EDNS (Extension mechanisms for DNS) information. */
typedef struct {
	unsigned short udpsize;
	unsigned short rcode;
	unsigned short version;
	unsigned char  do_flg;
} edns_info_t;


/* Macros to retrieve or store integer data that is not necessarily aligned.
   Also takes care of network to host byte order.
   The pointer cp is advanced and should be of type void* or char*.
   These are actually adapted versions of the NS_GET16 and NS_GET32
   macros in the arpa/nameser.h include file in the BIND 9 source.
*/

#define GETINT16(s,cp) do {							\
	register uint16_t t_s;							\
	register const unsigned char *t_cp = (const unsigned char *)(cp);	\
	t_s = (uint16_t)*t_cp++ << 8;						\
	t_s |= (uint16_t)*t_cp++;						\
	(s) = t_s;								\
	(cp) = (void *)t_cp;							\
} while (0)

#define GETINT32(l,cp) do {							\
	register uint32_t t_l;							\
	register const unsigned char *t_cp = (const unsigned char *)(cp);	\
	t_l = (uint32_t)*t_cp++ << 24;						\
	t_l |= (uint32_t)*t_cp++ << 16;						\
	t_l |= (uint32_t)*t_cp++ << 8;						\
	t_l |= (uint32_t)*t_cp++;						\
	(l) = t_l;								\
	(cp) = (void *)t_cp;							\
} while (0)

#define PUTINT16(s,cp) do {					\
	register uint16_t t_s = (uint16_t)(s);			\
	register unsigned char *t_cp = (unsigned char *)(cp);	\
	*t_cp++ = t_s >> 8;					\
	*t_cp++ = t_s;						\
	(cp) = (void *)t_cp;					\
} while (0)

#define PUTINT32(l,cp) do {					\
	register uint32_t t_l = (uint32_t)(l);			\
	register unsigned char *t_cp = (unsigned char *)(cp);	\
	*t_cp++ = t_l >> 24;					\
	*t_cp++ = t_l >> 16;					\
	*t_cp++ = t_l >> 8;					\
	*t_cp++ = t_l;						\
	(cp) = (void *)t_cp;					\
} while (0)


/* Size (number of bytes) of buffers used to hold domain names. */
#define DNSNAMEBUFSIZE 256

/* Recursion depth. */
#define MAX_HOPS 20

/*
 * Types for compression buffers.
 */
typedef struct {
	unsigned int  index;
	unsigned char s[0];
} compel_t;



int decompress_name(unsigned char *msg, size_t msgsz, unsigned char **src, size_t *sz, unsigned char *tgt, unsigned int *len);
/* int domain_name_match(const unsigned char *ms, const unsigned char *md, int *os, int *od); */
unsigned int domain_match(const unsigned char *ms, const unsigned char *md, unsigned int *os, unsigned int *od);
unsigned int compress_name(unsigned char *in, unsigned char *out, unsigned int offs, dlist *cb);
int a2ptrstr(pdnsd_ca *a, int tp, unsigned char *buf);
int read_hosts(const char *fn, unsigned char *rns, time_t ttl, unsigned flags, int aliases, char **errstr);

const char *getrrtpname(int tp);
#if DEBUG>0
const char *get_cname(int id);
const char *get_tname(int id);
const char *get_ename(int id);
#define DNSFLAGSMAXSTRSIZE (7*3+1)
char *dnsflags2str(dns_hdr_t *hdr, char *buf);
#endif

#if DEBUG>=9
void debug_dump_dns_msg(void *data, size_t len);
#define DEBUG_DUMP_DNS_MSG(d,l) {if(debug_p && global.verbosity>=9) debug_dump_dns_msg(d,l);}
#else
#define DEBUG_DUMP_DNS_MSG(d,l)
#endif

#endif
