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
#ifndef __LWIP_UDP_H__
#define __LWIP_UDP_H__

#include "lwip/opt.h"

#if LWIP_UDP /* don't build if not configured for use in lwipopts.h */

#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "lwip/ip_addr.h"
#include "lwip/ip.h"
#include "lwip/ip6_addr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UDP_HLEN 8

/* Fields are (of course) in network byte order. */
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/bpstruct.h"
#endif
PACK_STRUCT_BEGIN
struct udp_hdr {
  PACK_STRUCT_FIELD(u16_t src);
  PACK_STRUCT_FIELD(u16_t dest);  /* src/dest UDP ports */
  PACK_STRUCT_FIELD(u16_t len);
  PACK_STRUCT_FIELD(u16_t chksum);
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/epstruct.h"
#endif

#define UDP_FLAGS_NOCHKSUM       0x01U
#define UDP_FLAGS_UDPLITE        0x02U
#define UDP_FLAGS_CONNECTED      0x04U
#define UDP_FLAGS_MULTICAST_LOOP 0x08U

struct udp_pcb;

/** Function prototype for udp pcb receive callback functions
 * addr and port are in same byte order as in the pcb
 * The callback is responsible for freeing the pbuf
 * if it's not used any more.
 *
 * ATTENTION: Be aware that 'addr' points into the pbuf 'p' so freeing this pbuf
 *            makes 'addr' invalid, too.
 *
 * @param arg user supplied argument (udp_pcb.recv_arg)
 * @param pcb the udp_pcb which received data
 * @param p the packet buffer that was received
 * @param addr the remote IP address from which the packet was received
 * @param port the remote port from which the packet was received
 */
typedef void (*udp_recv_fn)(void *arg, struct udp_pcb *pcb, struct pbuf *p,
    ip_addr_t *addr, u16_t port);

#if LWIP_IPV6
/** Function prototype for udp pcb IPv6 receive callback functions
 * The callback is responsible for freeing the pbuf
 * if it's not used any more.
 *
 * @param arg user supplied argument (udp_pcb.recv_arg)
 * @param pcb the udp_pcb which received data
 * @param p the packet buffer that was received
 * @param addr the remote IPv6 address from which the packet was received
 * @param port the remote port from which the packet was received
 */
typedef void (*udp_recv_ip6_fn)(void *arg, struct udp_pcb *pcb, struct pbuf *p,
    ip6_addr_t *addr, u16_t port);
#endif /* LWIP_IPV6 */

#if LWIP_IPV6
#define UDP_PCB_RECV_IP6  udp_recv_ip6_fn ip6;
#else
#define UDP_PCB_RECV_IP6
#endif /* LWIP_IPV6 */

struct udp_pcb {
/* Common members of all PCB types */
  IP_PCB;

/* Protocol specific PCB members */

  struct udp_pcb *next;

  u8_t flags;
  /** ports are in host byte order */
  u16_t local_port, remote_port;

#if LWIP_IGMP
  /** outgoing network interface for multicast packets */
  ip_addr_t multicast_ip;
#endif /* LWIP_IGMP */

#if LWIP_UDPLITE
  /** used for UDP_LITE only */
  u16_t chksum_len_rx, chksum_len_tx;
#endif /* LWIP_UDPLITE */

  /** receive callback function */
  union {
    udp_recv_fn ip4;
    UDP_PCB_RECV_IP6
  }recv;
  /** user-supplied argument for the recv callback */
  void *recv_arg;  
};
/* udp_pcbs export for exernal reference (e.g. SNMP agent) */
extern struct udp_pcb *udp_pcbs;

/* The following functions is the application layer interface to the
   UDP code. */
struct udp_pcb * udp_new        (void);
void             udp_remove     (struct udp_pcb *pcb);
err_t            udp_bind       (struct udp_pcb *pcb, ip_addr_t *ipaddr,
                                 u16_t port);
err_t            udp_connect    (struct udp_pcb *pcb, ip_addr_t *ipaddr,
                                 u16_t port);
void             udp_disconnect (struct udp_pcb *pcb);
void             udp_recv       (struct udp_pcb *pcb, udp_recv_fn recv,
                                 void *recv_arg);
err_t            udp_sendto_if  (struct udp_pcb *pcb, struct pbuf *p,
                                 ip_addr_t *dst_ip, u16_t dst_port,
                                 struct netif *netif);
err_t            udp_sendto     (struct udp_pcb *pcb, struct pbuf *p,
                                 ip_addr_t *dst_ip, u16_t dst_port);
err_t            udp_send       (struct udp_pcb *pcb, struct pbuf *p);

#if LWIP_CHECKSUM_ON_COPY && CHECKSUM_GEN_UDP
err_t            udp_sendto_if_chksum(struct udp_pcb *pcb, struct pbuf *p,
                                 ip_addr_t *dst_ip, u16_t dst_port,
                                 struct netif *netif, u8_t have_chksum,
                                 u16_t chksum);
err_t            udp_sendto_chksum(struct udp_pcb *pcb, struct pbuf *p,
                                 ip_addr_t *dst_ip, u16_t dst_port,
                                 u8_t have_chksum, u16_t chksum);
err_t            udp_send_chksum(struct udp_pcb *pcb, struct pbuf *p,
                                 u8_t have_chksum, u16_t chksum);
#endif /* LWIP_CHECKSUM_ON_COPY && CHECKSUM_GEN_UDP */

#define          udp_flags(pcb) ((pcb)->flags)
#define          udp_setflags(pcb, f)  ((pcb)->flags = (f))

/* The following functions are the lower layer interface to UDP. */
void             udp_input      (struct pbuf *p, struct netif *inp);

void             udp_init       (void);

#if LWIP_IPV6
struct udp_pcb * udp_new_ip6(void);
#define          udp_bind_ip6(pcb, ip6addr, port) \
                   udp_bind(pcb, ip6_2_ip(ip6addr), port)
#define          udp_connect_ip6(pcb, ip6addr, port) \
                   udp_connect(pcb, ip6_2_ip(ip6addr), port)
#define          udp_recv_ip6(pcb, recv_ip6_fn, recv_arg) \
                   udp_recv(pcb, (udp_recv_fn)recv_ip6_fn, recv_arg)
#define          udp_sendto_ip6(pcb, pbuf, ip6addr, port) \
                   udp_sendto(pcb, pbuf, ip6_2_ip(ip6addr), port)
#define          udp_sendto_if_ip6(pcb, pbuf, ip6addr, port, netif) \
                   udp_sendto_if(pcb, pbuf, ip6_2_ip(ip6addr), port, netif)
#if LWIP_CHECKSUM_ON_COPY && CHECKSUM_GEN_UDP
#define          udp_sendto_chksum_ip6(pcb, pbuf, ip6addr, port, have_chk, chksum) \
                   udp_sendto_chksum(pcb, pbuf, ip6_2_ip(ip6addr), port, have_chk, chksum)
#define          udp_sendto_if_chksum_ip6(pcb, pbuf, ip6addr, port, netif, have_chk, chksum) \
                   udp_sendto_if_chksum(pcb, pbuf, ip6_2_ip(ip6addr), port, netif, have_chk, chksum)
#endif /*LWIP_CHECKSUM_ON_COPY && CHECKSUM_GEN_UDP */
#endif /* LWIP_IPV6 */

#if UDP_DEBUG
void udp_debug_print(struct udp_hdr *udphdr);
#else
#define udp_debug_print(udphdr)
#endif

#ifdef __cplusplus
}
#endif

#endif /* LWIP_UDP */

#endif /* __LWIP_UDP_H__ */
