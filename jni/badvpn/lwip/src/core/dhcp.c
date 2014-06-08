/**
 * @file
 * Dynamic Host Configuration Protocol client
 *
 */

/*
 *
 * Copyright (c) 2001-2004 Leon Woestenberg <leon.woestenberg@gmx.net>
 * Copyright (c) 2001-2004 Axon Digital Design B.V., The Netherlands.
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
 * This file is a contribution to the lwIP TCP/IP stack.
 * The Swedish Institute of Computer Science and Adam Dunkels
 * are specifically granted permission to redistribute this
 * source code.
 *
 * Author: Leon Woestenberg <leon.woestenberg@gmx.net>
 *
 * This is a DHCP client for the lwIP TCP/IP stack. It aims to conform
 * with RFC 2131 and RFC 2132.
 *
 * TODO:
 * - Support for interfaces other than Ethernet (SLIP, PPP, ...)
 *
 * Please coordinate changes and requests with Leon Woestenberg
 * <leon.woestenberg@gmx.net>
 *
 * Integration with your code:
 *
 * In lwip/dhcp.h
 * #define DHCP_COARSE_TIMER_SECS (recommended 60 which is a minute)
 * #define DHCP_FINE_TIMER_MSECS (recommended 500 which equals TCP coarse timer)
 *
 * Then have your application call dhcp_coarse_tmr() and
 * dhcp_fine_tmr() on the defined intervals.
 *
 * dhcp_start(struct netif *netif);
 * starts a DHCP client instance which configures the interface by
 * obtaining an IP address lease and maintaining it.
 *
 * Use dhcp_release(netif) to end the lease and use dhcp_stop(netif)
 * to remove the DHCP client.
 *
 */

#include "lwip/opt.h"

#if LWIP_DHCP /* don't build if not configured for use in lwipopts.h */

#include "lwip/stats.h"
#include "lwip/mem.h"
#include "lwip/udp.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/def.h"
#include "lwip/dhcp.h"
#include "lwip/autoip.h"
#include "lwip/dns.h"
#include "netif/etharp.h"

#include <string.h>

/** DHCP_CREATE_RAND_XID: if this is set to 1, the xid is created using
 * LWIP_RAND() (this overrides DHCP_GLOBAL_XID)
 */
#ifndef DHCP_CREATE_RAND_XID
#define DHCP_CREATE_RAND_XID 1
#endif

/** Default for DHCP_GLOBAL_XID is 0xABCD0000
 * This can be changed by defining DHCP_GLOBAL_XID and DHCP_GLOBAL_XID_HEADER, e.g.
 *  #define DHCP_GLOBAL_XID_HEADER "stdlib.h"
 *  #define DHCP_GLOBAL_XID rand()
 */
#ifdef DHCP_GLOBAL_XID_HEADER
#include DHCP_GLOBAL_XID_HEADER /* include optional starting XID generation prototypes */
#endif

/** DHCP_OPTION_MAX_MSG_SIZE is set to the MTU
 * MTU is checked to be big enough in dhcp_start */
#define DHCP_MAX_MSG_LEN(netif)        (netif->mtu)
#define DHCP_MAX_MSG_LEN_MIN_REQUIRED  576
/** Minimum length for reply before packet is parsed */
#define DHCP_MIN_REPLY_LEN             44

#define REBOOT_TRIES 2

/** Option handling: options are parsed in dhcp_parse_reply
 * and saved in an array where other functions can load them from.
 * This might be moved into the struct dhcp (not necessarily since
 * lwIP is single-threaded and the array is only used while in recv
 * callback). */
#define DHCP_OPTION_IDX_OVERLOAD    0
#define DHCP_OPTION_IDX_MSG_TYPE    1
#define DHCP_OPTION_IDX_SERVER_ID   2
#define DHCP_OPTION_IDX_LEASE_TIME  3
#define DHCP_OPTION_IDX_T1          4
#define DHCP_OPTION_IDX_T2          5
#define DHCP_OPTION_IDX_SUBNET_MASK 6
#define DHCP_OPTION_IDX_ROUTER      7
#define DHCP_OPTION_IDX_DNS_SERVER  8
#define DHCP_OPTION_IDX_MAX         (DHCP_OPTION_IDX_DNS_SERVER + DNS_MAX_SERVERS)

/** Holds the decoded option values, only valid while in dhcp_recv.
    @todo: move this into struct dhcp? */
u32_t dhcp_rx_options_val[DHCP_OPTION_IDX_MAX];
/** Holds a flag which option was received and is contained in dhcp_rx_options_val,
    only valid while in dhcp_recv.
    @todo: move this into struct dhcp? */
u8_t  dhcp_rx_options_given[DHCP_OPTION_IDX_MAX];

#ifdef DHCP_GLOBAL_XID
static u32_t xid;
static u8_t xid_initialised;
#endif /* DHCP_GLOBAL_XID */

#define dhcp_option_given(dhcp, idx)          (dhcp_rx_options_given[idx] != 0)
#define dhcp_got_option(dhcp, idx)            (dhcp_rx_options_given[idx] = 1)
#define dhcp_clear_option(dhcp, idx)          (dhcp_rx_options_given[idx] = 0)
#define dhcp_clear_all_options(dhcp)          (memset(dhcp_rx_options_given, 0, sizeof(dhcp_rx_options_given)))
#define dhcp_get_option_value(dhcp, idx)      (dhcp_rx_options_val[idx])
#define dhcp_set_option_value(dhcp, idx, val) (dhcp_rx_options_val[idx] = (val))


/* DHCP client state machine functions */
static err_t dhcp_discover(struct netif *netif);
static err_t dhcp_select(struct netif *netif);
static void dhcp_bind(struct netif *netif);
#if DHCP_DOES_ARP_CHECK
static err_t dhcp_decline(struct netif *netif);
#endif /* DHCP_DOES_ARP_CHECK */
static err_t dhcp_rebind(struct netif *netif);
static err_t dhcp_reboot(struct netif *netif);
static void dhcp_set_state(struct dhcp *dhcp, u8_t new_state);

/* receive, unfold, parse and free incoming messages */
static void dhcp_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, ip_addr_t *addr, u16_t port);

/* set the DHCP timers */
static void dhcp_timeout(struct netif *netif);
static void dhcp_t1_timeout(struct netif *netif);
static void dhcp_t2_timeout(struct netif *netif);

/* build outgoing messages */
/* create a DHCP message, fill in common headers */
static err_t dhcp_create_msg(struct netif *netif, struct dhcp *dhcp, u8_t message_type);
/* free a DHCP request */
static void dhcp_delete_msg(struct dhcp *dhcp);
/* add a DHCP option (type, then length in bytes) */
static void dhcp_option(struct dhcp *dhcp, u8_t option_type, u8_t option_len);
/* add option values */
static void dhcp_option_byte(struct dhcp *dhcp, u8_t value);
static void dhcp_option_short(struct dhcp *dhcp, u16_t value);
static void dhcp_option_long(struct dhcp *dhcp, u32_t value);
#if LWIP_NETIF_HOSTNAME
static void dhcp_option_hostname(struct dhcp *dhcp, struct netif *netif);
#endif /* LWIP_NETIF_HOSTNAME */
/* always add the DHCP options trailer to end and pad */
static void dhcp_option_trailer(struct dhcp *dhcp);

/**
 * Back-off the DHCP client (because of a received NAK response).
 *
 * Back-off the DHCP client because of a received NAK. Receiving a
 * NAK means the client asked for something non-sensible, for
 * example when it tries to renew a lease obtained on another network.
 *
 * We clear any existing set IP address and restart DHCP negotiation
 * afresh (as per RFC2131 3.2.3).
 *
 * @param netif the netif under DHCP control
 */
static void
dhcp_handle_nak(struct netif *netif)
{
  struct dhcp *dhcp = netif->dhcp;
  LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("dhcp_handle_nak(netif=%p) %c%c%"U16_F"\n", 
    (void*)netif, netif->name[0], netif->name[1], (u16_t)netif->num));
  /* Set the interface down since the address must no longer be used, as per RFC2131 */
  netif_set_down(netif);
  /* remove IP address from interface */
  netif_set_ipaddr(netif, IP_ADDR_ANY);
  netif_set_gw(netif, IP_ADDR_ANY);
  netif_set_netmask(netif, IP_ADDR_ANY); 
  /* Change to a defined state */
  dhcp_set_state(dhcp, DHCP_BACKING_OFF);
  /* We can immediately restart discovery */
  dhcp_discover(netif);
}

#if DHCP_DOES_ARP_CHECK
/**
 * Checks if the offered IP address is already in use.
 *
 * It does so by sending an ARP request for the offered address and
 * entering CHECKING state. If no ARP reply is received within a small
 * interval, the address is assumed to be free for use by us.
 *
 * @param netif the netif under DHCP control
 */
static void
dhcp_check(struct netif *netif)
{
  struct dhcp *dhcp = netif->dhcp;
  err_t result;
  u16_t msecs;
  LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("dhcp_check(netif=%p) %c%c\n", (void *)netif, (s16_t)netif->name[0],
    (s16_t)netif->name[1]));
  dhcp_set_state(dhcp, DHCP_CHECKING);
  /* create an ARP query for the offered IP address, expecting that no host
     responds, as the IP address should not be in use. */
  result = etharp_query(netif, &dhcp->offered_ip_addr, NULL);
  if (result != ERR_OK) {
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_WARNING, ("dhcp_check: could not perform ARP query\n"));
  }
  dhcp->tries++;
  msecs = 500;
  dhcp->request_timeout = (msecs + DHCP_FINE_TIMER_MSECS - 1) / DHCP_FINE_TIMER_MSECS;
  LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE, ("dhcp_check(): set request timeout %"U16_F" msecs\n", msecs));
}
#endif /* DHCP_DOES_ARP_CHECK */

