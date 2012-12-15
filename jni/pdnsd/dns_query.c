/* dns_query.c - Execute outgoing dns queries and write entries to cache

   Copyright (C) 2000, 2001 Thomas Moestl
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2011 Paul A. Rombouts

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
#ifdef HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif
#include <stdlib.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include "list.h"
#include "consts.h"
#include "ipvers.h"
#include "dns_query.h"
#include "cache.h"
#include "dns.h"
#include "conff.h"
#include "servers.h"
#include "helpers.h"
#include "netdev.h"
#include "error.h"
#include "debug.h"


#if defined(NO_TCP_QUERIES) && M_PRESET!=UDP_ONLY
# error "You may not define NO_TCP_QUERIES when M_PRESET is not set to UDP_ONLY"
#endif
#if defined(NO_UDP_QUERIES) && M_PRESET!=TCP_ONLY
# error "You may not define NO_UDP_QUERIES when M_PRESET is not set to TCP_ONLY"
#endif

/* data type to hold lists of IP addresses (both v4 and v6)
   The allocated size should be:
     sizeof(rejectlist_t) + na4*sizeof(addr4maskpair_t) + na6*sizeof(addr6maskpair_t)
*/
typedef struct rejectlist_s {
	struct rejectlist_s *next;
	short               policy;
	short               inherit;
	int                 na4;
#if ALLOW_LOCAL_AAAA
	int                 na6;
	addr6maskpair_t     rdata[0];  /* dummy array for alignment */
#else
	addr4maskpair_t     rdata[0];
#endif
} rejectlist_t;

/* --- structures and state constants for parallel query */
typedef struct {
	union {
#ifdef ENABLE_IPV4
		struct sockaddr_in  sin4;
#endif
#ifdef ENABLE_IPV6
		struct sockaddr_in6 sin6;
#endif
	}                   a;
#ifdef ENABLE_IPV6
	struct in_addr      a4fallback;
#endif
	time_t              timeout;
	unsigned short      flags;
	short               nocache;
	short               state;
	short               qm;
        char                auth_serv;
	char                lean_query;
	char                edns_query;
	char                needs_testing;
	char                trusted;
	char                aa;
	char                tc;
	char                failed;
	const unsigned char *nsdomain;
	rejectlist_t        *rejectlist;
	/* internal state for p_exec_query */
	int                 sock;
#if 0
	dns_cent_t          nent;
	dns_cent_t          servent;
#endif
	unsigned short      transl;
	unsigned short      recvl;
#ifndef NO_TCP_QUERIES
	int                 iolen;  /* number of bytes written or read up to now */
#endif
	dns_msg_t           *msg;
	dns_hdr_t           *recvbuf;
	unsigned short      myrid;
	int                 s_errno;
} query_stat_t;
typedef DYNAMIC_ARRAY(query_stat_t) *query_stat_array;

/* Some macros for handling data in reject lists
   Perhaps we should use inline functions instead of macros.
*/
#define have_rejectlist(st) ((st)->rejectlist!=NULL)
#define inherit_rejectlist(st) ((st)->rejectlist && (st)->rejectlist->inherit)
#define reject_policy(st)   ((st)->rejectlist->policy)
#define nreject_a4(st)      ((st)->rejectlist->na4)
#if ALLOW_LOCAL_AAAA
#define nreject_a6(st)      ((st)->rejectlist->na6)
#define rejectlist_a6(st)   ((addr6maskpair_t *)(st)->rejectlist->rdata)
#define rejectlist_a4(st)   ((addr4maskpair_t *)(rejectlist_a6(st)+nreject_a6(st)))
#else
#define rejectlist_a4(st)   ((addr4maskpair_t *)(st)->rejectlist->rdata)
#endif

#define QS_INITIAL       0  /* This is the initial state. Set this before starting. */

#define QS_TCPINITIAL    1  /* Start a TCP query. */
#define QS_TCPWRITE      2  /* Waiting to write data. */
#define QS_TCPREAD       3  /* Waiting to read data. */

#define QS_UDPINITIAL    4  /* Start a UDP query */
#define QS_UDPRECEIVE    5  /* UDP query transmitted, waiting for response. */

#define QS_QUERY_CASES   case QS_TCPINITIAL: case QS_TCPWRITE: case QS_TCPREAD: case QS_UDPINITIAL: case QS_UDPRECEIVE

#define QS_CANCELED      7  /* query was started, but canceled before completion */
#define QS_DONE          8  /* done, resources freed, result is in stat_t */


/* Events to be polled/selected for */
#define QS_WRITE_CASES case QS_TCPWRITE
#define QS_READ_CASES  case QS_TCPREAD: case QS_UDPRECEIVE

/*
 * This is for error handling to prevent spewing the log files.
 * Races do not really matter here, so no locks.
 */
#define MAXPOLLERRS 10
static volatile unsigned long poll_errs=0;

#define SOCK_ADDR(p) ((struct sockaddr *) &(p)->a)

#ifdef SIN_LEN
#undef SIN_LEN
#endif

#define SIN_LEN SEL_IPVER(sizeof(struct sockaddr_in),sizeof(struct sockaddr_in6))
#define PDNSD_A(p) SEL_IPVER(((pdnsd_a *) &(p)->a.sin4.sin_addr),((pdnsd_a *) &(p)->a.sin6.sin6_addr))

#ifndef EWOULDBLOCK
#define EWOULDBLOCK EAGAIN
#endif

typedef DYNAMIC_ARRAY(dns_cent_t) *dns_cent_array;


/*
 * Take the data from an RR and add it to an array of cache entries.
 * The return value will be RC_OK in case of success,
 * RC_SERVFAIL in case there is a problem with inconsistent ttl timestamps
 * or RC_FATALERR in case of a memory allocation failure.
 */
static int rr_to_cache(dns_cent_array *centa, unsigned char *oname, int tp, time_t ttl,
		       unsigned dlen, void *data, unsigned flags, time_t queryts)
{
	int i,n;
	dns_cent_t *cent;

	n=DA_NEL(*centa);
	for(i=0;i<n;++i) {
		cent=&DA_INDEX(*centa,i);
		if (rhnicmp(cent->qname,oname)) {
			int retval=RC_OK;
			/* We already have an entry in the array for this name. add_cent_rr is sufficient.
			   However, make sure there are no double records. This is done by add_cent_rr */
#ifdef RFC2181_ME_HARDER
			rr_set_t *rrset= getrrset(cent,tp);
			if (rrset && rrset->ttl!=ttl)
				retval= RC_SERVFAIL;
#endif
			return add_cent_rr(cent,tp,ttl,queryts,flags,dlen,data  DBG1)? retval: RC_FATALERR;
		}
	}

	/* Add a new entry to the array for this name. */
	if (!(*centa=DA_GROW1_F(*centa,free_cent0)))
		return RC_FATALERR;
	cent=&DA_LAST(*centa);
	if (!init_cent(cent,oname, 0, 0, 0  DBG1)) {
		*centa=DA_RESIZE(*centa,n);
		return RC_FATALERR;
	}
	return add_cent_rr(cent,tp,ttl,queryts,flags,dlen,data  DBG1)? RC_OK: RC_FATALERR;
}

/*
 * Takes a pointer (ptr) to a buffer with recnum rrs,decodes them and enters
 * them into an array of cache entries. *ptr is modified to point after the last
 * rr, and *lcnt is decremented by the size of the rrs.
 *
 * *numopt is incremented with the number of OPT pseudo RRs found (should be at most one).
 * The structure pointed to by ep is filled with the information of the first OPT pseudo RR found,
 * but only if *numopt was set to zero before the call.
 *
 * The return value will be either RC_OK (which indicates success),
 * or one of the failure codes RC_FORMAT, RC_TRUNC, RC_SERVFAIL or RC_FATALERR
 * (the latter indicates a memory allocation failure).
*/
static int rrs2cent(unsigned char *msg, size_t msgsz, unsigned char **ptr, size_t *lcnt, int recnum,
		    unsigned flags, time_t queryts, dns_cent_array *centa, int *numopt, edns_info_t *ep)
{
	int rc, retval=RC_OK;
	int i;
	uint16_t type,class; uint32_t ttl; uint16_t rdlength;

	for (i=0;i<recnum;i++) {
		unsigned char oname[DNSNAMEBUFSIZE], *ttlp;
		unsigned int len;
		if ((rc=decompress_name(msg, msgsz, ptr, lcnt, oname, &len))!=RC_OK) {
			return rc;
		}
		if (*lcnt<sizeof_rr_hdr_t) {
			return RC_TRUNC;
		}
		*lcnt -= sizeof_rr_hdr_t;
		GETINT16(type,*ptr);
		GETINT16(class,*ptr);
		ttlp= *ptr;   /* Remember pointer to ttl field. */
		GETINT32(ttl,*ptr);
		GETINT16(rdlength,*ptr);
		if (*lcnt<rdlength) {
			return RC_TRUNC;
		}

		if(type==T_OPT) {
			/* Found OPT pseudo-RR */
			if((*numopt)++ == 0) {
#if DEBUG>0
				if(oname[0]!=0) {
					DEBUG_MSG("rrs2cent: name in OPT record not empty!\n");
				}
#endif
				ep->udpsize= class;
				ep->rcode= ((uint16_t)ttlp[0]<<4) | ((dns_hdr_t *)msg)->rcode;
				ep->version= ttlp[1];
				ep->do_flg= (ttlp[2]>>7)&1;
#if DEBUG>0
				if(debug_p) {
					unsigned int Zflags= ((uint16_t)ttlp[2]<<8) | ttlp[3];
					if(Zflags & 0x7fff) {
						DEBUG_MSG("rrs2cent: Z field contains unknown nonzero bits (%04x).\n",
							  Zflags);
					}
				}
				if(rdlength) {
					DEBUG_MSG("rrs2cent: RDATA field in OPT record not empty!\n");
				}
#endif
			}
			else {
				DEBUG_MSG("rrs2cent: ingnoring surplus OPT record.\n");
			}
		}
		else if (!(PDNSD_NOT_CACHED_TYPE(type) || class!=C_IN)) {
			/* Some types contain names that may be compressed, so these need to be processed.
			 * The other records are taken as they are. */

			size_t blcnt=rdlength;
			unsigned char *bptr=*ptr;  /* make backup for decompression, because rdlength is the
						      authoritative record length and pointer and size will be
						      modified by decompress_name. */
			unsigned char *nptr;
			unsigned int slen;

			switch (type) {
			case T_A:
				/* Validate types we use internally */
				if(rdlength!=4) goto invalid_length;
				goto default_case;

			case T_CNAME:
			case T_MB:
			case T_MD:
			case T_MF:
			case T_MG:
			case T_MR:
			case T_NS:
			case T_PTR:
			{
				unsigned char db[DNSNAMEBUFSIZE];
				if ((rc=decompress_name(msg, msgsz, &bptr, &blcnt, db, &len))!=RC_OK)
					return rc==RC_TRUNC?RC_FORMAT:rc;
				if (blcnt!=0)
					goto trailing_junk;
				if ((rc=rr_to_cache(centa, oname, type, ttl, len, db, flags,queryts))!=RC_OK) {
					if(rc==RC_FATALERR)
						return rc;
					retval=rc;
				}
			}
				break;

#if IS_CACHED_MINFO || IS_CACHED_RP
#if IS_CACHED_MINFO
			case T_MINFO:
#endif
#if IS_CACHED_RP
			case T_RP:
#endif
			{
				unsigned char db[DNSNAMEBUFSIZE+DNSNAMEBUFSIZE];
				nptr=db;
				if ((rc=decompress_name(msg, msgsz, &bptr, &blcnt, nptr, &len))!=RC_OK)
					return rc==RC_TRUNC?RC_FORMAT:rc;
				/* PDNSD_ASSERT(len + DNSNAMEBUFSIZE <= sizeof(db), "T_MINFO/T_RP: buffer limit reached"); */
				nptr+=len;
				slen=len;
				if ((rc=decompress_name(msg, msgsz, &bptr, &blcnt, nptr, &len))!=RC_OK)
					return rc==RC_TRUNC?RC_FORMAT:rc;
				/*nptr+=len;*/
				slen+=len;
				if (blcnt!=0)
					goto trailing_junk;
				if ((rc=rr_to_cache(centa, oname, type, ttl, slen, db, flags,queryts))!=RC_OK) {
					if(rc==RC_FATALERR)
						return rc;
					retval=rc;
				}
			}
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
			{
				unsigned char db[2+DNSNAMEBUFSIZE];
				if (blcnt<2)
					goto record_too_short;
				memcpy(db,bptr,2); /* copy the preference field*/
				blcnt-=2;
				bptr+=2;
				nptr=db+2;
				slen=2;
				if ((rc=decompress_name(msg, msgsz, &bptr, &blcnt, nptr, &len))!=RC_OK)
					return rc==RC_TRUNC?RC_FORMAT:rc;
				/*nptr+=len;*/
				slen+=len;
				if (blcnt!=0)
					goto trailing_junk;
				if ((rc=rr_to_cache(centa, oname, type, ttl, slen, db, flags,queryts))!=RC_OK) {
					if(rc==RC_FATALERR)
						return rc;
					retval=rc;
				}
			}
				break;

			case T_SOA:
			{
				unsigned char db[DNSNAMEBUFSIZE+DNSNAMEBUFSIZE+20];
				nptr=db;
				if ((rc=decompress_name(msg, msgsz, &bptr, &blcnt, nptr, &len))!=RC_OK)
					return rc==RC_TRUNC?RC_FORMAT:rc;
				/* PDNSD_ASSERT(len + DNSNAMEBUFSIZE <= sizeof(db), "T_SOA: buffer limit reached"); */
				nptr+=len;
				slen=len;
				if ((rc=decompress_name(msg, msgsz, &bptr, &blcnt, nptr, &len))!=RC_OK)
					return rc==RC_TRUNC?RC_FORMAT:rc;
				nptr+=len;
				slen+=len;
				/* PDNSD_ASSERT(slen + 20 <= sizeof(db), "T_SOA: buffer limit reached"); */
				if (blcnt<20)
					goto record_too_short;
				memcpy(nptr,bptr,20); /*copy the rest of the SOA record*/
				blcnt-=20;
				slen+=20;
				if (blcnt!=0)
					goto trailing_junk;
				if ((rc=rr_to_cache(centa, oname, type, ttl, slen, db, flags,queryts))!=RC_OK) {
					if(rc==RC_FATALERR)
						return rc;
					retval=rc;
				}
			}
				break;
#if IS_CACHED_AAAA
			case T_AAAA:
				/* Validate types we use internally */
				if(rdlength!=16) goto invalid_length;
				goto default_case;
#endif
#if IS_CACHED_PX
			case T_PX:
			{
				unsigned char db[2+DNSNAMEBUFSIZE+DNSNAMEBUFSIZE];
				if (blcnt<2)
					goto record_too_short;
				memcpy(db,bptr,2); /* copy the preference field*/
				blcnt-=2;
				bptr+=2;
				nptr=db+2;
				slen=2;
				if ((rc=decompress_name(msg, msgsz, &bptr, &blcnt, nptr, &len))!=RC_OK)
					return rc==RC_TRUNC?RC_FORMAT:rc;
				/* PDNSD_ASSERT(len + DNSNAMEBUFSIZE <= sizeof(db), "T_PX: buffer limit reached"); */
				nptr+=len;
				slen+=len;
				if ((rc=decompress_name(msg, msgsz, &bptr, &blcnt, nptr, &len))!=RC_OK)
					return rc==RC_TRUNC?RC_FORMAT:rc;
				/* nptr+=len; */
				slen+=len;
				if (blcnt!=0)
					goto trailing_junk;
				if ((rc=rr_to_cache(centa, oname, type, ttl, slen, db, flags,queryts))!=RC_OK) {
					if(rc==RC_FATALERR)
						return rc;
					retval=rc;
				}
			}
				break;
#endif
#if IS_CACHED_SRV
			case T_SRV:
			{
				unsigned char db[6+DNSNAMEBUFSIZE];
				if (blcnt<6)
					goto record_too_short;
				memcpy(db,bptr,6);
				blcnt-=6;
				bptr+=6;
				nptr=db+6;
				slen=6;
				if ((rc=decompress_name(msg, msgsz, &bptr, &blcnt, nptr, &len))!=RC_OK)
					return rc==RC_TRUNC?RC_FORMAT:rc;
				/*nptr+=len;*/
				slen+=len;
				if (blcnt!=0)
					goto trailing_junk;
				if ((rc=rr_to_cache(centa, oname, type, ttl, slen, db, flags,queryts))!=RC_OK) {
					if(rc==RC_FATALERR)
						return rc;
					retval=rc;
				}
			}
				break;
#endif
#if IS_CACHED_NXT
			case T_NXT:
			{
				unsigned char db[1040];
				nptr=db;
				if ((rc=decompress_name(msg, msgsz, &bptr, &blcnt, nptr, &len))!=RC_OK)
					return rc==RC_TRUNC?RC_FORMAT:rc;
				nptr+=len;
				slen=len+blcnt;
				if (slen > sizeof(db))
					goto buffer_overflow;
				memcpy(nptr,bptr,blcnt);
				if ((rc=rr_to_cache(centa, oname, type, ttl, slen, db, flags,queryts))!=RC_OK) {
					if(rc==RC_FATALERR)
						return rc;
					retval=rc;
				}
			}
				break;
#endif
#if IS_CACHED_NAPTR
			case T_NAPTR:
			{
				int j;
				unsigned char db[4 + 3*256 + DNSNAMEBUFSIZE];
				nptr=db;
				/*
				 * After the preference field, three text strings follow, the maximum length being 255
				 * characters for each (this is ensured by the type of *bptr), plus one length byte for
				 * each, so 3 * 256 = 786 in total. In addition, the name below is up to DNSNAMEBUFSIZE characters
				 * in size, and the preference field is another 4 bytes in size, so the total length
				 * that can be taken up is 1028 characters. This means that the whole record will always
				 * fit into db.
				 */
				len=4;   /* also copy the preference field*/
				for (j=0;j<3;j++) {
					if (len>=blcnt)
						goto record_too_short;
					len += ((unsigned)bptr[len])+1;
				}
				if(len>blcnt)
					goto record_too_short;
				memcpy(nptr,bptr,len);
				blcnt-=len;
				bptr+=len;
				nptr+=len;
				slen=len;

				/* PDNSD_ASSERT(slen+DNSNAMEBUFSIZE <= sizeof(db), "T_NAPTR: buffer limit reached (name)"); */
				if ((rc=decompress_name(msg, msgsz, &bptr, &blcnt, nptr, &len))!=RC_OK)
					return rc==RC_TRUNC?RC_FORMAT:rc;
				/*nptr+=len;*/
				slen+=len;
				if (blcnt!=0)
					goto trailing_junk;
				if ((rc=rr_to_cache(centa, oname, type, ttl, slen, db, flags,queryts))!=RC_OK) {
					if(rc==RC_FATALERR)
						return rc;
					retval=rc;
				}
			}
				break;
#endif
#if IS_CACHED_IPSECKEY
			case T_IPSECKEY:
			{
				unsigned gwtp;
				/* An IPSECKEY record can contain a domain name, so we do some sanity checks just to be sure. */
				if(blcnt<3) goto record_too_short;
				gwtp= bptr[1];
				blcnt -= 3;
				bptr += 3;
				switch(gwtp) {
				case 0: goto default_case;
				case 1: /* There should be enough room for IPv4 address. */
					if(blcnt<4) goto record_too_short;
					goto default_case;
				case 2: /* There should be enough room for IPv6 address. */
					if(blcnt<16) goto record_too_short;
					goto default_case;
				case 3: /* Check that domain name is not compressed. */
					if(isnormalencdomname(bptr,blcnt)) goto default_case;
					/* It appears the name is compressed even though RFC 4025
					   says it shouldn't be. For the sake of flexibility, we
					   try to decompress it anyway. */
					{
						unsigned char *rbuf, nmbuf[DNSNAMEBUFSIZE];
						if ((rc=decompress_name(msg, msgsz, &bptr, &blcnt, nmbuf, &len))!=RC_OK)
							return rc==RC_TRUNC?RC_FORMAT:rc;
						slen=3+len+blcnt;
						rbuf=malloc(slen);
						if(!rbuf) return RC_FATALERR;
						nptr=mempcpy(rbuf,*ptr,3);
						nptr=mempcpy(nptr,nmbuf,len);
						memcpy(nptr,bptr,blcnt);
						rc=rr_to_cache(centa, oname, type, ttl, slen, rbuf, flags,queryts);
						free(rbuf);
						if(rc!=RC_OK) {
							if(rc==RC_FATALERR)
								return rc;
							retval=rc;
						}
					}
					break;
				default:
					DEBUG_MSG("rrs2cent: %s record contains unsupported gateway type (%u).\n",getrrtpname(type),gwtp);
					return RC_FORMAT;
				}
			}
				break;
#endif
#if IS_CACHED_RRSIG
			case T_RRSIG:
				/* An RRSIG record contains a domain name, so we do some sanity checks just to be sure. */
				if(blcnt<18) goto record_too_short;
				blcnt -= 18;
				bptr += 18;
				if(isnormalencdomname(bptr,blcnt)) goto default_case;
				/* It appears the name is compressed even though RFC 4034
				   says it shouldn't be. For the sake of flexibility, we
				   try to decompress it anyway. */
				{
					unsigned char *rbuf, nmbuf[DNSNAMEBUFSIZE];
					if ((rc=decompress_name(msg, msgsz, &bptr, &blcnt, nmbuf, &len))!=RC_OK)
						return rc==RC_TRUNC?RC_FORMAT:rc;
					slen=18+len+blcnt;
					rbuf=malloc(slen);
					if(!rbuf) return RC_FATALERR;
					nptr=mempcpy(rbuf,*ptr,18);
					nptr=mempcpy(nptr,nmbuf,len);
					memcpy(nptr,bptr,blcnt);
					rc=rr_to_cache(centa, oname, type, ttl, slen, rbuf, flags,queryts);
					free(rbuf);
					if(rc!=RC_OK) {
						if(rc==RC_FATALERR)
							return rc;
						retval=rc;
					}
				}
				break;
#endif
#if IS_CACHED_NSEC
			case T_NSEC:
				/* An NSEC record contains a domain name, so we do some sanity checks just to be sure. */
				if(isnormalencdomname(bptr,blcnt)) goto default_case;
				/* It appears the name is compressed even though RFC 4034
				   says it shouldn't be. For the sake of flexibility, we
				   try to decompress it anyway. */
				{
					unsigned char *rbuf, nmbuf[DNSNAMEBUFSIZE];
					if ((rc=decompress_name(msg, msgsz, &bptr, &blcnt, nmbuf, &len))!=RC_OK)
						return rc==RC_TRUNC?RC_FORMAT:rc;
					slen=len+blcnt;
					rbuf=malloc(slen);
					if(!rbuf) return RC_FATALERR;
					nptr=mempcpy(rbuf,nmbuf,len);
					memcpy(nptr,bptr,blcnt);
					rc=rr_to_cache(centa, oname, type, ttl, slen, rbuf, flags,queryts);
					free(rbuf);
					if(rc!=RC_OK) {
						if(rc==RC_FATALERR)
							return rc;
						retval=rc;
					}
				}
				break;
#endif
			default:
			default_case:
				if ((rc=rr_to_cache(centa, oname, type, ttl, rdlength, *ptr, flags,queryts))!=RC_OK) {
					if(rc==RC_FATALERR)
						return rc;
					retval=rc;
				}
			}
		}
		else {
			/* skip otherwise */
			DEBUG_MSG("rrs2cent: ignoring record of type %s (%d), class %s (%d).\n",
				  getrrtpname(type), type,
				  class==C_IN?"IN":"[unknown]", class);
		}

		*lcnt -= rdlength;
		*ptr += rdlength;
	}
	return retval;

 trailing_junk:
	DEBUG_MSG("rrs2cent: %s record has trailing junk.\n",getrrtpname(type));
	return RC_FORMAT;

 record_too_short:
	DEBUG_MSG("rrs2cent: %s record too short.\n",getrrtpname(type));
	return RC_FORMAT;

 buffer_overflow:
	DEBUG_MSG("rrs2cent: buffer too small to process %s record.\n",getrrtpname(type));
	return RC_FORMAT;

 invalid_length:
	DEBUG_MSG("rrs2cent: %s record has length %u.\n",getrrtpname(type),rdlength);
	return RC_FORMAT;
}

