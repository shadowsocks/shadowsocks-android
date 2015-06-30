/**
 * @file
 * IGMP - Internet Group Management Protocol
 *
 */

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

/*-------------------------------------------------------------
Note 1)
Although the rfc requires V1 AND V2 capability
we will only support v2 since now V1 is very old (August 1989)
V1 can be added if required

a debug print and statistic have been implemented to
show this up.
-------------------------------------------------------------
-------------------------------------------------------------
Note 2)
A query for a specific group address (as opposed to ALLHOSTS)
has now been implemented as I am unsure if it is required

a debug print and statistic have been implemented to
show this up.
-------------------------------------------------------------
-------------------------------------------------------------
Note 3)
The router alert rfc 2113 is implemented in outgoing packets
but not checked rigorously incoming
-------------------------------------------------------------
Steve Reynolds
------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
 * RFC 988  - Host extensions for IP multicasting                         - V0
 * RFC 1054 - Host extensions for IP multicasting                         -
 * RFC 1112 - Host extensions for IP multicasting                         - V1
 * RFC 2236 - Internet Group Management Protocol, Version 2               - V2  <- this code is based on this RFC (it's the "de facto" standard)
 * RFC 3376 - Internet Group Management Protocol, Version 3               - V3
 * RFC 4604 - Using Internet Group Management Protocol Version 3...       - V3+
 * RFC 2113 - IP Router Alert Option                                      - 
 *----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
 * Includes
 *----------------------------------------------------------------------------*/

#include "lwip/opt.h"

#if LWIP_IGMP /* don't build if not configured for use in lwipopts.h */

#include "lwip/igmp.h"
#include "lwip/debug.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/ip.h"
#include "lwip/inet_chksum.h"
#include "lwip/netif.h"
#include "lwip/icmp.h"
#include "lwip/udp.h"
#include "lwip/tcp.h"
#include "lwip/stats.h"

#include "string.h"

/* 
 * IGMP constants
 */
#define IGMP_TTL                       1
#define IGMP_MINLEN                    8
#define ROUTER_ALERT                   0x9404U
#define ROUTER_ALERTLEN                4

/*
 * IGMP message types, including version number.
 */
#define IGMP_MEMB_QUERY                0x11 /* Membership query         */
#define IGMP_V1_MEMB_REPORT            0x12 /* Ver. 1 membership report */
#define IGMP_V2_MEMB_REPORT            0x16 /* Ver. 2 membership report */
#define IGMP_LEAVE_GROUP               0x17 /* Leave-group message      */

/* Group  membership states */
#define IGMP_GROUP_NON_MEMBER          0
#define IGMP_GROUP_DELAYING_MEMBER     1
#define IGMP_GROUP_IDLE_MEMBER         2

