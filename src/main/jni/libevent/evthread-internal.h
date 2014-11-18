/*
 * Copyright (c) 2008-2012 Niels Provos, Nick Mathewson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _EVTHREAD_INTERNAL_H_
#define _EVTHREAD_INTERNAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "event2/thread.h"
#include "event2/event-config.h"
#include "util-internal.h"

struct event_base;

#ifndef WIN32
/* On Windows, the way we currently make DLLs, it's not allowed for us to
 * have shared global structures.  Thus, we only do the direct-call-to-function
 * code path if we know that the local shared library system supports it.
 */
#define EVTHREAD_EXPOSE_STRUCTS
#endif

#if ! defined(_EVENT_DISABLE_THREAD_SUPPORT) && defined(EVTHREAD_EXPOSE_STRUCTS)
/* Global function pointers to lock-related functions. NULL if locking isn't
   enabled. */
extern struct evthread_lock_callbacks _evthread_lock_fns;
extern struct evthread_condition_callbacks _evthread_cond_fns;
extern unsigned long (*_evthread_id_fn)(void);
extern int _evthread_lock_debugging_enabled;

/** Return the ID of the current thread, or 1 if threading isn't enabled. */
#define EVTHREAD_GET_ID() \
	(_evthread_id_fn ? _evthread_id_fn() : 1)

/** Return true iff we're in the thread that is currently (or most recently)
 * running a given event_base's loop. Requires lock. */
#define EVBASE_IN_THREAD(base)				 \
	(_evthread_id_fn == NULL ||			 \
	(base)->th_owner_id == _evthread_id_fn())

/** Return true iff we need to notify the base's main thread about changes to
 * its state, because it's currently running the main loop in another
 * thread. Requires lock. */
#define EVBASE_NEED_NOTIFY(base)			 \
	(_evthread_id_fn != NULL &&			 \
	    (base)->running_loop &&			 \
	    (base)->th_owner_id != _evthread_id_fn())

/** Allocate a new lock, and store it in lockvar, a void*.  Sets lockvar to
    NULL if locking is not enabled. */
#define EVTHREAD_ALLOC_LOCK(lockvar, locktype)		\
	((lockvar) = _evthread_lock_fns.alloc ?		\
	    _evthread_lock_fns.alloc(locktype) : NULL)

/** Free a given lock, if it is present and locking is enabled. */
#define EVTHREAD_FREE_LOCK(lockvar, locktype)				\
	do {								\
		void *_lock_tmp_ = (lockvar);				\
		if (_lock_tmp_ && _evthread_lock_fns.free)		\
			_evthread_lock_fns.free(_lock_tmp_, (locktype)); \
	} while (0)

/** Acquire a lock. */
#define EVLOCK_LOCK(lockvar,mode)					\
	do {								\
		if (lockvar)						\
			_evthread_lock_fns.lock(mode, lockvar);		\
	} while (0)

/** Release a lock */
#define EVLOCK_UNLOCK(lockvar,mode)					\
	do {								\
		if (lockvar)						\
			_evthread_lock_fns.unlock(mode, lockvar);	\
	} while (0)

/** Helper: put lockvar1 and lockvar2 into pointerwise ascending order. */
#define _EVLOCK_SORTLOCKS(lockvar1, lockvar2)				\
	do {								\
		if (lockvar1 && lockvar2 && lockvar1 > lockvar2) {	\
			void *tmp = lockvar1;				\
			lockvar1 = lockvar2;				\
			lockvar2 = tmp;					\
		}							\
	} while (0)

/** Lock an event_base, if it is set up for locking.  Acquires the lock
    in the base structure whose field is named 'lockvar'. */
#define EVBASE_ACQUIRE_LOCK(base, lockvar) do {				\
		EVLOCK_LOCK((base)->lockvar, 0);			\
	} while (0)

/** Unlock an event_base, if it is set up for locking. */
#define EVBASE_RELEASE_LOCK(base, lockvar) do {				\
		EVLOCK_UNLOCK((base)->lockvar, 0);			\
	} while (0)

/** If lock debugging is enabled, and lock is non-null, assert that 'lock' is
 * locked and held by us. */
