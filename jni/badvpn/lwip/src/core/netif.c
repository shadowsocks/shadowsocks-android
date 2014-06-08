/**
 * @file
 * lwIP network interface abstraction
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

#include "lwip/opt.h"

#include "lwip/def.h"
#include "lwip/ip_addr.h"
#include "lwip/ip6_addr.h"
#include "lwip/netif.h"
#include "lwip/tcp_impl.h"
#include "lwip/snmp.h"
#include "lwip/igmp.h"
#include "netif/etharp.h"
#include "lwip/stats.h"
#if ENABLE_LOOPBACK
#include "lwip/sys.h"
#if LWIP_NETIF_LOOPBACK_MULTITHREADING
#include "lwip/tcpip.h"
#endif /* LWIP_NETIF_LOOPBACK_MULTITHREADING */
#endif /* ENABLE_LOOPBACK */

#if LWIP_AUTOIP
#include "lwip/autoip.h"
#endif /* LWIP_AUTOIP */
#if LWIP_DHCP
#include "lwip/dhcp.h"
#endif /* LWIP_DHCP */
#if LWIP_IPV6_DHCP6
#include "lwip/dhcp6.h"
#endif /* LWIP_IPV6_DHCP6 */
#if LWIP_IPV6_MLD
#include "lwip/mld6.h"
#endif /* LWIP_IPV6_MLD */

#if LWIP_NETIF_STATUS_CALLBACK
#define NETIF_STATUS_CALLBACK(n) do{ if (n->status_callback) { (n->status_callback)(n); }}while(0)
#else
#define NETIF_STATUS_CALLBACK(n)
#endif /* LWIP_NETIF_STATUS_CALLBACK */ 

#if LWIP_NETIF_LINK_CALLBACK
#define NETIF_LINK_CALLBACK(n) do{ if (n->link_callback) { (n->link_callback)(n); }}while(0)
#else
#define NETIF_LINK_CALLBACK(n)
#endif /* LWIP_NETIF_LINK_CALLBACK */ 

struct netif *netif_list;
struct netif *netif_default;

static u8_t netif_num;

#if LWIP_IPV6
static err_t netif_null_output_ip6(struct netif *netif, struct pbuf *p, ip6_addr_t *ipaddr);
#endif /* LWIP_IPV6 */

#if LWIP_HAVE_LOOPIF
static struct netif loop_netif;

/**
 * Initialize a lwip network interface structure for a loopback interface
 *
 * @param netif the lwip network interface structure for this loopif
 * @return ERR_OK if the loopif is initialized
 *         ERR_MEM if private data couldn't be allocated
 */
static err_t
netif_loopif_init(struct netif *netif)
{
  /* initialize the snmp variables and counters inside the struct netif
   * ifSpeed: no assumption can be made!
   */
  NETIF_INIT_SNMP(netif, snmp_ifType_softwareLoopback, 0);

  netif->name[0] = 'l';
  netif->name[1] = 'o';
  netif->output = netif_loop_output;
  return ERR_OK;
}
#endif /* LWIP_HAVE_LOOPIF */

void
netif_init(void)
{
#if LWIP_HAVE_LOOPIF
  ip_addr_t loop_ipaddr, loop_netmask, loop_gw;
  IP4_ADDR(&loop_gw, 127,0,0,1);
  IP4_ADDR(&loop_ipaddr, 127,0,0,1);
  IP4_ADDR(&loop_netmask, 255,0,0,0);

#if NO_SYS
  netif_add(&loop_netif, &loop_ipaddr, &loop_netmask, &loop_gw, NULL, netif_loopif_init, ip_input);
#else  /* NO_SYS */
  netif_add(&loop_netif, &loop_ipaddr, &loop_netmask, &loop_gw, NULL, netif_loopif_init, tcpip_input);
#endif /* NO_SYS */
  netif_set_up(&loop_netif);

#endif /* LWIP_HAVE_LOOPIF */
}

