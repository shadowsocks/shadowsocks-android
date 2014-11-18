/* cache.h - Definitions for the dns cache

   Copyright (C) 2000 Thomas Moestl
   Copyright (C) 2003, 2004, 2005, 2010, 2011 Paul A. Rombouts

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


#ifndef _CACHE_H_
#define _CACHE_H_

#include <config.h>
#include "ipvers.h"
#include <stdio.h>
#include "list.h"
#include "dns.h"
#include "conff.h"

struct rr_lent_s;

/*
 * These values are converted to host byte order. the data is _not_.
 */
typedef struct rr_b_s  {
	struct rr_b_s    *next;                   /* this is the next pointer in the dns_cent_t list. */
	unsigned         rdlen;
#if ALLOW_LOCAL_AAAA || defined(ENABLE_IPV6)
	struct in6_addr  data[0];                 /* dummy for alignment */
#else
	struct in_addr   data[0];
#endif
} rr_bucket_t;

typedef struct {
	struct rr_lent_s *lent;                   /* this points to the list entry */
	time_t           ttl;
	time_t           ts;
	unsigned short   flags;
	rr_bucket_t      *rrs;
} rr_set_t;


typedef struct {
	unsigned char    *qname;                  /* Name of the domain in length byte - string notation. */
	size_t           cs;                      /* Size of the cache entry, including RR sets. */
	unsigned short   num_rrs;                 /* The number of RR sets. When this decreases to 0, the cent is deleted. */
	unsigned short   flags;                   /* Flags for the whole domain. */
	union {
		struct {                          /* Fields used only for negatively cached domains. */
			struct rr_lent_s *lent;   /* list entry for the whole cent. */
			time_t           ttl;     /* TTL for negative caching. */
			time_t           ts;      /* Timestamp. */
		} neg;
		struct {                          /* Fields used only for domains that actually exist. */
			rr_set_t         *(rrmu[NRRMU]); /* The most used records.
							    Use the the value obtained from rrlkuptab[] as index. */
			rr_set_t         **rrext; /* Pointer (may be NULL) to an array of size NNRREXT storing the
						     less frequently used records. */
		} rr;
	};
	unsigned char    c_ns,c_soa;              /* Number of trailing name elements in qname to use to find NS or SOA
						     records to add to the authority section of a response. */
} dns_cent_t;

/* This value is used to represent an undefined c_ns or c_soa field. */
#define cundef 0xff

/*
 * the flag values for RR sets in the cache
 */
#define CF_NEGATIVE    1       /* this one is for per-RRset negative caching*/
#define CF_LOCAL       2       /* Local zone entry */
#define CF_AUTH        4       /* authoritative record */
#define CF_NOCACHE     8       /* Only hold for the cache latency time period, then purge.
				* Not really written to cache, but used by add_cache. */
#define CF_ADDITIONAL 16       /* This was fetched as an additional or "off-topic" record. */
#define CF_NOPURGE    32       /* Do not purge this record */
#define CF_ROOTSERV   64       /* This record was directly obtained from a root server */

#define CFF_NOINHERIT (CF_LOCAL|CF_AUTH|CF_ADDITIONAL|CF_ROOTSERV) /* not to be inherited on requery */

/*
 * the flag values for whole domains in the cache
 */
#define DF_NEGATIVE    1       /* this one is for whole-domain negative caching (created on NXDOMAIN)*/
#define DF_LOCAL       2       /* local record (in conj. with DF_NEGATIVE) */
#define DF_AUTH        4       /* authoritative record */
#define DF_NOCACHE     8       /* Only hold for the cache latency time period, then purge.
				* Only used for negatively cached domains.
				* Not really written to cache, but used by add_cache. */
#define DF_WILD       16       /* subdomains of this domain have wildcard records */

/* #define DFF_NOINHERIT (DF_NEGATIVE) */ /* not to be inherited on requery */

enum {w_wild=1, w_neg, w_locnerr};  /* Used to distinguish different types of wildcard records. */

#if DEBUG>0
#define NCFLAGS 7
#define NDFLAGS 5
#define CFLAGSTRLEN (NCFLAGS*4)
#define DFLAGSTRLEN (NDFLAGS*4)
extern const char cflgnames[];
extern const char dflgnames[];
char *flags2str(unsigned flags,char *buf,int nflags,const char *flgnames);
#define cflags2str(flags,buf) flags2str(flags,buf,NCFLAGS,cflgnames)
#define dflags2str(flags,buf) flags2str(flags,buf,NDFLAGS,dflgnames)
#endif

