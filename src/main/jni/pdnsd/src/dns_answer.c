/* dns_answer.c - Receive and process incoming dns queries.

   Copyright (C) 2000, 2001 Thomas Moestl
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2009, 2010, 2011 Paul A. Rombouts

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

/*
 * STANDARD CONFORMITY
 *
 * There are several standard conformity issues noted in the comments.
 * Some additional comments:
 *
 * I always set RA but I ignore RD largely (in everything but CNAME recursion),
 * not because it is not supported, but because I _always_ do a recursive
 * resolve in order to be able to cache the results.
 */

#include <config.h>
#include "ipvers.h"
#include <pthread.h>
#include <sys/uio.h>
#include <sys/types.h>
#ifdef HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif
#include <sys/param.h>
#include <netdb.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include "thread.h"
#include "list.h"
#include "dns.h"
#include "dns_answer.h"
#include "dns_query.h"
#include "helpers.h"
#include "cache.h"
#include "error.h"
#include "debug.h"


/*
 * This is for error handling to prevent spewing the log files.
 * Maximums of different message types are set.
 * Races do not really matter here, so no locks.
 */
#define TCP_MAX_ERRS 10
#define UDP_MAX_ERRS 10
#define MEM_MAX_ERRS 10
#define THRD_MAX_ERRS 10
#define MISC_MAX_ERRS 10
static volatile unsigned long da_tcp_errs=0;
static volatile unsigned long da_udp_errs=0;
static volatile unsigned long da_mem_errs=0;
static volatile unsigned long da_thrd_errs=0;
#if DEBUG>0
static volatile unsigned long da_misc_errs=0;
#endif
static volatile int procs=0;   /* active query processes */
static volatile int qprocs=0;  /* queued query processes */
static volatile unsigned long dropped=0,spawned=0;
static volatile unsigned thrid_cnt=0;
static pthread_mutex_t proc_lock = PTHREAD_MUTEX_INITIALIZER;

#ifdef SOCKET_LOCKING
static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

typedef union {
#ifdef ENABLE_IPV4
# if (TARGET==TARGET_LINUX)
	struct in_pktinfo   pi4;
# else
	struct in_addr      ai4;
# endif
#endif
#ifdef ENABLE_IPV6
	struct in6_pktinfo  pi6;
#endif
} pkt_info_t;


typedef struct {
	union {
#ifdef ENABLE_IPV4
		struct sockaddr_in  sin4;
#endif
#ifdef ENABLE_IPV6
		struct sockaddr_in6 sin6;
#endif
	}                  addr;

	pkt_info_t         pi;

	int                sock;
	int                proto;
	size_t             len;
	unsigned char      buf[0];  /* Actual size determined by global.udpbufsize */
} udp_buf_t;


/* ALLOCINITIALSIZE should be at least sizeof(dns_msg_t) = 2+12 */
#define ALLOCINITIALSIZE 256
/* This mask corresponds to a chunk size of 128 bytes. */
#define ALLOCCHUNKSIZEMASK ((size_t)0x7f)

typedef struct {
	unsigned short qtype;
	unsigned short qclass;
	unsigned char  query[0];
} dns_queryel_t;


#define S_ANSWER     1
#define S_AUTHORITY  2
#define S_ADDITIONAL 3

typedef struct {
	unsigned short tp,dlen;
	unsigned char nm[0];
	/* unsigned char data[0]; */
} sva_t;


/*
 * Mark an additional record as added to avoid double records.
 */
static int sva_add(dlist *sva, const unsigned char *rhn, unsigned short tp, unsigned short dlen, void* data)
{
	if (sva) {
		size_t rlen=rhnlen(rhn);
		sva_t *st;
		if (!(*sva=dlist_grow(*sva,sizeof(sva_t)+rlen+dlen))) {
			return 0;
		}
		st=dlist_last(*sva);
		st->tp=tp;
		st->dlen=dlen;
		memcpy(mempcpy(st->nm,rhn,rlen),data,dlen);
	}
	return 1;
}

/* ans_ttl computes the ttl value to return to the client.
   This is the ttl value stored in the cache entry minus the time
   the cache entry has lived in the cache.
   Local cache entries are an exception, they never "age".
*/
inline static time_t ans_ttl(rr_set_t *rrset, time_t queryts)
{
	time_t ttl= rrset->ttl;

	if (!(rrset->flags&CF_LOCAL)) {
		time_t tpassed= queryts - rrset->ts;
		if(tpassed<0) tpassed=0;
		ttl -= tpassed;
		if(ttl<0) ttl=0;
	}
	return ttl;
}

/* follow_cname_chain takes a cache entry and a buffer (must be at least DNSNAMEBUFSIZE bytes),
   and copies the name indicated by the first cname record in the cache entry.
   The name is returned in length-byte string notation.
   follow_cname_chain returns 1 if a cname record is found, otherwise 0.
*/
inline static int follow_cname_chain(dns_cent_t *c, unsigned char *name)
{
	rr_set_t *rrset=getrrset_CNAME(c);
	rr_bucket_t *rr;
	if (!rrset || !(rr=rrset->rrs))
		return 0;
	PDNSD_ASSERT(rr->rdlen <= DNSNAMEBUFSIZE, "follow_cname_chain: record too long");
	memcpy(name,rr->data,rr->rdlen);
	return 1;
}


/*
 * Add data from a rr_bucket_t (as in cache) into a dns message in ans. Ans is grown
 * to fit, sz is the old size of the packet (it is modified so at the end of the procedure
 * it is the new size), type is the rr type and ltime is the time in seconds the record is
 * old.
 * cb is the buffer used for message compression. *cb should be NULL when you call compress_name
 * or add_to_response the first time.
 * It gets filled with a pointer to compression information that can be reused in subsequent calls
 * to add_to_response.
 * sect is the section (S_ANSWER, S_AUTHORITY or S_ADDITIONAL) in which the record
 * belongs logically. Note that you still have to add the rrs in the right order (answer rrs first,
 * then authority and last additional).
 */
