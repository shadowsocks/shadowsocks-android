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
#ifndef __LWIP_SYS_H__
#define __LWIP_SYS_H__

#include "lwip/opt.h"

#ifdef __cplusplus
extern "C" {
#endif

#if NO_SYS

/* For a totally minimal and standalone system, we provide null
   definitions of the sys_ functions. */
typedef u8_t sys_sem_t;
typedef u8_t sys_mutex_t;
typedef u8_t sys_mbox_t;

#define sys_sem_new(s, c) ERR_OK
#define sys_sem_signal(s)
#define sys_sem_wait(s)
#define sys_arch_sem_wait(s,t)
#define sys_sem_free(s)
#define sys_sem_valid(s) 0
#define sys_sem_set_invalid(s)
#define sys_mutex_new(mu) ERR_OK
#define sys_mutex_lock(mu)
#define sys_mutex_unlock(mu)
#define sys_mutex_free(mu)
#define sys_mutex_valid(mu) 0
#define sys_mutex_set_invalid(mu)
#define sys_mbox_new(m, s) ERR_OK
#define sys_mbox_fetch(m,d)
#define sys_mbox_tryfetch(m,d)
#define sys_mbox_post(m,d)
#define sys_mbox_trypost(m,d)
#define sys_mbox_free(m)
#define sys_mbox_valid(m)
#define sys_mbox_set_invalid(m)

#define sys_thread_new(n,t,a,s,p)

#define sys_msleep(t)

#else /* NO_SYS */

/** Return code for timeouts from sys_arch_mbox_fetch and sys_arch_sem_wait */
#define SYS_ARCH_TIMEOUT 0xffffffffUL

/** sys_mbox_tryfetch() returns SYS_MBOX_EMPTY if appropriate.
 * For now we use the same magic value, but we allow this to change in future.
 */
#define SYS_MBOX_EMPTY SYS_ARCH_TIMEOUT 

#include "lwip/err.h"
#include "arch/sys_arch.h"

/** Function prototype for thread functions */
typedef void (*lwip_thread_fn)(void *arg);

/* Function prototypes for functions to be implemented by platform ports
   (in sys_arch.c) */

/* Mutex functions: */

/** Define LWIP_COMPAT_MUTEX if the port has no mutexes and binary semaphores
    should be used instead */
#if LWIP_COMPAT_MUTEX
/* for old ports that don't have mutexes: define them to binary semaphores */
#define sys_mutex_t                   sys_sem_t
#define sys_mutex_new(mutex)          sys_sem_new(mutex, 1)
#define sys_mutex_lock(mutex)         sys_sem_wait(mutex)
#define sys_mutex_unlock(mutex)       sys_sem_signal(mutex)
#define sys_mutex_free(mutex)         sys_sem_free(mutex)
#define sys_mutex_valid(mutex)        sys_sem_valid(mutex)
#define sys_mutex_set_invalid(mutex)  sys_sem_set_invalid(mutex)

#else /* LWIP_COMPAT_MUTEX */

/** Create a new mutex
 * @param mutex pointer to the mutex to create
 * @return a new mutex */
err_t sys_mutex_new(sys_mutex_t *mutex);
/** Lock a mutex
 * @param mutex the mutex to lock */
void sys_mutex_lock(sys_mutex_t *mutex);
/** Unlock a mutex
 * @param mutex the mutex to unlock */
void sys_mutex_unlock(sys_mutex_t *mutex);
/** Delete a semaphore
 * @param mutex the mutex to delete */
void sys_mutex_free(sys_mutex_t *mutex); 
#ifndef sys_mutex_valid
/** Check if a mutex is valid/allocated: return 1 for valid, 0 for invalid */
int sys_mutex_valid(sys_mutex_t *mutex);
#endif
#ifndef sys_mutex_set_invalid
/** Set a mutex invalid so that sys_mutex_valid returns 0 */
void sys_mutex_set_invalid(sys_mutex_t *mutex);
#endif
#endif /* LWIP_COMPAT_MUTEX */

/* Semaphore functions: */

/** Create a new semaphore
 * @param sem pointer to the semaphore to create
 * @param count initial count of the semaphore
 * @return ERR_OK if successful, another err_t otherwise */
err_t sys_sem_new(sys_sem_t *sem, u8_t count);
/** Signals a semaphore
 * @param sem the semaphore to signal */
void sys_sem_signal(sys_sem_t *sem);
/** Wait for a semaphore for the specified timeout
 * @param sem the semaphore to wait for
 * @param timeout timeout in milliseconds to wait (0 = wait forever)
 * @return time (in milliseconds) waited for the semaphore
 *         or SYS_ARCH_TIMEOUT on timeout */
u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout);
/** Delete a semaphore
 * @param sem semaphore to delete */
