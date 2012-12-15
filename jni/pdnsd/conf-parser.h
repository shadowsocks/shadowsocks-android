/* conf-parser.h - definitions for parser of pdnsd config files.
   The parser was rewritten in C from scratch and doesn't require (f)lex
   or yacc/bison.

   Copyright (C) 2004,2008 Paul A. Rombouts.

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

#ifndef CONF_PARSER_H
#define CONF_PARSER_H

int confparse(FILE* in, char *prestr, globparm_t *global, servparm_array *servers, int includedepth, char **errstr);

#endif /* CONF_PARSER_H */
