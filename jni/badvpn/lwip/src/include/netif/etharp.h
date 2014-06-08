/*
 * Copyright (c) 2001-2003 Swedish Institute of Computer Science.
 * Copyright (c) 2003-2004 Leon Woestenberg <leon.woestenberg@axon.tv>
 * Copyright (c) 2003-2004 Axon Digital Design B.V., The Netherlands.
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

#ifndef __NETIF_ETHARP_H__
#define __NETIF_ETHARP_H__

#include "lwip/opt.h"

#if LWIP_ARP || LWIP_ETHERNET /* don't build if not configured for use in lwipopts.h */

#include "lwip/pbuf.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/ip.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ETHARP_HWADDR_LEN
#define ETHARP_HWADDR_LEN     6
#endif

#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/bpstruct.h"
#endif
PACK_STRUCT_BEGIN
struct eth_addr {
  PACK_STRUCT_FIELD(u8_t addr[ETHARP_HWADDR_LEN]);
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/epstruct.h"
#endif

#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/bpstruct.h"
#endif
PACK_STRUCT_BEGIN
/** Ethernet header */
struct eth_hdr {
#if ETH_PAD_SIZE
  PACK_STRUCT_FIELD(u8_t padding[ETH_PAD_SIZE]);
#endif
  PACK_STRUCT_FIELD(struct eth_addr dest);
  PACK_STRUCT_FIELD(struct eth_addr src);
  PACK_STRUCT_FIELD(u16_t type);
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/epstruct.h"
#endif

#define SIZEOF_ETH_HDR (14 + ETH_PAD_SIZE)

#if ETHARP_SUPPORT_VLAN

#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/bpstruct.h"
#endif
PACK_STRUCT_BEGIN
/** VLAN header inserted between ethernet header and payload
 * if 'type' in ethernet header is ETHTYPE_VLAN.
 * See IEEE802.Q */
struct eth_vlan_hdr {
  PACK_STRUCT_FIELD(u16_t prio_vid);
  PACK_STRUCT_FIELD(u16_t tpid);
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/epstruct.h"
#endif

#define SIZEOF_VLAN_HDR 4
#define VLAN_ID(vlan_hdr) (htons((vlan_hdr)->prio_vid) & 0xFFF)

#endif /* ETHARP_SUPPORT_VLAN */

#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/bpstruct.h"
#endif
PACK_STRUCT_BEGIN
/** the ARP message, see RFC 826 ("Packet format") */
struct etharp_hdr {
  PACK_STRUCT_FIELD(u16_t hwtype);
  PACK_STRUCT_FIELD(u16_t proto);
  PACK_STRUCT_FIELD(u8_t  hwlen);
  PACK_STRUCT_FIELD(u8_t  protolen);
  PACK_STRUCT_FIELD(u16_t opcode);
  PACK_STRUCT_FIELD(struct eth_addr shwaddr);
  PACK_STRUCT_FIELD(struct ip_addr2 sipaddr);
  PACK_STRUCT_FIELD(struct eth_addr dhwaddr);
  PACK_STRUCT_FIELD(struct ip_addr2 dipaddr);
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/epstruct.h"
#endif

#define SIZEOF_ETHARP_HDR 28
#define SIZEOF_ETHARP_PACKET (SIZEOF_ETH_HDR + SIZEOF_ETHARP_HDR)

/** 5 seconds period */
#define ARP_TMR_INTERVAL 5000

#define ETHTYPE_ARP       0x0806U
#define ETHTYPE_IP        0x0800U
#define ETHTYPE_VLAN      0x8100U
#define ETHTYPE_IPV6      0x86DDU
#define ETHTYPE_PPPOEDISC 0x8863U  /* PPP Over Ethernet Discovery Stage */
#define ETHTYPE_PPPOE     0x8864U  /* PPP Over Ethernet Session Stage */

/** MEMCPY-like macro to copy to/from struct eth_addr's that are local variables
 * or known to be 32-bit aligned within the protocol header. */
#ifndef ETHADDR32_COPY
#define ETHADDR32_COPY(src, dst)  SMEMCPY(src, dst, ETHARP_HWADDR_LEN)
#endif

/** MEMCPY-like macro to copy to/from struct eth_addr's that are no local
 * variables and known to be 16-bit aligned within the protocol header. */
#ifndef ETHADDR16_COPY
#define ETHADDR16_COPY(src, dst)  SMEMCPY(src, dst, ETHARP_HWADDR_LEN)
#endif

#if LWIP_ARP /* don't build if not configured for use in lwipopts.h */

/** ARP message types (opcodes) */
#define ARP_REQUEST 1
#define ARP_REPLY   2

/** Define this to 1 and define LWIP_ARP_FILTER_NETIF_FN(pbuf, netif, type)
 * to a filter function that returns the correct netif when using multiple
 * netifs on one hardware interface where the netif's low-level receive
 * routine cannot decide for the correct netif (e.g. when mapping multiple
 * IP addresses to one hardware interface).
 */
#ifndef LWIP_ARP_FILTER_NETIF
#define LWIP_ARP_FILTER_NETIF 0
#endif

#if ARP_QUEUEING
/** struct for queueing outgoing packets for unknown address
  * defined here to be accessed by memp.h
  */
struct etharp_q_entry {
  struct etharp_q_entry *next;
  struct pbuf *p;
};
#endif /* ARP_QUEUEING */

#define etharp_init() /* Compatibility define, not init needed. */
void etharp_tmr(void);
s8_t etharp_find_addr(struct netif *netif, ip_addr_t *ipaddr,
         struct eth_addr **eth_ret, ip_addr_t **ip_ret);
err_t etharp_output(struct netif *netif, struct pbuf *q, ip_addr_t *ipaddr);
err_t etharp_query(struct netif *netif, ip_addr_t *ipaddr, struct pbuf *q);
err_t etharp_request(struct netif *netif, ip_addr_t *ipaddr);
/** For Ethernet network interfaces, we might want to send "gratuitous ARP";
 *  this is an ARP packet sent by a node in order to spontaneously cause other
 *  nodes to update an entry in their ARP cache.
 *  From RFC 3220 "IP Mobility Support for IPv4" section 4.6. */
#define etharp_gratuitous(netif) etharp_request((netif), &(netif)->ip_addr)
void etharp_cleanup_netif(struct netif *netif);

#if ETHARP_SUPPORT_STATIC_ENTRIES
err_t etharp_add_static_entry(ip_addr_t *ipaddr, struct eth_addr *ethaddr);
err_t etharp_remove_static_entry(ip_addr_t *ipaddr);
#endif /* ETHARP_SUPPORT_STATIC_ENTRIES */

#if LWIP_AUTOIP
err_t etharp_raw(struct netif *netif, const struct eth_addr *ethsrc_addr,
                 const struct eth_addr *ethdst_addr,
                 const struct eth_addr *hwsrc_addr, const ip_addr_t *ipsrc_addr,
                 const struct eth_addr *hwdst_addr, const ip_addr_t *ipdst_addr,
                 const u16_t opcode);
#endif /* LWIP_AUTOIP */

#endif /* LWIP_ARP */

err_t ethernet_input(struct pbuf *p, struct netif *netif);

#define eth_addr_cmp(addr1, addr2) (memcmp((addr1)->addr, (addr2)->addr, ETHARP_HWADDR_LEN) == 0)

extern const struct eth_addr ethbroadcast, ethzero;

#ifdef __cplusplus
}
#endif

#endif /* LWIP_ARP || LWIP_ETHERNET */

#endif /* __NETIF_ARP_H__ */
