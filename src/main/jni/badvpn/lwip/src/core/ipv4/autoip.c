/**
 * @file
 * AutoIP Automatic LinkLocal IP Configuration
 *
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

/*******************************************************************************
 * USAGE:
 * 
 * define LWIP_AUTOIP 1  in your lwipopts.h
 * 
 * If you don't use tcpip.c (so, don't call, you don't call tcpip_init):
 * - First, call autoip_init().
 * - call autoip_tmr() all AUTOIP_TMR_INTERVAL msces,
 *   that should be defined in autoip.h.
 *   I recommend a value of 100. The value must divide 1000 with a remainder almost 0.
 *   Possible values are 1000, 500, 333, 250, 200, 166, 142, 125, 111, 100 ....
 *
 * Without DHCP:
 * - Call autoip_start() after netif_add().
 * 
 * With DHCP:
 * - define LWIP_DHCP_AUTOIP_COOP 1 in your lwipopts.h.
 * - Configure your DHCP Client.
 *
 */

#include "lwip/opt.h"

#if LWIP_AUTOIP /* don't build if not configured for use in lwipopts.h */

#include "lwip/mem.h"
#include "lwip/udp.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/autoip.h"
#include "netif/etharp.h"

#include <stdlib.h>
#include <string.h>

/* 169.254.0.0 */
#define AUTOIP_NET         0xA9FE0000
/* 169.254.1.0 */
#define AUTOIP_RANGE_START (AUTOIP_NET | 0x0100)
/* 169.254.254.255 */
#define AUTOIP_RANGE_END   (AUTOIP_NET | 0xFEFF)


/** Pseudo random macro based on netif informations.
 * You could use "rand()" from the C Library if you define LWIP_AUTOIP_RAND in lwipopts.h */
#ifndef LWIP_AUTOIP_RAND
#define LWIP_AUTOIP_RAND(netif) ( (((u32_t)((netif->hwaddr[5]) & 0xff) << 24) | \
                                   ((u32_t)((netif->hwaddr[3]) & 0xff) << 16) | \
                                   ((u32_t)((netif->hwaddr[2]) & 0xff) << 8) | \
                                   ((u32_t)((netif->hwaddr[4]) & 0xff))) + \
                                   (netif->autoip?netif->autoip->tried_llipaddr:0))
#endif /* LWIP_AUTOIP_RAND */

/**
 * Macro that generates the initial IP address to be tried by AUTOIP.
 * If you want to override this, define it to something else in lwipopts.h.
 */
#ifndef LWIP_AUTOIP_CREATE_SEED_ADDR
#define LWIP_AUTOIP_CREATE_SEED_ADDR(netif) \
  htonl(AUTOIP_RANGE_START + ((u32_t)(((u8_t)(netif->hwaddr[4])) | \
                 ((u32_t)((u8_t)(netif->hwaddr[5]))) << 8)))
#endif /* LWIP_AUTOIP_CREATE_SEED_ADDR */

/* static functions */
static void autoip_handle_arp_conflict(struct netif *netif);

/* creates a pseudo random LL IP-Address for a network interface */
static void autoip_create_addr(struct netif *netif, ip_addr_t *ipaddr);

/* sends an ARP probe */
static err_t autoip_arp_probe(struct netif *netif);

/* sends an ARP announce */
static err_t autoip_arp_announce(struct netif *netif);

/* configure interface for use with current LL IP-Address */
static err_t autoip_bind(struct netif *netif);

/* start sending probes for llipaddr */
static void autoip_start_probing(struct netif *netif);


/** Set a statically allocated struct autoip to work with.
 * Using this prevents autoip_start to allocate it using mem_malloc.
 *
 * @param netif the netif for which to set the struct autoip
 * @param dhcp (uninitialised) dhcp struct allocated by the application
 */
void
autoip_set_struct(struct netif *netif, struct autoip *autoip)
{
  LWIP_ASSERT("netif != NULL", netif != NULL);
  LWIP_ASSERT("autoip != NULL", autoip != NULL);
  LWIP_ASSERT("netif already has a struct autoip set", netif->autoip == NULL);

  /* clear data structure */
  memset(autoip, 0, sizeof(struct autoip));
  /* autoip->state = AUTOIP_STATE_OFF; */
  netif->autoip = autoip;
}

