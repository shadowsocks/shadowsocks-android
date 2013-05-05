/**
 * @file
 *
 * AutoIP Automatic LinkLocal IP Configuration
 */

/*
 *
 * Copyright (c) 2007 Dominik Spies <kontakt@dspies.de>
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
 * Author: Dominik Spies <kontakt@dspies.de>
 *
 * This is a AutoIP implementation for the lwIP TCP/IP stack. It aims to conform
 * with RFC 3927.
 *
 *
 * Please coordinate changes and requests with Dominik Spies
 * <kontakt@dspies.de>
 */
 
#ifndef __LWIP_AUTOIP_H__
#define __LWIP_AUTOIP_H__

#include "lwip/opt.h"

#if LWIP_AUTOIP /* don't build if not configured for use in lwipopts.h */

#include "lwip/netif.h"
#include "lwip/udp.h"
#include "netif/etharp.h"

#ifdef __cplusplus
extern "C" {
#endif

/* AutoIP Timing */
#define AUTOIP_TMR_INTERVAL      100
#define AUTOIP_TICKS_PER_SECOND (1000 / AUTOIP_TMR_INTERVAL)

/* RFC 3927 Constants */
#define PROBE_WAIT               1   /* second   (initial random delay)                 */
#define PROBE_MIN                1   /* second   (minimum delay till repeated probe)    */
#define PROBE_MAX                2   /* seconds  (maximum delay till repeated probe)    */
#define PROBE_NUM                3   /*          (number of probe packets)              */
#define ANNOUNCE_NUM             2   /*          (number of announcement packets)       */
#define ANNOUNCE_INTERVAL        2   /* seconds  (time between announcement packets)    */
#define ANNOUNCE_WAIT            2   /* seconds  (delay before announcing)              */
#define MAX_CONFLICTS            10  /*          (max conflicts before rate limiting)   */
#define RATE_LIMIT_INTERVAL      60  /* seconds  (delay between successive attempts)    */
#define DEFEND_INTERVAL          10  /* seconds  (min. wait between defensive ARPs)     */

/* AutoIP client states */
#define AUTOIP_STATE_OFF         0
#define AUTOIP_STATE_PROBING     1
#define AUTOIP_STATE_ANNOUNCING  2
#define AUTOIP_STATE_BOUND       3

struct autoip
{
  ip_addr_t llipaddr;       /* the currently selected, probed, announced or used LL IP-Address */
  u8_t state;               /* current AutoIP state machine state */
  u8_t sent_num;            /* sent number of probes or announces, dependent on state */
  u16_t ttw;                /* ticks to wait, tick is AUTOIP_TMR_INTERVAL long */
  u8_t lastconflict;        /* ticks until a conflict can be solved by defending */
  u8_t tried_llipaddr;      /* total number of probed/used Link Local IP-Addresses */
};


#define autoip_init() /* Compatibility define, no init needed. */

/** Set a struct autoip allocated by the application to work with */
void autoip_set_struct(struct netif *netif, struct autoip *autoip);

/** Start AutoIP client */
err_t autoip_start(struct netif *netif);

/** Stop AutoIP client */
err_t autoip_stop(struct netif *netif);

/** Handles every incoming ARP Packet, called by etharp_arp_input */
void autoip_arp_reply(struct netif *netif, struct etharp_hdr *hdr);

/** Has to be called in loop every AUTOIP_TMR_INTERVAL milliseconds */
void autoip_tmr(void);

/** Handle a possible change in the network configuration */
void autoip_network_changed(struct netif *netif);

#ifdef __cplusplus
}
#endif

#endif /* LWIP_AUTOIP */

#endif /* __LWIP_AUTOIP_H__ */