/*
 * Try to bind the socket to a port in the given port range. Returns 1 on success, or 0 on failure.
 */
static int bind_socket(int s)
{
	int query_port_start=global.query_port_start,query_port_end=global.query_port_end;

	/*
	 * -1, as a special value for query_port_start, denotes that we let the kernel select
	 * a port when we first use the socket, which used to be the default.
	 */
	if (query_port_start >= 0) {
		union {
#ifdef ENABLE_IPV4
			struct sockaddr_in sin4;
#endif
#ifdef ENABLE_IPV6
			struct sockaddr_in6 sin6;
#endif
		} sin;
		socklen_t sinl;
		int prt, pstart, range = query_port_end-query_port_start+1, m=0xffff;
		unsigned try1,try2, maxtry2;

		if (range<=0 || range>0x10000) {
			log_warn("Illegal port range in %s line %d, dropping query!\n",__FILE__,__LINE__);
			return 0;
		}
		if(range<=0x8000) {
			/* Find the smallest power of 2 >= range. */
			for(m=1; m<range; m <<= 1);
			/* Convert into a bit mask. */
			--m;
		}

		for (try2=0,maxtry2=range*2;;) {
			/* Get a random number < range, by rejecting those >= range. */
			for(try1=0;;) {
				prt= get_rand16()&m;
				if(prt<range) break;
				if(++try1>=0x10000) {
					log_warn("Cannot get random number < range"
						 " after %d tries in %s line %d,"
						 " bad random number generator?\n",
						 try1,__FILE__,__LINE__);
					return 0;
				}
			}
			prt += query_port_start;

			for(pstart=prt;;) {
#ifdef ENABLE_IPV4
				if (run_ipv4) {
					memset(&sin.sin4,0,sizeof(struct sockaddr_in));
					sin.sin4.sin_family=AF_INET;
					sin.sin4.sin_port=htons(prt);
					sin.sin4.sin_addr=global.out_a.ipv4;
					SET_SOCKA_LEN4(sin.sin4);
					sinl=sizeof(struct sockaddr_in);
				}
#endif
#ifdef ENABLE_IPV6
				ELSE_IPV6 {
					memset(&sin.sin6,0,sizeof(struct sockaddr_in6));
					sin.sin6.sin6_family=AF_INET6;
					sin.sin6.sin6_port=htons(prt);
					sin.sin6.sin6_flowinfo=IPV6_FLOWINFO;
					sin.sin6.sin6_addr=global.out_a.ipv6;
					SET_SOCKA_LEN6(sin.sin6);
					sinl=sizeof(struct sockaddr_in6);
				}
#endif
				if (bind(s,(struct sockaddr *)&sin,sinl)==-1) {
					if (errno!=EADDRINUSE &&
					    errno!=EADDRNOTAVAIL) { /* EADDRNOTAVAIL should not happen here... */
						log_warn("Could not bind to socket: %s\n", strerror(errno));
						return 0;
					}
					/* If the address is in use, we continue. */
				} else
					goto done;

				if(++try2>=maxtry2) {
					/* It is possible we missed the free ports by chance,
					   try scanning the whole range. */
					if (++prt>query_port_end)
						prt=query_port_start;
					if (prt==pstart) {
						/* Wrapped around, scanned the whole range. Give up. */
						log_warn("Out of ports in the range"
							 " %d-%d, dropping query!\n",
							 query_port_start,query_port_end);
						return 0;
					}
				}
				else    /* Try new random number */
					break;
			}
		}
	}
done:
	return 1;
}


inline static void *realloc_or_cleanup(void *ptr,size_t size)
{
	void *retval=pdnsd_realloc(ptr,size);
	if(!retval)
		pdnsd_free(ptr);
	return retval;
}

#if defined(NO_TCP_QUERIES)
# define USE_UDP(st) 1
#elif defined(NO_UDP_QUERIES)
# define USE_UDP(st) 0
#else /* !defined(NO_TCP_QUERIES) && !defined(NO_UDP_QUERIES) */
# define USE_UDP(st) ((st)->qm==UDP_ONLY || (st)->qm==UDP_TCP)

/* These functions will be used in case a TCP query might fail and we want to try again using UDP. */

# define tentative_tcp_query(st) ((st)->qm==TCP_UDP && ((st)->state==QS_TCPWRITE || ((st)->state==QS_TCPREAD && (st)->iolen==0)))

inline static void switch_to_udp(query_stat_t *st)
{
	st->qm=UDP_ONLY;
	st->myrid=get_rand16();
	st->msg->hdr.id=htons(st->myrid);
	st->state=QS_UDPINITIAL;
	/* st->failed=0; */
}

/* This function will be used in case a UDP reply was truncated and we want to try again using TCP. */

inline static void switch_to_tcp(query_stat_t *st)
{
	/* PDNSD_ASSERT(st->state==QS_INITIAL || st->state==QS_DONE || st->state==QS_CANCELED,
	   "Attempt to switch to TCP while a query is in progress."); */
	st->qm=TCP_ONLY;
	st->state=QS_INITIAL;
	st->failed=0;
}
#endif


/* ------ following is the parallel query code.
 * It has been observed that a whole lot of name servers are just damn lame, with response time
 * of about 1 min. If that slow one is by chance the first server we try, serializing the tries is quite
 * sub-optimal. Also when doing serial queries, the timeout values given in the config will add up, which
 * is not the Right Thing. Now that serial queries are in place, this is still true for CNAME recursion,
 * and for recursion in quest for the holy AA, but not totally for querying multiple servers.
 * The impact on network bandwith should be only marginal (given todays bandwith).
 *
 * The actual strategy is to do (max) PAR_QUERIES parallel queries, and, if these time out or fail, do again
 * that number of queries, until we are successful or there are no more servers to query.
 * Since the memory footprint of a thread is considerably large on some systems, and because we have better
 * control, we will do the parallel queries multiplexed in one thread.
 */

/* The query state machine that is called from p_exec_query. This is called once for initialization (state
 * QS_TCPINITIAL or QS_UDPINITIAL is preset), and the state that it gives back may either be state QS_DONE,
 * in which case it must return a return code other than -1 and is called no more for this server
 * (except perhaps in UDP mode if TCP failed). If p_query_sm returns -1, then the state machine is in a read
 * or write state, and a function higher up the calling chain can setup a poll() or select() together with st->sock.
 * If that poll/select is succesful for that socket, p_exec_query is called again and will hand over to p_query_sm.
 * So, you can assume that read(), write() and recvfrom() will not block at the start of a state handling when you
 * have returned -1 (which means "call again") as last step of the last state handling. */
