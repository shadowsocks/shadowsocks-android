/* netdev.h - Test network devices for existence and status
   Copyright (C) 2000 Thomas Moestl

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


#ifndef _NETDEV_H_
#define _NETDEV_H_

#include <config.h>
#include "ipvers.h"

int if_up(char *devname);
int dev_up(char *ifname, char *devname);
int is_local_addr(pdnsd_a *a);

#endif
