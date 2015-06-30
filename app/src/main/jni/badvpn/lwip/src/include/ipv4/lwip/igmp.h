/*
 * Copyright (c) 2002 CITEL Technologies Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 * 3. Neither the name of CITEL Technologies Ltd nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY CITEL TECHNOLOGIES AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL CITEL TECHNOLOGIES OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 *
 * This file is a contribution to the lwIP TCP/IP stack.
 * The Swedish Institute of Computer Science and Adam Dunkels
 * are specifically granted permission to redistribute this
 * source code.
*/

#ifndef __LWIP_IGMP_H__
#define __LWIP_IGMP_H__

#include "lwip/opt.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"

#if LWIP_IGMP /* don't build if not configured for use in lwipopts.h */

#ifdef __cplusplus
extern "C" {
#endif


/* IGMP timer */
#define IGMP_TMR_INTERVAL              100 /* Milliseconds */
#define IGMP_V1_DELAYING_MEMBER_TMR   (1000/IGMP_TMR_INTERVAL)
#define IGMP_JOIN_DELAYING_MEMBER_TMR (500 /IGMP_TMR_INTERVAL)

/* MAC Filter Actions, these are passed to a netif's
 * igmp_mac_filter callback function. */
#define IGMP_DEL_MAC_FILTER            0
#define IGMP_ADD_MAC_FILTER            1


/**
 * igmp group structure - there is
 * a list of groups for each interface
 * these should really be linked from the interface, but
 * if we keep them separate we will not affect the lwip original code
 * too much
 * 
 * There will be a group for the all systems group address but this 
 * will not run the state machine as it is used to kick off reports
 * from all the other groups
 */
struct igmp_group {
  /** next link */
  struct igmp_group *next;
  /** interface on which the group is active */
  struct netif      *netif;
  /** multicast address */
  ip_addr_t          group_address;
  /** signifies we were the last person to report */
  u8_t               last_reporter_flag;
  /** current state of the group */
  u8_t               group_state;
  /** timer for reporting, negative is OFF */
  u16_t              timer;
  /** counter of simultaneous uses */
  u8_t               use;
};

/*  Prototypes */
void   igmp_init(void);
err_t  igmp_start(struct netif *netif);
err_t  igmp_stop(struct netif *netif);
void   igmp_report_groups(struct netif *netif);
struct igmp_group *igmp_lookfor_group(struct netif *ifp, ip_addr_t *addr);
void   igmp_input(struct pbuf *p, struct netif *inp, ip_addr_t *dest);
err_t  igmp_joingroup(ip_addr_t *ifaddr, ip_addr_t *groupaddr);
err_t  igmp_leavegroup(ip_addr_t *ifaddr, ip_addr_t *groupaddr);
void   igmp_tmr(void);

#ifdef __cplusplus
}
#endif

#endif /* LWIP_IGMP */

#endif /* __LWIP_IGMP_H__ */
