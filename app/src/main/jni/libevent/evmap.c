/*
 * Copyright (c) 2007-2012 Niels Provos and Nick Mathewson
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
#include "event2/event-config.h"

#ifdef WIN32
#include <winsock2.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#endif
#include <sys/types.h>
#if !defined(WIN32) && defined(_EVENT_HAVE_SYS_TIME_H)
#include <sys/time.h>
#endif
#include <sys/queue.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <time.h>

#include "event-internal.h"
#include "evmap-internal.h"
#include "mm-internal.h"
#include "changelist-internal.h"

/** An entry for an evmap_io list: notes all the events that want to read or
	write on a given fd, and the number of each.
  */
struct evmap_io {
	struct event_list events;
	ev_uint16_t nread;
	ev_uint16_t nwrite;
};

/* An entry for an evmap_signal list: notes all the events that want to know
   when a signal triggers. */
struct evmap_signal {
	struct event_list events;
};

/* On some platforms, fds start at 0 and increment by 1 as they are
   allocated, and old numbers get used.  For these platforms, we
   implement io maps just like signal maps: as an array of pointers to
   struct evmap_io.  But on other platforms (windows), sockets are not
   0-indexed, not necessarily consecutive, and not necessarily reused.
   There, we use a hashtable to implement evmap_io.
*/
#ifdef EVMAP_USE_HT
struct event_map_entry {
	HT_ENTRY(event_map_entry) map_node;
	evutil_socket_t fd;
	union { /* This is a union in case we need to make more things that can
			   be in the hashtable. */
		struct evmap_io evmap_io;
	} ent;
};

/* Helper used by the event_io_map hashtable code; tries to return a good hash
 * of the fd in e->fd. */
static inline unsigned
hashsocket(struct event_map_entry *e)
{
	/* On win32, in practice, the low 2-3 bits of a SOCKET seem not to
	 * matter.  Our hashtable implementation really likes low-order bits,
	 * though, so let's do the rotate-and-add trick. */
	unsigned h = (unsigned) e->fd;
	h += (h >> 2) | (h << 30);
	return h;
}

/* Helper used by the event_io_map hashtable code; returns true iff e1 and e2
 * have the same e->fd. */
static inline int
eqsocket(struct event_map_entry *e1, struct event_map_entry *e2)
{
	return e1->fd == e2->fd;
}

HT_PROTOTYPE(event_io_map, event_map_entry, map_node, hashsocket, eqsocket)
HT_GENERATE(event_io_map, event_map_entry, map_node, hashsocket, eqsocket,
			0.5, mm_malloc, mm_realloc, mm_free)

#define GET_IO_SLOT(x, map, slot, type)					\
	do {								\
		struct event_map_entry _key, *_ent;			\
		_key.fd = slot;						\
		_ent = HT_FIND(event_io_map, map, &_key);		\
		(x) = _ent ? &_ent->ent.type : NULL;			\
	} while (0);

#define GET_IO_SLOT_AND_CTOR(x, map, slot, type, ctor, fdinfo_len)	\
	do {								\
		struct event_map_entry _key, *_ent;			\
		_key.fd = slot;						\
		_HT_FIND_OR_INSERT(event_io_map, map_node, hashsocket, map, \
		    event_map_entry, &_key, ptr,			\
		    {							\
			    _ent = *ptr;				\
		    },							\
		    {							\
			    _ent = mm_calloc(1,sizeof(struct event_map_entry)+fdinfo_len); \
			    if (EVUTIL_UNLIKELY(_ent == NULL))		\
				    return (-1);			\
			    _ent->fd = slot;				\
			    (ctor)(&_ent->ent.type);			\
			    _HT_FOI_INSERT(map_node, map, &_key, _ent, ptr) \
				});					\
		(x) = &_ent->ent.type;					\
	} while (0)

void evmap_io_initmap(struct event_io_map *ctx)
{
	HT_INIT(event_io_map, ctx);
}

void evmap_io_clear(struct event_io_map *ctx)
{
	struct event_map_entry **ent, **next, *this;
	for (ent = HT_START(event_io_map, ctx); ent; ent = next) {
		this = *ent;
		next = HT_NEXT_RMV(event_io_map, ctx, ent);
		mm_free(this);
	}
	HT_CLEAR(event_io_map, ctx); /* remove all storage held by the ctx. */
}
#endif