static int p_query_sm(query_stat_t *st)
{
	int retval=RC_SERVFAIL,rv;

#if !defined(NO_TCP_QUERIES) && !defined(NO_UDP_QUERIES)
 tryagain:
#endif
	switch (st->state){
		/* TCP query code */
#ifndef NO_TCP_QUERIES
	case QS_TCPINITIAL:
		if ((st->sock=socket(PDNSD_PF_INET,SOCK_STREAM,IPPROTO_TCP))==-1) {
			DEBUG_MSG("Could not open socket: %s\n", strerror(errno));
			break;
		}
		/* sin4 or sin6 is intialized, hopefully. */

		/* maybe bind */
		if (!bind_socket(st->sock)) {
			close(st->sock);
			break;
		}

		/* transmit query by tcp*/
		/* make the socket non-blocking */
		{
			int oldflags = fcntl(st->sock, F_GETFL, 0);
			if (oldflags == -1 || fcntl(st->sock,F_SETFL,oldflags|O_NONBLOCK)==-1) {
				DEBUG_PDNSDA_MSG("fcntl error while trying to make socket to %s non-blocking: %s\n", PDNSDA2STR(PDNSD_A(st)),strerror(errno));
				close(st->sock);
				break;
			}
 		}
		st->iolen=0;
#ifdef ENABLE_IPV6
	retry_tcp_connect:
#endif
		if (connect(st->sock,SOCK_ADDR(st),SIN_LEN)==-1) {
			if (errno==EINPROGRESS || errno==EPIPE) {
				st->state=QS_TCPWRITE;
				/* st->event=QEV_WRITE; */ /* wait for writability; the connect is then done */
				return -1;
			} else if (errno==ECONNREFUSED) {
				st->s_errno=errno;
				DEBUG_PDNSDA_MSG("TCP connection refused by %s\n", PDNSDA2STR(PDNSD_A(st)));
				close(st->sock);
				goto tcp_failed; /* We may want to try again using UDP */
			} else {
				/* Since immediate connect() errors do not cost any time, we do not try to switch the
				 * server status to offline */
#ifdef ENABLE_IPV6
				/* if IPv6 connectivity is for some reason unavailable, perhaps the
				   IPv4 fallback address can still be reached. */
				if(!run_ipv4 && (errno==ENETUNREACH || errno==ENETDOWN)
				   && st->a4fallback.s_addr!=INADDR_ANY)
				{
#if DEBUG>0
					char abuf[ADDRSTR_MAXLEN];
					DEBUG_PDNSDA_MSG("Connecting to %s failed: %s, retrying with IPv4 address %s\n",
							 PDNSDA2STR(PDNSD_A(st)),strerror(errno),
							 inet_ntop(AF_INET,&st->a4fallback,abuf,sizeof(abuf)));
#endif
					IPV6_MAPIPV4(&st->a4fallback,&st->a.sin6.sin6_addr);
					st->a4fallback.s_addr=INADDR_ANY;
					goto retry_tcp_connect;
				}
#endif
				DEBUG_PDNSDA_MSG("Error while connecting to %s: %s\n", PDNSDA2STR(PDNSD_A(st)),strerror(errno));
				close(st->sock);
				break;
			}
		}
		st->state=QS_TCPWRITE;
		/* st->event=QEV_WRITE; */
		/* fall through in case of not EINPROGRESS */
	case QS_TCPWRITE:
		{
			int rem= dnsmsghdroffset + st->transl - st->iolen;
			if(rem>0) {
				rv=write(st->sock,((unsigned char*)st->msg)+st->iolen,rem);
				if(rv==-1) {
					if(errno==EWOULDBLOCK)
						return -1;
					st->s_errno=errno;
					close(st->sock);
					if (st->iolen==0 &&
					    (st->s_errno==ECONNREFUSED || st->s_errno==ECONNRESET ||
					     st->s_errno==EPIPE))
					{
						/* This error may be delayed from connect() */
						DEBUG_PDNSDA_MSG("TCP connection to %s failed: %s\n", PDNSDA2STR(PDNSD_A(st)),strerror(st->s_errno));
						goto tcp_failed; /* We may want to try again using UDP */
					}
					DEBUG_PDNSDA_MSG("Error while sending data to %s: %s\n", PDNSDA2STR(PDNSD_A(st)),strerror(st->s_errno));
					break;
				}
				st->iolen += rv;
				if(rv<rem)
					return -1;
			}
		}
		st->state=QS_TCPREAD;
		st->iolen=0;
		/* st->event=QEV_READ; */
		/* fall through */
	case QS_TCPREAD:
	        if(st->iolen==0) {
			uint16_t recvl_net;
			rv=read(st->sock,&recvl_net,sizeof(recvl_net));
			if(rv==-1 && errno==EWOULDBLOCK)
				return -1;
			if(rv!=sizeof(recvl_net))
				goto error_receiv_data;
			st->iolen=rv;
			st->recvl=ntohs(recvl_net);
			if(!(st->recvbuf=(dns_hdr_t *)realloc_or_cleanup(st->recvbuf,st->recvl))) {
				close(st->sock);
				DEBUG_MSG("Out of memory in query.\n");
				retval=RC_FATALERR;
				break;
			}
		}
		{
			int offset=st->iolen-sizeof(uint16_t);
			int rem=st->recvl-offset;
			if(rem>0) {
				rv=read(st->sock,((unsigned char*)st->recvbuf)+offset,rem);
				if(rv==-1) {
					if(errno==EWOULDBLOCK)
						return -1;
					goto error_receiv_data;
				}
				if(rv==0)
					goto error_receiv_data; /* unexpected EOF */
				st->iolen += rv;
				if(rv<rem)
					return -1;
			}
		}
		close(st->sock);
		st->state=QS_DONE;
		return RC_OK;
	error_receiv_data:
		if(rv==-1) st->s_errno=errno;
		DEBUG_PDNSDA_MSG("Error while receiving data from %s: %s\n", PDNSDA2STR(PDNSD_A(st)),
				 rv==-1?strerror(errno):(rv==0 && st->iolen==0)?"no data":"incomplete data");
		close(st->sock);
	tcp_failed:
#if !defined(NO_TCP_QUERIES) && !defined(NO_UDP_QUERIES)
		if(st->qm==TCP_UDP) {
			switch_to_udp(st);
			DEBUG_PDNSDA_MSG("TCP query to %s failed. Trying to use UDP.\n", PDNSDA2STR(PDNSD_A(st)));
			goto tryagain;
		}
#endif
		break;
#endif

#ifndef NO_UDP_QUERIES
		/* UDP query code */
	case QS_UDPINITIAL:
		if ((st->sock=socket(PDNSD_PF_INET,SOCK_DGRAM,IPPROTO_UDP))==-1) {
			DEBUG_MSG("Could not open socket: %s\n", strerror(errno));
			break;
		}

		/* maybe bind */
		if (!bind_socket(st->sock)) {
			close(st->sock);
			break;
		}

		/* connect */
#ifdef ENABLE_IPV6
	retry_udp_connect:
#endif
		if (connect(st->sock,SOCK_ADDR(st),SIN_LEN)==-1) {
			if (errno==ECONNREFUSED) st->s_errno=errno;
#ifdef ENABLE_IPV6
			/* if IPv6 connectivity is for some reason unavailable, perhaps the
			   IPv4 fallback address can still be reached. */
			else if(!run_ipv4 && (errno==ENETUNREACH || errno==ENETDOWN)
				   && st->a4fallback.s_addr!=INADDR_ANY)
			{
#if DEBUG>0
				char abuf[ADDRSTR_MAXLEN];
				DEBUG_PDNSDA_MSG("Connecting to %s failed: %s, retrying with IPv4 address %s\n",
						 PDNSDA2STR(PDNSD_A(st)),strerror(errno),
						 inet_ntop(AF_INET,&st->a4fallback,abuf,sizeof(abuf)));
#endif
				IPV6_MAPIPV4(&st->a4fallback,&st->a.sin6.sin6_addr);
				st->a4fallback.s_addr=INADDR_ANY;
				goto retry_udp_connect;
			}
#endif
			DEBUG_PDNSDA_MSG("Error while connecting to %s: %s\n", PDNSDA2STR(PDNSD_A(st)),strerror(errno));
			close(st->sock);
			break;
		}

		/* transmit query by udp*/
		/* send will hopefully not block on a freshly opened socket (the buffer
		 * must be empty) */
		if (send(st->sock,&st->msg->hdr,st->transl,0)==-1) {
			st->s_errno=errno;
			DEBUG_PDNSDA_MSG("Error while sending data to %s: %s\n", PDNSDA2STR(PDNSD_A(st)),strerror(errno));
			close(st->sock);
			break;
		}
		st->state=QS_UDPRECEIVE;
		/* st->event=QEV_READ; */
		return -1;
	case QS_UDPRECEIVE:
	{
		int udpbufsize= (st->edns_query?global.udpbufsize:UDP_BUFSIZE);
		if(!(st->recvbuf=(dns_hdr_t *)realloc_or_cleanup(st->recvbuf,udpbufsize))) {
			close(st->sock);
			DEBUG_MSG("Out of memory in query.\n");
			retval=RC_FATALERR;
			break;
		}
		if ((rv=recv(st->sock,st->recvbuf,udpbufsize,0))==-1) {
			st->s_errno=errno;
			DEBUG_PDNSDA_MSG("Error while receiving data from %s: %s\n", PDNSDA2STR(PDNSD_A(st)),strerror(errno));
			close(st->sock);
			break;
		}
		st->recvl=rv;
		if (st->recvl<sizeof(dns_hdr_t) || ntohs(st->recvbuf->id)!=st->myrid) {
			DEBUG_MSG("Bad answer received. Ignoring it.\n");
			/* no need to care about timeouts here. That is done at an upper layer. */
			st->state=QS_UDPRECEIVE;
			/* st->event=QEV_READ; */
			return -1;
		}
		close(st->sock);
		st->state=QS_DONE;
		return RC_OK;
	}
#endif
	}

	/* If we get here, something has gone wrong. */
	st->state=QS_DONE;
	return retval; /* should be either RC_SERVFAIL or RC_FATALERR */
}

static dns_cent_t *lookup_cent_array(dns_cent_array ca, const unsigned char *nm)
{
	int i,n=DA_NEL(ca);
	for(i=0;i<n;++i) {
		dns_cent_t *ce=&DA_INDEX(ca,i);
		if(rhnicmp(ce->qname,nm))
			return ce;
	}
	return NULL;
}

/* Extract the minimum ttl field from the SOA record stored in an rr bucket. */
static time_t soa_minimum(rr_bucket_t *rrs)
{
	uint32_t minimum;
	unsigned char *p=(unsigned char *)(rrs->data);

	/* Skip owner and maintainer. Lengths are validated in cache. */
	p=skiprhn(skiprhn(p));
	/* Skip serial, refresh, retry, expire fields. */
	p += 4*sizeof(uint32_t);
	GETINT32(minimum,p);
	return minimum;
}

/*
 * The function that will actually execute a query. It takes a state structure in st.
 * st->state must be set to QS_INITIAL before calling.
 * This may return one of the RC_* codes, where RC_OK indicates success, the other
 * RC codes indicate the appropriate errors. -1 is the return value that indicates that
 * you should call p_exec_query again with the same state for the result until you get
 * a return value >0. Alternatively, call p_cancel_query to cancel it.
 * Timeouts are already handled by this function.
 * Any records that the query has yielded and that are not a direct answer to the query
 * (i.e. are records for other domains) are added to the cache, while the direct answers
 * are returned in ent.
 * All ns records, to whomever they might belong, are additionally returned in the ns list.
 * Free it when done.
 * This function calls another query state machine function that supports TCP and UDP.
 *
 * If you want to tell me that this function has a truly ugly coding style, ah, well...
 * You are right, somehow, but I feel it is conceptually elegant ;-)
 */