#define EVLOCK_ASSERT_LOCKED(lock)					\
	do {								\
		if ((lock) && _evthread_lock_debugging_enabled) {	\
			EVUTIL_ASSERT(_evthread_is_debug_lock_held(lock)); \
		}							\
	} while (0)

/** Try to grab the lock for 'lockvar' without blocking, and return 1 if we
 * manage to get it. */
static inline int EVLOCK_TRY_LOCK(void *lock);
static inline int
EVLOCK_TRY_LOCK(void *lock)
{
	if (lock && _evthread_lock_fns.lock) {
		int r = _evthread_lock_fns.lock(EVTHREAD_TRY, lock);
		return !r;
	} else {
		/* Locking is disabled either globally or for this thing;
		 * of course we count as having the lock. */
		return 1;
	}
}

/** Allocate a new condition variable and store it in the void *, condvar */
#define EVTHREAD_ALLOC_COND(condvar)					\
	do {								\
		(condvar) = _evthread_cond_fns.alloc_condition ?	\
		    _evthread_cond_fns.alloc_condition(0) : NULL;	\
	} while (0)
/** Deallocate and free a condition variable in condvar */
#define EVTHREAD_FREE_COND(cond)					\
	do {								\
		if (cond)						\
			_evthread_cond_fns.free_condition((cond));	\
	} while (0)
/** Signal one thread waiting on cond */
#define EVTHREAD_COND_SIGNAL(cond)					\
	( (cond) ? _evthread_cond_fns.signal_condition((cond), 0) : 0 )
/** Signal all threads waiting on cond */
#define EVTHREAD_COND_BROADCAST(cond)					\
	( (cond) ? _evthread_cond_fns.signal_condition((cond), 1) : 0 )
/** Wait until the condition 'cond' is signalled.  Must be called while
 * holding 'lock'.  The lock will be released until the condition is
 * signalled, at which point it will be acquired again.  Returns 0 for
 * success, -1 for failure. */
#define EVTHREAD_COND_WAIT(cond, lock)					\
	( (cond) ? _evthread_cond_fns.wait_condition((cond), (lock), NULL) : 0 )
/** As EVTHREAD_COND_WAIT, but gives up after 'tv' has elapsed.  Returns 1
 * on timeout. */
#define EVTHREAD_COND_WAIT_TIMED(cond, lock, tv)			\
	( (cond) ? _evthread_cond_fns.wait_condition((cond), (lock), (tv)) : 0 )

/** True iff locking functions have been configured. */
#define EVTHREAD_LOCKING_ENABLED()		\
	(_evthread_lock_fns.lock != NULL)

#elif ! defined(_EVENT_DISABLE_THREAD_SUPPORT)

unsigned long _evthreadimpl_get_id(void);
int _evthreadimpl_is_lock_debugging_enabled(void);
void *_evthreadimpl_lock_alloc(unsigned locktype);
void _evthreadimpl_lock_free(void *lock, unsigned locktype);
int _evthreadimpl_lock_lock(unsigned mode, void *lock);
int _evthreadimpl_lock_unlock(unsigned mode, void *lock);
void *_evthreadimpl_cond_alloc(unsigned condtype);
void _evthreadimpl_cond_free(void *cond);
int _evthreadimpl_cond_signal(void *cond, int broadcast);
int _evthreadimpl_cond_wait(void *cond, void *lock, const struct timeval *tv);
int _evthreadimpl_locking_enabled(void);

#define EVTHREAD_GET_ID() _evthreadimpl_get_id()
#define EVBASE_IN_THREAD(base)				\
	((base)->th_owner_id == _evthreadimpl_get_id())
#define EVBASE_NEED_NOTIFY(base)			 \
	((base)->running_loop &&			 \
	    ((base)->th_owner_id != _evthreadimpl_get_id()))

#define EVTHREAD_ALLOC_LOCK(lockvar, locktype)		\
	((lockvar) = _evthreadimpl_lock_alloc(locktype))

#define EVTHREAD_FREE_LOCK(lockvar, locktype)				\
	do {								\
		void *_lock_tmp_ = (lockvar);				\
		if (_lock_tmp_)						\
			_evthreadimpl_lock_free(_lock_tmp_, (locktype)); \
	} while (0)