/* Set the variable 'x' to the field in event_map 'map' with fields of type
   'struct type *' corresponding to the fd or signal 'slot'.  Set 'x' to NULL
   if there are no entries for 'slot'.  Does no bounds-checking. */
#define GET_SIGNAL_SLOT(x, map, slot, type)			\
	(x) = (struct type *)((map)->entries[slot])
/* As GET_SLOT, but construct the entry for 'slot' if it is not present,
   by allocating enough memory for a 'struct type', and initializing the new
   value by calling the function 'ctor' on it.  Makes the function
   return -1 on allocation failure.
 */
#define GET_SIGNAL_SLOT_AND_CTOR(x, map, slot, type, ctor, fdinfo_len)	\
	do {								\
		if ((map)->entries[slot] == NULL) {			\
			(map)->entries[slot] =				\
			    mm_calloc(1,sizeof(struct type)+fdinfo_len); \
			if (EVUTIL_UNLIKELY((map)->entries[slot] == NULL)) \
				return (-1);				\
			(ctor)((struct type *)(map)->entries[slot]);	\
		}							\
		(x) = (struct type *)((map)->entries[slot]);		\
	} while (0)

/* If we aren't using hashtables, then define the IO_SLOT macros and functions
   as thin aliases over the SIGNAL_SLOT versions. */
#ifndef EVMAP_USE_HT
#define GET_IO_SLOT(x,map,slot,type) GET_SIGNAL_SLOT(x,map,slot,type)
#define GET_IO_SLOT_AND_CTOR(x,map,slot,type,ctor,fdinfo_len)	\
	GET_SIGNAL_SLOT_AND_CTOR(x,map,slot,type,ctor,fdinfo_len)
#define FDINFO_OFFSET sizeof(struct evmap_io)
void
evmap_io_initmap(struct event_io_map* ctx)
{
	evmap_signal_initmap(ctx);
}
void
evmap_io_clear(struct event_io_map* ctx)
{
	evmap_signal_clear(ctx);
}
#endif


/** Expand 'map' with new entries of width 'msize' until it is big enough
	to store a value in 'slot'.
 */
static int
evmap_make_space(struct event_signal_map *map, int slot, int msize)
{
	if (map->nentries <= slot) {
		int nentries = map->nentries ? map->nentries : 32;
		void **tmp;

		while (nentries <= slot)
			nentries <<= 1;

		tmp = (void **)mm_realloc(map->entries, nentries * msize);
		if (tmp == NULL)
			return (-1);

		memset(&tmp[map->nentries], 0,
		    (nentries - map->nentries) * msize);

		map->nentries = nentries;
		map->entries = tmp;
	}

	return (0);
}

void
evmap_signal_initmap(struct event_signal_map *ctx)
{
	ctx->nentries = 0;
	ctx->entries = NULL;
}

void
evmap_signal_clear(struct event_signal_map *ctx)
{
	if (ctx->entries != NULL) {
		int i;
		for (i = 0; i < ctx->nentries; ++i) {
			if (ctx->entries[i] != NULL)
				mm_free(ctx->entries[i]);
		}
		mm_free(ctx->entries);
		ctx->entries = NULL;
	}
	ctx->nentries = 0;
}


/* code specific to file descriptors */

/** Constructor for struct evmap_io */
static void
evmap_io_init(struct evmap_io *entry)
{
	TAILQ_INIT(&entry->events);
	entry->nread = 0;
	entry->nwrite = 0;
}


/* return -1 on error, 0 on success if nothing changed in the event backend,
 * and 1 on success if something did. */