static int p_exec_query(dns_cent_t **entp, const unsigned char *name, int thint,
			query_stat_t *st, dlist *ns, unsigned char *c_soa)
{
	int rv,rcode;
	unsigned short rd;

	switch (st->state){
	case QS_INITIAL: {
		size_t transl,allocsz;
		unsigned int rrnlen=0;

		allocsz= sizeof(dns_msg_t);
		if(name) {
			rrnlen=rhnlen(name);
			allocsz += rrnlen+4;
			if(st->edns_query)
				allocsz += sizeof_opt_pseudo_rr;
		}
		st->msg=(dns_msg_t *)pdnsd_malloc(allocsz);
		if (!st->msg) {
			st->state=QS_DONE;
 			return RC_FATALERR; /* unrecoverable error */
		}
		st->myrid=get_rand16();
		st->msg->hdr.id=htons(st->myrid);
		st->msg->hdr.qr=QR_QUERY;
		st->msg->hdr.opcode=OP_QUERY;
		st->msg->hdr.aa=0;
		st->msg->hdr.tc=0;
		st->msg->hdr.rd=(name && st->trusted);
		st->msg->hdr.ra=0;
		st->msg->hdr.z=0;
		st->msg->hdr.ad=0;
		st->msg->hdr.cd=0;
		st->msg->hdr.rcode=RC_OK;
		st->msg->hdr.qdcount=htons(name!=NULL);
		st->msg->hdr.ancount=0;
		st->msg->hdr.nscount=0;
		st->msg->hdr.arcount=0;

		transl= sizeof(dns_hdr_t);
		if(name) {
			unsigned char *p = mempcpy((unsigned char *)(&st->msg->hdr+1),name,rrnlen);
			unsigned short qtype=(st->lean_query?thint:QT_ALL);
			PUTINT16(qtype,p);
			PUTINT16(C_IN,p);
			transl += rrnlen+4;
			if(st->edns_query)
				add_opt_pseudo_rr(&st->msg,&transl,&allocsz,
						  global.udpbufsize,RC_OK,0,0);
		}
		st->transl=transl;
#ifndef NO_TCP_QUERIES
		st->msg->len=htons(st->transl);
#endif
		st->recvbuf=NULL;
		st->state=(USE_UDP(st)?QS_UDPINITIAL:QS_TCPINITIAL);
		/* fall through */
	}
	QS_QUERY_CASES:
	tryagain:
		rv=p_query_sm(st);
		if (rv==-1) {
			return -1;
		}
		if (rv!=RC_OK) {
			pdnsd_free(st->msg);
			pdnsd_free(st->recvbuf);
			st->state=QS_DONE;
			if(st->needs_testing) {
				switch(st->s_errno) {
				case ENETUNREACH:  /* network unreachable */
				case EHOSTUNREACH: /* host unreachable */
				case ENOPROTOOPT:  /* protocol unreachable */
				case ECONNREFUSED: /* port unreachable */
				case ENETDOWN:     /* network down */
				case EHOSTDOWN:    /* host down */
#ifdef ENONET
				case ENONET:       /* machine not on the network */
#endif
					/* Mark this server as down for a period of time */
					sched_server_test(PDNSD_A(st),1,0);
					st->needs_testing=0;
				}
			}
			return rv;
		}
		/* rv==RC_OK */
		DEBUG_PDNSDA_MSG("Received reply from %s (msg len=%u).\n", PDNSDA2STR(PDNSD_A(st)), st->recvl);
		DEBUG_DUMP_DNS_MSG(st->recvbuf, st->recvl);

		/* Basic sanity checks */
		if (st->recvl<sizeof(dns_hdr_t)) {
			DEBUG_MSG("Message too short!\n");
			goto discard_reply;
		}
		{
			uint16_t recvid=ntohs(st->recvbuf->id);
			if (recvid!=st->myrid) {
				DEBUG_MSG("ID mismatch: expected %04x, got %04x!\n", st->myrid, recvid);
				goto discard_reply;
			}
		}
		if (st->recvbuf->qr!=QR_RESP) {
			DEBUG_MSG("The QR bit indicates this is a query, not a response!\n");
			goto discard_reply;
		}
		if (st->recvbuf->opcode!=OP_QUERY) {
			DEBUG_MSG("Not a reply to a standard query (opcode=%u).\n",st->recvbuf->opcode);
			goto discard_reply;
		}

		rcode=st->recvbuf->rcode;
#if DEBUG>0
		{
			char flgsbuf[DNSFLAGSMAXSTRSIZE];
			DEBUG_MSG("rcode=%u (%s), flags:%s\n", rcode, get_ename(rcode), dnsflags2str(st->recvbuf, flgsbuf));
		}
#endif
		if (st->recvbuf->z!=0) {
			DEBUG_MSG("Malformed response (nonzero Z bit).\n");
			goto discard_reply;
		}

		if(st->needs_testing) {
			/* We got an answer from this server, so don't bother with up tests for a while. */
			sched_server_test(PDNSD_A(st),1,1);
			st->needs_testing=0;
		}

		rv=rcode;
		if(rcode==RC_OK || rcode==RC_NAMEERR) {
			/* success or at least no requery is needed */
			st->state=QS_DONE;
			break;
		}
		else if (entp) {
			if(rcode==RC_SERVFAIL || rcode==RC_NOTSUPP || rcode==RC_REFUSED) {
				if (st->msg->hdr.rd && !st->recvbuf->ra) {
					/* seems as if we have got no recursion available.
					   We will have to do it by ourselves (sigh...) */
					DEBUG_PDNSDA_MSG("Server %s returned error code: %s."
							 " Maybe does not support recursive query?"
							 " Querying non-recursively.\n",
							 PDNSDA2STR(PDNSD_A(st)),get_ename(rcode));
					st->msg->hdr.rd=0;
					goto resetstate_tryagain;
				}
				else if(rcode!=RC_SERVFAIL && st->edns_query && st->msg->hdr.arcount)
					goto try_withoutedns;
				else if (st->recvbuf->ancount && st->auth_serv==2) {
					/* The name server returned a failure code,
					   but the answer section is not empty,
					   and the answer is from a server lower down the call chain.
					   Use this answer tentatively (it may be the
					   best we can get), but remember the failure. */
					DEBUG_PDNSDA_MSG("Server %s returned error code: %s,"
							 " but the answer section is not empty."
							 " Using the answer tentatively.\n",
							 PDNSDA2STR(PDNSD_A(st)),get_ename(rcode));
					st->failed=3;
					st->state=QS_DONE;
					break;
				}
			}
			else if(rcode==RC_FORMAT && st->edns_query && st->msg->hdr.arcount)
			try_withoutedns: {
				size_t transl;
				/* Perhaps the remote server barfs when the query
				   contains an OPT RR in the additional section.
				   Try again with an empty addtional section. */
				DEBUG_PDNSDA_MSG("Server %s returned error code: %s."
						 " Maybe cannot handle EDNS?"
						 " Querying with empty additional section.\n",
						 PDNSDA2STR(PDNSD_A(st)),get_ename(rcode));
				transl=remove_opt_pseudo_rr(st->msg,st->transl);
				if(transl!=0 && st->msg->hdr.arcount==0) {
					st->transl=transl;
#ifndef NO_TCP_QUERIES
					st->msg->len=htons(st->transl);
#endif
					st->edns_query=0;
				resetstate_tryagain:
					st->myrid=get_rand16();
					st->msg->hdr.id=htons(st->myrid);
					st->state=(USE_UDP(st)?QS_UDPINITIAL:QS_TCPINITIAL);
					goto tryagain;
				}
				else {
					DEBUG_PDNSDA_MSG("Internal error: could not remove additional section from query"
							 " to server %s\n", PDNSDA2STR(PDNSD_A(st)));
				}
			}
		}

	discard_reply:
		/* report failure */
		pdnsd_free(st->msg);
		pdnsd_free(st->recvbuf);
		/*close(st->sock);*/
		st->state=QS_DONE;
#if DEBUG>0
		if(entp) {
			DEBUG_PDNSDA_MSG("Discarding reply from server %s\n", PDNSDA2STR(PDNSD_A(st)));
		}
#endif
		if (rv!=RC_OK)
			return rv;

		return RC_SERVFAIL; /* mock error code */

	default: /* we shouldn't get here */
		st->state=QS_DONE;
		return RC_SERVFAIL; /* mock error code */
	}

        /* If we reach this code, we have successfully received an answer,
	 * because we have returned error codes on errors or -1 on AGAIN conditions.
	 * So we *should* have a usable dns record in recvbuf by now.
	 */
	rd= st->msg->hdr.rd; /* Save the 'Recursion Desired' bit of the query. */
	pdnsd_free(st->msg);
	if(entp) {
		time_t queryts=time(NULL);
		size_t lcnt= ((size_t)st->recvl) - sizeof(dns_hdr_t);
		unsigned char *rrp=(unsigned char *)(st->recvbuf+1);
		dns_cent_array secs[3]={NULL,NULL,NULL};
#		define ans_sec  secs[0]
#		define auth_sec secs[1]
#		define add_sec  secs[2]
		unsigned short qtype,flags,aa,neg_ans=0,reject_ans=0,num_ns=0;
		int numoptrr;
		edns_info_t ednsinfo= {0};

		if (ntohs(st->recvbuf->qdcount)!=1) {
			DEBUG_PDNSDA_MSG("Bad number of query records in answer from %s\n",
					 PDNSDA2STR(PDNSD_A(st)));
			rv=RC_SERVFAIL;
			goto free_recvbuf_return;
		}
		/* check & skip the query record. */
		{
			unsigned char nbuf[DNSNAMEBUFSIZE];
			if ((rv=decompress_name((unsigned char *)st->recvbuf, st->recvl, &rrp, &lcnt, nbuf, NULL))!=RC_OK) {
				DEBUG_PDNSDA_MSG("Cannot decompress QNAME in answer from %s\n",
						 PDNSDA2STR(PDNSD_A(st)));
				rv=RC_SERVFAIL;
				goto free_recvbuf_return;
			}
			if(!rhnicmp(nbuf,name)) {
				DEBUG_PDNSDA_MSG("Answer from %s does not match query.\n",
						 PDNSDA2STR(PDNSD_A(st)));
				rv=RC_SERVFAIL;
				goto free_recvbuf_return;
			}
		}

		qtype=(st->lean_query?thint:QT_ALL);
		if (lcnt<4) {
			DEBUG_PDNSDA_MSG("Format error in reply from %s (message truncated in qtype or qclass).\n",
					 PDNSDA2STR(PDNSD_A(st)));
			rv=RC_SERVFAIL; /* mock error code */
			goto free_recvbuf_return;
		}
		{
			unsigned short qt,qc;
			GETINT16(qt,rrp);
			GETINT16(qc,rrp);
			if(qt!=qtype) {
				DEBUG_PDNSDA_MSG("qtype in answer (%u) from %s does not match expected qtype (%u).\n",
						 qt,PDNSDA2STR(PDNSD_A(st)),qtype);
				rv=RC_SERVFAIL;
				goto free_recvbuf_return;
			}
		}
		lcnt-=4;

		st->aa= (st->recvbuf->aa && !st->failed);
		st->tc=st->recvbuf->tc;

		/* Don't flag cache entries from a truncated reply as authoritative. */
		aa= (st->aa && !st->tc);
		flags=st->flags;
		if (aa) flags|=CF_AUTH;


		/* Initialize a dns_cent_t in the array for the answer section */
		if (!(ans_sec=DA_GROW1(ans_sec))) {
			rv=RC_FATALERR; /* unrecoverable error */
			goto free_recvbuf_return;
		}
		/* By marking DF_AUTH, we mean authoritative AND complete. */
		if (!init_cent(&DA_INDEX(ans_sec,0), name, 0, 0, (aa && qtype==QT_ALL)?DF_AUTH:0  DBG1)) {
			rv=RC_FATALERR; /* unrecoverable error */
			goto free_centarrays_recvbuf_return;
		}

		/* Now read the answer, authority and additional sections,
		   storing the results in the arrays ans_sec,auth_sec and add_sec.
		*/
		numoptrr=0;
		rv=rrs2cent((unsigned char *)st->recvbuf, st->recvl, &rrp, &lcnt, ntohs(st->recvbuf->ancount),
			    flags, queryts, &ans_sec, &numoptrr, &ednsinfo);
#if DEBUG>0
		if(numoptrr!=0) {
			DEBUG_MSG("Answer section in reply contains %d OPT pseudo-RRs!\n", numoptrr);
		}
#endif
		numoptrr=0;
		if(rv==RC_OK) {
			uint16_t nscount=ntohs(st->recvbuf->nscount);
			if (nscount) {
				rv=rrs2cent((unsigned char *)st->recvbuf, st->recvl, &rrp, &lcnt, nscount,
					    flags|CF_ADDITIONAL, queryts, &auth_sec, &numoptrr, &ednsinfo);
#if DEBUG>0
				if(numoptrr!=0) {
					DEBUG_MSG("Authority section in reply contains %d OPT pseudo-RRs!\n", numoptrr);
				}
#endif
			}
		}

		numoptrr=0;
		if(rv==RC_OK) {
			uint16_t arcount=ntohs(st->recvbuf->arcount);
			if (arcount) {
				rv=rrs2cent((unsigned char *)st->recvbuf, st->recvl, &rrp, &lcnt, arcount,
					    flags|CF_ADDITIONAL, queryts, &add_sec, &numoptrr, &ednsinfo);
				if(numoptrr!=0) {
#if DEBUG>0
					if(numoptrr!=1) {
						DEBUG_MSG("Additional section in reply contains %d OPT pseudo-RRs!\n", numoptrr);
					}
					DEBUG_PDNSDA_MSG("Reply from %s contains OPT pseudosection: EDNS version = %u, udp size = %u, flag DO=%u\n",
							 PDNSDA2STR(PDNSD_A(st)), ednsinfo.version, ednsinfo.udpsize, ednsinfo.do_flg);
#endif
					if(rcode!=ednsinfo.rcode) {
						DEBUG_PDNSDA_MSG("Reply from %s contains unexpected EDNS rcode %u (%s)!\n",
								 PDNSDA2STR(PDNSD_A(st)), ednsinfo.rcode, get_ename(ednsinfo.rcode));
						rcode=ednsinfo.rcode;
						/* Mark as failed, but use answer tentatively. */
						if(!st->failed) st->failed=1;
					}
				}
			}
		}

		if(!(rv==RC_OK || (rv==RC_TRUNC && st->recvbuf->tc))) {
			DEBUG_PDNSDA_MSG(rv==RC_FORMAT?"Format error in reply from %s.\n":
					 rv==RC_TRUNC?"Format error in reply from %s (message unexpectedly truncated).\n":
					 rv==RC_SERVFAIL?"Inconsistent timestamps in reply from %s.\n":
					 "Out of memory while processing reply from %s.\n",
					 PDNSDA2STR(PDNSD_A(st)));
			if(rv==RC_SERVFAIL) {
				/* Inconsistent ttl timestamps and we are
				   enforcing strict RFC 2181 compliance.
				   Mark as failed, but use answer tentatively. */
				if(!st->failed) st->failed=1;
			}
			else {
				if(rv!=RC_FATALERR) rv=RC_SERVFAIL;
				goto free_ent_centarrays_recvbuf_return;
			}
		}

		{
			/* Remember references to NS and SOA records in the answer or authority section
			   so that we can add this information to our own reply. */
			int i,n=DA_NEL(ans_sec);
			for(i=0;i<n;++i) {
				dns_cent_t *cent=&DA_INDEX(ans_sec,i);
				unsigned scnt=rhnsegcnt(cent->qname);

				if(getrrset_NS(cent))
					cent->c_ns=scnt;
				if(getrrset_SOA(cent))
					cent->c_soa=scnt;

				if((qtype>=QT_MIN && qtype<=QT_MAX) ||
				   (/* (qtype>=T_MIN && qtype<=T_MAX) && */ getrrset(cent,qtype)) ||
				   (n==1 && cent->num_rrs==0))
				{
					/* Match this name with names in the authority section */
					int j,m=DA_NEL(auth_sec);
					for(j=0;j<m;++j) {
						dns_cent_t *ce=&DA_INDEX(auth_sec,j);
						unsigned int ml,rem;
						ml=domain_match(ce->qname,cent->qname, &rem, NULL);
						if(rem==0 &&
						   /* Don't accept records for the root domain from name servers
						      that were not listed in the configuration file. */
						   (ml || st->auth_serv!=2)) {
							if(getrrset_NS(ce)) {
								if(cent->c_ns==cundef || cent->c_ns<ml)
									cent->c_ns=ml;
							}
							if(getrrset_SOA(ce)) {
								if(cent->c_soa==cundef || cent->c_soa<ml)
									cent->c_soa=ml;
							}
						}
					}
				}
			}
		}

		/* Check whether the answer contains an IP address that should be rejected. */
		if(have_rejectlist(st)) {
			int i;
			int na4=nreject_a4(st);
			addr4maskpair_t *a4arr=rejectlist_a4(st);
#if ALLOW_LOCAL_AAAA
			int na6=nreject_a6(st);
			addr6maskpair_t *a6arr=rejectlist_a6(st);
#endif
			/* Check addresses in the answer, authority and additional sections. */
			for(i=0;i<3;++i) {
				dns_cent_array sec=secs[i];
				int j,nce=DA_NEL(sec);
				for(j=0;j<nce;++j) {
					dns_cent_t *cent=&DA_INDEX(sec,j);
					rr_set_t *rrset=getrrset_A(cent);
					if(rrset && na4) {
						/* This is far from the world's most efficient matching algorithm,
						   but it should work OK as long as the numbers involved are small.
						*/
						rr_bucket_t *rr;
						for(rr=rrset->rrs; rr; rr=rr->next) {
							struct in_addr *a=(struct in_addr *)(rr->data);
							int k;
							for(k=0;k<na4;++k) {
								addr4maskpair_t *am = &a4arr[k];
								if(ADDR4MASK_EQUIV(a,&am->a,&am->mask)) {
#if DEBUG>0
									unsigned char nmbuf[DNSNAMEBUFSIZE]; char abuf[ADDRSTR_MAXLEN];
									DEBUG_PDNSDA_MSG("Rejecting answer from server %s because it contains an A record"
											 " for \"%s\" with an address in the reject list: %s\n",
											 PDNSDA2STR(PDNSD_A(st)),
											 rhn2str(cent->qname,nmbuf,sizeof(nmbuf)),
											 inet_ntop(AF_INET,a,abuf,sizeof(abuf)));
#endif
									reject_ans=1; goto rejectlist_scan_done;
								}
							}
						}
					}
#if ALLOW_LOCAL_AAAA
					rrset=getrrset_AAAA(cent);
					if(rrset && na6) {
						rr_bucket_t *rr;
						for(rr=rrset->rrs; rr; rr=rr->next) {
							struct in6_addr *a=(struct in6_addr *)(rr->data);
							int k;
							for(k=0;k<na6;++k) {
								addr6maskpair_t *am = &a6arr[k];
								if(ADDR6MASK_EQUIV(a,&am->a,&am->mask)) {
#if DEBUG>0
									unsigned char nmbuf[DNSNAMEBUFSIZE]; char abuf[INET6_ADDRSTRLEN];
									DEBUG_PDNSDA_MSG("Rejecting answer from server %s because it contains an AAAA record"
											 " for \"%s\" with an address in the reject list: %s\n",
											 PDNSDA2STR(PDNSD_A(st)),
											 rhn2str(cent->qname,nmbuf,sizeof(nmbuf)),
											 inet_ntop(AF_INET6,a,abuf,sizeof(abuf)));
#endif
									reject_ans=1; goto rejectlist_scan_done;
								}
							}
						}
					}
#endif
				}
			}
		rejectlist_scan_done:;
		}

		/* negative caching for domains */
		if (rcode==RC_NAMEERR) {
			DEBUG_PDNSDA_MSG("Server %s returned error code: %s\n", PDNSDA2STR(PDNSD_A(st)),get_ename(rcode));
		name_error:
			neg_ans=1;
			{
				/* We did not get what we wanted. Cache according to policy */
				dns_cent_t *ent=&DA_INDEX(ans_sec,0);
				int neg_domain_pol=global.neg_domain_pol;
				if (neg_domain_pol==C_ON || (neg_domain_pol==C_AUTH && st->recvbuf->aa)) {
					time_t ttl=global.neg_ttl;

					/* Try to find a SOA record that came with the reply.
					 */
					if(ent->c_soa!=cundef) {
						unsigned scnt=rhnsegcnt(name);
						dns_cent_t *cent;
						if(ent->c_soa<scnt && (cent=lookup_cent_array(auth_sec,skipsegs(name,scnt-ent->c_soa)))) {
							rr_set_t *rrset=getrrset_SOA(cent);
							if (rrset && rrset->rrs) {
								time_t min=soa_minimum(rrset->rrs);
								ttl=rrset->ttl;
								if(ttl>min)
									ttl=min;
							}
						}
					}
					DEBUG_RHN_MSG("Caching domain %s negative with ttl %li\n",RHN2STR(name),(long)ttl);
					negate_cent(ent,ttl,queryts);
					if(st->nocache) ent->flags |= DF_NOCACHE;
					goto cleanup_return_OK;
				} else {
					if(c_soa) *c_soa=ent->c_soa;
					free_cent(ent  DBG1);
					rv=RC_NAMEERR;
					goto add_additional;
				}
			}
		}

		if(reject_ans) {
			if(reject_policy(st)==C_NEGATE && st->failed<=1)
				goto name_error;
			else {
				rv=RC_SERVFAIL;
				goto free_ent_centarrays_recvbuf_return;
			}
		}

		if(global.deleg_only_zones && st->auth_serv<3) { /* st->auth_serv==3 means this server is a root-server. */
			int missingdelegation,authcnt;
			/* The deleg_only_zones data may change due to runtime reconfiguration,
			   therefore use locks. */
			lock_server_data();
			missingdelegation=0; authcnt=0;
			{
				int i,n=DA_NEL(global.deleg_only_zones); unsigned rem,zrem;
				for(i=0;i<n;++i) {
					if(domain_match(name,DA_INDEX(global.deleg_only_zones,i),&rem,&zrem) && zrem==0)
						goto zone_match;
				}
				goto delegation_OK;
			zone_match:
				/* The name queried matches a delegation-only zone. */
				if(rem) {
					/* Check if we can find delegation in the answer or authority section. */
					/* dns_cent_array secs[2]={ans_sec,auth_sec}; */
					int j;
					for(j=0;j<2;++j) {
						dns_cent_array sec=secs[j];
						int k,m=DA_NEL(sec);
						for(k=0;k<m;++k) {
							dns_cent_t *ce=&DA_INDEX(sec,k);
							if(getrrset_NS(ce) || getrrset_SOA(ce)) {
								/* Found a NS or SOA record in the answer or authority section. */
								int l;
								++authcnt;
								for(l=0;l<n;++l) {
									if(domain_match(ce->qname,DA_INDEX(global.deleg_only_zones,l),&rem,&zrem) && zrem==0) {
										if(rem) break;
										else    goto try_next_auth;
									}
								}
								goto delegation_OK;
							}
						try_next_auth:;
						}
					}
#if DEBUG>0
					{
						unsigned char nmbuf[DNSNAMEBUFSIZE],zbuf[DNSNAMEBUFSIZE];
						DEBUG_PDNSDA_MSG(authcnt?"%s is in %s zone, but no delegation found in answer returned by server %s\n"
								 :"%s is in %s zone, but no authority information provided by server %s\n",
								 rhn2str(name,nmbuf,sizeof(nmbuf)), rhn2str(DA_INDEX(global.deleg_only_zones,i),zbuf,sizeof(zbuf)),
								 PDNSDA2STR(PDNSD_A(st)));
					}
#endif
					missingdelegation=1;
				}
			delegation_OK:;
			}
			unlock_server_data();

			if(missingdelegation) {
				if(authcnt && st->failed<=1) {
					/* Treat this as a nonexistant name. */
					goto name_error;
				}
				else if(st->auth_serv<2) {
					/* If this is one of the servers obtained from the list
					   pdnsd was configured with, treat this as a failure.
					   Hopefully one of the other servers in the list will
					   return a non-empty authority section.
					*/
					rv=RC_SERVFAIL;
					goto free_ent_centarrays_recvbuf_return;
				}
			}
		}

		{
			/* Negative caching of rr sets */
			dns_cent_t *ent=&DA_INDEX(ans_sec,0);

			if(!ent->num_rrs) neg_ans=1;

			if (thint>=T_MIN && thint<=T_MAX && !getrrset(ent,thint) && !st->tc && st->failed<=1) {
				/* We did not get what we wanted. Cache according to policy */
				int neg_rrs_pol=global.neg_rrs_pol;
				if (neg_rrs_pol==C_ON || (neg_rrs_pol==C_AUTH && aa) ||
				    (neg_rrs_pol==C_DEFAULT && (aa || (rd && st->recvbuf->ra))))
				{
					time_t ttl=global.neg_ttl;
					rr_set_t *rrset=getrrset_SOA(ent);
					dns_cent_t *cent;
					unsigned scnt;
					/* If we received a SOA, we should take the ttl of that record. */
					if ((rrset && rrset->rrs) ||
					    /* Try to find a SOA record higher up the hierarchy that came with the reply. */
					    ((cent=lookup_cent_array(auth_sec,
								     (ent->c_soa!=cundef && ent->c_soa<(scnt=rhnsegcnt(name)))?
								     skipsegs(name,scnt-ent->c_soa):
								     name)) &&
					     (rrset=getrrset_SOA(cent)) && rrset->rrs))
					{
						time_t min=soa_minimum(rrset->rrs);
						ttl=rrset->ttl;
						if(ttl>min)
							ttl=min;
					}
					DEBUG_RHN_MSG("Caching type %s for domain %s negative with ttl %li\n",getrrtpname(thint),RHN2STR(name),(long)ttl);
					if (!add_cent_rrset_by_type(ent, thint, ttl, queryts, CF_NEGATIVE|flags  DBG1)) {
						rv=RC_FATALERR;
						goto free_ent_centarrays_recvbuf_return;
					}
				}
			}
		}

		if (st->failed<=1) {
			/* The domain names of all name servers found in the answer and authority sections are placed in *ns,
			   which is automatically grown. */
			/* dns_cent_array secs[2]={ans_sec,auth_sec}; */
			int i;
			for(i=0;i<2;++i) {
				dns_cent_array sec=secs[i];
				int j,n=DA_NEL(sec);
				for(j=0;j<n;++j) {
					dns_cent_t *cent=&DA_INDEX(sec,j);
					unsigned int rem;
					/* Don't accept records for the root domain from name servers
					   that were not listed in the configuration file. */
					if((*(cent->qname) || st->auth_serv!=2) &&
					   /* Don't accept possibly poisoning nameserver entries in paranoid mode */
					   (st->trusted || !st->nsdomain || (domain_match(st->nsdomain, cent->qname, &rem,NULL),rem==0)) &&
					   /* The following test is actually redundant and should never fail. */
					   *(cent->qname)!=0xff)
					{
						/* Some nameservers obviously choose to send SOA records instead of NS ones.
						 * Although I think that this is poor behaviour, we'll have to work around that. */
						static const unsigned short nstypes[2]={T_NS,T_SOA};
						int k;
						for(k=0;k<2;++k) {
							rr_set_t *rrset=getrrset(cent,nstypes[k]);
							if(rrset) {
								rr_bucket_t *rr;
								unsigned short first=1;
								for(rr=rrset->rrs; rr; rr=rr->next) {
									size_t sz1,sz2;
									unsigned char *p;
									/* Skip duplicate records */
									for(p=dlist_first(*ns); p; p=dlist_next(p)) {
										if(rhnicmp(*p==0xff?p+1:skiprhn(p),(unsigned char *)(rr->data)))
											goto next_nsr;
									}
									/* add to the nameserver list.
									   Here we use a little compression trick: if
									   the first byte of a name is 0xff, this means
									   repeat the previous name.
									 */
									sz1= (first?rhnlen(cent->qname):1);
									sz2=rhnlen((unsigned char *)(rr->data));
									if (!(*ns=dlist_grow(*ns,sz1+sz2))) {
										rv=RC_FATALERR;
										goto free_ent_centarrays_recvbuf_return;
									}
									p=dlist_last(*ns);
									if(first) {
										first=0;
										p=mempcpy(p,cent->qname,sz1);
									}
									else
										*p++ = 0xff;  /* 0xff means 'idem' */
									/* This will only copy the first name, which is the NS */
									memcpy(p,(unsigned char *)(rr->data),sz2);
									++num_ns;
								next_nsr:;
								}
							}
						}
					}
				}
			}
		}
	cleanup_return_OK:
		if(st->failed && neg_ans && num_ns==0) {
			DEBUG_PDNSDA_MSG("Answer from server %s does not contain usable records.\n",
					 PDNSDA2STR(PDNSD_A(st)));
			rv=RC_SERVFAIL;
			goto free_ns_ent_centarrays_recvbuf_return;
		}
		if(!(*entp=malloc(sizeof(dns_cent_t)))) {
			rv=RC_FATALERR;
			goto free_ns_ent_centarrays_recvbuf_return;
		}
		**entp=DA_INDEX(ans_sec,0);
		rv=RC_OK;
	add_additional:
		if (!st->failed && !reject_ans) {
			/* Add the additional RRs to the cache. */
			/* dns_cent_array secs[3]={ans_sec,auth_sec,add_sec}; */
			int i;
#if DEBUG>0
			if(debug_p && neg_ans) {
				int j,n=DA_NEL(ans_sec);
				for(j=1; j<n; ++j) {
					unsigned char nmbuf[DNSNAMEBUFSIZE],nmbuf2[DNSNAMEBUFSIZE];
					DEBUG_PDNSDA_MSG("Reply from %s is negative for %s, dropping record(s) for %s in answer section.\n",
							 PDNSDA2STR(PDNSD_A(st)),
							 rhn2str(name,nmbuf,sizeof(nmbuf)),
							 rhn2str(DA_INDEX(ans_sec,j).qname,nmbuf2,sizeof(nmbuf2)));
				}
			}
#endif
			for(i=neg_ans; i<3; ++i) {
				dns_cent_array sec=secs[i];
				int j,n=DA_NEL(sec);
				/* The first entry in the answer section is treated separately, so skip that one. */
				for(j= !i; j<n; ++j) {
					dns_cent_t *cent=&DA_INDEX(sec,j);
					if(*(cent->qname) || st->auth_serv!=2) {
						unsigned int rem;
						if(st->trusted || !st->nsdomain || (domain_match(st->nsdomain, cent->qname, &rem, NULL),rem==0))
							add_cache(cent);
						else {
#if DEBUG>0
							unsigned char nmbuf[DNSNAMEBUFSIZE],nsbuf[DNSNAMEBUFSIZE];
							DEBUG_MSG("Record for %s not in nsdomain %s; dropped.\n",
								  rhn2str(cent->qname,nmbuf,sizeof(nmbuf)),rhn2str(st->nsdomain,nsbuf,sizeof(nsbuf)));
#endif
						}
					}
					else {
#if DEBUG>0
						static const char *const secname[3]={"answer","authority","additional"};
						DEBUG_PDNSDA_MSG("Record(s) for root domain in %s section from %s dropped.\n", secname[i],PDNSDA2STR(PDNSD_A(st)));
#endif
					}
				}
			}
		}
		goto free_centarrays_recvbuf_return;

	free_ns_ent_centarrays_recvbuf_return:
		dlist_free(*ns); *ns=NULL;
	free_ent_centarrays_recvbuf_return:
		if(DA_NEL(ans_sec)>=1) free_cent(&DA_INDEX(ans_sec,0)  DBG1);
	free_centarrays_recvbuf_return:
		{
			/* dns_cent_array secs[3]={ans_sec,auth_sec,add_sec}; */
			int i;
			for(i=0;i<3;++i) {
				dns_cent_array sec=secs[i];
				int j,n=DA_NEL(sec);
				/* The first entry in the answer section is treated separately, so skip that one. */
				for(j= !i; j<n; ++j)
					free_cent(&DA_INDEX(sec,j)  DBG1);

				da_free(sec);
			}
		}
#undef          ans_sec
#undef          auth_sec
#undef          add_sec
	}
 free_recvbuf_return:
	pdnsd_free(st->recvbuf);
	return rv;
}

