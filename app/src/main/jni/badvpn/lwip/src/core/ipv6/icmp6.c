/**
 * @file
 *
 * IPv6 version of ICMP, as per RFC 4443.
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

#if LWIP_ICMP6 && LWIP_IPV6 /* don't build if not configured for use in lwipopts.h */

#include "lwip/icmp6.h"
#include "lwip/ip6.h"
#include "lwip/ip6_addr.h"
#include "lwip/inet_chksum.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "lwip/nd6.h"
#include "lwip/mld6.h"
#include "lwip/stats.h"

#include <string.h>

#ifndef LWIP_ICMP6_DATASIZE
#define LWIP_ICMP6_DATASIZE   8
#endif
#if LWIP_ICMP6_DATASIZE == 0
#define LWIP_ICMP6_DATASIZE   8
#endif

/* Forward declarations */
static void icmp6_send_response(struct pbuf *p, u8_t code, u32_t data, u8_t type);


/**
 * Process an input ICMPv6 message. Called by ip6_input.
 *
 * Will generate a reply for echo requests. Other messages are forwarded
 * to nd6_input, or mld6_input.
 *
 * @param p the mld packet, p->payload pointing to the icmpv6 header
 * @param inp the netif on which this packet was received
 */
void
icmp6_input(struct pbuf *p, struct netif *inp)
{
  struct icmp6_hdr *icmp6hdr;
  struct pbuf * r;
  ip6_addr_t * reply_src;

  ICMP6_STATS_INC(icmp6.recv);

  /* Check that ICMPv6 header fits in payload */
  if (p->len < sizeof(struct icmp6_hdr)) {
    /* drop short packets */
    pbuf_free(p);
    ICMP6_STATS_INC(icmp6.lenerr);
    ICMP6_STATS_INC(icmp6.drop);
    return;
  }

  icmp6hdr = (struct icmp6_hdr *)p->payload;

#if LWIP_ICMP6_CHECKSUM_CHECK
  if (ip6_chksum_pseudo(p, IP6_NEXTH_ICMP6, p->tot_len, ip6_current_src_addr(),
                        ip6_current_dest_addr()) != 0) {
    /* Checksum failed */
    pbuf_free(p);
    ICMP6_STATS_INC(icmp6.chkerr);
    ICMP6_STATS_INC(icmp6.drop);
    return;
  }
#endif /* LWIP_ICMP6_CHECKSUM_CHECK */

  switch (icmp6hdr->type) {
  case ICMP6_TYPE_NA: /* Neighbor advertisement */
  case ICMP6_TYPE_NS: /* Neighbor solicitation */
  case ICMP6_TYPE_RA: /* Router advertisement */
  case ICMP6_TYPE_RD: /* Redirect */
  case ICMP6_TYPE_PTB: /* Packet too big */
    nd6_input(p, inp);
    return;
    break;
  case ICMP6_TYPE_RS:
#if LWIP_IPV6_FORWARD
    /* TODO implement router functionality */
#endif
    break;
#if LWIP_IPV6_MLD
  case ICMP6_TYPE_MLQ:
  case ICMP6_TYPE_MLR:
  case ICMP6_TYPE_MLD:
    mld6_input(p, inp);
    return;
    break;
#endif
  case ICMP6_TYPE_EREQ:
#if !LWIP_MULTICAST_PING
    /* multicast destination address? */
    if (ip6_addr_ismulticast(ip6_current_dest_addr())) {
      /* drop */
      pbuf_free(p);
      ICMP6_STATS_INC(icmp6.drop);
      return;
    }
#endif /* LWIP_MULTICAST_PING */

    /* Allocate reply. */
    r = pbuf_alloc(PBUF_IP, p->tot_len, PBUF_RAM);
    if (r == NULL) {
      /* drop */
      pbuf_free(p);
      ICMP6_STATS_INC(icmp6.memerr);
      return;
    }

    /* Copy echo request. */
    if (pbuf_copy(r, p) != ERR_OK) {
      /* drop */
      pbuf_free(p);
      pbuf_free(r);
      ICMP6_STATS_INC(icmp6.err);
      return;
    }

    /* Determine reply source IPv6 address. */
#if LWIP_MULTICAST_PING
    if (ip6_addr_ismulticast(ip6_current_dest_addr())) {
      reply_src = ip6_select_source_address(inp, ip6_current_src_addr());
      if (reply_src == NULL) {
        /* drop */
        pbuf_free(p);
        pbuf_free(r);
        ICMP6_STATS_INC(icmp6.rterr);
        return;
      }
    }
    else
#endif /* LWIP_MULTICAST_PING */
    {
      reply_src = ip6_current_dest_addr();
    }

    /* Set fields in reply. */
    ((struct icmp6_echo_hdr *)(r->payload))->type = ICMP6_TYPE_EREP;
    ((struct icmp6_echo_hdr *)(r->payload))->chksum = 0;
    ((struct icmp6_echo_hdr *)(r->payload))->chksum = ip6_chksum_pseudo(r,
        IP6_NEXTH_ICMP6, r->tot_len, reply_src, ip6_current_src_addr());

    /* Send reply. */
    ICMP6_STATS_INC(icmp6.xmit);
    ip6_output_if(r, reply_src, ip6_current_src_addr(),
        LWIP_ICMP6_HL, 0, IP6_NEXTH_ICMP6, inp);
    pbuf_free(r);

    break;
  default:
    ICMP6_STATS_INC(icmp6.proterr);
    ICMP6_STATS_INC(icmp6.drop);
    break;
  }

  pbuf_free(p);
}


