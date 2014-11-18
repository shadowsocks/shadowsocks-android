/**
 * @file
 *
 * Ethernet output for IPv6. Uses ND tables for link-layer addressing.
 */

/*
 * Copyright (c) 2010 Inico Technologies Ltd.
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
 * Author: Ivan Delamer <delamer@inicotech.com>
 *
 *
 * Please coordinate changes and requests with Ivan Delamer
 * <delamer@inicotech.com>
 */

#include "lwip/opt.h"

#if LWIP_IPV6 && LWIP_ETHERNET

#include "lwip/ethip6.h"
#include "lwip/nd6.h"
#include "lwip/pbuf.h"
#include "lwip/ip6.h"
#include "lwip/ip6_addr.h"
#include "lwip/inet_chksum.h"
#include "lwip/netif.h"
#include "lwip/icmp6.h"

#include <string.h>

#define ETHTYPE_IPV6        0x86DD

/** The ethernet address */
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/bpstruct.h"
#endif
PACK_STRUCT_BEGIN
struct eth_addr {
  PACK_STRUCT_FIELD(u8_t addr[6]);
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/epstruct.h"
#endif

/** Ethernet header */
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/bpstruct.h"
#endif
PACK_STRUCT_BEGIN
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

/**
 * Send an IPv6 packet on the network using netif->linkoutput
 * The ethernet header is filled in before sending.
 *
 * @params netif the lwIP network interface on which to send the packet
 * @params p the packet to send, p->payload pointing to the (uninitialized) ethernet header
 * @params src the source MAC address to be copied into the ethernet header
 * @params dst the destination MAC address to be copied into the ethernet header
 * @return ERR_OK if the packet was sent, any other err_t on failure
 */
static err_t
ethip6_send(struct netif *netif, struct pbuf *p, struct eth_addr *src, struct eth_addr *dst)
{
  struct eth_hdr *ethhdr = (struct eth_hdr *)p->payload;

  LWIP_ASSERT("netif->hwaddr_len must be 6 for ethip6!",
              (netif->hwaddr_len == 6));
  SMEMCPY(&ethhdr->dest, dst, 6);
  SMEMCPY(&ethhdr->src, src, 6);
  ethhdr->type = PP_HTONS(ETHTYPE_IPV6);
  LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE, ("ethip6_send: sending packet %p\n", (void *)p));
  /* send the packet */
  return netif->linkoutput(netif, p);
}

/**
 * Resolve and fill-in Ethernet address header for outgoing IPv6 packet.
 *
 * For IPv6 multicast, corresponding Ethernet addresses
 * are selected and the packet is transmitted on the link.
 *
 * For unicast addresses, ...
 *
 * @TODO anycast addresses
 *
 * @param netif The lwIP network interface which the IP packet will be sent on.
 * @param q The pbuf(s) containing the IP packet to be sent.
 * @param ip6addr The IP address of the packet destination.
 *
 * @return
 * - ERR_RTE No route to destination (no gateway to external networks),
 * or the return type of either etharp_query() or etharp_send_ip().
 */
err_t
ethip6_output(struct netif *netif, struct pbuf *q, ip6_addr_t *ip6addr)
{
  struct eth_addr dest;
  s8_t i;

  /* make room for Ethernet header - should not fail */
  if (pbuf_header(q, sizeof(struct eth_hdr)) != 0) {
    /* bail out */
    LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_SERIOUS,
      ("etharp_output: could not allocate room for header.\n"));
    return ERR_BUF;
  }

  /* multicast destination IP address? */
  if (ip6_addr_ismulticast(ip6addr)) {
    /* Hash IP multicast address to MAC address.*/
    dest.addr[0] = 0x33;
    dest.addr[1] = 0x33;
    dest.addr[2] = ((u8_t *)(&(ip6addr->addr[3])))[0];
    dest.addr[3] = ((u8_t *)(&(ip6addr->addr[3])))[1];
    dest.addr[4] = ((u8_t *)(&(ip6addr->addr[3])))[2];
    dest.addr[5] = ((u8_t *)(&(ip6addr->addr[3])))[3];

    /* Send out. */
    return ethip6_send(netif, q, (struct eth_addr*)(netif->hwaddr), &dest);
  }

  /* We have a unicast destination IP address */
  /* TODO anycast? */
  /* Get next hop record. */
  i = nd6_get_next_hop_entry(ip6addr, netif);
  if (i < 0) {
    /* failed to get a next hop neighbor record. */
    return ERR_MEM;
  }

  /* Now that we have a destination record, send or queue the packet. */
  if (neighbor_cache[i].state == ND6_STALE) {
    /* Switch to delay state. */
    neighbor_cache[i].state = ND6_DELAY;
    neighbor_cache[i].counter.delay_time = LWIP_ND6_DELAY_FIRST_PROBE_TIME;
  }
  /* TODO should we send or queue if PROBE? send for now, to let unicast NS pass. */
  if ((neighbor_cache[i].state == ND6_REACHABLE) ||
      (neighbor_cache[i].state == ND6_DELAY) ||
      (neighbor_cache[i].state == ND6_PROBE)) {

    /* Send out. */
    SMEMCPY(dest.addr, neighbor_cache[i].lladdr, 6);
    return ethip6_send(netif, q, (struct eth_addr*)(netif->hwaddr), &dest);
  }

  /* We should queue packet on this interface. */
  pbuf_header(q, -(s16_t)SIZEOF_ETH_HDR);
  return nd6_queue_packet(i, q);
}

#endif /* LWIP_IPV6 && LWIP_ETHERNET */
