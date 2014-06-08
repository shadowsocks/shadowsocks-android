/* dns.c - Declarations for dns handling and generic dns functions

   Copyright (C) 2000, 2001 Thomas Moestl
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2009, 2011 Paul A. Rombouts

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
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "error.h"
#include "helpers.h"
#include "dns.h"


/* Decompress a name record, taking the whole message as msg, returning its results in tgt
 * (which should be able hold at least DNSNAMEBUFSIZE chars),
 * taking sz as the remaining msg size (it is returned decremented by the name length, ready for further use) and
 * a source pointer (it is returned pointing to the location after the name). msgsize is the size of the whole message,
 * len is the total name length.
 * msg and msgsz are needed for decompression (see rfc1035). The returned data is decompressed, but still in the
 * rr name form (length byte - string of that length, terminated by a 0 length byte).
 *
 * Returned is a dns return code, with one exception: RC_TRUNC, as defined in dns.h, indicates that the message is
 * truncated at the name (which needs a special return code, as it might or might not be fatal).
 */
int decompress_name(unsigned char *msg, size_t msgsz, unsigned char **src, size_t *sz, unsigned char *tgt, unsigned int *len)
{
	unsigned int lb,offs;
	unsigned int hops=0,tpos=0;
	unsigned char *lptr=*src;
	size_t oldsz=*sz;
	size_t newsz=oldsz;

	if (newsz==0)
		goto name_outside_data;
	if (lptr-msg>=msgsz)
		goto name_outside_msg;

	for(;;) {
		newsz--;
		lb=*lptr++;

		if(lb>0x3f) {
 			if (lb<0xc0)     /* The two highest bits must be either 00 or 11 */
				goto unsupported_lbl_bits;
			if (newsz==0)
				goto name_outside_data;
			if (lptr-msg>=msgsz)
				goto name_outside_msg;
			newsz--;
			offs=((lb&0x3f)<<8)|(*lptr);
			if (offs>=msgsz)
				goto offset_outside_msg;
			lptr=msg+offs;
			goto jumped;
		}
 		tgt[tpos++]=lb;
		if (lb==0)
			break;

		if (newsz<=lb)
			goto name_outside_data;
		if (lptr+lb-msg>=msgsz)
			goto name_outside_msg;
		if (tpos+lb>DNSNAMEBUFSIZE-1) /* terminating null byte has to follow */
			goto name_buf_full;
		newsz -= lb;
		do {
			/* if (!*lptr || *lptr=='.')
				return RC_FORMAT; */
			tgt[tpos++]=*lptr++;
		} while(--lb);
	}
	goto return_OK;

 jumped:
	++hops;
	for(;;) {
		lb=*lptr++;

		while(lb>0x3f) {
 			if (lb<0xc0)     /* The two highest bits must be either 00 or 11 */
				goto unsupported_lbl_bits;
			if (lptr-msg>=msgsz)
				goto name_outside_msg;
			if (++hops>255)
				goto too_many_hops;
			offs=((lb&0x3f)<<8)|(*lptr);
			if (offs>=msgsz)
				goto offset_outside_msg;
			lptr=msg+offs;
			lb=*lptr++;
		}
 		tgt[tpos++]=lb;
		if (lb==0)
			break;

		if (lptr+lb-msg>=msgsz)
			goto name_outside_msg;
		if(tpos+lb>DNSNAMEBUFSIZE-1) /* terminating null byte has to follow */
			goto name_buf_full;
		do {
			/* if (!*lptr || *lptr=='.')
				return RC_FORMAT; */
			tgt[tpos++]=*lptr++;
		} while(--lb);
	}
 return_OK:
	*src += oldsz-newsz;
	*sz = newsz;
	if(len) *len=tpos;
	return RC_OK;

 name_outside_data:
	DEBUG_MSG("decompress_name: compressed name extends outside data field.\n");
	return RC_TRUNC;

 name_outside_msg:
	DEBUG_MSG("decompress_name: compressed name extends outside message.\n");
	return RC_FORMAT;

 unsupported_lbl_bits:
	DEBUG_MSG(lb==0x41?"decompress_name: Bit-string labels not supported.\n":
		  "decompress_name: unsupported label type.\n");
	return RC_FORMAT;

 offset_outside_msg:
	DEBUG_MSG("decompress_name: offset points outside message.\n");
	return RC_FORMAT;

 name_buf_full:
	DEBUG_MSG("decompress_name: decompressed name larger than %u bytes.\n", DNSNAMEBUFSIZE);
	return RC_FORMAT;

 too_many_hops:
	DEBUG_MSG("decompress_name: too many offsets in compressed name.\n");
	return RC_FORMAT;
}