/**
 * Send an icmpv6 'destination unreachable' packet.
 *
 * @param p the input packet for which the 'unreachable' should be sent,
 *          p->payload pointing to the IPv6 header
 * @param c ICMPv6 code for the unreachable type
 */
void
icmp6_dest_unreach(struct pbuf *p, enum icmp6_dur_code c)
{
  icmp6_send_response(p, c, 0, ICMP6_TYPE_DUR);
}

/**
 * Send an icmpv6 'packet too big' packet.
 *
 * @param p the input packet for which the 'packet too big' should be sent,
 *          p->payload pointing to the IPv6 header
 * @param mtu the maximum mtu that we can accept
 */
void
icmp6_packet_too_big(struct pbuf *p, u32_t mtu)
{
  icmp6_send_response(p, 0, mtu, ICMP6_TYPE_PTB);
}

/**
 * Send an icmpv6 'time exceeded' packet.
 *
 * @param p the input packet for which the 'unreachable' should be sent,
 *          p->payload pointing to the IPv6 header
 * @param c ICMPv6 code for the time exceeded type
 */
void
icmp6_time_exceeded(struct pbuf *p, enum icmp6_te_code c)
{
  icmp6_send_response(p, c, 0, ICMP6_TYPE_TE);
}

/**
 * Send an icmpv6 'parameter problem' packet.
 *
 * @param p the input packet for which the 'param problem' should be sent,
 *          p->payload pointing to the IP header
 * @param c ICMPv6 code for the param problem type
 * @param pointer the pointer to the byte where the parameter is found
 */
void
icmp6_param_problem(struct pbuf *p, enum icmp6_pp_code c, u32_t pointer)
{
  icmp6_send_response(p, c, pointer, ICMP6_TYPE_PP);
}

/**
 * Send an ICMPv6 packet in response to an incoming packet.
 *
 * @param p the input packet for which the response should be sent,
 *          p->payload pointing to the IPv6 header
 * @param code Code of the ICMPv6 header
 * @param data Additional 32-bit parameter in the ICMPv6 header
 * @param type Type of the ICMPv6 header
 */
static void
icmp6_send_response(struct pbuf *p, u8_t code, u32_t data, u8_t type)
{
  struct pbuf *q;
  struct icmp6_hdr *icmp6hdr;
  ip6_addr_t *reply_src, *reply_dest;
  ip6_addr_t reply_src_local, reply_dest_local;
  struct ip6_hdr *ip6hdr;
  struct netif *netif;

  /* ICMPv6 header + IPv6 header + data */
  q = pbuf_alloc(PBUF_IP, sizeof(struct icmp6_hdr) + IP6_HLEN + LWIP_ICMP6_DATASIZE,
                 PBUF_RAM);
  if (q == NULL) {
    LWIP_DEBUGF(ICMP_DEBUG, ("icmp_time_exceeded: failed to allocate pbuf for ICMPv6 packet.\n"));
    ICMP6_STATS_INC(icmp6.memerr);
    return;
  }
  LWIP_ASSERT("check that first pbuf can hold icmp 6message",
             (q->len >= (sizeof(struct icmp6_hdr) + IP6_HLEN + LWIP_ICMP6_DATASIZE)));

  icmp6hdr = (struct icmp6_hdr *)q->payload;
  icmp6hdr->type = type;
  icmp6hdr->code = code;
  icmp6hdr->data = data;

  /* copy fields from original packet */
  SMEMCPY((u8_t *)q->payload + sizeof(struct icmp6_hdr), (u8_t *)p->payload,
          IP6_HLEN + LWIP_ICMP6_DATASIZE);

  /* Get the destination address and netif for this ICMP message. */
  if ((ip_current_netif() == NULL) ||
      ((code == ICMP6_TE_FRAG) && (type == ICMP6_TYPE_TE))) {
    /* Special case, as ip6_current_xxx is either NULL, or points
     * to a different packet than the one that expired.
     * We must use the addresses that are stored in the expired packet. */
    ip6hdr = (struct ip6_hdr *)p->payload;
    /* copy from packed address to aligned address */
    ip6_addr_copy(reply_dest_local, ip6hdr->src);
    ip6_addr_copy(reply_src_local, ip6hdr->dest);
    reply_dest = &reply_dest_local;
    reply_src = &reply_src_local;
    netif = ip6_route(reply_src, reply_dest);
    if (netif == NULL) {
      /* drop */
      pbuf_free(q);
      ICMP6_STATS_INC(icmp6.rterr);
      return;
    }
  }
  else {
    netif = ip_current_netif();
    reply_dest = ip6_current_src_addr();

    /* Select an address to use as source. */
    reply_src = ip6_select_source_address(netif, reply_dest);
    if (reply_src == NULL) {
      /* drop */
      pbuf_free(q);
      ICMP6_STATS_INC(icmp6.rterr);
      return;
    }
  }

  /* calculate checksum */
  icmp6hdr->chksum = 0;
  icmp6hdr->chksum = ip6_chksum_pseudo(q, IP6_NEXTH_ICMP6, q->tot_len,
    reply_src, reply_dest);

  ICMP6_STATS_INC(icmp6.xmit);
  ip6_output_if(q, reply_src, reply_dest, LWIP_ICMP6_HL, 0, IP6_NEXTH_ICMP6, netif);
  pbuf_free(q);
}

#endif /* LWIP_ICMP6 && LWIP_IPV6 */