static int add_rr(dns_msg_t **ans, size_t *sz, size_t *allocsz,
		  unsigned char *rrn, unsigned short type, uint32_t ttl,
		  unsigned int dlen, void *data, char section, unsigned *udp, dlist *cb)
{
	size_t osz= *sz;
	unsigned int ilen,blen,rdlen;
	unsigned char *rrht;

	{
		unsigned int nlen;
		unsigned char nbuf[DNSNAMEBUFSIZE];

		if (!(nlen=compress_name(rrn,nbuf,*sz,cb)))
			return 0;

		/* This buffer is usually over-allocated due to compression.
		   Never mind, just a few bytes, and the buffer is freed soon. */
		{
			size_t newsz= dnsmsghdroffset + *sz + nlen + sizeof_rr_hdr_t + dlen;
			if(newsz > *allocsz) {
				/* Need to allocate more space.
				   To avoid frequent reallocs, we allocate
				   a multiple of a certain chunk size. */
				size_t newallocsz= (newsz+ALLOCCHUNKSIZEMASK)&(~ALLOCCHUNKSIZEMASK);
				dns_msg_t *newans=(dns_msg_t *)pdnsd_realloc(*ans,newallocsz);
				if (!newans)
					return 0;
				*ans=newans;
				*allocsz=newallocsz;
			}
		}
		memcpy((unsigned char *)(&(*ans)->hdr)+ *sz, nbuf, nlen);
		*sz += nlen;
	}

	/* the rr header will be filled in later. Just reserve some space for it. */
	rrht= ((unsigned char *)(&(*ans)->hdr)) + *sz;
	*sz += sizeof_rr_hdr_t;

	switch (type) {
	case T_CNAME:
	case T_MB:
	case T_MD:
	case T_MF:
	case T_MG:
	case T_MR:
	case T_NS:
	case T_PTR:
		if (!(rdlen=compress_name(((unsigned char *)data), ((unsigned char *)(&(*ans)->hdr))+(*sz),*sz,cb)))
			return 0;
		PDNSD_ASSERT(rdlen <= dlen, "T_CNAME/T_MB/...: got longer");
		*sz+=rdlen;
		break;
#if IS_CACHED_MINFO || IS_CACHED_RP
#if IS_CACHED_MINFO
	case T_MINFO:
#endif
#if IS_CACHED_RP
	case T_RP:
#endif
		if (!(rdlen=compress_name(((unsigned char *)data), ((unsigned char *)(&(*ans)->hdr))+(*sz),*sz,cb)))
			return 0;
		*sz+=rdlen;
		ilen=rhnlen((unsigned char *)data);
		PDNSD_ASSERT(rdlen <= ilen, "T_MINFO/T_RP: got longer");
		if (!(blen=compress_name(((unsigned char *)data)+ilen, ((unsigned char *)(&(*ans)->hdr))+(*sz),*sz,cb)))
			return 0;
		rdlen+=blen;
		PDNSD_ASSERT(rdlen <= dlen, "T_MINFO/T_RP: got longer");
		*sz+=blen;
		break;
#endif
	case T_MX:
#if IS_CACHED_AFSDB
	case T_AFSDB:
#endif
#if IS_CACHED_RT
	case T_RT:
#endif
#if IS_CACHED_KX
	case T_KX:
#endif
		PDNSD_ASSERT(dlen > 2, "T_MX/T_AFSDB/...: rr botch");
		memcpy(((unsigned char *)(&(*ans)->hdr))+(*sz),(unsigned char *)data,2);
		*sz+=2;
		if (!(blen=compress_name(((unsigned char *)data)+2, ((unsigned char *)(&(*ans)->hdr))+(*sz),*sz,cb)))
			return 0;
		rdlen=2+blen;
		PDNSD_ASSERT(rdlen <= dlen, "T_MX/T_AFSDB/...: got longer");
		*sz+=blen;
		break;
	case T_SOA:
		if (!(rdlen=compress_name(((unsigned char *)data), ((unsigned char *)(&(*ans)->hdr))+(*sz),*sz,cb)))
			return 0;
		*sz+=rdlen;
		ilen=rhnlen((unsigned char *)data);
		PDNSD_ASSERT(rdlen <= ilen, "T_SOA: got longer");
		if (!(blen=compress_name(((unsigned char *)data)+ilen, ((unsigned char *)(&(*ans)->hdr))+(*sz),*sz,cb)))
			return 0;
		rdlen+=blen;
		*sz+=blen;
		ilen+=rhnlen(((unsigned char *)data)+ilen);
		PDNSD_ASSERT(rdlen <= ilen, "T_SOA: got longer");
		memcpy(((unsigned char *)(&(*ans)->hdr))+(*sz),((unsigned char *)data)+ilen,20);
		rdlen+=20;
		PDNSD_ASSERT(rdlen <= dlen, "T_SOA: rr botch");
		*sz+=20;
		break;
#if IS_CACHED_PX
	case T_PX:
		PDNSD_ASSERT(dlen > 2, "T_PX: rr botch");
		memcpy(((unsigned char *)(&(*ans)->hdr))+(*sz),(unsigned char *)data,2);
		*sz+=2;
		ilen=2;
		if (!(blen=compress_name(((unsigned char *)data)+ilen, ((unsigned char *)(&(*ans)->hdr))+(*sz),*sz,cb)))
			return 0;
		rdlen=2+blen;
		*sz+=blen;
		ilen+=rhnlen(((unsigned char *)data)+ilen);
		PDNSD_ASSERT(rdlen <= ilen, "T_PX: got longer");
		if (!(blen=compress_name(((unsigned char *)data)+ilen, ((unsigned char *)(&(*ans)->hdr))+(*sz),*sz,cb)))
			return 0;
		rdlen+=blen;
		PDNSD_ASSERT(rdlen <= dlen, "T_PX: got longer");
		*sz+=blen;
		break;
#endif
#if IS_CACHED_SRV
	case T_SRV:
		PDNSD_ASSERT(dlen > 6, "T_SRV: rr botch");
		memcpy(((unsigned char *)(&(*ans)->hdr))+(*sz),(unsigned char *)data,6);
		*sz+=6;
		if (!(blen=compress_name(((unsigned char *)data)+6, ((unsigned char *)(&(*ans)->hdr))+(*sz),*sz,cb)))
			return 0;
		rdlen=6+blen;
		PDNSD_ASSERT(rdlen <= dlen, "T_SRV: got longer");
		*sz+=blen;
		break;
#endif
#if IS_CACHED_NXT
	case T_NXT:
		if (!(blen=compress_name(((unsigned char *)data), ((unsigned char *)(&(*ans)->hdr))+(*sz),*sz,cb)))
			return 0;
		rdlen=blen;
		*sz+=blen;
		ilen=rhnlen((unsigned char *)data);
		PDNSD_ASSERT(rdlen <= ilen, "T_NXT: got longer");
		PDNSD_ASSERT(dlen >= ilen, "T_NXT: rr botch");
		if (dlen > ilen) {
			unsigned int wlen = dlen - ilen;
			memcpy(((unsigned char *)(&(*ans)->hdr))+(*sz),((unsigned char *)data)+ilen,wlen);
			*sz+=wlen;
			rdlen+=wlen;
		}
		break;
#endif
#if IS_CACHED_NAPTR
	case T_NAPTR:
		PDNSD_ASSERT(dlen > 4, "T_NAPTR: rr botch");
		ilen=4;
		{
			int j;
			for (j=0;j<3;j++) {
				ilen += ((unsigned)*(((unsigned char *)data)+ilen)) + 1;
				PDNSD_ASSERT(dlen > ilen, "T_NAPTR: rr botch 2");
			}
		}
		memcpy(((unsigned char *)(&(*ans)->hdr))+(*sz),((unsigned char *)data),ilen);
		(*sz)+=ilen;

		if (!(blen=compress_name(((unsigned char *)data)+ilen, ((unsigned char *)(&(*ans)->hdr))+(*sz),*sz,cb)))
			return 0;
		rdlen=ilen+blen;
		PDNSD_ASSERT(rdlen <= dlen, "T_NAPTR: got longer");
		*sz+=blen;
		break;
#endif
	default:
		memcpy(((unsigned char *)(&(*ans)->hdr))+(*sz),((unsigned char *)data),dlen);
		rdlen=dlen;
		*sz+=dlen;
	}

	if (udp && *sz>*udp && section==S_ADDITIONAL) /* only add the record if we do not increase the length over 512 */
		*sz=osz;                              /* (or possibly more if the request used EDNS) in additionals for udp answer. */
	else {
		PUTINT16(type,rrht);
		PUTINT16(C_IN,rrht);
		PUTINT32(ttl,rrht);
		PUTINT16(rdlen,rrht);

		switch (section) {
		case S_ANSWER:
			(*ans)->hdr.ancount=htons(ntohs((*ans)->hdr.ancount)+1);
			break;
		case S_AUTHORITY:
			(*ans)->hdr.nscount=htons(ntohs((*ans)->hdr.nscount)+1);
			break;
		case S_ADDITIONAL:
			(*ans)->hdr.arcount=htons(ntohs((*ans)->hdr.arcount)+1);
			break;
		}
	}

	return 1;
}

/* Add an OPT pseudo RR containing EDNS info.
   Can only be added to the additional section!
*/
int add_opt_pseudo_rr(dns_msg_t **ans, size_t *sz, size_t *allocsz,
		      unsigned short udpsize, unsigned short rcode,
		      unsigned short ednsver, unsigned short Zflags)
{
	unsigned char *ptr;
	size_t newsz= dnsmsghdroffset + *sz + sizeof_opt_pseudo_rr;
	if(newsz > *allocsz) {
		/* Need to allocate more space.
		   To avoid frequent reallocs, we allocate
		   a multiple of a certain chunk size. */
		size_t newallocsz= (newsz+ALLOCCHUNKSIZEMASK)&(~ALLOCCHUNKSIZEMASK);
		dns_msg_t *newans=(dns_msg_t *)pdnsd_realloc(*ans,newallocsz);
		if (!newans)
			return 0;
		*ans=newans;
		*allocsz=newallocsz;
	}

	ptr= ((unsigned char *)(&(*ans)->hdr)) + *sz;
	*ptr++ = 0;             /* Empty name */
	PUTINT16(T_OPT,ptr);    /* type field */
	PUTINT16(udpsize,ptr);  /* class field */
	*ptr++ = rcode>>4;      /* 4 byte TTL field */
	*ptr++ = ednsver;
	PUTINT16(Zflags,ptr);
	PUTINT16(0,ptr);        /* rdlen field */
        /* Empty RDATA. */

	*sz += sizeof_opt_pseudo_rr;
	/* Increment arcount field in dns header. */
	(*ans)->hdr.arcount = htons(ntohs((*ans)->hdr.arcount)+1);
	return 1;
}

/* Remove the last entry in the additional section,
   assuming it is an OPT pseudo RR of fixed size.
   Returns the new message size if successful, or
   zero if an inconsistency is detected.
*/
size_t remove_opt_pseudo_rr(dns_msg_t *ans, size_t sz)
{
	uint16_t acnt=ntohs(ans->hdr.arcount), type;
	unsigned char *ptr;
	/* First do some sanity checks. */
	if(!(acnt>0 && sz >= sizeof(dns_hdr_t)+sizeof_opt_pseudo_rr))
		return 0;
	sz -= sizeof_opt_pseudo_rr;
	ptr= ((unsigned char *)(&ans->hdr)) + sz;
	if(*ptr++)
		return 0;  /* Name must be empty. */
	GETINT16(type,ptr);
	if(type!=T_OPT)
		return 0;  /* RR type must be OPT. */
	/* Decrement arcount field in dns header. */
	ans->hdr.arcount = htons(acnt-1);
	return sz;
}

typedef struct rre_s {
	unsigned short tp;
	unsigned short tsz;		/* Size of tnm field */
	uint32_t       ttl;		/* ttl of the record in the answer (if tp==T_NS or T_SOA) */
	unsigned char  tnm[0];		/* Name for the domain a record refers to */
	/* unsigned char  nm[0]; */	/* Name of the domain the record is for (if tp==T_NS or T_SOA) */
} rr_ext_t;


/* types for the tp field */
/* #define RRETP_NS	T_NS */		/* For name server: add to authority, add address to additional. */
/* #define RRETP_SOA	T_SOA */	/* For SOA record: add to authority. */
#define RRETP_ADD	0		/* For other records: add the address of buf to additional */

static int add_ar(dlist *ar,unsigned short tp, unsigned short tsz,void *tnm,unsigned char *nm, uint32_t ttl)
{
	rr_ext_t *re;
	unsigned char *p;
	size_t nmsz=0,size=sizeof(rr_ext_t)+tsz;
	if(tp==T_NS || tp==T_SOA) {
		nmsz=rhnlen(nm);
		size += nmsz;
	}
	if (!(*ar=dlist_grow(*ar,size)))
		return 0;
	re=dlist_last(*ar);
	re->tp=tp;
	re->tsz=tsz;
	re->ttl=ttl;
	p=mempcpy(re->tnm,tnm,tsz);
	if(tp==T_NS || tp==T_SOA) {
		memcpy(p,nm,nmsz);
	}
	return 1;
}


/* Select a random rr record from a list. */
inline static rr_bucket_t *randrr(rr_bucket_t *rrb)
{
	rr_bucket_t *rr;
	unsigned cnt=0;

	/* In order to have an equal chance for each record to be selected, we have to count first. */
	for(rr=rrb; rr; rr=rr->next) ++cnt;

	/* We do not use the pdnsd random functions (these might use /dev/urandom if the user is paranoid,
	 * and we do not need any good PRNG here). */
	if(cnt) for(cnt=random()%cnt; cnt; --cnt) rrb=rrb->next;

	return rrb;
}

#if IS_CACHED_SRV
#define AR_NUM 6
#else
#define AR_NUM 5
#endif
static const int ar_recs[AR_NUM]={T_NS, T_MD, T_MF, T_MB, T_MX
#if IS_CACHED_SRV
				  ,T_SRV
#endif
};
/* offsets from record data start to server name */
static const int ar_offs[AR_NUM]={0,0,0,0,2
#if IS_CACHED_SRV
				  ,6
#endif
};

/* This adds an rrset, optionally randomizing the first element it adds.
 * if that is done, all rrs after the randomized one appear in order, starting from
 * that one and wrapping over if needed. */