#if 0
/* Compare two names (ordinary C-strings) back-to-forth and return the longest match.
   The comparison is done at name granularity.
   The return value is the length of the match in name elements.
   *os (*od) is set to the offset in the domain name ms (md) of the match.
 */
int domain_name_match(const unsigned char *ms, const unsigned char *md, int *os, int *od)
{
	int i,j,k=0,offs,offd;

	offs=i=strlen(ms); offd=j=strlen(md);
	if(i && ms[i-1]=='.') --offs;
	if(j && md[j-1]=='.') --offd;

	if(i==0 || (i==1 && *ms=='.') || j==0 || (j==1 && *md=='.'))
		/* Special case: root domain */
		;
	else {
		--i; if(ms[i]=='.') --i;
		--j; if(md[j]=='.') --j;
		while(tolower(ms[i]) == tolower(md[j])) {
			if(ms[i]=='.') {
				++k;
				offs=i+1; offd=j+1;
			}
			if(i==0 || j==0) {
				if((i==0 || ms[i-1]=='.') && (j==0 || md[j-1]=='.')) {
					++k;
					offs=i; offd=j;
				}
				break;
			}
			--i; --j;
		}
	}
	if(os) *os=offs;
	if(od) *od=offd;
	return k;
}
#endif

/* Compare the names (in length byte-string notation) back-to-forth and return the longest match.
   The comparison is done at name granularity.
   The return value is the length of the match in name elements.
   *os (*od) is set to the offset in the domain name ms (md) of the match.
 */
unsigned int domain_match(const unsigned char *ms, const unsigned char *md, unsigned int *os, unsigned int *od)
{
	unsigned int i,j,k,n,ns=0,nd=0,offs,offd;
	unsigned char lb,ls[128],ld[128];

	/* first collect all length bytes */
	i=0;
	while((lb=ms[i])) {
		PDNSD_ASSERT(ns<128, "domain_match: too many name segments");
		ls[ns++]=lb;
		i += ((unsigned)lb)+1;
	}

	j=0;
	while((lb=md[j])) {
		PDNSD_ASSERT(nd<128, "domain_match: too many name segments");
		ld[nd++]=lb;
		j += ((unsigned)lb)+1;
	}

	n=ns;  if(n>nd) n=nd;

	for(k=1; offs=i,offd=j,k<=n; ++k) {
		lb=ls[ns-k];
		if(lb!=ld[nd-k]) goto mismatch;
		for(;lb;--lb)
			if(tolower(ms[--i]) != tolower(md[--j])) goto mismatch;
		--i; --j;
	}
 mismatch:

	if(os) *os=offs;
	if(od) *od=offd;
	return k-1;
}

/* compress the domain name in in and put the result (of maximum length of rhnlen(in)) and
 * fill cb with compression information for further strings.*cb may be NULL initially.
 * offs is the offset the generated string will be placed in the packet.
 * retval: 0 - error, otherwise length
 * When done, just free() cb (if it is NULL, free will behave correctly).
 * It is guaranteed (and insured by assertions) that the output is smaller or equal in
 * size to the input.
 */
