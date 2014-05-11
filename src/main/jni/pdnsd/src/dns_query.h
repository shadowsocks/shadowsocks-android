/* dns_query.h - Execute outgoing dns queries and write entries to cache

   Copyright (C) 2000, 2001 Thomas Moestl
   Copyright (C) 2002, 2003, 2004, 2006, 2009, 2011 Paul A. Rombouts

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


#ifndef DNS_QUERY_H
#define DNS_QUERY_H

#include "cache.h"

/* Default UDP buffer size (when EDNS is not used). */
#define UDP_BUFSIZE 512


typedef struct qhintnode_s qhintnode_t;

/* --- parallel query */
int r_dns_cached_resolve(unsigned char *name, int thint, dns_cent_t **cachedp,
			 int hops, qhintnode_t *qhlist, time_t queryts,
			 unsigned char *c_soa);
#define dns_cached_resolve(name,thint,cachedp,hops,queryts,c_soa) \
        r_dns_cached_resolve(name,thint,cachedp,hops,NULL,queryts,c_soa)

addr2_array dns_rootserver_resolv(atup_array atup_a, int port, char edns_query, time_t timeout);
int query_uptest(pdnsd_a *addr, int port, const unsigned char *name, time_t timeout, int rep);

/* --- from dns_answer.c */
int add_opt_pseudo_rr(dns_msg_t **ans, size_t *sz, size_t *allocsz,
		      unsigned short udpsize, unsigned short rcode,
		      unsigned short ednsver, unsigned short Zflags);
size_t remove_opt_pseudo_rr(dns_msg_t *ans, size_t sz);

#endif