int
evmap_io_add(struct event_base *base, evutil_socket_t fd, struct event *ev)
{
	const struct eventop *evsel = base->evsel;
	struct event_io_map *io = &base->io;
	struct evmap_io *ctx = NULL;
	int nread, nwrite, retval = 0;
	short res = 0, old = 0;
	struct event *old_ev;

	EVUTIL_ASSERT(fd == ev->ev_fd);

	if (fd < 0)
		return 0;

#ifndef EVMAP_USE_HT
	if (fd >= io->nentries) {
		if (evmap_make_space(io, fd, sizeof(struct evmap_io *)) == -1)
			return (-1);
	}
#endif
	GET_IO_SLOT_AND_CTOR(ctx, io, fd, evmap_io, evmap_io_init,
						 evsel->fdinfo_len);

	nread = ctx->nread;
	nwrite = ctx->nwrite;

	if (nread)
		old |= EV_READ;
	if (nwrite)
		old |= EV_WRITE;

	if (ev->ev_events & EV_READ) {
		if (++nread == 1)
			res |= EV_READ;
	}
	if (ev->ev_events & EV_WRITE) {
		if (++nwrite == 1)
			res |= EV_WRITE;
	}
	if (EVUTIL_UNLIKELY(nread > 0xffff || nwrite > 0xffff)) {
		event_warnx("Too many events reading or writing on fd %d",
		    (int)fd);
		return -1;
	}
	if (EVENT_DEBUG_MODE_IS_ON() &&
	    (old_ev = TAILQ_FIRST(&ctx->events)) &&
	    (old_ev->ev_events&EV_ET) != (ev->ev_events&EV_ET)) {
		event_warnx("Tried to mix edge-triggered and non-edge-triggered"
		    " events on fd %d", (int)fd);
		return -1;
	}

	if (res) {
		void *extra = ((char*)ctx) + sizeof(struct evmap_io);
		/* XXX(niels): we cannot mix edge-triggered and
		 * level-triggered, we should probably assert on
		 * this. */
		if (evsel->add(base, ev->ev_fd,
			old, (ev->ev_events & EV_ET) | res, extra) == -1)
			return (-1);
		retval = 1;
	}

	ctx->nread = (ev_uint16_t) nread;
	ctx->nwrite = (ev_uint16_t) nwrite;
	TAILQ_INSERT_TAIL(&ctx->events, ev, ev_io_next);

	return (retval);
}

/* return -1 on error, 0 on success if nothing changed in the event backend,
 * and 1 on success if something did. */
int
evmap_io_del(struct event_base *base, evutil_socket_t fd, struct event *ev)
{
	const struct eventop *evsel = base->evsel;
	struct event_io_map *io = &base->io;
	struct evmap_io *ctx;
	int nread, nwrite, retval = 0;
	short res = 0, old = 0;

	if (fd < 0)
		return 0;

	EVUTIL_ASSERT(fd == ev->ev_fd);

#ifndef EVMAP_USE_HT
	if (fd >= io->nentries)
		return (-1);
#endif

	GET_IO_SLOT(ctx, io, fd, evmap_io);

	nread = ctx->nread;
	nwrite = ctx->nwrite;

	if (nread)
		old |= EV_READ;
	if (nwrite)
		old |= EV_WRITE;

	if (ev->ev_events & EV_READ) {
		if (--nread == 0)
			res |= EV_READ;
		EVUTIL_ASSERT(nread >= 0);
	}
	if (ev->ev_events & EV_WRITE) {
		if (--nwrite == 0)
			res |= EV_WRITE;
		EVUTIL_ASSERT(nwrite >= 0);
	}

	if (res) {
		void *extra = ((char*)ctx) + sizeof(struct evmap_io);
		if (evsel->del(base, ev->ev_fd, old, res, extra) == -1)
			return (-1);
		retval = 1;
	}

	ctx->nread = nread;
	ctx->nwrite = nwrite;
	TAILQ_REMOVE(&ctx->events, ev, ev_io_next);

	return (retval);
}

void
evmap_io_active(struct event_base *base, evutil_socket_t fd, short events)
{
	struct event_io_map *io = &base->io;
	struct evmap_io *ctx;
	struct event *ev;

#ifndef EVMAP_USE_HT
	EVUTIL_ASSERT(fd < io->nentries);
#endif
	GET_IO_SLOT(ctx, io, fd, evmap_io);

	EVUTIL_ASSERT(ctx);
	TAILQ_FOREACH(ev, &ctx->events, ev_io_next) {
		if (ev->ev_events & events)
			event_active_nolock(ev, ev->ev_events & events, 1);
	}
}

/* code specific to signals */

static void
evmap_signal_init(struct evmap_signal *entry)
{
	TAILQ_INIT(&entry->events);
}


