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

/* Based on igmp.c implementation of igmp v2 protocol */

#include "lwip/opt.h"

#if LWIP_IPV6 && LWIP_IPV6_MLD  /* don't build if not configured for use in lwipopts.h */

#include "lwip/mld6.h"
#include "lwip/icmp6.h"
#include "lwip/ip6.h"
#include "lwip/ip6_addr.h"
#include "lwip/inet_chksum.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "lwip/memp.h"
#include "lwip/stats.h"

#include <string.h>


/*
 * MLD constants
 */
#define MLD6_HL                           1
#define MLD6_JOIN_DELAYING_MEMBER_TMR_MS  (500)

#define MLD6_GROUP_NON_MEMBER             0
#define MLD6_GROUP_DELAYING_MEMBER        1
#define MLD6_GROUP_IDLE_MEMBER            2


/* The list of joined groups. */
static struct mld_group* mld_group_list;


/* Forward declarations. */
static struct mld_group * mld6_new_group(struct netif *ifp, ip6_addr_t *addr);
static err_t mld6_free_group(struct mld_group *group);
static void mld6_delayed_report(struct mld_group *group, u16_t maxresp);
static void mld6_send(struct mld_group *group, u8_t type);


/**
 * Stop MLD processing on interface
 *
 * @param netif network interface on which stop MLD processing
 */
err_t
mld6_stop(struct netif *netif)
{
  struct mld_group *group = mld_group_list;
  struct mld_group *prev  = NULL;
  struct mld_group *next;

  /* look for groups joined on this interface further down the list */
  while (group != NULL) {
    next = group->next;
    /* is it a group joined on this interface? */
    if (group->netif == netif) {
      /* is it the first group of the list? */
      if (group == mld_group_list) {
        mld_group_list = next;
      }
      /* is there a "previous" group defined? */
      if (prev != NULL) {
        prev->next = next;
      }
      /* disable the group at the MAC level */
      if (netif->mld_mac_filter != NULL) {
        netif->mld_mac_filter(netif, &(group->group_address), MLD6_DEL_MAC_FILTER);
      }
      /* free group */
      memp_free(MEMP_MLD6_GROUP, group);
    } else {
      /* change the "previous" */
      prev = group;
    }
    /* move to "next" */
    group = next;
  }
  return ERR_OK;
}

/**
 * Report MLD memberships for this interface
 *
 * @param netif network interface on which report MLD memberships
 */
void
mld6_report_groups(struct netif *netif)
{
  struct mld_group *group = mld_group_list;

  while (group != NULL) {
    if (group->netif == netif) {
      mld6_delayed_report(group, MLD6_JOIN_DELAYING_MEMBER_TMR_MS);
    }
    group = group->next;
  }
}

/**
 * Search for a group that is joined on a netif
 *
 * @param ifp the network interface for which to look
 * @param addr the group ipv6 address to search for
 * @return a struct mld_group* if the group has been found,
 *         NULL if the group wasn't found.
 */
struct mld_group *
mld6_lookfor_group(struct netif *ifp, ip6_addr_t *addr)
{
  struct mld_group *group = mld_group_list;

  while (group != NULL) {
    if ((group->netif == ifp) && (ip6_addr_cmp(&(group->group_address), addr))) {
      return group;
    }
    group = group->next;
  }

  return NULL;
}


/**
 * create a new group
 *
 * @param ifp the network interface for which to create
 * @param addr the new group ipv6
 * @return a struct mld_group*,
 *         NULL on memory error.
 */
static struct mld_group *
mld6_new_group(struct netif *ifp, ip6_addr_t *addr)
{
  struct mld_group *group;

  group = (struct mld_group *)memp_malloc(MEMP_MLD6_GROUP);
  if (group != NULL) {
    group->netif              = ifp;
    ip6_addr_set(&(group->group_address), addr);
    group->timer              = 0; /* Not running */
    group->group_state        = MLD6_GROUP_IDLE_MEMBER;
    group->last_reporter_flag = 0;
    group->use                = 0;
    group->next               = mld_group_list;

    mld_group_list = group;
  }

  return group;
}

