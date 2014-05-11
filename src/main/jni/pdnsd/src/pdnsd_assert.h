/* This include file was added by Paul A. Rombouts.
   I had terrible difficulties with cyclic dependencies of the include files
   written by Thomas Moestl. The only way I knew how to break the cycle was to
   put some declarations in a seperate file.

   Copyright (C) 2000, 2001 Thomas Moestl
   Copyright (C) 2002 Paul A. Rombouts

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

#ifndef PDNSD_ASSERT_H
#define PDNSD_ASSERT_H

/* Originally in helpers.h */

/* format string checking for printf-like functions */
#ifdef __GNUC__
#define printfunc(fmt, firstva) __attribute__((__format__(__printf__, fmt, firstva)))
#else
#define printfunc(fmt, firstva)
#endif

void pdnsd_exit(void);


/*
 * Assert macro, used in some places. For now, it should be always defined, not
 * only in the DEBUG case, to be on the safe side security-wise.
 */
#define PDNSD_ASSERT(cond, msg)						\
	{ if (!(cond)) {						\
		log_error("%s:%d: %s", __FILE__, __LINE__, msg);	\
		pdnsd_exit();						\
 	} }

#endif
