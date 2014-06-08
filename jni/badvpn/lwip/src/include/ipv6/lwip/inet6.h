/**
 * @file
 *
 * INET v6 addresses.
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
#ifndef __LWIP_INET6_H__
#define __LWIP_INET6_H__

#include "lwip/opt.h"

#if LWIP_IPV6 && LWIP_SOCKET /* don't build if not configured for use in lwipopts.h */

#include "lwip/ip6_addr.h"
#include "lwip/def.h"

#ifdef __cplusplus
extern "C" {
#endif

/** For compatibility with BSD code */
struct in6_addr {
  union {
    u8_t  u8_addr[16];
    u32_t u32_addr[4];
  } un;
#define s6_addr  un.u32_addr
};

#define IN6ADDR_ANY_INIT {0,0,0,0}
#define IN6ADDR_LOOPBACK_INIT {0,0,0,PP_HTONL(1)}


#define inet6_addr_from_ip6addr(target_in6addr, source_ip6addr) {(target_in6addr)->un.u32_addr[0] = (source_ip6addr)->addr[0]; \
                                                                 (target_in6addr)->un.u32_addr[1] = (source_ip6addr)->addr[1]; \
                                                                 (target_in6addr)->un.u32_addr[2] = (source_ip6addr)->addr[2]; \
                                                                 (target_in6addr)->un.u32_addr[3] = (source_ip6addr)->addr[3];}
#define inet6_addr_to_ip6addr(target_ip6addr, source_in6addr)   {(target_ip6addr)->addr[0] = (source_in6addr)->un.u32_addr[0]; \
                                                                 (target_ip6addr)->addr[1] = (source_in6addr)->un.u32_addr[1]; \
                                                                 (target_ip6addr)->addr[2] = (source_in6addr)->un.u32_addr[2]; \
                                                                 (target_ip6addr)->addr[3] = (source_in6addr)->un.u32_addr[3];}
/* ATTENTION: the next define only works because both in6_addr and ip6_addr_t are an u32_t[4] effectively! */
#define inet6_addr_to_ip6addr_p(target_ip6addr_p, source_in6addr)   ((target_ip6addr_p) = (ip6_addr_t*)(source_in6addr))

/* directly map this to the lwip internal functions */
#define inet6_aton(cp, addr)   ip6addr_aton(cp, (ip6_addr_t*)addr)
#define inet6_ntoa(addr)       ip6addr_ntoa((ip6_addr_t*)&(addr))
#define inet6_ntoa_r(addr, buf, buflen) ip6addr_ntoa_r((ip6_addr_t*)&(addr), buf, buflen)


#ifdef __cplusplus
}
#endif

#endif /* LWIP_IPV6 */

#endif /* __LWIP_INET6_H__ */