/*
 * Cancel a query, freeing all resources. Any query state is valid as input (this may even be called
 * if a call to p_exec_query already returned error or success)
 */
static void p_cancel_query(query_stat_t *st)
{
	switch (st->state) {
	QS_WRITE_CASES:
	QS_READ_CASES:
		close(st->sock);
		/* fall through */
	case QS_TCPINITIAL:
	case QS_UDPINITIAL:
		pdnsd_free(st->recvbuf);
		pdnsd_free(st->msg);
	}
	if(st->state!=QS_INITIAL && st->state!=QS_DONE)
		st->state=QS_CANCELED;
}

#if 0
/*
 * Initialize a query_serv_t (server list for parallel query)
 * This is there for historical reasons only.
 */
inline static void init_qserv(query_stat_array *q)
{
	*q=NULL;
}
#endif

/*
 * Add a server entry to a query_serv_t
 * Note: only a reference to nsdomain is copied, not the name itself.
 * Be sure to free the q-list before freeing the name.
 */
static int add_qserv(query_stat_array *q, pdnsd_a2 *a, int port, time_t timeout, unsigned flags,
		     int nocache, char lean_query, char edns_query, char auth_s, char needs_testing, char trusted,
		     const unsigned char *nsdomain, rejectlist_t *rejectlist)
{
	query_stat_t *qs;

	if ((*q=DA_GROW1(*q))==NULL) {
		DEBUG_MSG("Out of memory in add_qserv()\n");
		return 0;
	}

	qs=&DA_LAST(*q);
#ifdef ENABLE_IPV4
	if (run_ipv4) {
		memset(&qs->a.sin4,0,sizeof(qs->a.sin4));
		qs->a.sin4.sin_family=AF_INET;
		qs->a.sin4.sin_port=htons(port);
		qs->a.sin4.sin_addr=a->ipv4;
		SET_SOCKA_LEN4(qs->a.sin4);
	}
#endif
#ifdef ENABLE_IPV6
	ELSE_IPV6 {
		memset(&qs->a.sin6,0,sizeof(qs->a.sin6));
		qs->a.sin6.sin6_family=AF_INET6;
		qs->a.sin6.sin6_port=htons(port);
		qs->a.sin6.sin6_flowinfo=IPV6_FLOWINFO;
		qs->a.sin6.sin6_addr=a->ipv6;
		SET_SOCKA_LEN6(qs->a.sin6);

		qs->a4fallback=a->ipv4;
	}
#endif
	qs->timeout=timeout;
	qs->flags=flags;
	qs->nocache=nocache;
	qs->auth_serv=auth_s;
	qs->lean_query=lean_query;
	qs->edns_query=edns_query;
	qs->needs_testing=needs_testing;
	qs->trusted=trusted;
	qs->aa=0;
	qs->tc=0;
	qs->failed=0;
	qs->nsdomain=nsdomain; /* Note: only a reference is copied, not the name itself! */
	qs->rejectlist=rejectlist;

	qs->state=QS_INITIAL;
	qs->qm=global.query_method;
	qs->s_errno=0;
	return 1;
}

/* This can be used to check whether a server address was already used in a
   previous query_stat_t entry. */
inline static int query_stat_same_inaddr2(query_stat_t *qs, pdnsd_a2 *b)
{
  return SEL_IPVER(  qs->a.sin4.sin_addr.s_addr==b->ipv4.s_addr,
		     IN6_ARE_ADDR_EQUAL(&qs->a.sin6.sin6_addr,&b->ipv6) &&
		      qs->a4fallback.s_addr==b->ipv4.s_addr );
}


/*
 * Free resources used by a query_serv_t
 * There for historical reasons only.
 */
inline static void del_qserv(query_stat_array q)
{
	da_free(q);
}

struct qstatnode_s {
	query_stat_array    qa;
	struct qstatnode_s  *next;
};
typedef struct qstatnode_s qstatnode_t;

struct qhintnode_s {
	const unsigned char *nm;
	int                 tp;
	struct qhintnode_s  *next;
};
/* typedef struct qhintnode_s qhintnode_t; */  /* Already defined in dns_query.h */

static int auth_ok(query_stat_array q, const unsigned char *name, int thint, dns_cent_t *ent,
		   int hops, qstatnode_t *qslist, qhintnode_t *qhlist,
		   query_stat_t *qse, dlist ns, query_stat_array *serv);
static int p_dns_cached_resolve(query_stat_array q, const unsigned char *name, int thint, dns_cent_t **cachedp,
				int hops, qstatnode_t *qslist, qhintnode_t *qhlist, time_t queryts,
				unsigned char *c_soa);
static int simple_dns_cached_resolve(atup_array atup_a, int port, char edns_query, time_t timeout,
				     const unsigned char *name, int thint, dns_cent_t **cachedp);


/*
 * Performs a semi-parallel query on the servers in q. PAR_QUERIES are executed parallel at a time.
 * name is the query name in dns protocol format (number.string etc),
 * ent is the dns_cent_t that will be filled.
 * hops is the number of recursions left.
 * qslist should refer to a list of server arrays used higher up in the calling chain. This way we can
 * avoid name servers that have already been tried for this name.
 * qhlist should refer to a list of names that we are trying to resolve higher up in the calling chain.
 * These names should be avoided further down the chain, or we risk getting caught in a wasteful cycle.
 * thint is a hint on the requested query type used to decide whether an aa record must be fetched
 * or a non-authoritative answer will be enough.
 *
 * nocache is needed because we add AA records to the cache. If the nocache flag is set, we do not
 * take the original values for the record, but flags=0 and ttl=0 (but only if we do not already have
 * a cached record for that set). These settings cause the record be purged on the next cache addition.
 * It will also not be used again.
 */