/**
 * Remove a group in the mld_group_list and free
 *
 * @param group the group to remove
 * @return ERR_OK if group was removed from the list, an err_t otherwise
 */
static err_t
mld6_free_group(struct mld_group *group)
{
  err_t err = ERR_OK;

  /* Is it the first group? */
  if (mld_group_list == group) {
    mld_group_list = group->next;
  } else {
    /* look for group further down the list */
    struct mld_group *tmpGroup;
    for (tmpGroup = mld_group_list; tmpGroup != NULL; tmpGroup = tmpGroup->next) {
      if (tmpGroup->next == group) {
        tmpGroup->next = group->next;
        break;
      }
    }
    /* Group not find group */
    if (tmpGroup == NULL)
      err = ERR_ARG;
  }
  /* free group */
  memp_free(MEMP_MLD6_GROUP, group);

  return err;
}


/**
 * Process an input MLD message. Called by icmp6_input.
 *
 * @param p the mld packet, p->payload pointing to the icmpv6 header
 * @param inp the netif on which this packet was received
 */
void
mld6_input(struct pbuf *p, struct netif *inp)
{
  struct mld_header * mld_hdr;
  struct mld_group* group;

  MLD6_STATS_INC(mld6.recv);

  /* Check that mld header fits in packet. */
  if (p->len < sizeof(struct mld_header)) {
    /* TODO debug message */
    pbuf_free(p);
    MLD6_STATS_INC(mld6.lenerr);
    MLD6_STATS_INC(mld6.drop);
    return;
  }

  mld_hdr = (struct mld_header *)p->payload;

  switch (mld_hdr->type) {
  case ICMP6_TYPE_MLQ: /* Multicast listener query. */
  {
    /* Is it a general query? */
    if (ip6_addr_isallnodes_linklocal(ip6_current_dest_addr()) &&
        ip6_addr_isany(&(mld_hdr->multicast_address))) {
      MLD6_STATS_INC(mld6.rx_general);
      /* Report all groups, except all nodes group, and if-local groups. */
      group = mld_group_list;
      while (group != NULL) {
        if ((group->netif == inp) &&
            (!(ip6_addr_ismulticast_iflocal(&(group->group_address)))) &&
            (!(ip6_addr_isallnodes_linklocal(&(group->group_address))))) {
          mld6_delayed_report(group, mld_hdr->max_resp_delay);
        }
        group = group->next;
      }
    }
    else {
      /* Have we joined this group?
       * We use IP6 destination address to have a memory aligned copy.
       * mld_hdr->multicast_address should be the same. */
      MLD6_STATS_INC(mld6.rx_group);
      group = mld6_lookfor_group(inp, ip6_current_dest_addr());
      if (group != NULL) {
        /* Schedule a report. */
        mld6_delayed_report(group, mld_hdr->max_resp_delay);
      }
    }
    break; /* ICMP6_TYPE_MLQ */
  }
  case ICMP6_TYPE_MLR: /* Multicast listener report. */
  {
    /* Have we joined this group?
     * We use IP6 destination address to have a memory aligned copy.
     * mld_hdr->multicast_address should be the same. */
    MLD6_STATS_INC(mld6.rx_report);
    group = mld6_lookfor_group(inp, ip6_current_dest_addr());
    if (group != NULL) {
      /* If we are waiting to report, cancel it. */
      if (group->group_state == MLD6_GROUP_DELAYING_MEMBER) {
        group->timer = 0; /* stopped */
        group->group_state = MLD6_GROUP_IDLE_MEMBER;
        group->last_reporter_flag = 0;
      }
    }
    break; /* ICMP6_TYPE_MLR */
  }
  case ICMP6_TYPE_MLD: /* Multicast listener done. */
  {
    /* Do nothing, router will query us. */
    break; /* ICMP6_TYPE_MLD */
  }
  default:
    MLD6_STATS_INC(mld6.proterr);
    MLD6_STATS_INC(mld6.drop);
    break;
  }

  pbuf_free(p);
}

/**
 * Join a group on a network interface.
 *
 * @param srcaddr ipv6 address of the network interface which should
 *                join a new group. If IP6_ADDR_ANY, join on all netifs
 * @param groupaddr the ipv6 address of the group to join
 * @return ERR_OK if group was joined on the netif(s), an err_t otherwise
 */