/** Acquire a lock. */
#define EVLOCK_LOCK(lockvar,mode)					\
	do {								\
		if (lockvar)						\
			_evthreadimpl_lock_lock(mode, lockvar);		\
	} while (0)

/** Release a lock */
#define EVLOCK_UNLOCK(lockvar,mode)					\
	do {								\
		if (lockvar)						\
			_evthreadimpl_lock_unlock(mode, lockvar);	\
	} while (0)

/** Lock an event_base, if it is set up for locking.  Acquires the lock
    in the base structure whose field is named 'lockvar'. */
#define EVBASE_ACQUIRE_LOCK(base, lockvar) do {				\
		EVLOCK_LOCK((base)->lockvar, 0);			\
	} while (0)

/** Unlock an event_base, if it is set up for locking. */
#define EVBASE_RELEASE_LOCK(base, lockvar) do {				\
		EVLOCK_UNLOCK((base)->lockvar, 0);			\
	} while (0)

/** If lock debugging is enabled, and lock is non-null, assert that 'lock' is
 * locked and held by us. */
#define EVLOCK_ASSERT_LOCKED(lock)					\
	do {								\
		if ((lock) && _evthreadimpl_is_lock_debugging_enabled()) { \
			EVUTIL_ASSERT(_evthread_is_debug_lock_held(lock)); \
		}							\
	} while (0)

/** Try to grab the lock for 'lockvar' without blocking, and return 1 if we
 * manage to get it. */
static inline int EVLOCK_TRY_LOCK(void *lock);
static inline int
EVLOCK_TRY_LOCK(void *lock)
{
	if (lock) {
		int r = _evthreadimpl_lock_lock(EVTHREAD_TRY, lock);
		return !r;
	} else {
		/* Locking is disabled either globally or for this thing;
		 * of course we count as having the lock. */
		return 1;
	}
}

/** Allocate a new condition variable and store it in the void *, condvar */
#define EVTHREAD_ALLOC_COND(condvar)					\
	do {								\
		(condvar) = _evthreadimpl_cond_alloc(0);		\
	} while (0)
/** Deallocate and free a condition variable in condvar */
#define EVTHREAD_FREE_COND(cond)					\
	do {								\
		if (cond)						\
			_evthreadimpl_cond_free((cond));		\
	} while (0)
/** Signal one thread waiting on cond */
#define EVTHREAD_COND_SIGNAL(cond)					\
	( (cond) ? _evthreadimpl_cond_signal((cond), 0) : 0 )
/** Signal all threads waiting on cond */
#define EVTHREAD_COND_BROADCAST(cond)					\
	( (cond) ? _evthreadimpl_cond_signal((cond), 1) : 0 )
/** Wait until the condition 'cond' is signalled.  Must be called while
 * holding 'lock'.  The lock will be released until the condition is
 * signalled, at which point it will be acquired again.  Returns 0 for
 * success, -1 for failure. */
#define EVTHREAD_COND_WAIT(cond, lock)					\
	( (cond) ? _evthreadimpl_cond_wait((cond), (lock), NULL) : 0 )
/** As EVTHREAD_COND_WAIT, but gives up after 'tv' has elapsed.  Returns 1
 * on timeout. */
#define EVTHREAD_COND_WAIT_TIMED(cond, lock, tv)			\
	( (cond) ? _evthreadimpl_cond_wait((cond), (lock), (tv)) : 0 )

#define EVTHREAD_LOCKING_ENABLED()		\
	(_evthreadimpl_locking_enabled())

#else /* _EVENT_DISABLE_THREAD_SUPPORT */

#define EVTHREAD_GET_ID()	1
#define EVTHREAD_ALLOC_LOCK(lockvar, locktype) _EVUTIL_NIL_STMT
#define EVTHREAD_FREE_LOCK(lockvar, locktype) _EVUTIL_NIL_STMT