/** Restart AutoIP client and check the next address (conflict detected)
 *
 * @param netif The netif under AutoIP control
 */
static void
autoip_restart(struct netif *netif)
{
  netif->autoip->tried_llipaddr++;
  autoip_start(netif);
}

/**
 * Handle a IP address conflict after an ARP conflict detection
 */
static void
autoip_handle_arp_conflict(struct netif *netif)
{
  /* Somehow detect if we are defending or retreating */
  unsigned char defend = 1; /* tbd */

  if (defend) {
    if (netif->autoip->lastconflict > 0) {
      /* retreat, there was a conflicting ARP in the last
       * DEFEND_INTERVAL seconds
       */
      LWIP_DEBUGF(AUTOIP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE,
        ("autoip_handle_arp_conflict(): we are defending, but in DEFEND_INTERVAL, retreating\n"));

      /* TODO: close all TCP sessions */
      autoip_restart(netif);
    } else {
      LWIP_DEBUGF(AUTOIP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE,
        ("autoip_handle_arp_conflict(): we are defend, send ARP Announce\n"));
      autoip_arp_announce(netif);
      netif->autoip->lastconflict = DEFEND_INTERVAL * AUTOIP_TICKS_PER_SECOND;
    }
  } else {
    LWIP_DEBUGF(AUTOIP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE,
      ("autoip_handle_arp_conflict(): we do not defend, retreating\n"));
    /* TODO: close all TCP sessions */
    autoip_restart(netif);
  }
}

/**
 * Create an IP-Address out of range 169.254.1.0 to 169.254.254.255
 *
 * @param netif network interface on which create the IP-Address
 * @param ipaddr ip address to initialize
 */
static void
autoip_create_addr(struct netif *netif, ip_addr_t *ipaddr)
{
  /* Here we create an IP-Address out of range 169.254.1.0 to 169.254.254.255
   * compliant to RFC 3927 Section 2.1
   * We have 254 * 256 possibilities */

  u32_t addr = ntohl(LWIP_AUTOIP_CREATE_SEED_ADDR(netif));
  addr += netif->autoip->tried_llipaddr;
  addr = AUTOIP_NET | (addr & 0xffff);
  /* Now, 169.254.0.0 <= addr <= 169.254.255.255 */ 

  if (addr < AUTOIP_RANGE_START) {
    addr += AUTOIP_RANGE_END - AUTOIP_RANGE_START + 1;
  }
  if (addr > AUTOIP_RANGE_END) {
    addr -= AUTOIP_RANGE_END - AUTOIP_RANGE_START + 1;
  }
  LWIP_ASSERT("AUTOIP address not in range", (addr >= AUTOIP_RANGE_START) &&
    (addr <= AUTOIP_RANGE_END));
  ip4_addr_set_u32(ipaddr, htonl(addr));
  
  LWIP_DEBUGF(AUTOIP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE,
    ("autoip_create_addr(): tried_llipaddr=%"U16_F", %"U16_F".%"U16_F".%"U16_F".%"U16_F"\n",
    (u16_t)(netif->autoip->tried_llipaddr), ip4_addr1_16(ipaddr), ip4_addr2_16(ipaddr),
    ip4_addr3_16(ipaddr), ip4_addr4_16(ipaddr)));
}

/**
 * Sends an ARP probe from a network interface
 *
 * @param netif network interface used to send the probe
 */
static err_t
autoip_arp_probe(struct netif *netif)
{
  return etharp_raw(netif, (struct eth_addr *)netif->hwaddr, &ethbroadcast,
    (struct eth_addr *)netif->hwaddr, IP_ADDR_ANY, &ethzero,
    &netif->autoip->llipaddr, ARP_REQUEST);
}

/**
 * Sends an ARP announce from a network interface
 *
 * @param netif network interface used to send the announce
 */