static int add_rrset(dns_msg_t **ans, size_t *sz, size_t *allocsz,
		     unsigned char *rrn, unsigned tp, time_t queryts,
		     dns_cent_t *cached, unsigned *udp, dlist *cb, dlist *sva, dlist *ar)
{
	rr_set_t *crrset=getrrset(cached,tp);

	if (crrset && crrset->rrs) {
		rr_bucket_t *b;
		rr_bucket_t *first=NULL; /* Initialized to inhibit compiler warning */
		int i;
		short rnd_recs=global.rnd_recs;

		b=crrset->rrs;
		if (rnd_recs) b=first=randrr(crrset->rrs);

		while (b) {
			if (!add_rr(ans, sz, allocsz, rrn, tp, ans_ttl(crrset,queryts),
				    b->rdlen, b->data, S_ANSWER, udp, cb))
				return 0;
			if (tp==T_NS || tp==T_A || tp==T_AAAA) {
				/* mark it as added */
				if (!sva_add(sva,rrn,tp,b->rdlen,b->data))
					return 0;
			}
			/* Mark for additional address records. XXX: this should be a more effective algorithm; at least the list is small */
			for (i=0;i<AR_NUM;i++) {
				if (ar_recs[i]==tp) {
					if (!add_ar(ar, RRETP_ADD,b->rdlen-ar_offs[i],((unsigned char *)(b->data))+ar_offs[i],
						    ucharp "", 0))
						return 0;
					break;
				}
			}
			b=b->next;
			if (rnd_recs) {
				if(!b) b=crrset->rrs; /* wraparound */
				if(b==first)	break;
			}
		}
	}
	return 1;
}

/*
 * Add the fitting elements of the cached record to the message in ans, where ans
 * is grown to fit, sz is the size of the packet and is modified to be the new size.
 * The query is in qe.
 * cb is the buffer used for message compression. *cb should be NULL if you call add_to_response
 * the first time. It gets filled with a pointer to compression information that can be
 * reused in subsequent calls to add_to_response.
 */
static int add_to_response(dns_msg_t **ans, size_t *sz, size_t *allocsz,
			   unsigned char *rrn, unsigned qtype, time_t queryts,
			   dns_cent_t *cached, unsigned *udp, dlist *cb, dlist *sva, dlist *ar)
{
	/* First of all, unless we have records of qtype, add cnames.
	   Well, actually, there should be at max one cname. */
	if (qtype!=T_CNAME && qtype!=QT_ALL && !(qtype>=T_MIN && qtype<=T_MAX && have_rr(cached,qtype)))
		if (!add_rrset(ans, sz, allocsz, rrn, T_CNAME, queryts, cached, udp, cb, sva, ar))
			return 0;

	/* We need no switch for qclass, since we already have filtered packets we cannot understand */
	if (qtype==QT_AXFR || qtype==QT_IXFR) {
		/* I do not know what to do in this case. Since we do not maintain zones (and since we are
		   no master server, so it is not our task), I just return an error message. If anyone
		   knows how to do this better, please notify me.
		   Anyway, this feature is rarely used in client communication, and there is no need for
		   other name servers to ask pdnsd. Btw: many bind servers reject an ?XFR query for security
		   reasons. */
		return 0;
	} else if (qtype==QT_MAILB) {
		if (!add_rrset(ans, sz, allocsz, rrn, T_MB, queryts, cached, udp, cb, sva, ar))
			return 0;
		if (!add_rrset(ans, sz, allocsz, rrn, T_MG, queryts, cached, udp, cb, sva, ar))
			return 0;
		if (!add_rrset(ans, sz, allocsz, rrn, T_MR, queryts, cached, udp, cb, sva, ar))
			return 0;
	} else if (qtype==QT_MAILA) {
		if (!add_rrset(ans, sz, allocsz, rrn, T_MD, queryts, cached, udp, cb, sva, ar))
			return 0;
		if (!add_rrset(ans, sz, allocsz, rrn, T_MF, queryts, cached, udp, cb, sva, ar))
			return 0;
	} else if (qtype==QT_ALL) {
		int i, n= NRRITERLIST(cached);
		const unsigned short *iterlist= RRITERLIST(cached);
		for (i=0; i<n; ++i) {
			if (!add_rrset(ans, sz, allocsz, rrn, iterlist[i], queryts, cached, udp, cb, sva, ar))
				return 0;
		}
	} else if (qtype>=T_MIN && qtype<=T_MAX) {
		if (!add_rrset(ans, sz, allocsz, rrn, qtype, queryts, cached, udp, cb, sva, ar))
			return 0;
	} else /* Shouldn't get here. */
		return 0;
#if 0
	if (!ntohs((*ans)->hdr.ancount)) {
		/* Add a SOA if we have one and no other records are present in the answer.
		 * This is to aid caches so that they have a ttl. */
		if (!add_rrset(ans, sz, allocsz, rrn, T_SOA , queryts, cached, udp, cb, sva, ar))
			return 0;
	}
#endif
	return 1;
}

/*
 * Add an additional
 */
static int add_additional_rr(dns_msg_t **ans, size_t *rlen, size_t *allocsz,
			     unsigned char *rhn, unsigned tp, time_t ttl,
			     unsigned dlen, void *data, int sect, unsigned *udp, dlist *cb, dlist *sva)
{
	sva_t *st;

	/* Check if already added; no double additionals */
	for (st=dlist_first(*sva); st; st=dlist_next(st)) {
		if (st->tp==tp && rhnicmp(st->nm,rhn) && st->dlen==dlen &&
		    (memcmp(skiprhn(st->nm),data, dlen)==0))
		{
			return 1;
		}
	}
	/* add_rr will do nothing when udp!=NULL and sz>*udp. */
	if(!add_rr(ans, rlen, allocsz, rhn, tp, ttl, dlen, data, sect, udp, cb))
		return 0;
	/* mark it as added */
	if (!sva_add(sva,rhn,tp,dlen,data))
		return 0;

	return 1;
}

/*
 * Add one or more additionals from an rr bucket.
 */
static int add_additional_rrs(dns_msg_t **ans, size_t *rlen, size_t *allocsz,
			      unsigned char *rhn, unsigned tp, time_t ttl,
			      rr_bucket_t *rrb, int sect, unsigned *udp, dlist *cb, dlist *sva)
{
	rr_bucket_t *rr;
	rr_bucket_t *first=NULL; /* Initialized to inhibit compiler warning */
	short rnd_recs=global.rnd_recs;

	rr=rrb;
	if (rnd_recs) rr=first=randrr(rrb);

	while(rr) {
		if (!add_additional_rr(ans, rlen, allocsz, rhn, tp, ttl, rr->rdlen,rr->data, sect, udp, cb, sva))
			return 0;
		rr=rr->next;
		if (rnd_recs) {
			if(!rr) rr=rrb; /* wraparound */
			if(rr==first)	break;
		}
	}
	return 1;
}

/*
 * The code below actually handles A and AAAA additionals.
 */
static int add_additional_a(dns_msg_t **ans, size_t *rlen, size_t *allocsz,
			    unsigned char *rhn, time_t queryts,
			    unsigned *udp, dlist *cb, dlist *sva)
{
	dns_cent_t *ae;
	int retval = 1;

	if ((ae=lookup_cache(rhn,NULL))) {
		rr_set_t *rrset; rr_bucket_t *rr;
		rrset=getrrset_A(ae);
		if (rrset && (rr=rrset->rrs))
			if (!add_additional_rrs(ans, rlen, allocsz,
						rhn, T_A, ans_ttl(rrset,queryts),
						rr, S_ADDITIONAL, udp, cb, sva))
				retval = 0;

#if IS_CACHED_AAAA
		if(retval) {
			rrset=getrrset_AAAA(ae);
			if (rrset && (rr=rrset->rrs))
				if (!add_additional_rrs(ans, rlen, allocsz,
							rhn, T_AAAA, ans_ttl(rrset,queryts),
							rr, S_ADDITIONAL, udp, cb, sva))
					retval = 0;
		}
#endif
		free_cent(ae  DBG1);
		pdnsd_free(ae);
	}
	return retval;
}

/*
 * Compose an answer message for the decoded query in ql, hdr is the header of the dns request
 * rlen is set to be the answer length.
 * If udp is not NULL, *udp indicates the max length the dns response may have.
 */
