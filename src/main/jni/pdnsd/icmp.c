/* icmp.c - Server response tests using ICMP echo requests

   Copyright (C) 2000, 2001 Thomas Moestl
   Copyright (C) 2003, 2005, 2007, 2012 Paul A. Rombouts

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
 * This should now work on both Linux and FreeBSD (and CYGWIN?). If anyone
 * with experience in other Unix flavors wants to contribute platform-specific
 * code, he is very welcome.
 */

#include <config.h>
#ifdef HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif
#include <sys/time.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "ipvers.h"
#if (TARGET==TARGET_LINUX)
# include <netinet/ip.h>
# include <linux/types.h>
# include <linux/icmp.h>
#elif (TARGET==TARGET_BSD)
# include <netinet/in_systm.h>
# include <netinet/ip.h>
# include <netinet/ip_icmp.h>
#elif (TARGET==TARGET_CYGWIN)
# include <netinet/ip.h>
# include <netinet/in_systm.h>
# include <netinet/ip_icmp.h>
# include "freebsd_netinet_ip_icmp.h"
#else
# error Unsupported platform!
#endif
#ifdef ENABLE_IPV6
# include <netinet/ip6.h>
# include <netinet/icmp6.h>
#endif
#include <netdb.h>
#include "icmp.h"
#include "error.h"
#include "helpers.h"
#include "servers.h"


#define ICMP_MAX_ERRS 10
static volatile unsigned long icmp_errs=0; /* This is only here to minimize log output.
					      Since the consequences of a race is only
					      one log message more/less (out of
					      ICMP_MAX_ERRS), no lock is required. */

volatile int ping_isocket=-1;
#ifdef ENABLE_IPV6
volatile int ping6_isocket=-1;
#endif

/* different names, same thing... be careful, as these are macros... */
#if (TARGET==TARGET_LINUX)
# define ip_saddr  saddr
# define ip_daddr  daddr
# define ip_hl     ihl
# define ip_p	   protocol
#else
# define icmphdr   icmp
# define iphdr     ip
# define ip_saddr  ip_src.s_addr
# define ip_daddr  ip_dst.s_addr
#endif

#if (TARGET==TARGET_LINUX)
# define icmp_type  type
# define icmp_code  code
# define icmp_cksum checksum
# define icmp_id un.echo.id
# define icmp_seq un.echo.sequence
#else
# define ICMP_DEST_UNREACH  ICMP_UNREACH
# define ICMP_TIME_EXCEEDED ICMP_TIMXCEED
#endif

#define ICMP_BASEHDR_LEN  8
#define ICMP4_ECHO_LEN    ICMP_BASEHDR_LEN

#if (TARGET==TARGET_LINUX) || (TARGET==TARGET_BSD) || (TARGET==TARGET_CYGWIN)
/*
 * These are the ping implementations for Linux/FreeBSD in their IPv4/ICMPv4 and IPv6/ICMPv6 versions.
 * I know they share some code, but I'd rather keep them separated in some parts, as some
 * things might go in different directions there.
 */

/* Initialize the sockets for pinging */
void init_ping_socket()
{
	if ((ping_isocket=socket(PF_INET, SOCK_RAW, IPPROTO_ICMP))==-1) {
		log_warn("icmp ping: socket() failed: %s",strerror(errno));
	}
#ifdef ENABLE_IPV6
	if (!run_ipv4)  {
		/* Failure to initialize the IPv4 ping socket is not
		   necessarily a problem, as long as the IPv6 version works. */
		if ((ping6_isocket=socket(PF_INET6, SOCK_RAW, IPPROTO_ICMPV6))==-1) {
			log_warn("icmpv6 ping: socket() failed: %s",strerror(errno));
		}
	}
#endif
}

/* Takes a packet as send out and a received ICMP packet and looks whether the ICMP packet is
 * an error reply on the sent-out one. packet is only the packet (without IP header).
 * errmsg includes an IP header.
 * to is the destination address of the original packet (the only thing that is actually
 * compared of the IP header). The RFC says that we get at least 8 bytes of the offending packet.
 * We do not compare more, as this is all we need.*/