/**
 * Remember the configuration offered by a DHCP server.
 *
 * @param netif the netif under DHCP control
 */
static void
dhcp_handle_offer(struct netif *netif)
{
  struct dhcp *dhcp = netif->dhcp;
  LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("dhcp_handle_offer(netif=%p) %c%c%"U16_F"\n",
    (void*)netif, netif->name[0], netif->name[1], (u16_t)netif->num));
  /* obtain the server address */
  if (dhcp_option_given(dhcp, DHCP_OPTION_IDX_SERVER_ID)) {
    ip4_addr_set_u32(&dhcp->server_ip_addr, htonl(dhcp_get_option_value(dhcp, DHCP_OPTION_IDX_SERVER_ID)));
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_STATE, ("dhcp_handle_offer(): server 0x%08"X32_F"\n",
      ip4_addr_get_u32(&dhcp->server_ip_addr)));
    /* remember offered address */
    ip_addr_copy(dhcp->offered_ip_addr, dhcp->msg_in->yiaddr);
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_STATE, ("dhcp_handle_offer(): offer for 0x%08"X32_F"\n",
      ip4_addr_get_u32(&dhcp->offered_ip_addr)));

    dhcp_select(netif);
  } else {
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_SERIOUS,
      ("dhcp_handle_offer(netif=%p) did not get server ID!\n", (void*)netif));
  }
}

/**
 * Select a DHCP server offer out of all offers.
 *
 * Simply select the first offer received.
 *
 * @param netif the netif under DHCP control
 * @return lwIP specific error (see error.h)
 */
static err_t
dhcp_select(struct netif *netif)
{
  struct dhcp *dhcp = netif->dhcp;
  err_t result;
  u16_t msecs;

  LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("dhcp_select(netif=%p) %c%c%"U16_F"\n", (void*)netif, netif->name[0], netif->name[1], (u16_t)netif->num));
  dhcp_set_state(dhcp, DHCP_REQUESTING);

  /* create and initialize the DHCP message header */
  result = dhcp_create_msg(netif, dhcp, DHCP_REQUEST);
  if (result == ERR_OK) {
    dhcp_option(dhcp, DHCP_OPTION_MAX_MSG_SIZE, DHCP_OPTION_MAX_MSG_SIZE_LEN);
    dhcp_option_short(dhcp, DHCP_MAX_MSG_LEN(netif));

    /* MUST request the offered IP address */
    dhcp_option(dhcp, DHCP_OPTION_REQUESTED_IP, 4);
    dhcp_option_long(dhcp, ntohl(ip4_addr_get_u32(&dhcp->offered_ip_addr)));

    dhcp_option(dhcp, DHCP_OPTION_SERVER_ID, 4);
    dhcp_option_long(dhcp, ntohl(ip4_addr_get_u32(&dhcp->server_ip_addr)));

    dhcp_option(dhcp, DHCP_OPTION_PARAMETER_REQUEST_LIST, 4/*num options*/);
    dhcp_option_byte(dhcp, DHCP_OPTION_SUBNET_MASK);
    dhcp_option_byte(dhcp, DHCP_OPTION_ROUTER);
    dhcp_option_byte(dhcp, DHCP_OPTION_BROADCAST);
    dhcp_option_byte(dhcp, DHCP_OPTION_DNS_SERVER);

#if LWIP_NETIF_HOSTNAME
    dhcp_option_hostname(dhcp, netif);
#endif /* LWIP_NETIF_HOSTNAME */

    dhcp_option_trailer(dhcp);
    /* shrink the pbuf to the actual content length */
    pbuf_realloc(dhcp->p_out, sizeof(struct dhcp_msg) - DHCP_OPTIONS_LEN + dhcp->options_out_len);

    /* send broadcast to any DHCP server */
    udp_sendto_if(dhcp->pcb, dhcp->p_out, IP_ADDR_BROADCAST, DHCP_SERVER_PORT, netif);
    dhcp_delete_msg(dhcp);
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE, ("dhcp_select: REQUESTING\n"));
  } else {
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_WARNING, ("dhcp_select: could not allocate DHCP request\n"));
  }
  dhcp->tries++;
  msecs = (dhcp->tries < 6 ? 1 << dhcp->tries : 60) * 1000;
  dhcp->request_timeout = (msecs + DHCP_FINE_TIMER_MSECS - 1) / DHCP_FINE_TIMER_MSECS;
  LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_STATE, ("dhcp_select(): set request timeout %"U16_F" msecs\n", msecs));
  return result;
}

/**
 * The DHCP timer that checks for lease renewal/rebind timeouts.
 */
void
dhcp_coarse_tmr()
{
  struct netif *netif = netif_list;
  LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("dhcp_coarse_tmr()\n"));
  /* iterate through all network interfaces */
  while (netif != NULL) {
    /* only act on DHCP configured interfaces */
    if (netif->dhcp != NULL) {
      /* timer is active (non zero), and triggers (zeroes) now? */
      if (netif->dhcp->t2_timeout-- == 1) {
        LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE, ("dhcp_coarse_tmr(): t2 timeout\n"));
        /* this clients' rebind timeout triggered */
        dhcp_t2_timeout(netif);
      /* timer is active (non zero), and triggers (zeroes) now */
      } else if (netif->dhcp->t1_timeout-- == 1) {
        LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE, ("dhcp_coarse_tmr(): t1 timeout\n"));
        /* this clients' renewal timeout triggered */
        dhcp_t1_timeout(netif);
      }
    }
    /* proceed to next netif */
    netif = netif->next;
  }
}

/**
 * DHCP transaction timeout handling
 *
 * A DHCP server is expected to respond within a short period of time.
 * This timer checks whether an outstanding DHCP request is timed out.
 */
void
dhcp_fine_tmr()
{
  struct netif *netif = netif_list;
  /* loop through netif's */
  while (netif != NULL) {
    /* only act on DHCP configured interfaces */
    if (netif->dhcp != NULL) {
      /* timer is active (non zero), and is about to trigger now */      
      if (netif->dhcp->request_timeout > 1) {
        netif->dhcp->request_timeout--;
      }
      else if (netif->dhcp->request_timeout == 1) {
        netif->dhcp->request_timeout--;
        /* { netif->dhcp->request_timeout == 0 } */
        LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE, ("dhcp_fine_tmr(): request timeout\n"));
        /* this client's request timeout triggered */
        dhcp_timeout(netif);
      }
    }
    /* proceed to next network interface */
    netif = netif->next;
  }
}

/**
 * A DHCP negotiation transaction, or ARP request, has timed out.
 *
 * The timer that was started with the DHCP or ARP request has
 * timed out, indicating no response was received in time.
 *
 * @param netif the netif under DHCP control
 */
static void
dhcp_timeout(struct netif *netif)
{
  struct dhcp *dhcp = netif->dhcp;
  LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("dhcp_timeout()\n"));
  /* back-off period has passed, or server selection timed out */
  if ((dhcp->state == DHCP_BACKING_OFF) || (dhcp->state == DHCP_SELECTING)) {
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("dhcp_timeout(): restarting discovery\n"));
    dhcp_discover(netif);
  /* receiving the requested lease timed out */
  } else if (dhcp->state == DHCP_REQUESTING) {
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE, ("dhcp_timeout(): REQUESTING, DHCP request timed out\n"));
    if (dhcp->tries <= 5) {
      dhcp_select(netif);
    } else {
      LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE, ("dhcp_timeout(): REQUESTING, releasing, restarting\n"));
      dhcp_release(netif);
      dhcp_discover(netif);
    }
#if DHCP_DOES_ARP_CHECK
  /* received no ARP reply for the offered address (which is good) */
  } else if (dhcp->state == DHCP_CHECKING) {
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE, ("dhcp_timeout(): CHECKING, ARP request timed out\n"));
    if (dhcp->tries <= 1) {
      dhcp_check(netif);
    /* no ARP replies on the offered address,
       looks like the IP address is indeed free */
    } else {
      /* bind the interface to the offered address */
      dhcp_bind(netif);
    }
#endif /* DHCP_DOES_ARP_CHECK */
  }
  /* did not get response to renew request? */
  else if (dhcp->state == DHCP_RENEWING) {
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE, ("dhcp_timeout(): RENEWING, DHCP request timed out\n"));
    /* just retry renewal */
    /* note that the rebind timer will eventually time-out if renew does not work */
    dhcp_renew(netif);
  /* did not get response to rebind request? */
  } else if (dhcp->state == DHCP_REBINDING) {
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE, ("dhcp_timeout(): REBINDING, DHCP request timed out\n"));
    if (dhcp->tries <= 8) {
      dhcp_rebind(netif);
    } else {
      LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE, ("dhcp_timeout(): RELEASING, DISCOVERING\n"));
      dhcp_release(netif);
      dhcp_discover(netif);
    }
  } else if (dhcp->state == DHCP_REBOOTING) {
    if (dhcp->tries < REBOOT_TRIES) {
      dhcp_reboot(netif);
    } else {
      dhcp_discover(netif);
    }
  }
}

/**
 * The renewal period has timed out.
 *
 * @param netif the netif under DHCP control
 */
static void
dhcp_t1_timeout(struct netif *netif)
{
  struct dhcp *dhcp = netif->dhcp;
  LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_STATE, ("dhcp_t1_timeout()\n"));
  if ((dhcp->state == DHCP_REQUESTING) || (dhcp->state == DHCP_BOUND) ||
      (dhcp->state == DHCP_RENEWING)) {
    /* just retry to renew - note that the rebind timer (t2) will
     * eventually time-out if renew tries fail. */
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE,
                ("dhcp_t1_timeout(): must renew\n"));
    /* This slightly different to RFC2131: DHCPREQUEST will be sent from state
       DHCP_RENEWING, not DHCP_BOUND */
    dhcp_renew(netif);
  }
}

