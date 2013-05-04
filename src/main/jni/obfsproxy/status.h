/* Copyright 2012 The Tor Project
   See LICENSE for other credits and copying information
*/

/**
 * \file status.h
 * \brief Headers for status.c.
 **/

void status_init(void);
void status_cleanup(void);
void status_note_connection(const char *addrport);
void status_log_heartbeat(void);
void status_connections_clear(int reinit);
#define status_connections_cleanup() status_connections_clear(0)
#define status_connections_reinit() status_connections_clear(1)