static int icmp4_errcmp(char *packet, int plen, struct in_addr *to, char *errmsg, int elen, int errtype)
{
	struct iphdr iph;
	struct icmphdr icmph;
	struct iphdr eiph;
	char *data;

	/* XXX: lots of memcpy to avoid unaligned accesses on alpha */
	if (elen<sizeof(struct iphdr))
		return 0;
	memcpy(&iph,errmsg,sizeof(iph));
	if (iph.ip_p!=IPPROTO_ICMP || elen<iph.ip_hl*4+ICMP_BASEHDR_LEN+sizeof(eiph))
		return 0;
	PDNSD_ASSERT(sizeof(icmph) >= ICMP_BASEHDR_LEN, "icmp4_errcmp: ICMP_BASEHDR_LEN botched");
	memcpy(&icmph,errmsg+iph.ip_hl*4,ICMP_BASEHDR_LEN);
	memcpy(&eiph,errmsg+iph.ip_hl*4+ICMP_BASEHDR_LEN,sizeof(eiph));
	if (elen<iph.ip_hl*4+ICMP_BASEHDR_LEN+eiph.ip_hl*4+8)
		return 0;
	data=errmsg+iph.ip_hl*4+ICMP_BASEHDR_LEN+eiph.ip_hl*4;
	return icmph.icmp_type==errtype && memcmp(&to->s_addr, &eiph.ip_daddr, sizeof(to->s_addr))==0 &&
		memcmp(data, packet, plen<8?plen:8)==0;
}