/*
 * This is the time in secs any record remains at least in the cache before it is purged.
 * (exception is that the cache is full)
 */
#define CACHE_LAT 120
#define CLAT_ADJ(ttl) ((ttl)<CACHE_LAT?CACHE_LAT:(ttl))
/* This is used internally to check if a rrset has timed out. */
#define timedout(rrset) ((rrset)->ts+CLAT_ADJ((rrset)->ttl)<time(NULL))
/* This is used internally to check if a negatively cached domain has timed out.
   Only use if the DF_NEGATIVE bit is set! */
#define timedout_nxdom(cent) ((cent)->neg.ts+CLAT_ADJ((cent)->neg.ttl)<time(NULL))

extern volatile short int use_cache_lock;


#ifdef ALLOC_DEBUG
#define DBGPARAM ,int dbg
#define DBGARG ,dbg
#define DBG0 ,0
#define DBG1 ,1
#else
#define DBGPARAM
#define DBGARG
#define DBG0
#define DBG1
#endif


/* Initialize the cache. Call only once. */
#define init_cache mk_dns_hash

/* Initialize the cache lock. Call only once. */
inline static void init_cache_lock()  __attribute__((always_inline));
inline static void init_cache_lock()
{
	use_cache_lock=1;
}

int empty_cache(slist_array sla);
void destroy_cache(void);
void read_disk_cache(void);
void write_disk_cache(void);

int report_cache_stat(int f);
int dump_cache(int fd, const unsigned char *name, int exact);

/*
 *  add_cache expects the dns_cent_t to be filled.
 */
void add_cache(dns_cent_t *cent);
int add_reverse_cache(dns_cent_t * cent);
void del_cache(const unsigned char *name);
void invalidate_record(const unsigned char *name);
int set_cent_flags(const unsigned char *name, unsigned flags);
unsigned char *getlocalowner(unsigned char *name,int tp);
dns_cent_t *lookup_cache(const unsigned char *name, int *wild);
rr_set_t *lookup_cache_local_rrset(const unsigned char *name, int type);
#if 0
int add_cache_rr_add(const unsigned char *name, int tp, time_t ttl, time_t ts, unsigned flags, unsigned dlen, void *data, unsigned long serial);
#endif

inline static unsigned int mk_flag_val(servparm_t *server)
  __attribute__((always_inline));
inline static unsigned int mk_flag_val(servparm_t *server)
{
	unsigned int fl=0;
	if (!server->purge_cache)
		fl|=CF_NOPURGE;
	if (server->nocache)
		fl|=CF_NOCACHE;
	if (server->rootserver)
		fl|=CF_ROOTSERV;
	return fl;
}

int init_cent(dns_cent_t *cent, const unsigned char *qname, time_t ttl, time_t ts, unsigned flags  DBGPARAM);
int add_cent_rrset_by_type(dns_cent_t *cent,  int type, time_t ttl, time_t ts, unsigned flags  DBGPARAM);
int add_cent_rr(dns_cent_t *cent, int type, time_t ttl, time_t ts, unsigned flags,unsigned dlen, void *data  DBGPARAM);
int del_rrset(rr_set_t *rrs  DBGPARAM);
void free_cent(dns_cent_t *cent  DBGPARAM);
void free_cent0(void *ptr);
void negate_cent(dns_cent_t *cent, time_t ttl, time_t ts);
void del_cent(dns_cent_t *cent);

/* Because this is empty by now, it is defined as an empty macro to save overhead.*/
/*void free_rr(rr_bucket_t cent);*/
#define free_rr(x)

dns_cent_t *copy_cent(dns_cent_t *cent  DBGPARAM);

#if 0
unsigned long get_serial(void);
#endif

/* Get pointer to rrset given cache entry and rr type value. */
inline static rr_set_t *getrrset(dns_cent_t *cent, int type)
  __attribute__((always_inline));
inline static rr_set_t *getrrset(dns_cent_t *cent, int type)
{
	if(!(cent->flags&DF_NEGATIVE)) {
		int tpi= type - T_MIN;

		if(tpi>=0 && tpi<T_NUM) {
			unsigned int idx = rrlkuptab[tpi];
			if(idx < NRRMU)
				return cent->rr.rrmu[idx];
			else {
				idx -= NRRMU;
				if(idx < NRREXT) {
					rr_set_t **rrext= cent->rr.rrext;
					if(rrext)
						return rrext[idx];
				}
			}
		}
	}

	return NULL;
}