static dns_msg_t *compose_answer(llist *ql, dns_hdr_t *hdr, size_t *rlen, edns_info_t *ednsinfo, unsigned *udp, int *rcodep)
{
	unsigned short rcode=RC_OK, aa=1;
	dlist cb=NULL;
	dlist sva=NULL;
	dlist ar=NULL;
	time_t queryts=time(NULL);
	dns_queryel_t *qe;
	dns_msg_t *ans;
	size_t allocsz= ALLOCINITIALSIZE;
	dns_cent_t *cached;

	ans=(dns_msg_t *)pdnsd_malloc(allocsz);
	if (!ans)
		goto return_ans;
	ans->hdr.id=hdr->id;
	ans->hdr.qr=QR_RESP;
	ans->hdr.opcode=OP_QUERY;
	ans->hdr.aa=0;
	ans->hdr.tc=0; /* If tc is needed, it is set when the response is sent in udp_answer_thread. */
	ans->hdr.rd=hdr->rd;
	ans->hdr.ra=1;
	ans->hdr.z=0;
	ans->hdr.ad=0;
	ans->hdr.cd=0;
	ans->hdr.rcode=rcode;
	ans->hdr.qdcount=0; /* this is first filled in and will be modified */
	ans->hdr.ancount=0;
	ans->hdr.nscount=0;
	ans->hdr.arcount=0;

	*rlen=sizeof(dns_hdr_t);
	/* first, add the query to the response */
	for (qe=llist_first(ql); qe; qe=llist_next(qe)) {
		unsigned int qclen;
		size_t newsz= dnsmsghdroffset + *rlen + rhnlen(qe->query) + 4;
		if(newsz > allocsz) {
			/* Need to allocate more space.
			   To avoid frequent reallocs, we allocate
			   a multiple of a certain chunk size. */
			size_t newallocsz= (newsz+ALLOCCHUNKSIZEMASK)&(~ALLOCCHUNKSIZEMASK);
			dns_msg_t *newans=(dns_msg_t *)pdnsd_realloc(ans,newallocsz);
			if (!newans)
				goto error_ans;
			ans=newans;
			allocsz=newallocsz;
		}

		{
			unsigned char *p = ((unsigned char *)&ans->hdr) + *rlen;
			/* the first name occurrence will not be compressed,
			   but the offset needs to be stored for future compressions */
			if (!(qclen=compress_name(qe->query,p,*rlen,&cb)))
				goto error_ans;
			p += qclen;
			PUTINT16(qe->qtype,p);
			PUTINT16(qe->qclass,p);
		}
		*rlen += qclen+4;
		ans->hdr.qdcount=htons(ntohs(ans->hdr.qdcount)+1);
	}

	/* Barf if we get a query we cannot answer */
	for (qe=llist_first(ql); qe; qe=llist_next(qe)) {
		if ((PDNSD_NOT_CACHED_TYPE(qe->qtype) &&
		     (qe->qtype!=QT_MAILB && qe->qtype!=QT_MAILA && qe->qtype!=QT_ALL)) ||
		    (qe->qclass!=C_IN && qe->qclass!=QC_ALL))
		{
			DEBUG_MSG("Unsupported QTYPE or QCLASS.\n");
			ans->hdr.rcode=rcode=RC_NOTSUPP;
			goto cleanup_return;
		}
	}

	/* second, the answer section */
	for (qe=llist_first(ql); qe; qe=llist_next(qe)) {
		int hops;
		unsigned char qname[DNSNAMEBUFSIZE];

		rhncpy(qname,qe->query);
		/* look if we have a cached copy. otherwise, perform a nameserver query. Same with timeout */
		hops=MAX_HOPS;
		do {
			int rc;
			unsigned char c_soa=cundef;
			if ((rc=dns_cached_resolve(qname,qe->qtype, &cached, MAX_HOPS,queryts,&c_soa))!=RC_OK) {
				ans->hdr.rcode=rcode=rc;
				if(rc==RC_NAMEERR) {
					if(c_soa!=cundef) {
						/* Try to add a SOA record to the authority section. */
						unsigned scnt=rhnsegcnt(qname);
						if(c_soa<scnt && (cached=lookup_cache(skipsegs(qname,scnt-c_soa),NULL))) {
							rr_set_t *rrset=getrrset_SOA(cached);
							if (rrset && !(rrset->flags&CF_NEGATIVE)) {
								rr_bucket_t *rr;
								for(rr=rrset->rrs; rr; rr=rr->next) {
									if (!add_rr(&ans,rlen,&allocsz,cached->qname,T_SOA,ans_ttl(rrset,queryts),
										    rr->rdlen,rr->data,S_AUTHORITY,udp,&cb))
										goto error_cached;
								}
							}
							free_cent(cached  DBG1);
							pdnsd_free(cached);
						}
					}

					/* Possibly add an OPT pseudo-RR to the additional section. */
					if(ednsinfo) {
						if(!add_opt_pseudo_rr(&ans, rlen, &allocsz, global.udpbufsize, rcode, 0,0))
							goto error_ans;
					}
				}
				goto cleanup_return;
			}
			if(!(cached->flags&DF_LOCAL))
				aa=0;

			if (!add_to_response(&ans,rlen,&allocsz,qname,qe->qtype,queryts,cached,udp,&cb,&sva,&ar))
				goto error_cached;
			if (hdr->rd && qe->qtype!=T_CNAME && qe->qtype!=QT_ALL &&
			    !(qe->qtype>=T_MIN && qe->qtype<=T_MAX && have_rr(cached,qe->qtype)) &&
			    follow_cname_chain(cached,qname))
				/* The rd bit is set and the response does not contain records of the requested type,
				 * but the response does contain a cname, so repeat the inquiry with the cname.
				 * add_to_response() has already added the cname to the response.
				 * Because of follow_cname_chain(), qname now contains the last cname in the chain. */
				;
			else {
				/* maintain a list (ar) for authority records: We will add every name server that was
				   listed as authoritative in a reply we received (and only those) to this list.
				   This list will be used to fill the authority and additional sections of our own reply.
				   We only do this for the last record in a cname chain, to prevent answer bloat. */
				rr_set_t *rrset;
				int rretp=T_NS;
				if((qe->qtype>=T_MIN && qe->qtype<=T_MAX && !have_rr(cached,qe->qtype)) ||
				   (qe->qtype==QT_MAILB && !have_rr_MB(cached) && !have_rr_MG(cached) && !have_rr_MR(cached)) ||
				   (qe->qtype==QT_MAILA && !have_rr_MD(cached) && !have_rr_MF(cached)))
				{
					/* no record of requested type in the answer section. */
					rretp=T_SOA;
				}
				rrset=getrrset(cached,rretp);
				if(rrset && (rrset->flags&CF_NEGATIVE))
					rrset=NULL;
				if(!rrset) {
					/* Try to find a name server higher up the hierarchy .
					 */
					dns_cent_t *prev=cached;
					unsigned scnt=rhnsegcnt(prev->qname);
					unsigned tcnt=(rretp==T_NS?prev->c_ns:prev->c_soa);
					if((cached=lookup_cache((tcnt!=cundef && tcnt<scnt)?skipsegs(prev->qname,scnt-tcnt):prev->qname,NULL))) {
						rrset=getrrset(cached,rretp);
						if(rrset && (rrset->flags&CF_NEGATIVE))
							rrset=NULL;
					}
					if(!rrset && (prev->flags&DF_LOCAL)) {
						unsigned char *nm=getlocalowner(prev->qname,rretp);
						if(nm) {
							if(cached) {
								free_cent(cached  DBG1);
								pdnsd_free(cached);
							}
							if((cached=lookup_cache(nm,NULL)))
								rrset=getrrset(cached,rretp);
						}
					}
					free_cent(prev  DBG1);
					pdnsd_free(prev);
				}
				if (rrset) {
					rr_bucket_t *rr;
					for (rr=rrset->rrs; rr; rr=rr->next) {
						if (!add_ar(&ar, rretp, rr->rdlen,rr->data, cached->qname,
							    ans_ttl(rrset,queryts)))
							goto error_cached;
					}
				}
				hops=0;  /* this will break the loop */
			}
			if(cached) {
				free_cent(cached  DBG1);
				pdnsd_free(cached);
			}
		} while (--hops>=0);
	}

	{
		rr_ext_t *rre;
		/* Add the authority section */
		for (rre=dlist_first(ar); rre; rre=dlist_next(rre)) {
			if (rre->tp == T_NS || rre->tp == T_SOA) {
				unsigned char *nm = rre->tnm + rre->tsz;
				if (!add_additional_rr(&ans, rlen, &allocsz,
						       nm, rre->tp, rre->ttl, rre->tsz, rre->tnm,
						       S_AUTHORITY, udp, &cb, &sva))
				{
					goto error_ans;
				}
			}
		}

		/* Add the additional section, but only if we stay within the UDP buffer limit. */
		/* If a pseudo RR doesn't fit, nothing else will. */
		if(!(udp && *rlen+sizeof_opt_pseudo_rr>*udp)) {

			/* Possibly add an OPT pseudo-RR to the additional section. */
			if(ednsinfo) {
				if(!add_opt_pseudo_rr(&ans, rlen, &allocsz, global.udpbufsize, rcode, 0,0))
					goto error_ans;
			}

			/* now add the name server addresses */
			for (rre=dlist_first(ar); rre; rre=dlist_next(rre)) {
				if (rre->tp == T_NS || rre->tp == RRETP_ADD) {
					if (!add_additional_a(&ans, rlen, &allocsz,
							      rre->tnm, queryts, udp, &cb, &sva))
						goto error_ans;
				}
			}
		}
	}
	if (aa)
		ans->hdr.aa=1;
	goto cleanup_return;

	/* You may not like goto's, but here we avoid lots of code duplication. */
error_cached:
	free_cent(cached  DBG1);
	pdnsd_free(cached);
error_ans:
	pdnsd_free(ans);
	ans=NULL;
cleanup_return:
	dlist_free(ar);
	dlist_free(sva);
	dlist_free(cb);
return_ans:
	if(rcodep) *rcodep=rcode;
	return ans;
}

/*
 * Decode the query (the query messgage is in data and rlen bytes long) into a dlist.
 * XXX: data needs to be aligned.
 * The return value can be RC_OK or RC_TRUNC, in which case the (partially) constructed list is
 * returned in qp, or something else (RC_FORMAT or RC_SERVFAIL), in which case no list is returned.
 *
 * *ptrrem  will be assigned the address just after the questions sections in the message, and *lenrem
 * the remaining message length after the questions section. These values are only meaningful if the
 * return value is RC_OK.
 */
static int decode_query(unsigned char *data, size_t rlen, unsigned char **ptrrem, size_t *lenrem, llist *qp)
{
	int i,res=RC_OK;
	dns_hdr_t *hdr=(dns_hdr_t *)data; /* aligned, so no prob. */
	unsigned char *ptr=(unsigned char *)(hdr+1);
	size_t sz= rlen - sizeof(dns_hdr_t);
	uint16_t qdcount=ntohs(hdr->qdcount);

	llist_init(qp);
	for (i=0; i<qdcount; ++i) {
		dns_queryel_t *qe;
		unsigned int qlen;
		unsigned char qbuf[DNSNAMEBUFSIZE];
		res=decompress_name(data,rlen,&ptr,&sz,qbuf,&qlen);
		if (res==RC_TRUNC)
			break;
		if (res!=RC_OK) {
			llist_free(qp);
			break;
		}
		if (sz<4) {
			/* truncated in qtype or qclass */
			DEBUG_MSG("decode_query: query truncated in qtype or qclass.\n");
			res=RC_TRUNC;
			break;
		}
		if(!llist_grow(qp,sizeof(dns_queryel_t)+qlen)) {
			res=RC_SERVFAIL;
			break;
		}
		qe=llist_last(qp);
		GETINT16(qe->qtype,ptr);
		GETINT16(qe->qclass,ptr);
		sz-=4;
		memcpy(qe->query,qbuf,qlen);
	}

	if(ptrrem) *ptrrem=ptr;
	if(lenrem) *lenrem=sz;
	return res;
}


