/*
 * Copyright (c) 2001, 2002 Leon Woestenberg <leon.woestenberg@axon.tv>
 * Copyright (c) 2001, 2002 Axon Digital Design B.V., The Netherlands.
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
 * Author: Leon Woestenberg <leon.woestenberg@axon.tv>
 *
 */
#ifndef __LWIP_SNMP_H__
#define __LWIP_SNMP_H__

#include "lwip/opt.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "lwip/ip_addr.h"

struct udp_pcb;
struct netif;

/**
 * @see RFC1213, "MIB-II, 6. Definitions"
 */
enum snmp_ifType {
  snmp_ifType_other=1,                /* none of the following */
  snmp_ifType_regular1822,
  snmp_ifType_hdh1822,
  snmp_ifType_ddn_x25,
  snmp_ifType_rfc877_x25,
  snmp_ifType_ethernet_csmacd,
  snmp_ifType_iso88023_csmacd,
  snmp_ifType_iso88024_tokenBus,
  snmp_ifType_iso88025_tokenRing,
  snmp_ifType_iso88026_man,
  snmp_ifType_starLan,
  snmp_ifType_proteon_10Mbit,
  snmp_ifType_proteon_80Mbit,
  snmp_ifType_hyperchannel,
  snmp_ifType_fddi,
  snmp_ifType_lapb,
  snmp_ifType_sdlc,
  snmp_ifType_ds1,                    /* T-1 */
  snmp_ifType_e1,                     /* european equiv. of T-1 */
  snmp_ifType_basicISDN,
  snmp_ifType_primaryISDN,            /* proprietary serial */
  snmp_ifType_propPointToPointSerial,
  snmp_ifType_ppp,
  snmp_ifType_softwareLoopback,
  snmp_ifType_eon,                    /* CLNP over IP [11] */
  snmp_ifType_ethernet_3Mbit,
  snmp_ifType_nsip,                   /* XNS over IP */
  snmp_ifType_slip,                   /* generic SLIP */
  snmp_ifType_ultra,                  /* ULTRA technologies */
  snmp_ifType_ds3,                    /* T-3 */
  snmp_ifType_sip,                    /* SMDS */
  snmp_ifType_frame_relay
};

#if LWIP_SNMP /* don't build if not configured for use in lwipopts.h */

/** SNMP "sysuptime" Interval */
#define SNMP_SYSUPTIME_INTERVAL 10

/** fixed maximum length for object identifier type */
#define LWIP_SNMP_OBJ_ID_LEN 32

/** internal object identifier representation */
struct snmp_obj_id
{
  u8_t len;
  s32_t id[LWIP_SNMP_OBJ_ID_LEN];
};

/* system */
void snmp_set_sysdesr(u8_t* str, u8_t* len);
void snmp_set_sysobjid(struct snmp_obj_id *oid);
void snmp_get_sysobjid_ptr(struct snmp_obj_id **oid);
void snmp_inc_sysuptime(void);
void snmp_add_sysuptime(u32_t value);
void snmp_get_sysuptime(u32_t *value);
void snmp_set_syscontact(u8_t *ocstr, u8_t *ocstrlen);
void snmp_set_sysname(u8_t *ocstr, u8_t *ocstrlen);
void snmp_set_syslocation(u8_t *ocstr, u8_t *ocstrlen);

/* network interface */
void snmp_add_ifinoctets(struct netif *ni, u32_t value); 
void snmp_inc_ifinucastpkts(struct netif *ni);
void snmp_inc_ifinnucastpkts(struct netif *ni);
void snmp_inc_ifindiscards(struct netif *ni);
void snmp_add_ifoutoctets(struct netif *ni, u32_t value);
void snmp_inc_ifoutucastpkts(struct netif *ni);
void snmp_inc_ifoutnucastpkts(struct netif *ni);
void snmp_inc_ifoutdiscards(struct netif *ni);
void snmp_inc_iflist(void);
void snmp_dec_iflist(void);

/* ARP (for atTable and ipNetToMediaTable) */
void snmp_insert_arpidx_tree(struct netif *ni, ip_addr_t *ip);
void snmp_delete_arpidx_tree(struct netif *ni, ip_addr_t *ip);

/* IP */
void snmp_inc_ipinreceives(void);
void snmp_inc_ipinhdrerrors(void);
void snmp_inc_ipinaddrerrors(void);
void snmp_inc_ipforwdatagrams(void);
void snmp_inc_ipinunknownprotos(void);
void snmp_inc_ipindiscards(void);
void snmp_inc_ipindelivers(void);
void snmp_inc_ipoutrequests(void);
void snmp_inc_ipoutdiscards(void);
void snmp_inc_ipoutnoroutes(void);
void snmp_inc_ipreasmreqds(void);
void snmp_inc_ipreasmoks(void);
void snmp_inc_ipreasmfails(void);
void snmp_inc_ipfragoks(void);
void snmp_inc_ipfragfails(void);
void snmp_inc_ipfragcreates(void);
void snmp_inc_iproutingdiscards(void);
void snmp_insert_ipaddridx_tree(struct netif *ni);
void snmp_delete_ipaddridx_tree(struct netif *ni);
void snmp_insert_iprteidx_tree(u8_t dflt, struct netif *ni);
void snmp_delete_iprteidx_tree(u8_t dflt, struct netif *ni);