/**
 * The rebind period has timed out.
 *
 * @param netif the netif under DHCP control
 */
static void
dhcp_t2_timeout(struct netif *netif)
{
  struct dhcp *dhcp = netif->dhcp;
  LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE, ("dhcp_t2_timeout()\n"));
  if ((dhcp->state == DHCP_REQUESTING) || (dhcp->state == DHCP_BOUND) ||
      (dhcp->state == DHCP_RENEWING)) {
    /* just retry to rebind */
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE,
                ("dhcp_t2_timeout(): must rebind\n"));
    /* This slightly different to RFC2131: DHCPREQUEST will be sent from state
       DHCP_REBINDING, not DHCP_BOUND */
    dhcp_rebind(netif);
  }
}

/**
 * Handle a DHCP ACK packet
 *
 * @param netif the netif under DHCP control
 */
static void
dhcp_handle_ack(struct netif *netif)
{
  struct dhcp *dhcp = netif->dhcp;
#if LWIP_DNS
  u8_t n;
#endif /* LWIP_DNS */

  /* clear options we might not get from the ACK */
  ip_addr_set_zero(&dhcp->offered_sn_mask);
  ip_addr_set_zero(&dhcp->offered_gw_addr);
#if LWIP_DHCP_BOOTP_FILE
  ip_addr_set_zero(&dhcp->offered_si_addr);
#endif /* LWIP_DHCP_BOOTP_FILE */

  /* lease time given? */
  if (dhcp_option_given(dhcp, DHCP_OPTION_IDX_LEASE_TIME)) {
    /* remember offered lease time */
    dhcp->offered_t0_lease = dhcp_get_option_value(dhcp, DHCP_OPTION_IDX_LEASE_TIME);
  }
  /* renewal period given? */
  if (dhcp_option_given(dhcp, DHCP_OPTION_IDX_T1)) {
    /* remember given renewal period */
    dhcp->offered_t1_renew = dhcp_get_option_value(dhcp, DHCP_OPTION_IDX_T1);
  } else {
    /* calculate safe periods for renewal */
    dhcp->offered_t1_renew = dhcp->offered_t0_lease / 2;
  }

  /* renewal period given? */
  if (dhcp_option_given(dhcp, DHCP_OPTION_IDX_T2)) {
    /* remember given rebind period */
    dhcp->offered_t2_rebind = dhcp_get_option_value(dhcp, DHCP_OPTION_IDX_T2);
  } else {
    /* calculate safe periods for rebinding */
    dhcp->offered_t2_rebind = dhcp->offered_t0_lease;
  }

  /* (y)our internet address */
  ip_addr_copy(dhcp->offered_ip_addr, dhcp->msg_in->yiaddr);

#if LWIP_DHCP_BOOTP_FILE
  /* copy boot server address,
     boot file name copied in dhcp_parse_reply if not overloaded */
  ip_addr_copy(dhcp->offered_si_addr, dhcp->msg_in->siaddr);
#endif /* LWIP_DHCP_BOOTP_FILE */

  /* subnet mask given? */
  if (dhcp_option_given(dhcp, DHCP_OPTION_IDX_SUBNET_MASK)) {
    /* remember given subnet mask */
    ip4_addr_set_u32(&dhcp->offered_sn_mask, htonl(dhcp_get_option_value(dhcp, DHCP_OPTION_IDX_SUBNET_MASK)));
    dhcp->subnet_mask_given = 1;
  } else {
    dhcp->subnet_mask_given = 0;
  }

  /* gateway router */
  if (dhcp_option_given(dhcp, DHCP_OPTION_IDX_ROUTER)) {
    ip4_addr_set_u32(&dhcp->offered_gw_addr, htonl(dhcp_get_option_value(dhcp, DHCP_OPTION_IDX_ROUTER)));
  }
  
#if LWIP_DNS
  /* DNS servers */
  for(n = 0; (n < DNS_MAX_SERVERS) && dhcp_option_given(dhcp, DHCP_OPTION_IDX_DNS_SERVER + n); n++) {
    ip_addr_t dns_addr;
    ip4_addr_set_u32(&dns_addr, htonl(dhcp_get_option_value(dhcp, DHCP_OPTION_IDX_DNS_SERVER + n)));
    dns_setserver(n, &dns_addr);
  }
#endif /* LWIP_DNS */
}

/** Set a statically allocated struct dhcp to work with.
 * Using this prevents dhcp_start to allocate it using mem_malloc.
 *
 * @param netif the netif for which to set the struct dhcp
 * @param dhcp (uninitialised) dhcp struct allocated by the application
 */
void
dhcp_set_struct(struct netif *netif, struct dhcp *dhcp)
{
  LWIP_ASSERT("netif != NULL", netif != NULL);
  LWIP_ASSERT("dhcp != NULL", dhcp != NULL);
  LWIP_ASSERT("netif already has a struct dhcp set", netif->dhcp == NULL);

  /* clear data structure */
  memset(dhcp, 0, sizeof(struct dhcp));
  /* dhcp_set_state(&dhcp, DHCP_OFF); */
  netif->dhcp = dhcp;
}

/** Removes a struct dhcp from a netif.
 *
 * ATTENTION: Only use this when not using dhcp_set_struct() to allocate the
 *            struct dhcp since the memory is passed back to the heap.
 *
 * @param netif the netif from which to remove the struct dhcp
 */
void dhcp_cleanup(struct netif *netif)
{
  LWIP_ASSERT("netif != NULL", netif != NULL);

  if (netif->dhcp != NULL) {
    mem_free(netif->dhcp);
    netif->dhcp = NULL;
  }
}

/**
 * Start DHCP negotiation for a network interface.
 *
 * If no DHCP client instance was attached to this interface,
 * a new client is created first. If a DHCP client instance
 * was already present, it restarts negotiation.
 *
 * @param netif The lwIP network interface
 * @return lwIP error code
 * - ERR_OK - No error
 * - ERR_MEM - Out of memory
 */
err_t
dhcp_start(struct netif *netif)
{
  struct dhcp *dhcp;
  err_t result = ERR_OK;

  LWIP_ERROR("netif != NULL", (netif != NULL), return ERR_ARG;);
  dhcp = netif->dhcp;
  LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE, ("dhcp_start(netif=%p) %c%c%"U16_F"\n", (void*)netif, netif->name[0], netif->name[1], (u16_t)netif->num));
  /* Remove the flag that says this netif is handled by DHCP,
     it is set when we succeeded starting. */
  netif->flags &= ~NETIF_FLAG_DHCP;

  /* check hwtype of the netif */
  if ((netif->flags & NETIF_FLAG_ETHARP) == 0) {
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("dhcp_start(): No ETHARP netif\n"));
    return ERR_ARG;
  }

  /* check MTU of the netif */
  if (netif->mtu < DHCP_MAX_MSG_LEN_MIN_REQUIRED) {
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("dhcp_start(): Cannot use this netif with DHCP: MTU is too small\n"));
    return ERR_MEM;
  }

  /* no DHCP client attached yet? */
  if (dhcp == NULL) {
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("dhcp_start(): starting new DHCP client\n"));
    dhcp = (struct dhcp *)mem_malloc(sizeof(struct dhcp));
    if (dhcp == NULL) {
      LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("dhcp_start(): could not allocate dhcp\n"));
      return ERR_MEM;
    }
    /* store this dhcp client in the netif */
    netif->dhcp = dhcp;
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("dhcp_start(): allocated dhcp"));
  /* already has DHCP client attached */
  } else {
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE, ("dhcp_start(): restarting DHCP configuration\n"));
    if (dhcp->pcb != NULL) {
      udp_remove(dhcp->pcb);
    }
    LWIP_ASSERT("pbuf p_out wasn't freed", dhcp->p_out == NULL);
    LWIP_ASSERT("reply wasn't freed", dhcp->msg_in == NULL );
  }
    
  /* clear data structure */
  memset(dhcp, 0, sizeof(struct dhcp));
  /* dhcp_set_state(&dhcp, DHCP_OFF); */
  /* allocate UDP PCB */
  dhcp->pcb = udp_new();
  if (dhcp->pcb == NULL) {
    LWIP_DEBUGF(DHCP_DEBUG  | LWIP_DBG_TRACE, ("dhcp_start(): could not obtain pcb\n"));
    return ERR_MEM;
  }
  ip_set_option(dhcp->pcb, SOF_BROADCAST);
  /* set up local and remote port for the pcb */
  udp_bind(dhcp->pcb, IP_ADDR_ANY, DHCP_CLIENT_PORT);
  udp_connect(dhcp->pcb, IP_ADDR_ANY, DHCP_SERVER_PORT);
  /* set up the recv callback and argument */
  udp_recv(dhcp->pcb, dhcp_recv, netif);
  LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("dhcp_start(): starting DHCP configuration\n"));
  /* (re)start the DHCP negotiation */
  result = dhcp_discover(netif);
  if (result != ERR_OK) {
    /* free resources allocated above */
    dhcp_stop(netif);
    return ERR_MEM;
  }
  /* Set the flag that says this netif is handled by DHCP. */
  netif->flags |= NETIF_FLAG_DHCP;
  return result;
}

/**
 * Inform a DHCP server of our manual configuration.
 *
 * This informs DHCP servers of our fixed IP address configuration
 * by sending an INFORM message. It does not involve DHCP address
 * configuration, it is just here to be nice to the network.
 *
 * @param netif The lwIP network interface
 */
