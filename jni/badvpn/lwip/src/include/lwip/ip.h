/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Author: Adam Dunkels <adam@sics.se>
 *
 */
#ifndef __LWIP_IP_H__
#define __LWIP_IP_H__

#include "lwip/opt.h"

#include "lwip/def.h"
#include "lwip/pbuf.h"
#include "lwip/ip_addr.h"
#include "lwip/err.h"
#include "lwip/netif.h"
#include "lwip/ip4.h"
#include "lwip/ip6.h"

#ifdef __cplusplus
extern "C" {
#endif

/* This is passed as the destination address to ip_output_if (not
   to ip_output), meaning that an IP header already is constructed
   in the pbuf. This is used when TCP retransmits. */
#ifdef IP_HDRINCL
#undef IP_HDRINCL
#endif /* IP_HDRINCL */
#define IP_HDRINCL  NULL

#if LWIP_NETIF_HWADDRHINT
#define IP_PCB_ADDRHINT ;u8_t addr_hint
#else
#define IP_PCB_ADDRHINT
#endif /* LWIP_NETIF_HWADDRHINT */

#if LWIP_IPV6
#define IP_PCB_ISIPV6_MEMBER  u8_t isipv6;
#define IP_PCB_IPVER_EQ(pcb1, pcb2)   ((pcb1)->isipv6 == (pcb2)->isipv6)
#define IP_PCB_IPVER_INPUT_MATCH(pcb) (ip_current_is_v6() ? \
                                       ((pcb)->isipv6 != 0) : \
                                       ((pcb)->isipv6 == 0))
#define PCB_ISIPV6(pcb) ((pcb)->isipv6)
#else
#define IP_PCB_ISIPV6_MEMBER
#define IP_PCB_IPVER_EQ(pcb1, pcb2)   1
#define IP_PCB_IPVER_INPUT_MATCH(pcb) 1
#define PCB_ISIPV6(pcb)            0
#endif /* LWIP_IPV6 */

/* This is the common part of all PCB types. It needs to be at the
   beginning of a PCB type definition. It is located here so that
   changes to this common part are made in one location instead of
   having to change all PCB structs. */
#define IP_PCB \
  IP_PCB_ISIPV6_MEMBER \
  /* ip addresses in network byte order */ \
  ipX_addr_t local_ip; \
  ipX_addr_t remote_ip; \
   /* Socket options */  \
  u8_t so_options;      \
   /* Type Of Service */ \
  u8_t tos;              \
  /* Time To Live */     \
  u8_t ttl               \
  /* link layer address resolution hint */ \
  IP_PCB_ADDRHINT

struct ip_pcb {
/* Common members of all PCB types */
  IP_PCB;
};

/*
 * Option flags per-socket. These are the same like SO_XXX.
 */
/*#define SOF_DEBUG       0x01U     Unimplemented: turn on debugging info recording */
#define SOF_ACCEPTCONN    0x02U  /* socket has had listen() */
#define SOF_REUSEADDR     0x04U  /* allow local address reuse */
#define SOF_KEEPALIVE     0x08U  /* keep connections alive */
/*#define SOF_DONTROUTE   0x10U     Unimplemented: just use interface addresses */
#define SOF_BROADCAST     0x20U  /* permit to send and to receive broadcast messages (see IP_SOF_BROADCAST option) */
/*#define SOF_USELOOPBACK 0x40U     Unimplemented: bypass hardware when possible */
#define SOF_LINGER        0x80U  /* linger on close if data present */
/*#define SOF_OOBINLINE   0x0100U     Unimplemented: leave received OOB data in line */
/*#define SOF_REUSEPORT   0x0200U     Unimplemented: allow local address & port reuse */

/* These flags are inherited (e.g. from a listen-pcb to a connection-pcb): */
#define SOF_INHERITED   (SOF_REUSEADDR|SOF_KEEPALIVE|SOF_LINGER/*|SOF_DEBUG|SOF_DONTROUTE|SOF_OOBINLINE*/)

/* Global variables of this module, kept in a struct for efficient access using base+index. */
struct ip_globals
{
  /** The interface that provided the packet for the current callback invocation. */
  struct netif *current_netif;
  /** Header of the input packet currently being processed. */
  const struct ip_hdr *current_ip4_header;
#if LWIP_IPV6
  /** Header of the input IPv6 packet currently being processed. */
  const struct ip6_hdr *current_ip6_header;
#endif /* LWIP_IPV6 */
  /** Total header length of current_ip4/6_header (i.e. after this, the UDP/TCP header starts) */
  u16_t current_ip_header_tot_len;
  /** Source IP address of current_header */
  ipX_addr_t current_iphdr_src;
  /** Destination IP address of current_header */
  ipX_addr_t current_iphdr_dest;
};
extern struct ip_globals ip_data;


/** Get the interface that received the current packet.
 * This function must only be called from a receive callback (udp_recv,
 * raw_recv, tcp_accept). It will return NULL otherwise. */
#define ip_current_netif()      (ip_data.current_netif)
/** Get the IP header of the current packet.
 * This function must only be called from a receive callback (udp_recv,
 * raw_recv, tcp_accept). It will return NULL otherwise. */
#define ip_current_header()     (ip_data.current_ip4_header)
/** Total header length of ip(6)_current_header() (i.e. after this, the UDP/TCP header starts) */
#define ip_current_header_tot_len() (ip_data.current_ip_header_tot_len)
/** Source IP address of current_header */
#define ipX_current_src_addr()   (&ip_data.current_iphdr_src)
/** Destination IP address of current_header */
#define ipX_current_dest_addr()  (&ip_data.current_iphdr_dest)

#if LWIP_IPV6
/** Get the IPv6 header of the current packet.
 * This function must only be called from a receive callback (udp_recv,
 * raw_recv, tcp_accept). It will return NULL otherwise. */