/* Scan the additional section of a query message for an OPT pseudo RR.
   data and rlen are as in decode_query(). Note in particular that data needs to be aligned!
   ptr should point the beginning of the additional section, sz should contain the
   length of this remaining part of the message and numrr the number of resource records in the section.
   *numopt is incremented with the number of OPT RRs found (should be at most one).

   Note that a return value of RC_OK means the additional section was parsed without errors, not that
   an OPT pseudo RR was found! Check the value of *numopt for the latter.

   The structure pointed to by ep is filled with the information of the first OPT pseudo RR found,
   but only if *numopt was set to zero before the call.
*/
static int decode_query_additional(unsigned char *data, size_t rlen, unsigned char *ptr, size_t sz, int numrr,
				   int *numopt, edns_info_t *ep)
{
	int i, res;

	for (i=0; i<numrr; ++i) {
		unsigned char nmbuf[DNSNAMEBUFSIZE];
		uint16_t type,class;
		unsigned char *ttlp;
		uint16_t rdlen;
		res=decompress_name(data,rlen,&ptr,&sz,nmbuf,NULL);
		if (res!=RC_OK)
			return res;
		if (sz<sizeof_rr_hdr_t) {
			/* truncated in rr header */
			DEBUG_MSG("decode_query_additional: additional section truncated in RR header.\n");
			return RC_TRUNC;
		}
		sz -= sizeof_rr_hdr_t;
		GETINT16(type,ptr);
		GETINT16(class,ptr);
		ttlp= ptr;   /* Remember pointer to ttl field. */
		ptr += 4;    /* Skip ttl field (4 bytes). */
		GETINT16(rdlen,ptr);
		if (sz<rdlen) {
			DEBUG_MSG("decode_query_additional: additional section truncated in RDATA field.\n");
			return RC_TRUNC;
		}

		if(type==T_OPT) {
			/* Found OPT pseudo-RR */
			if((*numopt)++ == 0) {
#if DEBUG>0
				if(nmbuf[0]!=0) {
					DEBUG_MSG("decode_query_additional: name in OPT record not empty!\n");
				}
#endif
				ep->udpsize= class;
				ep->rcode= ((uint16_t)ttlp[0]<<4) | ((dns_hdr_t *)data)->rcode;
				ep->version= ttlp[1];
				ep->do_flg= (ttlp[2]>>7)&1;
#if DEBUG>0
				if(debug_p) {
					unsigned int Zflags= ((uint16_t)ttlp[2]<<8) | ttlp[3];
					if(Zflags & 0x7fff) {
						DEBUG_MSG("decode_query_additional: Z field contains unknown nonzero bits (%04x).\n",
							  Zflags);
					}
					if(rdlen) {
						DEBUG_MSG("decode_query_additional: RDATA field in OPT record not empty!\n");
					}
				}
#endif
			}
			else {
				DEBUG_MSG("decode_query_additional: ingnoring surplus OPT record.\n");
			}
		}
		else {
			DEBUG_MSG("decode_query_additional: ignoring record of type %s (%d).\n",
				  getrrtpname(type), type);
		}

		/* Skip RDATA field. */
		sz -= rdlen;
		ptr += rdlen;
	}

	return RC_OK;
}

/* Make a dns error reply message
 * Id is the query id and still in network order.
 * op is the opcode to fill in, rescode - name says it all.
 */
static void mk_error_reply(unsigned short id, unsigned short opcode,unsigned short rescode,dns_hdr_t *rep)
{
	rep->id=id;
	rep->qr=QR_RESP;
	rep->opcode=opcode;
	rep->aa=0;
	rep->tc=0;
	rep->rd=0;
	rep->ra=1;
	rep->z=0;
	rep->ad=0;
	rep->cd=0;
	rep->rcode=rescode;
	rep->qdcount=0;
	rep->ancount=0;
	rep->nscount=0;
	rep->arcount=0;
}

/*
 * Analyze and answer the query in data. The answer is returned. rlen is at call the query length and at
 * return the length of the answer. You have to free the answer after sending it.
 */
static dns_msg_t *process_query(unsigned char *data, size_t *rlenp, unsigned *udp, int *rcodep)
{
	size_t rlen= *rlenp;
	int res;
	dns_hdr_t *hdr;
	llist ql;
	dns_msg_t *ans;
	edns_info_t ednsinfo= {0}, *ednsinfop= NULL;

	DEBUG_MSG("Received query (msg len=%u).\n", (unsigned int)rlen);
	DEBUG_DUMP_DNS_MSG(data, rlen);

	/*
	 * We will ignore all records that come with a query, except for the actual query records,
	 * and possible OPT pseudo RRs in the addtional section.
	 * We will send back the query in the response. We will reject all non-queries, and
	 * some not supported thingies.
	 * If anyone notices behaviour that is not in standard conformance, please notify me!
	 */
	hdr=(dns_hdr_t *)data;
	if (rlen<2) {
		DEBUG_MSG("Message too short.\n");
		return NULL; /* message too short: no id provided. */
	}
	if (rlen<sizeof(dns_hdr_t)) {
		DEBUG_MSG("Message too short.\n");
		res=RC_FORMAT;
		goto error_reply;
	}
	if (hdr->qr!=QR_QUERY) {
		DEBUG_MSG("The QR bit indicates this is a response, not a query.\n");
		return NULL; /* RFC says: discard */
	}
	if (hdr->opcode!=OP_QUERY) {
		DEBUG_MSG("Not a standard query (opcode=%u).\n",hdr->opcode);
		res=RC_NOTSUPP;
		goto error_reply;
	}
#if DEBUG>0
	if(debug_p) {
		char flgsbuf[DNSFLAGSMAXSTRSIZE];
		dnsflags2str(hdr, flgsbuf);
		if(flgsbuf[0]) {
			DEBUG_MSG("Flags:%s\n", flgsbuf);
		}
	}
#endif
	if (hdr->z!=0) {
		DEBUG_MSG("Malformed query (nonzero Z bit).\n");
		res=RC_FORMAT;
		goto error_reply;
	}
	if (hdr->rcode!=RC_OK) {
		DEBUG_MSG("Bad rcode(%u).\n",hdr->rcode);
		return NULL; /* discard (may cause error storms) */
	}

	if (hdr->ancount) {
		DEBUG_MSG("Query has a non-empty answer section!\n");
		res=RC_FORMAT;
		goto error_reply;
	}

	if (hdr->nscount) {
		DEBUG_MSG("Query has a non-empty authority section!\n");
		res=RC_FORMAT;
		goto error_reply;
	}

#if 0
	/* The following only makes sense if we completely disallow
	   Extension Mechanisms for DNS (RFC 2671). */
	if (hdr->arcount) {
		DEBUG_MSG("Query has a non-empty additional section!\n");
		res=RC_FORMAT;
		goto error_reply;
	}
#endif
	{
		unsigned char *ptr;
		size_t sz;
		uint16_t arcount;
		res=decode_query(data,rlen,&ptr,&sz,&ql);
		if(res!=RC_OK) {
			if(res==RC_TRUNC) {
				if(!hdr->tc || llist_isempty(&ql)) {
					res=RC_FORMAT;
					goto free_ql_error_reply;
				}
			}
			else
				goto error_reply;
		}

		if ((arcount=ntohs(hdr->arcount))) {
			int numoptrr= 0;
			DEBUG_MSG("Query has a non-empty additional section: "
				  "checking for OPT pseudo-RR.\n");
			if(res==RC_TRUNC) {
				DEBUG_MSG("Additional section cannot be read due to truncation!\n");
				res=RC_FORMAT;
				goto free_ql_error_reply;
			}
			res=decode_query_additional(data,rlen,ptr,sz,arcount, &numoptrr, &ednsinfo);
			if(!(res==RC_OK || (res==RC_TRUNC && hdr->tc))) {
				res=RC_FORMAT;
				goto free_ql_error_reply;
			}
			if(numoptrr) {
#if DEBUG>0
				if(numoptrr!=1) {
					DEBUG_MSG("Additional section in query contains %d OPT pseudo-RRs!\n", numoptrr);
				}
#endif
				if(ednsinfo.version!=0) {
					DEBUG_MSG("Query contains unsupported EDNS version %d!\n", ednsinfo.version);
					res=RC_BADVERS;
					goto free_ql_error_reply;
				}
				if(ednsinfo.rcode!=0) {
					DEBUG_MSG("Query contains non-zero EDNS rcode (%d)!\n", ednsinfo.rcode);
					res=RC_FORMAT;
					goto free_ql_error_reply;
				}
				DEBUG_MSG("Query contains OPT pseudosection: EDNS udp size = %u, flag DO=%u\n",
					  ednsinfo.udpsize, ednsinfo.do_flg);
				ednsinfop = &ednsinfo;
				if(udp && ednsinfo.udpsize>UDP_BUFSIZE) {
					unsigned udpbufsize = global.udpbufsize;
					if(udpbufsize > ednsinfo.udpsize)
						udpbufsize = ednsinfo.udpsize;
					*udp = udpbufsize;
				}
			}
		}
	}

#if DEBUG>0
	if (debug_p) {
		if(!llist_isempty(&ql)) {
			dns_queryel_t *qe;
			DEBUG_MSG("Questions are:\n");
			for (qe=llist_first(&ql); qe; qe=llist_next(qe)) {
				DEBUG_RHN_MSG("\tqc=%s (%u), qt=%s (%u), query=\"%s\"\n",
					      get_cname(qe->qclass),qe->qclass,get_tname(qe->qtype),qe->qtype,RHN2STR(qe->query));
			}
		}
		else {
			DEBUG_MSG("Query contains no questions.\n");
		}
	}
#endif

	if (llist_isempty(&ql)) {
		res=RC_FORMAT;
		goto error_reply;
	}
	if (!(ans=compose_answer(&ql, hdr, rlenp, ednsinfop, udp, rcodep))) {
		/* An out of memory condition or similar could cause NULL output. Send failure notification */
		res=RC_SERVFAIL;
		goto free_ql_error_reply;
	}
	llist_free(&ql);
	return ans;

 free_ql_error_reply:
	llist_free(&ql);
 error_reply:
	*rlenp=sizeof(dns_hdr_t);
	{
		size_t allocsz = sizeof(dns_msg_t);
		if(res&~0xf)
			allocsz += sizeof_opt_pseudo_rr;
		ans= (dns_msg_t *)pdnsd_malloc(allocsz);
		if (ans) {
			mk_error_reply(hdr->id,rlen>=3?hdr->opcode:OP_QUERY,res,&ans->hdr);
			if(res&~0xf)
				add_opt_pseudo_rr(&ans,rlenp,&allocsz,
						  global.udpbufsize,res,0,0);
		}
		else if (++da_mem_errs<=MEM_MAX_ERRS) {
			log_error("Out of memory in query processing.");
		}
	}
	if(rcodep) *rcodep= res;
	return ans;
}