static err_t
autoip_arp_announce(struct netif *netif)
{
  return etharp_raw(netif, (struct eth_addr *)netif->hwaddr, &ethbroadcast,
    (struct eth_addr *)netif->hwaddr, &netif->autoip->llipaddr, &ethzero,
    &netif->autoip->llipaddr, ARP_REQUEST);
}

/**
 * Configure interface for use with current LL IP-Address
 *
 * @param netif network interface to configure with current LL IP-Address
 */
static err_t
autoip_bind(struct netif *netif)
{
  struct autoip *autoip = netif->autoip;
  ip_addr_t sn_mask, gw_addr;

  LWIP_DEBUGF(AUTOIP_DEBUG | LWIP_DBG_TRACE,
    ("autoip_bind(netif=%p) %c%c%"U16_F" %"U16_F".%"U16_F".%"U16_F".%"U16_F"\n",
    (void*)netif, netif->name[0], netif->name[1], (u16_t)netif->num,
    ip4_addr1_16(&autoip->llipaddr), ip4_addr2_16(&autoip->llipaddr),
    ip4_addr3_16(&autoip->llipaddr), ip4_addr4_16(&autoip->llipaddr)));

  IP4_ADDR(&sn_mask, 255, 255, 0, 0);
  IP4_ADDR(&gw_addr, 0, 0, 0, 0);

  netif_set_ipaddr(netif, &autoip->llipaddr);
  netif_set_netmask(netif, &sn_mask);
  netif_set_gw(netif, &gw_addr);  

  /* bring the interface up */
  netif_set_up(netif);

  return ERR_OK;
}

/**
 * Start AutoIP client
 *
 * @param netif network interface on which start the AutoIP client
 */
err_t
autoip_start(struct netif *netif)
{
  struct autoip *autoip = netif->autoip;
  err_t result = ERR_OK;

  if (netif_is_up(netif)) {
    netif_set_down(netif);
  }

  /* Set IP-Address, Netmask and Gateway to 0 to make sure that
   * ARP Packets are formed correctly
   */
  ip_addr_set_zero(&netif->ip_addr);
  ip_addr_set_zero(&netif->netmask);
  ip_addr_set_zero(&netif->gw);

  LWIP_DEBUGF(AUTOIP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE,
    ("autoip_start(netif=%p) %c%c%"U16_F"\n", (void*)netif, netif->name[0],
    netif->name[1], (u16_t)netif->num));
  if (autoip == NULL) {
    /* no AutoIP client attached yet? */
    LWIP_DEBUGF(AUTOIP_DEBUG | LWIP_DBG_TRACE,
      ("autoip_start(): starting new AUTOIP client\n"));
    autoip = (struct autoip *)mem_malloc(sizeof(struct autoip));
    if (autoip == NULL) {
      LWIP_DEBUGF(AUTOIP_DEBUG | LWIP_DBG_TRACE,
        ("autoip_start(): could not allocate autoip\n"));
      return ERR_MEM;
    }
    memset(autoip, 0, sizeof(struct autoip));
    /* store this AutoIP client in the netif */
    netif->autoip = autoip;
    LWIP_DEBUGF(AUTOIP_DEBUG | LWIP_DBG_TRACE, ("autoip_start(): allocated autoip"));
  } else {
    autoip->state = AUTOIP_STATE_OFF;
    autoip->ttw = 0;
    autoip->sent_num = 0;
    ip_addr_set_zero(&autoip->llipaddr);
    autoip->lastconflict = 0;
  }

  autoip_create_addr(netif, &(autoip->llipaddr));
  autoip_start_probing(netif);

  return result;
}