err_t
mld6_joingroup(ip6_addr_t *srcaddr, ip6_addr_t *groupaddr)
{
  err_t              err = ERR_VAL; /* no matching interface */
  struct mld_group  *group;
  struct netif      *netif;
  u8_t               match;
  u8_t               i;

  /* loop through netif's */
  netif = netif_list;
  while (netif != NULL) {
    /* Should we join this interface ? */
    match = 0;
    if (ip6_addr_isany(srcaddr)) {
      match = 1;
    }
    else {
      for (i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {
        if (ip6_addr_cmp(srcaddr, netif_ip6_addr(netif, i))) {
          match = 1;
          break;
        }
      }
    }
    if (match) {
      /* find group or create a new one if not found */
      group = mld6_lookfor_group(netif, groupaddr);

      if (group == NULL) {
        /* Joining a new group. Create a new group entry. */
        group = mld6_new_group(netif, groupaddr);
        if (group == NULL) {
          return ERR_MEM;
        }

        /* Activate this address on the MAC layer. */
        if (netif->mld_mac_filter != NULL) {
          netif->mld_mac_filter(netif, groupaddr, MLD6_ADD_MAC_FILTER);
        }

        /* Report our membership. */
        MLD6_STATS_INC(mld6.tx_report);
        mld6_send(group, ICMP6_TYPE_MLR);
        mld6_delayed_report(group, MLD6_JOIN_DELAYING_MEMBER_TMR_MS);
      }

      /* Increment group use */
      group->use++;
      err = ERR_OK;
    }

    /* proceed to next network interface */
    netif = netif->next;
  }

  return err;
}

/**
 * Leave a group on a network interface.
 *
 * @param srcaddr ipv6 address of the network interface which should
 *                leave the group. If IP6_ISANY, leave on all netifs
 * @param groupaddr the ipv6 address of the group to leave
 * @return ERR_OK if group was left on the netif(s), an err_t otherwise
 */
err_t
mld6_leavegroup(ip6_addr_t *srcaddr, ip6_addr_t *groupaddr)
{
  err_t              err = ERR_VAL; /* no matching interface */
  struct mld_group  *group;
  struct netif      *netif;
  u8_t               match;
  u8_t               i;

  /* loop through netif's */
  netif = netif_list;
  while (netif != NULL) {
    /* Should we leave this interface ? */
    match = 0;
    if (ip6_addr_isany(srcaddr)) {
      match = 1;
    }
    else {
      for (i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {
        if (ip6_addr_cmp(srcaddr, netif_ip6_addr(netif, i))) {
          match = 1;
          break;
        }
      }
    }
    if (match) {
      /* find group */
      group = mld6_lookfor_group(netif, groupaddr);

      if (group != NULL) {
        /* Leave if there is no other use of the group */
        if (group->use <= 1) {
          /* If we are the last reporter for this group */
          if (group->last_reporter_flag) {
            MLD6_STATS_INC(mld6.tx_leave);
            mld6_send(group, ICMP6_TYPE_MLD);
          }

          /* Disable the group at the MAC level */
          if (netif->mld_mac_filter != NULL) {
            netif->mld_mac_filter(netif, groupaddr, MLD6_DEL_MAC_FILTER);
          }

          /* Free the group */
          mld6_free_group(group);
        } else {
          /* Decrement group use */
          group->use--;
        }
        /* Leave on this interface */
        err = ERR_OK;
      }
    }
    /* proceed to next network interface */
    netif = netif->next;
  }

  return err;
}


/**
 * Periodic timer for mld processing. Must be called every
 * MLD6_TMR_INTERVAL milliseconds (100).
 *
 * When a delaying member expires, a membership report is sent.
 */
void
mld6_tmr(void)
{
  struct mld_group *group = mld_group_list;

  while (group != NULL) {
    if (group->timer > 0) {
      group->timer--;
      if (group->timer == 0) {
        /* If the state is MLD6_GROUP_DELAYING_MEMBER then we send a report for this group */
        if (group->group_state == MLD6_GROUP_DELAYING_MEMBER) {
          MLD6_STATS_INC(mld6.tx_report);
          mld6_send(group, ICMP6_TYPE_MLR);
          group->group_state = MLD6_GROUP_IDLE_MEMBER;
        }
      }
    }
    group = group->next;
  }
}

/**
 * Schedule a delayed membership report for a group
 *
 * @param group the mld_group for which "delaying" membership report
 *              should be sent
 * @param maxresp the max resp delay provided in the query
 */
static void
mld6_delayed_report(struct mld_group *group, u16_t maxresp)
{
  /* Convert maxresp from milliseconds to tmr ticks */
  maxresp = maxresp / MLD6_TMR_INTERVAL;
  if (maxresp == 0) {
    maxresp = 1;
  }

#ifdef LWIP_RAND
  /* Randomize maxresp. (if LWIP_RAND is supported) */
  maxresp = (LWIP_RAND() % (maxresp - 1)) + 1;
#endif /* LWIP_RAND */

  /* Apply timer value if no report has been scheduled already. */
  if ((group->group_state == MLD6_GROUP_IDLE_MEMBER) ||
     ((group->group_state == MLD6_GROUP_DELAYING_MEMBER) &&
      ((group->timer == 0) || (maxresp < group->timer)))) {
    group->timer = maxresp;
    group->group_state = MLD6_GROUP_DELAYING_MEMBER;
  }
}

/**
 * Send a MLD message (report or done).
 *
 * An IPv6 hop-by-hop options header with a router alert option
 * is prepended.
 *
 * @param group the group to report or quit
 * @param type ICMP6_TYPE_MLR (report) or ICMP6_TYPE_MLD (done)
 */
static void
mld6_send(struct mld_group *group, u8_t type)
{
  struct mld_header * mld_hdr;
  struct pbuf * p;
  ip6_addr_t * src_addr;

  /* Allocate a packet. Size is MLD header + IPv6 Hop-by-hop options header. */
  p = pbuf_alloc(PBUF_IP, sizeof(struct mld_header) + sizeof(struct ip6_hbh_hdr), PBUF_RAM);
  if ((p == NULL) || (p->len < (sizeof(struct mld_header) + sizeof(struct ip6_hbh_hdr)))) {
    /* We couldn't allocate a suitable pbuf. drop it. */
    if (p != NULL) {
      pbuf_free(p);
    }
    MLD6_STATS_INC(mld6.memerr);
    return;
  }

  /* Move to make room for Hop-by-hop options header. */
  if (pbuf_header(p, -IP6_HBH_HLEN)) {
    pbuf_free(p);
    MLD6_STATS_INC(mld6.lenerr);
    return;
  }

  /* Select our source address. */
  if (!ip6_addr_isvalid(netif_ip6_addr_state(group->netif, 0))) {
    /* This is a special case, when we are performing duplicate address detection.
     * We must join the multicast group, but we don't have a valid address yet. */
    src_addr = IP6_ADDR_ANY;
  } else {
    /* Use link-local address as source address. */
    src_addr = netif_ip6_addr(group->netif, 0);
  }

  /* MLD message header pointer. */
  mld_hdr = (struct mld_header *)p->payload;

  /* Set fields. */
  mld_hdr->type = type;
  mld_hdr->code = 0;
  mld_hdr->chksum = 0;
  mld_hdr->max_resp_delay = 0;
  mld_hdr->reserved = 0;
  ip6_addr_set(&(mld_hdr->multicast_address), &(group->group_address));

  mld_hdr->chksum = ip6_chksum_pseudo(p, IP6_NEXTH_ICMP6, p->len,
    src_addr, &(group->group_address));

  /* Add hop-by-hop headers options: router alert with MLD value. */
  ip6_options_add_hbh_ra(p, IP6_NEXTH_ICMP6, IP6_ROUTER_ALERT_VALUE_MLD);

  /* Send the packet out. */
  MLD6_STATS_INC(mld6.xmit);
  ip6_output_if(p, (ip6_addr_isany(src_addr)) ? NULL : src_addr, &(group->group_address),
      MLD6_HL, 0, IP6_NEXTH_HOPBYHOP, group->netif);
  pbuf_free(p);
}



#endif /* LWIP_IPV6 */
