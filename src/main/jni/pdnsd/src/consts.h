/* consts.h - Common config constants & handling

   Copyright (C) 2000, 2001 Thomas Moestl
   Copyright (C) 2002, 2003, 2005, 2006, 2007, 2009 Paul A. Rombouts

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


#ifndef CONSTS_H
#define CONSTS_H

#include <config.h>

#define C_RRTOFFS  64

enum {
	C_ERR,
	C_ON,
	C_OFF,
	C_DEFAULT,
	C_DISCOVER,
	C_NONE,
	C_IF,
	C_EXEC,
	C_PING,
	C_QUERY,
	C_ONQUERY,
	C_ONTIMEOUT,
	UDP_ONLY,
	TCP_ONLY,
	TCP_UDP,
	UDP_TCP,
	C_DEV,
	C_DIALD,
	C_INCLUDED,
	C_EXCLUDED,
	C_SIMPLE_ONLY,
	C_FQDN_ONLY,
	C_AUTH,
	C_DOMAIN,
	C_FAIL,
	C_NEGATE
};

typedef struct {
	const char *name;
	int         val;
} namevalue_t;

int binsearch_keyword(const char *name, int len, const namevalue_t dic[], int range);
int lookup_const(const char *name, int len);
const char *const_name(int c);  /* Added by Paul Rombouts */

#endif