/* ICMP */
void snmp_inc_icmpinmsgs(void);
void snmp_inc_icmpinerrors(void);
void snmp_inc_icmpindestunreachs(void);
void snmp_inc_icmpintimeexcds(void);
void snmp_inc_icmpinparmprobs(void);
void snmp_inc_icmpinsrcquenchs(void);
void snmp_inc_icmpinredirects(void);
void snmp_inc_icmpinechos(void);
void snmp_inc_icmpinechoreps(void);
void snmp_inc_icmpintimestamps(void);
void snmp_inc_icmpintimestampreps(void);
void snmp_inc_icmpinaddrmasks(void);
void snmp_inc_icmpinaddrmaskreps(void);
void snmp_inc_icmpoutmsgs(void);
void snmp_inc_icmpouterrors(void);
void snmp_inc_icmpoutdestunreachs(void);
void snmp_inc_icmpouttimeexcds(void);
void snmp_inc_icmpoutparmprobs(void);
void snmp_inc_icmpoutsrcquenchs(void);
void snmp_inc_icmpoutredirects(void); 
void snmp_inc_icmpoutechos(void);
void snmp_inc_icmpoutechoreps(void);
void snmp_inc_icmpouttimestamps(void);
void snmp_inc_icmpouttimestampreps(void);
void snmp_inc_icmpoutaddrmasks(void);
void snmp_inc_icmpoutaddrmaskreps(void);

/* TCP */
void snmp_inc_tcpactiveopens(void);
void snmp_inc_tcppassiveopens(void);
void snmp_inc_tcpattemptfails(void);
void snmp_inc_tcpestabresets(void);
void snmp_inc_tcpinsegs(void);
void snmp_inc_tcpoutsegs(void);
void snmp_inc_tcpretranssegs(void);
void snmp_inc_tcpinerrs(void);
void snmp_inc_tcpoutrsts(void);

/* UDP */
void snmp_inc_udpindatagrams(void);
void snmp_inc_udpnoports(void);
void snmp_inc_udpinerrors(void);
void snmp_inc_udpoutdatagrams(void);
void snmp_insert_udpidx_tree(struct udp_pcb *pcb);
void snmp_delete_udpidx_tree(struct udp_pcb *pcb);

/* SNMP */
void snmp_inc_snmpinpkts(void);
void snmp_inc_snmpoutpkts(void);
void snmp_inc_snmpinbadversions(void);
void snmp_inc_snmpinbadcommunitynames(void);
void snmp_inc_snmpinbadcommunityuses(void);
void snmp_inc_snmpinasnparseerrs(void);
void snmp_inc_snmpintoobigs(void);
void snmp_inc_snmpinnosuchnames(void);
void snmp_inc_snmpinbadvalues(void);
void snmp_inc_snmpinreadonlys(void);
void snmp_inc_snmpingenerrs(void);
void snmp_add_snmpintotalreqvars(u8_t value);
void snmp_add_snmpintotalsetvars(u8_t value);
void snmp_inc_snmpingetrequests(void);
void snmp_inc_snmpingetnexts(void);
void snmp_inc_snmpinsetrequests(void);
void snmp_inc_snmpingetresponses(void);
void snmp_inc_snmpintraps(void);
void snmp_inc_snmpouttoobigs(void);
void snmp_inc_snmpoutnosuchnames(void);
void snmp_inc_snmpoutbadvalues(void);
void snmp_inc_snmpoutgenerrs(void);
void snmp_inc_snmpoutgetrequests(void);
void snmp_inc_snmpoutgetnexts(void);
void snmp_inc_snmpoutsetrequests(void);
void snmp_inc_snmpoutgetresponses(void);
void snmp_inc_snmpouttraps(void);
void snmp_get_snmpgrpid_ptr(struct snmp_obj_id **oid);
void snmp_set_snmpenableauthentraps(u8_t *value);
void snmp_get_snmpenableauthentraps(u8_t *value);

/* LWIP_SNMP support not available */
/* define everything to be empty */
#else

/* system */
#define snmp_set_sysdesr(str, len)
#define snmp_set_sysobjid(oid);
#define snmp_get_sysobjid_ptr(oid)
#define snmp_inc_sysuptime()
#define snmp_add_sysuptime(value)
#define snmp_get_sysuptime(value)
#define snmp_set_syscontact(ocstr, ocstrlen);
#define snmp_set_sysname(ocstr, ocstrlen);
#define snmp_set_syslocation(ocstr, ocstrlen);

/* network interface */
#define snmp_add_ifinoctets(ni,value) 
#define snmp_inc_ifinucastpkts(ni)
#define snmp_inc_ifinnucastpkts(ni)
#define snmp_inc_ifindiscards(ni)
#define snmp_add_ifoutoctets(ni,value)
#define snmp_inc_ifoutucastpkts(ni)
#define snmp_inc_ifoutnucastpkts(ni)
#define snmp_inc_ifoutdiscards(ni)
#define snmp_inc_iflist()
#define snmp_dec_iflist()