int
evmap_signal_add(struct event_base *base, int sig, struct event *ev)
{
	const struct eventop *evsel = base->evsigsel;
	struct event_signal_map *map = &base->sigmap;
	struct evmap_signal *ctx = NULL;

	if (sig >= map->nentries) {
		if (evmap_make_space(
			map, sig, sizeof(struct evmap_signal *)) == -1)
			return (-1);
	}
	GET_SIGNAL_SLOT_AND_CTOR(ctx, map, sig, evmap_signal, evmap_signal_init,
	    base->evsigsel->fdinfo_len);

	if (TAILQ_EMPTY(&ctx->events)) {
		if (evsel->add(base, ev->ev_fd, 0, EV_SIGNAL, NULL)
		    == -1)
			return (-1);
	}

	TAILQ_INSERT_TAIL(&ctx->events, ev, ev_signal_next);

	return (1);
}

int
evmap_signal_del(struct event_base *base, int sig, struct event *ev)
{
	const struct eventop *evsel = base->evsigsel;
	struct event_signal_map *map = &base->sigmap;
	struct evmap_signal *ctx;

	if (sig >= map->nentries)
		return (-1);

	GET_SIGNAL_SLOT(ctx, map, sig, evmap_signal);

	if (TAILQ_FIRST(&ctx->events) == TAILQ_LAST(&ctx->events, event_list)) {
		if (evsel->del(base, ev->ev_fd, 0, EV_SIGNAL, NULL) == -1)
			return (-1);
	}

	TAILQ_REMOVE(&ctx->events, ev, ev_signal_next);

	return (1);
}

void
evmap_signal_active(struct event_base *base, evutil_socket_t sig, int ncalls)
{
	struct event_signal_map *map = &base->sigmap;
	struct evmap_signal *ctx;
	struct event *ev;

	EVUTIL_ASSERT(sig < map->nentries);
	GET_SIGNAL_SLOT(ctx, map, sig, evmap_signal);

	TAILQ_FOREACH(ev, &ctx->events, ev_signal_next)
		event_active_nolock(ev, EV_SIGNAL, ncalls);
}

void *
evmap_io_get_fdinfo(struct event_io_map *map, evutil_socket_t fd)
{
	struct evmap_io *ctx;
	GET_IO_SLOT(ctx, map, fd, evmap_io);
	if (ctx)
		return ((char*)ctx) + sizeof(struct evmap_io);
	else
		return NULL;
}

/** Per-fd structure for use with changelists.  It keeps track, for each fd or
 * signal using the changelist, of where its entry in the changelist is.
 */
struct event_changelist_fdinfo {
	int idxplus1; /* this is the index +1, so that memset(0) will make it
		       * a no-such-element */
};

void
event_changelist_init(struct event_changelist *changelist)
{
	changelist->changes = NULL;
	changelist->changes_size = 0;
	changelist->n_changes = 0;
}

/** Helper: return the changelist_fdinfo corresponding to a given change. */
static inline struct event_changelist_fdinfo *
event_change_get_fdinfo(struct event_base *base,
    const struct event_change *change)
{
	char *ptr;
	if (change->read_change & EV_CHANGE_SIGNAL) {
		struct evmap_signal *ctx;
		GET_SIGNAL_SLOT(ctx, &base->sigmap, change->fd, evmap_signal);
		ptr = ((char*)ctx) + sizeof(struct evmap_signal);
	} else {
		struct evmap_io *ctx;
		GET_IO_SLOT(ctx, &base->io, change->fd, evmap_io);
		ptr = ((char*)ctx) + sizeof(struct evmap_io);
	}
	return (void*)ptr;
}

#ifdef DEBUG_CHANGELIST
/** Make sure that the changelist is consistent with the evmap structures. */
static void
event_changelist_check(struct event_base *base)
{
	int i;
	struct event_changelist *changelist = &base->changelist;

	EVUTIL_ASSERT(changelist->changes_size >= changelist->n_changes);
	for (i = 0; i < changelist->n_changes; ++i) {
		struct event_change *c = &changelist->changes[i];
		struct event_changelist_fdinfo *f;
		EVUTIL_ASSERT(c->fd >= 0);
		f = event_change_get_fdinfo(base, c);
		EVUTIL_ASSERT(f);
		EVUTIL_ASSERT(f->idxplus1 == i + 1);
	}

	for (i = 0; i < base->io.nentries; ++i) {
		struct evmap_io *io = base->io.entries[i];
		struct event_changelist_fdinfo *f;
		if (!io)
			continue;
		f = (void*)
		    ( ((char*)io) + sizeof(struct evmap_io) );
		if (f->idxplus1) {
			struct event_change *c = &changelist->changes[f->idxplus1 - 1];
			EVUTIL_ASSERT(c->fd == i);
		}
	}
}
#else
#define event_changelist_check(base)  ((void)0)
#endif

