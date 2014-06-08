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
#ifndef __LWIP_TCPIP_H__
#define __LWIP_TCPIP_H__

#include "lwip/opt.h"

#if !NO_SYS /* don't build if not configured for use in lwipopts.h */

#include "lwip/api_msg.h"
#include "lwip/netifapi.h"
#include "lwip/pbuf.h"
#include "lwip/api.h"
#include "lwip/sys.h"
#include "lwip/timers.h"
#include "lwip/netif.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Define this to something that triggers a watchdog. This is called from
 * tcpip_thread after processing a message. */
#ifndef LWIP_TCPIP_THREAD_ALIVE
#define LWIP_TCPIP_THREAD_ALIVE()
#endif

#if LWIP_TCPIP_CORE_LOCKING
/** The global semaphore to lock the stack. */
extern sys_mutex_t lock_tcpip_core;
#define LOCK_TCPIP_CORE()     sys_mutex_lock(&lock_tcpip_core)
#define UNLOCK_TCPIP_CORE()   sys_mutex_unlock(&lock_tcpip_core)
#ifdef LWIP_DEBUG
#define TCIP_APIMSG_SET_ERR(m, e) (m)->msg.err = e  /* catch functions that don't set err */
#else
#define TCIP_APIMSG_SET_ERR(m, e)
#endif
#define TCPIP_APIMSG_NOERR(m,f) do { \
  TCIP_APIMSG_SET_ERR(m, ERR_VAL); \
  LOCK_TCPIP_CORE(); \
  f(&((m)->msg)); \
  UNLOCK_TCPIP_CORE(); \
} while(0)
#define TCPIP_APIMSG(m,f,e)   do { \
  TCPIP_APIMSG_NOERR(m,f); \
  (e) = (m)->msg.err; \
} while(0)
#define TCPIP_APIMSG_ACK(m)
#define TCPIP_NETIFAPI(m)     tcpip_netifapi_lock(m)
#define TCPIP_NETIFAPI_ACK(m)
#else /* LWIP_TCPIP_CORE_LOCKING */
#define LOCK_TCPIP_CORE()
#define UNLOCK_TCPIP_CORE()
#define TCPIP_APIMSG_NOERR(m,f) do { (m)->function = f; tcpip_apimsg(m); } while(0)
#define TCPIP_APIMSG(m,f,e)   do { (m)->function = f; (e) = tcpip_apimsg(m); } while(0)
#define TCPIP_APIMSG_ACK(m)   sys_sem_signal(&m->conn->op_completed)
#define TCPIP_NETIFAPI(m)     tcpip_netifapi(m)
#define TCPIP_NETIFAPI_ACK(m) sys_sem_signal(&m->sem)
#endif /* LWIP_TCPIP_CORE_LOCKING */

/** Function prototype for the init_done function passed to tcpip_init */
typedef void (*tcpip_init_done_fn)(void *arg);
/** Function prototype for functions passed to tcpip_callback() */
typedef void (*tcpip_callback_fn)(void *ctx);

/* Forward declarations */
struct tcpip_callback_msg;

void tcpip_init(tcpip_init_done_fn tcpip_init_done, void *arg);

#if LWIP_NETCONN
err_t tcpip_apimsg(struct api_msg *apimsg);
#endif /* LWIP_NETCONN */

err_t tcpip_input(struct pbuf *p, struct netif *inp);

#if LWIP_NETIF_API
err_t tcpip_netifapi(struct netifapi_msg *netifapimsg);
#if LWIP_TCPIP_CORE_LOCKING
err_t tcpip_netifapi_lock(struct netifapi_msg *netifapimsg);
#endif /* LWIP_TCPIP_CORE_LOCKING */
#endif /* LWIP_NETIF_API */

err_t tcpip_callback_with_block(tcpip_callback_fn function, void *ctx, u8_t block);
#define tcpip_callback(f, ctx)              tcpip_callback_with_block(f, ctx, 1)

struct tcpip_callback_msg* tcpip_callbackmsg_new(tcpip_callback_fn function, void *ctx);
void   tcpip_callbackmsg_delete(struct tcpip_callback_msg* msg);
err_t  tcpip_trycallback(struct tcpip_callback_msg* msg);

/* free pbufs or heap memory from another context without blocking */
err_t pbuf_free_callback(struct pbuf *p);
err_t mem_free_callback(void *m);

#if LWIP_TCPIP_TIMEOUT
err_t tcpip_timeout(u32_t msecs, sys_timeout_handler h, void *arg);
err_t tcpip_untimeout(sys_timeout_handler h, void *arg);
#endif /* LWIP_TCPIP_TIMEOUT */

enum tcpip_msg_type {
#if LWIP_NETCONN
  TCPIP_MSG_API,
#endif /* LWIP_NETCONN */
  TCPIP_MSG_INPKT,
#if LWIP_NETIF_API
  TCPIP_MSG_NETIFAPI,
#endif /* LWIP_NETIF_API */
#if LWIP_TCPIP_TIMEOUT
  TCPIP_MSG_TIMEOUT,
  TCPIP_MSG_UNTIMEOUT,
#endif /* LWIP_TCPIP_TIMEOUT */
  TCPIP_MSG_CALLBACK,
  TCPIP_MSG_CALLBACK_STATIC
};

struct tcpip_msg {
  enum tcpip_msg_type type;
  sys_sem_t *sem;
  union {
#if LWIP_NETCONN
    struct api_msg *apimsg;
#endif /* LWIP_NETCONN */
#if LWIP_NETIF_API
    struct netifapi_msg *netifapimsg;
#endif /* LWIP_NETIF_API */
    struct {
      struct pbuf *p;
      struct netif *netif;
    } inp;
    struct {
      tcpip_callback_fn function;
      void *ctx;
    } cb;
#if LWIP_TCPIP_TIMEOUT
    struct {
      u32_t msecs;
      sys_timeout_handler h;
      void *arg;
    } tmo;
#endif /* LWIP_TCPIP_TIMEOUT */
  } msg;
};

#ifdef __cplusplus
}
#endif

#endif /* !NO_SYS */

#endif /* __LWIP_TCPIP_H__ */
