/* Copyright 2011 Nick Mathewson, George Kadianakis
   See LICENSE for other credits and copying information
*/

/**
 * \file main.h
 * \brief Headers for main.c.
 **/


#ifndef MAIN_H
#define MAIN_H

void finish_shutdown(void);

void obfsproxy_init();
void obfsproxy_cleanup();

int is_supported_protocol(const char *name);
void ATTR_NORETURN usage(void);

struct event_base *get_event_base(void);

#endif
