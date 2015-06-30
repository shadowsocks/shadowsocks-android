/**
 * @file
 *
 * IPv6 layer.
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

#if LWIP_IPV6  /* don't build if not configured for use in lwipopts.h */

#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/netif.h"
#include "lwip/ip6.h"
#include "lwip/ip6_addr.h"
#include "lwip/ip6_frag.h"
#include "lwip/icmp6.h"
#include "lwip/raw.h"
#include "lwip/udp.h"
#include "lwip/tcp_impl.h"
#include "lwip/dhcp6.h"
#include "lwip/nd6.h"
#include "lwip/mld6.h"
#include "lwip/debug.h"
#include "lwip/stats.h"


/**
 * Finds the appropriate network interface for a given IPv6 address. It tries to select
 * a netif following a sequence of heuristics:
 * 1) if there is only 1 netif, return it
 * 2) if the destination is a link-local address, try to match the src address to a netif.
 *    this is a tricky case because with multiple netifs, link-local addresses only have
 *    meaning within a particular subnet/link.
 * 3) tries to match the destination subnet to a configured address
 * 4) tries to find a router
 * 5) tries to match the source address to the netif
 * 6) returns the default netif, if configured
 *
 * @param src the source IPv6 address, if known
 * @param dest the destination IPv6 address for which to find the route
 * @return the netif on which to send to reach dest
 */
