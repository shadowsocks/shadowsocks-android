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
#ifndef __LWIP_API_MSG_H__
#define __LWIP_API_MSG_H__

#include "lwip/opt.h"

#if LWIP_NETCONN /* don't build if not configured for use in lwipopts.h */

#include <stddef.h> /* for size_t */

#include "lwip/ip_addr.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/igmp.h"
#include "lwip/api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* For the netconn API, these values are use as a bitmask! */
#define NETCONN_SHUT_RD   1
#define NETCONN_SHUT_WR   2
#define NETCONN_SHUT_RDWR (NETCONN_SHUT_RD | NETCONN_SHUT_WR)

/* IP addresses and port numbers are expected to be in
 * the same byte order as in the corresponding pcb.
 */
/** This struct includes everything that is necessary to execute a function
    for a netconn in another thread context (mainly used to process netconns
    in the tcpip_thread context to be thread safe). */
struct api_msg_msg {
  /** The netconn which to process - always needed: it includes the semaphore
      which is used to block the application thread until the function finished. */
  struct netconn *conn;
  /** The return value of the function executed in tcpip_thread. */
  err_t err;
  /** Depending on the executed function, one of these union members is used */
  union {
    /** used for lwip_netconn_do_send */
    struct netbuf *b;
    /** used for lwip_netconn_do_newconn */
    struct {
      u8_t proto;
    } n;
    /** used for lwip_netconn_do_bind and lwip_netconn_do_connect */
    struct {
      ip_addr_t *ipaddr;
      u16_t port;
    } bc;
    /** used for lwip_netconn_do_getaddr */
    struct {
      ipX_addr_t *ipaddr;
      u16_t *port;
      u8_t local;
    } ad;
    /** used for lwip_netconn_do_write */
    struct {
      const void *dataptr;
      size_t len;
      u8_t apiflags;
#if LWIP_SO_SNDTIMEO
      u32_t time_started;
#endif /* LWIP_SO_SNDTIMEO */
    } w;
    /** used for lwip_netconn_do_recv */
    struct {
      u32_t len;
    } r;
    /** used for lwip_netconn_do_close (/shutdown) */
    struct {
      u8_t shut;
    } sd;
#if LWIP_IGMP || (LWIP_IPV6 && LWIP_IPV6_MLD)
    /** used for lwip_netconn_do_join_leave_group */
    struct {
      ipX_addr_t *multiaddr;
      ipX_addr_t *netif_addr;
      enum netconn_igmp join_or_leave;
    } jl;
#endif /* LWIP_IGMP || (LWIP_IPV6 && LWIP_IPV6_MLD) */
#if TCP_LISTEN_BACKLOG
    struct {
      u8_t backlog;
    } lb;
#endif /* TCP_LISTEN_BACKLOG */
  } msg;
};

/** This struct contains a function to execute in another thread context and
    a struct api_msg_msg that serves as an argument for this function.
    This is passed to tcpip_apimsg to execute functions in tcpip_thread context. */
struct api_msg {
  /** function to execute in tcpip_thread context */
  void (* function)(struct api_msg_msg *msg);
  /** arguments for this function */
  struct api_msg_msg msg;
};

#if LWIP_DNS
/** As lwip_netconn_do_gethostbyname requires more arguments but doesn't require a netconn,
    it has its own struct (to avoid struct api_msg getting bigger than necessary).
    lwip_netconn_do_gethostbyname must be called using tcpip_callback instead of tcpip_apimsg
    (see netconn_gethostbyname). */
struct dns_api_msg {
  /** Hostname to query or dotted IP address string */
  const char *name;
  /** Rhe resolved address is stored here */
  ip_addr_t *addr;
  /** This semaphore is posted when the name is resolved, the application thread
      should wait on it. */
  sys_sem_t *sem;
  /** Errors are given back here */
  err_t *err;
};
#endif /* LWIP_DNS */

void lwip_netconn_do_newconn         ( struct api_msg_msg *msg);
void lwip_netconn_do_delconn         ( struct api_msg_msg *msg);
void lwip_netconn_do_bind            ( struct api_msg_msg *msg);
void lwip_netconn_do_connect         ( struct api_msg_msg *msg);
void lwip_netconn_do_disconnect      ( struct api_msg_msg *msg);
void lwip_netconn_do_listen          ( struct api_msg_msg *msg);
void lwip_netconn_do_send            ( struct api_msg_msg *msg);
void lwip_netconn_do_recv            ( struct api_msg_msg *msg);
void lwip_netconn_do_write           ( struct api_msg_msg *msg);
void lwip_netconn_do_getaddr         ( struct api_msg_msg *msg);
void lwip_netconn_do_close           ( struct api_msg_msg *msg);
void lwip_netconn_do_shutdown        ( struct api_msg_msg *msg);
#if LWIP_IGMP || (LWIP_IPV6 && LWIP_IPV6_MLD)
void lwip_netconn_do_join_leave_group( struct api_msg_msg *msg);
#endif /* LWIP_IGMP || (LWIP_IPV6 && LWIP_IPV6_MLD) */

#if LWIP_DNS
void lwip_netconn_do_gethostbyname(void *arg);
#endif /* LWIP_DNS */

struct netconn* netconn_alloc(enum netconn_type t, netconn_callback callback);
void netconn_free(struct netconn *conn);

#ifdef __cplusplus
}
#endif

#endif /* LWIP_NETCONN */

#endif /* __LWIP_API_MSG_H__ */