static int p_recursive_query(query_stat_array q, const unsigned char *name, int thint, dns_cent_t **entp,
			     int *nocache, int hops, qstatnode_t *qslist, qhintnode_t *qhlist,
			     unsigned char *c_soa)
{
	dns_cent_t *ent,*entsave=NULL;
	int i,j,k;
	int rv=RC_SERVFAIL;
	int qualval=0;
	query_stat_t *qse=NULL;  /* Initialized to inhibit compiler warning */
	dlist ns=NULL,nssave=NULL;
	query_stat_array serv=NULL,servsave=NULL;

#       define W_AUTHOK    8
#       define W_NOTFAILED 2
#       define W_NOTTRUNC  1
#       define NOTFAILMASK 6
#       define GOODQUAL    (W_AUTHOK+3*W_NOTFAILED)
#	define save_query_result(ent,qs,ns,serv,authok)						\
	{											\
		int qval = authok*W_AUTHOK + (3-qs->failed)*W_NOTFAILED + (!qs->tc)*W_NOTTRUNC; \
		if(entsave && qval>qualval) {							\
			/* Free the old copy, because the new result is better. */		\
			free_cent(entsave DBG1);						\
			pdnsd_free(entsave);							\
			entsave=NULL;								\
			del_qserv(servsave);							\
			dlist_free(nssave);							\
		}										\
		if(!entsave) {									\
			entsave=ent;								\
			servsave=serv;								\
			/* The serv array contains references to data within the ns list,	\
			   so we need to save a copy of the ns list as well! */			\
			if(DA_NEL(serv)>0) nssave=ns; else {nssave=NULL;dlist_free(ns);}	\
			qualval=qval;								\
			qse=qs;									\
		}										\
		else {										\
			/* We already have a copy, free the present one. */			\
			free_cent(ent DBG1);							\
			pdnsd_free(ent);							\
			del_qserv(serv);							\
			dlist_free(ns);								\
		}										\
		serv=NULL;									\
		ns=NULL;									\
	}

	{
		time_t ts0=time(NULL),global_timeout=global.timeout;
		int dc=0,mc=0,nq=DA_NEL(q),parqueries=global.par_queries;

		for (j=0; j<nq; j += parqueries) {
			mc=j+parqueries;
			if (mc>nq) mc=nq;

			/* First, call p_exec_query once for each parallel set to initialize.
			 * Then, as long as not all have the state QS_DONE or we have a timeout,
			 * build a poll/select set for all active queries and call them accordingly. */
			for (i=dc;i<mc;i++) {
				query_stat_t *qs=&DA_INDEX(q,i);
				if(i>=j) {
					/* The below should not happen any more, but may once again
					 * (immediate success) */
					DEBUG_PDNSDA_MSG("Sending query to %s\n", PDNSDA2STR(PDNSD_A(qs)));
				retryquery:
					rv=p_exec_query(&ent, name, thint, qs,&ns,c_soa);
					if (rv==RC_OK) {
						int authok;
						DEBUG_PDNSDA_MSG("Query to %s succeeded.\n", PDNSDA2STR(PDNSD_A(qs)));
						if((authok=auth_ok(q, name, thint, ent, hops, qslist, qhlist, qs, ns, &serv))) {
							if(authok>=0) {
								if(!qs->failed
#if !defined(NO_TCP_QUERIES) && !defined(NO_UDP_QUERIES)
								   && !(qs->qm==UDP_TCP && qs->tc)
#endif
								  )
								{
									qse=qs;
									mc=i; /* No need to cancel queries beyond i */
									goto done;
								}
							}
							else {
								mc=i; /* No need to cancel queries beyond i */
								goto free_ent_return_failed;
							}
						}
						/* We do not have a satisfactory answer.
						   However, we will save a copy in case none of the other
						   servers in the q list give a satisfactory answer either.
						 */
						save_query_result(ent,qs,ns,serv,authok);
#if !defined(NO_TCP_QUERIES) && !defined(NO_UDP_QUERIES)
						if(qs->qm==UDP_TCP && qs->tc) {
							switch_to_tcp(qs);
							DEBUG_PDNSDA_MSG("Reply from %s was truncated. Trying again using TCP.\n",
									 PDNSDA2STR(PDNSD_A(qs)));
							goto retryquery;
						}
#endif
					}
					else if (rv==RC_NAMEERR || rv==RC_FATALERR) {
						mc=i; /* No need to cancel queries beyond i */
						goto done;
					}
				}
				if (qs->state==QS_DONE && i==dc)
					dc++;
			}
			if (dc<mc) {
				time_t ts,maxto,now;
				int pc,nevents;
#ifdef NO_POLL
				int maxfd;
				fd_set reads;
				fd_set writes;
				struct timeval tv;
#else
				int ic;
				struct pollfd polls[mc-dc];  /* Variable length array, may cause portability problems */
#endif
				/* we do time keeping by hand, because poll/select might be interrupted and
				 * the returned times are not always to be trusted upon */
				ts=time(NULL);
				do {
					/* build poll/select sets, maintain time.
					 * If you do parallel queries, the highest timeout will be honored
					 * also for the other servers when their timeout is exceeded and
					 * the highest is not.
					 * Changed by Paul Rombouts: queries are not canceled until we receive
					 * a useful reply or everything has failed or timed out (also taking into
					 * account the global timeout option).
					 * Thus in the worst case all the queries in the q list will be active
					 * simultaneously. The downside is that we may be wasting more resources
					 * this way. The advantage is that we have a greater chance of catching a
					 * reply. After all, if we wait longer anyway, why not for more servers. */
					maxto=0;
					pc=0;
					rv=RC_SERVFAIL;

#ifdef NO_POLL
					FD_ZERO(&reads);
					FD_ZERO(&writes);
					maxfd=0;
#endif
					for (i=dc;i<mc;i++) {
						query_stat_t *qs=&DA_INDEX(q,i);
						if (qs->state!=QS_DONE) {
							if (i>=j && qs->timeout>maxto)
								maxto=qs->timeout;
#ifdef NO_POLL
							if (qs->sock>maxfd) {
								maxfd=qs->sock;
								PDNSD_ASSERT(maxfd<FD_SETSIZE,"socket file descriptor exceeds FD_SETSIZE.");
							}

							switch (qs->state) {
							QS_READ_CASES:
								FD_SET(qs->sock,&reads);
								break;
							QS_WRITE_CASES:
								FD_SET(qs->sock,&writes);
								break;
							}
#else
							polls[pc].fd=qs->sock;
							switch (qs->state) {
							QS_READ_CASES:
								polls[pc].events=POLLIN;
								break;
							QS_WRITE_CASES:
								polls[pc].events=POLLOUT;
								break;
							default:
								polls[pc].events=0;
							}
#endif
							pc++;
						}
					}
					if (pc==0) {
						/* In this case, ALL are done and we do not need to cancel any
						 * query. */
						dc=mc;
						break;
					}
					now=time(NULL);
					maxto -= now-ts;
					if (mc==nq) {
#if !defined(NO_TCP_QUERIES) && !defined(NO_UDP_QUERIES)
						/* Don't use the global timeout if there are TCP queries
						   we might want to retry using UDP. */
						for (i=j;i<mc;i++) {
							query_stat_t *qs=&DA_INDEX(q,i);
							if(tentative_tcp_query(qs))
								goto skip_globto;
						}
#endif
						{
							time_t globto=global_timeout-(now-ts0);
							if(globto>maxto) maxto=globto;
						}
#if !defined(NO_TCP_QUERIES) && !defined(NO_UDP_QUERIES)
					skip_globto:;
#endif
					}
#ifdef NO_POLL
					tv.tv_sec=(maxto>0)?maxto:0;
					tv.tv_usec=0;
					nevents=select(maxfd+1,&reads,&writes,NULL,&tv);
#else
					nevents=poll(polls,pc,(maxto>0)?(maxto*1000):0);
#endif
					if (nevents<0) {
						/* if(errno==EINTR)
							continue; */
						log_warn("poll/select failed: %s",strerror(errno));
						goto done;
					}
					if (nevents==0) {
						/* We have timed out. Mark the unresponsive servers so that we can consider
						   them for retesting later on. We will continue to listen for replies from
						   these servers as long as we have additional servers to try. */
						for (i=j;i<mc;i++) {
							query_stat_t *qs=&DA_INDEX(q,i);
							if (qs->state!=QS_DONE && qs->needs_testing)
								qs->needs_testing=2;
#if !defined(NO_TCP_QUERIES) && !defined(NO_UDP_QUERIES)
							if (tentative_tcp_query(qs)) {
								/* We timed out while waiting for a TCP connection.
								   Try again using UDP.
								*/
								close(qs->sock);
								switch_to_udp(qs);
								DEBUG_PDNSDA_MSG("TCP connection to %s timed out. Trying to use UDP.\n",
										 PDNSDA2STR(PDNSD_A(qs)));

								rv=p_exec_query(&ent, name, thint, qs,&ns,c_soa);
								/* In the unlikely case of immediate success */
								if (rv==RC_OK) {
									int authok;
									DEBUG_PDNSDA_MSG("Query to %s succeeded.\n", PDNSDA2STR(PDNSD_A(qs)));
									if((authok=auth_ok(q, name, thint, ent, hops, qslist, qhlist, qs, ns, &serv))) {
										if(authok>=0) {
											if(!qs->failed) {
												qse=qs;
												goto done;
											}
										}
										else
											goto free_ent_return_failed;
									}
									save_query_result(ent,qs,ns,serv,authok);
								}
								else if (rv==RC_NAMEERR || rv==RC_FATALERR) {
									goto done;
								}
								++nevents;
							}
#endif
						}
#if !defined(NO_TCP_QUERIES) && !defined(NO_UDP_QUERIES)
						if (mc==nq) {
							/* We will not try additional servers, but we might want to try again
							   using UDP instead of TCP
							*/
							if(nevents && (time(NULL)-ts0)<global_timeout)
								continue;
						}
#endif
						break;
					}
#ifndef NO_POLL
					ic=0;
#endif
					for (i=dc;i<mc;i++) {
						query_stat_t *qs=&DA_INDEX(q,i);
						/* Check if we got a poll/select event */
						if (qs->state!=QS_DONE) {
							int srv_event=0;
							/* This detection may seem suboptimal, but normally, we have at most 2-3 parallel
							 * queries, and anything else would be higher overhead, */
#ifdef NO_POLL
							switch (qs->state) {
							QS_READ_CASES:
								srv_event=FD_ISSET(qs->sock,&reads);
								break;
							QS_WRITE_CASES:
								srv_event=FD_ISSET(qs->sock,&writes);
								break;
							}
#else
							do {
								PDNSD_ASSERT(ic<pc, "file descriptor not found in poll() array");
								k=ic++;
							} while(polls[k].fd!=qs->sock);
							/*
							 * In case of an error, reenter the state machine
							 * to catch it.
							 */
							switch (qs->state) {
							QS_READ_CASES:
								srv_event=polls[k].revents&(POLLIN|POLLERR|POLLHUP|POLLNVAL);
								break;
							QS_WRITE_CASES:
								srv_event=polls[k].revents&(POLLOUT|POLLERR|POLLHUP|POLLNVAL);
								break;
							}
#endif
							if (srv_event) {
								--nevents;
							retryquery2:
								rv=p_exec_query(&ent, name, thint, qs,&ns,c_soa);
								if (rv==RC_OK) {
									int authok;
									DEBUG_PDNSDA_MSG("Query to %s succeeded.\n", PDNSDA2STR(PDNSD_A(qs)));
									if((authok=auth_ok(q, name, thint, ent, hops, qslist, qhlist, qs, ns, &serv))) {
										if(authok>=0) {
											if(!qs->failed
#if !defined(NO_TCP_QUERIES) && !defined(NO_UDP_QUERIES)
											   && !(qs->qm==UDP_TCP && qs->tc)
#endif
											  )
											{
												qse=qs;
												goto done;
											}
										}
										else
											goto free_ent_return_failed;
									}
									save_query_result(ent,qs,ns,serv,authok);
#if !defined(NO_TCP_QUERIES) && !defined(NO_UDP_QUERIES)
									if(qs->qm==UDP_TCP && qs->tc) {
										switch_to_tcp(qs);
										DEBUG_PDNSDA_MSG("Reply from %s was truncated. Trying again using TCP.\n",
												 PDNSDA2STR(PDNSD_A(qs)));
										goto retryquery2;
									}
#endif
								}
								else if (rv==RC_NAMEERR || rv==RC_FATALERR) {
									goto done;
								}
							}
						}
						/* recheck, this might have changed after the last p_exec_query */
						if (qs->state==QS_DONE && i==dc)
							dc++;
					}
					if(nevents>0) {
						/* We have not managed to handle all the events reported by poll/select.
						   Better call it quits, or we risk getting caught in a wasteful cycle.
						*/
						if(++poll_errs<=MAXPOLLERRS)
							log_error("%d unhandled poll/select event(s) in p_recursive_query() at %s, line %d.",nevents,__FILE__,__LINE__);
						rv=RC_SERVFAIL;
						goto done;
					}
				} while (dc<mc);
			}
		}
		goto cancel_queries;
	free_ent_return_failed:
		free_cent(ent  DBG1);
		pdnsd_free(ent);
		rv=RC_FATALERR;
	done:
		if (entsave) {
			/* We have or will get an authoritative answer, or we have encountered an error.
			   Free the non-authoritative answer. */
			free_cent(entsave DBG1);
			pdnsd_free(entsave);
			entsave=NULL;
			del_qserv(servsave);
			dlist_free(nssave);
		}
	cancel_queries:
		/* Cancel any remaining queries. */
		for (i=dc;i<mc;i++)
			p_cancel_query(&DA_INDEX(q,i));

		{
			/* See if any servers need to be retested for availability.
			   We build up a list of addresses rather than call
			   sched_server_test() separately for each address to
			   reduce the overhead caused by locking and signaling */
			int n=0;
			for (i=0;i<mc;i++)
				if (DA_INDEX(q,i).needs_testing > 1)
					++n;
			if(n>0) {
				pdnsd_a addrs[n]; /* variable length array */
				k=0;
				for (i=0;i<mc;i++) {
					query_stat_t *qs=&DA_INDEX(q,i);
					if (qs->needs_testing > 1)
						addrs[k++]= *PDNSD_A(qs);
				}
				sched_server_test(addrs,n,-1);
			}
		}
	}

	if(entsave) {
		/*
		 * If we didn't get rrs from any of the authoritative servers, or the answers were
		 * unsatisfactory for another reason, take the one we had.
		 * However, raise the CF_NOCACHE flag, so that it won't be used again (outside the
		 * cache latency period).
		 */
		DEBUG_PDNSDA_MSG("Using %s reply from %s.\n",
				 !(qualval&NOTFAILMASK)? "reportedly failed":
				 !(qualval&W_NOTFAILED)? "inconsistent":
				 !(qualval&W_NOTTRUNC)? "truncated":
				 !(qualval&W_AUTHOK)? "non-authoritative": "good",
				 PDNSDA2STR(PDNSD_A(qse)));
		ent=entsave;
		serv=servsave;
		ns=nssave;
		if(qualval<GOODQUAL) {
			if(!(ent->flags&DF_NEGATIVE)) {
				int jlim= RRARR_LEN(ent);
				for (j=0; j<jlim; ++j) {
					rr_set_t *rrs= RRARR_INDEX(ent,j);
					if (rrs)
						rrs->flags |= CF_NOCACHE;
				}
			}
			else    /* Very unlikely, but not impossible. */
				ent->flags |= DF_NOCACHE;
		}
		rv=RC_OK;
	}
	else if (rv!=RC_OK) {
		if(rv==RC_FATALERR) {
			DEBUG_MSG("Unrecoverable error encountered while processing query.\n");
			rv=RC_SERVFAIL;
		}
		DEBUG_MSG("No query succeeded. Returning error code \"%s\"\n",get_ename(rv));
		goto clean_up_return;
	}

	if(nocache) *nocache=qse->nocache;

	if (DA_NEL(serv)>0) {
		/* Authority records present. Ask them, because the answer was non-authoritative. */
		qstatnode_t qsn={q,qslist};
		unsigned char save_ns=ent->c_ns,save_soa=ent->c_soa;
		free_cent(ent  DBG1);
		pdnsd_free(ent);
		rv=p_dns_cached_resolve(serv, name, thint,&ent,hops-1,&qsn,qhlist,time(NULL),c_soa);
		if(rv==RC_OK) {
			if(save_ns!=cundef && (ent->c_ns==cundef || ent->c_ns<save_ns))
				ent->c_ns=save_ns;
			if(save_soa!=cundef && (ent->c_soa==cundef || ent->c_soa<save_soa))
				ent->c_soa=save_soa;
		}
		else if(rv==RC_NAMEERR && c_soa) {
			if(save_soa!=cundef && (*c_soa==cundef || *c_soa<save_soa))
				*c_soa=save_soa;
		}
		/* return the answer in any case. */
	}

 clean_up_return:
	/* Always free the serv array before freeing the ns list,
	   because the serv array contains references to data within the ns list! */
	del_qserv(serv);
	dlist_free(ns);

	if(rv==RC_OK) *entp=ent;
	return rv;
#	undef save_query_result
}

/* auth_ok returns 1 if we don't need an authoritative answer or
   if we can find servers to ask for an authoritative answer.
   In the latter case these servers will be added to the *serv list.
   A return value of 0 means the answer is not satisfactory in the
   previous sense.
   A return value of -1 indicates an error.
*/
static int auth_ok(query_stat_array q, const unsigned char *name, int thint, dns_cent_t *ent,
		   int hops, qstatnode_t *qslist, qhintnode_t *qhlist,
		   query_stat_t *qse, dlist ns, query_stat_array *serv)
{
	int retval=0;

	/* If the answer was obtained from a name server which returned a failure code,
	   the answer is never satisfactory. */
	if(qse->failed > 1) return 0;

	/*
	  Look into the query type hint. If it is a wildcard (QT_*), we need an authoritative answer.
	  Same if there is no record that answers the query.
	  This test will also succeed if we have a negative cached record. This is done purposely.
	*/
#define aa_needed ((thint>=QT_MIN && thint<=QT_MAX) || \
	           ((thint>=T_MIN && thint<=T_MAX) && \
		    (!have_rr(ent,thint) && !have_rr_CNAME(ent))))

	/* We will want to query authoritative servers if all of the following conditions apply:

	   1) The server from which we got the answer was not configured as "proxy only".
	   2) The answer is not a negatively cached domain (i.e. the server did not reply with NXDOMAIN).
	   3) The query type is a wild card (QT_*), or no record answers the query.
	   4) The answer that we have is non-authoritative.
	*/
	if(!(qse->auth_serv && !(ent->flags&DF_NEGATIVE) && aa_needed))
		return 1;

	if(qse->aa) {
		/* The reply we have claims to be authoritative.
		   However, I have seen cases where name servers raise the authority flag incorrectly (groan...),
		   so as a work-around, we will check whether the domains for which the servers in the ns
		   list are responsible, match the queried name better than the domain for which the
		   last server was responsible. */
		unsigned char *nsdomain;

		if(!qse->nsdomain)
			return 1;

		nsdomain=dlist_first(ns);
		if(!nsdomain)
			return 1;
		for(;;) {
			unsigned int rem,crem;
			domain_match(nsdomain,qse->nsdomain,&rem,&crem);
			if(!(rem>0 && crem==0))
				return 1;
			domain_match(nsdomain,name,&rem,NULL);
			if(rem!=0)
				return 1;
			do {
				nsdomain=dlist_next(nsdomain);
				if(!nsdomain)
					goto done_checkauth;
			} while(*nsdomain==0xff);  /* Skip repeats. */
		}
	done_checkauth:;

		/* The name servers in the ns list are a better match for the queried name than
		   the server from which we got the last reply, so ignore the aa flag.
		*/
#if DEBUG>0
		if(debug_p) {
			unsigned char dbuf[DNSNAMEBUFSIZE],sdbuf[DNSNAMEBUFSIZE];
			nsdomain=dlist_first(ns);
			DEBUG_PDNSDA_MSG("The name server %s which is responsible for the %s domain, raised the aa flag, but appears to delegate to the sub-domain %s\n",
					 PDNSDA2STR(PDNSD_A(qse)),
					 rhn2str(qse->nsdomain,dbuf,sizeof(dbuf)),
					 rhn2str(nsdomain,sdbuf,sizeof(sdbuf)));
		}
#endif
	}

	/* The answer was non-authoritative. Try to build a list of addresses of authoritative servers. */
	if (hops>0) {
		unsigned char *nsdomp, *nsdomain=NULL;
		rr_set_t *localrrset=NULL;
		rr_bucket_t *localrr=NULL;
		for (nsdomp=dlist_first(ns);;) {
			unsigned char *nsname=NULL;  /* Initialize to inhibit compiler warning. */
			pdnsd_a2 serva;

			/* Get next name server. */
			if(localrr) {
				/* Use next locally defined NS record. */
				nsname=(unsigned char *)(localrr->data);
				localrr= localrr->next;
			}
			else {
				if(localrrset) {
					/* clean up rrset */
					del_rrset(localrrset  DBG1);
					localrrset=NULL;
				}
				if(!nsdomp)
					break;
				else if(*nsdomp!=0xff) {
					/* New domain. */
					nsdomain=nsdomp;
					if (global.paranoid) {
						unsigned int rem;
						/* paranoia mode: don't query name servers that are not responsible */
						domain_match(nsdomain,name,&rem,NULL);
						if (rem!=0) {
#if DEBUG>0
							unsigned char nmbuf[DNSNAMEBUFSIZE],dbuf[DNSNAMEBUFSIZE],nsbuf[DNSNAMEBUFSIZE];
							DEBUG_MSG("The name server %s is responsible for the %s domain, which does not match %s\n",
								  rhn2str(nsname,nsbuf,sizeof(nsbuf)),
								  rhn2str(nsdomain,dbuf,sizeof(dbuf)),
								  rhn2str(name,nmbuf,sizeof(nmbuf)));
#endif
							/* Skip records in ns list for the same domain. */
							do {
								nsdomp=dlist_next(nsdomp);
							} while (nsdomp && *nsdomp==0xff);
							continue;
						}
					}
					/*   Check if we have locally defined NS records, because
					     they will override the ones provided by remote servers.
					*/
					localrrset=lookup_cache_local_rrset(nsdomain,T_NS);
					if(localrrset) {
						/* Skip records in ns list for the same domain. */
						do {
							nsdomp=dlist_next(nsdomp);
						} while (nsdomp && *nsdomp==0xff);
						localrr=localrrset->rrs;
						if(!localrr) continue;
						nsname=(unsigned char *)(localrr->data);
						localrr= localrr->next;
					}
					else {
						nsname=skiprhn(nsdomp);
						nsdomp=dlist_next(nsdomp);
					}
				}
				else {
					/* domain repeated. */
					nsname= nsdomp+1;
					nsdomp=dlist_next(nsdomp);
				}
			}
			/* look it up in the cache or resolve it if needed.
			   The records received should be in the cache now, so it's ok.
			*/
#ifdef ENABLE_IPV6
			if(!run_ipv4)
				serva.ipv6=in6addr_any;
#endif
			serva.ipv4.s_addr=INADDR_ANY;
			{
				const unsigned char *nm=name;
				int tp=thint;
				qhintnode_t *ql=qhlist;

				for(;;) {
					if(rhnicmp(nm,nsname) && tp==T_A) {
						DEBUG_RHN_MSG("Not looking up address for name server \"%s\": "
							      "risk of infinite recursion.\n",RHN2STR(nsname));
						goto skip_server;
					}
					if(!ql) break;
					nm=ql->nm;
					tp=ql->tp;
					ql=ql->next;
				}
				{
					qhintnode_t qhn={name,thint,qhlist};
					dns_cent_t *servent;
					if (r_dns_cached_resolve(nsname,T_A, &servent, hops-1, &qhn,time(NULL),NULL)==RC_OK) {
#ifdef ENABLE_IPV4
						if (run_ipv4) {
							rr_set_t *rrset=getrrset_A(servent);
							if (rrset && rrset->rrs)
								serva.ipv4 = *((struct in_addr *)rrset->rrs->data);
						}
#endif
#ifdef ENABLE_IPV6
						ELSE_IPV6 {
							rr_set_t *rrset;
							if ((rrset=getrrset_AAAA(servent)) && rrset->rrs) {
								serva.ipv6 = *((struct in6_addr *)rrset->rrs->data);
								if ((rrset=getrrset_A(servent)) && rrset->rrs) {
									/* Store IPv4 address as fallback. */
									serva.ipv4 = *((struct in_addr *)rrset->rrs->data);
								}
							}
							else if ((rrset=getrrset_A(servent)) && rrset->rrs) {
								struct in_addr *ina = (struct in_addr *)rrset->rrs->data;
								IPV6_MAPIPV4(ina,&serva.ipv6);
							}
						}
#endif
						free_cent(servent  DBG1);
						pdnsd_free(servent);
					}
				}
			}

			if(is_inaddr2_any(&serva))
				continue;  /* address resolution failed. */

			if(is_local_addr(PDNSD_A2_TO_A(&serva)))
				continue;  /* Do not use local address (as defined in netdev.c). */

			{       /* Skip duplicate addresses. */
				int i,n=DA_NEL(*serv);
				for (i=0; i<n; ++i) {
					query_stat_t *qs=&DA_INDEX(*serv,i);
					if (query_stat_same_inaddr2(qs,&serva))
						goto skip_server;
				}
			}

			{       /* We've got an address. Add it to the list if it wasn't one of the servers we queried. */
				query_stat_array qa=q;
				qstatnode_t *ql=qslist;
				for(;;) {
					int i,n=DA_NEL(qa);
					for (i=0; i<n; ++i) {
						/* If qa[i].state == QS_DONE, then p_exec_query() has been called,
						   and we should not query this server again */
						query_stat_t *qs=&DA_INDEX(qa,i);
						if (qs->state==QS_DONE && equiv_inaddr2(PDNSD_A(qs),&serva)) {
							DEBUG_PDNSDA_MSG("Not trying name server %s, already queried.\n", PDNSDA2STR(PDNSD_A2_TO_A(&serva)));
							goto skip_server;
						}
					}
					if(!ql) break;
					qa=ql->qa;
					ql=ql->next;
				}
			}

			/* lean query mode is inherited. CF_AUTH and CF_ADDITIONAL are not (as specified
			 * in CFF_NOINHERIT). */
			if (!add_qserv(serv, &serva, 53, qse->timeout, qse->flags&~CFF_NOINHERIT, 0,
				       qse->lean_query,qse->edns_query,2,0,!global.paranoid,nsdomain,
				       inherit_rejectlist(qse)?qse->rejectlist:NULL))
			{
				return -1;
			}
			retval=1;
		skip_server:;
		}
#if DEBUG>0
		if(!retval) {
			DEBUG_PDNSDA_MSG("No remaining authoritative name servers to try in authority section from %s.\n", PDNSDA2STR(PDNSD_A(qse)));
		}
#endif
	}
	else {
		DEBUG_MSG("Maximum hops count reached; not trying any more name servers.\n");
	}

	return retval;

