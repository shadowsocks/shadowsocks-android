/*
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
 */
 
#ifndef __LWIP_NETIFAPI_H__
#define __LWIP_NETIFAPI_H__

#include "lwip/opt.h"

#if LWIP_NETIF_API /* don't build if not configured for use in lwipopts.h */

#include "lwip/sys.h"
#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include "lwip/autoip.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*netifapi_void_fn)(struct netif *netif);
typedef err_t (*netifapi_errt_fn)(struct netif *netif);

struct netifapi_msg_msg {
#if !LWIP_TCPIP_CORE_LOCKING
  sys_sem_t sem;
#endif /* !LWIP_TCPIP_CORE_LOCKING */
  err_t err;
  struct netif *netif;
  union {
    struct {
      ip_addr_t *ipaddr;
      ip_addr_t *netmask;
      ip_addr_t *gw;
      void *state;
      netif_init_fn init;
      netif_input_fn input;
    } add;
    struct {
      netifapi_void_fn voidfunc;
      netifapi_errt_fn errtfunc;
    } common;
  } msg;
};

struct netifapi_msg {
  void (* function)(struct netifapi_msg_msg *msg);
  struct netifapi_msg_msg msg;
};


/* API for application */
err_t netifapi_netif_add       ( struct netif *netif,
                                 ip_addr_t *ipaddr,
                                 ip_addr_t *netmask,
                                 ip_addr_t *gw,
                                 void *state,
                                 netif_init_fn init,
                                 netif_input_fn input);

err_t netifapi_netif_set_addr  ( struct netif *netif,
                                 ip_addr_t *ipaddr,
                                 ip_addr_t *netmask,
                                 ip_addr_t *gw );

err_t netifapi_netif_common    ( struct netif *netif,
                                 netifapi_void_fn voidfunc,
                                 netifapi_errt_fn errtfunc);

#define netifapi_netif_remove(n)      netifapi_netif_common(n, netif_remove, NULL)
#define netifapi_netif_set_up(n)      netifapi_netif_common(n, netif_set_up, NULL)
#define netifapi_netif_set_down(n)    netifapi_netif_common(n, netif_set_down, NULL)
#define netifapi_netif_set_default(n) netifapi_netif_common(n, netif_set_default, NULL)
#define netifapi_dhcp_start(n)        netifapi_netif_common(n, NULL, dhcp_start)
#define netifapi_dhcp_stop(n)         netifapi_netif_common(n, dhcp_stop, NULL)
#define netifapi_autoip_start(n)      netifapi_netif_common(n, NULL, autoip_start)
#define netifapi_autoip_stop(n)       netifapi_netif_common(n, NULL, autoip_stop)

#ifdef __cplusplus
}
#endif

#endif /* LWIP_NETIF_API */

#endif /* __LWIP_NETIFAPI_H__ */
