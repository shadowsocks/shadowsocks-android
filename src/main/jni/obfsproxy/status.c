/* Copyright 2012 The Tor Project
   See LICENSE for other credits and copying information
*/

/**
 * \file status.c
 * \headerfile status.h
 * \brief Keeps status information and logs heartbeat messages.
 **/

#include "util.h"

#include "status.h"
#include "container.h"

#include <string.h>
#include <stdio.h>
#include <time.h>

#include <event2/event.h>
#include <event2/listener.h>

/** Count total connections. */
static unsigned int connections = 0;

/** Count connections from unique addresses. */
static strmap_t *addresses = NULL;

/** Clear connection counters. If <b>reinit</b> is false, don't
    reinitialize the info-keeping structures. */
void
status_connections_clear(int reinit)
{
  connections = 0;
  if (!addresses)
    return;
  strmap_free(addresses, NULL); /* NULL, because no need to free values. */
  addresses = NULL;
  if (reinit)
    addresses = strmap_new();
}

/** Note that we saw a new connection. */
void
status_note_connection(const char *addrport)
{
  char *addr = xstrdup(addrport), *p;

  if ((p = strrchr(addr, ':'))) {
    *p = '\0';
  } else {
    log_warn("Error in address %s: port expected.", addrport);
    free(addr);
    return;
  }

  if (!addresses)
    addresses = strmap_new();
  strmap_set_lc(addresses, addr, (void*)(1));
  free(addr);
  connections++;
}

/** Time when we started. */
static time_t started = 0;

/** Time when we last reset connection and address counters. */
static time_t last_reset_counters = 0;

/** Reset status information this often. */
#define RESET_COUNTERS 86400 /* 86400 seconds == 24 hours */

/** Initialize status information to print out heartbeat messages. */
void
status_init(void)
{
  time_t now = time(NULL);
  started = now;
  last_reset_counters = now;
  if (!addresses)
    addresses = strmap_new();
}

/** Log information about our uptime and the number of connections we saw.
 */
void
status_log_heartbeat(void)
{
  time_t now = time(NULL);
  long secs = now - started;
  long days = secs / 86400;
  int hours = (int)((secs - (days * 86400)) / 3600);
  int last_reset_hours = (int) (now - last_reset_counters) / 3600;
  int minutes = (int)((secs - (days * 86400) - (hours * 3600)) / 60);
  log_notice("Heartbeat: obfsproxy's uptime is %ld day(s), %d hour(s), and "
             "%d minute(s).", days, hours, minutes);

  /* Also log connection stats, if we are keeping notes. */
  if (strmap_size(addresses) > 0)
    log_notice("Heartbeat: During the last %d hour(s) we saw %u connection(s) "
               "from %d unique address(es).",
               last_reset_hours, connections, strmap_size(addresses));

  if (now - last_reset_counters >= RESET_COUNTERS) {
    log_info("Resetting connection counters.");
    status_connections_reinit();
    last_reset_counters += RESET_COUNTERS;
  }
}
