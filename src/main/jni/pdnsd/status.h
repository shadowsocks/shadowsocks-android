/* status.h - Make server status information accessible through a named pipe

   Copyright (C) 2000, 2001 Thomas Moestl
   Copyright (C) 2002, 2004, 2008, 2009 Paul A. Rombouts

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


#ifndef _STATUS_H_
#define _STATUS_H_

#include <config.h>
#include "conff.h"

extern char *sock_path;
extern int stat_sock;

/* The commands for pdnsd-ctl */
#define CTL_CMDVERNR 0x6800  /* pdnsd-ctl command version (magic number used to check compatibility) */

#define CTL_MIN      1
#define CTL_STATS    1 /* Give out stats (like the "traditional" status pipe) */
#define CTL_SERVER   2 /* Enable or disable a server */
#define CTL_RECORD   3 /* Delete or invalidate records */
#define CTL_SOURCE   4 /* Read a hosts-style file */
#define CTL_ADD      5 /* Add a record of the given type */
#define CTL_NEG      6 /* Add a negative cached record */
#define CTL_CONFIG   7 /* Re-read config file */
#define CTL_INCLUDE  8 /* Read file as config file, disregarding global and server sections */
#define CTL_EVAL     9 /* Parse string as if part of config file */
#define CTL_EMPTY   10 /* Empty the cache */
#define CTL_DUMP    11 /* Dump cache contents */
#define CTL_MAX     11

#define CTL_S_UP     1
#define CTL_S_DOWN   2
#define CTL_S_RETEST 3
#define CTL_R_DELETE 1
#define CTL_R_INVAL  2

void init_stat_sock(void);
int start_stat_sock(void);

#endif
