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
#ifndef __LWIP_RAW_H__
#define __LWIP_RAW_H__

#include "lwip/opt.h"

#if LWIP_RAW /* don't build if not configured for use in lwipopts.h */

#include "lwip/pbuf.h"
#include "lwip/def.h"
#include "lwip/ip.h"
#include "lwip/ip_addr.h"
#include "lwip/ip6_addr.h"

#ifdef __cplusplus
extern "C" {
#endif

struct raw_pcb;

/** Function prototype for raw pcb receive callback functions.
 * @param arg user supplied argument (raw_pcb.recv_arg)
 * @param pcb the raw_pcb which received data
 * @param p the packet buffer that was received
 * @param addr the remote IP address from which the packet was received
 * @return 1 if the packet was 'eaten' (aka. deleted),
 *         0 if the packet lives on
 * If returning 1, the callback is responsible for freeing the pbuf
 * if it's not used any more.
 */
typedef u8_t (*raw_recv_fn)(void *arg, struct raw_pcb *pcb, struct pbuf *p,
    ip_addr_t *addr);

#if LWIP_IPV6
/** Function prototype for raw pcb IPv6 receive callback functions.
 * @param arg user supplied argument (raw_pcb.recv_arg)
 * @param pcb the raw_pcb which received data
 * @param p the packet buffer that was received
 * @param addr the remote IPv6 address from which the packet was received
 * @return 1 if the packet was 'eaten' (aka. deleted),
 *         0 if the packet lives on
 * If returning 1, the callback is responsible for freeing the pbuf
 * if it's not used any more.
 */
typedef u8_t (*raw_recv_ip6_fn)(void *arg, struct raw_pcb *pcb, struct pbuf *p,
    ip6_addr_t *addr);
#endif /* LWIP_IPV6 */

#if LWIP_IPV6
#define RAW_PCB_RECV_IP6  raw_recv_ip6_fn ip6;
#else
#define RAW_PCB_RECV_IP6
#endif /* LWIP_IPV6 */

struct raw_pcb {
  /* Common members of all PCB types */
  IP_PCB;

  struct raw_pcb *next;

  u8_t protocol;

  /** receive callback function */
  union {
    raw_recv_fn ip4;
    RAW_PCB_RECV_IP6
  } recv;
  /* user-supplied argument for the recv callback */
  void *recv_arg;
};

/* The following functions is the application layer interface to the
   RAW code. */
struct raw_pcb * raw_new        (u8_t proto);
void             raw_remove     (struct raw_pcb *pcb);
err_t            raw_bind       (struct raw_pcb *pcb, ip_addr_t *ipaddr);
err_t            raw_connect    (struct raw_pcb *pcb, ip_addr_t *ipaddr);

void             raw_recv       (struct raw_pcb *pcb, raw_recv_fn recv, void *recv_arg);
err_t            raw_sendto     (struct raw_pcb *pcb, struct pbuf *p, ip_addr_t *ipaddr);
err_t            raw_send       (struct raw_pcb *pcb, struct pbuf *p);

#if LWIP_IPV6
struct raw_pcb * raw_new_ip6   (u8_t proto);
#define          raw_bind_ip6(pcb, ip6addr) raw_bind(pcb, ip6_2_ip(ip6addr))
#define          raw_connect_ip6(pcb, ip6addr) raw_connect(pcb, ip6_2_ip(ip6addr))
#define          raw_recv_ip6(pcb, recv_ip6_fn, recv_arg) raw_recv(pcb, (raw_recv_fn)recv_ip6_fn, recv_arg)
#define          raw_sendto_ip6(pcb, pbuf, ip6addr) raw_sendto(pcb, pbuf, ip6_2_ip(ip6addr))
#endif /* LWIP_IPV6 */

/* The following functions are the lower layer interface to RAW. */
u8_t             raw_input      (struct pbuf *p, struct netif *inp);
#define raw_init() /* Compatibility define, not init needed. */

#ifdef __cplusplus
}
#endif

#endif /* LWIP_RAW */

#endif /* __LWIP_RAW_H__ */