/**
 * Add a network interface to the list of lwIP netifs.
 *
 * @param netif a pre-allocated netif structure
 * @param ipaddr IP address for the new netif
 * @param netmask network mask for the new netif
 * @param gw default gateway IP address for the new netif
 * @param state opaque data passed to the new netif
 * @param init callback function that initializes the interface
 * @param input callback function that is called to pass
 * ingress packets up in the protocol layer stack.
 *
 * @return netif, or NULL if failed.
 */
struct netif *
netif_add(struct netif *netif, ip_addr_t *ipaddr, ip_addr_t *netmask,
  ip_addr_t *gw, void *state, netif_init_fn init, netif_input_fn input)
{
#if LWIP_IPV6
  u32_t i;
#endif

  LWIP_ASSERT("No init function given", init != NULL);

  /* reset new interface configuration state */
  ip_addr_set_zero(&netif->ip_addr);
  ip_addr_set_zero(&netif->netmask);
  ip_addr_set_zero(&netif->gw);
#if LWIP_IPV6
  for (i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {
    ip6_addr_set_zero(&netif->ip6_addr[i]);
    netif_ip6_addr_set_state(netif, i, IP6_ADDR_INVALID);
  }
  netif->output_ip6 = netif_null_output_ip6;
#endif /* LWIP_IPV6 */
  netif->flags = 0;
#if LWIP_DHCP
  /* netif not under DHCP control by default */
  netif->dhcp = NULL;
#endif /* LWIP_DHCP */
#if LWIP_AUTOIP
  /* netif not under AutoIP control by default */
  netif->autoip = NULL;
#endif /* LWIP_AUTOIP */
#if LWIP_IPV6_AUTOCONFIG
  /* IPv6 address autoconfiguration not enabled by default */
  netif->ip6_autoconfig_enabled = 0;
#endif /* LWIP_IPV6_AUTOCONFIG */
#if LWIP_IPV6_SEND_ROUTER_SOLICIT
  netif->rs_count = LWIP_ND6_MAX_MULTICAST_SOLICIT;
#endif /* LWIP_IPV6_SEND_ROUTER_SOLICIT */
#if LWIP_IPV6_DHCP6
  /* netif not under DHCPv6 control by default */
  netif->dhcp6 = NULL;
#endif /* LWIP_IPV6_DHCP6 */
#if LWIP_NETIF_STATUS_CALLBACK
  netif->status_callback = NULL;
#endif /* LWIP_NETIF_STATUS_CALLBACK */
#if LWIP_NETIF_LINK_CALLBACK
  netif->link_callback = NULL;
#endif /* LWIP_NETIF_LINK_CALLBACK */
#if LWIP_IGMP
  netif->igmp_mac_filter = NULL;
#endif /* LWIP_IGMP */
#if LWIP_IPV6 && LWIP_IPV6_MLD
  netif->mld_mac_filter = NULL;
#endif /* LWIP_IPV6 && LWIP_IPV6_MLD */
#if ENABLE_LOOPBACK
  netif->loop_first = NULL;
  netif->loop_last = NULL;
#endif /* ENABLE_LOOPBACK */

  /* remember netif specific state information data */
  netif->state = state;
  netif->num = netif_num++;
  netif->input = input;
  NETIF_SET_HWADDRHINT(netif, NULL);
#if ENABLE_LOOPBACK && LWIP_LOOPBACK_MAX_PBUFS
  netif->loop_cnt_current = 0;
#endif /* ENABLE_LOOPBACK && LWIP_LOOPBACK_MAX_PBUFS */

  netif_set_addr(netif, ipaddr, netmask, gw);

  /* call user specified initialization function for netif */
  if (init(netif) != ERR_OK) {
    return NULL;
  }

  /* add this netif to the list */
  netif->next = netif_list;
  netif_list = netif;
  snmp_inc_iflist();

#if LWIP_IGMP
  /* start IGMP processing */
  if (netif->flags & NETIF_FLAG_IGMP) {
    igmp_start(netif);
  }
#endif /* LWIP_IGMP */

  LWIP_DEBUGF(NETIF_DEBUG, ("netif: added interface %c%c IP addr ",
    netif->name[0], netif->name[1]));
  ip_addr_debug_print(NETIF_DEBUG, ipaddr);
  LWIP_DEBUGF(NETIF_DEBUG, (" netmask "));
  ip_addr_debug_print(NETIF_DEBUG, netmask);
  LWIP_DEBUGF(NETIF_DEBUG, (" gw "));
  ip_addr_debug_print(NETIF_DEBUG, gw);
  LWIP_DEBUGF(NETIF_DEBUG, ("\n"));
  return netif;
}

/**
 * Change IP address configuration for a network interface (including netmask
 * and default gateway).
 *
 * @param netif the network interface to change
 * @param ipaddr the new IP address
 * @param netmask the new netmask
 * @param gw the new default gateway
 */
void
netif_set_addr(struct netif *netif, ip_addr_t *ipaddr, ip_addr_t *netmask,
    ip_addr_t *gw)
{
  netif_set_ipaddr(netif, ipaddr);
  netif_set_netmask(netif, netmask);
  netif_set_gw(netif, gw);
}

/**
 * Remove a network interface from the list of lwIP netifs.
 *
 * @param netif the network interface to remove
 */
void
netif_remove(struct netif *netif)
{
  if (netif == NULL) {
    return;
  }

#if LWIP_IGMP
  /* stop IGMP processing */
  if (netif->flags & NETIF_FLAG_IGMP) {
    igmp_stop(netif);
  }
#endif /* LWIP_IGMP */
#if LWIP_IPV6 && LWIP_IPV6_MLD
  /* stop MLD processing */
  mld6_stop(netif);
#endif /* LWIP_IPV6 && LWIP_IPV6_MLD */
  if (netif_is_up(netif)) {
    /* set netif down before removing (call callback function) */
    netif_set_down(netif);
  }

  snmp_delete_ipaddridx_tree(netif);

  /*  is it the first netif? */
  if (netif_list == netif) {
    netif_list = netif->next;
  } else {
    /*  look for netif further down the list */
    struct netif * tmpNetif;
    for (tmpNetif = netif_list; tmpNetif != NULL; tmpNetif = tmpNetif->next) {
      if (tmpNetif->next == netif) {
        tmpNetif->next = netif->next;
        break;
      }
    }
    if (tmpNetif == NULL)
      return; /*  we didn't find any netif today */
  }
  snmp_dec_iflist();
  /* this netif is default? */
  if (netif_default == netif) {
    /* reset default netif */
    netif_set_default(NULL);
  }
#if LWIP_NETIF_REMOVE_CALLBACK
  if (netif->remove_callback) {
    netif->remove_callback(netif);
  }
#endif /* LWIP_NETIF_REMOVE_CALLBACK */
  LWIP_DEBUGF( NETIF_DEBUG, ("netif_remove: removed netif\n") );
}

/**
 * Find a network interface by searching for its name
 *
 * @param name the name of the netif (like netif->name) plus concatenated number
 * in ascii representation (e.g. 'en0')
 */
struct netif *
netif_find(char *name)
{
  struct netif *netif;
  u8_t num;

  if (name == NULL) {
    return NULL;
  }

  num = name[2] - '0';

  for(netif = netif_list; netif != NULL; netif = netif->next) {
    if (num == netif->num &&
       name[0] == netif->name[0] &&
       name[1] == netif->name[1]) {
      LWIP_DEBUGF(NETIF_DEBUG, ("netif_find: found %c%c\n", name[0], name[1]));
      return netif;
    }
  }
  LWIP_DEBUGF(NETIF_DEBUG, ("netif_find: didn't find %c%c\n", name[0], name[1]));
  return NULL;
}

int netif_is_named (struct netif *netif, const char name[3])
{
    u8_t num = name[2] - '0';
    
    return (!memcmp(netif->name, name, 2) && netif->num == num);
}

/**
 * Change the IP address of a network interface
 *
 * @param netif the network interface to change
 * @param ipaddr the new IP address
 *
 * @note call netif_set_addr() if you also want to change netmask and
 * default gateway
 */
void
netif_set_ipaddr(struct netif *netif, ip_addr_t *ipaddr)
{
  /* TODO: Handling of obsolete pcbs */
  /* See:  http://mail.gnu.org/archive/html/lwip-users/2003-03/msg00118.html */
#if LWIP_TCP
  struct tcp_pcb *pcb;
  struct tcp_pcb_listen *lpcb;

  /* address is actually being changed? */
  if (ipaddr && (ip_addr_cmp(ipaddr, &(netif->ip_addr))) == 0) {
    /* extern struct tcp_pcb *tcp_active_pcbs; defined by tcp.h */
    LWIP_DEBUGF(NETIF_DEBUG | LWIP_DBG_STATE, ("netif_set_ipaddr: netif address being changed\n"));
    pcb = tcp_active_pcbs;
    while (pcb != NULL) {
      /* PCB bound to current local interface address? */
      if (ip_addr_cmp(ipX_2_ip(&pcb->local_ip), &(netif->ip_addr))
#if LWIP_AUTOIP
        /* connections to link-local addresses must persist (RFC3927 ch. 1.9) */
        && !ip_addr_islinklocal(ipX_2_ip(&pcb->local_ip))
#endif /* LWIP_AUTOIP */
        ) {
        /* this connection must be aborted */
        struct tcp_pcb *next = pcb->next;
        LWIP_DEBUGF(NETIF_DEBUG | LWIP_DBG_STATE, ("netif_set_ipaddr: aborting TCP pcb %p\n", (void *)pcb));
        tcp_abort(pcb);
        pcb = next;
      } else {
        pcb = pcb->next;
      }
    }
    for (lpcb = tcp_listen_pcbs.listen_pcbs; lpcb != NULL; lpcb = lpcb->next) {
      /* PCB bound to current local interface address? */
      if ((!(ip_addr_isany(ipX_2_ip(&lpcb->local_ip)))) &&
          (ip_addr_cmp(ipX_2_ip(&lpcb->local_ip), &(netif->ip_addr)))) {
        /* The PCB is listening to the old ipaddr and
         * is set to listen to the new one instead */
        ip_addr_set(ipX_2_ip(&lpcb->local_ip), ipaddr);
      }
    }
  }
#endif
  snmp_delete_ipaddridx_tree(netif);
  snmp_delete_iprteidx_tree(0,netif);
  /* set new IP address to netif */
  ip_addr_set(&(netif->ip_addr), ipaddr);
  snmp_insert_ipaddridx_tree(netif);
  snmp_insert_iprteidx_tree(0,netif);

  LWIP_DEBUGF(NETIF_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE, ("netif: IP address of interface %c%c set to %"U16_F".%"U16_F".%"U16_F".%"U16_F"\n",
    netif->name[0], netif->name[1],
    ip4_addr1_16(&netif->ip_addr),
    ip4_addr2_16(&netif->ip_addr),
    ip4_addr3_16(&netif->ip_addr),
    ip4_addr4_16(&netif->ip_addr)));
}

/**
 * Change the default gateway for a network interface
 *
 * @param netif the network interface to change
 * @param gw the new default gateway
 *
 * @note call netif_set_addr() if you also want to change ip address and netmask
 */
void
netif_set_gw(struct netif *netif, ip_addr_t *gw)
{
  ip_addr_set(&(netif->gw), gw);
  LWIP_DEBUGF(NETIF_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE, ("netif: GW address of interface %c%c set to %"U16_F".%"U16_F".%"U16_F".%"U16_F"\n",
    netif->name[0], netif->name[1],
    ip4_addr1_16(&netif->gw),
    ip4_addr2_16(&netif->gw),
    ip4_addr3_16(&netif->gw),
    ip4_addr4_16(&netif->gw)));
}

void netif_set_pretend_tcp (struct netif *netif, u8_t pretend)
{
    if (pretend) {
        netif->flags |= NETIF_FLAG_PRETEND_TCP;
    } else {
        netif->flags &= ~NETIF_FLAG_PRETEND_TCP;
    }
}

/**
 * Change the netmask of a network interface
 *
 * @param netif the network interface to change
 * @param netmask the new netmask
 *
 * @note call netif_set_addr() if you also want to change ip address and
 * default gateway
 */
void
netif_set_netmask(struct netif *netif, ip_addr_t *netmask)
{
  snmp_delete_iprteidx_tree(0, netif);
  /* set new netmask to netif */
  ip_addr_set(&(netif->netmask), netmask);
  snmp_insert_iprteidx_tree(0, netif);
  LWIP_DEBUGF(NETIF_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE, ("netif: netmask of interface %c%c set to %"U16_F".%"U16_F".%"U16_F".%"U16_F"\n",
    netif->name[0], netif->name[1],
    ip4_addr1_16(&netif->netmask),
    ip4_addr2_16(&netif->netmask),
    ip4_addr3_16(&netif->netmask),
    ip4_addr4_16(&netif->netmask)));
}

/**
 * Set a network interface as the default network interface
 * (used to output all packets for which no specific route is found)
 *
 * @param netif the default network interface
 */
void
netif_set_default(struct netif *netif)
{
  if (netif == NULL) {
    /* remove default route */
    snmp_delete_iprteidx_tree(1, netif);
  } else {
    /* install default route */
    snmp_insert_iprteidx_tree(1, netif);
  }
  netif_default = netif;
  LWIP_DEBUGF(NETIF_DEBUG, ("netif: setting default interface %c%c\n",
           netif ? netif->name[0] : '\'', netif ? netif->name[1] : '\''));
}

/**
 * Bring an interface up, available for processing
 * traffic.
 * 
 * @note: Enabling DHCP on a down interface will make it come
 * up once configured.
 * 
 * @see dhcp_start()
 */ 
void netif_set_up(struct netif *netif)
{
  if (!(netif->flags & NETIF_FLAG_UP)) {
    netif->flags |= NETIF_FLAG_UP;
    
#if LWIP_SNMP
    snmp_get_sysuptime(&netif->ts);
#endif /* LWIP_SNMP */

    NETIF_STATUS_CALLBACK(netif);

    if (netif->flags & NETIF_FLAG_LINK_UP) {
#if LWIP_ARP
      /* For Ethernet network interfaces, we would like to send a "gratuitous ARP" */ 
      if (netif->flags & (NETIF_FLAG_ETHARP)) {
        etharp_gratuitous(netif);
      }
#endif /* LWIP_ARP */

#if LWIP_IGMP
      /* resend IGMP memberships */
      if (netif->flags & NETIF_FLAG_IGMP) {
        igmp_report_groups( netif);
      }
#endif /* LWIP_IGMP */
#if LWIP_IPV6 && LWIP_IPV6_MLD
      /* send mld memberships */
      mld6_report_groups( netif);
#endif /* LWIP_IPV6 && LWIP_IPV6_MLD */

#if LWIP_IPV6_SEND_ROUTER_SOLICIT
      /* Send Router Solicitation messages. */
      netif->rs_count = LWIP_ND6_MAX_MULTICAST_SOLICIT;
#endif /* LWIP_IPV6_SEND_ROUTER_SOLICIT */

    }
  }
}

/**
 * Bring an interface down, disabling any traffic processing.
 *
 * @note: Enabling DHCP on a down interface will make it come
 * up once configured.
 * 
 * @see dhcp_start()
 */ 
void netif_set_down(struct netif *netif)
{
  if (netif->flags & NETIF_FLAG_UP) {
    netif->flags &= ~NETIF_FLAG_UP;
#if LWIP_SNMP
    snmp_get_sysuptime(&netif->ts);
#endif

#if LWIP_ARP
    if (netif->flags & NETIF_FLAG_ETHARP) {
      etharp_cleanup_netif(netif);
    }
#endif /* LWIP_ARP */
    NETIF_STATUS_CALLBACK(netif);
  }
}

#if LWIP_NETIF_STATUS_CALLBACK
/**
 * Set callback to be called when interface is brought up/down
 */
void netif_set_status_callback(struct netif *netif, netif_status_callback_fn status_callback)
{
  if (netif) {
    netif->status_callback = status_callback;
  }
}
#endif /* LWIP_NETIF_STATUS_CALLBACK */

#if LWIP_NETIF_REMOVE_CALLBACK
/**
 * Set callback to be called when the interface has been removed
 */
void
netif_set_remove_callback(struct netif *netif, netif_status_callback_fn remove_callback)
{
  if (netif) {
    netif->remove_callback = remove_callback;
  }
}
#endif /* LWIP_NETIF_REMOVE_CALLBACK */

/**
 * Called by a driver when its link goes up
 */
void netif_set_link_up(struct netif *netif )
{
  if (!(netif->flags & NETIF_FLAG_LINK_UP)) {
    netif->flags |= NETIF_FLAG_LINK_UP;

#if LWIP_DHCP
    if (netif->dhcp) {
      dhcp_network_changed(netif);
    }
#endif /* LWIP_DHCP */

#if LWIP_AUTOIP
    if (netif->autoip) {
      autoip_network_changed(netif);
    }
#endif /* LWIP_AUTOIP */

    if (netif->flags & NETIF_FLAG_UP) {
#if LWIP_ARP
      /* For Ethernet network interfaces, we would like to send a "gratuitous ARP" */ 
      if (netif->flags & NETIF_FLAG_ETHARP) {
        etharp_gratuitous(netif);
      }
#endif /* LWIP_ARP */

#if LWIP_IGMP
      /* resend IGMP memberships */
      if (netif->flags & NETIF_FLAG_IGMP) {
        igmp_report_groups( netif);
      }
#endif /* LWIP_IGMP */
#if LWIP_IPV6 && LWIP_IPV6_MLD
      /* send mld memberships */
      mld6_report_groups( netif);
#endif /* LWIP_IPV6 && LWIP_IPV6_MLD */
    }
    NETIF_LINK_CALLBACK(netif);
  }
}

/**
 * Called by a driver when its link goes down
 */
void netif_set_link_down(struct netif *netif )
{
  if (netif->flags & NETIF_FLAG_LINK_UP) {
    netif->flags &= ~NETIF_FLAG_LINK_UP;
    NETIF_LINK_CALLBACK(netif);
  }
}

#if LWIP_NETIF_LINK_CALLBACK
/**
 * Set callback to be called when link is brought up/down
 */
void netif_set_link_callback(struct netif *netif, netif_status_callback_fn link_callback)
{
  if (netif) {
    netif->link_callback = link_callback;
  }
}
#endif /* LWIP_NETIF_LINK_CALLBACK */

#if ENABLE_LOOPBACK
/**
 * Send an IP packet to be received on the same netif (loopif-like).
 * The pbuf is simply copied and handed back to netif->input.
 * In multithreaded mode, this is done directly since netif->input must put
 * the packet on a queue.
 * In callback mode, the packet is put on an internal queue and is fed to
 * netif->input by netif_poll().
 *
 * @param netif the lwip network interface structure
 * @param p the (IP) packet to 'send'
 * @param ipaddr the ip address to send the packet to (not used)
 * @return ERR_OK if the packet has been sent
 *         ERR_MEM if the pbuf used to copy the packet couldn't be allocated
 */
err_t
netif_loop_output(struct netif *netif, struct pbuf *p,
       ip_addr_t *ipaddr)
{
  struct pbuf *r;
  err_t err;
  struct pbuf *last;
#if LWIP_LOOPBACK_MAX_PBUFS
  u8_t clen = 0;
#endif /* LWIP_LOOPBACK_MAX_PBUFS */
  /* If we have a loopif, SNMP counters are adjusted for it,
   * if not they are adjusted for 'netif'. */
#if LWIP_SNMP
#if LWIP_HAVE_LOOPIF
  struct netif *stats_if = &loop_netif;
#else /* LWIP_HAVE_LOOPIF */
  struct netif *stats_if = netif;
#endif /* LWIP_HAVE_LOOPIF */
#endif /* LWIP_SNMP */
  SYS_ARCH_DECL_PROTECT(lev);
  LWIP_UNUSED_ARG(ipaddr);

  /* Allocate a new pbuf */
  r = pbuf_alloc(PBUF_LINK, p->tot_len, PBUF_RAM);
  if (r == NULL) {
    LINK_STATS_INC(link.memerr);
    LINK_STATS_INC(link.drop);
    snmp_inc_ifoutdiscards(stats_if);
    return ERR_MEM;
  }
#if LWIP_LOOPBACK_MAX_PBUFS
  clen = pbuf_clen(r);
  /* check for overflow or too many pbuf on queue */
  if(((netif->loop_cnt_current + clen) < netif->loop_cnt_current) ||
     ((netif->loop_cnt_current + clen) > LWIP_LOOPBACK_MAX_PBUFS)) {
    pbuf_free(r);
    LINK_STATS_INC(link.memerr);
    LINK_STATS_INC(link.drop);
    snmp_inc_ifoutdiscards(stats_if);
    return ERR_MEM;
  }
  netif->loop_cnt_current += clen;
#endif /* LWIP_LOOPBACK_MAX_PBUFS */

  /* Copy the whole pbuf queue p into the single pbuf r */
  if ((err = pbuf_copy(r, p)) != ERR_OK) {
    pbuf_free(r);
    LINK_STATS_INC(link.memerr);
    LINK_STATS_INC(link.drop);
    snmp_inc_ifoutdiscards(stats_if);
    return err;
  }

  /* Put the packet on a linked list which gets emptied through calling
     netif_poll(). */

  /* let last point to the last pbuf in chain r */
  for (last = r; last->next != NULL; last = last->next);

  SYS_ARCH_PROTECT(lev);
  if(netif->loop_first != NULL) {
    LWIP_ASSERT("if first != NULL, last must also be != NULL", netif->loop_last != NULL);
    netif->loop_last->next = r;
    netif->loop_last = last;
  } else {
    netif->loop_first = r;
    netif->loop_last = last;
  }
  SYS_ARCH_UNPROTECT(lev);

  LINK_STATS_INC(link.xmit);
  snmp_add_ifoutoctets(stats_if, p->tot_len);
  snmp_inc_ifoutucastpkts(stats_if);

#if LWIP_NETIF_LOOPBACK_MULTITHREADING
  /* For multithreading environment, schedule a call to netif_poll */
  tcpip_callback((tcpip_callback_fn)netif_poll, netif);
#endif /* LWIP_NETIF_LOOPBACK_MULTITHREADING */

  return ERR_OK;
}

/**
 * Call netif_poll() in the main loop of your application. This is to prevent
 * reentering non-reentrant functions like tcp_input(). Packets passed to
 * netif_loop_output() are put on a list that is passed to netif->input() by
 * netif_poll().
 */
void
netif_poll(struct netif *netif)
{
  struct pbuf *in;
  /* If we have a loopif, SNMP counters are adjusted for it,
   * if not they are adjusted for 'netif'. */
#if LWIP_SNMP
#if LWIP_HAVE_LOOPIF
  struct netif *stats_if = &loop_netif;
#else /* LWIP_HAVE_LOOPIF */
  struct netif *stats_if = netif;
#endif /* LWIP_HAVE_LOOPIF */
#endif /* LWIP_SNMP */
  SYS_ARCH_DECL_PROTECT(lev);

  do {
    /* Get a packet from the list. With SYS_LIGHTWEIGHT_PROT=1, this is protected */
    SYS_ARCH_PROTECT(lev);
    in = netif->loop_first;
    if (in != NULL) {
      struct pbuf *in_end = in;
#if LWIP_LOOPBACK_MAX_PBUFS
      u8_t clen = pbuf_clen(in);
      /* adjust the number of pbufs on queue */
      LWIP_ASSERT("netif->loop_cnt_current underflow",
        ((netif->loop_cnt_current - clen) < netif->loop_cnt_current));
      netif->loop_cnt_current -= clen;
#endif /* LWIP_LOOPBACK_MAX_PBUFS */
      while (in_end->len != in_end->tot_len) {
        LWIP_ASSERT("bogus pbuf: len != tot_len but next == NULL!", in_end->next != NULL);
        in_end = in_end->next;
      }
      /* 'in_end' now points to the last pbuf from 'in' */
      if (in_end == netif->loop_last) {
        /* this was the last pbuf in the list */
        netif->loop_first = netif->loop_last = NULL;
      } else {
        /* pop the pbuf off the list */
        netif->loop_first = in_end->next;
        LWIP_ASSERT("should not be null since first != last!", netif->loop_first != NULL);
      }
      /* De-queue the pbuf from its successors on the 'loop_' list. */
      in_end->next = NULL;
    }
    SYS_ARCH_UNPROTECT(lev);

    if (in != NULL) {
      LINK_STATS_INC(link.recv);
      snmp_add_ifinoctets(stats_if, in->tot_len);
      snmp_inc_ifinucastpkts(stats_if);
      /* loopback packets are always IP packets! */
      if (ip_input(in, netif) != ERR_OK) {
        pbuf_free(in);
      }
      /* Don't reference the packet any more! */
      in = NULL;
    }
  /* go on while there is a packet on the list */
  } while (netif->loop_first != NULL);
}

#if !LWIP_NETIF_LOOPBACK_MULTITHREADING
/**
 * Calls netif_poll() for every netif on the netif_list.
 */
void
netif_poll_all(void)
{
  struct netif *netif = netif_list;
  /* loop through netifs */
  while (netif != NULL) {
    netif_poll(netif);
    /* proceed to next network interface */
    netif = netif->next;
  }
}
#endif /* !LWIP_NETIF_LOOPBACK_MULTITHREADING */
#endif /* ENABLE_LOOPBACK */

#if LWIP_IPV6
s8_t
netif_matches_ip6_addr(struct netif * netif, ip6_addr_t * ip6addr)
{
  s8_t i;
  for (i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {
    if (ip6_addr_cmp(netif_ip6_addr(netif, i), ip6addr)) {
      return i;
    }
  }
  return -1;
}

void
netif_create_ip6_linklocal_address(struct netif * netif, u8_t from_mac_48bit)
{
  u8_t i, addr_index;

  /* Link-local prefix. */
  netif->ip6_addr[0].addr[0] = PP_HTONL(0xfe800000ul);
  netif->ip6_addr[0].addr[1] = 0;

  /* Generate interface ID. */
  if (from_mac_48bit) {
    /* Assume hwaddr is a 48-bit IEEE 802 MAC. Convert to EUI-64 address. Complement Group bit. */
    netif->ip6_addr[0].addr[2] = htonl((((u32_t)(netif->hwaddr[0] ^ 0x02)) << 24) |
        ((u32_t)(netif->hwaddr[1]) << 16) |
        ((u32_t)(netif->hwaddr[2]) << 8) |
        (0xff));
    netif->ip6_addr[0].addr[3] = htonl((0xfeul << 24) |
        ((u32_t)(netif->hwaddr[3]) << 16) |
        ((u32_t)(netif->hwaddr[4]) << 8) |
        (netif->hwaddr[5]));
  }
  else {
    /* Use hwaddr directly as interface ID. */
    netif->ip6_addr[0].addr[2] = 0;
    netif->ip6_addr[0].addr[3] = 0;

    addr_index = 3;
    for (i = 0; i < 8; i++) {
      if (i == 4) {
        addr_index--;
      }
      netif->ip6_addr[0].addr[addr_index] |= ((u32_t)(netif->hwaddr[netif->hwaddr_len - i - 1])) << (8 * (i & 0x03));
    }
  }

  /* Set address state. */
#if LWIP_IPV6_DUP_DETECT_ATTEMPTS
  /* Will perform duplicate address detection (DAD). */
  netif->ip6_addr_state[0] = IP6_ADDR_TENTATIVE;
#else
  /* Consider address valid. */
  netif->ip6_addr_state[0] = IP6_ADDR_PREFERRED;
#endif /* LWIP_IPV6_AUTOCONFIG */
}

static err_t
netif_null_output_ip6(struct netif *netif, struct pbuf *p, ip6_addr_t *ipaddr)
{
    (void)netif;
    (void)p;
    (void)ipaddr;

    return ERR_IF;
}
#endif /* LWIP_IPV6 */
