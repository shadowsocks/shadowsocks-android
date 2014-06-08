/* servers.h - manage a set of dns servers

   Copyright (C) 2000 Thomas Moestl
   Copyright (C) 2002, 2003, 2004, 2005 Paul A. Rombouts

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


#ifndef _SERVERS_H_
#define _SERVERS_H_

#include <config.h>
#include "consts.h"

/* Number of ping timeouts before we take a server offline. */
#define PINGREPEAT 2

extern pthread_t servstat_thrid;
extern volatile int signal_interrupt;


int start_servstat_thread(void);
void sched_server_test(pdnsd_a *sa, int nadr, int up);
int mark_servers(int i, char* label, int up);
void test_onquery(void);
void lock_server_data();
void unlock_server_data();
int exclusive_lock_server_data(int tm);
void exclusive_unlock_server_data(int retest);
int change_servers(int i, addr_array ar, int up);

inline static int needs_testing(servparm_t *sp)
  __attribute__((always_inline));
inline static int needs_testing(servparm_t *sp)
{
  return ((sp->interval>0 || sp->interval==-2) && (sp->uptest!=C_NONE || sp->scheme[0]));
}

inline static int needs_intermittent_testing(servparm_t *sp)
  __attribute__((always_inline));
inline static int needs_intermittent_testing(servparm_t *sp)
{
  return (sp->interval>0 && (sp->uptest!=C_NONE || sp->scheme[0]));
}

inline static int is_interrupted_servstat_thread()
  __attribute__((always_inline));
inline static int is_interrupted_servstat_thread()
{
  return (signal_interrupt && pthread_equal(pthread_self(),servstat_thrid));
}

#endif
