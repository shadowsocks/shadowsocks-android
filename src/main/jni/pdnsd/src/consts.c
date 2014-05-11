/* consts.c - Common config constants & handling

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

#include <config.h>
#include <stdlib.h>
#include <string.h>
#include "consts.h"
#include "rr_types.h"


/* Order alphabetically!! */
static const namevalue_t const_dic[]={
	{"auth",        C_AUTH},
	{"default",     C_DEFAULT},
	{"dev",         C_DEV},
	{"diald",       C_DIALD},
	{"discover",    C_DISCOVER},
	{"domain",      C_DOMAIN},
	{"excluded",    C_EXCLUDED},
	{"exec",        C_EXEC},
	{"fail",        C_FAIL},
	{"false",       C_OFF},
	{"fqdn_only",   C_FQDN_ONLY},
	{"if",          C_IF},
	{"included",    C_INCLUDED},
	{"negate",      C_NEGATE},
	{"no",          C_OFF},
	{"none",        C_NONE},
	{"off",         C_OFF},
	{"on",          C_ON},
	{"onquery",     C_ONQUERY},
	{"ontimeout",   C_ONTIMEOUT},
	{"ping",        C_PING},
	{"query",       C_QUERY},
	{"simple_only", C_SIMPLE_ONLY},
	{"tcp_only",    TCP_ONLY},
	{"tcp_udp",     TCP_UDP},
	{"true",        C_ON},
	{"udp_only",    UDP_ONLY},
	{"udp_tcp",     UDP_TCP},
	{"yes",         C_ON}
};

/* Added by Paul Rombouts */
static const char *const const_names[]={
	"error",
	"on",
	"off",
	"default",
	"discover",
	"none",
	"if",
	"exec",
	"ping",
	"query",
	"onquery",
	"ontimeout",
	"udp_only",
	"tcp_only",
	"tcp_udp",
	"udp_tcp",
	"dev",
	"diald",
	"included",
	"excluded",
	"simple_only",
	"fqdn_only",
	"auth",
	"domain",
	"fail",
	"negate"
};

/* compare two strings.
   The first one is given as pointer to a char array of length len (which
   should not contain any null chars),
   the second one as a pointer to a null terminated char array.
*/
inline static int keyncmp(const char *key1, int len, const char *key2)
{
	int cmp=strncmp(key1,key2,len);
	if(cmp) return cmp;
	return -(int)((unsigned char)(key2[len]));
}

int binsearch_keyword(const char *name, int len, const namevalue_t dic[], int range)
{
	int i=0,j=range;

	while(i<j) {
		int k=(i+j)/2;
		int cmp=keyncmp(name,len,dic[k].name);
		if(cmp<0)
			j=k;
		else if(cmp>0)
			i=k+1;
		else
			return dic[k].val;
	}

	return 0;
}


int lookup_const(const char *name, int len)
{
	return binsearch_keyword(name,len,const_dic,sizeof(const_dic)/sizeof(namevalue_t));
}

/* Added by Paul Rombouts */
const char *const_name(int c)
{
  return (c>=0 && c<sizeof(const_names)/sizeof(char *))? const_names[c] : "ILLEGAL!";
}