/* IPv4/ICMPv4 ping. Called from ping (see below) */
static int ping4(struct in_addr addr, int timeout, int rep)
{
	int i;
	int isock;
#if (TARGET==TARGET_LINUX)
	struct icmp_filter f;
#endif
	unsigned short id=(unsigned short)get_rand16(); /* randomize a ping id */

	isock=ping_isocket;

#if (TARGET==TARGET_LINUX)
	/* Fancy ICMP filering -- only on Linux (as far is I know) */

	/* In fact, there should be macros for treating icmp_filter, but I haven't found them in Linux 2.2.15.
	 * So, set it manually and unportable ;-) */
	/* This filter lets ECHO_REPLY (0), DEST_UNREACH(3) and TIME_EXCEEDED(11) pass. */
	/* !(0000 1000 0000 1001) = 0xff ff f7 f6 */
	f.data=0xfffff7f6;
	if (setsockopt(isock,SOL_RAW,ICMP_FILTER,&f,sizeof(f))==-1) {
		if (++icmp_errs<=ICMP_MAX_ERRS) {
			log_warn("icmp ping: setsockopt() failed: %s", strerror(errno));
		}
		return -1;
	}
#endif

	for (i=0;i<rep;i++) {
		struct sockaddr_in from,to;
		struct icmphdr icmpd;
		unsigned long sum;
		uint16_t *ptr;
		long tm,tpassed;
		int j;

		icmpd.icmp_type=ICMP_ECHO;
		icmpd.icmp_code=0;
		icmpd.icmp_cksum=0;
		icmpd.icmp_id=htons((uint16_t)id);
		icmpd.icmp_seq=htons((uint16_t)i);

		/* Checksumming - Algorithm taken from nmap. Thanks... */

		ptr=(uint16_t *)&icmpd;
		sum=0;

		for (j=0;j<4;j++) {
			sum+=*ptr++;
		}
		sum = (sum >> 16) + (sum & 0xffff);
		sum += (sum >> 16);
		icmpd.icmp_cksum=~sum;

		memset(&to,0,sizeof(to));
		to.sin_family=AF_INET;
		to.sin_port=0;
		to.sin_addr=addr;
		SET_SOCKA_LEN4(to);
		if (sendto(isock,&icmpd,ICMP4_ECHO_LEN,0,(struct sockaddr *)&to,sizeof(to))==-1) {
			if (++icmp_errs<=ICMP_MAX_ERRS) {
				log_warn("icmp ping: sendto() failed: %s.",strerror(errno));
			}
			return -1;
		}
		/* listen for reply. */
		tm=time(NULL); tpassed=0;
		do {
			int psres;
#ifdef NO_POLL
			fd_set fds,fdse;
			struct timeval tv;
			FD_ZERO(&fds);
			PDNSD_ASSERT(isock<FD_SETSIZE,"socket file descriptor exceeds FD_SETSIZE.");
			FD_SET(isock, &fds);
			fdse=fds;
			tv.tv_usec=0;
			tv.tv_sec=timeout>tpassed?timeout-tpassed:0;
			/* There is a possible race condition with the arrival of a signal here,
			   but it is so unlikely to be a problem in practice that the effort
			   to do this properly is not worth the trouble.
			*/
			if(is_interrupted_servstat_thread()) {
				DEBUG_MSG("server status thread interrupted.\n");
				return -1;
			}
			psres=select(isock+1,&fds,NULL,&fdse,&tv);
#else
			struct pollfd pfd;
			pfd.fd=isock;
			pfd.events=POLLIN;
			/* There is a possible race condition with the arrival of a signal here,
			   but it is so unlikely to be a problem in practice that the effort
			   to do this properly is not worth the trouble.
			*/
			if(is_interrupted_servstat_thread()) {
				DEBUG_MSG("server status thread interrupted.\n");
				return -1;
			}
			psres=poll(&pfd,1,timeout>tpassed?(timeout-tpassed)*1000:0);
#endif

			if (psres<0) {
				if(errno==EINTR && is_interrupted_servstat_thread()) {
					DEBUG_MSG("poll/select interrupted in server status thread.\n");
				}
				else if (++icmp_errs<=ICMP_MAX_ERRS) {
					log_warn("poll/select failed: %s",strerror(errno));
				}
				return -1;
			}
			if (psres==0)  /* timed out */
				break;

#ifdef NO_POLL
			if (FD_ISSET(isock,&fds) || FD_ISSET(isock,&fdse))
#else
			if (pfd.revents&(POLLIN|POLLERR))
#endif
			{
				char buf[1024];
				socklen_t sl=sizeof(from);
				int len;

				if ((len=recvfrom(isock,&buf,sizeof(buf),0,(struct sockaddr *)&from,&sl))!=-1) {
					if (len>sizeof(struct iphdr)) {
						struct iphdr iph;

						memcpy(&iph, buf, sizeof(iph));
						if (len-iph.ip_hl*4>=ICMP_BASEHDR_LEN) {
							struct icmphdr icmpp;

							memcpy(&icmpp, ((uint32_t *)buf)+iph.ip_hl, sizeof(icmpp));
							if (iph.ip_saddr==addr.s_addr && icmpp.icmp_type==ICMP_ECHOREPLY &&
							    ntohs(icmpp.icmp_id)==id && ntohs(icmpp.icmp_seq)<=i) {
								return (i-ntohs(icmpp.icmp_seq))*timeout+(time(NULL)-tm); /* return the number of ticks */
							} else {
								/* No regular echo reply. Maybe an error? */
								if (icmp4_errcmp((char *)&icmpd, ICMP4_ECHO_LEN, &to.sin_addr, buf, len, ICMP_DEST_UNREACH) ||
								    icmp4_errcmp((char *)&icmpd, ICMP4_ECHO_LEN, &to.sin_addr, buf, len, ICMP_TIME_EXCEEDED)) {
									return -1;
								}
							}
						}
					}
				} else {
					return -1; /* error */
				}
			}
			else {
				if (++icmp_errs<=ICMP_MAX_ERRS) {
					log_error("Unhandled poll/select event in ping4() at %s, line %d.",__FILE__,__LINE__);
				}
				return -1;
			}
			tpassed=time(NULL)-tm;
		} while (tpassed<timeout);
	}
	return -1; /* no answer */
}


#ifdef ENABLE_IPV6

/* Takes a packet as send out and a received ICMPv6 packet and looks whether the ICMPv6 packet is
 * an error reply on the sent-out one. packet is only the packet (without IPv6 header).
 * errmsg does not include an IPv6 header. to is the address the sent packet went to.
 * This is specialized for icmpv6: It zeros out the checksum field, which is filled in
 * by the kernel, and expects that the checksum field in the sent-out packet is zeroed out too
 * We need a little magic to parse the answer, as there could be extension headers present, end
 * we don't know their length a priori.*/