void
dhcp_inform(struct netif *netif)
{
  struct dhcp dhcp;
  err_t result = ERR_OK;
  struct udp_pcb *pcb;

  LWIP_ERROR("netif != NULL", (netif != NULL), return;);

  memset(&dhcp, 0, sizeof(struct dhcp));
  dhcp_set_state(&dhcp, DHCP_INFORM);

  if ((netif->dhcp != NULL) && (netif->dhcp->pcb != NULL)) {
    /* re-use existing pcb */
    pcb = netif->dhcp->pcb;
  } else {
    pcb = udp_new();
    if (pcb == NULL) {
      LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_SERIOUS, ("dhcp_inform(): could not obtain pcb"));
      return;
    }
    dhcp.pcb = pcb;
    ip_set_option(dhcp.pcb, SOF_BROADCAST);
    udp_bind(dhcp.pcb, IP_ADDR_ANY, DHCP_CLIENT_PORT);
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("dhcp_inform(): created new udp pcb\n"));
  }
  /* create and initialize the DHCP message header */
  result = dhcp_create_msg(netif, &dhcp, DHCP_INFORM);
  if (result == ERR_OK) {
    dhcp_option(&dhcp, DHCP_OPTION_MAX_MSG_SIZE, DHCP_OPTION_MAX_MSG_SIZE_LEN);
    dhcp_option_short(&dhcp, DHCP_MAX_MSG_LEN(netif));

    dhcp_option_trailer(&dhcp);

    pbuf_realloc(dhcp.p_out, sizeof(struct dhcp_msg) - DHCP_OPTIONS_LEN + dhcp.options_out_len);

    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE, ("dhcp_inform: INFORMING\n"));
    udp_sendto_if(pcb, dhcp.p_out, IP_ADDR_BROADCAST, DHCP_SERVER_PORT, netif);
    dhcp_delete_msg(&dhcp);
  } else {
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_SERIOUS, ("dhcp_inform: could not allocate DHCP request\n"));
  }

  if (dhcp.pcb != NULL) {
    /* otherwise, the existing pcb was used */
    udp_remove(dhcp.pcb);
  }
}

/** Handle a possible change in the network configuration.
 *
 * This enters the REBOOTING state to verify that the currently bound
 * address is still valid.
 */
void
dhcp_network_changed(struct netif *netif)
{
  struct dhcp *dhcp = netif->dhcp;
  if (!dhcp)
    return;
  switch (dhcp->state) {
  case DHCP_REBINDING:
  case DHCP_RENEWING:
  case DHCP_BOUND:
  case DHCP_REBOOTING:
    netif_set_down(netif);
    dhcp->tries = 0;
    dhcp_reboot(netif);
    break;
  case DHCP_OFF:
    /* stay off */
    break;
  default:
    dhcp->tries = 0;
#if LWIP_DHCP_AUTOIP_COOP
    if(dhcp->autoip_coop_state == DHCP_AUTOIP_COOP_STATE_ON) {
      autoip_stop(netif);
      dhcp->autoip_coop_state = DHCP_AUTOIP_COOP_STATE_OFF;
    }
#endif /* LWIP_DHCP_AUTOIP_COOP */
    dhcp_discover(netif);
    break;
  }
}

#if DHCP_DOES_ARP_CHECK
/**
 * Match an ARP reply with the offered IP address.
 *
 * @param netif the network interface on which the reply was received
 * @param addr The IP address we received a reply from
 */
void dhcp_arp_reply(struct netif *netif, ip_addr_t *addr)
{
  LWIP_ERROR("netif != NULL", (netif != NULL), return;);
  LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("dhcp_arp_reply()\n"));
  /* is a DHCP client doing an ARP check? */
  if ((netif->dhcp != NULL) && (netif->dhcp->state == DHCP_CHECKING)) {
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE, ("dhcp_arp_reply(): CHECKING, arp reply for 0x%08"X32_F"\n",
      ip4_addr_get_u32(addr)));
    /* did a host respond with the address we
       were offered by the DHCP server? */
    if (ip_addr_cmp(addr, &netif->dhcp->offered_ip_addr)) {
      /* we will not accept the offered address */
      LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE | LWIP_DBG_LEVEL_WARNING,
        ("dhcp_arp_reply(): arp reply matched with offered address, declining\n"));
      dhcp_decline(netif);
    }
  }
}

/**
 * Decline an offered lease.
 *
 * Tell the DHCP server we do not accept the offered address.
 * One reason to decline the lease is when we find out the address
 * is already in use by another host (through ARP).
 *
 * @param netif the netif under DHCP control
 */
static err_t
dhcp_decline(struct netif *netif)
{
  struct dhcp *dhcp = netif->dhcp;
  err_t result = ERR_OK;
  u16_t msecs;
  LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("dhcp_decline()\n"));
  dhcp_set_state(dhcp, DHCP_BACKING_OFF);
  /* create and initialize the DHCP message header */
  result = dhcp_create_msg(netif, dhcp, DHCP_DECLINE);
  if (result == ERR_OK) {
    dhcp_option(dhcp, DHCP_OPTION_REQUESTED_IP, 4);
    dhcp_option_long(dhcp, ntohl(ip4_addr_get_u32(&dhcp->offered_ip_addr)));

    dhcp_option_trailer(dhcp);
    /* resize pbuf to reflect true size of options */
    pbuf_realloc(dhcp->p_out, sizeof(struct dhcp_msg) - DHCP_OPTIONS_LEN + dhcp->options_out_len);

    /* per section 4.4.4, broadcast DECLINE messages */
    udp_sendto_if(dhcp->pcb, dhcp->p_out, IP_ADDR_BROADCAST, DHCP_SERVER_PORT, netif);
    dhcp_delete_msg(dhcp);
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE, ("dhcp_decline: BACKING OFF\n"));
  } else {
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_SERIOUS,
      ("dhcp_decline: could not allocate DHCP request\n"));
  }
  dhcp->tries++;
  msecs = 10*1000;
  dhcp->request_timeout = (msecs + DHCP_FINE_TIMER_MSECS - 1) / DHCP_FINE_TIMER_MSECS;
  LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("dhcp_decline(): set request timeout %"U16_F" msecs\n", msecs));
  return result;
}
#endif /* DHCP_DOES_ARP_CHECK */


/**
 * Start the DHCP process, discover a DHCP server.
 *
 * @param netif the netif under DHCP control
 */
static err_t
dhcp_discover(struct netif *netif)
{
  struct dhcp *dhcp = netif->dhcp;
  err_t result = ERR_OK;
  u16_t msecs;
  LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("dhcp_discover()\n"));
  ip_addr_set_any(&dhcp->offered_ip_addr);
  dhcp_set_state(dhcp, DHCP_SELECTING);
  /* create and initialize the DHCP message header */
  result = dhcp_create_msg(netif, dhcp, DHCP_DISCOVER);
  if (result == ERR_OK) {
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("dhcp_discover: making request\n"));

    dhcp_option(dhcp, DHCP_OPTION_MAX_MSG_SIZE, DHCP_OPTION_MAX_MSG_SIZE_LEN);
    dhcp_option_short(dhcp, DHCP_MAX_MSG_LEN(netif));

    dhcp_option(dhcp, DHCP_OPTION_PARAMETER_REQUEST_LIST, 4/*num options*/);
    dhcp_option_byte(dhcp, DHCP_OPTION_SUBNET_MASK);
    dhcp_option_byte(dhcp, DHCP_OPTION_ROUTER);
    dhcp_option_byte(dhcp, DHCP_OPTION_BROADCAST);
    dhcp_option_byte(dhcp, DHCP_OPTION_DNS_SERVER);

    dhcp_option_trailer(dhcp);

    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("dhcp_discover: realloc()ing\n"));
    pbuf_realloc(dhcp->p_out, sizeof(struct dhcp_msg) - DHCP_OPTIONS_LEN + dhcp->options_out_len);

    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("dhcp_discover: sendto(DISCOVER, IP_ADDR_BROADCAST, DHCP_SERVER_PORT)\n"));
    udp_sendto_if(dhcp->pcb, dhcp->p_out, IP_ADDR_BROADCAST, DHCP_SERVER_PORT, netif);
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("dhcp_discover: deleting()ing\n"));
    dhcp_delete_msg(dhcp);
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE, ("dhcp_discover: SELECTING\n"));
  } else {
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_SERIOUS, ("dhcp_discover: could not allocate DHCP request\n"));
  }
  dhcp->tries++;
#if LWIP_DHCP_AUTOIP_COOP
  if(dhcp->tries >= LWIP_DHCP_AUTOIP_COOP_TRIES && dhcp->autoip_coop_state == DHCP_AUTOIP_COOP_STATE_OFF) {
    dhcp->autoip_coop_state = DHCP_AUTOIP_COOP_STATE_ON;
    autoip_start(netif);
  }
#endif /* LWIP_DHCP_AUTOIP_COOP */
  msecs = (dhcp->tries < 6 ? 1 << dhcp->tries : 60) * 1000;
  dhcp->request_timeout = (msecs + DHCP_FINE_TIMER_MSECS - 1) / DHCP_FINE_TIMER_MSECS;
  LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE, ("dhcp_discover(): set request timeout %"U16_F" msecs\n", msecs));
  return result;
}


/**
 * Bind the interface to the offered IP address.
 *
 * @param netif network interface to bind to the offered address
 */