void sys_sem_free(sys_sem_t *sem);
/** Wait for a semaphore - forever/no timeout */
#define sys_sem_wait(sem)                  sys_arch_sem_wait(sem, 0)
#ifndef sys_sem_valid
/** Check if a sempahore is valid/allocated: return 1 for valid, 0 for invalid */
int sys_sem_valid(sys_sem_t *sem);
#endif
#ifndef sys_sem_set_invalid
/** Set a semaphore invalid so that sys_sem_valid returns 0 */
void sys_sem_set_invalid(sys_sem_t *sem);
#endif

/* Time functions. */
#ifndef sys_msleep
void sys_msleep(u32_t ms); /* only has a (close to) 1 jiffy resolution. */
#endif

/* Mailbox functions. */

/** Create a new mbox of specified size
 * @param mbox pointer to the mbox to create
 * @param size (miminum) number of messages in this mbox
 * @return ERR_OK if successful, another err_t otherwise */
err_t sys_mbox_new(sys_mbox_t *mbox, int size);
/** Post a message to an mbox - may not fail
 * -> blocks if full, only used from tasks not from ISR
 * @param mbox mbox to posts the message
 * @param msg message to post (ATTENTION: can be NULL) */
void sys_mbox_post(sys_mbox_t *mbox, void *msg);
/** Try to post a message to an mbox - may fail if full or ISR
 * @param mbox mbox to posts the message
 * @param msg message to post (ATTENTION: can be NULL) */
err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg);
/** Wait for a new message to arrive in the mbox
 * @param mbox mbox to get a message from
 * @param msg pointer where the message is stored
 * @param timeout maximum time (in milliseconds) to wait for a message (0 = wait forever)
 * @return time (in milliseconds) waited for a message, may be 0 if not waited
           or SYS_ARCH_TIMEOUT on timeout
 *         The returned time has to be accurate to prevent timer jitter! */
u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout);
/* Allow port to override with a macro, e.g. special timout for sys_arch_mbox_fetch() */
#ifndef sys_arch_mbox_tryfetch
/** Wait for a new message to arrive in the mbox
 * @param mbox mbox to get a message from
 * @param msg pointer where the message is stored
 * @return 0 (milliseconds) if a message has been received
 *         or SYS_MBOX_EMPTY if the mailbox is empty */
u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg);
#endif
/** For now, we map straight to sys_arch implementation. */
#define sys_mbox_tryfetch(mbox, msg) sys_arch_mbox_tryfetch(mbox, msg)
/** Delete an mbox
 * @param mbox mbox to delete */
void sys_mbox_free(sys_mbox_t *mbox);
#define sys_mbox_fetch(mbox, msg) sys_arch_mbox_fetch(mbox, msg, 0)
#ifndef sys_mbox_valid
/** Check if an mbox is valid/allocated: return 1 for valid, 0 for invalid */
int sys_mbox_valid(sys_mbox_t *mbox);
#endif
#ifndef sys_mbox_set_invalid
/** Set an mbox invalid so that sys_mbox_valid returns 0 */
void sys_mbox_set_invalid(sys_mbox_t *mbox);
#endif

/** The only thread function:
 * Creates a new thread
 * @param name human-readable name for the thread (used for debugging purposes)
 * @param thread thread-function
 * @param arg parameter passed to 'thread'
 * @param stacksize stack size in bytes for the new thread (may be ignored by ports)
 * @param prio priority of the new thread (may be ignored by ports) */
sys_thread_t sys_thread_new(const char *name, lwip_thread_fn thread, void *arg, int stacksize, int prio);

#endif /* NO_SYS */

/* sys_init() must be called before anthing else. */
void sys_init(void);

#ifndef sys_jiffies
/** Ticks/jiffies since power up. */
u32_t sys_jiffies(void);
#endif