static void
autoip_start_probing(struct netif *netif)
{
  struct autoip *autoip = netif->autoip;

  autoip->state = AUTOIP_STATE_PROBING;
  autoip->sent_num = 0;
  LWIP_DEBUGF(AUTOIP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE,
     ("autoip_start_probing(): changing state to PROBING: %"U16_F".%"U16_F".%"U16_F".%"U16_F"\n",
      ip4_addr1_16(&netif->autoip->llipaddr), ip4_addr2_16(&netif->autoip->llipaddr),
      ip4_addr3_16(&netif->autoip->llipaddr), ip4_addr4_16(&netif->autoip->llipaddr)));

  /* time to wait to first probe, this is randomly
   * choosen out of 0 to PROBE_WAIT seconds.
   * compliant to RFC 3927 Section 2.2.1
   */
  autoip->ttw = (u16_t)(LWIP_AUTOIP_RAND(netif) % (PROBE_WAIT * AUTOIP_TICKS_PER_SECOND));

  /*
   * if we tried more then MAX_CONFLICTS we must limit our rate for
   * accquiring and probing address
   * compliant to RFC 3927 Section 2.2.1
   */
  if (autoip->tried_llipaddr > MAX_CONFLICTS) {
    autoip->ttw = RATE_LIMIT_INTERVAL * AUTOIP_TICKS_PER_SECOND;
  }
}

/**
 * Handle a possible change in the network configuration.
 *
 * If there is an AutoIP address configured, take the interface down
 * and begin probing with the same address.
 */
void
autoip_network_changed(struct netif *netif)
{
  if (netif->autoip && netif->autoip->state != AUTOIP_STATE_OFF) {
    netif_set_down(netif);
    autoip_start_probing(netif);
  }
}

/**
 * Stop AutoIP client
 *
 * @param netif network interface on which stop the AutoIP client
 */
err_t
autoip_stop(struct netif *netif)
{
  netif->autoip->state = AUTOIP_STATE_OFF;
  netif_set_down(netif);
  return ERR_OK;
}

/**
 * Has to be called in loop every AUTOIP_TMR_INTERVAL milliseconds
 */
void
autoip_tmr()
{
  struct netif *netif = netif_list;
  /* loop through netif's */
  while (netif != NULL) {
    /* only act on AutoIP configured interfaces */
    if (netif->autoip != NULL) {
      if (netif->autoip->lastconflict > 0) {
        netif->autoip->lastconflict--;
      }

      LWIP_DEBUGF(AUTOIP_DEBUG | LWIP_DBG_TRACE,
        ("autoip_tmr() AutoIP-State: %"U16_F", ttw=%"U16_F"\n",
        (u16_t)(netif->autoip->state), netif->autoip->ttw));

      switch(netif->autoip->state) {
        case AUTOIP_STATE_PROBING:
          if (netif->autoip->ttw > 0) {
            netif->autoip->ttw--;
          } else {
            if (netif->autoip->sent_num >= PROBE_NUM) {
              netif->autoip->state = AUTOIP_STATE_ANNOUNCING;
              netif->autoip->sent_num = 0;
              netif->autoip->ttw = ANNOUNCE_WAIT * AUTOIP_TICKS_PER_SECOND;
              LWIP_DEBUGF(AUTOIP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE,
                 ("autoip_tmr(): changing state to ANNOUNCING: %"U16_F".%"U16_F".%"U16_F".%"U16_F"\n",
                  ip4_addr1_16(&netif->autoip->llipaddr), ip4_addr2_16(&netif->autoip->llipaddr),
                  ip4_addr3_16(&netif->autoip->llipaddr), ip4_addr4_16(&netif->autoip->llipaddr)));
            } else {
              autoip_arp_probe(netif);
              LWIP_DEBUGF(AUTOIP_DEBUG | LWIP_DBG_TRACE,
                ("autoip_tmr() PROBING Sent Probe\n"));
              netif->autoip->sent_num++;
              /* calculate time to wait to next probe */
              netif->autoip->ttw = (u16_t)((LWIP_AUTOIP_RAND(netif) %
                ((PROBE_MAX - PROBE_MIN) * AUTOIP_TICKS_PER_SECOND) ) +
                PROBE_MIN * AUTOIP_TICKS_PER_SECOND);
            }
          }
          break;

        case AUTOIP_STATE_ANNOUNCING:
          if (netif->autoip->ttw > 0) {
            netif->autoip->ttw--;
          } else {
            if (netif->autoip->sent_num == 0) {
             /* We are here the first time, so we waited ANNOUNCE_WAIT seconds
              * Now we can bind to an IP address and use it.
              *
              * autoip_bind calls netif_set_up. This triggers a gratuitous ARP
              * which counts as an announcement.
              */
              autoip_bind(netif);
            } else {
              autoip_arp_announce(netif);
              LWIP_DEBUGF(AUTOIP_DEBUG | LWIP_DBG_TRACE,
                ("autoip_tmr() ANNOUNCING Sent Announce\n"));
            }
            netif->autoip->ttw = ANNOUNCE_INTERVAL * AUTOIP_TICKS_PER_SECOND;
            netif->autoip->sent_num++;

            if (netif->autoip->sent_num >= ANNOUNCE_NUM) {
                netif->autoip->state = AUTOIP_STATE_BOUND;
                netif->autoip->sent_num = 0;
                netif->autoip->ttw = 0;
                 LWIP_DEBUGF(AUTOIP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE,
                    ("autoip_tmr(): changing state to BOUND: %"U16_F".%"U16_F".%"U16_F".%"U16_F"\n",
                     ip4_addr1_16(&netif->autoip->llipaddr), ip4_addr2_16(&netif->autoip->llipaddr),
                     ip4_addr3_16(&netif->autoip->llipaddr), ip4_addr4_16(&netif->autoip->llipaddr)));
            }
          }
          break;
      }
    }
    /* proceed to next network interface */
    netif = netif->next;
  }
}