/*
 * Called by *_answer_thread exit handler to clean up process count.
 */
inline static void decrease_procs()
{

	pthread_mutex_lock(&proc_lock);
	procs--;
	qprocs--;
	pthread_mutex_unlock(&proc_lock);
}

static void udp_answer_thread_cleanup(void *data)
{
	pdnsd_free(data);
	decrease_procs();
}

/*
 * A thread opened to answer a query transmitted via udp. Data is a pointer to the structure udp_buf_t that
 * contains the received data and various other parameters.
 * After the query is answered, the thread terminates
 * XXX: data must point to a correctly aligned buffer
 */
static void *udp_answer_thread(void *data)
{
	struct msghdr msg;
	struct iovec v;
	struct cmsghdr *cmsg;
#if defined(SRC_ADDR_DISC)
	char ctrl[CMSG_SPACE(sizeof(pkt_info_t))];
#endif
	size_t rlen=((udp_buf_t *)data)->len;
	unsigned udpmaxrespsize = UDP_BUFSIZE;
	/* XXX: process_query is assigned to this, this mallocs, so this points to aligned memory */
	dns_msg_t *resp;
	int rcode;
	unsigned thrid;
	pthread_cleanup_push(udp_answer_thread_cleanup, data);
	THREAD_SIGINIT;

	if (!global.strict_suid) {
		if (!run_as(global.run_as)) {
			pdnsd_exit();
		}
	}

	for(;;) {
		pthread_mutex_lock(&proc_lock);
		if (procs<global.proc_limit)
			break;
		pthread_mutex_unlock(&proc_lock);
		usleep_r(50000);
	}
	++procs;
	thrid= ++thrid_cnt;
	pthread_mutex_unlock(&proc_lock);

#if DEBUG>0
	if(debug_p) {
		int err;
		if ((err=pthread_setspecific(thrid_key, &thrid)) != 0) {
			if(++da_misc_errs<=MISC_MAX_ERRS)
				log_error("pthread_setspecific failed: %s",strerror(err));
			/* pdnsd_exit(); */
		}
	}
#endif

	if (!(resp=process_query(((udp_buf_t *)data)->buf,&rlen,&udpmaxrespsize,&rcode))) {
		/*
		 * A return value of NULL is a fatal error that prohibits even the sending of an error message.
		 * logging is already done. Just exit the thread now.
		 */
		pthread_exit(NULL); /* data freed by cleanup handler */
	}
	pthread_cleanup_push(free, resp);
	if (rlen>udpmaxrespsize) {
		rlen=udpmaxrespsize;
		resp->hdr.tc=1; /*set truncated bit*/
	}
	DEBUG_MSG("Outbound msg len %li, tc=%u, rc=\"%s\"\n",(long)rlen,resp->hdr.tc,get_ename(rcode));

	v.iov_base=(char *)&resp->hdr;
	v.iov_len=rlen;
	msg.msg_iov=&v;
	msg.msg_iovlen=1;
#if (TARGET!=TARGET_CYGWIN)
#if defined(SRC_ADDR_DISC)
	msg.msg_control=ctrl;
	msg.msg_controllen=sizeof(ctrl);
#else
	msg.msg_control=NULL;
	msg.msg_controllen=0;
#endif
	msg.msg_flags=0;  /* to avoid warning message by Valgrind */
#endif

#ifdef ENABLE_IPV4
	if (run_ipv4) {

		msg.msg_name=&((udp_buf_t *)data)->addr.sin4;
		msg.msg_namelen=sizeof(struct sockaddr_in);
# if defined(SRC_ADDR_DISC)
#  if (TARGET==TARGET_LINUX)
		((udp_buf_t *)data)->pi.pi4.ipi_spec_dst=((udp_buf_t *)data)->pi.pi4.ipi_addr;
		cmsg=CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_len=CMSG_LEN(sizeof(struct in_pktinfo));
		cmsg->cmsg_level=SOL_IP;
		cmsg->cmsg_type=IP_PKTINFO;
		memcpy(CMSG_DATA(cmsg),&((udp_buf_t *)data)->pi.pi4,sizeof(struct in_pktinfo));
		msg.msg_controllen=CMSG_SPACE(sizeof(struct in_pktinfo));
#  else
		cmsg=CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_len=CMSG_LEN(sizeof(struct in_addr));
		cmsg->cmsg_level=IPPROTO_IP;
		cmsg->cmsg_type=IP_RECVDSTADDR;
		memcpy(CMSG_DATA(cmsg),&((udp_buf_t *)data)->pi.ai4,sizeof(struct in_addr));
		msg.msg_controllen=CMSG_SPACE(sizeof(struct in_addr));
#  endif
# endif
# if DEBUG>0
		{
			char buf[ADDRSTR_MAXLEN];

			DEBUG_MSG("Answering to: %s", inet_ntop(AF_INET,&((udp_buf_t *)data)->addr.sin4.sin_addr,buf,ADDRSTR_MAXLEN));
#  if defined(SRC_ADDR_DISC)
#   if (TARGET==TARGET_LINUX)
			DEBUG_MSGC(", source address: %s\n", inet_ntop(AF_INET,&((udp_buf_t *)data)->pi.pi4.ipi_spec_dst,buf,ADDRSTR_MAXLEN));
#   else
			DEBUG_MSGC(", source address: %s\n", inet_ntop(AF_INET,&((udp_buf_t *)data)->pi.ai4,buf,ADDRSTR_MAXLEN));
#   endif
#  else
			DEBUG_MSGC("\n");
#  endif
		}
# endif /* DEBUG */
	}
#endif
#ifdef ENABLE_IPV6
	ELSE_IPV6 {

		msg.msg_name=&((udp_buf_t *)data)->addr.sin6;
		msg.msg_namelen=sizeof(struct sockaddr_in6);
# if defined(SRC_ADDR_DISC)
		cmsg=CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_len=CMSG_LEN(sizeof(struct in6_pktinfo));
		cmsg->cmsg_level=SOL_IPV6;
		cmsg->cmsg_type=IPV6_PKTINFO;
		memcpy(CMSG_DATA(cmsg),&((udp_buf_t *)data)->pi.pi6,sizeof(struct in6_pktinfo));
		msg.msg_controllen=CMSG_SPACE(sizeof(struct in6_pktinfo));
# endif
# if DEBUG>0
		{
			char buf[ADDRSTR_MAXLEN];

			DEBUG_MSG("Answering to: %s", inet_ntop(AF_INET6,&((udp_buf_t *)data)->addr.sin6.sin6_addr,buf,ADDRSTR_MAXLEN));
#  if defined(SRC_ADDR_DISC)
			DEBUG_MSGC(", source address: %s\n", inet_ntop(AF_INET6,&((udp_buf_t *)data)->pi.pi6.ipi6_addr,buf,ADDRSTR_MAXLEN));
#  else
			DEBUG_MSGC("\n");
#  endif
		}
# endif /* DEBUG */
	}
#endif

	/* Lock the socket, and clear the error flag before dropping the lock */
#ifdef SOCKET_LOCKING
	pthread_mutex_lock(&s_lock);
#endif
	if (sendmsg(((udp_buf_t *)data)->sock,&msg,0)<0) {
#ifdef SOCKET_LOCKING
		pthread_mutex_unlock(&s_lock);
#endif
		if (++da_udp_errs<=UDP_MAX_ERRS) {
			log_error("Error in udp send: %s",strerror(errno));
		}
	} else {
		int tmp;
		socklen_t sl=sizeof(tmp);
		getsockopt(((udp_buf_t *)data)->sock, SOL_SOCKET, SO_ERROR, &tmp, &sl);
#ifdef SOCKET_LOCKING
		pthread_mutex_unlock(&s_lock);
#endif
	}

	pthread_cleanup_pop(1);  /* free(resp) */
	pthread_cleanup_pop(1);  /* free(data) */
	return NULL;
}