unsigned int compress_name(unsigned char *in, unsigned char *out, unsigned int offs, dlist *cb)
{
	compel_t *ci;
	unsigned int longest=0,lrem=0,coffs=0;
	unsigned int rl=0;
	unsigned ilen = rhnlen(in);
	unsigned short add=1;

	PDNSD_ASSERT(ilen<=DNSNAMEBUFSIZE, "compress_name: name too long");

	/* part 1: compression */
	for (ci=dlist_first(*cb); ci; ci=dlist_next(ci)) {
		unsigned int rv,rem,to;
		if ((rv=domain_match(in, ci->s, &rem,&to))>longest) {
			/*
			 * This has some not obvious implications that should be noted: If a
			 * domain name as saved in the list has been compressed, we only can
			 * index the non-compressed part. We rely here that the first occurence
			 * can't be compressed. So we take the first occurence of a given length.
			 * This works perfectly, but watch it if you change something.
			 */
			unsigned int newoffs= ci->index + to;
			 /* Only use if the offset is not too large. */
			if(newoffs<=0x3fff) {
				longest=rv;
				lrem=rem;
				coffs= newoffs;
			}
		}
	}
	if (longest>0) {
		PDNSD_ASSERT(lrem+2 <= ilen, "compress_name: length increased");
		memcpy(out, in,lrem);
		out[lrem]=0xc0|((coffs&0x3f00)>>8);
		out[lrem+1]=coffs&0xff;
		rl=lrem+2;
		add= lrem!=0;
	}
	else {
		memcpy(out,in,ilen);
		rl=ilen;
	}

	/* part 2: addition to the cache structure */
	if (add) {
		if (!(*cb=dlist_grow(*cb,sizeof(compel_t)+ilen)))
			return 0;
		ci=dlist_last(*cb);
		ci->index=offs;
		memcpy(ci->s,in,ilen);
	}
	return rl;
}

/* Convert a numeric IP address into a domain name representation
   (C string) suitable for PTR records.
   buf is assumed to be at least DNSNAMEBUFSIZE bytes in size.
*/
int a2ptrstr(pdnsd_ca *a, int tp, unsigned char *buf)
{
	if(tp==T_A) {
		unsigned char *p=(unsigned char *)&a->ipv4.s_addr;
		int n=snprintf(charp buf,DNSNAMEBUFSIZE,"%u.%u.%u.%u.in-addr.arpa.",p[3],p[2],p[1],p[0]);
		if(n<0 || n>=DNSNAMEBUFSIZE)
			return 0;
	}
	else
#if ALLOW_LOCAL_AAAA
	if(tp==T_AAAA) {
		unsigned char *p=(unsigned char *)&a->ipv6;
		int i,offs=0;
		for (i=15;i>=0;--i) {
			unsigned char bt=p[i];
			int n=snprintf(charp(buf+offs), DNSNAMEBUFSIZE-offs,"%x.%x.",bt&0xf,(bt>>4)&0xf);
			if(n<0) return 0;
			offs+=n;
			if(offs>=DNSNAMEBUFSIZE) return 0;
		}
		if(!strncp(charp(buf+offs),"ip6.arpa.",DNSNAMEBUFSIZE-offs))
			return 0;
	}
	else
#endif
		return 0;
	return 1;
}

/*
 * Add records for a host as read from a hosts-style file.
 * Returns 1 on success, 0 in an out of memory condition, and -1 when there was a problem with
 * the record data.
 */
static int add_host(unsigned char *pn, unsigned char *rns, pdnsd_ca *a, int tp, int a_sz, time_t ttl, unsigned flags, int reverse)
{
	dns_cent_t ce;

	if (!init_cent(&ce, pn, 0, 0, flags  DBG0))
		return 0;
	if (!add_cent_rr(&ce,tp,ttl,0,CF_LOCAL,a_sz,a  DBG0))
		goto free_cent_return0;
	if (!add_cent_rr(&ce,T_NS,ttl,0,CF_LOCAL,rhnlen(rns),rns  DBG0))
		goto free_cent_return0;
	add_cache(&ce);
	free_cent(&ce  DBG0);
	if (reverse) {
		unsigned char b2[DNSNAMEBUFSIZE],rhn[DNSNAMEBUFSIZE];
		if(!a2ptrstr(a,tp,b2))
			return -1;
		if (!str2rhn(b2,rhn))
			return -1;
		if (!init_cent(&ce, rhn, 0, 0, flags  DBG0))
			return 0;
		if (!add_cent_rr(&ce,T_PTR,ttl,0,CF_LOCAL,rhnlen(pn),pn  DBG0))
			goto free_cent_return0;
		if (!add_cent_rr(&ce,T_NS,ttl,0,CF_LOCAL,rhnlen(rns),rns  DBG0))
			goto free_cent_return0;
		add_cache(&ce);
		free_cent(&ce  DBG0);
	}
	return 1;

 free_cent_return0:
	free_cent(&ce  DBG0);
	return 0;
}

