/*
 * Copyright (c) 2000-2007 Niels Provos <provos@citi.umich.edu>
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
#ifndef _EVENT2_EVENT_H_
#define _EVENT2_EVENT_H_

/**
   @mainpage

  @section intro Introduction

  Libevent is an event notification library for developing scalable network
  servers.  The Libevent API provides a mechanism to execute a callback
  function when a specific event occurs on a file descriptor or after a
  timeout has been reached. Furthermore, Libevent also support callbacks due
  to signals or regular timeouts.

  Libevent is meant to replace the event loop found in event driven network
  servers. An application just needs to call event_dispatch() and then add or
  remove events dynamically without having to change the event loop.


  Currently, Libevent supports /dev/poll, kqueue(2), select(2), poll(2),
  epoll(4), and evports. The internal event mechanism is completely
  independent of the exposed event API, and a simple update of Libevent can
  provide new functionality without having to redesign the applications. As a
  result, Libevent allows for portable application development and provides
  the most scalable event notification mechanism available on an operating
  system.  Libevent can also be used for multithreaded programs.  Libevent
  should compile on Linux, *BSD, Mac OS X, Solaris and, Windows.

  @section usage Standard usage

  Every program that uses Libevent must inclurde the <event2/event.h>
  header, and pass the -levent flag to the linker.  (You can instead link
  -levent_core if you only want the main event and buffered IO-based code,
  and don't want to link any protocol code.)

  @section setup Library setup

  Before you call any other Libevent functions, you need to set up the
  library.  If you're going to use Libevent from multiple threads in a
  multithreaded application, you need to initialize thread support --
  typically by using evthread_use_pthreads() or
  evthread_use_windows_threads().  See <event2/thread.h> for more
  information.

  This is also the point where you can replace Libevent's memory
  management functions with event_set_mem_functions, and enable debug mode
  with event_enable_debug_mode().

  @section base Creating an event base

  Next, you need to create an event_base structure, using event_base_new()
  or event_base_new_with_config().  The event_base is responsible for
  keeping track of which events are "pending" (that is to say, being
  watched to see if they become active) and which events are "active".
  Every event is associated with a single event_base.

  @section event Event notification

  For each file descriptor that you wish to monitor, you must create an
  event structure with event_new().  (You may also declare an event
  structure and call event_assign() to initialize the members of the
  structure.)  To enable notification, you add the structure to the list
  of monitored events by calling event_add().  The event structure must
  remain allocated as long as it is active, so it should generally be
  allocated on the heap.

  @section loop Dispaching evets.

  Finally, you call event_base_dispatch() to loop and dispatch events.
  You can also use event_base_loop() for more fine-grained control.

  Currently, only one thread can be dispatching a given event_base at a
  time.  If you want to run events in multiple threads at once, you can
  either have a single event_base whose events add work to a work queue,
  or you can create multiple event_base objects.

  @section bufferevent I/O Buffers

  Libevent provides a buffered I/O abstraction on top of the regular event
  callbacks. This abstraction is called a bufferevent. A bufferevent
  provides input and output buffers that get filled and drained
  automatically. The user of a buffered event no longer deals directly
  with the I/O, but instead is reading from input and writing to output
  buffers.

  Once initialized via bufferevent_socket_new(), the bufferevent structure
  can be used repeatedly with bufferevent_enable() and
  bufferevent_disable().  Instead of reading and writing directly to a
  socket, you would call bufferevent_read() and bufferevent_write().

  When read enabled the bufferevent will try to read from the file descriptor
  and call the read callback. The write callback is executed whenever the
  output buffer is drained below the write low watermark, which is 0 by
  default.

  See <event2/bufferevent*.h> for more information.

  @section timers Timers

  Libevent can also be used to create timers that invoke a callback after a
  certain amount of time has expired. The evtimer_new() function returns
  an event struct to use as a timer. To activate the timer, call
  evtimer_add(). Timers can be deactivated by calling evtimer_del().

  @section evdns Asynchronous DNS resolution

  Libevent provides an asynchronous DNS resolver that should be used instead
  of the standard DNS resolver functions.  See the <event2/dns.h>
  functions for more detail.

  @section evhttp Event-driven HTTP servers

  Libevent provides a very simple event-driven HTTP server that can be
  embedded in your program and used to service HTTP requests.

  To use this capability, you need to include the <event2/http.h> header in your
  program.  See that header for more information.

  @section evrpc A framework for RPC servers and clients

  Libevent provides a framework for creating RPC servers and clients.  It
  takes care of marshaling and unmarshaling all data structures.

  @section api API Reference

  To browse the complete documentation of the libevent API, click on any of
  the following links.

  event2/event.h
  The primary libevent header

  event2/thread.h
  Functions for use by multithreaded programs

  event2/buffer.h and event2/bufferevent.h
  Buffer management for network reading and writing

  event2/util.h
  Utility functions for portable nonblocking network code

  event2/dns.h
  Asynchronous DNS resolution

  event2/http.h
  An embedded libevent-based HTTP server

  event2/rpc.h
  A framework for creating RPC servers and clients

 */

