/*
 * Copyright (c) 2009-2012 Niels Provos and Nick Mathewson
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
#ifndef _DEFER_INTERNAL_H_
#define _DEFER_INTERNAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "event2/event-config.h"
#include <sys/queue.h>

struct deferred_cb;

typedef void (*deferred_cb_fn)(struct deferred_cb *, void *);

/** A deferred_cb is a callback that can be scheduled to run as part of
 * an event_base's event_loop, rather than running immediately. */
struct deferred_cb {
	/** Links to the adjacent active (pending) deferred_cb objects. */
	TAILQ_ENTRY (deferred_cb) cb_next;
	/** True iff this deferred_cb is pending in an event_base. */
	unsigned queued : 1;
	/** The function to execute when the callback runs. */
	deferred_cb_fn cb;
	/** The function's second argument. */
	void *arg;
};

/** A deferred_cb_queue is a list of deferred_cb that we can add to and run. */
struct deferred_cb_queue {
	/** Lock used to protect the queue. */
	void *lock;

	/** How many entries are in the queue? */
	int active_count;

	/** Function called when adding to the queue from another thread. */
	void (*notify_fn)(struct deferred_cb_queue *, void *);
	void *notify_arg;

	/** Deferred callback management: a list of deferred callbacks to
	 * run active the active events. */
	TAILQ_HEAD (deferred_cb_list, deferred_cb) deferred_cb_list;
};

/**
   Initialize an empty, non-pending deferred_cb.

   @param deferred The deferred_cb structure to initialize.
   @param cb The function to run when the deferred_cb executes.
   @param arg The function's second argument.
 */
void event_deferred_cb_init(struct deferred_cb *, deferred_cb_fn, void *);
/**
   Cancel a deferred_cb if it is currently scheduled in an event_base.
 */
void event_deferred_cb_cancel(struct deferred_cb_queue *, struct deferred_cb *);
/**
   Activate a deferred_cb if it is not currently scheduled in an event_base.
 */
void event_deferred_cb_schedule(struct deferred_cb_queue *, struct deferred_cb *);

#define LOCK_DEFERRED_QUEUE(q)						\
	EVLOCK_LOCK((q)->lock, 0)
#define UNLOCK_DEFERRED_QUEUE(q)					\
	EVLOCK_UNLOCK((q)->lock, 0)

#ifdef __cplusplus
}
#endif

void event_deferred_cb_queue_init(struct deferred_cb_queue *);
struct deferred_cb_queue *event_base_get_deferred_cb_queue(struct event_base *);

#endif /* _EVENT_INTERNAL_H_ */