void
event_changelist_remove_all(struct event_changelist *changelist,
    struct event_base *base)
{
	int i;

	event_changelist_check(base);

	for (i = 0; i < changelist->n_changes; ++i) {
		struct event_change *ch = &changelist->changes[i];
		struct event_changelist_fdinfo *fdinfo =
		    event_change_get_fdinfo(base, ch);
		EVUTIL_ASSERT(fdinfo->idxplus1 == i + 1);
		fdinfo->idxplus1 = 0;
	}

	changelist->n_changes = 0;

	event_changelist_check(base);
}

void
event_changelist_freemem(struct event_changelist *changelist)
{
	if (changelist->changes)
		mm_free(changelist->changes);
	event_changelist_init(changelist); /* zero it all out. */
}

/** Increase the size of 'changelist' to hold more changes. */
static int
event_changelist_grow(struct event_changelist *changelist)
{
	int new_size;
	struct event_change *new_changes;
	if (changelist->changes_size < 64)
		new_size = 64;
	else
		new_size = changelist->changes_size * 2;

	new_changes = mm_realloc(changelist->changes,
	    new_size * sizeof(struct event_change));

	if (EVUTIL_UNLIKELY(new_changes == NULL))
		return (-1);

	changelist->changes = new_changes;
	changelist->changes_size = new_size;

	return (0);
}

/** Return a pointer to the changelist entry for the file descriptor or signal
 * 'fd', whose fdinfo is 'fdinfo'.  If none exists, construct it, setting its
 * old_events field to old_events.
 */
static struct event_change *
event_changelist_get_or_construct(struct event_changelist *changelist,
    evutil_socket_t fd,
    short old_events,
    struct event_changelist_fdinfo *fdinfo)
{
	struct event_change *change;

	if (fdinfo->idxplus1 == 0) {
		int idx;
		EVUTIL_ASSERT(changelist->n_changes <= changelist->changes_size);

		if (changelist->n_changes == changelist->changes_size) {
			if (event_changelist_grow(changelist) < 0)
				return NULL;
		}

		idx = changelist->n_changes++;
		change = &changelist->changes[idx];
		fdinfo->idxplus1 = idx + 1;

		memset(change, 0, sizeof(struct event_change));
		change->fd = fd;
		change->old_events = old_events;
	} else {
		change = &changelist->changes[fdinfo->idxplus1 - 1];
		EVUTIL_ASSERT(change->fd == fd);
	}
	return change;
}

int
event_changelist_add(struct event_base *base, evutil_socket_t fd, short old, short events,
    void *p)
{
	struct event_changelist *changelist = &base->changelist;
	struct event_changelist_fdinfo *fdinfo = p;
	struct event_change *change;

	event_changelist_check(base);

	change = event_changelist_get_or_construct(changelist, fd, old, fdinfo);
	if (!change)
		return -1;

	/* An add replaces any previous delete, but doesn't result in a no-op,
	 * since the delete might fail (because the fd had been closed since
	 * the last add, for instance. */

	if (events & (EV_READ|EV_SIGNAL)) {
		change->read_change = EV_CHANGE_ADD |
		    (events & (EV_ET|EV_PERSIST|EV_SIGNAL));
	}
	if (events & EV_WRITE) {
		change->write_change = EV_CHANGE_ADD |
		    (events & (EV_ET|EV_PERSIST|EV_SIGNAL));
	}

	event_changelist_check(base);
	return (0);
}