/** @file event2/event.h

  Core functions for waiting for and receiving events, and using event bases.
*/

#ifdef __cplusplus
extern "C" {
#endif

#include <event2/event-config.h>
#ifdef _EVENT_HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef _EVENT_HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include <stdio.h>

/* For int types. */
#include <event2/util.h>

/**
 * Structure to hold information and state for a Libevent dispatch loop.
 *
 * The event_base lies at the center of Libevent; every application will
 * have one.  It keeps track of all pending and active events, and
 * notifies your application of the active ones.
 *
 * This is an opaque structure; you can allocate one using
 * event_base_new() or event_base_new_with_config().
 *
 * @see event_base_new(), event_base_free(), event_base_loop(),
 *    event_base_new_with_config()
 */
struct event_base
#ifdef _EVENT_IN_DOXYGEN
{/*Empty body so that doxygen will generate documentation here.*/}
#endif
;

/**
 * @struct event
 *
 * Structure to represent a single event.
 *
 * An event can have some underlying condition it represents: a socket
 * becoming readable or writeable (or both), or a signal becoming raised.
 * (An event that represents no underlying condition is still useful: you
 * can use one to implement a timer, or to communicate between threads.)
 *
 * Generally, you can create events with event_new(), then make them
 * pending with event_add().  As your event_base runs, it will run the
 * callbacks of an events whose conditions are triggered.  When you
 * longer want the event, free it with event_free().
 *
 * In more depth:
 *
 * An event may be "pending" (one whose condition we are watching),
 * "active" (one whose condition has triggered and whose callback is about
 * to run), neither, or both.  Events come into existence via
 * event_assign() or event_new(), and are then neither active nor pending.
 *
 * To make an event pending, pass it to event_add().  When doing so, you
 * can also set a timeout for the event.
 *
 * Events become active during an event_base_loop() call when either their
 * condition has triggered, or when their timeout has elapsed.  You can
 * also activate an event manually using event_active().  The even_base
 * loop will run the callbacks of active events; after it has done so, it
 * marks them as no longer active.
 *
 * You can make an event non-pending by passing it to event_del().  This
 * also makes the event non-active.
 *
 * Events can be "persistent" or "non-persistent".  A non-persistent event
 * becomes non-pending as soon as it is triggered: thus, it only runs at
 * most once per call to event_add().  A persistent event remains pending
 * even when it becomes active: you'll need to event_del() it manually in
 * order to make it non-pending.  When a persistent event with a timeout
 * becomes active, its timeout is reset: this means you can use persistent
 * events to implement periodic timeouts.
 *
 * This should be treated as an opaque structure; you should never read or
 * write any of its fields directly.  For backward compatibility with old
 * code, it is defined in the event2/event_struct.h header; including this
 * header may make your code incompatible with other versions of Libevent.
 *
 * @see event_new(), event_free(), event_assign(), event_get_assignment(),
 *    event_add(), event_del(), event_active(), event_pending(),
 *    event_get_fd(), event_get_base(), event_get_events(),
 *    event_get_callback(), event_get_callback_arg(),
 *    event_priority_set()
 */
struct event
#ifdef _EVENT_IN_DOXYGEN
{/*Empty body so that doxygen will generate documentation here.*/}
#endif
;

/**
 * Configuration for an event_base.
 *
 * There are many options that can be used to alter the behavior and
 * implementation of an event_base.  To avoid having to pass them all in a
 * complex many-argument constructor, we provide an abstract data type
 * wrhere you set up configation information before passing it to
 * event_base_new_with_config().
 *
 * @see event_config_new(), event_config_free(), event_base_new_with_config(),
 *   event_config_avoid_method(), event_config_require_features(),
 *   event_config_set_flag(), event_config_set_num_cpus_hint()
 */
struct event_config
#ifdef _EVENT_IN_DOXYGEN
{/*Empty body so that doxygen will generate documentation here.*/}
#endif
;

/**
 * Enable some relatively expensive debugging checks in Libevent that
 * would normally be turned off.  Generally, these checks cause code that
 * would otherwise crash mysteriously to fail earlier with an assertion
 * failure.  Note that this method MUST be called before any events or
 * event_bases have been created.
 *
 * Debug mode can currently catch the following errors:
 *    An event is re-assigned while it is added
 *    Any function is called on a non-assigned event
 *
 * Note that debugging mode uses memory to track every event that has been
 * initialized (via event_assign, event_set, or event_new) but not yet
 * released (via event_free or event_debug_unassign).  If you want to use
 * debug mode, and you find yourself running out of memory, you will need
 * to use event_debug_unassign to explicitly stop tracking events that
 * are no longer considered set-up.
 *
 * @see event_debug_unassign()
 */
void event_enable_debug_mode(void);

/**
 * When debugging mode is enabled, informs Libevent that an event should no
 * longer be considered as assigned. When debugging mode is not enabled, does
 * nothing.
 *
 * This function must only be called on a non-added event.
 *
 * @see event_enable_debug_mode()
 */
void event_debug_unassign(struct event *);

/**
 * Create and return a new event_base to use with the rest of Libevent.
 *
 * @return a new event_base on success, or NULL on failure.
 *
 * @see event_base_free(), event_base_new_with_config()
 */
struct event_base *event_base_new(void);

/**
  Reinitialize the event base after a fork

  Some event mechanisms do not survive across fork.   The event base needs
  to be reinitialized with the event_reinit() function.

  @param base the event base that needs to be re-initialized
  @return 0 if successful, or -1 if some events could not be re-added.
  @see event_base_new()
*/
int event_reinit(struct event_base *base);

/**
   Event dispatching loop

  This loop will run the event base until either there are no more added
  events, or until something calls event_base_loopbreak() or
  event_base_loopexit().

  @param base the event_base structure returned by event_base_new() or
     event_base_new_with_config()
  @return 0 if successful, -1 if an error occurred, or 1 if no events were
    registered.
  @see event_base_loop()
 */
int event_base_dispatch(struct event_base *);

/**
 Get the kernel event notification mechanism used by Libevent.

 @param eb the event_base structure returned by event_base_new()
 @return a string identifying the kernel event mechanism (kqueue, epoll, etc.)
 */
const char *event_base_get_method(const struct event_base *);

/**
   Gets all event notification mechanisms supported by Libevent.

   This functions returns the event mechanism in order preferred by
   Libevent.  Note that this list will include all backends that
   Libevent has compiled-in support for, and will not necessarily check
   your OS to see whether it has the required resources.

   @return an array with pointers to the names of support methods.
     The end of the array is indicated by a NULL pointer.  If an
     error is encountered NULL is returned.
*/
const char **event_get_supported_methods(void);

/**
   Allocates a new event configuration object.

   The event configuration object can be used to change the behavior of
   an event base.

   @return an event_config object that can be used to store configuration, or
     NULL if an error is encountered.
   @see event_base_new_with_config(), event_config_free(), event_config
*/
struct event_config *event_config_new(void);

/**
   Deallocates all memory associated with an event configuration object

   @param cfg the event configuration object to be freed.
*/
void event_config_free(struct event_config *cfg);

/**
   Enters an event method that should be avoided into the configuration.

   This can be used to avoid event mechanisms that do not support certain
   file descriptor types, or for debugging to avoid certain event
   mechanisms.  An application can make use of multiple event bases to
   accommodate incompatible file descriptor types.

   @param cfg the event configuration object
   @param method the name of the event method to avoid
   @return 0 on success, -1 on failure.
*/
int event_config_avoid_method(struct event_config *cfg, const char *method);

/**
   A flag used to describe which features an event_base (must) provide.

   Because of OS limitations, not every Libevent backend supports every
   possible feature.  You can use this type with
   event_config_require_features() to tell Libevent to only proceed if your
   event_base implements a given feature, and you can receive this type from
   event_base_get_features() to see which features are available.
*/
enum event_method_feature {
    /** Require an event method that allows edge-triggered events with EV_ET. */
    EV_FEATURE_ET = 0x01,
    /** Require an event method where having one event triggered among
     * many is [approximately] an O(1) operation. This excludes (for
     * example) select and poll, which are approximately O(N) for N
     * equal to the total number of possible events. */
    EV_FEATURE_O1 = 0x02,
    /** Require an event method that allows file descriptors as well as
     * sockets. */
    EV_FEATURE_FDS = 0x04
};

/**
   A flag passed to event_config_set_flag().

    These flags change the behavior of an allocated event_base.

    @see event_config_set_flag(), event_base_new_with_config(),
       event_method_feature
 */
enum event_base_config_flag {
	/** Do not allocate a lock for the event base, even if we have
	    locking set up. */
	EVENT_BASE_FLAG_NOLOCK = 0x01,
	/** Do not check the EVENT_* environment variables when configuring
	    an event_base  */
	EVENT_BASE_FLAG_IGNORE_ENV = 0x02,
	/** Windows only: enable the IOCP dispatcher at startup

	    If this flag is set then bufferevent_socket_new() and
	    evconn_listener_new() will use IOCP-backed implementations
	    instead of the usual select-based one on Windows.
	 */
	EVENT_BASE_FLAG_STARTUP_IOCP = 0x04,
	/** Instead of checking the current time every time the event loop is
	    ready to run timeout callbacks, check after each timeout callback.
	 */
	EVENT_BASE_FLAG_NO_CACHE_TIME = 0x08,

	/** If we are using the epoll backend, this flag says that it is
	    safe to use Libevent's internal change-list code to batch up
	    adds and deletes in order to try to do as few syscalls as
	    possible.  Setting this flag can make your code run faster, but
	    it may trigger a Linux bug: it is not safe to use this flag
	    if you have any fds cloned by dup() or its variants.  Doing so
	    will produce strange and hard-to-diagnose bugs.

	    This flag can also be activated by settnig the
	    EVENT_EPOLL_USE_CHANGELIST environment variable.

	    This flag has no effect if you wind up using a backend other than
	    epoll.
	 */
	EVENT_BASE_FLAG_EPOLL_USE_CHANGELIST = 0x10
};

/**
   Return a bitmask of the features implemented by an event base.  This
   will be a bitwise OR of one or more of the values of
   event_method_feature

   @see event_method_feature
 */
int event_base_get_features(const struct event_base *base);

/**
   Enters a required event method feature that the application demands.

   Note that not every feature or combination of features is supported
   on every platform.  Code that requests features should be prepared
   to handle the case where event_base_new_with_config() returns NULL, as in:
   <pre>
     event_config_require_features(cfg, EV_FEATURE_ET);
     base = event_base_new_with_config(cfg);
     if (base == NULL) {
       // We can't get edge-triggered behavior here.
       event_config_require_features(cfg, 0);
       base = event_base_new_with_config(cfg);
     }
   </pre>

   @param cfg the event configuration object
   @param feature a bitfield of one or more event_method_feature values.
          Replaces values from previous calls to this function.
   @return 0 on success, -1 on failure.
   @see event_method_feature, event_base_new_with_config()
*/
int event_config_require_features(struct event_config *cfg, int feature);

/**
 * Sets one or more flags to configure what parts of the eventual event_base
 * will be initialized, and how they'll work.
 *
 * @see event_base_config_flags, event_base_new_with_config()
 **/
int event_config_set_flag(struct event_config *cfg, int flag);

/**
 * Records a hint for the number of CPUs in the system. This is used for
 * tuning thread pools, etc, for optimal performance.  In Libevent 2.0,
 * it is only on Windows, and only when IOCP is in use.
 *
 * @param cfg the event configuration object
 * @param cpus the number of cpus
 * @return 0 on success, -1 on failure.
 */
int event_config_set_num_cpus_hint(struct event_config *cfg, int cpus);

/**
  Initialize the event API.

  Use event_base_new_with_config() to initialize a new event base, taking
  the specified configuration under consideration.  The configuration object
  can currently be used to avoid certain event notification mechanisms.

  @param cfg the event configuration object
  @return an initialized event_base that can be used to registering events,
     or NULL if no event base can be created with the requested event_config.
  @see event_base_new(), event_base_free(), event_init(), event_assign()
*/
struct event_base *event_base_new_with_config(const struct event_config *);

/**
  Deallocate all memory associated with an event_base, and free the base.

  Note that this function will not close any fds or free any memory passed
  to event_new as the argument to callback.

  @param eb an event_base to be freed
 */
void event_base_free(struct event_base *);

/** @name Log severities
 */
/**@{*/
#define EVENT_LOG_DEBUG 0
#define EVENT_LOG_MSG   1
#define EVENT_LOG_WARN  2
#define EVENT_LOG_ERR   3
/**@}*/

/* Obsolete names: these are deprecated, but older programs might use them.
 * They violate the reserved-identifier namespace. */
#define _EVENT_LOG_DEBUG EVENT_LOG_DEBUG
#define _EVENT_LOG_MSG EVENT_LOG_MSG
#define _EVENT_LOG_WARN EVENT_LOG_WARN
#define _EVENT_LOG_ERR EVENT_LOG_ERR

/**
  A callback function used to intercept Libevent's log messages.

  @see event_set_log_callback
 */
typedef void (*event_log_cb)(int severity, const char *msg);
/**
  Redirect Libevent's log messages.

  @param cb a function taking two arguments: an integer severity between
     _EVENT_LOG_DEBUG and _EVENT_LOG_ERR, and a string.  If cb is NULL,
	 then the default log is used.

  NOTE: The function you provide *must not* call any other libevent
  functionality.  Doing so can produce undefined behavior.
  */
void event_set_log_callback(event_log_cb cb);

/**
   A function to be called if Libevent encounters a fatal internal error.

   @see event_set_fatal_callback
 */
typedef void (*event_fatal_cb)(int err);

/**
 Override Libevent's behavior in the event of a fatal internal error.

 By default, Libevent will call exit(1) if a programming error makes it
 impossible to continue correct operation.  This function allows you to supply
 another callback instead.  Note that if the function is ever invoked,
 something is wrong with your program, or with Libevent: any subsequent calls
 to Libevent may result in undefined behavior.

 Libevent will (almost) always log an _EVENT_LOG_ERR message before calling
 this function; look at the last log message to see why Libevent has died.
 */
void event_set_fatal_callback(event_fatal_cb cb);

/**
  Associate a different event base with an event.

  The event to be associated must not be currently active or pending.

  @param eb the event base
  @param ev the event
  @return 0 on success, -1 on failure.
 */
int event_base_set(struct event_base *, struct event *);

/** @name Loop flags

    These flags control the behavior of event_base_loop().
 */
/**@{*/
/** Block until we have an active event, then exit once all active events
 * have had their callbacks run. */
#define EVLOOP_ONCE	0x01
/** Do not block: see which events are ready now, run the callbacks
 * of the highest-priority ones, then exit. */
#define EVLOOP_NONBLOCK	0x02
/**@}*/

/**
  Wait for events to become active, and run their callbacks.

  This is a more flexible version of event_base_dispatch().

  By default, this loop will run the event base until either there are no more
  added events, or until something calls event_base_loopbreak() or
  evenet_base_loopexit().  You can override this behavior with the 'flags'
  argument.

  @param eb the event_base structure returned by event_base_new() or
     event_base_new_with_config()
  @param flags any combination of EVLOOP_ONCE | EVLOOP_NONBLOCK
  @return 0 if successful, -1 if an error occurred, or 1 if no events were
    registered.
  @see event_base_loopexit(), event_base_dispatch(), EVLOOP_ONCE,
     EVLOOP_NONBLOCK
  */
int event_base_loop(struct event_base *, int);

/**
  Exit the event loop after the specified time

  The next event_base_loop() iteration after the given timer expires will
  complete normally (handling all queued events) then exit without
  blocking for events again.

  Subsequent invocations of event_base_loop() will proceed normally.

  @param eb the event_base structure returned by event_init()
  @param tv the amount of time after which the loop should terminate,
    or NULL to exit after running all currently active events.
  @return 0 if successful, or -1 if an error occurred
  @see event_base_loopbreak()
 */
int event_base_loopexit(struct event_base *, const struct timeval *);

/**
  Abort the active event_base_loop() immediately.

  event_base_loop() will abort the loop after the next event is completed;
  event_base_loopbreak() is typically invoked from this event's callback.
  This behavior is analogous to the "break;" statement.

  Subsequent invocations of event_loop() will proceed normally.

  @param eb the event_base structure returned by event_init()
  @return 0 if successful, or -1 if an error occurred
  @see event_base_loopexit()
 */
int event_base_loopbreak(struct event_base *);

/**
  Checks if the event loop was told to exit by event_loopexit().

  This function will return true for an event_base at every point after
  event_loopexit() is called, until the event loop is next entered.

  @param eb the event_base structure returned by event_init()
  @return true if event_base_loopexit() was called on this event base,
    or 0 otherwise
  @see event_base_loopexit()
  @see event_base_got_break()
 */
int event_base_got_exit(struct event_base *);

/**
  Checks if the event loop was told to abort immediately by event_loopbreak().

  This function will return true for an event_base at every point after
  event_loopbreak() is called, until the event loop is next entered.

  @param eb the event_base structure returned by event_init()
  @return true if event_base_loopbreak() was called on this event base,
    or 0 otherwise
  @see event_base_loopbreak()
  @see event_base_got_exit()
 */
int event_base_got_break(struct event_base *);

/**
 * @name event flags
 *
 * Flags to pass to event_new(), event_assign(), event_pending(), and
 * anything else with an argument of the form "short events"
 */
/**@{*/
/** Indicates that a timeout has occurred.  It's not necessary to pass
 * this flag to event_for new()/event_assign() to get a timeout. */
#define EV_TIMEOUT	0x01
/** Wait for a socket or FD to become readable */
#define EV_READ		0x02
/** Wait for a socket or FD to become writeable */
#define EV_WRITE	0x04
/** Wait for a POSIX signal to be raised*/
#define EV_SIGNAL	0x08
/**
 * Persistent event: won't get removed automatically when activated.
 *
 * When a persistent event with a timeout becomes activated, its timeout
 * is reset to 0.
 */
#define EV_PERSIST	0x10
/** Select edge-triggered behavior, if supported by the backend. */
#define EV_ET       0x20
/**@}*/

/**
   @name evtimer_* macros

    Aliases for working with one-shot timer events */
/**@{*/
#define evtimer_assign(ev, b, cb, arg) \
	event_assign((ev), (b), -1, 0, (cb), (arg))
#define evtimer_new(b, cb, arg)	       event_new((b), -1, 0, (cb), (arg))
#define evtimer_add(ev, tv)		event_add((ev), (tv))
#define evtimer_del(ev)			event_del(ev)
#define evtimer_pending(ev, tv)		event_pending((ev), EV_TIMEOUT, (tv))
#define evtimer_initialized(ev)		event_initialized(ev)
/**@}*/

/**
   @name evsignal_* macros

   Aliases for working with signal events
 */
/**@{*/
#define evsignal_add(ev, tv)		event_add((ev), (tv))
#define evsignal_assign(ev, b, x, cb, arg)			\
	event_assign((ev), (b), (x), EV_SIGNAL|EV_PERSIST, cb, (arg))
#define evsignal_new(b, x, cb, arg)				\
	event_new((b), (x), EV_SIGNAL|EV_PERSIST, (cb), (arg))
#define evsignal_del(ev)		event_del(ev)
#define evsignal_pending(ev, tv)	event_pending((ev), EV_SIGNAL, (tv))
#define evsignal_initialized(ev)	event_initialized(ev)
/**@}*/

/**
   A callback function for an event.

   It receives three arguments:

   @param fd An fd or signal
   @param events One or more EV_* flags
   @param arg A user-supplied argument.

   @see event_new()
 */
typedef void (*event_callback_fn)(evutil_socket_t, short, void *);

/**
  Allocate and asssign a new event structure, ready to be added.

  The function event_new() returns a new event that can be used in
  future calls to event_add() and event_del().  The fd and events
  arguments determine which conditions will trigger the event; the
  callback and callback_arg arguments tell Libevent what to do when the
  event becomes active.

  If events contains one of EV_READ, EV_WRITE, or EV_READ|EV_WRITE, then
  fd is a file descriptor or socket that should get monitored for
  readiness to read, readiness to write, or readiness for either operation
  (respectively).  If events contains EV_SIGNAL, then fd is a signal
  number to wait for.  If events contains none of those flags, then the
  event can be triggered only by a timeout or by manual activation with
  event_active(): In this case, fd must be -1.

  The EV_PERSIST flag can also be passed in the events argument: it makes
  event_add() persistent until event_del() is called.

  The EV_ET flag is compatible with EV_READ and EV_WRITE, and supported
  only by certain backends.  It tells Libevent to use edge-triggered
  events.

  The EV_TIMEOUT flag has no effect here.

  It is okay to have multiple events all listening on the same fds; but
  they must either all be edge-triggered, or all not be edge triggerd.

  When the event becomes active, the event loop will run the provided
  callbuck function, with three arguments.  The first will be the provided
  fd value.  The second will be a bitfield of the events that triggered:
  EV_READ, EV_WRITE, or EV_SIGNAL.  Here the EV_TIMEOUT flag indicates
  that a timeout occurred, and EV_ET indicates that an edge-triggered
  event occurred.  The third event will be the callback_arg pointer that
  you provide.

  @param base the event base to which the event should be attached.
  @param fd the file descriptor or signal to be monitored, or -1.
  @param events desired events to monitor: bitfield of EV_READ, EV_WRITE,
      EV_SIGNAL, EV_PERSIST, EV_ET.
  @param callback callback function to be invoked when the event occurs
  @param callback_arg an argument to be passed to the callback function

  @return a newly allocated struct event that must later be freed with
    event_free().
  @see event_free(), event_add(), event_del(), event_assign()
 */
struct event *event_new(struct event_base *, evutil_socket_t, short, event_callback_fn, void *);


/**
  Prepare a new, already-allocated event structure to be added.

  The function event_assign() prepares the event structure ev to be used
  in future calls to event_add() and event_del().  Unlike event_new(), it
  doesn't allocate memory itself: it requires that you have already
  allocated a struct event, probably on the heap.  Doing this will
  typically make your code depend on the size of the event structure, and
  thereby create incompatibility with future versions of Libevent.

  The easiest way to avoid this problem is just to use event_new() and
  event_free() instead.

  A slightly harder way to future-proof your code is to use
  event_get_struct_event_size() to determine the required size of an event
  at runtime.

  Note that it is NOT safe to call this function on an event that is
  active or pending.  Doing so WILL corrupt internal data structures in
  Libevent, and lead to strange, hard-to-diagnose bugs.  You _can_ use
  event_assign to change an existing event, but only if it is not active
  or pending!

  The arguments for this function, and the behavior of the events that it
  makes, are as for event_new().

  @param ev an event struct to be modified
  @param base the event base to which ev should be attached.
  @param fd the file descriptor to be monitored
  @param events desired events to monitor; can be EV_READ and/or EV_WRITE
  @param callback callback function to be invoked when the event occurs
  @param callback_arg an argument to be passed to the callback function

  @return 0 if success, or -1 on invalid arguments.

  @see event_new(), event_add(), event_del(), event_base_once(),
    event_get_struct_event_size()
  */
int event_assign(struct event *, struct event_base *, evutil_socket_t, short, event_callback_fn, void *);

/**
   Deallocate a struct event * returned by event_new().

   If the event is pending or active, first make it non-pending and
   non-active.
 */
void event_free(struct event *);

/**
  Schedule a one-time event

  The function event_base_once() is similar to event_set().  However, it
  schedules a callback to be called exactly once, and does not require the
  caller to prepare an event structure.

  Note that in Libevent 2.0 and earlier, if the event is never triggered,
  the internal memory used to hold it will never be freed.  This may be
  fixed in a later version of Libevent.

  @param base an event_base
  @param fd a file descriptor to monitor, or -1 for no fd.
  @param events event(s) to monitor; can be any of EV_READ |
         EV_WRITE, or EV_TIMEOUT
  @param callback callback function to be invoked when the event occurs
  @param arg an argument to be passed to the callback function
  @param timeout the maximum amount of time to wait for the event. NULL
         makes an EV_READ/EV_WRITE event make forever; NULL makes an
        EV_TIMEOUT event succees immediately.
  @return 0 if successful, or -1 if an error occurred
 */
int event_base_once(struct event_base *, evutil_socket_t, short, event_callback_fn, void *, const struct timeval *);

/**
  Add an event to the set of pending events.

  The function event_add() schedules the execution of the ev event when the
  event specified in event_assign()/event_new() occurs, or when the time
  specified in timeout has elapesed.  If atimeout is NULL, no timeout
  occurs and the function will only be
  called if a matching event occurs.  The event in the
  ev argument must be already initialized by event_assign() or event_new()
  and may not be used
  in calls to event_assign() until it is no longer pending.

  If the event in the ev argument already has a scheduled timeout, calling
  event_add() replaces the old timeout with the new one, or clears the old
  timeout if the timeout argument is NULL.

  @param ev an event struct initialized via event_set()
  @param timeout the maximum amount of time to wait for the event, or NULL
         to wait forever
  @return 0 if successful, or -1 if an error occurred
  @see event_del(), event_assign(), event_new()
  */
int event_add(struct event *ev, const struct timeval *timeout);

/**
  Remove an event from the set of monitored events.

  The function event_del() will cancel the event in the argument ev.  If the
  event has already executed or has never been added the call will have no
  effect.

  @param ev an event struct to be removed from the working set
  @return 0 if successful, or -1 if an error occurred
  @see event_add()
 */
int event_del(struct event *);


/**
  Make an event active.

  You can use this function on a pending or a non-pending event to make it
  active, so that its callback will be run by event_base_dispatch() or
  event_base_loop().

  One common use in multithreaded programs is to wake the thread running
  event_base_loop() from another thread.

  @param ev an event to make active.
  @param res a set of flags to pass to the event's callback.
  @param ncalls an obsolete argument: this is ignored.
 **/
void event_active(struct event *ev, int res, short ncalls);

/**
  Checks if a specific event is pending or scheduled.

  @param ev an event struct previously passed to event_add()
  @param events the requested event type; any of EV_TIMEOUT|EV_READ|
         EV_WRITE|EV_SIGNAL
  @param tv if this field is not NULL, and the event has a timeout,
         this field is set to hold the time at which the timeout will
	 expire.

  @return true if the event is pending on any of the events in 'what', (that
  is to say, it has been added), or 0 if the event is not added.
 */
int event_pending(const struct event *ev, short events, struct timeval *tv);


/**
  Test if an event structure might be initialized.

  The event_initialized() function can be used to check if an event has been
  initialized.

  Warning: This function is only useful for distinguishing a a zeroed-out
    piece of memory from an initialized event, it can easily be confused by
    uninitialized memory.  Thus, it should ONLY be used to distinguish an
    initialized event from zero.

  @param ev an event structure to be tested
  @return 1 if the structure might be initialized, or 0 if it has not been
          initialized
 */
int event_initialized(const struct event *ev);

/**
   Get the signal number assigned to a signal event
*/
#define event_get_signal(ev) ((int)event_get_fd(ev))

/**
   Get the socket or signal assigned to an event, or -1 if the event has
   no socket.
*/
evutil_socket_t event_get_fd(const struct event *ev);

/**
   Get the event_base associated with an event.
*/
struct event_base *event_get_base(const struct event *ev);

/**
   Return the events (EV_READ, EV_WRITE, etc) assigned to an event.
*/
short event_get_events(const struct event *ev);

/**
   Return the callback assigned to an event.
*/
event_callback_fn event_get_callback(const struct event *ev);

/**
   Return the callback argument assigned to an event.
*/
void *event_get_callback_arg(const struct event *ev);

/**
   Extract _all_ of arguments given to construct a given event.  The
   event_base is copied into *base_out, the fd is copied into *fd_out, and so
   on.

   If any of the "_out" arguments is NULL, it will be ignored.
 */
void event_get_assignment(const struct event *event,
    struct event_base **base_out, evutil_socket_t *fd_out, short *events_out,
    event_callback_fn *callback_out, void **arg_out);

/**
   Return the size of struct event that the Libevent library was compiled
   with.

   This will be NO GREATER than sizeof(struct event) if you're running with
   the same version of Libevent that your application was built with, but
   otherwise might not.

   Note that it might be SMALLER than sizeof(struct event) if some future
   version of Libevent adds extra padding to the end of struct event.
   We might do this to help ensure ABI-compatibility between different
   versions of Libevent.
 */
size_t event_get_struct_event_size(void);

/**
   Get the Libevent version.

   Note that this will give you the version of the library that you're
   currently linked against, not the version of the headers that you've
   compiled against.

   @return a string containing the version number of Libevent
*/
const char *event_get_version(void);

/**
   Return a numeric representation of Libevent's version.

   Note that this will give you the version of the library that you're
   currently linked against, not the version of the headers you've used to
   compile.

   The format uses one byte each for the major, minor, and patchlevel parts of
   the version number.  The low-order byte is unused.  For example, version
   2.0.1-alpha has a numeric representation of 0x02000100
*/
ev_uint32_t event_get_version_number(void);

/** As event_get_version, but gives the version of Libevent's headers. */
#define LIBEVENT_VERSION _EVENT_VERSION
/** As event_get_version_number, but gives the version number of Libevent's
 * headers. */
#define LIBEVENT_VERSION_NUMBER _EVENT_NUMERIC_VERSION

/** Largest number of priorities that Libevent can support. */
#define EVENT_MAX_PRIORITIES 256
/**
  Set the number of different event priorities

  By default Libevent schedules all active events with the same priority.
  However, some time it is desirable to process some events with a higher
  priority than others.  For that reason, Libevent supports strict priority
  queues.  Active events with a lower priority are always processed before
  events with a higher priority.

  The number of different priorities can be set initially with the
  event_base_priority_init() function.  This function should be called
  before the first call to event_base_dispatch().  The
  event_priority_set() function can be used to assign a priority to an
  event.  By default, Libevent assigns the middle priority to all events
  unless their priority is explicitly set.

  Note that urgent-priority events can starve less-urgent events: after
  running all urgent-priority callbacks, Libevent checks for more urgent
  events again, before running less-urgent events.  Less-urgent events
  will not have their callbacks run until there are no events more urgent
  than them that want to be active.

  @param eb the event_base structure returned by event_base_new()
  @param npriorities the maximum number of priorities
  @return 0 if successful, or -1 if an error occurred
  @see event_priority_set()
 */
int	event_base_priority_init(struct event_base *, int);

/**
  Assign a priority to an event.

  @param ev an event struct
  @param priority the new priority to be assigned
  @return 0 if successful, or -1 if an error occurred
  @see event_priority_init()
  */
int	event_priority_set(struct event *, int);

/**
   Prepare an event_base to use a large number of timeouts with the same
   duration.

   Libevent's default scheduling algorithm is optimized for having a large
   number of timeouts with their durations more or less randomly
   distributed.  But if you have a large number of timeouts that all have
   the same duration (for example, if you have a large number of
   connections that all have a 10-second timeout), then you can improve
   Libevent's performance by telling Libevent about it.

   To do this, call this function with the common duration.  It will return a
   pointer to a different, opaque timeout value.  (Don't depend on its actual
   contents!)  When you use this timeout value in event_add(), Libevent will
   schedule the event more efficiently.

   (This optimization probably will not be worthwhile until you have thousands
   or tens of thousands of events with the same timeout.)
 */
const struct timeval *event_base_init_common_timeout(struct event_base *base,
    const struct timeval *duration);

#if !defined(_EVENT_DISABLE_MM_REPLACEMENT) || defined(_EVENT_IN_DOXYGEN)
/**
 Override the functions that Libevent uses for memory management.

 Usually, Libevent uses the standard libc functions malloc, realloc, and
 free to allocate memory.  Passing replacements for those functions to
 event_set_mem_functions() overrides this behavior.

 Note that all memory returned from Libevent will be allocated by the
 replacement functions rather than by malloc() and realloc().  Thus, if you
 have replaced those functions, it will not be appropriate to free() memory
 that you get from Libevent.  Instead, you must use the free_fn replacement
 that you provided.

 Note also that if you are going to call this function, you should do so
 before any call to any Libevent function that does allocation.
 Otherwise, those funtions will allocate their memory using malloc(), but
 then later free it using your provided free_fn.

 @param malloc_fn A replacement for malloc.
 @param realloc_fn A replacement for realloc
 @param free_fn A replacement for free.
 **/
void event_set_mem_functions(
	void *(*malloc_fn)(size_t sz),
	void *(*realloc_fn)(void *ptr, size_t sz),
	void (*free_fn)(void *ptr));
/** This definition is present if Libevent was built with support for
    event_set_mem_functions() */
#define EVENT_SET_MEM_FUNCTIONS_IMPLEMENTED
#endif

void event_base_dump_events(struct event_base *, FILE *);

/** Sets 'tv' to the current time (as returned by gettimeofday()),
    looking at the cached value in 'base' if possible, and calling
    gettimeofday() or clock_gettime() as appropriate if there is no
    cached time.

    Generally, this value will only be cached while actually
    processing event callbacks, and may be very inaccuate if your
    callbacks take a long time to execute.

    Returns 0 on success, negative on failure.
 */
int event_base_gettimeofday_cached(struct event_base *base,
    struct timeval *tv);

#ifdef __cplusplus
}
#endif

#endif /* _EVENT2_EVENT_H_ */