/**
 * IGMP packet format.
 */
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/bpstruct.h"
#endif
PACK_STRUCT_BEGIN
struct igmp_msg {
 PACK_STRUCT_FIELD(u8_t           igmp_msgtype);
 PACK_STRUCT_FIELD(u8_t           igmp_maxresp);
 PACK_STRUCT_FIELD(u16_t          igmp_checksum);
 PACK_STRUCT_FIELD(ip_addr_p_t    igmp_group_address);
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/epstruct.h"
#endif


static struct igmp_group *igmp_lookup_group(struct netif *ifp, ip_addr_t *addr);
static err_t  igmp_remove_group(struct igmp_group *group);
static void   igmp_timeout( struct igmp_group *group);
static void   igmp_start_timer(struct igmp_group *group, u8_t max_time);
static void   igmp_delaying_member(struct igmp_group *group, u8_t maxresp);
static err_t  igmp_ip_output_if(struct pbuf *p, ip_addr_t *src, ip_addr_t *dest, struct netif *netif);
static void   igmp_send(struct igmp_group *group, u8_t type);


static struct igmp_group* igmp_group_list;
static ip_addr_t     allsystems;
static ip_addr_t     allrouters;


/**
 * Initialize the IGMP module
 */
void
igmp_init(void)
{
  LWIP_DEBUGF(IGMP_DEBUG, ("igmp_init: initializing\n"));

  IP4_ADDR(&allsystems, 224, 0, 0, 1);
  IP4_ADDR(&allrouters, 224, 0, 0, 2);
}

#ifdef LWIP_DEBUG
/**
 * Dump global IGMP groups list
 */
void
igmp_dump_group_list()
{ 
  struct igmp_group *group = igmp_group_list;

  while (group != NULL) {
    LWIP_DEBUGF(IGMP_DEBUG, ("igmp_dump_group_list: [%"U32_F"] ", (u32_t)(group->group_state)));
    ip_addr_debug_print(IGMP_DEBUG, &group->group_address);
    LWIP_DEBUGF(IGMP_DEBUG, (" on if %p\n", group->netif));
    group = group->next;
  }
  LWIP_DEBUGF(IGMP_DEBUG, ("\n"));
}
#else
#define igmp_dump_group_list()
#endif /* LWIP_DEBUG */

/**
 * Start IGMP processing on interface
 *
 * @param netif network interface on which start IGMP processing
 */
err_t
igmp_start(struct netif *netif)
{
  struct igmp_group* group;

  LWIP_DEBUGF(IGMP_DEBUG, ("igmp_start: starting IGMP processing on if %p\n", netif));

  group = igmp_lookup_group(netif, &allsystems);

  if (group != NULL) {
    group->group_state = IGMP_GROUP_IDLE_MEMBER;
    group->use++;

    /* Allow the igmp messages at the MAC level */
    if (netif->igmp_mac_filter != NULL) {
      LWIP_DEBUGF(IGMP_DEBUG, ("igmp_start: igmp_mac_filter(ADD "));
      ip_addr_debug_print(IGMP_DEBUG, &allsystems);
      LWIP_DEBUGF(IGMP_DEBUG, (") on if %p\n", netif));
      netif->igmp_mac_filter(netif, &allsystems, IGMP_ADD_MAC_FILTER);
    }

    return ERR_OK;
  }

  return ERR_MEM;
}

/**
 * Stop IGMP processing on interface
 *
 * @param netif network interface on which stop IGMP processing
 */
err_t
igmp_stop(struct netif *netif)
{
  struct igmp_group *group = igmp_group_list;
  struct igmp_group *prev  = NULL;
  struct igmp_group *next;

  /* look for groups joined on this interface further down the list */
  while (group != NULL) {
    next = group->next;
    /* is it a group joined on this interface? */
    if (group->netif == netif) {
      /* is it the first group of the list? */
      if (group == igmp_group_list) {
        igmp_group_list = next;
      }
      /* is there a "previous" group defined? */
      if (prev != NULL) {
        prev->next = next;
      }
      /* disable the group at the MAC level */
      if (netif->igmp_mac_filter != NULL) {
        LWIP_DEBUGF(IGMP_DEBUG, ("igmp_stop: igmp_mac_filter(DEL "));
        ip_addr_debug_print(IGMP_DEBUG, &group->group_address);
        LWIP_DEBUGF(IGMP_DEBUG, (") on if %p\n", netif));
        netif->igmp_mac_filter(netif, &(group->group_address), IGMP_DEL_MAC_FILTER);
      }
      /* free group */
      memp_free(MEMP_IGMP_GROUP, group);
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
 * Report IGMP memberships for this interface
 *
 * @param netif network interface on which report IGMP memberships
 */
void
igmp_report_groups(struct netif *netif)
{
  struct igmp_group *group = igmp_group_list;

  LWIP_DEBUGF(IGMP_DEBUG, ("igmp_report_groups: sending IGMP reports on if %p\n", netif));

  while (group != NULL) {
    if (group->netif == netif) {
      igmp_delaying_member(group, IGMP_JOIN_DELAYING_MEMBER_TMR);
    }
    group = group->next;
  }
}

/**
 * Search for a group in the global igmp_group_list
 *
 * @param ifp the network interface for which to look
 * @param addr the group ip address to search for
 * @return a struct igmp_group* if the group has been found,
 *         NULL if the group wasn't found.
 */
struct igmp_group *
igmp_lookfor_group(struct netif *ifp, ip_addr_t *addr)
{
  struct igmp_group *group = igmp_group_list;

  while (group != NULL) {
    if ((group->netif == ifp) && (ip_addr_cmp(&(group->group_address), addr))) {
      return group;
    }
    group = group->next;
  }

  /* to be clearer, we return NULL here instead of
   * 'group' (which is also NULL at this point).
   */
  return NULL;
}

/**
 * Search for a specific igmp group and create a new one if not found-
 *
 * @param ifp the network interface for which to look
 * @param addr the group ip address to search
 * @return a struct igmp_group*,
 *         NULL on memory error.
 */
struct igmp_group *
igmp_lookup_group(struct netif *ifp, ip_addr_t *addr)
{
  struct igmp_group *group = igmp_group_list;
  
  /* Search if the group already exists */
  group = igmp_lookfor_group(ifp, addr);
  if (group != NULL) {
    /* Group already exists. */
    return group;
  }

  /* Group doesn't exist yet, create a new one */
  group = (struct igmp_group *)memp_malloc(MEMP_IGMP_GROUP);
  if (group != NULL) {
    group->netif              = ifp;
    ip_addr_set(&(group->group_address), addr);
    group->timer              = 0; /* Not running */
    group->group_state        = IGMP_GROUP_NON_MEMBER;
    group->last_reporter_flag = 0;
    group->use                = 0;
    group->next               = igmp_group_list;
    
    igmp_group_list = group;
  }

  LWIP_DEBUGF(IGMP_DEBUG, ("igmp_lookup_group: %sallocated a new group with address ", (group?"":"impossible to ")));
  ip_addr_debug_print(IGMP_DEBUG, addr);
  LWIP_DEBUGF(IGMP_DEBUG, (" on if %p\n", ifp));

  return group;
}

/**
 * Remove a group in the global igmp_group_list
 *
 * @param group the group to remove from the global igmp_group_list
 * @return ERR_OK if group was removed from the list, an err_t otherwise
 */
static err_t
igmp_remove_group(struct igmp_group *group)
{
  err_t err = ERR_OK;

  /* Is it the first group? */
  if (igmp_group_list == group) {
    igmp_group_list = group->next;
  } else {
    /* look for group further down the list */
    struct igmp_group *tmpGroup;
    for (tmpGroup = igmp_group_list; tmpGroup != NULL; tmpGroup = tmpGroup->next) {
      if (tmpGroup->next == group) {
        tmpGroup->next = group->next;
        break;
      }
    }
    /* Group not found in the global igmp_group_list */
    if (tmpGroup == NULL)
      err = ERR_ARG;
  }
  /* free group */
  memp_free(MEMP_IGMP_GROUP, group);

  return err;
}

/**
 * Called from ip_input() if a new IGMP packet is received.
 *
 * @param p received igmp packet, p->payload pointing to the igmp header
 * @param inp network interface on which the packet was received
 * @param dest destination ip address of the igmp packet
 */
void
igmp_input(struct pbuf *p, struct netif *inp, ip_addr_t *dest)
{
  struct igmp_msg*   igmp;
  struct igmp_group* group;
  struct igmp_group* groupref;

  IGMP_STATS_INC(igmp.recv);

  /* Note that the length CAN be greater than 8 but only 8 are used - All are included in the checksum */    
  if (p->len < IGMP_MINLEN) {
    pbuf_free(p);
    IGMP_STATS_INC(igmp.lenerr);
    LWIP_DEBUGF(IGMP_DEBUG, ("igmp_input: length error\n"));
    return;
  }

  LWIP_DEBUGF(IGMP_DEBUG, ("igmp_input: message from "));
  ip_addr_debug_print(IGMP_DEBUG, &(ip_current_header()->src));
  LWIP_DEBUGF(IGMP_DEBUG, (" to address "));
  ip_addr_debug_print(IGMP_DEBUG, &(ip_current_header()->dest));
  LWIP_DEBUGF(IGMP_DEBUG, (" on if %p\n", inp));

  /* Now calculate and check the checksum */
  igmp = (struct igmp_msg *)p->payload;
  if (inet_chksum(igmp, p->len)) {
    pbuf_free(p);
    IGMP_STATS_INC(igmp.chkerr);
    LWIP_DEBUGF(IGMP_DEBUG, ("igmp_input: checksum error\n"));
    return;
  }

  /* Packet is ok so find an existing group */
  group = igmp_lookfor_group(inp, dest); /* use the destination IP address of incoming packet */
  
  /* If group can be found or create... */
  if (!group) {
    pbuf_free(p);
    IGMP_STATS_INC(igmp.drop);
    LWIP_DEBUGF(IGMP_DEBUG, ("igmp_input: IGMP frame not for us\n"));
    return;
  }

  /* NOW ACT ON THE INCOMING MESSAGE TYPE... */
  switch (igmp->igmp_msgtype) {
   case IGMP_MEMB_QUERY: {
     /* IGMP_MEMB_QUERY to the "all systems" address ? */
     if ((ip_addr_cmp(dest, &allsystems)) && ip_addr_isany(&igmp->igmp_group_address)) {
       /* THIS IS THE GENERAL QUERY */
       LWIP_DEBUGF(IGMP_DEBUG, ("igmp_input: General IGMP_MEMB_QUERY on \"ALL SYSTEMS\" address (224.0.0.1) [igmp_maxresp=%i]\n", (int)(igmp->igmp_maxresp)));

       if (igmp->igmp_maxresp == 0) {
         IGMP_STATS_INC(igmp.rx_v1);
         LWIP_DEBUGF(IGMP_DEBUG, ("igmp_input: got an all hosts query with time== 0 - this is V1 and not implemented - treat as v2\n"));
         igmp->igmp_maxresp = IGMP_V1_DELAYING_MEMBER_TMR;
       } else {
         IGMP_STATS_INC(igmp.rx_general);
       }

       groupref = igmp_group_list;
       while (groupref) {
         /* Do not send messages on the all systems group address! */
         if ((groupref->netif == inp) && (!(ip_addr_cmp(&(groupref->group_address), &allsystems)))) {
           igmp_delaying_member(groupref, igmp->igmp_maxresp);
         }
         groupref = groupref->next;
       }
     } else {
       /* IGMP_MEMB_QUERY to a specific group ? */
       if (!ip_addr_isany(&igmp->igmp_group_address)) {
         LWIP_DEBUGF(IGMP_DEBUG, ("igmp_input: IGMP_MEMB_QUERY to a specific group "));
         ip_addr_debug_print(IGMP_DEBUG, &igmp->igmp_group_address);
         if (ip_addr_cmp(dest, &allsystems)) {
           ip_addr_t groupaddr;
           LWIP_DEBUGF(IGMP_DEBUG, (" using \"ALL SYSTEMS\" address (224.0.0.1) [igmp_maxresp=%i]\n", (int)(igmp->igmp_maxresp)));
           /* we first need to re-look for the group since we used dest last time */
           ip_addr_copy(groupaddr, igmp->igmp_group_address);
           group = igmp_lookfor_group(inp, &groupaddr);
         } else {
           LWIP_DEBUGF(IGMP_DEBUG, (" with the group address as destination [igmp_maxresp=%i]\n", (int)(igmp->igmp_maxresp)));
         }

         if (group != NULL) {
           IGMP_STATS_INC(igmp.rx_group);
           igmp_delaying_member(group, igmp->igmp_maxresp);
         } else {
           IGMP_STATS_INC(igmp.drop);
         }
       } else {
         IGMP_STATS_INC(igmp.proterr);
       }
     }
     break;
   }
   case IGMP_V2_MEMB_REPORT: {
     LWIP_DEBUGF(IGMP_DEBUG, ("igmp_input: IGMP_V2_MEMB_REPORT\n"));
     IGMP_STATS_INC(igmp.rx_report);
     if (group->group_state == IGMP_GROUP_DELAYING_MEMBER) {
       /* This is on a specific group we have already looked up */
       group->timer = 0; /* stopped */
       group->group_state = IGMP_GROUP_IDLE_MEMBER;
       group->last_reporter_flag = 0;
     }
     break;
   }
   default: {
     LWIP_DEBUGF(IGMP_DEBUG, ("igmp_input: unexpected msg %d in state %d on group %p on if %p\n",
       igmp->igmp_msgtype, group->group_state, &group, group->netif));
     IGMP_STATS_INC(igmp.proterr);
     break;
   }
  }

  pbuf_free(p);
  return;
}

/**
 * Join a group on one network interface.
 *
 * @param ifaddr ip address of the network interface which should join a new group
 * @param groupaddr the ip address of the group which to join
 * @return ERR_OK if group was joined on the netif(s), an err_t otherwise
 */
err_t
igmp_joingroup(ip_addr_t *ifaddr, ip_addr_t *groupaddr)
{
  err_t              err = ERR_VAL; /* no matching interface */
  struct igmp_group *group;
  struct netif      *netif;

  /* make sure it is multicast address */
  LWIP_ERROR("igmp_joingroup: attempt to join non-multicast address", ip_addr_ismulticast(groupaddr), return ERR_VAL;);
  LWIP_ERROR("igmp_joingroup: attempt to join allsystems address", (!ip_addr_cmp(groupaddr, &allsystems)), return ERR_VAL;);

  /* loop through netif's */
  netif = netif_list;
  while (netif != NULL) {
    /* Should we join this interface ? */
    if ((netif->flags & NETIF_FLAG_IGMP) && ((ip_addr_isany(ifaddr) || ip_addr_cmp(&(netif->ip_addr), ifaddr)))) {
      /* find group or create a new one if not found */
      group = igmp_lookup_group(netif, groupaddr);

      if (group != NULL) {
        /* This should create a new group, check the state to make sure */
        if (group->group_state != IGMP_GROUP_NON_MEMBER) {
          LWIP_DEBUGF(IGMP_DEBUG, ("igmp_joingroup: join to group not in state IGMP_GROUP_NON_MEMBER\n"));
        } else {
          /* OK - it was new group */
          LWIP_DEBUGF(IGMP_DEBUG, ("igmp_joingroup: join to new group: "));
          ip_addr_debug_print(IGMP_DEBUG, groupaddr);
          LWIP_DEBUGF(IGMP_DEBUG, ("\n"));

          /* If first use of the group, allow the group at the MAC level */
          if ((group->use==0) && (netif->igmp_mac_filter != NULL)) {
            LWIP_DEBUGF(IGMP_DEBUG, ("igmp_joingroup: igmp_mac_filter(ADD "));
            ip_addr_debug_print(IGMP_DEBUG, groupaddr);
            LWIP_DEBUGF(IGMP_DEBUG, (") on if %p\n", netif));
            netif->igmp_mac_filter(netif, groupaddr, IGMP_ADD_MAC_FILTER);
          }

          IGMP_STATS_INC(igmp.tx_join);
          igmp_send(group, IGMP_V2_MEMB_REPORT);

          igmp_start_timer(group, IGMP_JOIN_DELAYING_MEMBER_TMR);

          /* Need to work out where this timer comes from */
          group->group_state = IGMP_GROUP_DELAYING_MEMBER;
        }
        /* Increment group use */
        group->use++;
        /* Join on this interface */
        err = ERR_OK;
      } else {
        /* Return an error even if some network interfaces are joined */
        /** @todo undo any other netif already joined */
        LWIP_DEBUGF(IGMP_DEBUG, ("igmp_joingroup: Not enought memory to join to group\n"));
        return ERR_MEM;
      }
    }
    /* proceed to next network interface */
    netif = netif->next;
  }

  return err;
}

/**
 * Leave a group on one network interface.
 *
 * @param ifaddr ip address of the network interface which should leave a group
 * @param groupaddr the ip address of the group which to leave
 * @return ERR_OK if group was left on the netif(s), an err_t otherwise
 */
err_t
igmp_leavegroup(ip_addr_t *ifaddr, ip_addr_t *groupaddr)
{
  err_t              err = ERR_VAL; /* no matching interface */
  struct igmp_group *group;
  struct netif      *netif;

  /* make sure it is multicast address */
  LWIP_ERROR("igmp_leavegroup: attempt to leave non-multicast address", ip_addr_ismulticast(groupaddr), return ERR_VAL;);
  LWIP_ERROR("igmp_leavegroup: attempt to leave allsystems address", (!ip_addr_cmp(groupaddr, &allsystems)), return ERR_VAL;);

  /* loop through netif's */
  netif = netif_list;
  while (netif != NULL) {
    /* Should we leave this interface ? */
    if ((netif->flags & NETIF_FLAG_IGMP) && ((ip_addr_isany(ifaddr) || ip_addr_cmp(&(netif->ip_addr), ifaddr)))) {
      /* find group */
      group = igmp_lookfor_group(netif, groupaddr);

      if (group != NULL) {
        /* Only send a leave if the flag is set according to the state diagram */
        LWIP_DEBUGF(IGMP_DEBUG, ("igmp_leavegroup: Leaving group: "));
        ip_addr_debug_print(IGMP_DEBUG, groupaddr);
        LWIP_DEBUGF(IGMP_DEBUG, ("\n"));

        /* If there is no other use of the group */
        if (group->use <= 1) {
          /* If we are the last reporter for this group */
          if (group->last_reporter_flag) {
            LWIP_DEBUGF(IGMP_DEBUG, ("igmp_leavegroup: sending leaving group\n"));
            IGMP_STATS_INC(igmp.tx_leave);
            igmp_send(group, IGMP_LEAVE_GROUP);
          }
          
          /* Disable the group at the MAC level */
          if (netif->igmp_mac_filter != NULL) {
            LWIP_DEBUGF(IGMP_DEBUG, ("igmp_leavegroup: igmp_mac_filter(DEL "));
            ip_addr_debug_print(IGMP_DEBUG, groupaddr);
            LWIP_DEBUGF(IGMP_DEBUG, (") on if %p\n", netif));
            netif->igmp_mac_filter(netif, groupaddr, IGMP_DEL_MAC_FILTER);
          }
          
          LWIP_DEBUGF(IGMP_DEBUG, ("igmp_leavegroup: remove group: "));
          ip_addr_debug_print(IGMP_DEBUG, groupaddr);
          LWIP_DEBUGF(IGMP_DEBUG, ("\n"));          
          
          /* Free the group */
          igmp_remove_group(group);
        } else {
          /* Decrement group use */
          group->use--;
        }
        /* Leave on this interface */
        err = ERR_OK;
      } else {
        /* It's not a fatal error on "leavegroup" */
        LWIP_DEBUGF(IGMP_DEBUG, ("igmp_leavegroup: not member of group\n"));
      }
    }
    /* proceed to next network interface */
    netif = netif->next;
  }

  return err;
}

/**
 * The igmp timer function (both for NO_SYS=1 and =0)
 * Should be called every IGMP_TMR_INTERVAL milliseconds (100 ms is default).
 */
void
igmp_tmr(void)
{
  struct igmp_group *group = igmp_group_list;

  while (group != NULL) {
    if (group->timer > 0) {
      group->timer--;
      if (group->timer == 0) {
        igmp_timeout(group);
      }
    }
    group = group->next;
  }
}

/**
 * Called if a timeout for one group is reached.
 * Sends a report for this group.
 *
 * @param group an igmp_group for which a timeout is reached
 */
static void
igmp_timeout(struct igmp_group *group)
{
  /* If the state is IGMP_GROUP_DELAYING_MEMBER then we send a report for this group */
  if (group->group_state == IGMP_GROUP_DELAYING_MEMBER) {
    LWIP_DEBUGF(IGMP_DEBUG, ("igmp_timeout: report membership for group with address "));
    ip_addr_debug_print(IGMP_DEBUG, &(group->group_address));
    LWIP_DEBUGF(IGMP_DEBUG, (" on if %p\n", group->netif));

    IGMP_STATS_INC(igmp.tx_report);
    igmp_send(group, IGMP_V2_MEMB_REPORT);
  }
}

/**
 * Start a timer for an igmp group
 *
 * @param group the igmp_group for which to start a timer
 * @param max_time the time in multiples of IGMP_TMR_INTERVAL (decrease with
 *        every call to igmp_tmr())
 */
static void
igmp_start_timer(struct igmp_group *group, u8_t max_time)
{
  /* ensure the input value is > 0 */
  if (max_time == 0) {
    max_time = 1;
  }
#ifdef LWIP_RAND
  /* ensure the random value is > 0 */
  group->timer = (LWIP_RAND() % (max_time - 1)) + 1;
#endif /* LWIP_RAND */
}

/**
 * Delaying membership report for a group if necessary
 *
 * @param group the igmp_group for which "delaying" membership report
 * @param maxresp query delay
 */
static void
igmp_delaying_member(struct igmp_group *group, u8_t maxresp)
{
  if ((group->group_state == IGMP_GROUP_IDLE_MEMBER) ||
     ((group->group_state == IGMP_GROUP_DELAYING_MEMBER) &&
      ((group->timer == 0) || (maxresp < group->timer)))) {
    igmp_start_timer(group, maxresp);
    group->group_state = IGMP_GROUP_DELAYING_MEMBER;
  }
}


/**
 * Sends an IP packet on a network interface. This function constructs the IP header
 * and calculates the IP header checksum. If the source IP address is NULL,
 * the IP address of the outgoing network interface is filled in as source address.
 *
 * @param p the packet to send (p->payload points to the data, e.g. next
            protocol header; if dest == IP_HDRINCL, p already includes an IP
            header and p->payload points to that IP header)
 * @param src the source IP address to send from (if src == IP_ADDR_ANY, the
 *         IP  address of the netif used to send is used as source address)
 * @param dest the destination IP address to send the packet to
 * @param ttl the TTL value to be set in the IP header
 * @param proto the PROTOCOL to be set in the IP header
 * @param netif the netif on which to send this packet
 * @return ERR_OK if the packet was sent OK
 *         ERR_BUF if p doesn't have enough space for IP/LINK headers
 *         returns errors returned by netif->output
 */
static err_t
igmp_ip_output_if(struct pbuf *p, ip_addr_t *src, ip_addr_t *dest, struct netif *netif)
{
  /* This is the "router alert" option */
  u16_t ra[2];
  ra[0] = PP_HTONS(ROUTER_ALERT);
  ra[1] = 0x0000; /* Router shall examine packet */
  IGMP_STATS_INC(igmp.xmit);
  return ip_output_if_opt(p, src, dest, IGMP_TTL, 0, IP_PROTO_IGMP, netif, ra, ROUTER_ALERTLEN);
}

/**
 * Send an igmp packet to a specific group.
 *
 * @param group the group to which to send the packet
 * @param type the type of igmp packet to send
 */
static void
igmp_send(struct igmp_group *group, u8_t type)
{
  struct pbuf*     p    = NULL;
  struct igmp_msg* igmp = NULL;
  ip_addr_t   src  = *IP_ADDR_ANY;
  ip_addr_t*  dest = NULL;

  /* IP header + "router alert" option + IGMP header */
  p = pbuf_alloc(PBUF_TRANSPORT, IGMP_MINLEN, PBUF_RAM);
  
  if (p) {
    igmp = (struct igmp_msg *)p->payload;
    LWIP_ASSERT("igmp_send: check that first pbuf can hold struct igmp_msg",
               (p->len >= sizeof(struct igmp_msg)));
    ip_addr_copy(src, group->netif->ip_addr);
     
    if (type == IGMP_V2_MEMB_REPORT) {
      dest = &(group->group_address);
      ip_addr_copy(igmp->igmp_group_address, group->group_address);
      group->last_reporter_flag = 1; /* Remember we were the last to report */
    } else {
      if (type == IGMP_LEAVE_GROUP) {
        dest = &allrouters;
        ip_addr_copy(igmp->igmp_group_address, group->group_address);
      }
    }

    if ((type == IGMP_V2_MEMB_REPORT) || (type == IGMP_LEAVE_GROUP)) {
      igmp->igmp_msgtype  = type;
      igmp->igmp_maxresp  = 0;
      igmp->igmp_checksum = 0;
      igmp->igmp_checksum = inet_chksum(igmp, IGMP_MINLEN);

      igmp_ip_output_if(p, &src, dest, group->netif);
    }

    pbuf_free(p);
  } else {
    LWIP_DEBUGF(IGMP_DEBUG, ("igmp_send: not enough memory for igmp_send\n"));
    IGMP_STATS_INC(igmp.memerr);
  }
}

#endif /* LWIP_IGMP */
