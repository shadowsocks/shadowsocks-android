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

#ifndef __LWIP_ND6_H__
#define __LWIP_ND6_H__

#include "lwip/opt.h"

#if LWIP_IPV6  /* don't build if not configured for use in lwipopts.h */

#include "lwip/pbuf.h"
#include "lwip/ip6.h"
#include "lwip/ip6_addr.h"
#include "lwip/netif.h"


#ifdef __cplusplus
extern "C" {
#endif

/* Struct for tables. */
struct nd6_neighbor_cache_entry {
  ip6_addr_t next_hop_address;
  struct netif * netif;
  u8_t lladdr[NETIF_MAX_HWADDR_LEN];
  /*u32_t pmtu;*/
#if LWIP_ND6_QUEUEING
  /** Pointer to queue of pending outgoing packets on this entry. */
  struct nd6_q_entry *q;
#else /* LWIP_ND6_QUEUEING */
  /** Pointer to a single pending outgoing packet on this entry. */
  struct pbuf *q;
#endif /* LWIP_ND6_QUEUEING */
  u8_t state;
  u8_t isrouter;
  union {
    u32_t reachable_time;
    u32_t delay_time;
    u32_t probes_sent;
    u32_t stale_time;
  } counter;
};

struct nd6_destination_cache_entry {
  ip6_addr_t destination_addr;
  ip6_addr_t next_hop_addr;
  u32_t pmtu;
  u32_t age;
};

struct nd6_prefix_list_entry {
  ip6_addr_t prefix;
  struct netif * netif;
  u32_t invalidation_timer;
#if LWIP_IPV6_AUTOCONFIG
  u8_t flags;
#define ND6_PREFIX_AUTOCONFIG_AUTONOMOUS 0x01
#define ND6_PREFIX_AUTOCONFIG_ADDRESS_GENERATED 0x02
#define ND6_PREFIX_AUTOCONFIG_ADDRESS_DUPLICATE 0x04
#endif /* LWIP_IPV6_AUTOCONFIG */
};

struct nd6_router_list_entry {
  struct nd6_neighbor_cache_entry * neighbor_entry;
  u32_t invalidation_timer;
  u8_t flags;
};


enum nd6_neighbor_cache_entry_state {
  ND6_NO_ENTRY = 0,
  ND6_INCOMPLETE,
  ND6_REACHABLE,
  ND6_STALE,
  ND6_DELAY,
  ND6_PROBE
};

#if LWIP_ND6_QUEUEING
/** struct for queueing outgoing packets for unknown address
  * defined here to be accessed by memp.h
  */
struct nd6_q_entry {
  struct nd6_q_entry *next;
  struct pbuf *p;
};
#endif /* LWIP_ND6_QUEUEING */

/** Neighbor solicitation message header. */
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/bpstruct.h"
#endif
PACK_STRUCT_BEGIN
struct ns_header {
  PACK_STRUCT_FIELD(u8_t type);
  PACK_STRUCT_FIELD(u8_t code);
  PACK_STRUCT_FIELD(u16_t chksum);
  PACK_STRUCT_FIELD(u32_t reserved);
  PACK_STRUCT_FIELD(ip6_addr_p_t target_address);
  /* Options follow. */
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/epstruct.h"
#endif

/** Neighbor advertisement message header. */
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/bpstruct.h"
#endif
PACK_STRUCT_BEGIN
struct na_header {
  PACK_STRUCT_FIELD(u8_t type);
  PACK_STRUCT_FIELD(u8_t code);
  PACK_STRUCT_FIELD(u16_t chksum);
  PACK_STRUCT_FIELD(u8_t flags);
  PACK_STRUCT_FIELD(u8_t reserved[3]);
  PACK_STRUCT_FIELD(ip6_addr_p_t target_address);
  /* Options follow. */
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/epstruct.h"
#endif
#define ND6_FLAG_ROUTER      (0x80)
#define ND6_FLAG_SOLICITED   (0x40)
#define ND6_FLAG_OVERRIDE    (0x20)

/** Router solicitation message header. */
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/bpstruct.h"
#endif
PACK_STRUCT_BEGIN
struct rs_header {
  PACK_STRUCT_FIELD(u8_t type);
  PACK_STRUCT_FIELD(u8_t code);
  PACK_STRUCT_FIELD(u16_t chksum);
  PACK_STRUCT_FIELD(u32_t reserved);
  /* Options follow. */
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/epstruct.h"
#endif

/** Router advertisement message header. */
#define ND6_RA_FLAG_MANAGED_ADDR_CONFIG (0x80)
#define ND6_RA_FLAG_OTHER_STATEFUL_CONFIG (0x40)
#define ND6_RA_FLAG_HOME_AGENT (0x20)
#define ND6_RA_PREFERENCE_MASK (0x18)
#define ND6_RA_PREFERENCE_HIGH (0x08)
#define ND6_RA_PREFERENCE_MEDIUM (0x00)
#define ND6_RA_PREFERENCE_LOW (0x18)
#define ND6_RA_PREFERENCE_DISABLED (0x10)
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/bpstruct.h"
#endif
PACK_STRUCT_BEGIN
struct ra_header {
  PACK_STRUCT_FIELD(u8_t type);
  PACK_STRUCT_FIELD(u8_t code);
  PACK_STRUCT_FIELD(u16_t chksum);
  PACK_STRUCT_FIELD(u8_t current_hop_limit);
  PACK_STRUCT_FIELD(u8_t flags);
  PACK_STRUCT_FIELD(u16_t router_lifetime);
  PACK_STRUCT_FIELD(u32_t reachable_time);
  PACK_STRUCT_FIELD(u32_t retrans_timer);
  /* Options follow. */
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/epstruct.h"
#endif

/** Redirect message header. */
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/bpstruct.h"
#endif
PACK_STRUCT_BEGIN
struct redirect_header {
  PACK_STRUCT_FIELD(u8_t type);
  PACK_STRUCT_FIELD(u8_t code);
  PACK_STRUCT_FIELD(u16_t chksum);
  PACK_STRUCT_FIELD(u32_t reserved);
  PACK_STRUCT_FIELD(ip6_addr_p_t target_address);
  PACK_STRUCT_FIELD(ip6_addr_p_t destination_address);
  /* Options follow. */
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/epstruct.h"
#endif

/** Link-layer address option. */
#define ND6_OPTION_TYPE_SOURCE_LLADDR (0x01)
#define ND6_OPTION_TYPE_TARGET_LLADDR (0x02)
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/bpstruct.h"
#endif
PACK_STRUCT_BEGIN
struct lladdr_option {
  PACK_STRUCT_FIELD(u8_t type);
  PACK_STRUCT_FIELD(u8_t length);
  PACK_STRUCT_FIELD(u8_t addr[NETIF_MAX_HWADDR_LEN]);
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/epstruct.h"
#endif

/** Prefix information option. */
#define ND6_OPTION_TYPE_PREFIX_INFO (0x03)
#define ND6_PREFIX_FLAG_ON_LINK (0x80)
#define ND6_PREFIX_FLAG_AUTONOMOUS (0x40)
#define ND6_PREFIX_FLAG_ROUTER_ADDRESS (0x20)
#define ND6_PREFIX_FLAG_SITE_PREFIX (0x10)
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/bpstruct.h"
#endif
PACK_STRUCT_BEGIN
struct prefix_option {
  PACK_STRUCT_FIELD(u8_t type);
  PACK_STRUCT_FIELD(u8_t length);
  PACK_STRUCT_FIELD(u8_t prefix_length);
  PACK_STRUCT_FIELD(u8_t flags);
  PACK_STRUCT_FIELD(u32_t valid_lifetime);
  PACK_STRUCT_FIELD(u32_t preferred_lifetime);
  PACK_STRUCT_FIELD(u8_t reserved2[3]);
  PACK_STRUCT_FIELD(u8_t site_prefix_length);
  PACK_STRUCT_FIELD(ip6_addr_p_t prefix);
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/epstruct.h"
#endif

/** Redirected header option. */
#define ND6_OPTION_TYPE_REDIR_HDR (0x04)
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/bpstruct.h"
#endif
PACK_STRUCT_BEGIN
struct redirected_header_option {
  PACK_STRUCT_FIELD(u8_t type);
  PACK_STRUCT_FIELD(u8_t length);
  PACK_STRUCT_FIELD(u8_t reserved[6]);
  /* Portion of redirected packet follows. */
  /* PACK_STRUCT_FIELD(u8_t redirected[8]); */
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/epstruct.h"
#endif

/** MTU option. */
#define ND6_OPTION_TYPE_MTU (0x05)
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/bpstruct.h"
#endif
PACK_STRUCT_BEGIN
struct mtu_option {
  PACK_STRUCT_FIELD(u8_t type);
  PACK_STRUCT_FIELD(u8_t length);
  PACK_STRUCT_FIELD(u16_t reserved);
  PACK_STRUCT_FIELD(u32_t mtu);
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/epstruct.h"
#endif

/** Route information option. */
#define ND6_OPTION_TYPE_ROUTE_INFO (24)
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/bpstruct.h"
#endif
PACK_STRUCT_BEGIN
struct route_option {
  PACK_STRUCT_FIELD(u8_t type);
  PACK_STRUCT_FIELD(u8_t length);
  PACK_STRUCT_FIELD(u8_t prefix_length);
  PACK_STRUCT_FIELD(u8_t preference);
  PACK_STRUCT_FIELD(u32_t route_lifetime);
  PACK_STRUCT_FIELD(ip6_addr_p_t prefix);
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/epstruct.h"
#endif

/* the possible states of an IP address */
#define IP6_ADDRESS_STATE_INVALID     (0)
#define IP6_ADDRESS_STATE_VALID       (0x4)
#define IP6_ADDRESS_STATE_PREFERRED   (0x5) /* includes valid */
#define IP6_ADDRESS_STATE_DEPRECATED  (0x6) /* includes valid */
#define IP6_ADDRESS_STATE_TENTATIV    (0x8)

/** 1 second period */
#define ND6_TMR_INTERVAL 1000

/* Router tables. */
/* TODO make these static? and entries accessible through API? */
extern struct nd6_neighbor_cache_entry neighbor_cache[];
extern struct nd6_destination_cache_entry destination_cache[];
extern struct nd6_prefix_list_entry prefix_list[];
extern struct nd6_router_list_entry default_router_list[];

/* Default values, can be updated by a RA message. */
extern u32_t reachable_time;
extern u32_t retrans_timer;

#define nd6_init() /* TODO should we init tables? */
void nd6_tmr(void);
void nd6_input(struct pbuf *p, struct netif *inp);
s8_t nd6_get_next_hop_entry(ip6_addr_t * ip6addr, struct netif * netif);
s8_t nd6_select_router(ip6_addr_t * ip6addr, struct netif * netif);
u16_t nd6_get_destination_mtu(ip6_addr_t * ip6addr, struct netif * netif);
err_t nd6_queue_packet(s8_t neighbor_index, struct pbuf * p);
#if LWIP_ND6_TCP_REACHABILITY_HINTS
void nd6_reachability_hint(ip6_addr_t * ip6addr);
#endif /* LWIP_ND6_TCP_REACHABILITY_HINTS */

#ifdef __cplusplus
}
#endif

#endif /* LWIP_IPV6 */

#endif /* __LWIP_ND6_H__ */