static int icmp6_errcmp(char *packet, int plen, struct in6_addr *to, char *errmsg, int elen, int errtype)
{
	struct icmp6_hdr icmph;
	struct ip6_hdr eiph;
	struct ip6_hbh hbh;
	char *data;
	int rlen,nxt;

	/* XXX: lots of memcpy here to avoid unaligned access faults on alpha */
	if (elen<sizeof(icmph)+sizeof(eiph))
		return 0;
	memcpy(&icmph,errmsg,sizeof(icmph));
	memcpy(&eiph,errmsg+sizeof(icmph),sizeof(eiph));
	if (!IN6_ARE_ADDR_EQUAL(&eiph.ip6_dst, to))
		return 0;
	rlen=elen-sizeof(icmph)-sizeof(eiph);
	data=errmsg+sizeof(icmph)+sizeof(eiph);
	nxt=eiph.ip6_nxt;
	/* Now, jump over any known option header that might be present, and then
	 * try to compare the packets. */
	while (nxt!=IPPROTO_ICMPV6) {
		/* Those are the headers we understand. */
		if (nxt!=IPPROTO_HOPOPTS && nxt!=IPPROTO_ROUTING && nxt!=IPPROTO_DSTOPTS)
			return 0;
		if (rlen<sizeof(hbh))
			return 0;
		memcpy(&hbh,data,sizeof(hbh));
		if (rlen<hbh.ip6h_len)
			return 0;
		rlen-=hbh.ip6h_len;
		nxt=hbh.ip6h_nxt;
		data+=hbh.ip6h_len;
	}
	if (rlen<sizeof(struct icmp6_hdr))
		return 0;
	/* Zero out the checksum of the enclosed ICMPv6 header, it is kernel-filled in the original data */
	memset(((char *)data)+offsetof(struct icmp6_hdr,icmp6_cksum),0,sizeof(icmph.icmp6_cksum));
	return icmph.icmp6_type==errtype && memcmp(data, packet, plen<rlen?plen:rlen)==0;
}

