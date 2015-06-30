/**
 * lwip DNS resolver header file.

 * Author: Jim Pettinato 
 *   April 2007

 * ported from uIP resolv.c Copyright (c) 2002-2003, Adam Dunkels.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __LWIP_DNS_H__
#define __LWIP_DNS_H__

#include "lwip/opt.h"

#if LWIP_DNS /* don't build if not configured for use in lwipopts.h */

#ifdef __cplusplus
extern "C" {
#endif

/** DNS timer period */
#define DNS_TMR_INTERVAL          1000

/** DNS field TYPE used for "Resource Records" */
#define DNS_RRTYPE_A              1     /* a host address */
#define DNS_RRTYPE_NS             2     /* an authoritative name server */
#define DNS_RRTYPE_MD             3     /* a mail destination (Obsolete - use MX) */
#define DNS_RRTYPE_MF             4     /* a mail forwarder (Obsolete - use MX) */
#define DNS_RRTYPE_CNAME          5     /* the canonical name for an alias */
#define DNS_RRTYPE_SOA            6     /* marks the start of a zone of authority */
#define DNS_RRTYPE_MB             7     /* a mailbox domain name (EXPERIMENTAL) */
#define DNS_RRTYPE_MG             8     /* a mail group member (EXPERIMENTAL) */
#define DNS_RRTYPE_MR             9     /* a mail rename domain name (EXPERIMENTAL) */
#define DNS_RRTYPE_NULL           10    /* a null RR (EXPERIMENTAL) */
#define DNS_RRTYPE_WKS            11    /* a well known service description */
#define DNS_RRTYPE_PTR            12    /* a domain name pointer */
#define DNS_RRTYPE_HINFO          13    /* host information */
#define DNS_RRTYPE_MINFO          14    /* mailbox or mail list information */
#define DNS_RRTYPE_MX             15    /* mail exchange */
#define DNS_RRTYPE_TXT            16    /* text strings */

/** DNS field CLASS used for "Resource Records" */
#define DNS_RRCLASS_IN            1     /* the Internet */
#define DNS_RRCLASS_CS            2     /* the CSNET class (Obsolete - used only for examples in some obsolete RFCs) */
#define DNS_RRCLASS_CH            3     /* the CHAOS class */
#define DNS_RRCLASS_HS            4     /* Hesiod [Dyer 87] */
#define DNS_RRCLASS_FLUSH         0x800 /* Flush bit */

/* The size used for the next line is rather a hack, but it prevents including socket.h in all files
   that include memp.h, and that would possibly break portability (since socket.h defines some types
   and constants possibly already define by the OS).
   Calculation rule:
   sizeof(struct addrinfo) + sizeof(struct sockaddr_in) + DNS_MAX_NAME_LENGTH + 1 byte zero-termination */
#define NETDB_ELEM_SIZE           (32 + 16 + DNS_MAX_NAME_LENGTH + 1)

#if DNS_LOCAL_HOSTLIST
/** struct used for local host-list */
struct local_hostlist_entry {
  /** static hostname */
  const char *name;
  /** static host address in network byteorder */
  ip_addr_t addr;
  struct local_hostlist_entry *next;
};
#if DNS_LOCAL_HOSTLIST_IS_DYNAMIC
#ifndef DNS_LOCAL_HOSTLIST_MAX_NAMELEN
#define DNS_LOCAL_HOSTLIST_MAX_NAMELEN  DNS_MAX_NAME_LENGTH
#endif
#define LOCALHOSTLIST_ELEM_SIZE ((sizeof(struct local_hostlist_entry) + DNS_LOCAL_HOSTLIST_MAX_NAMELEN + 1))
#endif /* DNS_LOCAL_HOSTLIST_IS_DYNAMIC */
#endif /* DNS_LOCAL_HOSTLIST */

/** Callback which is invoked when a hostname is found.
 * A function of this type must be implemented by the application using the DNS resolver.
 * @param name pointer to the name that was looked up.
 * @param ipaddr pointer to an ip_addr_t containing the IP address of the hostname,
 *        or NULL if the name could not be found (or on any other error).
 * @param callback_arg a user-specified callback argument passed to dns_gethostbyname
*/
typedef void (*dns_found_callback)(const char *name, ip_addr_t *ipaddr, void *callback_arg);

void           dns_init(void);
void           dns_tmr(void);
void           dns_setserver(u8_t numdns, ip_addr_t *dnsserver);
ip_addr_t      dns_getserver(u8_t numdns);
err_t          dns_gethostbyname(const char *hostname, ip_addr_t *addr,
                                 dns_found_callback found, void *callback_arg);

#if DNS_LOCAL_HOSTLIST && DNS_LOCAL_HOSTLIST_IS_DYNAMIC
int            dns_local_removehost(const char *hostname, const ip_addr_t *addr);
err_t          dns_local_addhost(const char *hostname, const ip_addr_t *addr);
#endif /* DNS_LOCAL_HOSTLIST && DNS_LOCAL_HOSTLIST_IS_DYNAMIC */

#ifdef __cplusplus
}
#endif

#endif /* LWIP_DNS */

#endif /* __LWIP_DNS_H__ */