int init_udp_socket()
{
	int sock;
	int so=1;
	union {
#ifdef ENABLE_IPV4
		struct sockaddr_in sin4;
#endif
#ifdef ENABLE_IPV6
		struct sockaddr_in6 sin6;
#endif
	} sin;
	socklen_t sinl;

#ifdef ENABLE_IPV4
	if (run_ipv4) {
		if ((sock=socket(PF_INET,SOCK_DGRAM,IPPROTO_UDP))==-1) {
			log_error("Could not open udp socket: %s",strerror(errno));
			return -1;
		}
		memset(&sin.sin4,0,sizeof(struct sockaddr_in));
		sin.sin4.sin_family=AF_INET;
		sin.sin4.sin_port=htons(global.port);
		sin.sin4.sin_addr=global.a.ipv4;
		SET_SOCKA_LEN4(sin.sin4);
		sinl=sizeof(struct sockaddr_in);
	}
#endif
#ifdef ENABLE_IPV6
	ELSE_IPV6 {
		if ((sock=socket(PF_INET6,SOCK_DGRAM,IPPROTO_UDP))==-1) {
			log_error("Could not open udp socket: %s",strerror(errno));
			return -1;
		}
		memset(&sin.sin6,0,sizeof(struct sockaddr_in6));
		sin.sin6.sin6_family=AF_INET6;
		sin.sin6.sin6_port=htons(global.port);
		sin.sin6.sin6_flowinfo=IPV6_FLOWINFO;
		sin.sin6.sin6_addr=global.a.ipv6;
		SET_SOCKA_LEN6(sin.sin6);
		sinl=sizeof(struct sockaddr_in6);
	}
#endif

#ifdef SRC_ADDR_DISC
# if (TARGET!=TARGET_LINUX)
	if (run_ipv4) {
# endif
		/* The following must be set on any case because it also applies for IPv4 packets sent to
		 * ipv6 addresses. */
# if (TARGET==TARGET_LINUX )
		if (setsockopt(sock,SOL_IP,IP_PKTINFO,&so,sizeof(so))!=0) {
# else
		if (setsockopt(sock,IPPROTO_IP,IP_RECVDSTADDR,&so,sizeof(so))!=0) {
# endif
			log_error("Could not set options on udp socket: %s",strerror(errno));
			close(sock);
			return -1;
		}
# if (TARGET!=TARGET_LINUX)
	}
# endif

# ifdef ENABLE_IPV6
	if (!run_ipv4) {
		if (setsockopt(sock,SOL_IPV6,IPV6_RECVPKTINFO,&so,sizeof(so))!=0) {
			log_error("Could not set options on udp socket: %s",strerror(errno));
			close(sock);
			return -1;
		}
	}
# endif
#endif
	if (bind(sock,(struct sockaddr *)&sin,sinl)!=0) {
		log_error("Could not bind to udp socket: %s",strerror(errno));
		close(sock);
		return -1;
	}
	return sock;
}

/*
 * Listen on the specified port for udp packets and answer them (each in a new thread to be nonblocking)
 * This was changed to support sending UDP packets with exactly the same source address as they were coming
 * to us, as required by rfc2181. Although this is a sensible requirement, it is slightly more difficult
 * and may introduce portability issues.
 */
void *udp_server_thread(void *dummy)
{
	int sock;
	ssize_t qlen;
	pthread_t pt;
	udp_buf_t *buf;
	struct msghdr msg;
	struct iovec v;
	struct cmsghdr *cmsg;
	char ctrl[512];
#if defined(ENABLE_IPV6) && (TARGET==TARGET_LINUX)
	struct in_pktinfo sip;
#endif
	/* (void)dummy; */ /* To inhibit "unused variable" warning */

	THREAD_SIGINIT;


	if (!global.strict_suid) {
		if (!run_as(global.run_as)) {
			pdnsd_exit();
		}
	}

	sock=udp_socket;

	while (1) {
		int udpbufsize= global.udpbufsize;
		if (!(buf=(udp_buf_t *)pdnsd_calloc(1,sizeof(udp_buf_t)+udpbufsize))) {
			if (++da_mem_errs<=MEM_MAX_ERRS) {
				log_error("Out of memory in request handling.");
			}
			break;
		}

		buf->sock=sock;

		v.iov_base=(char *)buf->buf;
		v.iov_len=udpbufsize;
		msg.msg_iov=&v;
		msg.msg_iovlen=1;
#if (TARGET!=TARGET_CYGWIN)
		msg.msg_control=ctrl;
		msg.msg_controllen=sizeof(ctrl);
#endif

#if defined(SRC_ADDR_DISC)
# ifdef ENABLE_IPV4
		if (run_ipv4) {
			msg.msg_name=&buf->addr.sin4;
			msg.msg_namelen=sizeof(struct sockaddr_in);
			if ((qlen=recvmsg(sock,&msg,0))>=0) {
				cmsg=CMSG_FIRSTHDR(&msg);
				while(cmsg) {
#  if (TARGET==TARGET_LINUX)
					if (cmsg->cmsg_level==SOL_IP && cmsg->cmsg_type==IP_PKTINFO) {
						memcpy(&buf->pi.pi4,CMSG_DATA(cmsg),sizeof(struct in_pktinfo));
						break;
					}
#  else
					if (cmsg->cmsg_level==IPPROTO_IP && cmsg->cmsg_type==IP_RECVDSTADDR) {
						memcpy(&buf->pi.ai4,CMSG_DATA(cmsg),sizeof(buf->pi.ai4));
						break;
					}
#  endif
					cmsg=CMSG_NXTHDR(&msg,cmsg);
				}
				if (!cmsg) {
					if (++da_udp_errs<=UDP_MAX_ERRS) {
						log_error("Could not discover udp destination address");
					}
					goto free_buf_continue;
				}
			} else if (errno!=EINTR) {
				if (++da_udp_errs<=UDP_MAX_ERRS) {
					log_error("error in UDP recv: %s", strerror(errno));
				}
			}
		}
# endif
# ifdef ENABLE_IPV6
		ELSE_IPV6 {
			msg.msg_name=&buf->addr.sin6;
			msg.msg_namelen=sizeof(struct sockaddr_in6);
			if ((qlen=recvmsg(sock,&msg,0))>=0) {
				cmsg=CMSG_FIRSTHDR(&msg);
				while(cmsg) {
					if (cmsg->cmsg_level==SOL_IPV6 && cmsg->cmsg_type==IPV6_PKTINFO) {
						memcpy(&buf->pi.pi6,CMSG_DATA(cmsg),sizeof(struct in6_pktinfo));
						break;
					}
					cmsg=CMSG_NXTHDR(&msg,cmsg);
				}
				if (!cmsg) {
				       /* We might have an IPv4 Packet incoming on our IPv6 port, so we also have to
				        * check for IPv4 sender addresses */
					cmsg=CMSG_FIRSTHDR(&msg);
					while(cmsg) {
#  if (TARGET==TARGET_LINUX)
						if (cmsg->cmsg_level==SOL_IP && cmsg->cmsg_type==IP_PKTINFO) {
							memcpy(&sip,CMSG_DATA(cmsg),sizeof(sip));
							IPV6_MAPIPV4(&sip.ipi_addr,&buf->pi.pi6.ipi6_addr);
							buf->pi.pi6.ipi6_ifindex=sip.ipi_ifindex;
							break;
						}
						/* FIXME: What about BSD? probably ok, but... */
#  endif
						cmsg=CMSG_NXTHDR(&msg,cmsg);
					}
					if (!cmsg) {
						if (++da_udp_errs<=UDP_MAX_ERRS) {
							log_error("Could not discover udp destination address");
						}
						goto free_buf_continue;
					}
				}
			} else if (errno!=EINTR) {
				if (++da_udp_errs<=UDP_MAX_ERRS) {
					log_error("error in UDP recv: %s", strerror(errno));
				}
			}
		}
# endif
#else /* !SRC_ADDR_DISC */
# ifdef ENABLE_IPV4
		if (run_ipv4) {
			msg.msg_name=&buf->addr.sin4;
			msg.msg_namelen=sizeof(struct sockaddr_in);
		}
# endif
# ifdef ENABLE_IPV6
		ELSE_IPV6 {
			msg.msg_name=&buf->addr.sin6;
			msg.msg_namelen=sizeof(struct sockaddr_in6);
		}
# endif
		qlen=recvmsg(sock,&msg,0);
		if (qlen<0 && errno!=EINTR) {
			if (++da_udp_errs<=UDP_MAX_ERRS) {
				log_error("error in UDP recv: %s", strerror(errno));
			}
		}
#endif /* SRC_ADDR_DISC */

		if (qlen>=0) {
			pthread_mutex_lock(&proc_lock);
			if (qprocs<global.proc_limit+global.procq_limit) {
				int err;
				++qprocs; ++spawned;
				pthread_mutex_unlock(&proc_lock);
				buf->len=qlen;
				err=pthread_create(&pt,&attr_detached,udp_answer_thread,(void *)buf);
				if(err==0)
					continue;
				if(++da_thrd_errs<=THRD_MAX_ERRS)
					log_warn("pthread_create failed: %s",strerror(err));
				/* If thread creation failed, free resources associated with it. */
				pthread_mutex_lock(&proc_lock);
				--qprocs; --spawned;
			}
			++dropped;
			pthread_mutex_unlock(&proc_lock);
		}
	free_buf_continue:
		pdnsd_free(buf);
		usleep_r(50000);
	}

	udp_socket=-1;
	close(sock);
	udps_thrid=main_thrid;
	if (tcp_socket==-1)
	  pdnsd_exit();
	return NULL;
}

#ifndef NO_TCP_SERVER

static void tcp_answer_thread_cleanup(void *csock)
{
	close(*((int *)csock));
	pdnsd_free(csock);
	decrease_procs();
}

/*
 * Process a dns query via tcp. The argument is a pointer to the socket.
 */
static void *tcp_answer_thread(void *csock)
{
	/* XXX: This should be OK, the original must be (and is) aligned */
	int sock=*((int *)csock);
	unsigned thrid;

	pthread_cleanup_push(tcp_answer_thread_cleanup, csock);
	THREAD_SIGINIT;

	if (!global.strict_suid) {
		if (!run_as(global.run_as)) {
			pdnsd_exit();
		}
	}

	for(;;) {
		pthread_mutex_lock(&proc_lock);
		if (procs<global.proc_limit)
			break;
		pthread_mutex_unlock(&proc_lock);
		usleep_r(50000);
	}
	++procs;
	thrid= ++thrid_cnt;
	pthread_mutex_unlock(&proc_lock);

#if DEBUG>0
	if(debug_p) {
		int err;
		if ((err=pthread_setspecific(thrid_key, &thrid)) != 0) {
			if(++da_misc_errs<=MISC_MAX_ERRS)
				log_error("pthread_setspecific failed: %s",strerror(err));
			/* pdnsd_exit(); */
		}
	}
#endif
#ifdef TCP_SUBSEQ

	/* rfc1035 says we should process multiple queries in succession, so we are looping until
	 * the socket is closed by the other side or by tcp timeout.
	 * This in fact makes DoSing easier. If that is your concern, you should disable pdnsd's
	 * TCP server.*/
	for(;;)
#endif
	{
		int rlen,olen;
		size_t nlen;
		unsigned char *buf;
		dns_msg_t *resp;

#ifdef NO_POLL
		fd_set fds;
		struct timeval tv;
		FD_ZERO(&fds);
		PDNSD_ASSERT(sock<FD_SETSIZE,"socket file descriptor exceeds FD_SETSIZE.");
		FD_SET(sock, &fds);
		tv.tv_usec=0;
		tv.tv_sec=global.tcp_qtimeout;
		if (select(sock+1,&fds,NULL,NULL,&tv)<=0)
			pthread_exit(NULL); /* socket is closed by cleanup handler */
#else
		struct pollfd pfd;
		pfd.fd=sock;
		pfd.events=POLLIN;
		if (poll(&pfd,1,global.tcp_qtimeout*1000)<=0)
			pthread_exit(NULL); /* socket is closed by cleanup handler */
#endif
		{
			ssize_t err;
			uint16_t rlen_net;
			if ((err=read(sock,&rlen_net,sizeof(rlen_net)))!=sizeof(rlen_net)) {
				DEBUG_MSG("Error while reading from TCP client: %s\n",err==-1?strerror(errno):"incomplete data");
				/*
				 * If the socket timed or was closed before we even received the
				 * query length, we cannot return an error. So exit silently.
				 */
				pthread_exit(NULL); /* socket is closed by cleanup handler */
			}
			rlen=ntohs(rlen_net);
		}
		if (rlen == 0) {
			log_error("TCP zero size query received.\n");
			pthread_exit(NULL);
		}
		buf=(unsigned char *)pdnsd_malloc(rlen);
		if (!buf) {
			if (++da_mem_errs<=MEM_MAX_ERRS) {
				log_error("Out of memory in request handling.");
			}
			pthread_exit(NULL); /* socket is closed by cleanup handler */
		}
		pthread_cleanup_push(free, buf);

		olen=0;
		while(olen<rlen) {
			int rv;
#ifdef NO_POLL
			FD_ZERO(&fds);
			FD_SET(sock, &fds);
			tv.tv_usec=0;
			tv.tv_sec=global.tcp_qtimeout;
			if (select(sock+1,&fds,NULL,NULL,&tv)<=0)
				pthread_exit(NULL);  /* buf freed and socket closed by cleanup handlers */
#else
			pfd.fd=sock;
			pfd.events=POLLIN;
			if (poll(&pfd,1,global.tcp_qtimeout*1000)<=0)
				pthread_exit(NULL);  /* buf freed and socket closed by cleanup handlers */
#endif
			rv=read(sock,buf+olen,rlen-olen);
			if (rv<=0) {
				DEBUG_MSG("Error while reading from TCP client: %s\n",rv==-1?strerror(errno):"incomplete data");
				/*
				 * If the promised length was not sent, we should return an error message,
				 * but if read fails that way, it is unlikely that it will arrive. Nevertheless...
				 */
				if (olen>=2) { /* We need the id to send a valid reply. */
					dns_msg_t err;
					mk_error_reply(((dns_hdr_t*)buf)->id,
						       olen>=3?((dns_hdr_t*)buf)->opcode:OP_QUERY,
						       RC_FORMAT,
						       &err.hdr);
					err.len=htons(sizeof(dns_hdr_t));
					write_all(sock,&err,sizeof(err)); /* error anyway. */
				}
				pthread_exit(NULL); /* buf freed and socket closed by cleanup handlers */
			}
			olen += rv;
		}
		nlen=rlen;
		if (!(resp=process_query(buf,&nlen,NULL,NULL))) {
			/*
			 * A return value of NULL is a fatal error that prohibits even the sending of an error message.
			 * logging is already done. Just exit the thread now.
			 */
			pthread_exit(NULL);
		}
		pthread_cleanup_pop(1);  /* free(buf) */
		pthread_cleanup_push(free,resp);
		{
			int err; size_t rsize;
			resp->len=htons(nlen);
			rsize=dnsmsghdroffset+nlen;
			if ((err=write_all(sock,resp,rsize))!=rsize) {
				DEBUG_MSG("Error while writing to TCP client: %s\n",err==-1?strerror(errno):"unknown error");
				pthread_exit(NULL); /* resp is freed and socket is closed by cleanup handlers */
			}
		}
		pthread_cleanup_pop(1);  /* free(resp) */
	}

	/* socket is closed by cleanup handler */
	pthread_cleanup_pop(1);
	return NULL;
}

int init_tcp_socket()
{
	int sock;
	union {
#ifdef ENABLE_IPV4
		struct sockaddr_in sin4;
#endif
#ifdef ENABLE_IPV6
		struct sockaddr_in6 sin6;
#endif
	} sin;
	socklen_t sinl;

#ifdef ENABLE_IPV4
	if (run_ipv4) {
		if ((sock=socket(PF_INET,SOCK_STREAM,IPPROTO_TCP))==-1) {
			log_error("Could not open tcp socket: %s",strerror(errno));
			return -1;
		}
		memset(&sin.sin4,0,sizeof(struct sockaddr_in));
		sin.sin4.sin_family=AF_INET;
		sin.sin4.sin_port=htons(global.port);
		sin.sin4.sin_addr=global.a.ipv4;
		SET_SOCKA_LEN4(sin.sin4);
		sinl=sizeof(struct sockaddr_in);
	}
#endif
#ifdef ENABLE_IPV6
	ELSE_IPV6 {
		if ((sock=socket(PF_INET6,SOCK_STREAM,IPPROTO_TCP))==-1) {
			log_error("Could not open tcp socket: %s",strerror(errno));
			return -1;
		}
		memset(&sin.sin6,0,sizeof(struct sockaddr_in6));
		sin.sin6.sin6_family=AF_INET6;
		sin.sin6.sin6_port=htons(global.port);
		sin.sin6.sin6_flowinfo=IPV6_FLOWINFO;
		sin.sin6.sin6_addr=global.a.ipv6;
		SET_SOCKA_LEN6(sin.sin6);
		sinl=sizeof(struct sockaddr_in6);
	}
#endif
	{
		int so=1;
		/* The SO_REUSEADDR socket option tells the kernel that even if this port
		   is busy (in the TIME_WAIT state), go ahead and reuse it anyway. If it
		   is busy, but with another state, we should get an address already in
		   use error. It is useful if pdnsd is shut down, and then restarted right
		   away while sockets are still active on its port. There is a slight risk
		   though. If unexpected data comes in, it may confuse pdnsd, but while
		   this is possible, it is not likely.
		*/
		if(setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&so,sizeof(so)))
			log_warn("Could not set options on tcp socket: %s",strerror(errno));
	}
	if (bind(sock,(struct sockaddr *)&sin,sinl)) {
		log_error("Could not bind tcp socket: %s",strerror(errno));
		close(sock);
		return -1;
	}
	return sock;
}