#define ip6_current_header()      (ip_data.current_ip6_header)
/** Returns TRUE if the current IP input packet is IPv6, FALSE if it is IPv4 */
#define ip_current_is_v6()        (ip6_current_header() != NULL)
/** Source IPv6 address of current_header */
#define ip6_current_src_addr()    (ipX_2_ip6(&ip_data.current_iphdr_src))
/** Destination IPv6 address of current_header */
#define ip6_current_dest_addr()   (ipX_2_ip6(&ip_data.current_iphdr_dest))
/** Get the transport layer protocol */
#define ip_current_header_proto() (ip_current_is_v6() ? \
                                   IP6H_NEXTH(ip6_current_header()) :\
                                   IPH_PROTO(ip_current_header()))
/** Get the transport layer header */
#define ipX_next_header_ptr()     ((void*)((ip_current_is_v6() ? \
  (u8_t*)ip6_current_header() : (u8_t*)ip_current_header())  + ip_current_header_tot_len()))

/** Set an IP_PCB to IPv6 (IPv4 is the default) */
#define ip_set_v6(pcb, val)       do{if(pcb != NULL) { pcb->isipv6 = val; }}while(0)

/** Source IP4 address of current_header */
#define ip_current_src_addr()     (ipX_2_ip(&ip_data.current_iphdr_src))
/** Destination IP4 address of current_header */
#define ip_current_dest_addr()    (ipX_2_ip(&ip_data.current_iphdr_dest))

#else /* LWIP_IPV6 */

/** Always returns FALSE when only supporting IPv4 */
#define ip_current_is_v6()        0
/** Get the transport layer protocol */
#define ip_current_header_proto() IPH_PROTO(ip_current_header())
/** Get the transport layer header */
#define ipX_next_header_ptr()     ((void*)((u8_t*)ip_current_header() + ip_current_header_tot_len()))
/** Source IP4 address of current_header */
#define ip_current_src_addr()     (&ip_data.current_iphdr_src)
/** Destination IP4 address of current_header */
#define ip_current_dest_addr()    (&ip_data.current_iphdr_dest)

#endif /* LWIP_IPV6 */

/** Union source address of current_header */
#define ipX_current_src_addr()    (&ip_data.current_iphdr_src)
/** Union destination address of current_header */
#define ipX_current_dest_addr()   (&ip_data.current_iphdr_dest)

/** Gets an IP pcb option (SOF_* flags) */
#define ip_get_option(pcb, opt)   ((pcb)->so_options & (opt))
/** Sets an IP pcb option (SOF_* flags) */
#define ip_set_option(pcb, opt)   ((pcb)->so_options |= (opt))
/** Resets an IP pcb option (SOF_* flags) */
#define ip_reset_option(pcb, opt) ((pcb)->so_options &= ~(opt))

#if LWIP_IPV6
#define ipX_output(isipv6, p, src, dest, ttl, tos, proto) \
        ((isipv6) ? \
        ip6_output(p, ipX_2_ip6(src), ipX_2_ip6(dest), ttl, tos, proto) : \
        ip_output(p, ipX_2_ip(src), ipX_2_ip(dest), ttl, tos, proto))
#define ipX_output_if(isipv6, p, src, dest, ttl, tos, proto, netif) \
        ((isipv6) ? \
        ip6_output_if(p, ip_2_ip6(src), ip_2_ip6(dest), ttl, tos, proto, netif) : \
        ip_output_if(p, (src), (dest), ttl, tos, proto, netif))
#define ipX_output_hinted(isipv6, p, src, dest, ttl, tos, proto, addr_hint) \
        ((isipv6) ? \
        ip6_output_hinted(p, ipX_2_ip6(src), ipX_2_ip6(dest), ttl, tos, proto, addr_hint) : \
        ip_output_hinted(p, ipX_2_ip(src), ipX_2_ip(dest), ttl, tos, proto, addr_hint))
#define ipX_route(isipv6, src, dest) \
        ((isipv6) ? \
        ip6_route(ipX_2_ip6(src), ipX_2_ip6(dest)) : \
        ip_route(ipX_2_ip(dest)))
#define ipX_netif_get_local_ipX(isipv6, netif, dest) \
        ((isipv6) ? \
        ip6_netif_get_local_ipX(netif, ipX_2_ip6(dest)) : \
        ip_netif_get_local_ipX(netif))
#define ipX_debug_print(is_ipv6, p) ((is_ipv6) ? ip6_debug_print(p) : ip_debug_print(p))
#else /* LWIP_IPV6 */
#define ipX_output(isipv6, p, src, dest, ttl, tos, proto) \
        ip_output(p, src, dest, ttl, tos, proto)
#define ipX_output_if(isipv6, p, src, dest, ttl, tos, proto, netif) \
        ip_output_if(p, src, dest, ttl, tos, proto, netif)
#define ipX_output_hinted(isipv6, p, src, dest, ttl, tos, proto, addr_hint) \
        ip_output_hinted(p, src, dest, ttl, tos, proto, addr_hint)
#define ipX_route(isipv6, src, dest) \
        ip_route(ipX_2_ip(dest))
#define ipX_netif_get_local_ipX(isipv6, netif, dest) \
        ip_netif_get_local_ipX(netif)
#define ipX_debug_print(is_ipv6, p) ip_debug_print(p)
#endif /* LWIP_IPV6 */

#define ipX_route_get_local_ipX(isipv6, src, dest, netif, ipXaddr) do { \
  (netif) = ipX_route(isipv6, src, dest); \
  (ipXaddr) = ipX_netif_get_local_ipX(isipv6, netif, dest); \
}while(0)

#ifdef __cplusplus
}
#endif

#endif /* __LWIP_IP_H__ */