/*
 * Read a file in /etc/hosts-format and add generate rrs for it.
 * Errors are largely ignored so that we can skip entries we do not understand
 * (but others possibly do).
 */
int read_hosts(const char *fn, unsigned char *rns, time_t ttl, unsigned flags, int aliases, char **errstr)
{
	int rv=0;
	FILE *f;
	char *buf;
	size_t buflen=256;

	if (!(f=fopen(fn,"r"))) {
		if(asprintf(errstr, "Failed to source %s: %s", fn, strerror(errno))<0) *errstr=NULL;
		return 0;
	}
	buf=malloc(buflen);
	if(!buf) {
		*errstr=NULL;
		goto fclose_return;
	}
	while(getline(&buf,&buflen,f)>=0) {
		unsigned int len;
		unsigned char *p,*pn,*pi;
		unsigned char rhn[DNSNAMEBUFSIZE];
		int tp,sz;
		pdnsd_ca a;

		p= ucharp strchr(buf,'#');
		if(p) *p=0;
		p= ucharp buf;
		for(;;) {
			if(!*p) goto nextline;
			if(!isspace(*p)) break;
			++p;
		}
		pi=p;
		do {
			if(!*++p) goto nextline;
		} while(!isspace(*p));
		*p=0;
		do {
			if(!*++p) goto nextline;
		} while (isspace(*p));
		pn=p;
		do {
			++p;
		} while(*p && !isspace(*p));
		len=p-pn;
		if (parsestr2rhn(pn,len,rhn)!=NULL)
			continue;
		if (inet_aton(charp pi,&a.ipv4)) {
			tp=T_A;
			sz=sizeof(struct in_addr);
		} else {
#if ALLOW_LOCAL_AAAA    /* We don't read them otherwise, as the C library may not be able to to that.*/
			if (inet_pton(AF_INET6,charp pi,&a.ipv6)>0) {
				tp=T_AAAA;
				sz=sizeof(struct in6_addr);
			} else
#endif
			continue;
		}
		{
			int res=add_host(rhn, rns, &a, tp,sz, ttl, flags, 1);
			if(res==0) {
				*errstr= NULL;
				goto cleanup_return;
			}
			else if(res<0)
				continue;
		}
		if(aliases) {
			for(;;) {
				for(;;) {
					if(!*p) goto nextline;
					if(!isspace(*p)) break;
					++p;
				}
				pn=p;
				do {
					++p;
				} while(*p && !isspace(*p));
				len=p-pn;
				if (parsestr2rhn(pn,len,rhn)!=NULL)
					break;
				if (add_host(rhn, rns, &a, tp,sz, ttl, flags, 0) == 0) {
					*errstr= NULL;
					goto cleanup_return;
				}
			}
		}
	nextline:;
	}
	if (feof(f))
		rv=1;
	else if(asprintf(errstr, "Failed to source %s: %s", fn, strerror(errno))<0) *errstr=NULL;
 cleanup_return:
	free(buf);
 fclose_return:
	fclose(f);
	return rv;
}


/* Get the name of an RR type given its value. */
const char *getrrtpname(int tp)
{
	return tp>=T_MIN && tp<=T_MAX? rrnames[tp-T_MIN]: "[unknown]";
}

#if DEBUG>0
/*
 * Const decoders for debugging display
 */