/*
 * Listen on the specified port for tcp connects and answer them (each in a new thread to be nonblocking)
 */
void *tcp_server_thread(void *p)
{
	int sock;
	pthread_t pt;
	int *csock;

	/* (void)p; */  /* To inhibit "unused variable" warning */

	THREAD_SIGINIT;

	if (!global.strict_suid) {
		if (!run_as(global.run_as)) {
			pdnsd_exit();
		}
	}

	sock=tcp_socket;

	if (listen(sock,5)) {
		if (++da_tcp_errs<=TCP_MAX_ERRS) {
			log_error("Could not listen on tcp socket: %s",strerror(errno));
		}
		goto close_sock_return;
	}

	while (1) {
		if (!(csock=(int *)pdnsd_malloc(sizeof(int)))) {
			if (++da_mem_errs<=MEM_MAX_ERRS) {
				log_error("Out of memory in request handling.");
			}
			break;
		}
		if ((*csock=accept(sock,NULL,0))==-1) {
			if (errno!=EINTR && ++da_tcp_errs<=TCP_MAX_ERRS) {
				log_error("tcp accept failed: %s",strerror(errno));
			}
		} else {
			/*
			 * With creating a new thread, we follow recommendations
			 * in rfc1035 not to block
			 */
			pthread_mutex_lock(&proc_lock);
			if (qprocs<global.proc_limit+global.procq_limit) {
				int err;
				++qprocs; ++spawned;
				pthread_mutex_unlock(&proc_lock);
				err=pthread_create(&pt,&attr_detached,tcp_answer_thread,(void *)csock);
				if(err==0)
					continue;
				if(++da_thrd_errs<=THRD_MAX_ERRS)
					log_warn("pthread_create failed: %s",strerror(err));
				/* If thread creation failed, free resources associated with it. */
				pthread_mutex_lock(&proc_lock);
				--qprocs; --spawned;
			}
			++dropped;
			pthread_mutex_unlock(&proc_lock);
			close(*csock);
		}
		pdnsd_free(csock);
		usleep_r(50000);
	}
 close_sock_return:
	tcp_socket=-1;
	close(sock);
	tcps_thrid=main_thrid;
	if (udp_socket==-1)
		pdnsd_exit();
	return NULL;
}
#endif

/*
 * Starts the tcp server thread and the udp server thread. Both threads
 * are not terminated, so only a signal can interrupt the server.
 */
void start_dns_servers()
{

#ifndef NO_TCP_SERVER
	if (tcp_socket!=-1) {
		pthread_t tcps;

		if (pthread_create(&tcps,&attr_detached,tcp_server_thread,NULL)) {
			log_error("Could not create TCP server thread. Exiting.");
			pdnsd_exit();
		} else {
			tcps_thrid=tcps;
			log_info(2,"TCP server thread started.");
		}
	}
#endif

	if (udp_socket!=-1) {
		pthread_t udps;

		if (pthread_create(&udps,&attr_detached,udp_server_thread,NULL)) {
			log_error("Could not create UDP server thread. Exiting.");
			pdnsd_exit();
		} else {
			udps_thrid=udps;
			log_info(2,"UDP server thread started.");
		}
	}
}


/* Report the thread status to the file descriptor f, for the status fifo (see status.c) */
int report_thread_stat(int f)
{
	unsigned long nspawned,ndropped;
	int nactive,ncurrent,nqueued;

	/* The thread counters are volatile, so we will make copies
	   under locked conditions to make sure we get consistent data.
	*/
	pthread_mutex_lock(&proc_lock);
	nspawned=spawned; ndropped=dropped;
	nactive=procs; ncurrent=qprocs;
	nqueued=ncurrent-nactive;
	pthread_mutex_unlock(&proc_lock);

	fsprintf_or_return(f,"\nThread status:\n==============\n");
	if(!pthread_equal(servstat_thrid,main_thrid))
		fsprintf_or_return(f,"server status thread is running.\n");
	if(!pthread_equal(statsock_thrid,main_thrid))
		fsprintf_or_return(f,"pdnsd control thread is running.\n");
	if(!pthread_equal(tcps_thrid,main_thrid))
		fsprintf_or_return(f,"tcp server thread is running.\n");
	if(!pthread_equal(udps_thrid,main_thrid))
		fsprintf_or_return(f,"udp server thread is running.\n");
	fsprintf_or_return(f,"%lu query threads spawned in total (%lu queries dropped).\n",
			   nspawned,ndropped);
	fsprintf_or_return(f,"%i running query threads (%i active, %i queued).\n",
			   ncurrent,nactive,nqueued);
	return 0;
}

