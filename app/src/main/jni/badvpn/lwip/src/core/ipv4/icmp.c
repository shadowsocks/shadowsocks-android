/**
 * @file
 * ICMP - Internet Control Message Protocol
 *
 */

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

/* Some ICMP messages should be passed to the transport protocols. This
   is not implemented. */

#include "lwip/opt.h"

#if LWIP_ICMP /* don't build if not configured for use in lwipopts.h */

#include "lwip/icmp.h"
#include "lwip/inet_chksum.h"
#include "lwip/ip.h"
#include "lwip/def.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"

#include <string.h>

/** Small optimization: set to 0 if incoming PBUF_POOL pbuf always can be
 * used to modify and send a response packet (and to 1 if this is not the case,
 * e.g. when link header is stripped of when receiving) */
#ifndef LWIP_ICMP_ECHO_CHECK_INPUT_PBUF_LEN
#define LWIP_ICMP_ECHO_CHECK_INPUT_PBUF_LEN 1
#endif /* LWIP_ICMP_ECHO_CHECK_INPUT_PBUF_LEN */

/* The amount of data from the original packet to return in a dest-unreachable */
#define ICMP_DEST_UNREACH_DATASIZE 8

static void icmp_send_response(struct pbuf *p, u8_t type, u8_t code);

/**
 * Processes ICMP input packets, called from ip_input().
 *
 * Currently only processes icmp echo requests and sends
 * out the echo response.
 *
 * @param p the icmp echo request packet, p->payload pointing to the icmp header
 * @param inp the netif on which this packet was received
 */