static void
dhcp_bind(struct netif *netif)
{
  u32_t timeout;
  struct dhcp *dhcp;
  ip_addr_t sn_mask, gw_addr;
  LWIP_ERROR("dhcp_bind: netif != NULL", (netif != NULL), return;);
  dhcp = netif->dhcp;
  LWIP_ERROR("dhcp_bind: dhcp != NULL", (dhcp != NULL), return;);
  LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("dhcp_bind(netif=%p) %c%c%"U16_F"\n", (void*)netif, netif->name[0], netif->name[1], (u16_t)netif->num));

  /* temporary DHCP lease? */
  if (dhcp->offered_t1_renew != 0xffffffffUL) {
    /* set renewal period timer */
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("dhcp_bind(): t1 renewal timer %"U32_F" secs\n", dhcp->offered_t1_renew));
    timeout = (dhcp->offered_t1_renew + DHCP_COARSE_TIMER_SECS / 2) / DHCP_COARSE_TIMER_SECS;
    if(timeout > 0xffff) {
      timeout = 0xffff;
    }
    dhcp->t1_timeout = (u16_t)timeout;
    if (dhcp->t1_timeout == 0) {
      dhcp->t1_timeout = 1;
    }
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE, ("dhcp_bind(): set request timeout %"U32_F" msecs\n", dhcp->offered_t1_renew*1000));
  }
  /* set renewal period timer */
  if (dhcp->offered_t2_rebind != 0xffffffffUL) {
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("dhcp_bind(): t2 rebind timer %"U32_F" secs\n", dhcp->offered_t2_rebind));
    timeout = (dhcp->offered_t2_rebind + DHCP_COARSE_TIMER_SECS / 2) / DHCP_COARSE_TIMER_SECS;
    if(timeout > 0xffff) {
      timeout = 0xffff;
    }
    dhcp->t2_timeout = (u16_t)timeout;
    if (dhcp->t2_timeout == 0) {
      dhcp->t2_timeout = 1;
    }
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE, ("dhcp_bind(): set request timeout %"U32_F" msecs\n", dhcp->offered_t2_rebind*1000));
  }

  /* If we have sub 1 minute lease, t2 and t1 will kick in at the same time. */
  if ((dhcp->t1_timeout >= dhcp->t2_timeout) && (dhcp->t2_timeout > 0)) {
    dhcp->t1_timeout = 0;
  }

  if (dhcp->subnet_mask_given) {
    /* copy offered network mask */
    ip_addr_copy(sn_mask, dhcp->offered_sn_mask);
  } else {
    /* subnet mask not given, choose a safe subnet mask given the network class */
    u8_t first_octet = ip4_addr1(&dhcp->offered_ip_addr);
    if (first_octet <= 127) {
      ip4_addr_set_u32(&sn_mask, PP_HTONL(0xff000000UL));
    } else if (first_octet >= 192) {
      ip4_addr_set_u32(&sn_mask, PP_HTONL(0xffffff00UL));
    } else {
      ip4_addr_set_u32(&sn_mask, PP_HTONL(0xffff0000UL));
    }
  }

  ip_addr_copy(gw_addr, dhcp->offered_gw_addr);
  /* gateway address not given? */
  if (ip_addr_isany(&gw_addr)) {
    /* copy network address */
    ip_addr_get_network(&gw_addr, &dhcp->offered_ip_addr, &sn_mask);
    /* use first host address on network as gateway */
    ip4_addr_set_u32(&gw_addr, ip4_addr_get_u32(&gw_addr) | PP_HTONL(0x00000001UL));
  }

#if LWIP_DHCP_AUTOIP_COOP
  if(dhcp->autoip_coop_state == DHCP_AUTOIP_COOP_STATE_ON) {
    autoip_stop(netif);
    dhcp->autoip_coop_state = DHCP_AUTOIP_COOP_STATE_OFF;
  }
#endif /* LWIP_DHCP_AUTOIP_COOP */

  LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_STATE, ("dhcp_bind(): IP: 0x%08"X32_F"\n",
    ip4_addr_get_u32(&dhcp->offered_ip_addr)));
  netif_set_ipaddr(netif, &dhcp->offered_ip_addr);
  LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_STATE, ("dhcp_bind(): SN: 0x%08"X32_F"\n",
    ip4_addr_get_u32(&sn_mask)));
  netif_set_netmask(netif, &sn_mask);
  LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_STATE, ("dhcp_bind(): GW: 0x%08"X32_F"\n",
    ip4_addr_get_u32(&gw_addr)));
  netif_set_gw(netif, &gw_addr);
  /* bring the interface up */
  netif_set_up(netif);
  /* netif is now bound to DHCP leased address */
  dhcp_set_state(dhcp, DHCP_BOUND);
}

/**
 * Renew an existing DHCP lease at the involved DHCP server.
 *
 * @param netif network interface which must renew its lease
 */
err_t
dhcp_renew(struct netif *netif)
{
  struct dhcp *dhcp = netif->dhcp;
  err_t result;
  u16_t msecs;
  LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("dhcp_renew()\n"));
  dhcp_set_state(dhcp, DHCP_RENEWING);

  /* create and initialize the DHCP message header */
  result = dhcp_create_msg(netif, dhcp, DHCP_REQUEST);
  if (result == ERR_OK) {
    dhcp_option(dhcp, DHCP_OPTION_MAX_MSG_SIZE, DHCP_OPTION_MAX_MSG_SIZE_LEN);
    dhcp_option_short(dhcp, DHCP_MAX_MSG_LEN(netif));

#if 0
    dhcp_option(dhcp, DHCP_OPTION_REQUESTED_IP, 4);
    dhcp_option_long(dhcp, ntohl(dhcp->offered_ip_addr.addr));
#endif

#if 0
    dhcp_option(dhcp, DHCP_OPTION_SERVER_ID, 4);
    dhcp_option_long(dhcp, ntohl(dhcp->server_ip_addr.addr));
#endif

#if LWIP_NETIF_HOSTNAME
    dhcp_option_hostname(dhcp, netif);
#endif /* LWIP_NETIF_HOSTNAME */

    /* append DHCP message trailer */
    dhcp_option_trailer(dhcp);

    pbuf_realloc(dhcp->p_out, sizeof(struct dhcp_msg) - DHCP_OPTIONS_LEN + dhcp->options_out_len);

    udp_sendto_if(dhcp->pcb, dhcp->p_out, &dhcp->server_ip_addr, DHCP_SERVER_PORT, netif);
    dhcp_delete_msg(dhcp);

    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE, ("dhcp_renew: RENEWING\n"));
  } else {
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_SERIOUS, ("dhcp_renew: could not allocate DHCP request\n"));
  }
  dhcp->tries++;
  /* back-off on retries, but to a maximum of 20 seconds */
  msecs = dhcp->tries < 10 ? dhcp->tries * 2000 : 20 * 1000;
  dhcp->request_timeout = (msecs + DHCP_FINE_TIMER_MSECS - 1) / DHCP_FINE_TIMER_MSECS;
  LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE, ("dhcp_renew(): set request timeout %"U16_F" msecs\n", msecs));
  return result;
}

/**
 * Rebind with a DHCP server for an existing DHCP lease.
 *
 * @param netif network interface which must rebind with a DHCP server
 */
static err_t
dhcp_rebind(struct netif *netif)
{
  struct dhcp *dhcp = netif->dhcp;
  err_t result;
  u16_t msecs;
  LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE, ("dhcp_rebind()\n"));
  dhcp_set_state(dhcp, DHCP_REBINDING);

  /* create and initialize the DHCP message header */
  result = dhcp_create_msg(netif, dhcp, DHCP_REQUEST);
  if (result == ERR_OK) {
    dhcp_option(dhcp, DHCP_OPTION_MAX_MSG_SIZE, DHCP_OPTION_MAX_MSG_SIZE_LEN);
    dhcp_option_short(dhcp, DHCP_MAX_MSG_LEN(netif));

#if LWIP_NETIF_HOSTNAME
    dhcp_option_hostname(dhcp, netif);
#endif /* LWIP_NETIF_HOSTNAME */

#if 0
    dhcp_option(dhcp, DHCP_OPTION_REQUESTED_IP, 4);
    dhcp_option_long(dhcp, ntohl(dhcp->offered_ip_addr.addr));

    dhcp_option(dhcp, DHCP_OPTION_SERVER_ID, 4);
    dhcp_option_long(dhcp, ntohl(dhcp->server_ip_addr.addr));
#endif

    dhcp_option_trailer(dhcp);

    pbuf_realloc(dhcp->p_out, sizeof(struct dhcp_msg) - DHCP_OPTIONS_LEN + dhcp->options_out_len);

    /* broadcast to server */
    udp_sendto_if(dhcp->pcb, dhcp->p_out, IP_ADDR_BROADCAST, DHCP_SERVER_PORT, netif);
    dhcp_delete_msg(dhcp);
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE, ("dhcp_rebind: REBINDING\n"));
  } else {
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_SERIOUS, ("dhcp_rebind: could not allocate DHCP request\n"));
  }
  dhcp->tries++;
  msecs = dhcp->tries < 10 ? dhcp->tries * 1000 : 10 * 1000;
  dhcp->request_timeout = (msecs + DHCP_FINE_TIMER_MSECS - 1) / DHCP_FINE_TIMER_MSECS;
  LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE, ("dhcp_rebind(): set request timeout %"U16_F" msecs\n", msecs));
  return result;
}

/**
 * Enter REBOOTING state to verify an existing lease
 *
 * @param netif network interface which must reboot
 */
static err_t
dhcp_reboot(struct netif *netif)
{
  struct dhcp *dhcp = netif->dhcp;
  err_t result;
  u16_t msecs;
  LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE, ("dhcp_reboot()\n"));
  dhcp_set_state(dhcp, DHCP_REBOOTING);

  /* create and initialize the DHCP message header */
  result = dhcp_create_msg(netif, dhcp, DHCP_REQUEST);
  if (result == ERR_OK) {
    dhcp_option(dhcp, DHCP_OPTION_MAX_MSG_SIZE, DHCP_OPTION_MAX_MSG_SIZE_LEN);
    dhcp_option_short(dhcp, 576);

    dhcp_option(dhcp, DHCP_OPTION_REQUESTED_IP, 4);
    dhcp_option_long(dhcp, ntohl(ip4_addr_get_u32(&dhcp->offered_ip_addr)));

    dhcp_option_trailer(dhcp);

    pbuf_realloc(dhcp->p_out, sizeof(struct dhcp_msg) - DHCP_OPTIONS_LEN + dhcp->options_out_len);

    /* broadcast to server */
    udp_sendto_if(dhcp->pcb, dhcp->p_out, IP_ADDR_BROADCAST, DHCP_SERVER_PORT, netif);
    dhcp_delete_msg(dhcp);
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE, ("dhcp_reboot: REBOOTING\n"));
  } else {
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_SERIOUS, ("dhcp_reboot: could not allocate DHCP request\n"));
  }
  dhcp->tries++;
  msecs = dhcp->tries < 10 ? dhcp->tries * 1000 : 10 * 1000;
  dhcp->request_timeout = (msecs + DHCP_FINE_TIMER_MSECS - 1) / DHCP_FINE_TIMER_MSECS;
  LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE, ("dhcp_reboot(): set request timeout %"U16_F" msecs\n", msecs));
  return result;
}