struct netif *
ip6_route(struct ip6_addr *src, struct ip6_addr *dest)
{
  struct netif *netif;
  s8_t i;

  /* If single netif configuration, fast return. */
  if ((netif_list != NULL) && (netif_list->next == NULL)) {
    return netif_list;
  }

  /* Special processing for link-local addresses. */
  if (ip6_addr_islinklocal(dest)) {
    if (ip6_addr_isany(src)) {
      /* Use default netif. */
      return netif_default;
    }

    /* Try to find the netif for the source address. */
    for(netif = netif_list; netif != NULL; netif = netif->next) {
      for (i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {
        if (ip6_addr_isvalid(netif_ip6_addr_state(netif, i)) &&
            ip6_addr_cmp(src, netif_ip6_addr(netif, i))) {
          return netif;
        }
      }
    }

    /* netif not found, use default netif */
    return netif_default;
  }

  /* See if the destination subnet matches a configured address. */
  for(netif = netif_list; netif != NULL; netif = netif->next) {
    for (i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {
      if (ip6_addr_isvalid(netif_ip6_addr_state(netif, i)) &&
          ip6_addr_netcmp(dest, netif_ip6_addr(netif, i))) {
        return netif;
      }
    }
  }

  /* Get the netif for a suitable router. */
  i = nd6_select_router(dest, NULL);
  if (i >= 0) {
    if (default_router_list[i].neighbor_entry != NULL) {
      if (default_router_list[i].neighbor_entry->netif != NULL) {
        return default_router_list[i].neighbor_entry->netif;
      }
    }
  }

  /* try with the netif that matches the source address. */
  if (!ip6_addr_isany(src)) {
    for(netif = netif_list; netif != NULL; netif = netif->next) {
      for (i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {
        if (ip6_addr_isvalid(netif_ip6_addr_state(netif, i)) &&
            ip6_addr_cmp(src, netif_ip6_addr(netif, i))) {
          return netif;
        }
      }
    }
  }

  /* no matching netif found, use default netif */
  return netif_default;
}

/**
 * Select the best IPv6 source address for a given destination
 * IPv6 address. Loosely follows RFC 3484. "Strong host" behavior
 * is assumed.
 *
 * @param netif the netif on which to send a packet
 * @param dest the destination we are trying to reach
 * @return the most suitable source address to use, or NULL if no suitable
 *         source address is found
 */
ip6_addr_t *
ip6_select_source_address(struct netif *netif, ip6_addr_t * dest)
{
  ip6_addr_t * src = NULL;
  u8_t i;

  /* If dest is link-local, choose a link-local source. */
  if (ip6_addr_islinklocal(dest) || ip6_addr_ismulticast_linklocal(dest) || ip6_addr_ismulticast_iflocal(dest)) {
    for (i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {
      if (ip6_addr_isvalid(netif_ip6_addr_state(netif, i)) &&
          ip6_addr_islinklocal(netif_ip6_addr(netif, i))) {
        return netif_ip6_addr(netif, i);
      }
    }
  }

  /* Choose a site-local with matching prefix. */
  if (ip6_addr_issitelocal(dest) || ip6_addr_ismulticast_sitelocal(dest)) {
    for (i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {
      if (ip6_addr_isvalid(netif_ip6_addr_state(netif, i)) &&
          ip6_addr_issitelocal(netif_ip6_addr(netif, i)) &&
          ip6_addr_netcmp(dest, netif_ip6_addr(netif, i))) {
        return netif_ip6_addr(netif, i);
      }
    }
  }

  /* Choose a unique-local with matching prefix. */
  if (ip6_addr_isuniquelocal(dest) || ip6_addr_ismulticast_orglocal(dest)) {
    for (i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {
      if (ip6_addr_isvalid(netif_ip6_addr_state(netif, i)) &&
          ip6_addr_isuniquelocal(netif_ip6_addr(netif, i)) &&
          ip6_addr_netcmp(dest, netif_ip6_addr(netif, i))) {
        return netif_ip6_addr(netif, i);
      }
    }
  }

  /* Choose a global with best matching prefix. */
  if (ip6_addr_isglobal(dest) || ip6_addr_ismulticast_global(dest)) {
    for (i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {
      if (ip6_addr_isvalid(netif_ip6_addr_state(netif, i)) &&
          ip6_addr_isglobal(netif_ip6_addr(netif, i))) {
        if (src == NULL) {
          src = netif_ip6_addr(netif, i);
        }
        else {
          /* Replace src only if we find a prefix match. */
          /* TODO find longest matching prefix. */
          if ((!(ip6_addr_netcmp(src, dest))) &&
              ip6_addr_netcmp(netif_ip6_addr(netif, i), dest)) {
            src = netif_ip6_addr(netif, i);
          }
        }
      }
    }
    if (src != NULL) {
      return src;
    }
  }

  /* Last resort: see if arbitrary prefix matches. */
  for (i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {
    if (ip6_addr_isvalid(netif_ip6_addr_state(netif, i)) &&
        ip6_addr_netcmp(dest, netif_ip6_addr(netif, i))) {
      return netif_ip6_addr(netif, i);
    }
  }

  return NULL;
}

#if LWIP_IPV6_FORWARD
/**
 * Forwards an IPv6 packet. It finds an appropriate route for the
 * packet, decrements the HL value of the packet, and outputs
 * the packet on the appropriate interface.
 *
 * @param p the packet to forward (p->payload points to IP header)
 * @param iphdr the IPv6 header of the input packet
 * @param inp the netif on which this packet was received
 */
static void
ip6_forward(struct pbuf *p, struct ip6_hdr *iphdr, struct netif *inp)
{
  struct netif *netif;

  /* do not forward link-local addresses */
  if (ip6_addr_islinklocal(ip6_current_dest_addr())) {
    LWIP_DEBUGF(IP6_DEBUG, ("ip6_forward: not forwarding link-local address.\n"));
    IP6_STATS_INC(ip6.rterr);
    IP6_STATS_INC(ip6.drop);
    return;
  }

  /* Find network interface where to forward this IP packet to. */
  netif = ip6_route(IP6_ADDR_ANY, ip6_current_dest_addr());
  if (netif == NULL) {
    LWIP_DEBUGF(IP6_DEBUG, ("ip6_forward: no route for %"X16_F":%"X16_F":%"X16_F":%"X16_F":%"X16_F":%"X16_F":%"X16_F":%"X16_F"\n",
        IP6_ADDR_BLOCK1(ip6_current_dest_addr()),
        IP6_ADDR_BLOCK2(ip6_current_dest_addr()),
        IP6_ADDR_BLOCK3(ip6_current_dest_addr()),
        IP6_ADDR_BLOCK4(ip6_current_dest_addr()),
        IP6_ADDR_BLOCK5(ip6_current_dest_addr()),
        IP6_ADDR_BLOCK6(ip6_current_dest_addr()),
        IP6_ADDR_BLOCK7(ip6_current_dest_addr()),
        IP6_ADDR_BLOCK8(ip6_current_dest_addr())));
#if LWIP_ICMP6
    /* Don't send ICMP messages in response to ICMP messages */
    if (IP6H_NEXTH(iphdr) != IP6_NEXTH_ICMP6) {
      icmp6_dest_unreach(p, ICMP6_DUR_NO_ROUTE);
    }
#endif /* LWIP_ICMP6 */
    IP6_STATS_INC(ip6.rterr);
    IP6_STATS_INC(ip6.drop);
    return;
  }
  /* Do not forward packets onto the same network interface on which
   * they arrived. */
  if (netif == inp) {
    LWIP_DEBUGF(IP6_DEBUG, ("ip6_forward: not bouncing packets back on incoming interface.\n"));
    IP6_STATS_INC(ip6.rterr);
    IP6_STATS_INC(ip6.drop);
    return;
  }

  /* decrement HL */
  IP6H_HOPLIM_SET(iphdr, IP6H_HOPLIM(iphdr) - 1);
  /* send ICMP6 if HL == 0 */
  if (IP6H_HOPLIM(iphdr) == 0) {
#if LWIP_ICMP6
    /* Don't send ICMP messages in response to ICMP messages */
    if (IP6H_NEXTH(iphdr) != IP6_NEXTH_ICMP6) {
      icmp6_time_exceeded(p, ICMP6_TE_HL);
    }
#endif /* LWIP_ICMP6 */
    IP6_STATS_INC(ip6.drop);
    return;
  }

  if (netif->mtu && (p->tot_len > netif->mtu)) {
#if LWIP_ICMP6
    /* Don't send ICMP messages in response to ICMP messages */
    if (IP6H_NEXTH(iphdr) != IP6_NEXTH_ICMP6) {
      icmp6_packet_too_big(p, netif->mtu);
    }
#endif /* LWIP_ICMP6 */
    IP6_STATS_INC(ip6.drop);
    return;
  }

  LWIP_DEBUGF(IP6_DEBUG, ("ip6_forward: forwarding packet to %"X16_F":%"X16_F":%"X16_F":%"X16_F":%"X16_F":%"X16_F":%"X16_F":%"X16_F"\n",
      IP6_ADDR_BLOCK1(ip6_current_dest_addr()),
      IP6_ADDR_BLOCK2(ip6_current_dest_addr()),
      IP6_ADDR_BLOCK3(ip6_current_dest_addr()),
      IP6_ADDR_BLOCK4(ip6_current_dest_addr()),
      IP6_ADDR_BLOCK5(ip6_current_dest_addr()),
      IP6_ADDR_BLOCK6(ip6_current_dest_addr()),
      IP6_ADDR_BLOCK7(ip6_current_dest_addr()),
      IP6_ADDR_BLOCK8(ip6_current_dest_addr())));

  /* transmit pbuf on chosen interface */
  netif->output_ip6(netif, p, ip6_current_dest_addr());
  IP6_STATS_INC(ip6.fw);
  IP6_STATS_INC(ip6.xmit);
  return;
}
#endif /* LWIP_IPV6_FORWARD */


/**
 * This function is called by the network interface device driver when
 * an IPv6 packet is received. The function does the basic checks of the
 * IP header such as packet size being at least larger than the header
 * size etc. If the packet was not destined for us, the packet is
 * forwarded (using ip6_forward).
 *
 * Finally, the packet is sent to the upper layer protocol input function.
 *
 * @param p the received IPv6 packet (p->payload points to IPv6 header)
 * @param inp the netif on which this packet was received
 * @return ERR_OK if the packet was processed (could return ERR_* if it wasn't
 *         processed, but currently always returns ERR_OK)
 */
err_t
ip6_input(struct pbuf *p, struct netif *inp)
{
  struct ip6_hdr *ip6hdr;
  struct netif *netif;
  u8_t nexth;
  u16_t hlen; /* the current header length */
  u8_t i;
#if 0 /*IP_ACCEPT_LINK_LAYER_ADDRESSING*/
  @todo
  int check_ip_src=1;
#endif /* IP_ACCEPT_LINK_LAYER_ADDRESSING */

  IP6_STATS_INC(ip6.recv);

  /* identify the IP header */
  ip6hdr = (struct ip6_hdr *)p->payload;
  if (IP6H_V(ip6hdr) != 6) {
    LWIP_DEBUGF(IP6_DEBUG | LWIP_DBG_LEVEL_WARNING, ("IPv6 packet dropped due to bad version number %"U32_F"\n",
        IP6H_V(ip6hdr)));
    pbuf_free(p);
    IP6_STATS_INC(ip6.err);
    IP6_STATS_INC(ip6.drop);
    return ERR_OK;
  }

  /* header length exceeds first pbuf length, or ip length exceeds total pbuf length? */
  if ((IP6_HLEN > p->len) || ((IP6H_PLEN(ip6hdr) + IP6_HLEN) > p->tot_len)) {
    if (IP6_HLEN > p->len) {
      LWIP_DEBUGF(IP6_DEBUG | LWIP_DBG_LEVEL_SERIOUS,
        ("IPv6 header (len %"U16_F") does not fit in first pbuf (len %"U16_F"), IP packet dropped.\n",
            IP6_HLEN, p->len));
    }
    if ((IP6H_PLEN(ip6hdr) + IP6_HLEN) > p->tot_len) {
      LWIP_DEBUGF(IP6_DEBUG | LWIP_DBG_LEVEL_SERIOUS,
        ("IPv6 (plen %"U16_F") is longer than pbuf (len %"U16_F"), IP packet dropped.\n",
            IP6H_PLEN(ip6hdr) + IP6_HLEN, p->tot_len));
    }
    /* free (drop) packet pbufs */
    pbuf_free(p);
    IP6_STATS_INC(ip6.lenerr);
    IP6_STATS_INC(ip6.drop);
    return ERR_OK;
  }

  /* Trim pbuf. This should have been done at the netif layer,
   * but we'll do it anyway just to be sure that its done. */
  pbuf_realloc(p, IP6_HLEN + IP6H_PLEN(ip6hdr));

  /* copy IP addresses to aligned ip6_addr_t */
  ip6_addr_copy(ip_data.current_iphdr_dest.ip6, ip6hdr->dest);
  ip6_addr_copy(ip_data.current_iphdr_src.ip6, ip6hdr->src);

  /* current header pointer. */
  ip_data.current_ip6_header = ip6hdr;

  /* In netif, used in case we need to send ICMPv6 packets back. */
  ip_data.current_netif = inp;

  /* match packet against an interface, i.e. is this packet for us? */
  if (ip6_addr_ismulticast(ip6_current_dest_addr())) {
    /* Always joined to multicast if-local and link-local all-nodes group. */
    if (ip6_addr_isallnodes_iflocal(ip6_current_dest_addr()) ||
        ip6_addr_isallnodes_linklocal(ip6_current_dest_addr())) {
      netif = inp;
    }
#if LWIP_IPV6_MLD
    else if (mld6_lookfor_group(inp, ip6_current_dest_addr())) {
      netif = inp;
    }
#else /* LWIP_IPV6_MLD */
    else if (ip6_addr_issolicitednode(ip6_current_dest_addr())) {
      /* Filter solicited node packets when MLD is not enabled
       * (for Neighbor discovery). */
      netif = NULL;
      for (i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {
        if (ip6_addr_isvalid(netif_ip6_addr_state(inp, i)) &&
            ip6_addr_cmp_solicitednode(ip6_current_dest_addr(), netif_ip6_addr(inp, i))) {
          netif = inp;
          LWIP_DEBUGF(IP6_DEBUG, ("ip6_input: solicited node packet accepted on interface %c%c\n",
              netif->name[0], netif->name[1]));
          break;
        }
      }
    }
#endif /* LWIP_IPV6_MLD */
    else {
      netif = NULL;
    }
  }
  else {
    /* start trying with inp. if that's not acceptable, start walking the
       list of configured netifs.
       'first' is used as a boolean to mark whether we started walking the list */
    int first = 1;
    netif = inp;
    do {
      /* interface is up? */
      if (netif_is_up(netif)) {
        /* unicast to this interface address? address configured? */
        for (i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {
          if (ip6_addr_isvalid(netif_ip6_addr_state(netif, i)) &&
              ip6_addr_cmp(ip6_current_dest_addr(), netif_ip6_addr(netif, i))) {
            /* exit outer loop */
            goto netif_found;
          }
        }
      }
      if (ip6_addr_islinklocal(ip6_current_dest_addr())) {
        /* Do not match link-local addresses to other netifs. */
        netif = NULL;
        break;
      }
      if (first) {
        first = 0;
        netif = netif_list;
      } else {
        netif = netif->next;
      }
      if (netif == inp) {
        netif = netif->next;
      }
    } while(netif != NULL);
netif_found:
    LWIP_DEBUGF(IP6_DEBUG, ("ip6_input: packet accepted on interface %c%c\n",
        netif ? netif->name[0] : 'X', netif? netif->name[1] : 'X'));
  }

  /* "::" packet source address? (used in duplicate address detection) */
  if (ip6_addr_isany(ip6_current_src_addr()) &&
      (!ip6_addr_issolicitednode(ip6_current_dest_addr()))) {
    /* packet source is not valid */
    /* free (drop) packet pbufs */
    LWIP_DEBUGF(IP6_DEBUG, ("ip6_input: packet with src ANY_ADDRESS dropped\n"));
    pbuf_free(p);
    IP6_STATS_INC(ip6.drop);
    goto ip6_input_cleanup;
  }
  
  /* if we're pretending we are everyone for TCP, assume the packet is for source interface if it
     isn't for a local address */
  if (netif == NULL && (inp->flags & NETIF_FLAG_PRETEND_TCP) && IP6H_NEXTH(ip6hdr) == IP6_NEXTH_TCP) {
      netif = inp;
  }

  /* packet not for us? */
  if (netif == NULL) {
    /* packet not for us, route or discard */
    LWIP_DEBUGF(IP6_DEBUG | LWIP_DBG_TRACE, ("ip6_input: packet not for us.\n"));
#if LWIP_IPV6_FORWARD
    /* non-multicast packet? */
    if (!ip6_addr_ismulticast(ip6_current_dest_addr())) {
      /* try to forward IP packet on (other) interfaces */
      ip6_forward(p, ip6hdr, inp);
    }
#endif /* LWIP_IPV6_FORWARD */
    pbuf_free(p);
    goto ip6_input_cleanup;
  }

  /* current netif pointer. */
  ip_data.current_netif = netif;

  /* Save next header type. */
  nexth = IP6H_NEXTH(ip6hdr);

  /* Init header length. */
  hlen = ip_data.current_ip_header_tot_len = IP6_HLEN;

  /* Move to payload. */
  pbuf_header(p, -IP6_HLEN);

  /* Process known option extension headers, if present. */
  while (nexth != IP6_NEXTH_NONE)
  {
    switch (nexth) {
    case IP6_NEXTH_HOPBYHOP:
      LWIP_DEBUGF(IP6_DEBUG, ("ip6_input: packet with Hop-by-Hop options header\n"));
      /* Get next header type. */
      nexth = *((u8_t *)p->payload);

      /* Get the header length. */
      hlen = 8 * (1 + *((u8_t *)p->payload + 1));
      ip_data.current_ip_header_tot_len += hlen;

      /* Skip over this header. */
      if (hlen > p->len) {
        LWIP_DEBUGF(IP6_DEBUG | LWIP_DBG_LEVEL_SERIOUS,
          ("IPv6 options header (hlen %"U16_F") does not fit in first pbuf (len %"U16_F"), IPv6 packet dropped.\n",
              hlen, p->len));
        /* free (drop) packet pbufs */
        pbuf_free(p);
        IP6_STATS_INC(ip6.lenerr);
        IP6_STATS_INC(ip6.drop);
        goto ip6_input_cleanup;
      }

      pbuf_header(p, -hlen);
      break;
    case IP6_NEXTH_DESTOPTS:
      LWIP_DEBUGF(IP6_DEBUG, ("ip6_input: packet with Destination options header\n"));
      /* Get next header type. */
      nexth = *((u8_t *)p->payload);

      /* Get the header length. */
      hlen = 8 * (1 + *((u8_t *)p->payload + 1));
      ip_data.current_ip_header_tot_len += hlen;

      /* Skip over this header. */
      if (hlen > p->len) {
        LWIP_DEBUGF(IP6_DEBUG | LWIP_DBG_LEVEL_SERIOUS,
          ("IPv6 options header (hlen %"U16_F") does not fit in first pbuf (len %"U16_F"), IPv6 packet dropped.\n",
              hlen, p->len));
        /* free (drop) packet pbufs */
        pbuf_free(p);
        IP6_STATS_INC(ip6.lenerr);
        IP6_STATS_INC(ip6.drop);
        goto ip6_input_cleanup;
      }

      pbuf_header(p, -hlen);
      break;
    case IP6_NEXTH_ROUTING:
      LWIP_DEBUGF(IP6_DEBUG, ("ip6_input: packet with Routing header\n"));
      /* Get next header type. */
      nexth = *((u8_t *)p->payload);

      /* Get the header length. */
      hlen = 8 * (1 + *((u8_t *)p->payload + 1));
      ip_data.current_ip_header_tot_len += hlen;

      /* Skip over this header. */
      if (hlen > p->len) {
        LWIP_DEBUGF(IP6_DEBUG | LWIP_DBG_LEVEL_SERIOUS,
          ("IPv6 options header (hlen %"U16_F") does not fit in first pbuf (len %"U16_F"), IPv6 packet dropped.\n",
              hlen, p->len));
        /* free (drop) packet pbufs */
        pbuf_free(p);
        IP6_STATS_INC(ip6.lenerr);
        IP6_STATS_INC(ip6.drop);
        goto ip6_input_cleanup;
      }

      pbuf_header(p, -hlen);
      break;

    case IP6_NEXTH_FRAGMENT:
    {
      struct ip6_frag_hdr * frag_hdr;
      LWIP_DEBUGF(IP6_DEBUG, ("ip6_input: packet with Fragment header\n"));

      frag_hdr = (struct ip6_frag_hdr *)p->payload;

      /* Get next header type. */
      nexth = frag_hdr->_nexth;

      /* Fragment Header length. */
      hlen = 8;
      ip_data.current_ip_header_tot_len += hlen;

      /* Make sure this header fits in current pbuf. */
      if (hlen > p->len) {
        LWIP_DEBUGF(IP6_DEBUG | LWIP_DBG_LEVEL_SERIOUS,
          ("IPv6 options header (hlen %"U16_F") does not fit in first pbuf (len %"U16_F"), IPv6 packet dropped.\n",
              hlen, p->len));
        /* free (drop) packet pbufs */
        pbuf_free(p);
        IP6_FRAG_STATS_INC(ip6_frag.lenerr);
        IP6_FRAG_STATS_INC(ip6_frag.drop);
        goto ip6_input_cleanup;
      }

      /* Offset == 0 and more_fragments == 0? */
      if (((frag_hdr->_fragment_offset & IP6_FRAG_OFFSET_MASK) == 0) &&
          ((frag_hdr->_fragment_offset & IP6_FRAG_MORE_FLAG) == 0)) {

        /* This is a 1-fragment packet, usually a packet that we have
         * already reassembled. Skip this header anc continue. */
        pbuf_header(p, -hlen);
      }
      else {
#if LWIP_IPV6_REASS

        /* reassemble the packet */
        p = ip6_reass(p);
        /* packet not fully reassembled yet? */
        if (p == NULL) {
          goto ip6_input_cleanup;
        }

        /* Returned p point to IPv6 header.
         * Update all our variables and pointers and continue. */
        ip6hdr = (struct ip6_hdr *)p->payload;
        nexth = IP6H_NEXTH(ip6hdr);
        hlen = ip_data.current_ip_header_tot_len = IP6_HLEN;
        pbuf_header(p, -IP6_HLEN);

#else /* LWIP_IPV6_REASS */
        /* free (drop) packet pbufs */
        LWIP_DEBUGF(IP6_DEBUG, ("ip6_input: packet with Fragment header dropped (with LWIP_IPV6_REASS==0)\n"));
        pbuf_free(p);
        IP6_STATS_INC(ip6.opterr);
        IP6_STATS_INC(ip6.drop);
        goto ip6_input_cleanup;
#endif /* LWIP_IPV6_REASS */
      }
      break;
    }
    default:
      goto options_done;
      break;
    }
  }
options_done:

  /* p points to IPv6 header again. */
  pbuf_header(p, ip_data.current_ip_header_tot_len);

  /* send to upper layers */
  LWIP_DEBUGF(IP6_DEBUG, ("ip6_input: \n"));
  ip6_debug_print(p);
  LWIP_DEBUGF(IP6_DEBUG, ("ip6_input: p->len %"U16_F" p->tot_len %"U16_F"\n", p->len, p->tot_len));

#if LWIP_RAW
  /* raw input did not eat the packet? */
  if (raw_input(p, inp) == 0)
#endif /* LWIP_RAW */
  {
    switch (nexth) {
    case IP6_NEXTH_NONE:
      pbuf_free(p);
      break;
#if LWIP_UDP
    case IP6_NEXTH_UDP:
#if LWIP_UDPLITE
    case IP6_NEXTH_UDPLITE:
#endif /* LWIP_UDPLITE */
      /* Point to payload. */
      pbuf_header(p, -ip_data.current_ip_header_tot_len);
      udp_input(p, inp);
      break;
#endif /* LWIP_UDP */
#if LWIP_TCP
    case IP6_NEXTH_TCP:
      /* Point to payload. */
      pbuf_header(p, -ip_data.current_ip_header_tot_len);
      tcp_input(p, inp);
      break;
#endif /* LWIP_TCP */
#if LWIP_ICMP6
    case IP6_NEXTH_ICMP6:
      /* Point to payload. */
      pbuf_header(p, -ip_data.current_ip_header_tot_len);
      icmp6_input(p, inp);
      break;
#endif /* LWIP_ICMP */
    default:
#if LWIP_ICMP6
      /* send ICMP parameter problem unless it was a multicast or ICMPv6 */
      if ((!ip6_addr_ismulticast(ip6_current_dest_addr())) &&
          (IP6H_NEXTH(ip6hdr) != IP6_NEXTH_ICMP6)) {
        icmp6_param_problem(p, ICMP6_PP_HEADER, ip_data.current_ip_header_tot_len - hlen);
      }
#endif /* LWIP_ICMP */
      LWIP_DEBUGF(IP6_DEBUG | LWIP_DBG_LEVEL_SERIOUS, ("ip6_input: Unsupported transport protocol %"U16_F"\n", IP6H_NEXTH(ip6hdr)));
      pbuf_free(p);
      IP6_STATS_INC(ip6.proterr);
      IP6_STATS_INC(ip6.drop);
      break;
    }
  }

ip6_input_cleanup:
  ip_data.current_netif = NULL;
  ip_data.current_ip6_header = NULL;
  ip_data.current_ip_header_tot_len = 0;
  ip6_addr_set_any(&ip_data.current_iphdr_src.ip6);
  ip6_addr_set_any(&ip_data.current_iphdr_dest.ip6);

  return ERR_OK;
}


/**
 * Sends an IPv6 packet on a network interface. This function constructs
 * the IPv6 header. If the source IPv6 address is NULL, the IPv6 "ANY" address is
 * used as source (usually during network startup). If the source IPv6 address it
 * IP6_ADDR_ANY, the most appropriate IPv6 address of the outgoing network
 * interface is filled in as source address. If the destination IPv6 address is
 * IP_HDRINCL, p is assumed to already include an IPv6 header and p->payload points
 * to it instead of the data.
 *
 * @param p the packet to send (p->payload points to the data, e.g. next
            protocol header; if dest == IP_HDRINCL, p already includes an
            IPv6 header and p->payload points to that IPv6 header)
 * @param src the source IPv6 address to send from (if src == IP6_ADDR_ANY, an
 *         IP address of the netif is selected and used as source address.
 *         if src == NULL, IP6_ADDR_ANY is used as source)
 * @param dest the destination IPv6 address to send the packet to
 * @param hl the Hop Limit value to be set in the IPv6 header
 * @param tc the Traffic Class value to be set in the IPv6 header
 * @param nexth the Next Header to be set in the IPv6 header
 * @param netif the netif on which to send this packet
 * @return ERR_OK if the packet was sent OK
 *         ERR_BUF if p doesn't have enough space for IPv6/LINK headers
 *         returns errors returned by netif->output
 */
err_t
ip6_output_if(struct pbuf *p, ip6_addr_t *src, ip6_addr_t *dest,
             u8_t hl, u8_t tc,
             u8_t nexth, struct netif *netif)
{
  struct ip6_hdr *ip6hdr;
  ip6_addr_t dest_addr;

  /* pbufs passed to IP must have a ref-count of 1 as their payload pointer
     gets altered as the packet is passed down the stack */
  LWIP_ASSERT("p->ref == 1", p->ref == 1);

  /* Should the IPv6 header be generated or is it already included in p? */
  if (dest != IP_HDRINCL) {
    /* generate IPv6 header */
    if (pbuf_header(p, IP6_HLEN)) {
      LWIP_DEBUGF(IP6_DEBUG | LWIP_DBG_LEVEL_SERIOUS, ("ip6_output: not enough room for IPv6 header in pbuf\n"));
      IP6_STATS_INC(ip6.err);
      return ERR_BUF;
    }

    ip6hdr = (struct ip6_hdr *)p->payload;
    LWIP_ASSERT("check that first pbuf can hold struct ip6_hdr",
               (p->len >= sizeof(struct ip6_hdr)));

    IP6H_HOPLIM_SET(ip6hdr, hl);
    IP6H_NEXTH_SET(ip6hdr, nexth);

    /* dest cannot be NULL here */
    ip6_addr_copy(ip6hdr->dest, *dest);

    IP6H_VTCFL_SET(ip6hdr, 6, tc, 0);
    IP6H_PLEN_SET(ip6hdr, p->tot_len - IP6_HLEN);

    if (src == NULL) {
      src = IP6_ADDR_ANY;
    }
    else if (ip6_addr_isany(src)) {
      src = ip6_select_source_address(netif, dest);
      if ((src == NULL) || ip6_addr_isany(src)) {
        /* No appropriate source address was found for this packet. */
        LWIP_DEBUGF(IP6_DEBUG | LWIP_DBG_LEVEL_SERIOUS, ("ip6_output: No suitable source address for packet.\n"));
        IP6_STATS_INC(ip6.rterr);
        return ERR_RTE;
      }
    }
    /* src cannot be NULL here */
    ip6_addr_copy(ip6hdr->src, *src);

  } else {
    /* IP header already included in p */
    ip6hdr = (struct ip6_hdr *)p->payload;
    ip6_addr_copy(dest_addr, ip6hdr->dest);
    dest = &dest_addr;
  }

  IP6_STATS_INC(ip6.xmit);

  LWIP_DEBUGF(IP6_DEBUG, ("ip6_output_if: %c%c%"U16_F"\n", netif->name[0], netif->name[1], netif->num));
  ip6_debug_print(p);

#if ENABLE_LOOPBACK
  /* TODO implement loopback for v6
  if (ip6_addr_cmp(dest, netif_ip6_addr(0))) {
    return netif_loop_output(netif, p, dest);
  }*/
#endif /* ENABLE_LOOPBACK */
#if LWIP_IPV6_FRAG
  /* don't fragment if interface has mtu set to 0 [loopif] */
  if (netif->mtu && (p->tot_len > nd6_get_destination_mtu(dest, netif))) {
    return ip6_frag(p, netif, dest);
  }
#endif /* LWIP_IPV6_FRAG */

  LWIP_DEBUGF(IP6_DEBUG, ("netif->output_ip6()"));
  return netif->output_ip6(netif, p, dest);
}

/**
 * Simple interface to ip6_output_if. It finds the outgoing network
 * interface and calls upon ip6_output_if to do the actual work.
 *
 * @param p the packet to send (p->payload points to the data, e.g. next
            protocol header; if dest == IP_HDRINCL, p already includes an
            IPv6 header and p->payload points to that IPv6 header)
 * @param src the source IPv6 address to send from (if src == IP6_ADDR_ANY, an
 *         IP address of the netif is selected and used as source address.
 *         if src == NULL, IP6_ADDR_ANY is used as source)
 * @param dest the destination IPv6 address to send the packet to
 * @param hl the Hop Limit value to be set in the IPv6 header
 * @param tc the Traffic Class value to be set in the IPv6 header
 * @param nexth the Next Header to be set in the IPv6 header
 *
 * @return ERR_RTE if no route is found
 *         see ip_output_if() for more return values
 */
err_t
ip6_output(struct pbuf *p, ip6_addr_t *src, ip6_addr_t *dest,
          u8_t hl, u8_t tc, u8_t nexth)
{
  struct netif *netif;
  struct ip6_hdr *ip6hdr;
  ip6_addr_t src_addr, dest_addr;

  /* pbufs passed to IPv6 must have a ref-count of 1 as their payload pointer
     gets altered as the packet is passed down the stack */
  LWIP_ASSERT("p->ref == 1", p->ref == 1);

  if (dest != IP_HDRINCL) {
    netif = ip6_route(src, dest);
  } else {
    /* IP header included in p, read addresses. */
    ip6hdr = (struct ip6_hdr *)p->payload;
    ip6_addr_copy(src_addr, ip6hdr->src);
    ip6_addr_copy(dest_addr, ip6hdr->dest);
    netif = ip6_route(&src_addr, &dest_addr);
  }

  if (netif == NULL) {
    LWIP_DEBUGF(IP6_DEBUG, ("ip6_output: no route for %"X16_F":%"X16_F":%"X16_F":%"X16_F":%"X16_F":%"X16_F":%"X16_F":%"X16_F"\n",
        IP6_ADDR_BLOCK1(dest),
        IP6_ADDR_BLOCK2(dest),
        IP6_ADDR_BLOCK3(dest),
        IP6_ADDR_BLOCK4(dest),
        IP6_ADDR_BLOCK5(dest),
        IP6_ADDR_BLOCK6(dest),
        IP6_ADDR_BLOCK7(dest),
        IP6_ADDR_BLOCK8(dest)));
    IP6_STATS_INC(ip6.rterr);
    return ERR_RTE;
  }

  return ip6_output_if(p, src, dest, hl, tc, nexth, netif);
}


#if LWIP_NETIF_HWADDRHINT
/** Like ip6_output, but takes and addr_hint pointer that is passed on to netif->addr_hint
 *  before calling ip6_output_if.
 *
 * @param p the packet to send (p->payload points to the data, e.g. next
            protocol header; if dest == IP_HDRINCL, p already includes an
            IPv6 header and p->payload points to that IPv6 header)
 * @param src the source IPv6 address to send from (if src == IP6_ADDR_ANY, an
 *         IP address of the netif is selected and used as source address.
 *         if src == NULL, IP6_ADDR_ANY is used as source)
 * @param dest the destination IPv6 address to send the packet to
 * @param hl the Hop Limit value to be set in the IPv6 header
 * @param tc the Traffic Class value to be set in the IPv6 header
 * @param nexth the Next Header to be set in the IPv6 header
 * @param addr_hint address hint pointer set to netif->addr_hint before
 *        calling ip_output_if()
 *
 * @return ERR_RTE if no route is found
 *         see ip_output_if() for more return values
 */
err_t
ip6_output_hinted(struct pbuf *p, ip6_addr_t *src, ip6_addr_t *dest,
          u8_t hl, u8_t tc, u8_t nexth, u8_t *addr_hint)
{
  struct netif *netif;
  struct ip6_hdr *ip6hdr;
  ip6_addr_t src_addr, dest_addr;
  err_t err;

  /* pbufs passed to IP must have a ref-count of 1 as their payload pointer
     gets altered as the packet is passed down the stack */
  LWIP_ASSERT("p->ref == 1", p->ref == 1);

  if (dest != IP_HDRINCL) {
    netif = ip6_route(src, dest);
  } else {
    /* IP header included in p, read addresses. */
    ip6hdr = (struct ip6_hdr *)p->payload;
    ip6_addr_copy(src_addr, ip6hdr->src);
    ip6_addr_copy(dest_addr, ip6hdr->dest);
    netif = ip6_route(&src_addr, &dest_addr);
  }

  if (netif == NULL) {
    LWIP_DEBUGF(IP6_DEBUG, ("ip6_output: no route for %"X16_F":%"X16_F":%"X16_F":%"X16_F":%"X16_F":%"X16_F":%"X16_F":%"X16_F"\n",
        IP6_ADDR_BLOCK1(dest),
        IP6_ADDR_BLOCK2(dest),
        IP6_ADDR_BLOCK3(dest),
        IP6_ADDR_BLOCK4(dest),
        IP6_ADDR_BLOCK5(dest),
        IP6_ADDR_BLOCK6(dest),
        IP6_ADDR_BLOCK7(dest),
        IP6_ADDR_BLOCK8(dest)));
    IP6_STATS_INC(ip6.rterr);
    return ERR_RTE;
  }

  NETIF_SET_HWADDRHINT(netif, addr_hint);
  err = ip6_output_if(p, src, dest, hl, tc, nexth, netif);
  NETIF_SET_HWADDRHINT(netif, NULL);

  return err;
}
#endif /* LWIP_NETIF_HWADDRHINT*/

#if LWIP_IPV6_MLD
/**
 * Add a hop-by-hop options header with a router alert option and padding.
 *
 * Used by MLD when sending a Multicast listener report/done message.
 *
 * @param p the packet to which we will prepend the options header
 * @param nexth the next header protocol number (e.g. IP6_NEXTH_ICMP6)
 * @param value the value of the router alert option data (e.g. IP6_ROUTER_ALERT_VALUE_MLD)
 * @return ERR_OK if hop-by-hop header was added, ERR_* otherwise
 */
err_t
ip6_options_add_hbh_ra(struct pbuf * p, u8_t nexth, u8_t value)
{
  struct ip6_hbh_hdr * hbh_hdr;

  /* Move pointer to make room for hop-by-hop options header. */
  if (pbuf_header(p, sizeof(struct ip6_hbh_hdr))) {
    LWIP_DEBUGF(IP6_DEBUG, ("ip6_options: no space for options header\n"));
    IP6_STATS_INC(ip6.err);
    return ERR_BUF;
  }

  hbh_hdr = (struct ip6_hbh_hdr *)p->payload;

  /* Set fields. */
  hbh_hdr->_nexth = nexth;
  hbh_hdr->_hlen = 0;
  hbh_hdr->_ra_opt_type = IP6_ROUTER_ALERT_OPTION;
  hbh_hdr->_ra_opt_dlen = 2;
  hbh_hdr->_ra_opt_data = value;
  hbh_hdr->_padn_opt_type = IP6_PADN_ALERT_OPTION;
  hbh_hdr->_padn_opt_dlen = 0;

  return ERR_OK;
}
#endif /* LWIP_IPV6_MLD */

#if IP6_DEBUG
/* Print an IPv6 header by using LWIP_DEBUGF
 * @param p an IPv6 packet, p->payload pointing to the IPv6 header
 */
void
ip6_debug_print(struct pbuf *p)
{
  struct ip6_hdr *ip6hdr = (struct ip6_hdr *)p->payload;

  LWIP_DEBUGF(IP6_DEBUG, ("IPv6 header:\n"));
  LWIP_DEBUGF(IP6_DEBUG, ("+-------------------------------+\n"));
  LWIP_DEBUGF(IP6_DEBUG, ("| %2"U16_F" |  %3"U16_F"  |      %7"U32_F"     | (ver, class, flow)\n",
                    IP6H_V(ip6hdr),
                    IP6H_TC(ip6hdr),
                    IP6H_FL(ip6hdr)));
  LWIP_DEBUGF(IP6_DEBUG, ("+-------------------------------+\n"));
  LWIP_DEBUGF(IP6_DEBUG, ("|     %5"U16_F"     |  %3"U16_F"  |  %3"U16_F"  | (plen, nexth, hopl)\n",
                    IP6H_PLEN(ip6hdr),
                    IP6H_NEXTH(ip6hdr),
                    IP6H_HOPLIM(ip6hdr)));
  LWIP_DEBUGF(IP6_DEBUG, ("+-------------------------------+\n"));
  LWIP_DEBUGF(IP6_DEBUG, ("|  %4"X32_F" |  %4"X32_F" |  %4"X32_F" |  %4"X32_F" | (src)\n",
                    IP6_ADDR_BLOCK1(&(ip6hdr->src)),
                    IP6_ADDR_BLOCK2(&(ip6hdr->src)),
                    IP6_ADDR_BLOCK3(&(ip6hdr->src)),
                    IP6_ADDR_BLOCK4(&(ip6hdr->src))));
  LWIP_DEBUGF(IP6_DEBUG, ("|  %4"X32_F" |  %4"X32_F" |  %4"X32_F" |  %4"X32_F" |\n",
                    IP6_ADDR_BLOCK5(&(ip6hdr->src)),
                    IP6_ADDR_BLOCK6(&(ip6hdr->src)),
                    IP6_ADDR_BLOCK7(&(ip6hdr->src)),
                    IP6_ADDR_BLOCK8(&(ip6hdr->src))));
  LWIP_DEBUGF(IP6_DEBUG, ("+-------------------------------+\n"));
  LWIP_DEBUGF(IP6_DEBUG, ("|  %4"X32_F" |  %4"X32_F" |  %4"X32_F" |  %4"X32_F" | (dest)\n",
                    IP6_ADDR_BLOCK1(&(ip6hdr->dest)),
                    IP6_ADDR_BLOCK2(&(ip6hdr->dest)),
                    IP6_ADDR_BLOCK3(&(ip6hdr->dest)),
                    IP6_ADDR_BLOCK4(&(ip6hdr->dest))));
  LWIP_DEBUGF(IP6_DEBUG, ("|  %4"X32_F" |  %4"X32_F" |  %4"X32_F" |  %4"X32_F" |\n",
                    IP6_ADDR_BLOCK5(&(ip6hdr->dest)),
                    IP6_ADDR_BLOCK6(&(ip6hdr->dest)),
                    IP6_ADDR_BLOCK7(&(ip6hdr->dest)),
                    IP6_ADDR_BLOCK8(&(ip6hdr->dest))));
  LWIP_DEBUGF(IP6_DEBUG, ("+-------------------------------+\n"));
}
#endif /* IP6_DEBUG */

#endif /* LWIP_IPV6 */
