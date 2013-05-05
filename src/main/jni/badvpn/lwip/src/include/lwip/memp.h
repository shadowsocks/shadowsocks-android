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

#ifndef __LWIP_MEMP_H__
#define __LWIP_MEMP_H__

#include "lwip/opt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Create the list of all memory pools managed by memp. MEMP_MAX represents a NULL pool at the end */
typedef enum {
#define LWIP_MEMPOOL(name,num,size,desc)  MEMP_##name,
#include "lwip/memp_std.h"
  MEMP_MAX
} memp_t;

#if MEM_USE_POOLS
/* Use a helper type to get the start and end of the user "memory pools" for mem_malloc */
typedef enum {
    /* Get the first (via:
       MEMP_POOL_HELPER_START = ((u8_t) 1*MEMP_POOL_A + 0*MEMP_POOL_B + 0*MEMP_POOL_C + 0)*/
    MEMP_POOL_HELPER_FIRST = ((u8_t)
#define LWIP_MEMPOOL(name,num,size,desc)
#define LWIP_MALLOC_MEMPOOL_START 1
#define LWIP_MALLOC_MEMPOOL(num, size) * MEMP_POOL_##size + 0
#define LWIP_MALLOC_MEMPOOL_END
#include "lwip/memp_std.h"
    ) ,
    /* Get the last (via:
       MEMP_POOL_HELPER_END = ((u8_t) 0 + MEMP_POOL_A*0 + MEMP_POOL_B*0 + MEMP_POOL_C*1) */
    MEMP_POOL_HELPER_LAST = ((u8_t)
#define LWIP_MEMPOOL(name,num,size,desc)
#define LWIP_MALLOC_MEMPOOL_START
#define LWIP_MALLOC_MEMPOOL(num, size) 0 + MEMP_POOL_##size *
#define LWIP_MALLOC_MEMPOOL_END 1
#include "lwip/memp_std.h"
    )
} memp_pool_helper_t;

/* The actual start and stop values are here (cast them over)
   We use this helper type and these defines so we can avoid using const memp_t values */
#define MEMP_POOL_FIRST ((memp_t) MEMP_POOL_HELPER_FIRST)
#define MEMP_POOL_LAST   ((memp_t) MEMP_POOL_HELPER_LAST)
#endif /* MEM_USE_POOLS */

#if MEMP_MEM_MALLOC || MEM_USE_POOLS
extern const u16_t memp_sizes[MEMP_MAX];
#endif /* MEMP_MEM_MALLOC || MEM_USE_POOLS */

#if MEMP_MEM_MALLOC

#include "mem.h"

#define memp_init()
#define memp_malloc(type)     mem_malloc(memp_sizes[type])
#define memp_free(type, mem)  mem_free(mem)

#else /* MEMP_MEM_MALLOC */

#if MEM_USE_POOLS
/** This structure is used to save the pool one element came from. */
struct memp_malloc_helper
{
   memp_t poolnr;
};
#endif /* MEM_USE_POOLS */

void  memp_init(void);

#if MEMP_OVERFLOW_CHECK
void *memp_malloc_fn(memp_t type, const char* file, const int line);
#define memp_malloc(t) memp_malloc_fn((t), __FILE__, __LINE__)
#else
void *memp_malloc(memp_t type);
#endif
void  memp_free(memp_t type, void *mem);

#endif /* MEMP_MEM_MALLOC */

#ifdef __cplusplus
}
#endif

#endif /* __LWIP_MEMP_H__ */
