/* icmp.h - Server response tests using ICMP echo requests
   Copyright (C) 2000 Thomas Moestl
   Copyright (C) 2007 Paul A. Rombouts

  This file is part of the pdnsd package.

  pdnsd is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  pdnsd is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with pdnsd; see the file COPYING. If not, see
  <http://www.gnu.org/licenses/>.
*/


#ifndef ICMP_H
#define ICMP_H


#include <config.h>
#include "ipvers.h"

volatile extern int ping_isocket;
volatile extern int ping6_isocket;

/* initialize a socket for pinging */
void init_ping_socket(void);

/*
 * This is a classical ping routine.
 * timeout in 10ths of seconds, rep is the repetition count.
 */

int ping(pdnsd_a *addr, int timeout, int rep);

#endif
