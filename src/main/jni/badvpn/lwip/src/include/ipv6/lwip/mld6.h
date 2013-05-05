/**
 * @file
 *
 * Multicast listener discovery for IPv6. Aims to be compliant with RFC 2710.
 * No support for MLDv2.
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

#ifndef __LWIP_MLD6_H__
#define __LWIP_MLD6_H__

#include "lwip/opt.h"

#if LWIP_IPV6_MLD && LWIP_IPV6  /* don't build if not configured for use in lwipopts.h */

#include "lwip/pbuf.h"
#include "lwip/netif.h"


#ifdef __cplusplus
extern "C" {
#endif

struct mld_group {
  /** next link */
  struct mld_group *next;
  /** interface on which the group is active */
  struct netif      *netif;
  /** multicast address */
  ip6_addr_t         group_address;
  /** signifies we were the last person to report */
  u8_t               last_reporter_flag;
  /** current state of the group */
  u8_t               group_state;
  /** timer for reporting */
  u16_t              timer;
  /** counter of simultaneous uses */
  u8_t               use;
};

/** Multicast listener report/query/done message header. */
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/bpstruct.h"
#endif
PACK_STRUCT_BEGIN
struct mld_header {
  PACK_STRUCT_FIELD(u8_t type);
  PACK_STRUCT_FIELD(u8_t code);
  PACK_STRUCT_FIELD(u16_t chksum);
  PACK_STRUCT_FIELD(u16_t max_resp_delay);
  PACK_STRUCT_FIELD(u16_t reserved);
  PACK_STRUCT_FIELD(ip6_addr_p_t multicast_address);
  /* Options follow. */
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/epstruct.h"
#endif

#define MLD6_TMR_INTERVAL              100 /* Milliseconds */

/* MAC Filter Actions, these are passed to a netif's
 * mld_mac_filter callback function. */
#define MLD6_DEL_MAC_FILTER            0
#define MLD6_ADD_MAC_FILTER            1


#define mld6_init() /* TODO should we init tables? */
err_t  mld6_stop(struct netif *netif);
void   mld6_report_groups(struct netif *netif);
void   mld6_tmr(void);
struct mld_group *mld6_lookfor_group(struct netif *ifp, ip6_addr_t *addr);
void   mld6_input(struct pbuf *p, struct netif *inp);
err_t  mld6_joingroup(ip6_addr_t *srcaddr, ip6_addr_t *groupaddr);
err_t  mld6_leavegroup(ip6_addr_t *srcaddr, ip6_addr_t *groupaddr);


#ifdef __cplusplus
}
#endif

#endif /* LWIP_IPV6_MLD && LWIP_IPV6 */

#endif /* __LWIP_MLD6_H__ */
