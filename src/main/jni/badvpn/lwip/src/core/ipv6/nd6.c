/**
 * @file
 *
 * Neighbor discovery and stateless address autoconfiguration for IPv6.
 * Aims to be compliant with RFC 4861 (Neighbor discovery) and RFC 4862
 * (Address autoconfiguration).
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

#include "lwip/nd6.h"
#include "lwip/pbuf.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/ip6.h"
#include "lwip/ip6_addr.h"
#include "lwip/inet_chksum.h"
#include "lwip/netif.h"
#include "lwip/icmp6.h"
#include "lwip/mld6.h"
#include "lwip/stats.h"

#include <string.h>


/* Router tables. */
struct nd6_neighbor_cache_entry neighbor_cache[LWIP_ND6_NUM_NEIGHBORS];
struct nd6_destination_cache_entry destination_cache[LWIP_ND6_NUM_DESTINATIONS];
struct nd6_prefix_list_entry prefix_list[LWIP_ND6_NUM_PREFIXES];
struct nd6_router_list_entry default_router_list[LWIP_ND6_NUM_ROUTERS];

/* Default values, can be updated by a RA message. */
u32_t reachable_time = LWIP_ND6_REACHABLE_TIME;
u32_t retrans_timer = LWIP_ND6_RETRANS_TIMER; /* TODO implement this value in timer */

/* Index for cache entries. */
static u8_t nd6_cached_neighbor_index;
static u8_t nd6_cached_destination_index;

/* Multicast address holder. */
static ip6_addr_t multicast_address;

/* Static buffer to parse RA packet options (size of a prefix option, biggest option) */
static u8_t nd6_ra_buffer[sizeof(struct prefix_option)];

/* Forward declarations. */
static s8_t nd6_find_neighbor_cache_entry(ip6_addr_t * ip6addr);
static s8_t nd6_new_neighbor_cache_entry(void);
static void nd6_free_neighbor_cache_entry(s8_t i);
static s8_t nd6_find_destination_cache_entry(ip6_addr_t * ip6addr);
static s8_t nd6_new_destination_cache_entry(void);
static s8_t nd6_is_prefix_in_netif(ip6_addr_t * ip6addr, struct netif * netif);
static s8_t nd6_get_router(ip6_addr_t * router_addr, struct netif * netif);
static s8_t nd6_new_router(ip6_addr_t * router_addr, struct netif * netif);
static s8_t nd6_get_onlink_prefix(ip6_addr_t * prefix, struct netif * netif);
static s8_t nd6_new_onlink_prefix(ip6_addr_t * prefix, struct netif * netif);

#define ND6_SEND_FLAG_MULTICAST_DEST 0x01
#define ND6_SEND_FLAG_ALLNODES_DEST 0x02
static void nd6_send_ns(struct netif * netif, ip6_addr_t * target_addr, u8_t flags);
static void nd6_send_na(struct netif * netif, ip6_addr_t * target_addr, u8_t flags);
#if LWIP_IPV6_SEND_ROUTER_SOLICIT
static void nd6_send_rs(struct netif * netif);
#endif /* LWIP_IPV6_SEND_ROUTER_SOLICIT */

#if LWIP_ND6_QUEUEING
static void nd6_free_q(struct nd6_q_entry *q);
#else /* LWIP_ND6_QUEUEING */
#define nd6_free_q(q) pbuf_free(q)
#endif /* LWIP_ND6_QUEUEING */
static void nd6_send_q(s8_t i);


/**
 * Process an incoming neighbor discovery message
 *
 * @param p the nd packet, p->payload pointing to the icmpv6 header
 * @param inp the netif on which this packet was received
 */