void
icmp_input(struct pbuf *p, struct netif *inp)
{
  u8_t type;
#ifdef LWIP_DEBUG
  u8_t code;
#endif /* LWIP_DEBUG */
  struct icmp_echo_hdr *iecho;
  struct ip_hdr *iphdr;
  s16_t hlen;

  ICMP_STATS_INC(icmp.recv);
  snmp_inc_icmpinmsgs();

  iphdr = (struct ip_hdr *)ip_current_header();
  hlen = IPH_HL(iphdr) * 4;
  if (p->len < sizeof(u16_t)*2) {
    LWIP_DEBUGF(ICMP_DEBUG, ("icmp_input: short ICMP (%"U16_F" bytes) received\n", p->tot_len));
    goto lenerr;
  }

  type = *((u8_t *)p->payload);
#ifdef LWIP_DEBUG
  code = *(((u8_t *)p->payload)+1);
#endif /* LWIP_DEBUG */
  switch (type) {
  case ICMP_ER:
    /* This is OK, echo reply might have been parsed by a raw PCB
       (as obviously, an echo request has been sent, too). */
    break; 
  case ICMP_ECHO:
#if !LWIP_MULTICAST_PING || !LWIP_BROADCAST_PING
    {
      int accepted = 1;
#if !LWIP_MULTICAST_PING
      /* multicast destination address? */
      if (ip_addr_ismulticast(ip_current_dest_addr())) {
        accepted = 0;
      }
#endif /* LWIP_MULTICAST_PING */
#if !LWIP_BROADCAST_PING
      /* broadcast destination address? */
      if (ip_addr_isbroadcast(ip_current_dest_addr(), inp)) {
        accepted = 0;
      }
#endif /* LWIP_BROADCAST_PING */
      /* broadcast or multicast destination address not acceptd? */
      if (!accepted) {
        LWIP_DEBUGF(ICMP_DEBUG, ("icmp_input: Not echoing to multicast or broadcast pings\n"));
        ICMP_STATS_INC(icmp.err);
        pbuf_free(p);
        return;
      }
    }
#endif /* !LWIP_MULTICAST_PING || !LWIP_BROADCAST_PING */
    LWIP_DEBUGF(ICMP_DEBUG, ("icmp_input: ping\n"));
    if (p->tot_len < sizeof(struct icmp_echo_hdr)) {
      LWIP_DEBUGF(ICMP_DEBUG, ("icmp_input: bad ICMP echo received\n"));
      goto lenerr;
    }
    if (inet_chksum_pbuf(p) != 0) {
      LWIP_DEBUGF(ICMP_DEBUG, ("icmp_input: checksum failed for received ICMP echo\n"));
      pbuf_free(p);
      ICMP_STATS_INC(icmp.chkerr);
      snmp_inc_icmpinerrors();
      return;
    }
#if LWIP_ICMP_ECHO_CHECK_INPUT_PBUF_LEN
    if (pbuf_header(p, (PBUF_IP_HLEN + PBUF_LINK_HLEN))) {
      /* p is not big enough to contain link headers
       * allocate a new one and copy p into it
       */
      struct pbuf *r;
      /* switch p->payload to ip header */
      if (pbuf_header(p, hlen)) {
        LWIP_ASSERT("icmp_input: moving p->payload to ip header failed\n", 0);
        goto memerr;
      }
      /* allocate new packet buffer with space for link headers */
      r = pbuf_alloc(PBUF_LINK, p->tot_len, PBUF_RAM);
      if (r == NULL) {
        LWIP_DEBUGF(ICMP_DEBUG, ("icmp_input: allocating new pbuf failed\n"));
        goto memerr;
      }
      LWIP_ASSERT("check that first pbuf can hold struct the ICMP header",
                  (r->len >= hlen + sizeof(struct icmp_echo_hdr)));
      /* copy the whole packet including ip header */
      if (pbuf_copy(r, p) != ERR_OK) {
        LWIP_ASSERT("icmp_input: copying to new pbuf failed\n", 0);
        goto memerr;
      }
      iphdr = (struct ip_hdr *)r->payload;
      /* switch r->payload back to icmp header */
      if (pbuf_header(r, -hlen)) {
        LWIP_ASSERT("icmp_input: restoring original p->payload failed\n", 0);
        goto memerr;
      }
      /* free the original p */
      pbuf_free(p);
      /* we now have an identical copy of p that has room for link headers */
      p = r;
    } else {
      /* restore p->payload to point to icmp header */
      if (pbuf_header(p, -(s16_t)(PBUF_IP_HLEN + PBUF_LINK_HLEN))) {
        LWIP_ASSERT("icmp_input: restoring original p->payload failed\n", 0);
        goto memerr;
      }
    }
#endif /* LWIP_ICMP_ECHO_CHECK_INPUT_PBUF_LEN */
    /* At this point, all checks are OK. */
    /* We generate an answer by switching the dest and src ip addresses,
     * setting the icmp type to ECHO_RESPONSE and updating the checksum. */
    iecho = (struct icmp_echo_hdr *)p->payload;
    ip_addr_copy(iphdr->src, *ip_current_dest_addr());
    ip_addr_copy(iphdr->dest, *ip_current_src_addr());
    ICMPH_TYPE_SET(iecho, ICMP_ER);
#if CHECKSUM_GEN_ICMP
    /* adjust the checksum */
    if (iecho->chksum >= PP_HTONS(0xffffU - (ICMP_ECHO << 8))) {
      iecho->chksum += PP_HTONS(ICMP_ECHO << 8) + 1;
    } else {
      iecho->chksum += PP_HTONS(ICMP_ECHO << 8);
    }
#else /* CHECKSUM_GEN_ICMP */
    iecho->chksum = 0;
#endif /* CHECKSUM_GEN_ICMP */

    /* Set the correct TTL and recalculate the header checksum. */
    IPH_TTL_SET(iphdr, ICMP_TTL);
    IPH_CHKSUM_SET(iphdr, 0);
#if CHECKSUM_GEN_IP
    IPH_CHKSUM_SET(iphdr, inet_chksum(iphdr, IP_HLEN));
#endif /* CHECKSUM_GEN_IP */

    ICMP_STATS_INC(icmp.xmit);
    /* increase number of messages attempted to send */
    snmp_inc_icmpoutmsgs();
    /* increase number of echo replies attempted to send */
    snmp_inc_icmpoutechoreps();

    if(pbuf_header(p, hlen)) {
      LWIP_ASSERT("Can't move over header in packet", 0);
    } else {
      err_t ret;
      /* send an ICMP packet, src addr is the dest addr of the curren packet */
      ret = ip_output_if(p, ip_current_dest_addr(), IP_HDRINCL,
                   ICMP_TTL, 0, IP_PROTO_ICMP, inp);
      if (ret != ERR_OK) {
        LWIP_DEBUGF(ICMP_DEBUG, ("icmp_input: ip_output_if returned an error: %c.\n", ret));
      }
    }
    break;
  default:
    LWIP_DEBUGF(ICMP_DEBUG, ("icmp_input: ICMP type %"S16_F" code %"S16_F" not supported.\n", 
                (s16_t)type, (s16_t)code));
    ICMP_STATS_INC(icmp.proterr);
    ICMP_STATS_INC(icmp.drop);
  }
  pbuf_free(p);
  return;
lenerr:
  pbuf_free(p);
  ICMP_STATS_INC(icmp.lenerr);
  snmp_inc_icmpinerrors();
  return;
#if LWIP_ICMP_ECHO_CHECK_INPUT_PBUF_LEN
memerr:
  pbuf_free(p);
  ICMP_STATS_INC(icmp.err);
  snmp_inc_icmpinerrors();
  return;
#endif /* LWIP_ICMP_ECHO_CHECK_INPUT_PBUF_LEN */
}