/**
 * Release a DHCP lease.
 *
 * @param netif network interface which must release its lease
 */
err_t
dhcp_release(struct netif *netif)
{
  struct dhcp *dhcp = netif->dhcp;
  err_t result;
  u16_t msecs;
  LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("dhcp_release()\n"));
  if (dhcp == NULL) {
    return ERR_ARG;
  }

  /* idle DHCP client */
  dhcp_set_state(dhcp, DHCP_OFF);
  /* clean old DHCP offer */
  ip_addr_set_zero(&dhcp->server_ip_addr);
  ip_addr_set_zero(&dhcp->offered_ip_addr);
  ip_addr_set_zero(&dhcp->offered_sn_mask);
  ip_addr_set_zero(&dhcp->offered_gw_addr);
#if LWIP_DHCP_BOOTP_FILE
  ip_addr_set_zero(&dhcp->offered_si_addr);
#endif /* LWIP_DHCP_BOOTP_FILE */
  dhcp->offered_t0_lease = dhcp->offered_t1_renew = dhcp->offered_t2_rebind = 0;
  
  /* create and initialize the DHCP message header */
  result = dhcp_create_msg(netif, dhcp, DHCP_RELEASE);
  if (result == ERR_OK) {
    dhcp_option_trailer(dhcp);

    pbuf_realloc(dhcp->p_out, sizeof(struct dhcp_msg) - DHCP_OPTIONS_LEN + dhcp->options_out_len);

    udp_sendto_if(dhcp->pcb, dhcp->p_out, &dhcp->server_ip_addr, DHCP_SERVER_PORT, netif);
    dhcp_delete_msg(dhcp);
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE, ("dhcp_release: RELEASED, DHCP_OFF\n"));
  } else {
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_SERIOUS, ("dhcp_release: could not allocate DHCP request\n"));
  }
  dhcp->tries++;
  msecs = dhcp->tries < 10 ? dhcp->tries * 1000 : 10 * 1000;
  dhcp->request_timeout = (msecs + DHCP_FINE_TIMER_MSECS - 1) / DHCP_FINE_TIMER_MSECS;
  LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE, ("dhcp_release(): set request timeout %"U16_F" msecs\n", msecs));
  /* bring the interface down */
  netif_set_down(netif);
  /* remove IP address from interface */
  netif_set_ipaddr(netif, IP_ADDR_ANY);
  netif_set_gw(netif, IP_ADDR_ANY);
  netif_set_netmask(netif, IP_ADDR_ANY);
  
  return result;
}

/**
 * Remove the DHCP client from the interface.
 *
 * @param netif The network interface to stop DHCP on
 */
void
dhcp_stop(struct netif *netif)
{
  struct dhcp *dhcp;
  LWIP_ERROR("dhcp_stop: netif != NULL", (netif != NULL), return;);
  dhcp = netif->dhcp;
  /* Remove the flag that says this netif is handled by DHCP. */
  netif->flags &= ~NETIF_FLAG_DHCP;

  LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("dhcp_stop()\n"));
  /* netif is DHCP configured? */
  if (dhcp != NULL) {
#if LWIP_DHCP_AUTOIP_COOP
    if(dhcp->autoip_coop_state == DHCP_AUTOIP_COOP_STATE_ON) {
      autoip_stop(netif);
      dhcp->autoip_coop_state = DHCP_AUTOIP_COOP_STATE_OFF;
    }
#endif /* LWIP_DHCP_AUTOIP_COOP */

    if (dhcp->pcb != NULL) {
      udp_remove(dhcp->pcb);
      dhcp->pcb = NULL;
    }
    LWIP_ASSERT("reply wasn't freed", dhcp->msg_in == NULL);
    dhcp_set_state(dhcp, DHCP_OFF);
  }
}

/*
 * Set the DHCP state of a DHCP client.
 *
 * If the state changed, reset the number of tries.
 */
static void
dhcp_set_state(struct dhcp *dhcp, u8_t new_state)
{
  if (new_state != dhcp->state) {
    dhcp->state = new_state;
    dhcp->tries = 0;
    dhcp->request_timeout = 0;
  }
}

/*
 * Concatenate an option type and length field to the outgoing
 * DHCP message.
 *
 */
static void
dhcp_option(struct dhcp *dhcp, u8_t option_type, u8_t option_len)
{
  LWIP_ASSERT("dhcp_option: dhcp->options_out_len + 2 + option_len <= DHCP_OPTIONS_LEN", dhcp->options_out_len + 2U + option_len <= DHCP_OPTIONS_LEN);
  dhcp->msg_out->options[dhcp->options_out_len++] = option_type;
  dhcp->msg_out->options[dhcp->options_out_len++] = option_len;
}
/*
 * Concatenate a single byte to the outgoing DHCP message.
 *
 */
static void
dhcp_option_byte(struct dhcp *dhcp, u8_t value)
{
  LWIP_ASSERT("dhcp_option_byte: dhcp->options_out_len < DHCP_OPTIONS_LEN", dhcp->options_out_len < DHCP_OPTIONS_LEN);
  dhcp->msg_out->options[dhcp->options_out_len++] = value;
}

static void
dhcp_option_short(struct dhcp *dhcp, u16_t value)
{
  LWIP_ASSERT("dhcp_option_short: dhcp->options_out_len + 2 <= DHCP_OPTIONS_LEN", dhcp->options_out_len + 2U <= DHCP_OPTIONS_LEN);
  dhcp->msg_out->options[dhcp->options_out_len++] = (u8_t)((value & 0xff00U) >> 8);
  dhcp->msg_out->options[dhcp->options_out_len++] = (u8_t) (value & 0x00ffU);
}

static void
dhcp_option_long(struct dhcp *dhcp, u32_t value)
{
  LWIP_ASSERT("dhcp_option_long: dhcp->options_out_len + 4 <= DHCP_OPTIONS_LEN", dhcp->options_out_len + 4U <= DHCP_OPTIONS_LEN);
  dhcp->msg_out->options[dhcp->options_out_len++] = (u8_t)((value & 0xff000000UL) >> 24);
  dhcp->msg_out->options[dhcp->options_out_len++] = (u8_t)((value & 0x00ff0000UL) >> 16);
  dhcp->msg_out->options[dhcp->options_out_len++] = (u8_t)((value & 0x0000ff00UL) >> 8);
  dhcp->msg_out->options[dhcp->options_out_len++] = (u8_t)((value & 0x000000ffUL));
}

#if LWIP_NETIF_HOSTNAME
static void
dhcp_option_hostname(struct dhcp *dhcp, struct netif *netif)
{
  if (netif->hostname != NULL) {
    size_t namelen = strlen(netif->hostname);
    if (namelen > 0) {
      u8_t len;
      const char *p = netif->hostname;
      /* Shrink len to available bytes (need 2 bytes for OPTION_HOSTNAME
         and 1 byte for trailer) */
      size_t available = DHCP_OPTIONS_LEN - dhcp->options_out_len - 3;
      LWIP_ASSERT("DHCP: hostname is too long!", namelen <= available);
      len = LWIP_MIN(namelen, available);
      dhcp_option(dhcp, DHCP_OPTION_HOSTNAME, len);
      while (len--) {
        dhcp_option_byte(dhcp, *p++);
      }
    }
  }
}
#endif /* LWIP_NETIF_HOSTNAME */

/**
 * Extract the DHCP message and the DHCP options.
 *
 * Extract the DHCP message and the DHCP options, each into a contiguous
 * piece of memory. As a DHCP message is variable sized by its options,
 * and also allows overriding some fields for options, the easy approach
 * is to first unfold the options into a conitguous piece of memory, and
 * use that further on.
 *
 */
