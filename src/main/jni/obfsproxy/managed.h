/* Copyright 2011 Nick Mathewson, George Kadianakis
   See LICENSE for other credits and copying information
*/

/**
 * \file managed.h
 * \brief Headers for managed.c.
 **/

#ifndef MANAGED_H
#define MANAGED_H

int launch_managed_proxy();

#ifdef MANAGED_PRIVATE

int validate_bindaddrs(const char *all_bindaddrs, const char *all_transports);

#endif /* MANAGED_PRIVATE */

#endif