static const char *const c_names[C_NUM] = {"IN","CS","CH","HS"};
static const char *const qt_names[QT_NUM]={"IXFR","AXFR","MAILB","MAILA","*"};

const char *get_cname(int id)
{
	if (id>=C_MIN && id<=C_MAX)
		return c_names[id-C_MIN];
	if (id==QC_ALL)
		return "*";
	return "[unknown]";
}

const char *get_tname(int id)
{
	if (id>=T_MIN && id<=T_MAX)
		return rrnames[id-T_MIN];
        else if (id>=QT_MIN && id<=QT_MAX)
		return qt_names[id-QT_MIN];
	return "[unknown]";
}


#define NRC 17
static const char *const e_names[NRC]={
	"no error",
	"query format error",
	"server failed",
	"non-existent domain",
	"not supported",
	"query refused",
	"name exists when it should not",
	"RR set exists when it should not",
	"RR set that should exist does not",
	"server not authoritative for zone",
	"name not contained in zone",
	"11",
	"12",
	"13",
	"14",
	"15",
	"bad OPT version"
};

const char *get_ename(int id)
{
	if (id>=0 && id<NRC)
		return e_names[id];
	return "[unknown]";
}


/* Construct a human-readable string listing the flags that are set
   in a dns header. buf must have a size of at least DNSFLAGSMAXSTRSIZE.
   Used for debugging purposes only.
*/
char *dnsflags2str(dns_hdr_t *hdr, char *buf)
{
	char *p= buf;

	if (hdr->aa)
		p=mempcpy(p, " AA", 3);
	if (hdr->tc)
		p=mempcpy(p, " TC", 3);
	if (hdr->rd)
		p=mempcpy(p, " RD", 3);
	if (hdr->ra)
		p=mempcpy(p, " RA", 3);
	if (hdr->z)
		p=mempcpy(p, " Z", 2);
	if (hdr->ad)
		p=mempcpy(p, " AD", 3);
	if (hdr->cd)
		p=mempcpy(p, " CD", 3);
	*p=0;

	return buf;
}

#endif


#if DEBUG>=9
/* Based on debug code contributed by Kiyo Kelvin Lee. */

void debug_dump_dns_msg(void *data, size_t len)
{
	unsigned char *udata = (unsigned char *)data;
#       define dmpchksz 16
	char buf[dmpchksz*4+2];
	size_t i, j, k, l;

	DEBUG_MSG("pointer=%p len=%lu\n", udata, (unsigned long)len);

	for (i = 0; i < len; i += dmpchksz) {
		char *cp = buf;
		k = l = i + dmpchksz;
		if(k > len) k = len;
		for (j = i; j < k; ++j) {
			int n = sprintf(cp, "%02x ", udata[j]);
			cp += n;
		}
		for (; j < l; ++j) {
			*cp++ = ' ';
			*cp++ = ' ';
			*cp++ = ' ';
		}
		*cp++ = ' ';
		for (j = i; j < k; ++j) {
			*cp++ = isprint(udata[j]) ? udata[j] : '.';
		}
		PDNSD_ASSERT(cp < buf + sizeof(buf), "debug_dump_dns_msg: line buffer overflowed");
		*cp = '\0';
		DEBUG_MSG("%s\n", buf);
	}

	if(len >= sizeof(dns_hdr_t)) {
		dns_hdr_t *hdr = (dns_hdr_t *)data;

		DEBUG_MSG(
			"id=%04x qr=%x opcode=%x aa=%x tc=%x rd=%x "
			"ra=%x z=%x ad=%x cd=%x rcode=%x\n",
			ntohs(hdr->id), hdr->qr, hdr->opcode, hdr->aa, hdr->tc, hdr->rd,
			hdr->ra, hdr->z, hdr->ad, hdr->cd, hdr->rcode);
		DEBUG_MSG(
			"qdcount=%04x ancount=%04x nscount=%04x arcount=%04x\n",
			ntohs(hdr->qdcount), ntohs(hdr->ancount), ntohs(hdr->nscount), ntohs(hdr->arcount));
	}
}
#endif