#undef  aa_needed
}

/*
 * This checks the given name to resolve against the access list given for the server using the
 * include=, exclude= and policy= parameters.
 */
static int use_server(servparm_t *s, const unsigned char *name)
{
	int i,n=DA_NEL(s->alist);

	for (i=0;i<n;i++) {
		slist_t *sl=&DA_INDEX(s->alist,i);
		unsigned int nrem,lrem;
		domain_match(name,sl->domain,&nrem,&lrem);
		if(!lrem && (!sl->exact || !nrem))
			return sl->rule==C_INCLUDED;
	}

	if (s->policy==C_SIMPLE_ONLY || s->policy==C_FQDN_ONLY) {
                if(rhnsegcnt(name)<=1)
			return s->policy==C_SIMPLE_ONLY;
                else
			return s->policy==C_FQDN_ONLY;
        }

	return s->policy==C_INCLUDED;
}

#if ALLOW_LOCAL_AAAA
#define serv_has_rejectlist(s) ((s)->reject_a4!=NULL || (s)->reject_a6!=NULL)
#else
#define serv_has_rejectlist(s) ((s)->reject_a4!=NULL)
#endif

/* Take the lists of IP addresses from a server section sp and
   convert them into a form that can be used by p_exec_query().
   If successful, add_rejectlist returns a new list which is added to the old list rl,
   otherwise the return value is NULL.
*/
static rejectlist_t *add_rejectlist(rejectlist_t *rl, servparm_t *sp)
{
	int i,na4=DA_NEL(sp->reject_a4);
	addr4maskpair_t *a4p;
#if ALLOW_LOCAL_AAAA
	int na6=DA_NEL(sp->reject_a6);
	addr6maskpair_t *a6p;
#endif
	rejectlist_t *rlist = malloc(sizeof(rejectlist_t) + na4*sizeof(addr4maskpair_t)
#if ALLOW_LOCAL_AAAA
				     + na6*sizeof(addr6maskpair_t)
#endif
				     );

	if(rlist) {
#if ALLOW_LOCAL_AAAA
		/* Store the larger IPv6 addresses first to avoid possible alignment problems. */
		rlist->na6 = na6;
		a6p = (addr6maskpair_t *)rlist->rdata;
		for(i=0;i<na6;++i)
			*a6p++ = DA_INDEX(sp->reject_a6,i);
#endif
		rlist->na4 = na4;
#if ALLOW_LOCAL_AAAA
		a4p = (addr4maskpair_t *)a6p;
#else
		a4p = (addr4maskpair_t *)rlist->rdata;
#endif
		for(i=0;i<na4;++i)
			*a4p++ = DA_INDEX(sp->reject_a4,i);

		rlist->policy = sp->rejectpolicy;
		rlist->inherit = sp->rejectrecursively;
		rlist->next = rl;
	}

	return rlist;
}

inline static void free_rejectlist(rejectlist_t *rl)
{
	while(rl) {
		rejectlist_t *next = rl->next;
		free(rl);
		rl=next;
	}
}

/* Lookup addresses of nameservers provided by root servers for a given domain in the cache.
   Returns NULL if unsuccessful (or the cache entries have timed out).
*/
static addr2_array lookup_ns(const unsigned char *domain)
{
	addr2_array res=NULL;

	dns_cent_t *cent=lookup_cache(domain,NULL);
	if(cent) {
		rr_set_t *rrset=getrrset_NS(cent);
		if(rrset && (rrset->flags&CF_ROOTSERV) && !timedout(rrset)) {
			rr_bucket_t *rr;
			for(rr=rrset->rrs; rr; rr=rr->next) {
				pdnsd_a2 *serva;
				dns_cent_t *servent;
				if(!(res=DA_GROW1(res))) {
					DEBUG_MSG("Out of memory in lookup_ns()\n");
					break;
				}
				serva=&DA_LAST(res);
#ifdef ENABLE_IPV6
				if(!run_ipv4)
					serva->ipv6=in6addr_any;
#endif
				serva->ipv4.s_addr=INADDR_ANY;

				servent=lookup_cache((unsigned char*)(rr->data),NULL);
				if(servent) {
#ifdef ENABLE_IPV4
					if (run_ipv4) {
						rr_set_t *rrset=getrrset_A(servent);
						if (rrset && !timedout(rrset) && rrset->rrs)
							serva->ipv4 = *((struct in_addr *)rrset->rrs->data);
					}
#endif
#ifdef ENABLE_IPV6
					ELSE_IPV6 {
						rr_set_t *rrset;
						if ((rrset=getrrset_AAAA(servent)) && !(rrset->flags&CF_NEGATIVE)) {
							if(!timedout(rrset) && rrset->rrs) {
								serva->ipv6 = *((struct in6_addr *)rrset->rrs->data);
								if ((rrset=getrrset_A(servent)) && !(rrset->flags&CF_NEGATIVE)) {
									if(!timedout(rrset) && rrset->rrs)
										serva->ipv4 = *((struct in_addr *)rrset->rrs->data);
									else /* Treat this as a failure. */
										serva->ipv6=in6addr_any;
								}
							}
						}
						else if ((rrset=getrrset_A(servent)) && !timedout(rrset) && rrset->rrs) {
							struct in_addr *ina = (struct in_addr *)rrset->rrs->data;
							IPV6_MAPIPV4(ina,&serva->ipv6);
						}
					}
#endif
					free_cent(servent  DBG1);
					pdnsd_free(servent);
				}
				if(is_inaddr2_any(serva)) {
					/* Address lookup failed. */
					da_free(res); res=NULL;
					break;
				}
			}
		}
		free_cent(cent  DBG1);
		pdnsd_free(cent);
	}

	return res;
}


/* Find addresses of root servers by looking them up in the cache or querying (non-recursively)
   the name servers in the list provided.
   Returns NULL if unsuccessful (or the cache entries have timed out).
*/
addr2_array dns_rootserver_resolv(atup_array atup_a, int port, char edns_query, time_t timeout)
{
	addr2_array res=NULL;
	dns_cent_t *cent;
	static const unsigned char rdomain[1]={0};  /* root-domain name. */
	int rc;

	rc=simple_dns_cached_resolve(atup_a,port,edns_query,timeout,rdomain,T_NS,&cent);
	if(rc==RC_OK) {
		rr_set_t *rrset=getrrset_NS(cent);
		if(rrset) {
			rr_bucket_t *rr;
			unsigned nfail=0;
			for(rr=rrset->rrs; rr; rr=rr->next) {
				pdnsd_a2 serva;
				dns_cent_t *servent;
#ifdef ENABLE_IPV6
				if(!run_ipv4)
					serva.ipv6=in6addr_any;
#endif
				serva.ipv4.s_addr=INADDR_ANY;

				rc=simple_dns_cached_resolve(atup_a,port,edns_query,timeout,
							     (const unsigned char *)(rr->data),T_A,&servent);
				if(rc==RC_OK) {
#ifdef ENABLE_IPV4
					if (run_ipv4) {
						rr_set_t *rrset=getrrset_A(servent);
						if (rrset && rrset->rrs)
							serva.ipv4 = *((struct in_addr *)rrset->rrs->data);
					}
#endif
#ifdef ENABLE_IPV6
					ELSE_IPV6 {
						rr_set_t *rrset;
						if ((rrset=getrrset_AAAA(servent)) && rrset->rrs) {
							serva.ipv6 = *((struct in6_addr *)rrset->rrs->data);
							if ((rrset=getrrset_A(servent)) && rrset->rrs) {
								/* Store IPv4 address as fallback. */
								serva.ipv4 = *((struct in_addr *)rrset->rrs->data);
							}
						}
						else if ((rrset=getrrset_A(servent)) && rrset->rrs) {
							struct in_addr *ina = (struct in_addr *)rrset->rrs->data;
							IPV6_MAPIPV4(ina,&serva.ipv6);
						}
					}
#endif
					free_cent(servent  DBG1);
					pdnsd_free(servent);
				}
				else {
					DEBUG_RHN_MSG("Simple query for %s type A failed (rc: %s)\n",
						      RHN2STR((const unsigned char *)(rr->data)),get_ename(rc));
				}

				if(is_inaddr2_any(&serva)) {
					/* Address lookup failed. */
					DEBUG_RHN_MSG("Failed to obtain address of root server %s in dns_rootserver_resolv()\n",
						      RHN2STR((const unsigned char *)(rr->data)));
					++nfail;
					continue;
				}
				if(!(res=DA_GROW1(res))) {
					DEBUG_MSG("Out of memory in dns_rootserver_resolv()\n");
					goto free_cent_return;
				}
				DA_LAST(res)= serva;
			}
			/* At least half of the names should resolve, otherwise we reject the result. */
			if(nfail>DA_NEL(res)) {
				DEBUG_MSG("Too many root-server resolve failures (%u succeeded, %u failed),"
					  " rejecting the result.\n", DA_NEL(res),nfail);
				da_free(res); res=NULL;
			}
		}
	free_cent_return:
		free_cent(cent  DBG1);
		pdnsd_free(cent);
	}
	else {
		DEBUG_MSG("Simple query for root domain type NS failed (rc: %s)\n",get_ename(rc));
	}

	return res;
}


static int p_dns_resolve(const unsigned char *name, int thint, dns_cent_t **cachedp, int hops, qhintnode_t *qhlist,
			 unsigned char *c_soa)
{
	int i,n,rc;
	int one_up=0,seenrootserv=0;
	query_stat_array serv=NULL;
	rejectlist_t *rejectlist=NULL;

	/* try the servers in the order of their definition */
	lock_server_data();
	n=DA_NEL(servers);
	for (i=0;i<n;++i) {
		servparm_t *sp=&DA_INDEX(servers,i);
		if(sp->rootserver<=1 && use_server(sp,name)) {
			int m=DA_NEL(sp->atup_a);
			if(m>0) {
				rejectlist_t *rjl=NULL;
				int j=0, jstart=0;
				if(sp->rand_servers) j=jstart=random()%m;
				do {
					atup_t *at=&DA_INDEX(sp->atup_a,j);
					if (at->is_up) {
						if(sp->rootserver) {
							if(!seenrootserv) {
								int nseg,mseg=1,l=0;
								const unsigned char *topdomain=NULL;
								addr2_array adrs=NULL;
								seenrootserv=1;
								nseg=rhnsegcnt(name);
								if(nseg>=2) {
									static const unsigned char rhn_arpa[6]= {4,'a','r','p','a',0};
									unsigned int rem;
									/* Check if the queried name ends in "arpa" */
									domain_match(rhn_arpa, name, &rem,NULL);
									if(rem==0) mseg=3;
								}
								if(nseg<=mseg) {
									if(nseg>0) mseg=nseg-1; else mseg=0;
								}
								for(;mseg>=1; --mseg) {
									topdomain=skipsegs(name,nseg-mseg);
									adrs=lookup_ns(topdomain);
									l=DA_NEL(adrs);
									if(l>0) break;
									if(adrs) da_free(adrs);
								}
								if(l>0) {
									/* The name servers for this top level domain have been found in the cache.
									   Instead of asking the root server, we will use this cached information.
									*/
									int k=0, kstart=0;
									if(sp->rand_servers) k=kstart=random()%l;
									if(serv_has_rejectlist(sp) && sp->rejectrecursively && !rjl) {
										rjl=add_rejectlist(rejectlist,sp);
										if(!rjl) {one_up=0; da_free(adrs); goto done;}
										rejectlist=rjl;
									}
									do {
										one_up=add_qserv(&serv, &DA_INDEX(adrs,k), 53, sp->timeout,
												 mk_flag_val(sp)&~CFF_NOINHERIT, sp->nocache,
												 sp->lean_query,sp->edns_query,2,0,
												 !global.paranoid,topdomain, rjl);
										if(!one_up) {
											da_free(adrs);
											goto done;
										}
										if(++k==l) k=0;
									} while(k!=kstart);
									da_free(adrs);
									DEBUG_PDNSDA_MSG("Not querying root-server %s, using cached information instead.\n",
											 PDNSDA2STR(PDNSD_A2_TO_A(&at->a)));
									seenrootserv=2;
									break;
								}
							}
							else if(seenrootserv==2)
								break;
						}
						if(serv_has_rejectlist(sp) && !rjl) {
							rjl=add_rejectlist(rejectlist,sp);
							if(!rjl) {one_up=0; goto done;}
							rejectlist=rjl;
						}
						{
							one_up=add_qserv(&serv, &at->a, sp->port, sp->timeout,
									 mk_flag_val(sp), sp->nocache, sp->lean_query,sp->edns_query,
									 sp->rootserver?3:(!sp->is_proxy),
									 needs_testing(sp), 1, NULL, rjl);
						}
						if(!one_up)
							goto done;
					}
					if(++j==m) j=0;
				} while(j!=jstart);
			}
		}
	}
 done:
	unlock_server_data();
	if (one_up) {
		dns_cent_t *cached;
		int nocache;
		rc=p_recursive_query(serv, name, thint, &cached, &nocache, hops, NULL, qhlist, c_soa);
		if (rc==RC_OK) {
			if (!nocache) {
				dns_cent_t *tc;
				add_cache(cached);
				if ((tc=lookup_cache(name,NULL))) {
					/* The cache may hold more information  than the recent query yielded.
					 * try to get the merged record. If that fails, revert to the new one. */
					free_cent(cached  DBG1);
					pdnsd_free(cached);
					cached=tc;
				} else
					DEBUG_MSG("p_dns_resolve: merging answer with cache failed, using local cent copy.\n");
			} else
				DEBUG_MSG("p_dns_resolve: nocache.\n");

			*cachedp=cached;
		}
	}
	else {
		DEBUG_MSG("No server is marked up and allowed for this domain.\n");
		rc=RC_SERVFAIL; /* No server up */
	}
	del_qserv(serv);
	free_rejectlist(rejectlist);
	return rc;
}