void
nd6_input(struct pbuf *p, struct netif *inp)
{
  u8_t msg_type;
  s8_t i;

  ND6_STATS_INC(nd6.recv);

  msg_type = *((u8_t *)p->payload);
  switch (msg_type) {
  case ICMP6_TYPE_NA: /* Neighbor Advertisement. */
  {
    struct na_header * na_hdr;
    struct lladdr_option * lladdr_opt;

    /* Check that na header fits in packet. */
    if (p->len < (sizeof(struct na_header))) {
      /* TODO debug message */
      pbuf_free(p);
      ND6_STATS_INC(nd6.lenerr);
      ND6_STATS_INC(nd6.drop);
      return;
    }

    na_hdr = (struct na_header *)p->payload;

    /* Unsolicited NA?*/
    if (ip6_addr_ismulticast(ip6_current_dest_addr())) {
      /* This is an unsolicited NA.
       * link-layer changed?
       * part of DAD mechanism? */

      /* Check that link-layer address option also fits in packet. */
      if (p->len < (sizeof(struct na_header) + sizeof(struct lladdr_option))) {
        /* TODO debug message */
        pbuf_free(p);
        ND6_STATS_INC(nd6.lenerr);
        ND6_STATS_INC(nd6.drop);
        return;
      }

      lladdr_opt = (struct lladdr_option *)((u8_t*)p->payload + sizeof(struct na_header));

      /* Override ip6_current_dest_addr() so that we have an aligned copy. */
      ip6_addr_set(ip6_current_dest_addr(), &(na_hdr->target_address));

#if LWIP_IPV6_DUP_DETECT_ATTEMPTS
      /* If the target address matches this netif, it is a DAD response. */
      for (i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {
        if (ip6_addr_cmp(ip6_current_dest_addr(), netif_ip6_addr(inp, i))) {
          /* We are using a duplicate address. */
          netif_ip6_addr_set_state(inp, i, IP6_ADDR_INVALID);

#if LWIP_IPV6_MLD
          /* Leave solicited node multicast group. */
          ip6_addr_set_solicitednode(&multicast_address, netif_ip6_addr(inp, i)->addr[3]);
          mld6_leavegroup(netif_ip6_addr(inp, i), &multicast_address);
#endif /* LWIP_IPV6_MLD */




#if LWIP_IPV6_AUTOCONFIG
          /* Check to see if this address was autoconfigured. */
          if (!ip6_addr_islinklocal(ip6_current_dest_addr())) {
            i = nd6_get_onlink_prefix(ip6_current_dest_addr(), inp);
            if (i >= 0) {
              /* Mark this prefix as duplicate, so that we don't use it
               * to generate this address again. */
              prefix_list[i].flags |= ND6_PREFIX_AUTOCONFIG_ADDRESS_DUPLICATE;
            }
          }
#endif /* LWIP_IPV6_AUTOCONFIG */

          pbuf_free(p);
          return;
        }
      }
#endif /* LWIP_IPV6_DUP_DETECT_ATTEMPTS */

      /* This is an unsolicited NA, most likely there was a LLADDR change. */
      i = nd6_find_neighbor_cache_entry(ip6_current_dest_addr());
      if (i >= 0) {
        if (na_hdr->flags & ND6_FLAG_OVERRIDE) {
          MEMCPY(neighbor_cache[i].lladdr, lladdr_opt->addr, inp->hwaddr_len);
        }
      }
    }
    else {
      /* This is a solicited NA.
       * neighbor address resolution response?
       * neighbor unreachability detection response? */

      /* Override ip6_current_dest_addr() so that we have an aligned copy. */
      ip6_addr_set(ip6_current_dest_addr(), &(na_hdr->target_address));

      /* Find the cache entry corresponding to this na. */
      i = nd6_find_neighbor_cache_entry(ip6_current_dest_addr());
      if (i < 0) {
        /* We no longer care about this target address. drop it. */
        pbuf_free(p);
        return;
      }

      /* Update cache entry. */
      neighbor_cache[i].netif = inp;
      neighbor_cache[i].counter.reachable_time = reachable_time;
      if ((na_hdr->flags & ND6_FLAG_OVERRIDE) ||
          (neighbor_cache[i].state == ND6_INCOMPLETE)) {
        /* Check that link-layer address option also fits in packet. */
        if (p->len < (sizeof(struct na_header) + sizeof(struct lladdr_option))) {
          /* TODO debug message */
          pbuf_free(p);
          ND6_STATS_INC(nd6.lenerr);
          ND6_STATS_INC(nd6.drop);
          return;
        }

        lladdr_opt = (struct lladdr_option *)((u8_t*)p->payload + sizeof(struct na_header));

        MEMCPY(neighbor_cache[i].lladdr, lladdr_opt->addr, inp->hwaddr_len);
      }
      neighbor_cache[i].state = ND6_REACHABLE;

      /* Send queued packets, if any. */
      if (neighbor_cache[i].q != NULL) {
        nd6_send_q(i);
      }
    }

    break; /* ICMP6_TYPE_NA */
  }
  case ICMP6_TYPE_NS: /* Neighbor solicitation. */
  {
    struct ns_header * ns_hdr;
    struct lladdr_option * lladdr_opt;
    u8_t accepted;

    /* Check that ns header fits in packet. */
    if (p->len < sizeof(struct ns_header)) {
      /* TODO debug message */
      pbuf_free(p);
      ND6_STATS_INC(nd6.lenerr);
      ND6_STATS_INC(nd6.drop);
      return;
    }

    ns_hdr = (struct ns_header *)p->payload;

    /* Check if there is a link-layer address provided. Only point to it if in this buffer. */
    lladdr_opt = NULL;
    if (p->len >= (sizeof(struct ns_header) + sizeof(struct lladdr_option))) {
      lladdr_opt = (struct lladdr_option *)((u8_t*)p->payload + sizeof(struct ns_header));
    }

    /* Check if the target address is configured on the receiving netif. */
    accepted = 0;
    for (i = 0; i < LWIP_IPV6_NUM_ADDRESSES; ++i) {
      if ((ip6_addr_isvalid(netif_ip6_addr_state(inp, i)) ||
           (ip6_addr_istentative(netif_ip6_addr_state(inp, i)) &&
            ip6_addr_isany(ip6_current_src_addr()))) &&
          ip6_addr_cmp(&(ns_hdr->target_address), netif_ip6_addr(inp, i))) {
        accepted = 1;
        break;
      }
    }

    /* NS not for us? */
    if (!accepted) {
      pbuf_free(p);
      return;
    }

    /* Check for ANY address in src (DAD algorithm). */
    if (ip6_addr_isany(ip6_current_src_addr())) {
      /* Sender is validating this address. */
      for (i = 0; i < LWIP_IPV6_NUM_ADDRESSES; ++i) {
        if (ip6_addr_cmp(&(ns_hdr->target_address), netif_ip6_addr(inp, i))) {
          /* Send a NA back so that the sender does not use this address. */
          nd6_send_na(inp, netif_ip6_addr(inp, i), ND6_FLAG_OVERRIDE | ND6_SEND_FLAG_ALLNODES_DEST);
          if (ip6_addr_istentative(netif_ip6_addr_state(inp, i))) {
            /* We shouldn't use this address either. */
            netif_ip6_addr_set_state(inp, i, IP6_ADDR_INVALID);
          }
        }
      }
    }
    else {
      /* Sender is trying to resolve our address. */
      /* Verify that they included their own link-layer address. */
      if (lladdr_opt == NULL) {
        /* Not a valid message. */
        pbuf_free(p);
        ND6_STATS_INC(nd6.proterr);
        ND6_STATS_INC(nd6.drop);
        return;
      }

      i = nd6_find_neighbor_cache_entry(ip6_current_src_addr());
      if ( i>= 0) {
        /* We already have a record for the solicitor. */
        if (neighbor_cache[i].state == ND6_INCOMPLETE) {
          neighbor_cache[i].netif = inp;
          MEMCPY(neighbor_cache[i].lladdr, lladdr_opt->addr, inp->hwaddr_len);

          /* Delay probe in case we get confirmation of reachability from upper layer (TCP). */
          neighbor_cache[i].state = ND6_DELAY;
          neighbor_cache[i].counter.delay_time = LWIP_ND6_DELAY_FIRST_PROBE_TIME;
        }
      }
      else
      {
        /* Add their IPv6 address and link-layer address to neighbor cache.
         * We will need it at least to send a unicast NA message, but most
         * likely we will also be communicating with this node soon. */
        i = nd6_new_neighbor_cache_entry();
        if (i < 0) {
          /* We couldn't assign a cache entry for this neighbor.
           * we won't be able to reply. drop it. */
          pbuf_free(p);
          ND6_STATS_INC(nd6.memerr);
          return;
        }
        neighbor_cache[i].netif = inp;
        MEMCPY(neighbor_cache[i].lladdr, lladdr_opt->addr, inp->hwaddr_len);
        ip6_addr_set(&(neighbor_cache[i].next_hop_address), ip6_current_src_addr());

        /* Receiving a message does not prove reachability: only in one direction.
         * Delay probe in case we get confirmation of reachability from upper layer (TCP). */
        neighbor_cache[i].state = ND6_DELAY;
        neighbor_cache[i].counter.delay_time = LWIP_ND6_DELAY_FIRST_PROBE_TIME;
      }

      /* Override ip6_current_dest_addr() so that we have an aligned copy. */
      ip6_addr_set(ip6_current_dest_addr(), &(ns_hdr->target_address));

      /* Send back a NA for us. Allocate the reply pbuf. */
      nd6_send_na(inp, ip6_current_dest_addr(), ND6_FLAG_SOLICITED | ND6_FLAG_OVERRIDE);
    }

    break; /* ICMP6_TYPE_NS */
  }
  case ICMP6_TYPE_RA: /* Router Advertisement. */
  {
    struct ra_header * ra_hdr;
    u8_t * buffer; /* Used to copy options. */
    u16_t offset;

    /* Check that RA header fits in packet. */
    if (p->len < sizeof(struct ra_header)) {
      /* TODO debug message */
      pbuf_free(p);
      ND6_STATS_INC(nd6.lenerr);
      ND6_STATS_INC(nd6.drop);
      return;
    }

    ra_hdr = (struct ra_header *)p->payload;

    /* If we are sending RS messages, stop. */
#if LWIP_IPV6_SEND_ROUTER_SOLICIT
    inp->rs_count = 0;
#endif /* LWIP_IPV6_SEND_ROUTER_SOLICIT */

    /* Get the matching default router entry. */
    i = nd6_get_router(ip6_current_src_addr(), inp);
    if (i < 0) {
      /* Create a new router entry. */
      i = nd6_new_router(ip6_current_src_addr(), inp);
    }

    if (i < 0) {
      /* Could not create a new router entry. */
      pbuf_free(p);
      ND6_STATS_INC(nd6.memerr);
      return;
    }

    /* Re-set invalidation timer. */
    default_router_list[i].invalidation_timer = ra_hdr->router_lifetime;

    /* Re-set default timer values. */
#if LWIP_ND6_ALLOW_RA_UPDATES
    if (ra_hdr->retrans_timer > 0) {
      retrans_timer = ra_hdr->retrans_timer;
    }
    if (ra_hdr->reachable_time > 0) {
      reachable_time = ra_hdr->reachable_time;
    }
#endif /* LWIP_ND6_ALLOW_RA_UPDATES */

    /* TODO set default hop limit... */
    /* ra_hdr->current_hop_limit;*/

    /* Update flags in local entry (incl. preference). */
    default_router_list[i].flags = ra_hdr->flags;

    /* Offset to options. */
    offset = sizeof(struct ra_header);

    /* Process each option. */
    while ((p->tot_len - offset) > 0) {
      if (p->len == p->tot_len) {
        /* no need to copy from contiguous pbuf */
        buffer = &((u8_t*)p->payload)[offset];
      } else {
        buffer = nd6_ra_buffer;
        pbuf_copy_partial(p, buffer, sizeof(struct prefix_option), offset);
      }
      switch (buffer[0]) {
      case ND6_OPTION_TYPE_SOURCE_LLADDR:
      {
        struct lladdr_option * lladdr_opt;
        lladdr_opt = (struct lladdr_option *)buffer;
        if ((default_router_list[i].neighbor_entry != NULL) &&
            (default_router_list[i].neighbor_entry->state == ND6_INCOMPLETE)) {
          SMEMCPY(default_router_list[i].neighbor_entry->lladdr, lladdr_opt->addr, inp->hwaddr_len);
          default_router_list[i].neighbor_entry->state = ND6_REACHABLE;
          default_router_list[i].neighbor_entry->counter.reachable_time = reachable_time;
        }
        break;
      }
      case ND6_OPTION_TYPE_MTU:
      {
        struct mtu_option * mtu_opt;
        mtu_opt = (struct mtu_option *)buffer;
        if (mtu_opt->mtu >= 1280) {
#if LWIP_ND6_ALLOW_RA_UPDATES
          inp->mtu = mtu_opt->mtu;
#endif /* LWIP_ND6_ALLOW_RA_UPDATES */
        }
        break;
      }
      case ND6_OPTION_TYPE_PREFIX_INFO:
      {
        struct prefix_option * prefix_opt;
        prefix_opt = (struct prefix_option *)buffer;

        if (prefix_opt->flags & ND6_PREFIX_FLAG_ON_LINK) {
          /* Add to on-link prefix list. */

          /* Get a memory-aligned copy of the prefix. */
          ip6_addr_set(ip6_current_dest_addr(), &(prefix_opt->prefix));

          /* find cache entry for this prefix. */
          i = nd6_get_onlink_prefix(ip6_current_dest_addr(), inp);
          if (i < 0) {
            /* Create a new cache entry. */
            i = nd6_new_onlink_prefix(ip6_current_dest_addr(), inp);
          }
          if (i >= 0) {
            prefix_list[i].invalidation_timer = prefix_opt->valid_lifetime;

#if LWIP_IPV6_AUTOCONFIG
            if (prefix_opt->flags & ND6_PREFIX_FLAG_AUTONOMOUS) {
              /* Mark prefix as autonomous, so that address autoconfiguration can take place.
               * Only OR flag, so that we don't over-write other flags (such as ADDRESS_DUPLICATE)*/
              prefix_list[i].flags |= ND6_PREFIX_AUTOCONFIG_AUTONOMOUS;
            }
#endif /* LWIP_IPV6_AUTOCONFIG */
          }
        }

        break;
      }
      case ND6_OPTION_TYPE_ROUTE_INFO:
      {
        /* TODO implement preferred routes.
        struct route_option * route_opt;
        route_opt = (struct route_option *)buffer;*/

        break;
      }
      default:
        /* Unrecognized option, abort. */
        ND6_STATS_INC(nd6.proterr);
        break;
      }
      offset += 8 * ((u16_t)buffer[1]);
    }

    break; /* ICMP6_TYPE_RA */
  }
  case ICMP6_TYPE_RD: /* Redirect */
  {
    struct redirect_header * redir_hdr;
    struct lladdr_option * lladdr_opt;

    /* Check that Redir header fits in packet. */
    if (p->len < sizeof(struct redirect_header)) {
      /* TODO debug message */
      pbuf_free(p);
      ND6_STATS_INC(nd6.lenerr);
      ND6_STATS_INC(nd6.drop);
      return;
    }

    redir_hdr = (struct redirect_header *)p->payload;

    lladdr_opt = NULL;
    if (p->len >= (sizeof(struct redirect_header) + sizeof(struct lladdr_option))) {
      lladdr_opt = (struct lladdr_option *)((u8_t*)p->payload + sizeof(struct redirect_header));
    }

    /* Copy original destination address to current source address, to have an aligned copy. */
    ip6_addr_set(ip6_current_src_addr(), &(redir_hdr->destination_address));

    /* Find dest address in cache */
    i = nd6_find_destination_cache_entry(ip6_current_src_addr());
    if (i < 0) {
      /* Destination not in cache, drop packet. */
      pbuf_free(p);
      return;
    }

    /* Set the new target address. */
    ip6_addr_set(&(destination_cache[i].next_hop_addr), &(redir_hdr->target_address));

    /* If Link-layer address of other router is given, try to add to neighbor cache. */
    if (lladdr_opt != NULL) {
      if (lladdr_opt->type == ND6_OPTION_TYPE_TARGET_LLADDR) {
        /* Copy target address to current source address, to have an aligned copy. */
        ip6_addr_set(ip6_current_src_addr(), &(redir_hdr->target_address));

        i = nd6_find_neighbor_cache_entry(ip6_current_src_addr());
        if (i < 0) {
          i = nd6_new_neighbor_cache_entry();
          if (i >= 0) {
            neighbor_cache[i].netif = inp;
            MEMCPY(neighbor_cache[i].lladdr, lladdr_opt->addr, inp->hwaddr_len);
            ip6_addr_set(&(neighbor_cache[i].next_hop_address), ip6_current_src_addr());

            /* Receiving a message does not prove reachability: only in one direction.
             * Delay probe in case we get confirmation of reachability from upper layer (TCP). */
            neighbor_cache[i].state = ND6_DELAY;
            neighbor_cache[i].counter.delay_time = LWIP_ND6_DELAY_FIRST_PROBE_TIME;
          }
        }
        if (i >= 0) {
          if (neighbor_cache[i].state == ND6_INCOMPLETE) {
            MEMCPY(neighbor_cache[i].lladdr, lladdr_opt->addr, inp->hwaddr_len);
            /* Receiving a message does not prove reachability: only in one direction.
             * Delay probe in case we get confirmation of reachability from upper layer (TCP). */
            neighbor_cache[i].state = ND6_DELAY;
            neighbor_cache[i].counter.delay_time = LWIP_ND6_DELAY_FIRST_PROBE_TIME;
          }
        }
      }
    }
    break; /* ICMP6_TYPE_RD */
  }
  case ICMP6_TYPE_PTB: /* Packet too big */
  {
    struct icmp6_hdr *icmp6hdr; /* Packet too big message */
    struct ip6_hdr * ip6hdr; /* IPv6 header of the packet which caused the error */

    /* Check that ICMPv6 header + IPv6 header fit in payload */
    if (p->len < (sizeof(struct icmp6_hdr) + IP6_HLEN)) {
      /* drop short packets */
      pbuf_free(p);
      ND6_STATS_INC(nd6.lenerr);
      ND6_STATS_INC(nd6.drop);
      return;
    }

    icmp6hdr = (struct icmp6_hdr *)p->payload;
    ip6hdr = (struct ip6_hdr *)((u8_t*)p->payload + sizeof(struct icmp6_hdr));

    /* Copy original destination address to current source address, to have an aligned copy. */
    ip6_addr_set(ip6_current_src_addr(), &(ip6hdr->dest));

    /* Look for entry in destination cache. */
    i = nd6_find_destination_cache_entry(ip6_current_src_addr());
    if (i < 0) {
      /* Destination not in cache, drop packet. */
      pbuf_free(p);
      return;
    }

    /* Change the Path MTU. */
    destination_cache[i].pmtu = icmp6hdr->data;

    break; /* ICMP6_TYPE_PTB */
  }

  default:
    ND6_STATS_INC(nd6.proterr);
    ND6_STATS_INC(nd6.drop);
    break; /* default */
  }

  pbuf_free(p);
}


/**
 * Periodic timer for Neighbor discovery functions:
 *
 * - Update neighbor reachability states
 * - Update destination cache entries age
 * - Update invalidation timers of default routers and on-link prefixes
 * - Perform duplicate address detection (DAD) for our addresses
 * - Send router solicitations
 */
void
nd6_tmr(void)
{
  s8_t i, j;
  struct netif * netif;

  /* Process neighbor entries. */
  for (i = 0; i < LWIP_ND6_NUM_NEIGHBORS; i++) {
    switch (neighbor_cache[i].state) {
    case ND6_INCOMPLETE:
      if (neighbor_cache[i].counter.probes_sent >= LWIP_ND6_MAX_MULTICAST_SOLICIT) {
        /* Retries exceeded. */
        nd6_free_neighbor_cache_entry(i);
      }
      else {
        /* Send a NS for this entry. */
        neighbor_cache[i].counter.probes_sent++;
        nd6_send_ns(neighbor_cache[i].netif, &(neighbor_cache[i].next_hop_address), ND6_SEND_FLAG_MULTICAST_DEST);
      }
      break;
    case ND6_REACHABLE:
      /* Send queued packets, if any are left. Should have been sent already. */
      if (neighbor_cache[i].q != NULL) {
        nd6_send_q(i);
      }
      if (neighbor_cache[i].counter.reachable_time <= ND6_TMR_INTERVAL) {
        /* Change to stale state. */
        neighbor_cache[i].state = ND6_STALE;
        neighbor_cache[i].counter.stale_time = 0;
      }
      else {
        neighbor_cache[i].counter.reachable_time -= ND6_TMR_INTERVAL;
      }
      break;
    case ND6_STALE:
      neighbor_cache[i].counter.stale_time += ND6_TMR_INTERVAL;
      break;
    case ND6_DELAY:
      if (neighbor_cache[i].counter.delay_time <= ND6_TMR_INTERVAL) {
        /* Change to PROBE state. */
        neighbor_cache[i].state = ND6_PROBE;
        neighbor_cache[i].counter.probes_sent = 0;
      }
      else {
        neighbor_cache[i].counter.delay_time -= ND6_TMR_INTERVAL;
      }
      break;
    case ND6_PROBE:
      if (neighbor_cache[i].counter.probes_sent >= LWIP_ND6_MAX_MULTICAST_SOLICIT) {
        /* Retries exceeded. */
        nd6_free_neighbor_cache_entry(i);
      }
      else {
        /* Send a NS for this entry. */
        neighbor_cache[i].counter.probes_sent++;
        nd6_send_ns(neighbor_cache[i].netif, &(neighbor_cache[i].next_hop_address), 0);
      }
      break;
    case ND6_NO_ENTRY:
    default:
      /* Do nothing. */
      break;
    }
  }

  /* Process destination entries. */
  for (i = 0; i < LWIP_ND6_NUM_DESTINATIONS; i++) {
    destination_cache[i].age++;
  }

  /* Process router entries. */
  for (i = 0; i < LWIP_ND6_NUM_ROUTERS; i++) {
    if (default_router_list[i].neighbor_entry != NULL) {
      /* Active entry. */
      if (default_router_list[i].invalidation_timer > 0) {
        default_router_list[i].invalidation_timer -= ND6_TMR_INTERVAL / 1000;
      }
      if (default_router_list[i].invalidation_timer < ND6_TMR_INTERVAL / 1000) {
        /* Less than 1 second remainig. Clear this entry. */
        default_router_list[i].neighbor_entry->isrouter = 0;
        default_router_list[i].neighbor_entry = NULL;
        default_router_list[i].invalidation_timer = 0;
        default_router_list[i].flags = 0;
      }
    }
  }

  /* Process prefix entries. */
  for (i = 0; i < LWIP_ND6_NUM_PREFIXES; i++) {
    if (prefix_list[i].invalidation_timer < ND6_TMR_INTERVAL / 1000) {
      prefix_list[i].invalidation_timer = 0;
    }
    if ((prefix_list[i].invalidation_timer > 0) &&
        (prefix_list[i].netif != NULL)) {
      prefix_list[i].invalidation_timer -= ND6_TMR_INTERVAL / 1000;

#if LWIP_IPV6_AUTOCONFIG
      /* Initiate address autoconfiguration for this prefix, if conditions are met. */
      if (prefix_list[i].netif->ip6_autoconfig_enabled &&
          (prefix_list[i].flags & ND6_PREFIX_AUTOCONFIG_AUTONOMOUS) &&
          !(prefix_list[i].flags & ND6_PREFIX_AUTOCONFIG_ADDRESS_GENERATED)) {
        /* Try to get an address on this netif that is invalid.
         * Skip 0 index (link-local address) */
        for (j = 1; j < LWIP_IPV6_NUM_ADDRESSES; j++) {
          if (netif_ip6_addr_state(prefix_list[i].netif, j) == IP6_ADDRESS_STATE_INVALID) {
            /* Generate an address using this prefix and interface ID from link-local address. */
            prefix_list[i].netif->ip6_addr[j].addr[0] = prefix_list[i].prefix.addr[0];
            prefix_list[i].netif->ip6_addr[j].addr[1] = prefix_list[i].prefix.addr[1];
            prefix_list[i].netif->ip6_addr[j].addr[2] = prefix_list[i].netif->ip6_addr[0].addr[2];
            prefix_list[i].netif->ip6_addr[j].addr[3] = prefix_list[i].netif->ip6_addr[0].addr[3];

            /* Mark it as tentative (DAD will be performed if configured). */
            netif_ip6_addr_set_state(prefix_list[i].netif, j, IP6_ADDR_TENTATIVE);

            /* Mark this prefix with ADDRESS_GENERATED, so that we don't try again. */
            prefix_list[i].flags |= ND6_PREFIX_AUTOCONFIG_ADDRESS_GENERATED;

            /* Exit loop. */
            break;
          }
        }
      }
#endif /* LWIP_IPV6_AUTOCONFIG */
    }
  }


  /* Process our own addresses, if DAD configured. */
  for (netif = netif_list; netif != NULL; netif = netif->next) {
    for (i = 0; i < LWIP_IPV6_NUM_ADDRESSES; ++i) {
      if (ip6_addr_istentative(netif->ip6_addr_state[i])) {
        if ((netif->ip6_addr_state[i] & 0x07) >= LWIP_IPV6_DUP_DETECT_ATTEMPTS) {
          /* No NA received in response. Mark address as valid. */
          netif->ip6_addr_state[i] = IP6_ADDR_PREFERRED;
          /* TODO implement preferred and valid lifetimes. */
        }
        else if (netif->flags & NETIF_FLAG_UP) {
#if LWIP_IPV6_MLD
          if ((netif->ip6_addr_state[i] & 0x07) == 0) {
            /* Join solicited node multicast group. */
            ip6_addr_set_solicitednode(&multicast_address, netif_ip6_addr(netif, i)->addr[3]);
            mld6_joingroup(netif_ip6_addr(netif, i), &multicast_address);
          }
#endif /* LWIP_IPV6_MLD */
          /* Send a NS for this address. */
          nd6_send_ns(netif, netif_ip6_addr(netif, i), ND6_SEND_FLAG_MULTICAST_DEST);
          (netif->ip6_addr_state[i])++;
          /* TODO send max 1 NS per tmr call? enable return*/
          /*return;*/
        }
      }
    }
  }

#if LWIP_IPV6_SEND_ROUTER_SOLICIT
  /* Send router solicitation messages, if necessary. */
  for (netif = netif_list; netif != NULL; netif = netif->next) {
    if ((netif->rs_count > 0) && (netif->flags & NETIF_FLAG_UP)) {
      nd6_send_rs(netif);
      netif->rs_count--;
    }
  }
#endif /* LWIP_IPV6_SEND_ROUTER_SOLICIT */

}

/**
 * Send a neighbor solicitation message
 *
 * @param netif the netif on which to send the message
 * @param target_addr the IPv6 target address for the ND message
 * @param flags one of ND6_SEND_FLAG_*
 */
static void
nd6_send_ns(struct netif * netif, ip6_addr_t * target_addr, u8_t flags)
{
  struct ns_header * ns_hdr;
  struct lladdr_option * lladdr_opt;
  struct pbuf * p;
  ip6_addr_t * src_addr;

  if (ip6_addr_isvalid(netif_ip6_addr_state(netif,0))) {
    /* Use link-local address as source address. */
    src_addr = netif_ip6_addr(netif, 0);
  } else {
    src_addr = IP6_ADDR_ANY;
  }

  /* Allocate a packet. */
  p = pbuf_alloc(PBUF_IP, sizeof(struct ns_header) + sizeof(struct lladdr_option), PBUF_RAM);
  if ((p == NULL) || (p->len < (sizeof(struct ns_header) + sizeof(struct lladdr_option)))) {
    /* We couldn't allocate a suitable pbuf for the ns. drop it. */
    if (p != NULL) {
      pbuf_free(p);
    }
    ND6_STATS_INC(nd6.memerr);
    return;
  }

  /* Set fields. */
  ns_hdr = (struct ns_header *)p->payload;
  lladdr_opt = (struct lladdr_option *)((u8_t*)p->payload + sizeof(struct ns_header));

  ns_hdr->type = ICMP6_TYPE_NS;
  ns_hdr->code = 0;
  ns_hdr->chksum = 0;
  ns_hdr->reserved = 0;
  ip6_addr_set(&(ns_hdr->target_address), target_addr);

  lladdr_opt->type = ND6_OPTION_TYPE_SOURCE_LLADDR;
  lladdr_opt->length = ((netif->hwaddr_len + 2) >> 3) + (((netif->hwaddr_len + 2) & 0x07) ? 1 : 0);
  SMEMCPY(lladdr_opt->addr, netif->hwaddr, netif->hwaddr_len);

  /* Generate the solicited node address for the target address. */
  if (flags & ND6_SEND_FLAG_MULTICAST_DEST) {
    ip6_addr_set_solicitednode(&multicast_address, target_addr->addr[3]);
    target_addr = &multicast_address;
  }

  ns_hdr->chksum = ip6_chksum_pseudo(p, IP6_NEXTH_ICMP6, p->len, src_addr,
    target_addr);

  /* Send the packet out. */
  ND6_STATS_INC(nd6.xmit);
  ip6_output_if(p, (src_addr == IP6_ADDR_ANY) ? NULL : src_addr, target_addr,
      LWIP_ICMP6_HL, 0, IP6_NEXTH_ICMP6, netif);
  pbuf_free(p);
}

/**
 * Send a neighbor advertisement message
 *
 * @param netif the netif on which to send the message
 * @param target_addr the IPv6 target address for the ND message
 * @param flags one of ND6_SEND_FLAG_*
 */
static void
nd6_send_na(struct netif * netif, ip6_addr_t * target_addr, u8_t flags)
{
  struct na_header * na_hdr;
  struct lladdr_option * lladdr_opt;
  struct pbuf * p;
  ip6_addr_t * src_addr;
  ip6_addr_t * dest_addr;

  /* Use link-local address as source address. */
  /* src_addr = &(netif->ip6_addr[0]); */
  /* Use target address as source address. */
  src_addr = target_addr;

  /* Allocate a packet. */
  p = pbuf_alloc(PBUF_IP, sizeof(struct na_header) + sizeof(struct lladdr_option), PBUF_RAM);
  if ((p == NULL) || (p->len < (sizeof(struct na_header) + sizeof(struct lladdr_option)))) {
    /* We couldn't allocate a suitable pbuf for the ns. drop it. */
    if (p != NULL) {
      pbuf_free(p);
    }
    ND6_STATS_INC(nd6.memerr);
    return;
  }

  /* Set fields. */
  na_hdr = (struct na_header *)p->payload;
  lladdr_opt = (struct lladdr_option *)((u8_t*)p->payload + sizeof(struct na_header));

  na_hdr->type = ICMP6_TYPE_NA;
  na_hdr->code = 0;
  na_hdr->chksum = 0;
  na_hdr->flags = flags & 0xf0;
  na_hdr->reserved[0] = 0;
  na_hdr->reserved[1] = 0;
  na_hdr->reserved[2] = 0;
  ip6_addr_set(&(na_hdr->target_address), target_addr);

  lladdr_opt->type = ND6_OPTION_TYPE_TARGET_LLADDR;
  lladdr_opt->length = ((netif->hwaddr_len + 2) >> 3) + (((netif->hwaddr_len + 2) & 0x07) ? 1 : 0);
  SMEMCPY(lladdr_opt->addr, netif->hwaddr, netif->hwaddr_len);

  /* Generate the solicited node address for the target address. */
  if (flags & ND6_SEND_FLAG_MULTICAST_DEST) {
    ip6_addr_set_solicitednode(&multicast_address, target_addr->addr[3]);
    dest_addr = &multicast_address;
  }
  else if (flags & ND6_SEND_FLAG_ALLNODES_DEST) {
    ip6_addr_set_allnodes_linklocal(&multicast_address);
    dest_addr = &multicast_address;
  }
  else {
    dest_addr = ip6_current_src_addr();
  }

  na_hdr->chksum = ip6_chksum_pseudo(p, IP6_NEXTH_ICMP6, p->len, src_addr,
    dest_addr);

  /* Send the packet out. */
  ND6_STATS_INC(nd6.xmit);
  ip6_output_if(p, src_addr, dest_addr,
      LWIP_ICMP6_HL, 0, IP6_NEXTH_ICMP6, netif);
  pbuf_free(p);
}

#if LWIP_IPV6_SEND_ROUTER_SOLICIT
/**
 * Send a router solicitation message
 *
 * @param netif the netif on which to send the message
 */
static void
nd6_send_rs(struct netif * netif)
{
  struct rs_header * rs_hdr;
  struct lladdr_option * lladdr_opt;
  struct pbuf * p;
  ip6_addr_t * src_addr;
  u16_t packet_len;

  /* Link-local source address, or unspecified address? */
  if (ip6_addr_isvalid(netif_ip6_addr_state(netif, 0))) {
    src_addr = netif_ip6_addr(netif, 0);
  }
  else {
    src_addr = IP6_ADDR_ANY;
  }

  /* Generate the all routers target address. */
  ip6_addr_set_allrouters_linklocal(&multicast_address);

  /* Allocate a packet. */
  packet_len = sizeof(struct rs_header);
  if (src_addr != IP6_ADDR_ANY) {
    packet_len += sizeof(struct lladdr_option);
  }
  p = pbuf_alloc(PBUF_IP, packet_len, PBUF_RAM);
  if ((p == NULL) || (p->len < packet_len)) {
    /* We couldn't allocate a suitable pbuf for the ns. drop it. */
    if (p != NULL) {
      pbuf_free(p);
    }
    ND6_STATS_INC(nd6.memerr);
    return;
  }

  /* Set fields. */
  rs_hdr = (struct rs_header *)p->payload;

  rs_hdr->type = ICMP6_TYPE_RS;
  rs_hdr->code = 0;
  rs_hdr->chksum = 0;
  rs_hdr->reserved = 0;

  if (src_addr != IP6_ADDR_ANY) {
    /* Include our hw address. */
    lladdr_opt = (struct lladdr_option *)((u8_t*)p->payload + sizeof(struct rs_header));
    lladdr_opt->type = ND6_OPTION_TYPE_SOURCE_LLADDR;
    lladdr_opt->length = ((netif->hwaddr_len + 2) >> 3) + (((netif->hwaddr_len + 2) & 0x07) ? 1 : 0);
    SMEMCPY(lladdr_opt->addr, netif->hwaddr, netif->hwaddr_len);
  }

  rs_hdr->chksum = ip6_chksum_pseudo(p, IP6_NEXTH_ICMP6, p->len, src_addr,
    &multicast_address);

  /* Send the packet out. */
  ND6_STATS_INC(nd6.xmit);
  ip6_output_if(p, src_addr, &multicast_address,
      LWIP_ICMP6_HL, 0, IP6_NEXTH_ICMP6, netif);
  pbuf_free(p);
}
#endif /* LWIP_IPV6_SEND_ROUTER_SOLICIT */

/**
 * Search for a neighbor cache entry
 *
 * @param ip6addr the IPv6 address of the neighbor
 * @return The neighbor cache entry index that matched, -1 if no
 * entry is found
 */
static s8_t
nd6_find_neighbor_cache_entry(ip6_addr_t * ip6addr)
{
  s8_t i;
  for (i = 0; i < LWIP_ND6_NUM_NEIGHBORS; i++) {
    if (ip6_addr_cmp(ip6addr, &(neighbor_cache[i].next_hop_address))) {
      return i;
    }
  }
  return -1;
}

/**
 * Create a new neighbor cache entry.
 *
 * If no unused entry is found, will try to recycle an old entry
 * according to ad-hoc "age" heuristic.
 *
 * @return The neighbor cache entry index that was created, -1 if no
 * entry could be created
 */
static s8_t
nd6_new_neighbor_cache_entry(void)
{
  s8_t i;
  s8_t j;
  u32_t time;


  /* First, try to find an empty entry. */
  for (i = 0; i < LWIP_ND6_NUM_NEIGHBORS; i++) {
    if (neighbor_cache[i].state == ND6_NO_ENTRY) {
      return i;
    }
  }

  /* We need to recycle an entry. in general, do not recycle if it is a router. */

  /* Next, try to find a Stale entry. */
  for (i = 0; i < LWIP_ND6_NUM_NEIGHBORS; i++) {
    if ((neighbor_cache[i].state == ND6_STALE) &&
        (!neighbor_cache[i].isrouter)) {
      nd6_free_neighbor_cache_entry(i);
      return i;
    }
  }

  /* Next, try to find a Probe entry. */
  for (i = 0; i < LWIP_ND6_NUM_NEIGHBORS; i++) {
    if ((neighbor_cache[i].state == ND6_PROBE) &&
        (!neighbor_cache[i].isrouter)) {
      nd6_free_neighbor_cache_entry(i);
      return i;
    }
  }

  /* Next, try to find a Delayed entry. */
  for (i = 0; i < LWIP_ND6_NUM_NEIGHBORS; i++) {
    if ((neighbor_cache[i].state == ND6_DELAY) &&
        (!neighbor_cache[i].isrouter)) {
      nd6_free_neighbor_cache_entry(i);
      return i;
    }
  }

  /* Next, try to find the oldest reachable entry. */
  time = 0xfffffffful;
  j = -1;
  for (i = 0; i < LWIP_ND6_NUM_NEIGHBORS; i++) {
    if ((neighbor_cache[i].state == ND6_REACHABLE) &&
        (!neighbor_cache[i].isrouter)) {
      if (neighbor_cache[i].counter.reachable_time < time) {
        j = i;
        time = neighbor_cache[i].counter.reachable_time;
      }
    }
  }
  if (j >= 0) {
    nd6_free_neighbor_cache_entry(j);
    return j;
  }

  /* Next, find oldest incomplete entry without queued packets. */
  time = 0;
  j = -1;
  for (i = 0; i < LWIP_ND6_NUM_NEIGHBORS; i++) {
    if (
        (neighbor_cache[i].q == NULL) &&
        (neighbor_cache[i].state == ND6_INCOMPLETE) &&
        (!neighbor_cache[i].isrouter)) {
      if (neighbor_cache[i].counter.probes_sent >= time) {
        j = i;
        time = neighbor_cache[i].counter.probes_sent;
      }
    }
  }
  if (j >= 0) {
    nd6_free_neighbor_cache_entry(j);
    return j;
  }

  /* Next, find oldest incomplete entry with queued packets. */
  time = 0;
  j = -1;
  for (i = 0; i < LWIP_ND6_NUM_NEIGHBORS; i++) {
    if ((neighbor_cache[i].state == ND6_INCOMPLETE) &&
        (!neighbor_cache[i].isrouter)) {
      if (neighbor_cache[i].counter.probes_sent >= time) {
        j = i;
        time = neighbor_cache[i].counter.probes_sent;
      }
    }
  }
  if (j >= 0) {
    nd6_free_neighbor_cache_entry(j);
    return j;
  }

  /* No more entries to try. */
  return -1;
}

/**
 * Will free any resources associated with a neighbor cache
 * entry, and will mark it as unused.
 *
 * @param i the neighbor cache entry index to free
 */
static void
nd6_free_neighbor_cache_entry(s8_t i)
{
  if ((i < 0) || (i >= LWIP_ND6_NUM_NEIGHBORS)) {
    return;
  }

  /* Free any queued packets. */
  if (neighbor_cache[i].q != NULL) {
    nd6_free_q(neighbor_cache[i].q);
    neighbor_cache[i].q = NULL;
  }

  neighbor_cache[i].state = ND6_NO_ENTRY;
  neighbor_cache[i].isrouter = 0;
  neighbor_cache[i].netif = NULL;
  neighbor_cache[i].counter.reachable_time = 0;
  ip6_addr_set_zero(&(neighbor_cache[i].next_hop_address));
}

/**
 * Search for a destination cache entry
 *
 * @param ip6addr the IPv6 address of the destination
 * @return The destination cache entry index that matched, -1 if no
 * entry is found
 */
static s8_t
nd6_find_destination_cache_entry(ip6_addr_t * ip6addr)
{
  s8_t i;
  for (i = 0; i < LWIP_ND6_NUM_DESTINATIONS; i++) {
    if (ip6_addr_cmp(ip6addr, &(destination_cache[i].destination_addr))) {
      return i;
    }
  }
  return -1;
}

/**
 * Create a new destination cache entry. If no unused entry is found,
 * will recycle oldest entry.
 *
 * @return The destination cache entry index that was created, -1 if no
 * entry was created
 */
static s8_t
nd6_new_destination_cache_entry(void)
{
  s8_t i, j;
  u32_t age;

  /* Find an empty entry. */
  for (i = 0; i < LWIP_ND6_NUM_DESTINATIONS; i++) {
    if (ip6_addr_isany(&(destination_cache[i].destination_addr))) {
      return i;
    }
  }

  /* Find oldest entry. */
  age = 0;
  j = LWIP_ND6_NUM_DESTINATIONS - 1;
  for (i = 0; i < LWIP_ND6_NUM_DESTINATIONS; i++) {
    if (destination_cache[i].age > age) {
      j = i;
    }
  }

  return j;
}

/**
 * Determine whether an address matches an on-link prefix.
 *
 * @param ip6addr the IPv6 address to match
 * @return 1 if the address is on-link, 0 otherwise
 */
static s8_t
nd6_is_prefix_in_netif(ip6_addr_t * ip6addr, struct netif * netif)
{
  s8_t i;
  for (i = 0; i < LWIP_ND6_NUM_PREFIXES; i++) {
    if ((prefix_list[i].netif == netif) &&
        (prefix_list[i].invalidation_timer > 0) &&
        ip6_addr_netcmp(ip6addr, &(prefix_list[i].prefix))) {
      return 1;
    }
  }
  /* Check to see if address prefix matches a (manually?) configured address. */
  for (i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {
    if (ip6_addr_isvalid(netif_ip6_addr_state(netif, i)) &&
        ip6_addr_netcmp(ip6addr, netif_ip6_addr(netif, i))) {
      return 1;
    }
  }
  return 0;
}

/**
 * Select a default router for a destination.
 *
 * @param ip6addr the destination address
 * @param netif the netif for the outgoing packet, if known
 * @return the default router entry index, or -1 if no suitable
 *         router is found
 */
s8_t
nd6_select_router(ip6_addr_t * ip6addr, struct netif * netif)
{
  s8_t i;
  /* last_router is used for round-robin router selection (as recommended
   * in RFC). This is more robust in case one router is not reachable,
   * we are not stuck trying to resolve it. */
  static s8_t last_router;
  (void)ip6addr; /* TODO match preferred routes!! (must implement ND6_OPTION_TYPE_ROUTE_INFO) */

  /* TODO: implement default router preference */

  /* Look for reachable routers. */
  for (i = 0; i < LWIP_ND6_NUM_ROUTERS; i++) {
    if (++last_router >= LWIP_ND6_NUM_ROUTERS) {
      last_router = 0;
    }
    if ((default_router_list[i].neighbor_entry != NULL) &&
        (netif != NULL ? netif == default_router_list[i].neighbor_entry->netif : 1) &&
        (default_router_list[i].invalidation_timer > 0) &&
        (default_router_list[i].neighbor_entry->state == ND6_REACHABLE)) {
      return i;
    }
  }

  /* Look for router in other reachability states, but still valid according to timer. */
  for (i = 0; i < LWIP_ND6_NUM_ROUTERS; i++) {
    if (++last_router >= LWIP_ND6_NUM_ROUTERS) {
      last_router = 0;
    }
    if ((default_router_list[i].neighbor_entry != NULL) &&
        (netif != NULL ? netif == default_router_list[i].neighbor_entry->netif : 1) &&
        (default_router_list[i].invalidation_timer > 0)) {
      return i;
    }
  }

  /* Look for any router for which we have any information at all. */
  for (i = 0; i < LWIP_ND6_NUM_ROUTERS; i++) {
    if (++last_router >= LWIP_ND6_NUM_ROUTERS) {
      last_router = 0;
    }
    if (default_router_list[i].neighbor_entry != NULL &&
        (netif != NULL ? netif == default_router_list[i].neighbor_entry->netif : 1)) {
      return i;
    }
  }

  /* no suitable router found. */
  return -1;
}

/**
 * Find an entry for a default router.
 *
 * @param router_addr the IPv6 address of the router
 * @param netif the netif on which the router is found, if known
 * @return the index of the router entry, or -1 if not found
 */
static s8_t
nd6_get_router(ip6_addr_t * router_addr, struct netif * netif)
{
  s8_t i;

  /* Look for router. */
  for (i = 0; i < LWIP_ND6_NUM_ROUTERS; i++) {
    if ((default_router_list[i].neighbor_entry != NULL) &&
        ((netif != NULL) ? netif == default_router_list[i].neighbor_entry->netif : 1) &&
        ip6_addr_cmp(router_addr, &(default_router_list[i].neighbor_entry->next_hop_address))) {
      return i;
    }
  }

  /* router not found. */
  return -1;
}

/**
 * Create a new entry for a default router.
 *
 * @param router_addr the IPv6 address of the router
 * @param netif the netif on which the router is connected, if known
 * @return the index on the router table, or -1 if could not be created
 */
static s8_t
nd6_new_router(ip6_addr_t * router_addr, struct netif * netif)
{
  s8_t router_index;
  s8_t neighbor_index;

  /* Do we have a neighbor entry for this router? */
  neighbor_index = nd6_find_neighbor_cache_entry(router_addr);
  if (neighbor_index < 0) {
    /* Create a neighbor entry for this router. */
    neighbor_index = nd6_new_neighbor_cache_entry();
    if (neighbor_index < 0) {
      /* Could not create neighbor entry for this router. */
      return -1;
    }
    ip6_addr_set(&(neighbor_cache[neighbor_index].next_hop_address), router_addr);
    neighbor_cache[neighbor_index].netif = netif;
    neighbor_cache[neighbor_index].q = NULL;
    neighbor_cache[neighbor_index].state = ND6_INCOMPLETE;
    neighbor_cache[neighbor_index].counter.probes_sent = 0;
  }

  /* Mark neighbor as router. */
  neighbor_cache[neighbor_index].isrouter = 1;

  /* Look for empty entry. */
  for (router_index = 0; router_index < LWIP_ND6_NUM_ROUTERS; router_index++) {
    if (default_router_list[router_index].neighbor_entry == NULL) {
      default_router_list[router_index].neighbor_entry = &(neighbor_cache[neighbor_index]);
      return router_index;
    }
  }

  /* Could not create a router entry. */

  /* Mark neighbor entry as not-router. Entry might be useful as neighbor still. */
  neighbor_cache[neighbor_index].isrouter = 0;

  /* router not found. */
  return -1;
}

/**
 * Find the cached entry for an on-link prefix.
 *
 * @param prefix the IPv6 prefix that is on-link
 * @param netif the netif on which the prefix is on-link
 * @return the index on the prefix table, or -1 if not found
 */
static s8_t
nd6_get_onlink_prefix(ip6_addr_t * prefix, struct netif * netif)
{
  s8_t i;

  /* Look for prefix in list. */
  for (i = 0; i < LWIP_ND6_NUM_PREFIXES; ++i) {
    if ((ip6_addr_netcmp(&(prefix_list[i].prefix), prefix)) &&
        (prefix_list[i].netif == netif)) {
      return i;
    }
  }

  /* Entry not available. */
  return -1;
}

/**
 * Creates a new entry for an on-link prefix.
 *
 * @param prefix the IPv6 prefix that is on-link
 * @param netif the netif on which the prefix is on-link
 * @return the index on the prefix table, or -1 if not created
 */
static s8_t
nd6_new_onlink_prefix(ip6_addr_t * prefix, struct netif * netif)
{
  s8_t i;

  /* Create new entry. */
  for (i = 0; i < LWIP_ND6_NUM_PREFIXES; ++i) {
    if ((prefix_list[i].netif == NULL) ||
        (prefix_list[i].invalidation_timer == 0)) {
      /* Found empty prefix entry. */
      prefix_list[i].netif = netif;
      ip6_addr_set(&(prefix_list[i].prefix), prefix);
#if LWIP_IPV6_AUTOCONFIG
      prefix_list[i].flags = 0;
#endif
      return i;
    }
  }

  /* Entry not available. */
  return -1;
}

/**
 * Determine the next hop for a destination. Will determine if the
 * destination is on-link, else a suitable on-link router is selected.
 *
 * The last entry index is cached for fast entry search.
 *
 * @param ip6addr the destination address
 * @param netif the netif on which the packet will be sent
 * @return the neighbor cache entry for the next hop, ERR_RTE if no
 *         suitable next hop was found, ERR_MEM if no cache entry
 *         could be created
 */
s8_t
nd6_get_next_hop_entry(ip6_addr_t * ip6addr, struct netif * netif)
{
  s8_t i;

#if LWIP_NETIF_HWADDRHINT
  if (netif->addr_hint != NULL) {
    /* per-pcb cached entry was given */
    u8_t addr_hint = *(netif->addr_hint);
    if (addr_hint < LWIP_ND6_NUM_DESTINATIONS) {
      nd6_cached_destination_index = addr_hint;
    }
  }
#endif /* LWIP_NETIF_HWADDRHINT */

  /* Look for ip6addr in destination cache. */
  if (ip6_addr_cmp(ip6addr, &(destination_cache[nd6_cached_destination_index].destination_addr))) {
    /* the cached entry index is the right one! */
    /* do nothing. */
    ND6_STATS_INC(nd6.cachehit);
  } else {
    /* Search destination cache. */
    i = nd6_find_destination_cache_entry(ip6addr);
    if (i >= 0) {
      /* found destination entry. make it our new cached index. */
      nd6_cached_destination_index = i;
    }
    else {
      /* Not found. Create a new destination entry. */
      i = nd6_new_destination_cache_entry();
      if (i >= 0) {
        /* got new destination entry. make it our new cached index. */
        nd6_cached_destination_index = i;
      } else {
        /* Could not create a destination cache entry. */
        return ERR_MEM;
      }

      /* Copy dest address to destination cache. */
      ip6_addr_set(&(destination_cache[nd6_cached_destination_index].destination_addr), ip6addr);

      /* Now find the next hop. is it a neighbor? */
      if (ip6_addr_islinklocal(ip6addr) ||
          nd6_is_prefix_in_netif(ip6addr, netif)) {
        /* Destination in local link. */
        destination_cache[nd6_cached_destination_index].pmtu = netif->mtu;
        ip6_addr_copy(destination_cache[nd6_cached_destination_index].next_hop_addr, destination_cache[nd6_cached_destination_index].destination_addr);
      }
      else {
        /* We need to select a router. */
        i = nd6_select_router(ip6addr, netif);
        if (i < 0) {
          /* No router found. */
          ip6_addr_set_any(&(destination_cache[nd6_cached_destination_index].destination_addr));
          return ERR_RTE;
        }
        destination_cache[nd6_cached_destination_index].pmtu = netif->mtu; /* Start with netif mtu, correct through ICMPv6 if necessary */
        ip6_addr_copy(destination_cache[nd6_cached_destination_index].next_hop_addr, default_router_list[i].neighbor_entry->next_hop_address);
      }
    }
  }

#if LWIP_NETIF_HWADDRHINT
  if (netif->addr_hint != NULL) {
    /* per-pcb cached entry was given */
    *(netif->addr_hint) = nd6_cached_destination_index;
  }
#endif /* LWIP_NETIF_HWADDRHINT */

  /* Look in neighbor cache for the next-hop address. */
  if (ip6_addr_cmp(&(destination_cache[nd6_cached_destination_index].next_hop_addr),
                   &(neighbor_cache[nd6_cached_neighbor_index].next_hop_address))) {
    /* Cache hit. */
    /* Do nothing. */
    ND6_STATS_INC(nd6.cachehit);
  } else {
    i = nd6_find_neighbor_cache_entry(&(destination_cache[nd6_cached_destination_index].next_hop_addr));
    if (i >= 0) {
      /* Found a matching record, make it new cached entry. */
      nd6_cached_neighbor_index = i;
    }
    else {
      /* Neighbor not in cache. Make a new entry. */
      i = nd6_new_neighbor_cache_entry();
      if (i >= 0) {
        /* got new neighbor entry. make it our new cached index. */
        nd6_cached_neighbor_index = i;
      } else {
        /* Could not create a neighbor cache entry. */
        return ERR_MEM;
      }

      /* Initialize fields. */
      ip6_addr_copy(neighbor_cache[i].next_hop_address,
                   destination_cache[nd6_cached_destination_index].next_hop_addr);
      neighbor_cache[i].isrouter = 0;
      neighbor_cache[i].netif = netif;
      neighbor_cache[i].state = ND6_INCOMPLETE;
      neighbor_cache[i].counter.probes_sent = 0;
    }
  }

  /* Reset this destination's age. */
  destination_cache[nd6_cached_destination_index].age = 0;

  return nd6_cached_neighbor_index;
}

/**
 * Queue a packet for a neighbor.
 *
 * @param neighbor_index the index in the neighbor cache table
 * @param q packet to be queued
 * @return ERR_OK if succeeded, ERR_MEM if out of memory
 */
err_t
nd6_queue_packet(s8_t neighbor_index, struct pbuf * q)
{
  err_t result = ERR_MEM;
  struct pbuf *p;
  int copy_needed = 0;
#if LWIP_ND6_QUEUEING
  struct nd6_q_entry *new_entry, *r;
#endif /* LWIP_ND6_QUEUEING */

  if ((neighbor_index < 0) || (neighbor_index >= LWIP_ND6_NUM_NEIGHBORS)) {
    return ERR_ARG;
  }

  /* IF q includes a PBUF_REF, PBUF_POOL or PBUF_RAM, we have no choice but
   * to copy the whole queue into a new PBUF_RAM (see bug #11400)
   * PBUF_ROMs can be left as they are, since ROM must not get changed. */
  p = q;
  while (p) {
    if(p->type != PBUF_ROM) {
      copy_needed = 1;
      break;
    }
    p = p->next;
  }
  if(copy_needed) {
    /* copy the whole packet into new pbufs */
    p = pbuf_alloc(PBUF_LINK, q->tot_len, PBUF_RAM);
    while ((p == NULL) && (neighbor_cache[neighbor_index].q != NULL)) {
      /* Free oldest packet (as per RFC recommendation) */
#if LWIP_ND6_QUEUEING
      r = neighbor_cache[neighbor_index].q;
      neighbor_cache[neighbor_index].q = r->next;
      r->next = NULL;
      nd6_free_q(r);
#else /* LWIP_ND6_QUEUEING */
      pbuf_free(neighbor_cache[neighbor_index].q);
      neighbor_cache[neighbor_index].q = NULL;
#endif /* LWIP_ND6_QUEUEING */
      p = pbuf_alloc(PBUF_LINK, q->tot_len, PBUF_RAM);
    }
    if(p != NULL) {
      if (pbuf_copy(p, q) != ERR_OK) {
        pbuf_free(p);
        p = NULL;
      }
    }
  } else {
    /* referencing the old pbuf is enough */
    p = q;
    pbuf_ref(p);
  }
  /* packet was copied/ref'd? */
  if (p != NULL) {
    /* queue packet ... */
#if LWIP_ND6_QUEUEING
    /* allocate a new nd6 queue entry */
    new_entry = (struct nd6_q_entry *)memp_malloc(MEMP_ND6_QUEUE);
    if ((new_entry == NULL) && (neighbor_cache[neighbor_index].q != NULL)) {
      /* Free oldest packet (as per RFC recommendation) */
      r = neighbor_cache[neighbor_index].q;
      neighbor_cache[neighbor_index].q = r->next;
      r->next = NULL;
      nd6_free_q(r);
      new_entry = (struct nd6_q_entry *)memp_malloc(MEMP_ND6_QUEUE);
    }
    if (new_entry != NULL) {
      new_entry->next = NULL;
      new_entry->p = p;
      if(neighbor_cache[neighbor_index].q != NULL) {
        /* queue was already existent, append the new entry to the end */
        r = neighbor_cache[neighbor_index].q;
        while (r->next != NULL) {
          r = r->next;
        }
        r->next = new_entry;
      } else {
        /* queue did not exist, first item in queue */
        neighbor_cache[neighbor_index].q = new_entry;
      }
      LWIP_DEBUGF(LWIP_DBG_TRACE, ("ipv6: queued packet %p on neighbor entry %"S16_F"\n", (void *)p, (s16_t)neighbor_index));
      result = ERR_OK;
    } else {
      /* the pool MEMP_ND6_QUEUE is empty */
      pbuf_free(p);
      LWIP_DEBUGF(LWIP_DBG_TRACE, ("ipv6: could not queue a copy of packet %p (out of memory)\n", (void *)p));
      /* { result == ERR_MEM } through initialization */
    }
#else /* LWIP_ND6_QUEUEING */
    /* Queue a single packet. If an older packet is already queued, free it as per RFC. */
    if (neighbor_cache[neighbor_index].q != NULL) {
      pbuf_free(neighbor_cache[neighbor_index].q);
    }
    neighbor_cache[neighbor_index].q = p;
    LWIP_DEBUGF(LWIP_DBG_TRACE, ("ipv6: queued packet %p on neighbor entry %"S16_F"\n", (void *)p, (s16_t)neighbor_index));
    result = ERR_OK;
#endif /* LWIP_ND6_QUEUEING */
  } else {
    LWIP_DEBUGF(LWIP_DBG_TRACE, ("ipv6: could not queue a copy of packet %p (out of memory)\n", (void *)q));
    /* { result == ERR_MEM } through initialization */
  }

  return result;
}

#if LWIP_ND6_QUEUEING
/**
 * Free a complete queue of nd6 q entries
 *
 * @param q a queue of nd6_q_entry to free
 */
static void
nd6_free_q(struct nd6_q_entry *q)
{
  struct nd6_q_entry *r;
  LWIP_ASSERT("q != NULL", q != NULL);
  LWIP_ASSERT("q->p != NULL", q->p != NULL);
  while (q) {
    r = q;
    q = q->next;
    LWIP_ASSERT("r->p != NULL", (r->p != NULL));
    pbuf_free(r->p);
    memp_free(MEMP_ND6_QUEUE, r);
  }
}
#endif /* LWIP_ND6_QUEUEING */

/**
 * Send queued packets for a neighbor
 *
 * @param i the neighbor to send packets to
 */
static void
nd6_send_q(s8_t i)
{
  struct ip6_hdr *ip6hdr;
#if LWIP_ND6_QUEUEING
  struct nd6_q_entry *q;
#endif /* LWIP_ND6_QUEUEING */

  if ((i < 0) || (i >= LWIP_ND6_NUM_NEIGHBORS)) {
    return;
  }

#if LWIP_ND6_QUEUEING
  while (neighbor_cache[i].q != NULL) {
    /* remember first in queue */
    q = neighbor_cache[i].q;
    /* pop first item off the queue */
    neighbor_cache[i].q = q->next;
    /* Get ipv6 header. */
    ip6hdr = (struct ip6_hdr *)(q->p->payload);
    /* Override ip6_current_dest_addr() so that we have an aligned copy. */
    ip6_addr_set(ip6_current_dest_addr(), &(ip6hdr->dest));
    /* send the queued IPv6 packet */
    (neighbor_cache[i].netif)->output_ip6(neighbor_cache[i].netif, q->p, ip6_current_dest_addr());
    /* free the queued IP packet */
    pbuf_free(q->p);
    /* now queue entry can be freed */
    memp_free(MEMP_ND6_QUEUE, q);
  }
#else /* LWIP_ND6_QUEUEING */
  if (neighbor_cache[i].q != NULL) {
    /* Get ipv6 header. */
    ip6hdr = (struct ip6_hdr *)(neighbor_cache[i].q->payload);
    /* Override ip6_current_dest_addr() so that we have an aligned copy. */
    ip6_addr_set(ip6_current_dest_addr(), &(ip6hdr->dest));
    /* send the queued IPv6 packet */
    (neighbor_cache[i].netif)->output_ip6(neighbor_cache[i].netif, neighbor_cache[i].q, ip6_current_dest_addr());
    /* free the queued IP packet */
    pbuf_free(neighbor_cache[i].q);
    neighbor_cache[i].q = NULL;
  }
#endif /* LWIP_ND6_QUEUEING */
}


/**
 * Get the Path MTU for a destination.
 *
 * @param ip6addr the destination address
 * @param netif the netif on which the packet will be sent
 * @return the Path MTU, if known, or the netif default MTU
 */
u16_t
nd6_get_destination_mtu(ip6_addr_t * ip6addr, struct netif * netif)
{
  s8_t i;

  i = nd6_find_destination_cache_entry(ip6addr);
  if (i >= 0) {
    if (destination_cache[i].pmtu > 0) {
      return destination_cache[i].pmtu;
    }
  }

  if (netif != NULL) {
    return netif->mtu;
  }

  return 1280; /* Minimum MTU */
}


#if LWIP_ND6_TCP_REACHABILITY_HINTS
/**
 * Provide the Neighbor discovery process with a hint that a
 * destination is reachable. Called by tcp_receive when ACKs are
 * received or sent (as per RFC). This is useful to avoid sending
 * NS messages every 30 seconds.
 *
 * @param ip6addr the destination address which is know to be reachable
 *                by an upper layer protocol (TCP)
 */
void
nd6_reachability_hint(ip6_addr_t * ip6addr)
{
  s8_t i;

  /* Find destination in cache. */
  if (ip6_addr_cmp(ip6addr, &(destination_cache[nd6_cached_destination_index].destination_addr))) {
    i = nd6_cached_destination_index;
    ND6_STATS_INC(nd6.cachehit);
  }
  else {
    i = nd6_find_destination_cache_entry(ip6addr);
  }
  if (i < 0) {
    return;
  }

  /* Find next hop neighbor in cache. */
  if (ip6_addr_cmp(&(destination_cache[i].next_hop_addr), &(neighbor_cache[nd6_cached_neighbor_index].next_hop_address))) {
    i = nd6_cached_neighbor_index;
    ND6_STATS_INC(nd6.cachehit);
  }
  else {
    i = nd6_find_neighbor_cache_entry(&(destination_cache[i].next_hop_addr));
  }
  if (i < 0) {
    return;
  }

  /* Set reachability state. */
  neighbor_cache[i].state = ND6_REACHABLE;
  neighbor_cache[i].counter.reachable_time = reachable_time;
}
#endif /* LWIP_ND6_TCP_REACHABILITY_HINTS */

#endif /* LWIP_IPV6 */