/**
 * Handles every incoming ARP Packet, called by etharp_arp_input.
 *
 * @param netif network interface to use for autoip processing
 * @param hdr Incoming ARP packet
 */
void
autoip_arp_reply(struct netif *netif, struct etharp_hdr *hdr)
{
  LWIP_DEBUGF(AUTOIP_DEBUG | LWIP_DBG_TRACE, ("autoip_arp_reply()\n"));
  if ((netif->autoip != NULL) && (netif->autoip->state != AUTOIP_STATE_OFF)) {
   /* when ip.src == llipaddr && hw.src != netif->hwaddr
    *
    * when probing  ip.dst == llipaddr && hw.src != netif->hwaddr
    * we have a conflict and must solve it
    */
    ip_addr_t sipaddr, dipaddr;
    struct eth_addr netifaddr;
    ETHADDR16_COPY(netifaddr.addr, netif->hwaddr);

    /* Copy struct ip_addr2 to aligned ip_addr, to support compilers without
     * structure packing (not using structure copy which breaks strict-aliasing rules).
     */
    IPADDR2_COPY(&sipaddr, &hdr->sipaddr);
    IPADDR2_COPY(&dipaddr, &hdr->dipaddr);
      
    if ((netif->autoip->state == AUTOIP_STATE_PROBING) ||
        ((netif->autoip->state == AUTOIP_STATE_ANNOUNCING) &&
         (netif->autoip->sent_num == 0))) {
     /* RFC 3927 Section 2.2.1:
      * from beginning to after ANNOUNCE_WAIT
      * seconds we have a conflict if
      * ip.src == llipaddr OR
      * ip.dst == llipaddr && hw.src != own hwaddr
      */
      if ((ip_addr_cmp(&sipaddr, &netif->autoip->llipaddr)) ||
          (ip_addr_cmp(&dipaddr, &netif->autoip->llipaddr) &&
           !eth_addr_cmp(&netifaddr, &hdr->shwaddr))) {
        LWIP_DEBUGF(AUTOIP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE | LWIP_DBG_LEVEL_WARNING,
          ("autoip_arp_reply(): Probe Conflict detected\n"));
        autoip_restart(netif);
      }
    } else {
     /* RFC 3927 Section 2.5:
      * in any state we have a conflict if
      * ip.src == llipaddr && hw.src != own hwaddr
      */
      if (ip_addr_cmp(&sipaddr, &netif->autoip->llipaddr) &&
          !eth_addr_cmp(&netifaddr, &hdr->shwaddr)) {
        LWIP_DEBUGF(AUTOIP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE | LWIP_DBG_LEVEL_WARNING,
          ("autoip_arp_reply(): Conflicting ARP-Packet detected\n"));
        autoip_handle_arp_conflict(netif);
      }
    }
  }
}

#endif /* LWIP_AUTOIP */
