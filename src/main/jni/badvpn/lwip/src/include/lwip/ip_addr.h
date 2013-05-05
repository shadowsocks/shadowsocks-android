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
#ifndef __LWIP_IP_ADDR_H__
#define __LWIP_IP_ADDR_H__

#include "lwip/opt.h"
#include "lwip/def.h"

#include "lwip/ip4_addr.h"
#include "lwip/ip6_addr.h"

#ifdef __cplusplus
extern "C" {
#endif

#if LWIP_IPV6
/* A union struct for both IP version's addresses. */
typedef union {
  ip_addr_t ip4;
  ip6_addr_t ip6;
} ipX_addr_t;

/** These functions only exist for type-safe conversion from ip_addr_t to
    ip6_addr_t and back */
#ifdef LWIP_ALLOW_STATIC_FN_IN_HEADER
static ip6_addr_t* ip_2_ip6(ip_addr_t *ipaddr)
{ return (ip6_addr_t*)ipaddr;}
static ip_addr_t* ip6_2_ip(ip6_addr_t *ip6addr)
{ return (ip_addr_t*)ip6addr; }
static ipX_addr_t* ip_2_ipX(ip_addr_t *ipaddr)
{ return (ipX_addr_t*)ipaddr; }
static ipX_addr_t* ip6_2_ipX(ip6_addr_t *ip6addr)
{ return (ipX_addr_t*)ip6addr; }
#else /* LWIP_ALLOW_STATIC_FN_IN_HEADER */
#define ip_2_ip6(ipaddr)   ((ip6_addr_t*)(ipaddr))
#define ip6_2_ip(ip6addr)  ((ip_addr_t*)(ip6addr))
#define ip_2_ipX(ipaddr)   ((ipX_addr_t*)ipaddr)
#define ip6_2_ipX(ip6addr) ((ipX_addr_t*)ip6addr)
#endif /* LWIP_ALLOW_STATIC_FN_IN_HEADER*/
#define ipX_2_ip6(ip6addr) (&((ip6addr)->ip6))
#define ipX_2_ip(ipaddr)   (&((ipaddr)->ip4))

#define ipX_addr_copy(is_ipv6, dest, src)      do{if(is_ipv6){ \
  ip6_addr_copy((dest).ip6, (src).ip6); }else{ \
  ip_addr_copy((dest).ip4, (src).ip4); }}while(0)
#define ipX_addr_set(is_ipv6, dest, src) do{if(is_ipv6){ \
  ip6_addr_set(ipX_2_ip6(dest), ipX_2_ip6(src)); }else{ \
  ip_addr_set(ipX_2_ip(dest), ipX_2_ip(src)); }}while(0)
#define ipX_addr_set_ipaddr(is_ipv6, dest, src) do{if(is_ipv6){ \
  ip6_addr_set(ipX_2_ip6(dest), ip_2_ip6(src)); }else{ \
  ip_addr_set(ipX_2_ip(dest), src); }}while(0)
#define ipX_addr_set_zero(is_ipv6, ipaddr)     do{if(is_ipv6){ \
  ip6_addr_set_zero(ipX_2_ip6(ipaddr)); }else{ \
  ip_addr_set_zero(ipX_2_ip(ipaddr)); }}while(0)
#define ipX_addr_set_any(is_ipv6, ipaddr)      do{if(is_ipv6){ \
  ip6_addr_set_any(ipX_2_ip6(ipaddr)); }else{ \
  ip_addr_set_any(ipX_2_ip(ipaddr)); }}while(0)
#define ipX_addr_set_loopback(is_ipv6, ipaddr) do{if(is_ipv6){ \
  ip6_addr_set_loopback(ipX_2_ip6(ipaddr)); }else{ \
  ip_addr_set_loopback(ipX_2_ip(ipaddr)); }}while(0)
#define ipX_addr_set_hton(is_ipv6, dest, src)  do{if(is_ipv6){ \
  ip6_addr_set_hton(ipX_2_ip6(ipaddr), (src)) ;}else{ \
  ip_addr_set_hton(ipX_2_ip(ipaddr), (src));}}while(0)
#define ipX_addr_cmp(is_ipv6, addr1, addr2)    ((is_ipv6) ? \
  ip6_addr_cmp(ipX_2_ip6(addr1), ipX_2_ip6(addr2)) : \
  ip_addr_cmp(ipX_2_ip(addr1), ipX_2_ip(addr2)))
#define ipX_addr_isany(is_ipv6, ipaddr)        ((is_ipv6) ? \
  ip6_addr_isany(ipX_2_ip6(ipaddr)) : \
  ip_addr_isany(ipX_2_ip(ipaddr)))
#define ipX_addr_ismulticast(is_ipv6, ipaddr)  ((is_ipv6) ? \
  ip6_addr_ismulticast(ipX_2_ip6(ipaddr)) : \
  ip_addr_ismulticast(ipX_2_ip(ipaddr)))
#define ipX_addr_debug_print(is_ipv6, debug, ipaddr) do { if(is_ipv6) { \
  ip6_addr_debug_print(debug, ipX_2_ip6(ipaddr)); } else { \
  ip_addr_debug_print(debug, ipX_2_ip(ipaddr)); }}while(0)

#else /* LWIP_IPV6 */

typedef ip_addr_t ipX_addr_t;
#define ipX_2_ip(ipaddr) (ipaddr)
#define ip_2_ipX(ipaddr) (ipaddr)

#define ipX_addr_copy(is_ipv6, dest, src)       ip_addr_copy(dest, src)
#define ipX_addr_set(is_ipv6, dest, src)        ip_addr_set(dest, src)
#define ipX_addr_set_ipaddr(is_ipv6, dest, src) ip_addr_set(dest, src)
#define ipX_addr_set_zero(is_ipv6, ipaddr)      ip_addr_set_zero(ipaddr)
#define ipX_addr_set_any(is_ipv6, ipaddr)       ip_addr_set_any(ipaddr)
#define ipX_addr_set_loopback(is_ipv6, ipaddr)  ip_addr_set_loopback(ipaddr)
#define ipX_addr_set_hton(is_ipv6, dest, src)   ip_addr_set_hton(dest, src)
#define ipX_addr_cmp(is_ipv6, addr1, addr2)     ip_addr_cmp(addr1, addr2)
#define ipX_addr_isany(is_ipv6, ipaddr)         ip_addr_isany(ipaddr)
#define ipX_addr_ismulticast(is_ipv6, ipaddr)   ip_addr_ismulticast(ipaddr)
#define ipX_addr_debug_print(is_ipv6, debug, ipaddr) ip_addr_debug_print(debug, ipaddr)

#endif /* LWIP_IPV6 */

#ifdef __cplusplus
}
#endif

#endif /* __LWIP_IP_ADDR_H__ */