static err_t
dhcp_parse_reply(struct dhcp *dhcp, struct pbuf *p)
{
  u8_t *options;
  u16_t offset;
  u16_t offset_max;
  u16_t options_idx;
  u16_t options_idx_max;
  struct pbuf *q;
  int parse_file_as_options = 0;
  int parse_sname_as_options = 0;

  /* clear received options */
  dhcp_clear_all_options(dhcp);
  /* check that beginning of dhcp_msg (up to and including chaddr) is in first pbuf */
  if (p->len < DHCP_SNAME_OFS) {
    return ERR_BUF;
  }
  dhcp->msg_in = (struct dhcp_msg *)p->payload;
#if LWIP_DHCP_BOOTP_FILE
  /* clear boot file name */
  dhcp->boot_file_name[0] = 0;
#endif /* LWIP_DHCP_BOOTP_FILE */

  /* parse options */

  /* start with options field */
  options_idx = DHCP_OPTIONS_OFS;
  /* parse options to the end of the received packet */
  options_idx_max = p->tot_len;
again:
  q = p;
  while((q != NULL) && (options_idx >= q->len)) {
    options_idx -= q->len;
    options_idx_max -= q->len;
    q = q->next;
  }
  if (q == NULL) {
    return ERR_BUF;
  }
  offset = options_idx;
  offset_max = options_idx_max;
  options = (u8_t*)q->payload;
  /* at least 1 byte to read and no end marker, then at least 3 bytes to read? */
  while((q != NULL) && (options[offset] != DHCP_OPTION_END) && (offset < offset_max)) {
    u8_t op = options[offset];
    u8_t len;
    u8_t decode_len = 0;
    int decode_idx = -1;
    u16_t val_offset = offset + 2;
    /* len byte might be in the next pbuf */
    if (offset + 1 < q->len) {
      len = options[offset + 1];
    } else {
      len = (q->next != NULL ? ((u8_t*)q->next->payload)[0] : 0);
    }
    /* LWIP_DEBUGF(DHCP_DEBUG, ("msg_offset=%"U16_F", q->len=%"U16_F, msg_offset, q->len)); */
    decode_len = len;
    switch(op) {
      /* case(DHCP_OPTION_END): handled above */
      case(DHCP_OPTION_PAD):
        /* special option: no len encoded */
        decode_len = len = 0;
        /* will be increased below */
        offset--;
        break;
      case(DHCP_OPTION_SUBNET_MASK):
        LWIP_ERROR("len == 4", len == 4, return ERR_VAL;);
        decode_idx = DHCP_OPTION_IDX_SUBNET_MASK;
        break;
      case(DHCP_OPTION_ROUTER):
        decode_len = 4; /* only copy the first given router */
        LWIP_ERROR("len >= decode_len", len >= decode_len, return ERR_VAL;);
        decode_idx = DHCP_OPTION_IDX_ROUTER;
        break;
      case(DHCP_OPTION_DNS_SERVER):
        /* special case: there might be more than one server */
        LWIP_ERROR("len % 4 == 0", len % 4 == 0, return ERR_VAL;);
        /* limit number of DNS servers */
        decode_len = LWIP_MIN(len, 4 * DNS_MAX_SERVERS);
        LWIP_ERROR("len >= decode_len", len >= decode_len, return ERR_VAL;);
        decode_idx = DHCP_OPTION_IDX_DNS_SERVER;
        break;
      case(DHCP_OPTION_LEASE_TIME):
        LWIP_ERROR("len == 4", len == 4, return ERR_VAL;);
        decode_idx = DHCP_OPTION_IDX_LEASE_TIME;
        break;
      case(DHCP_OPTION_OVERLOAD):
        LWIP_ERROR("len == 1", len == 1, return ERR_VAL;);
        decode_idx = DHCP_OPTION_IDX_OVERLOAD;
        break;
      case(DHCP_OPTION_MESSAGE_TYPE):
        LWIP_ERROR("len == 1", len == 1, return ERR_VAL;);
        decode_idx = DHCP_OPTION_IDX_MSG_TYPE;
        break;
      case(DHCP_OPTION_SERVER_ID):
        LWIP_ERROR("len == 4", len == 4, return ERR_VAL;);
        decode_idx = DHCP_OPTION_IDX_SERVER_ID;
        break;
      case(DHCP_OPTION_T1):
        LWIP_ERROR("len == 4", len == 4, return ERR_VAL;);
        decode_idx = DHCP_OPTION_IDX_T1;
        break;
      case(DHCP_OPTION_T2):
        LWIP_ERROR("len == 4", len == 4, return ERR_VAL;);
        decode_idx = DHCP_OPTION_IDX_T2;
        break;
      default:
        decode_len = 0;
        LWIP_DEBUGF(DHCP_DEBUG, ("skipping option %"U16_F" in options\n", op));
        break;
    }
    offset += len + 2;
    if (decode_len > 0) {
      u32_t value = 0;
      u16_t copy_len;
decode_next:
      LWIP_ASSERT("check decode_idx", decode_idx >= 0 && decode_idx < DHCP_OPTION_IDX_MAX);
      if (!dhcp_option_given(dhcp, decode_idx)) {
        copy_len = LWIP_MIN(decode_len, 4);
        pbuf_copy_partial(q, &value, copy_len, val_offset);
        if (decode_len > 4) {
          /* decode more than one u32_t */
          LWIP_ERROR("decode_len % 4 == 0", decode_len % 4 == 0, return ERR_VAL;);
          dhcp_got_option(dhcp, decode_idx);
          dhcp_set_option_value(dhcp, decode_idx, htonl(value));
          decode_len -= 4;
          val_offset += 4;
          decode_idx++;
          goto decode_next;
        } else if (decode_len == 4) {
          value = ntohl(value);
        } else {
          LWIP_ERROR("invalid decode_len", decode_len == 1, return ERR_VAL;);
          value = ((u8_t*)&value)[0];
        }
        dhcp_got_option(dhcp, decode_idx);
        dhcp_set_option_value(dhcp, decode_idx, value);
      }
    }
    if (offset >= q->len) {
      offset -= q->len;
      offset_max -= q->len;
      if ((offset < offset_max) && offset_max) {
        q = q->next;
        LWIP_ASSERT("next pbuf was null", q);
        options = (u8_t*)q->payload;
      } else {
        /* We've run out of bytes, probably no end marker. Don't proceed. */
        break;
      }
    }
  }
  /* is this an overloaded message? */
  if (dhcp_option_given(dhcp, DHCP_OPTION_IDX_OVERLOAD)) {
    u32_t overload = dhcp_get_option_value(dhcp, DHCP_OPTION_IDX_OVERLOAD);
    dhcp_clear_option(dhcp, DHCP_OPTION_IDX_OVERLOAD);
    if (overload == DHCP_OVERLOAD_FILE) {
      parse_file_as_options = 1;
      LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("overloaded file field\n"));
    } else if (overload == DHCP_OVERLOAD_SNAME) {
      parse_sname_as_options = 1;
      LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("overloaded sname field\n"));
    } else if (overload == DHCP_OVERLOAD_SNAME_FILE) {
      parse_sname_as_options = 1;
      parse_file_as_options = 1;
      LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("overloaded sname and file field\n"));
    } else {
      LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("invalid overload option: %d\n", (int)overload));
    }
#if LWIP_DHCP_BOOTP_FILE
    if (!parse_file_as_options) {
      /* only do this for ACK messages */
      if (dhcp_option_given(dhcp, DHCP_OPTION_IDX_MSG_TYPE) &&
        (dhcp_get_option_value(dhcp, DHCP_OPTION_IDX_MSG_TYPE) == DHCP_ACK))
      /* copy bootp file name, don't care for sname (server hostname) */
      pbuf_copy_partial(p, dhcp->boot_file_name, DHCP_FILE_LEN-1, DHCP_FILE_OFS);
      /* make sure the string is really NULL-terminated */
      dhcp->boot_file_name[DHCP_FILE_LEN-1] = 0;
    }
#endif /* LWIP_DHCP_BOOTP_FILE */
  }
  if (parse_file_as_options) {
    /* if both are overloaded, parse file first and then sname (RFC 2131 ch. 4.1) */
    parse_file_as_options = 0;
    options_idx = DHCP_FILE_OFS;
    options_idx_max = DHCP_FILE_OFS + DHCP_FILE_LEN;
    goto again;
  } else if (parse_sname_as_options) {
    parse_sname_as_options = 0;
    options_idx = DHCP_SNAME_OFS;
    options_idx_max = DHCP_SNAME_OFS + DHCP_SNAME_LEN;
    goto again;
  }
  return ERR_OK;
}

/**
 * If an incoming DHCP message is in response to us, then trigger the state machine
 */