static int set_flags_ttl(unsigned short *flags, time_t *ttl, dns_cent_t *cached, int tp)
{
	rr_set_t *rrset=getrrset(cached,tp);
	if (rrset) {
		time_t t;
		*flags|=rrset->flags;
		t=rrset->ts+CLAT_ADJ(rrset->ttl);
		if (!*ttl || *ttl>t)
			*ttl=t;
		return 1;
	}
	return 0;
}

static void set_all_flags_ttl(unsigned short *flags, time_t *ttl, dns_cent_t *cached)
{
	int i, ilim= RRARR_LEN(cached);

	for(i=0; i<ilim; ++i) {
		rr_set_t *rrset= RRARR_INDEX(cached,i);
		if (rrset) {
			time_t t;
			*flags|=rrset->flags;
			t=rrset->ts+CLAT_ADJ(rrset->ttl);
			if (!*ttl || *ttl>t)
				*ttl=t;
		}
	}
}

/*
  Lookup name in the cache, and if records of type thint are found, check whether a requery is needed.
  Possible returns values are:
    RC_OK:        the name is locally defined.
    RC_NAMEERR:   the name is locally negatively cached.
    RC_CACHED:    name was found in the cache, requery not needed.
    RC_STALE:     name was found in the cache, but requery is needed.
    RC_NOTCACHED: name was not found in the cache.
*/
static int lookup_cache_status(const unsigned char *name, int thint, dns_cent_t **cachedp, unsigned short *flagsp,
			       time_t queryts, unsigned char *c_soa)
{
	dns_cent_t *cached;
	int rc=RC_NOTCACHED;
	int wild=0;
	unsigned short flags=0;

	if ((cached=lookup_cache(name,&wild))) {
		short int neg=0,timed=0,need_req=0;
		time_t ttl=0;

		if (cached->flags&DF_LOCAL) {
#if DEBUG>0
			{
				char dflagstr[DFLAGSTRLEN];
				DEBUG_RHN_MSG("Entry found in cache for '%s' with dflags=%s.\n",
					      RHN2STR(cached->qname),dflags2str(cached->flags,dflagstr));
			}
#endif
			if((cached->flags&DF_NEGATIVE) || wild==w_locnerr) {
				if(c_soa) {
					if(cached->c_soa!=cundef)
						*c_soa=cached->c_soa;
					else if(have_rr_SOA(cached))
						*c_soa=rhnsegcnt(cached->qname);
					else {
						unsigned char *owner=getlocalowner(cached->qname,T_SOA);
						if(owner)
							*c_soa=rhnsegcnt(owner);
					}
				}
				free_cent(cached  DBG1);
				pdnsd_free(cached);
				rc= RC_NAMEERR;
				goto return_rc;
			}
			else {
				rc= RC_OK;
				goto return_rc_cent;
			}
		}
		DEBUG_RHN_MSG("Record found in cache for %s\n",RHN2STR(cached->qname));
		if (cached->flags&DF_NEGATIVE) {
			if ((ttl=cached->neg.ts+CLAT_ADJ(cached->neg.ttl))>=queryts)
				neg=1;
			else
				timed=1;
		} else {
			if (thint==QT_ALL) {
				set_all_flags_ttl(&flags, &ttl, cached);
			}
			else if (!set_flags_ttl(&flags, &ttl, cached, T_CNAME) || (getrrset_CNAME(cached)->flags&CF_NEGATIVE)) {
				flags=0; ttl=0;
				if (thint>=T_MIN && thint<=T_MAX) {
					if (set_flags_ttl(&flags, &ttl, cached, thint))
						neg=getrrset(cached,thint)->flags&CF_NEGATIVE && ttl>=queryts;
				}
				else if (thint==QT_MAILB) {
					set_flags_ttl(&flags, &ttl, cached, T_MB);
					set_flags_ttl(&flags, &ttl, cached, T_MG);
					set_flags_ttl(&flags, &ttl, cached, T_MR);
				}
				else if (thint==QT_MAILA) {
					set_flags_ttl(&flags, &ttl, cached, T_MD);
					set_flags_ttl(&flags, &ttl, cached, T_MF);
				}
			}
			if(!(flags&CF_LOCAL)) {
				if (thint==QT_ALL) {
					if(!(cached->flags&DF_AUTH))
						need_req=1;
				}
				else if (thint>=QT_MIN && thint<=QT_MAX) {
					if(!(flags&CF_AUTH && !(flags&CF_ADDITIONAL)))
						need_req=1;
				}
				if (ttl<queryts)
					timed=1;
			}
		}
#if DEBUG>0
		{
			char dflagstr[DFLAGSTRLEN],cflagstr[CFLAGSTRLEN];
			DEBUG_MSG("Requery decision: dflags=%s, cflags=%s, req=%i, neg=%i, timed=%i, %s=%li\n",
				  dflags2str(cached->flags,dflagstr),cflags2str(flags,cflagstr),need_req,neg,timed,
				  ttl?"ttl":"timestamp",(long)(ttl?(ttl-queryts):ttl));
		}
#endif
		rc = (!neg && (need_req || timed))? RC_STALE: RC_CACHED;
	return_rc_cent:
		*cachedp=cached;
	}

return_rc:
	if(flagsp) *flagsp=flags;
	return rc;
}


/*
 * Resolve records for name into dns_cent_t, type thint
 * q is the set of servers to query from. Set q to NULL if you want to ask the servers registered with pdnsd.
 * qslist should refer to a list of server arrays already used higher up the calling chain (may be NULL).
 */
static int p_dns_cached_resolve(query_stat_array q, const unsigned char *name, int thint, dns_cent_t **cachedp,
				int hops, qstatnode_t *qslist, qhintnode_t *qhlist, time_t queryts,
				unsigned char *c_soa)
{
	dns_cent_t *cached=NULL;
	int rc;
	unsigned short flags=0;

	DEBUG_RHN_MSG("Starting cached resolve for: %s, query %s\n",RHN2STR(name),get_tname(thint));
	rc= lookup_cache_status(name, thint, &cached, &flags,queryts,c_soa);
	if(rc==RC_OK) {
		/* Locally defined record. */
		*cachedp=cached;
		return RC_OK;
	}
	else if(rc==RC_NAMEERR)  /* Locally negated name. */
		return RC_NAMEERR;

	/* update server records set onquery */
	if(global.onquery) test_onquery();
	if (global.lndown_kluge && !(flags&CF_LOCAL)) {
		int i,n,linkdown=1;
		lock_server_data();
		n=DA_NEL(servers);
		for(i=0;i<n;++i) {
			servparm_t *sp=&DA_INDEX(servers,i);
			if(sp->rootserver<=1) {
				int j,m=DA_NEL(sp->atup_a);
				for(j=0;j<m;++j) {
					if (DA_INDEX(sp->atup_a,j).is_up) {
						linkdown=0;
						goto done;
					}
				}
			}
		}
	done:
		unlock_server_data();
		if (linkdown) {
			DEBUG_MSG("Link is down.\n");
			rc=RC_SERVFAIL;
			goto cleanup_return;
		}
	}
	if (rc!=RC_CACHED) {
		dns_cent_t *ent;
		DEBUG_MSG("Trying name servers.\n");
		if (q)
			rc=p_recursive_query(q,name,thint, &ent,NULL,hops,qslist,qhlist,c_soa);
		else
			rc=p_dns_resolve(name,thint, &ent,hops,qhlist,c_soa);
		if (rc!=RC_OK) {
			if (rc==RC_SERVFAIL && cached && (flags&CF_NOPURGE)) {
				/* We could not get a new record, but we have a timed-out cached one
				   with the nopurge flag set. This means that we shall use it even
				   if timed out when no new one is available*/
				DEBUG_MSG("Falling back to cached record.\n");
			} else {
				goto cleanup_return;
			}
		} else {
			if (cached) {
				free_cent(cached  DBG1);
				pdnsd_free(cached);
			}
			cached=ent;
		}
	} else {
		DEBUG_MSG("Using cached record.\n");
	}
	*cachedp=cached;
	return RC_OK;

 cleanup_return:
	if(cached) {
		free_cent(cached  DBG1);
		pdnsd_free(cached);
	}
	return rc;
}


/* r_dns_cached_resolve() is like p_dns_cached_resolve(), except that r_dns_cached_resolve()
   will not return negatively cached entries, but return RC_NAMEERR instead.
*/
int r_dns_cached_resolve(unsigned char *name, int thint, dns_cent_t **cachedp,
			 int hops, qhintnode_t *qhlist, time_t queryts,
			 unsigned char *c_soa)
{
	dns_cent_t *cached;
	int rc=p_dns_cached_resolve(NULL,name,thint,&cached,hops,NULL,qhlist,queryts,c_soa);
	if(rc==RC_OK) {
		if(cached->flags&DF_NEGATIVE) {
			if(c_soa)
				*c_soa=cached->c_soa;
			free_cent(cached  DBG1);
			pdnsd_free(cached);
			rc=RC_NAMEERR;
		}
		else
			*cachedp=cached;
	}
	return rc;
}


static int simple_dns_cached_resolve(atup_array atup_a, int port, char edns_query, time_t timeout,
				     const unsigned char *name, int thint, dns_cent_t **cachedp)
{
	dns_cent_t *cached=NULL;
	int rc;

	DEBUG_RHN_MSG("Starting simple cached resolve for: %s, query %s\n",RHN2STR(name),get_tname(thint));
	rc= lookup_cache_status(name, thint, &cached, NULL, time(NULL), NULL);
	if(rc==RC_OK) {
		/* Locally defined record. */
		*cachedp=cached;
		return RC_OK;
	}
	else if(rc==RC_NAMEERR)  /* Locally negated name. */
		return RC_NAMEERR;

	if (rc!=RC_CACHED) {
		query_stat_array qserv;
		int j,m;
		if (cached) {
			free_cent(cached  DBG1);
			pdnsd_free(cached);
			cached=NULL;
		}
		DEBUG_MSG("Trying name servers.\n");
		qserv=NULL;
		m=DA_NEL(atup_a);
		for(j=0; j<m; ++j) {
			if(!add_qserv(&qserv, &DA_INDEX(atup_a,j).a, port, timeout, 0, 0, 1, edns_query, 0, 0, 1, NULL, NULL)) {
				/* Note: qserv array already cleaned up by add_qserv() */
				return RC_SERVFAIL;
			}
		}
		rc=p_recursive_query(qserv,name,thint, &cached,NULL,0,NULL,NULL,NULL);
		del_qserv(qserv);
		if (rc==RC_OK) {
			dns_cent_t *tc;
			add_cache(cached);
			if ((tc=lookup_cache(name,NULL))) {
				/* The cache may hold more information  than the recent query yielded.
				 * try to get the merged record. If that fails, revert to the new one. */
				free_cent(cached  DBG1);
				pdnsd_free(cached);
				cached=tc;
			} else
				DEBUG_MSG("simple_dns_cached_resolve: merging answer with cache failed, using local cent copy.\n");
		}
		else
			return rc;
	} else {
		DEBUG_MSG("Using cached record.\n");
	}

	if(cached->flags&DF_NEGATIVE) {
		free_cent(cached  DBG1);
		pdnsd_free(cached);
		return RC_NAMEERR;
	}

	*cachedp=cached;
	return RC_OK;
}


/* Check whether a server is responsive by sending it an (empty) query.
   rep is the number of times this is tried in case of no reply.
 */
int query_uptest(pdnsd_a *addr, int port, const unsigned char *name, time_t timeout, int rep)
{
	query_stat_t qs;
	int iter=0,rv;

#ifdef ENABLE_IPV4
	if (run_ipv4) {
		memset(&qs.a.sin4,0,sizeof(qs.a.sin4));
		qs.a.sin4.sin_family=AF_INET;
		qs.a.sin4.sin_port=htons(port);
		qs.a.sin4.sin_addr=addr->ipv4;
		SET_SOCKA_LEN4(qs.a.sin4);
	}
#endif
#ifdef ENABLE_IPV6
	ELSE_IPV6 {
		memset(&qs.a.sin6,0,sizeof(qs.a.sin6));
		qs.a.sin6.sin6_family=AF_INET6;
		qs.a.sin6.sin6_port=htons(port);
		qs.a.sin6.sin6_flowinfo=IPV6_FLOWINFO;
		qs.a.sin6.sin6_addr=addr->ipv6;
		SET_SOCKA_LEN6(qs.a.sin6);

		qs.a4fallback.s_addr=INADDR_ANY;
	}
#endif
	qs.timeout=timeout;
	qs.flags=0;
	qs.nocache=0;
	qs.auth_serv=0;
	qs.lean_query=1;
	qs.edns_query=0;
	qs.needs_testing=0;
	qs.trusted=1;
	qs.aa=0;
	qs.tc=0;
	qs.nsdomain=NULL;
	qs.rejectlist=NULL;

 try_again:
	qs.state=QS_INITIAL;
	qs.qm=global.query_method;
	qs.s_errno=0;
	rv=p_exec_query(NULL, name, T_A, &qs, NULL, NULL);
	if(rv==-1) {
		time_t ts, tpassed;
		for(ts=time(NULL), tpassed=0;; tpassed=time(NULL)-ts) {
			int event;
#ifdef NO_POLL
			fd_set reads;
			fd_set writes;
			struct timeval tv;
			FD_ZERO(&reads);
			FD_ZERO(&writes);
			PDNSD_ASSERT(qs.sock<FD_SETSIZE,"socket file descriptor exceeds FD_SETSIZE.");
			switch (qs.state) {
			QS_READ_CASES:
				FD_SET(qs.sock,&reads);
				break;
			QS_WRITE_CASES:
				FD_SET(qs.sock,&writes);
				break;
			}
			tv.tv_sec=timeout>tpassed?timeout-tpassed:0;
			tv.tv_usec=0;
			/* There is a possible race condition with the arrival of a signal here,
			   but it is so unlikely to be a problem in practice that doing
			   this properly is not worth the trouble.
			*/
			if(is_interrupted_servstat_thread()) {
				DEBUG_MSG("server status thread interrupted.\n");
				p_cancel_query(&qs);
				return 0;
			}
			event=select(qs.sock+1,&reads,&writes,NULL,&tv);
#else
			struct pollfd pfd;
			pfd.fd=qs.sock;
			switch (qs.state) {
			QS_READ_CASES:
				pfd.events=POLLIN;
				break;
			QS_WRITE_CASES:
				pfd.events=POLLOUT;
				break;
			default:
				pfd.events=0;
			}
			/* There is a possible race condition with the arrival of a signal here,
			   but it is so unlikely to be a problem in practice that doing
			   this properly is not worth the trouble.
			*/
			if(is_interrupted_servstat_thread()) {
				DEBUG_MSG("server status thread interrupted.\n");
				p_cancel_query(&qs);
				return 0;
			}
			event=poll(&pfd,1,timeout>tpassed?(timeout-tpassed)*1000:0);
#endif
			if (event<0) {
				if(errno==EINTR && is_interrupted_servstat_thread()) {
					DEBUG_MSG("poll/select interrupted in server status thread.\n");
				}
				else
					log_warn("poll/select failed: %s",strerror(errno));
				p_cancel_query(&qs);
				return 0;
			}
			if(event==0) {
				/* timed out */
				p_cancel_query(&qs);
				if(++iter<rep) goto try_again;
				return 0;
			}
			event=0;
#ifdef NO_POLL
			switch (qs.state) {
			QS_READ_CASES:
				event=FD_ISSET(qs.sock,&reads);
				break;
			QS_WRITE_CASES:
				event=FD_ISSET(qs.sock,&writes);
				break;
			}
#else
			switch (qs.state) {
			QS_READ_CASES:
				event=pfd.revents&(POLLIN|POLLERR|POLLHUP|POLLNVAL);
				break;
			QS_WRITE_CASES:
				event=pfd.revents&(POLLOUT|POLLERR|POLLHUP|POLLNVAL);
				break;
			}
#endif
			if(event) {
				rv=p_exec_query(NULL, name, T_A, &qs, NULL, NULL);
				if(rv!=-1) break;
			}
			else {
				if(++poll_errs<=MAXPOLLERRS)
					log_error("Unhandled poll/select event in query_uptest() at %s, line %d.",__FILE__,__LINE__);
				p_cancel_query(&qs);
				return 0;
			}
		}
	}
	return (rv!=RC_SERVFAIL && rv!=RC_FATALERR);
}