/* IPv6/ICMPv6 ping. Called from ping (see below) */
static int ping6(struct in6_addr a, int timeout, int rep)
{
	int i;
/*	int ck_offs=2;*/
	int isock;
	struct icmp6_filter f;
	unsigned short id=(unsigned short)(rand()&0xffff); /* randomize a ping id */

	isock=ping6_isocket;

	ICMP6_FILTER_SETBLOCKALL(&f);
	ICMP6_FILTER_SETPASS(ICMP6_ECHO_REPLY,&f);
	ICMP6_FILTER_SETPASS(ICMP6_DST_UNREACH,&f);
	ICMP6_FILTER_SETPASS(ICMP6_TIME_EXCEEDED,&f);

	if (setsockopt(isock,IPPROTO_ICMPV6,ICMP6_FILTER,&f,sizeof(f))==-1) {
		if (++icmp_errs<=ICMP_MAX_ERRS) {
			log_warn("icmpv6 ping: setsockopt() failed: %s", strerror(errno));
		}
		return -1;
	}

	for (i=0;i<rep;i++) {
		struct sockaddr_in6 from;
		struct icmp6_hdr icmpd;
		long tm,tpassed;

		icmpd.icmp6_type=ICMP6_ECHO_REQUEST;
		icmpd.icmp6_code=0;
		icmpd.icmp6_cksum=0; /* The friendly kernel does fill that in for us. */
		icmpd.icmp6_id=htons((uint16_t)id);
		icmpd.icmp6_seq=htons((uint16_t)i);

		memset(&from,0,sizeof(from));
		from.sin6_family=AF_INET6;
		from.sin6_flowinfo=IPV6_FLOWINFO;
		from.sin6_port=0;
		from.sin6_addr=a;
		SET_SOCKA_LEN6(from);
		if (sendto(isock,&icmpd,sizeof(icmpd),0,(struct sockaddr *)&from,sizeof(from))==-1) {
			if (++icmp_errs<=ICMP_MAX_ERRS) {
				log_warn("icmpv6 ping: sendto() failed: %s.",strerror(errno));
			}
			return -1;
		}
		/* listen for reply. */
		tm=time(NULL); tpassed=0;
		do {
			int psres;
#ifdef NO_POLL
			fd_set fds,fdse;
			struct timeval tv;
			FD_ZERO(&fds);
			PDNSD_ASSERT(isock<FD_SETSIZE,"socket file descriptor exceeds FD_SETSIZE.");
			FD_SET(isock, &fds);
			fdse=fds;
			tv.tv_usec=0;
			tv.tv_sec=timeout>tpassed?timeout-tpassed:0;
			/* There is a possible race condition with the arrival of a signal here,
			   but it is so unlikely to be a problem in practice that the effort
			   to do this properly is not worth the trouble.
			*/
			if(is_interrupted_servstat_thread()) {
				DEBUG_MSG("server status thread interrupted.\n");
				return -1;
			}
			psres=select(isock+1,&fds,NULL,&fdse,&tv);
#else
			struct pollfd pfd;
			pfd.fd=isock;
			pfd.events=POLLIN;
			/* There is a possible race condition with the arrival of a signal here,
			   but it is so unlikely to be a problem in practice that the effort
			   to do this properly is not worth the trouble.
			*/
			if(is_interrupted_servstat_thread()) {
				DEBUG_MSG("server status thread interrupted.\n");
				return -1;
			}
			psres=poll(&pfd,1,timeout>tpassed?(timeout-tpassed)*1000:0);
#endif

			if (psres<0) {
				if(errno==EINTR && is_interrupted_servstat_thread()) {
					DEBUG_MSG("poll/select interrupted in server status thread.\n");
				}
				else if (++icmp_errs<=ICMP_MAX_ERRS) {
					log_warn("poll/select failed: %s",strerror(errno));
				}
				return -1;
			}
			if (psres==0)  /* timed out */
				break;

#ifdef NO_POLL
			if (FD_ISSET(isock,&fds) || FD_ISSET(isock,&fdse))
#else
			if (pfd.revents&(POLLIN|POLLERR))
#endif
			{
				char buf[1024];
				socklen_t sl=sizeof(from);
				int len;
				if ((len=recvfrom(isock,&buf,sizeof(buf),0,(struct sockaddr *)&from,&sl))!=-1) {
					if (len>=sizeof(struct icmp6_hdr)) {
						/* we get packets without IPv6 header, luckily */
						struct icmp6_hdr icmpp;

						memcpy(&icmpp, buf, sizeof(icmpp));
						if (IN6_ARE_ADDR_EQUAL(&from.sin6_addr,&a) &&
						    ntohs(icmpp.icmp6_id)==id && ntohs(icmpp.icmp6_seq)<=i) {
							return (i-ntohs(icmpp.icmp6_seq))*timeout+(time(NULL)-tm); /* return the number of ticks */
						} else {
							/* No regular echo reply. Maybe an error? */
							if (icmp6_errcmp((char *)&icmpd, sizeof(icmpd), &from.sin6_addr, buf, len, ICMP6_DST_UNREACH) ||
							    icmp6_errcmp((char *)&icmpd, sizeof(icmpd), &from.sin6_addr, buf, len, ICMP6_TIME_EXCEEDED)) {
								return -1;
							}
						}
					}
				} else {
					return -1; /* error */
				}
			}
			else {
				if (++icmp_errs<=ICMP_MAX_ERRS) {
					log_error("Unhandled poll/select event in ping6() at %s, line %d.",__FILE__,__LINE__);
				}
				return -1;
			}
			tpassed=time(NULL)-tm;
		} while (tpassed<timeout);
	}
	return -1; /* no answer */
}
#endif /* ENABLE_IPV6*/


/* Perform an icmp ping on a host, returning -1 on timeout or
 * "host unreachable" or the ping time in 10ths of secs
 * (but actually, we are not that accurate any more).
 * timeout in 10ths of seconds, rep is the repetition count
 */
int ping(pdnsd_a *addr, int timeout, int rep)
{

	if (SEL_IPVER(ping_isocket,ping6_isocket) == -1)
		return -1;

	/* We were given a timeout in 10ths of seconds,
	   but ping4 and ping6 want a timeout in seconds. */
	timeout /= 10;

#ifdef ENABLE_IPV4
	if (run_ipv4)
		return ping4(addr->ipv4,timeout,rep);
#endif
#ifdef ENABLE_IPV6
	ELSE_IPV6 {
		/* If it is a IPv4 mapped IPv6 address, we prefer ICMPv4. */
		if (ping_isocket!=-1 && IN6_IS_ADDR_V4MAPPED(&addr->ipv6)) {
			struct in_addr v4;
			v4.s_addr=((uint32_t *)&addr->ipv6)[3];
			return ping4(v4,timeout,rep);
		} else
			return ping6(addr->ipv6,timeout,rep);
	}
#endif
	return -1;
}

#else
# error "Huh! No OS macro defined!"
#endif /*(TARGET==TARGET_LINUX) || (TARGET==TARGET_BSD) || (TARGET==TARGET_CYGWIN)*/