#define EVLOCK_LOCK(lockvar, mode) _EVUTIL_NIL_STMT
#define EVLOCK_UNLOCK(lockvar, mode) _EVUTIL_NIL_STMT
#define EVLOCK_LOCK2(lock1,lock2,mode1,mode2) _EVUTIL_NIL_STMT
#define EVLOCK_UNLOCK2(lock1,lock2,mode1,mode2) _EVUTIL_NIL_STMT

#define EVBASE_IN_THREAD(base)	1
#define EVBASE_NEED_NOTIFY(base) 0
#define EVBASE_ACQUIRE_LOCK(base, lock) _EVUTIL_NIL_STMT
#define EVBASE_RELEASE_LOCK(base, lock) _EVUTIL_NIL_STMT
#define EVLOCK_ASSERT_LOCKED(lock) _EVUTIL_NIL_STMT

#define EVLOCK_TRY_LOCK(lock) 1

#define EVTHREAD_ALLOC_COND(condvar) _EVUTIL_NIL_STMT
#define EVTHREAD_FREE_COND(cond) _EVUTIL_NIL_STMT
#define EVTHREAD_COND_SIGNAL(cond) _EVUTIL_NIL_STMT
#define EVTHREAD_COND_BROADCAST(cond) _EVUTIL_NIL_STMT
#define EVTHREAD_COND_WAIT(cond, lock) _EVUTIL_NIL_STMT
#define EVTHREAD_COND_WAIT_TIMED(cond, lock, howlong) _EVUTIL_NIL_STMT

#define EVTHREAD_LOCKING_ENABLED() 0

#endif

/* This code is shared between both lock impls */
#if ! defined(_EVENT_DISABLE_THREAD_SUPPORT)
/** Helper: put lockvar1 and lockvar2 into pointerwise ascending order. */
#define _EVLOCK_SORTLOCKS(lockvar1, lockvar2)				\
	do {								\
		if (lockvar1 && lockvar2 && lockvar1 > lockvar2) {	\
			void *tmp = lockvar1;				\
			lockvar1 = lockvar2;				\
			lockvar2 = tmp;					\
		}							\
	} while (0)

/** Acquire both lock1 and lock2.  Always allocates locks in the same order,
 * so that two threads locking two locks with LOCK2 will not deadlock. */
#define EVLOCK_LOCK2(lock1,lock2,mode1,mode2)				\
	do {								\
		void *_lock1_tmplock = (lock1);				\
		void *_lock2_tmplock = (lock2);				\
		_EVLOCK_SORTLOCKS(_lock1_tmplock,_lock2_tmplock);	\
		EVLOCK_LOCK(_lock1_tmplock,mode1);			\
		if (_lock2_tmplock != _lock1_tmplock)			\
			EVLOCK_LOCK(_lock2_tmplock,mode2);		\
	} while (0)
/** Release both lock1 and lock2.  */
#define EVLOCK_UNLOCK2(lock1,lock2,mode1,mode2)				\
	do {								\
		void *_lock1_tmplock = (lock1);				\
		void *_lock2_tmplock = (lock2);				\
		_EVLOCK_SORTLOCKS(_lock1_tmplock,_lock2_tmplock);	\
		if (_lock2_tmplock != _lock1_tmplock)			\
			EVLOCK_UNLOCK(_lock2_tmplock,mode2);		\
		EVLOCK_UNLOCK(_lock1_tmplock,mode1);			\
	} while (0)

int _evthread_is_debug_lock_held(void *lock);
void *_evthread_debug_get_real_lock(void *lock);

void *evthread_setup_global_lock_(void *lock_, unsigned locktype,
    int enable_locks);

#define EVTHREAD_SETUP_GLOBAL_LOCK(lockvar, locktype)			\
	do {								\
		lockvar = evthread_setup_global_lock_(lockvar,		\
		    (locktype), enable_locks);				\
		if (!lockvar) {						\
			event_warn("Couldn't allocate %s", #lockvar);	\
			return -1;					\
		}							\
	} while (0);

int event_global_setup_locks_(const int enable_locks);
int evsig_global_setup_locks_(const int enable_locks);
int evutil_secure_rng_global_setup_locks_(const int enable_locks);

#endif

#ifdef __cplusplus
}
#endif

#endif /* _EVTHREAD_INTERNAL_H_ */