/* ARP */
#define snmp_insert_arpidx_tree(ni,ip)
#define snmp_delete_arpidx_tree(ni,ip)

/* IP */
#define snmp_inc_ipinreceives()
#define snmp_inc_ipinhdrerrors()
#define snmp_inc_ipinaddrerrors()
#define snmp_inc_ipforwdatagrams()
#define snmp_inc_ipinunknownprotos()
#define snmp_inc_ipindiscards()
#define snmp_inc_ipindelivers()
#define snmp_inc_ipoutrequests()
#define snmp_inc_ipoutdiscards()
#define snmp_inc_ipoutnoroutes()
#define snmp_inc_ipreasmreqds()
#define snmp_inc_ipreasmoks()
#define snmp_inc_ipreasmfails()
#define snmp_inc_ipfragoks()
#define snmp_inc_ipfragfails()
#define snmp_inc_ipfragcreates()
#define snmp_inc_iproutingdiscards()
#define snmp_insert_ipaddridx_tree(ni)
#define snmp_delete_ipaddridx_tree(ni)
#define snmp_insert_iprteidx_tree(dflt, ni)
#define snmp_delete_iprteidx_tree(dflt, ni)

/* ICMP */
#define snmp_inc_icmpinmsgs()
#define snmp_inc_icmpinerrors() 
#define snmp_inc_icmpindestunreachs() 
#define snmp_inc_icmpintimeexcds()
#define snmp_inc_icmpinparmprobs() 
#define snmp_inc_icmpinsrcquenchs() 
#define snmp_inc_icmpinredirects() 
#define snmp_inc_icmpinechos() 
#define snmp_inc_icmpinechoreps()
#define snmp_inc_icmpintimestamps() 
#define snmp_inc_icmpintimestampreps()
#define snmp_inc_icmpinaddrmasks()
#define snmp_inc_icmpinaddrmaskreps()
#define snmp_inc_icmpoutmsgs()
#define snmp_inc_icmpouterrors()
#define snmp_inc_icmpoutdestunreachs() 
#define snmp_inc_icmpouttimeexcds() 
#define snmp_inc_icmpoutparmprobs()
#define snmp_inc_icmpoutsrcquenchs()
#define snmp_inc_icmpoutredirects() 
#define snmp_inc_icmpoutechos() 
#define snmp_inc_icmpoutechoreps()
#define snmp_inc_icmpouttimestamps()
#define snmp_inc_icmpouttimestampreps()
#define snmp_inc_icmpoutaddrmasks()
#define snmp_inc_icmpoutaddrmaskreps()
/* TCP */
#define snmp_inc_tcpactiveopens()
#define snmp_inc_tcppassiveopens()
#define snmp_inc_tcpattemptfails()
#define snmp_inc_tcpestabresets()
#define snmp_inc_tcpinsegs()
#define snmp_inc_tcpoutsegs()
#define snmp_inc_tcpretranssegs()
#define snmp_inc_tcpinerrs()
#define snmp_inc_tcpoutrsts()

/* UDP */
#define snmp_inc_udpindatagrams()
#define snmp_inc_udpnoports()
#define snmp_inc_udpinerrors()
#define snmp_inc_udpoutdatagrams()
#define snmp_insert_udpidx_tree(pcb)
#define snmp_delete_udpidx_tree(pcb)

/* SNMP */
#define snmp_inc_snmpinpkts()
#define snmp_inc_snmpoutpkts()
#define snmp_inc_snmpinbadversions()
#define snmp_inc_snmpinbadcommunitynames()
#define snmp_inc_snmpinbadcommunityuses()
#define snmp_inc_snmpinasnparseerrs()
#define snmp_inc_snmpintoobigs()
#define snmp_inc_snmpinnosuchnames()
#define snmp_inc_snmpinbadvalues()
#define snmp_inc_snmpinreadonlys()
#define snmp_inc_snmpingenerrs()
#define snmp_add_snmpintotalreqvars(value)
#define snmp_add_snmpintotalsetvars(value)
#define snmp_inc_snmpingetrequests()
#define snmp_inc_snmpingetnexts()
#define snmp_inc_snmpinsetrequests()
#define snmp_inc_snmpingetresponses()
#define snmp_inc_snmpintraps()
#define snmp_inc_snmpouttoobigs()
#define snmp_inc_snmpoutnosuchnames()
#define snmp_inc_snmpoutbadvalues()
#define snmp_inc_snmpoutgenerrs()
#define snmp_inc_snmpoutgetrequests()
#define snmp_inc_snmpoutgetnexts()
#define snmp_inc_snmpoutsetrequests()
#define snmp_inc_snmpoutgetresponses()
#define snmp_inc_snmpouttraps()
#define snmp_get_snmpgrpid_ptr(oid)
#define snmp_set_snmpenableauthentraps(value)
#define snmp_get_snmpenableauthentraps(value)

#endif /* LWIP_SNMP */

#ifdef __cplusplus
}
#endif

#endif /* __LWIP_SNMP_H__ */