/* This version of getrrset is slightly more efficient,
   but also more dangerous, because it performs less checks.
   It is safe to use if T_MIN <= type <= T_MAX and cent
   is not negative.
*/
inline static rr_set_t *getrrset_eff(dns_cent_t *cent, int type)
  __attribute__((always_inline));
inline static rr_set_t *getrrset_eff(dns_cent_t *cent, int type)
{
	unsigned int idx = rrlkuptab[type-T_MIN];
	if(idx < NRRMU)
		return cent->rr.rrmu[idx];
	else {
		idx -= NRRMU;
		if(idx < NRREXT) {
			rr_set_t **rrext= cent->rr.rrext;
			if(rrext)
				return rrext[idx];
		}
	}

	return NULL;
}


/* have_rr() tests whether a cache entry has at least one record of a given type.
   Only use if T_MIN <= type <=T_MAX
*/
inline static int have_rr(dns_cent_t *cent, int type)
  __attribute__((always_inline));
inline static int have_rr(dns_cent_t *cent, int type)
{
	rr_set_t *rrset;
	return !(cent->flags&DF_NEGATIVE) && (rrset=getrrset_eff(cent, type)) && rrset->rrs;
}

/* Some quick and dirty and hopefully fast macros. */
#define PDNSD_NOT_CACHED_TYPE(type) ((type)<T_MIN || (type)>T_MAX || rrlkuptab[(type)-T_MIN]>=NRRTOT)

/* This is useful for iterating over all the RR types in a cache entry in strict ascending order. */
#define NRRITERLIST(cent) ((cent)->flags&DF_NEGATIVE?0:(cent)->rr.rrext?NRRTOT:NRRMU)
#define RRITERLIST(cent)  ((cent)->flags&DF_NEGATIVE?NULL:(cent)->rr.rrext?rrcachiterlist:rrmuiterlist)

/* The following macros use array indices as arguments, not RR type values! */
#define GET_RRSMU(cent,i)  (!((cent)->flags&DF_NEGATIVE)?(cent)->rr.rrmu[i]:NULL)
#define GET_RRSEXT(cent,i) (!((cent)->flags&DF_NEGATIVE) && (cent)->rr.rrext?(cent)->rr.rrext[i]:NULL)
#define HAVE_RRMU(cent,i)  (!((cent)->flags&DF_NEGATIVE) && (cent)->rr.rrmu[i] && (cent)->rr.rrmu[i]->rrs)
#define HAVE_RREXT(cent,i) (!((cent)->flags&DF_NEGATIVE) && (cent)->rr.rrext && (cent)->rr.rrext[i] && (cent)->rr.rrext[i]->rrs)

#define RRARR_LEN(cent) ((cent)->flags&DF_NEGATIVE?0:(cent)->rr.rrext?NRRTOT:NRRMU)

/* This allows us to index the RR-set arrays in a cache entry as if they formed one contiguous array. */
#define RRARR_INDEX_TESTEXT(cent,i)    ((cent)->flags&DF_NEGATIVE?NULL:(i)<NRRMU?(cent)->rr.rrmu[i]:(cent)->rr.rrext?(cent)->rr.rrext[(i)-NRRMU]:NULL)
/* This gets the address where the pointer to an RR-set is stored in a cache entry,
   given the cache entry and an RR-set index.
   Address may be NULL if no storage space for the type has been allocated. */
#define RRARR_INDEX_PA_TESTEXT(cent,i) ((cent)->flags&DF_NEGATIVE?NULL:(i)<NRRMU?&(cent)->rr.rrmu[i]:(cent)->rr.rrext?&(cent)->rr.rrext[(i)-NRRMU]:NULL)

/* The following macros should only be used if 0 <= i < RRARR_LEN(cent) ! */
#define RRARR_INDEX(cent,i)    ((i)<NRRMU?(cent)->rr.rrmu[i]:(cent)->rr.rrext[(i)-NRRMU])
#define RRARR_INDEX_PA(cent,i) ((i)<NRRMU?&(cent)->rr.rrmu[i]:&(cent)->rr.rrext[(i)-NRRMU])

#endif