/** Returns the current time in milliseconds,
 * may be the same as sys_jiffies or at least based on it. */
u32_t sys_now(void);

/* Critical Region Protection */
/* These functions must be implemented in the sys_arch.c file.
   In some implementations they can provide a more light-weight protection
   mechanism than using semaphores. Otherwise semaphores can be used for
   implementation */
#ifndef SYS_ARCH_PROTECT
/** SYS_LIGHTWEIGHT_PROT
 * define SYS_LIGHTWEIGHT_PROT in lwipopts.h if you want inter-task protection
 * for certain critical regions during buffer allocation, deallocation and memory
 * allocation and deallocation.
 */
#if SYS_LIGHTWEIGHT_PROT

/** SYS_ARCH_DECL_PROTECT
 * declare a protection variable. This macro will default to defining a variable of
 * type sys_prot_t. If a particular port needs a different implementation, then
 * this macro may be defined in sys_arch.h.
 */
#define SYS_ARCH_DECL_PROTECT(lev) sys_prot_t lev
/** SYS_ARCH_PROTECT
 * Perform a "fast" protect. This could be implemented by
 * disabling interrupts for an embedded system or by using a semaphore or
 * mutex. The implementation should allow calling SYS_ARCH_PROTECT when
 * already protected. The old protection level is returned in the variable
 * "lev". This macro will default to calling the sys_arch_protect() function
 * which should be implemented in sys_arch.c. If a particular port needs a
 * different implementation, then this macro may be defined in sys_arch.h
 */
#define SYS_ARCH_PROTECT(lev) lev = sys_arch_protect()
/** SYS_ARCH_UNPROTECT
 * Perform a "fast" set of the protection level to "lev". This could be
 * implemented by setting the interrupt level to "lev" within the MACRO or by
 * using a semaphore or mutex.  This macro will default to calling the
 * sys_arch_unprotect() function which should be implemented in
 * sys_arch.c. If a particular port needs a different implementation, then
 * this macro may be defined in sys_arch.h
 */
#define SYS_ARCH_UNPROTECT(lev) sys_arch_unprotect(lev)
sys_prot_t sys_arch_protect(void);
void sys_arch_unprotect(sys_prot_t pval);

#else

#define SYS_ARCH_DECL_PROTECT(lev)
#define SYS_ARCH_PROTECT(lev)
#define SYS_ARCH_UNPROTECT(lev)

#endif /* SYS_LIGHTWEIGHT_PROT */

#endif /* SYS_ARCH_PROTECT */

/*
 * Macros to set/get and increase/decrease variables in a thread-safe way.
 * Use these for accessing variable that are used from more than one thread.
 */

#ifndef SYS_ARCH_INC
#define SYS_ARCH_INC(var, val) do { \
                                SYS_ARCH_DECL_PROTECT(old_level); \
                                SYS_ARCH_PROTECT(old_level); \
                                var += val; \
                                SYS_ARCH_UNPROTECT(old_level); \
                              } while(0)
#endif /* SYS_ARCH_INC */

#ifndef SYS_ARCH_DEC
#define SYS_ARCH_DEC(var, val) do { \
                                SYS_ARCH_DECL_PROTECT(old_level); \
                                SYS_ARCH_PROTECT(old_level); \
                                var -= val; \
                                SYS_ARCH_UNPROTECT(old_level); \
                              } while(0)
#endif /* SYS_ARCH_DEC */

#ifndef SYS_ARCH_GET
#define SYS_ARCH_GET(var, ret) do { \
                                SYS_ARCH_DECL_PROTECT(old_level); \
                                SYS_ARCH_PROTECT(old_level); \
                                ret = var; \
                                SYS_ARCH_UNPROTECT(old_level); \
                              } while(0)
#endif /* SYS_ARCH_GET */

#ifndef SYS_ARCH_SET
#define SYS_ARCH_SET(var, val) do { \
                                SYS_ARCH_DECL_PROTECT(old_level); \
                                SYS_ARCH_PROTECT(old_level); \
                                var = val; \
                                SYS_ARCH_UNPROTECT(old_level); \
                              } while(0)
#endif /* SYS_ARCH_SET */


#ifdef __cplusplus
}
#endif

#endif /* __LWIP_SYS_H__ */