int
event_changelist_del(struct event_base *base, evutil_socket_t fd, short old, short events,
    void *p)
{
	struct event_changelist *changelist = &base->changelist;
	struct event_changelist_fdinfo *fdinfo = p;
	struct event_change *change;

	event_changelist_check(base);
	change = event_changelist_get_or_construct(changelist, fd, old, fdinfo);
	event_changelist_check(base);
	if (!change)
		return -1;

	/* A delete removes any previous add, rather than replacing it:
	   on those platforms where "add, delete, dispatch" is not the same
	   as "no-op, dispatch", we want the no-op behavior.

	   As well as checking the current operation we should also check
	   the original set of events to make sure were not ignoring
	   the case where the add operation is present on an event that
	   was already set.

	   If we have a no-op item, we could remove it it from the list
	   entirely, but really there's not much point: skipping the no-op
	   change when we do the dispatch later is far cheaper than rejuggling
	   the array now.

	   As this stands, it also lets through deletions of events that are
	   not currently set.
	 */

	if (events & (EV_READ|EV_SIGNAL)) {
		if (!(change->old_events & (EV_READ | EV_SIGNAL)) &&
		    (change->read_change & EV_CHANGE_ADD))
			change->read_change = 0;
		else
			change->read_change = EV_CHANGE_DEL;
	}
	if (events & EV_WRITE) {
		if (!(change->old_events & EV_WRITE) &&
		    (change->write_change & EV_CHANGE_ADD))
			change->write_change = 0;
		else
			change->write_change = EV_CHANGE_DEL;
	}

	event_changelist_check(base);
	return (0);
}

void
evmap_check_integrity(struct event_base *base)
{
#define EVLIST_X_SIGFOUND 0x1000
#define EVLIST_X_IOFOUND 0x2000

	evutil_socket_t i;
	struct event *ev;
	struct event_io_map *io = &base->io;
	struct event_signal_map *sigmap = &base->sigmap;
#ifdef EVMAP_USE_HT
	struct event_map_entry **mapent;
#endif
	int nsignals, ntimers, nio;
	nsignals = ntimers = nio = 0;

	TAILQ_FOREACH(ev, &base->eventqueue, ev_next) {
		EVUTIL_ASSERT(ev->ev_flags & EVLIST_INSERTED);
		EVUTIL_ASSERT(ev->ev_flags & EVLIST_INIT);
		ev->ev_flags &= ~(EVLIST_X_SIGFOUND|EVLIST_X_IOFOUND);
	}

#ifdef EVMAP_USE_HT
	HT_FOREACH(mapent, event_io_map, io) {
		struct evmap_io *ctx = &(*mapent)->ent.evmap_io;
		i = (*mapent)->fd;
#else
	for (i = 0; i < io->nentries; ++i) {
		struct evmap_io *ctx = io->entries[i];

		if (!ctx)
			continue;
#endif

		TAILQ_FOREACH(ev, &ctx->events, ev_io_next) {
			EVUTIL_ASSERT(!(ev->ev_flags & EVLIST_X_IOFOUND));
			EVUTIL_ASSERT(ev->ev_fd == i);
			ev->ev_flags |= EVLIST_X_IOFOUND;
			nio++;
		}
	}

	for (i = 0; i < sigmap->nentries; ++i) {
		struct evmap_signal *ctx = sigmap->entries[i];
		if (!ctx)
			continue;

		TAILQ_FOREACH(ev, &ctx->events, ev_signal_next) {
			EVUTIL_ASSERT(!(ev->ev_flags & EVLIST_X_SIGFOUND));
			EVUTIL_ASSERT(ev->ev_fd == i);
			ev->ev_flags |= EVLIST_X_SIGFOUND;
			nsignals++;
		}
	}

	TAILQ_FOREACH(ev, &base->eventqueue, ev_next) {
		if (ev->ev_events & (EV_READ|EV_WRITE)) {
			EVUTIL_ASSERT(ev->ev_flags & EVLIST_X_IOFOUND);
			--nio;
		}
		if (ev->ev_events & EV_SIGNAL) {
			EVUTIL_ASSERT(ev->ev_flags & EVLIST_X_SIGFOUND);
			--nsignals;
		}
	}

	EVUTIL_ASSERT(nio == 0);
	EVUTIL_ASSERT(nsignals == 0);
	/* There is no "EVUTIL_ASSERT(ntimers == 0)": eventqueue is only for
	 * pending signals and io events.
	 */
}