/**
 * Send an icmp 'destination unreachable' packet, called from ip_input() if
 * the transport layer protocol is unknown and from udp_input() if the local
 * port is not bound.
 *
 * @param p the input packet for which the 'unreachable' should be sent,
 *          p->payload pointing to the IP header
 * @param t type of the 'unreachable' packet
 */
void
icmp_dest_unreach(struct pbuf *p, enum icmp_dur_type t)
{
  icmp_send_response(p, ICMP_DUR, t);
}

#if IP_FORWARD || IP_REASSEMBLY
/**
 * Send a 'time exceeded' packet, called from ip_forward() if TTL is 0.
 *
 * @param p the input packet for which the 'time exceeded' should be sent,
 *          p->payload pointing to the IP header
 * @param t type of the 'time exceeded' packet
 */
void
icmp_time_exceeded(struct pbuf *p, enum icmp_te_type t)
{
  icmp_send_response(p, ICMP_TE, t);
}

#endif /* IP_FORWARD || IP_REASSEMBLY */

/**
 * Send an icmp packet in response to an incoming packet.
 *
 * @param p the input packet for which the 'unreachable' should be sent,
 *          p->payload pointing to the IP header
 * @param type Type of the ICMP header
 * @param code Code of the ICMP header
 */
static void
icmp_send_response(struct pbuf *p, u8_t type, u8_t code)
{
  struct pbuf *q;
  struct ip_hdr *iphdr;
  /* we can use the echo header here */
  struct icmp_echo_hdr *icmphdr;
  ip_addr_t iphdr_src;

  /* ICMP header + IP header + 8 bytes of data */
  q = pbuf_alloc(PBUF_IP, sizeof(struct icmp_echo_hdr) + IP_HLEN + ICMP_DEST_UNREACH_DATASIZE,
                 PBUF_RAM);
  if (q == NULL) {
    LWIP_DEBUGF(ICMP_DEBUG, ("icmp_time_exceeded: failed to allocate pbuf for ICMP packet.\n"));
    return;
  }
  LWIP_ASSERT("check that first pbuf can hold icmp message",
             (q->len >= (sizeof(struct icmp_echo_hdr) + IP_HLEN + ICMP_DEST_UNREACH_DATASIZE)));

  iphdr = (struct ip_hdr *)p->payload;
  LWIP_DEBUGF(ICMP_DEBUG, ("icmp_time_exceeded from "));
  ip_addr_debug_print(ICMP_DEBUG, &(iphdr->src));
  LWIP_DEBUGF(ICMP_DEBUG, (" to "));
  ip_addr_debug_print(ICMP_DEBUG, &(iphdr->dest));
  LWIP_DEBUGF(ICMP_DEBUG, ("\n"));

  icmphdr = (struct icmp_echo_hdr *)q->payload;
  icmphdr->type = type;
  icmphdr->code = code;
  icmphdr->id = 0;
  icmphdr->seqno = 0;

  /* copy fields from original packet */
  SMEMCPY((u8_t *)q->payload + sizeof(struct icmp_echo_hdr), (u8_t *)p->payload,
          IP_HLEN + ICMP_DEST_UNREACH_DATASIZE);

  /* calculate checksum */
  icmphdr->chksum = 0;
  icmphdr->chksum = inet_chksum(icmphdr, q->len);
  ICMP_STATS_INC(icmp.xmit);
  /* increase number of messages attempted to send */
  snmp_inc_icmpoutmsgs();
  /* increase number of destination unreachable messages attempted to send */
  snmp_inc_icmpouttimeexcds();
  ip_addr_copy(iphdr_src, iphdr->src);
  ip_output(q, NULL, &iphdr_src, ICMP_TTL, 0, IP_PROTO_ICMP);
  pbuf_free(q);
}

#endif /* LWIP_ICMP */