static void
dhcp_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, ip_addr_t *addr, u16_t port)
{
  struct netif *netif = (struct netif *)arg;
  struct dhcp *dhcp = netif->dhcp;
  struct dhcp_msg *reply_msg = (struct dhcp_msg *)p->payload;
  u8_t msg_type;
  u8_t i;
  LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("dhcp_recv(pbuf = %p) from DHCP server %"U16_F".%"U16_F".%"U16_F".%"U16_F" port %"U16_F"\n", (void*)p,
    ip4_addr1_16(addr), ip4_addr2_16(addr), ip4_addr3_16(addr), ip4_addr4_16(addr), port));
  LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("pbuf->len = %"U16_F"\n", p->len));
  LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("pbuf->tot_len = %"U16_F"\n", p->tot_len));
  /* prevent warnings about unused arguments */
  LWIP_UNUSED_ARG(pcb);
  LWIP_UNUSED_ARG(addr);
  LWIP_UNUSED_ARG(port);

  LWIP_ASSERT("reply wasn't freed", dhcp->msg_in == NULL);

  if (p->len < DHCP_MIN_REPLY_LEN) {
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_WARNING, ("DHCP reply message or pbuf too short\n"));
    goto free_pbuf_and_return;
  }

  if (reply_msg->op != DHCP_BOOTREPLY) {
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_WARNING, ("not a DHCP reply message, but type %"U16_F"\n", (u16_t)reply_msg->op));
    goto free_pbuf_and_return;
  }
  /* iterate through hardware address and match against DHCP message */
  for (i = 0; i < netif->hwaddr_len; i++) {
    if (netif->hwaddr[i] != reply_msg->chaddr[i]) {
      LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_WARNING,
        ("netif->hwaddr[%"U16_F"]==%02"X16_F" != reply_msg->chaddr[%"U16_F"]==%02"X16_F"\n",
        (u16_t)i, (u16_t)netif->hwaddr[i], (u16_t)i, (u16_t)reply_msg->chaddr[i]));
      goto free_pbuf_and_return;
    }
  }
  /* match transaction ID against what we expected */
  if (ntohl(reply_msg->xid) != dhcp->xid) {
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_WARNING,
      ("transaction id mismatch reply_msg->xid(%"X32_F")!=dhcp->xid(%"X32_F")\n",ntohl(reply_msg->xid),dhcp->xid));
    goto free_pbuf_and_return;
  }
  /* option fields could be unfold? */
  if (dhcp_parse_reply(dhcp, p) != ERR_OK) {
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_SERIOUS,
      ("problem unfolding DHCP message - too short on memory?\n"));
    goto free_pbuf_and_return;
  }

  LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("searching DHCP_OPTION_MESSAGE_TYPE\n"));
  /* obtain pointer to DHCP message type */
  if (!dhcp_option_given(dhcp, DHCP_OPTION_IDX_MSG_TYPE)) {
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_WARNING, ("DHCP_OPTION_MESSAGE_TYPE option not found\n"));
    goto free_pbuf_and_return;
  }

  /* read DHCP message type */
  msg_type = (u8_t)dhcp_get_option_value(dhcp, DHCP_OPTION_IDX_MSG_TYPE);
  /* message type is DHCP ACK? */
  if (msg_type == DHCP_ACK) {
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("DHCP_ACK received\n"));
    /* in requesting state? */
    if (dhcp->state == DHCP_REQUESTING) {
      dhcp_handle_ack(netif);
#if DHCP_DOES_ARP_CHECK
      /* check if the acknowledged lease address is already in use */
      dhcp_check(netif);
#else
      /* bind interface to the acknowledged lease address */
      dhcp_bind(netif);
#endif
    }
    /* already bound to the given lease address? */
    else if ((dhcp->state == DHCP_REBOOTING) || (dhcp->state == DHCP_REBINDING) || (dhcp->state == DHCP_RENEWING)) {
      dhcp_bind(netif);
    }
  }
  /* received a DHCP_NAK in appropriate state? */
  else if ((msg_type == DHCP_NAK) &&
    ((dhcp->state == DHCP_REBOOTING) || (dhcp->state == DHCP_REQUESTING) ||
     (dhcp->state == DHCP_REBINDING) || (dhcp->state == DHCP_RENEWING  ))) {
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("DHCP_NAK received\n"));
    dhcp_handle_nak(netif);
  }
  /* received a DHCP_OFFER in DHCP_SELECTING state? */
  else if ((msg_type == DHCP_OFFER) && (dhcp->state == DHCP_SELECTING)) {
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE, ("DHCP_OFFER received in DHCP_SELECTING state\n"));
    dhcp->request_timeout = 0;
    /* remember offered lease */
    dhcp_handle_offer(netif);
  }
free_pbuf_and_return:
  dhcp->msg_in = NULL;
  pbuf_free(p);
}

/**
 * Create a DHCP request, fill in common headers
 *
 * @param netif the netif under DHCP control
 * @param dhcp dhcp control struct
 * @param message_type message type of the request
 */
static err_t
dhcp_create_msg(struct netif *netif, struct dhcp *dhcp, u8_t message_type)
{
  u16_t i;
#ifndef DHCP_GLOBAL_XID
  /** default global transaction identifier starting value (easy to match
   *  with a packet analyser). We simply increment for each new request.
   *  Predefine DHCP_GLOBAL_XID to a better value or a function call to generate one
   *  at runtime, any supporting function prototypes can be defined in DHCP_GLOBAL_XID_HEADER */
#if DHCP_CREATE_RAND_XID && defined(LWIP_RAND)
  static u32_t xid;
#else /* DHCP_CREATE_RAND_XID && defined(LWIP_RAND) */
  static u32_t xid = 0xABCD0000;
#endif /* DHCP_CREATE_RAND_XID && defined(LWIP_RAND) */
#else
  if (!xid_initialised) {
    xid = DHCP_GLOBAL_XID;
    xid_initialised = !xid_initialised;
  }
#endif
  LWIP_ERROR("dhcp_create_msg: netif != NULL", (netif != NULL), return ERR_ARG;);
  LWIP_ERROR("dhcp_create_msg: dhcp != NULL", (dhcp != NULL), return ERR_VAL;);
  LWIP_ASSERT("dhcp_create_msg: dhcp->p_out == NULL", dhcp->p_out == NULL);
  LWIP_ASSERT("dhcp_create_msg: dhcp->msg_out == NULL", dhcp->msg_out == NULL);
  dhcp->p_out = pbuf_alloc(PBUF_TRANSPORT, sizeof(struct dhcp_msg), PBUF_RAM);
  if (dhcp->p_out == NULL) {
    LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_LEVEL_SERIOUS,
      ("dhcp_create_msg(): could not allocate pbuf\n"));
    return ERR_MEM;
  }
  LWIP_ASSERT("dhcp_create_msg: check that first pbuf can hold struct dhcp_msg",
           (dhcp->p_out->len >= sizeof(struct dhcp_msg)));

  /* reuse transaction identifier in retransmissions */
  if (dhcp->tries == 0) {
#if DHCP_CREATE_RAND_XID && defined(LWIP_RAND)
    xid = LWIP_RAND();
#else /* DHCP_CREATE_RAND_XID && defined(LWIP_RAND) */
    xid++;
#endif /* DHCP_CREATE_RAND_XID && defined(LWIP_RAND) */
  }
  dhcp->xid = xid;
  LWIP_DEBUGF(DHCP_DEBUG | LWIP_DBG_TRACE,
              ("transaction id xid(%"X32_F")\n", xid));

  dhcp->msg_out = (struct dhcp_msg *)dhcp->p_out->payload;

  dhcp->msg_out->op = DHCP_BOOTREQUEST;
  /* TODO: make link layer independent */
  dhcp->msg_out->htype = DHCP_HTYPE_ETH;
  dhcp->msg_out->hlen = netif->hwaddr_len;
  dhcp->msg_out->hops = 0;
  dhcp->msg_out->xid = htonl(dhcp->xid);
  dhcp->msg_out->secs = 0;
  /* we don't need the broadcast flag since we can receive unicast traffic
     before being fully configured! */
  dhcp->msg_out->flags = 0;
  ip_addr_set_zero(&dhcp->msg_out->ciaddr);
  /* set ciaddr to netif->ip_addr based on message_type and state */
  if ((message_type == DHCP_INFORM) || (message_type == DHCP_DECLINE) ||
      ((message_type == DHCP_REQUEST) && /* DHCP_BOUND not used for sending! */
       ((dhcp->state==DHCP_RENEWING) || dhcp->state==DHCP_REBINDING))) {
    ip_addr_copy(dhcp->msg_out->ciaddr, netif->ip_addr);
  }
  ip_addr_set_zero(&dhcp->msg_out->yiaddr);
  ip_addr_set_zero(&dhcp->msg_out->siaddr);
  ip_addr_set_zero(&dhcp->msg_out->giaddr);
  for (i = 0; i < DHCP_CHADDR_LEN; i++) {
    /* copy netif hardware address, pad with zeroes */
    dhcp->msg_out->chaddr[i] = (i < netif->hwaddr_len && i < NETIF_MAX_HWADDR_LEN) ? netif->hwaddr[i] : 0/* pad byte*/;
  }
  for (i = 0; i < DHCP_SNAME_LEN; i++) {
    dhcp->msg_out->sname[i] = 0;
  }
  for (i = 0; i < DHCP_FILE_LEN; i++) {
    dhcp->msg_out->file[i] = 0;
  }
  dhcp->msg_out->cookie = PP_HTONL(DHCP_MAGIC_COOKIE);
  dhcp->options_out_len = 0;
  /* fill options field with an incrementing array (for debugging purposes) */
  for (i = 0; i < DHCP_OPTIONS_LEN; i++) {
    dhcp->msg_out->options[i] = (u8_t)i; /* for debugging only, no matter if truncated */
  }
  /* Add option MESSAGE_TYPE */
  dhcp_option(dhcp, DHCP_OPTION_MESSAGE_TYPE, DHCP_OPTION_MESSAGE_TYPE_LEN);
  dhcp_option_byte(dhcp, message_type);
  return ERR_OK;
}

/**
 * Free previously allocated memory used to send a DHCP request.
 *
 * @param dhcp the dhcp struct to free the request from
 */
static void
dhcp_delete_msg(struct dhcp *dhcp)
{
  LWIP_ERROR("dhcp_delete_msg: dhcp != NULL", (dhcp != NULL), return;);
  LWIP_ASSERT("dhcp_delete_msg: dhcp->p_out != NULL", dhcp->p_out != NULL);
  LWIP_ASSERT("dhcp_delete_msg: dhcp->msg_out != NULL", dhcp->msg_out != NULL);
  if (dhcp->p_out != NULL) {
    pbuf_free(dhcp->p_out);
  }
  dhcp->p_out = NULL;
  dhcp->msg_out = NULL;
}

/**
 * Add a DHCP message trailer
 *
 * Adds the END option to the DHCP message, and if
 * necessary, up to three padding bytes.
 *
 * @param dhcp DHCP state structure
 */
static void
dhcp_option_trailer(struct dhcp *dhcp)
{
  LWIP_ERROR("dhcp_option_trailer: dhcp != NULL", (dhcp != NULL), return;);
  LWIP_ASSERT("dhcp_option_trailer: dhcp->msg_out != NULL\n", dhcp->msg_out != NULL);
  LWIP_ASSERT("dhcp_option_trailer: dhcp->options_out_len < DHCP_OPTIONS_LEN\n", dhcp->options_out_len < DHCP_OPTIONS_LEN);
  dhcp->msg_out->options[dhcp->options_out_len++] = DHCP_OPTION_END;
  /* packet is too small, or not 4 byte aligned? */
  while (((dhcp->options_out_len < DHCP_MIN_OPTIONS_LEN) || (dhcp->options_out_len & 3)) &&
         (dhcp->options_out_len < DHCP_OPTIONS_LEN)) {
    /* add a fill/padding byte */
    dhcp->msg_out->options[dhcp->options_out_len++] = 0;
  }
}

#endif /* LWIP_DHCP */
