/* hash.h - Manage hashes for cached dns records

   Copyright (C) 2000 Thomas Moestl
   Copyright (C) 2003, 2005 Paul A. Rombouts

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


#ifndef _HASH_H_
#define _HASH_H_
#include <config.h>
#include "cache.h"

typedef struct dns_hash_ent_s {
	struct dns_hash_ent_s	*next;
	unsigned long		rhash; /* this is a better hash */
	dns_cent_t		*data;
} dns_hash_ent_t;

/* Redefine this if you want another hash size. Should work ;-).
 * The number of hash buckets is computed as power of two;
 * so, e.g. HASH_SZ set to 10 yields 1024 hash rows (2^10 or 1<<10).
 * Only powers of two are possible conveniently.
 * HASH_SZ may not be bigger than 32 (if you set it even close to that value,
 * you are nuts.) */
/* #define HASH_SZ       10 */  /* Now defined in config.h */
#define HASH_NUM_BUCKETS (1<<HASH_SZ)

#define HASH_BITMASK     (HASH_NUM_BUCKETS-1)

extern dns_hash_ent_t *hash_buckets[];

/* A type for remembering the position in the hash table where a new entry can be inserted. */
typedef struct {
	dns_hash_ent_t **pos;      /* pointer to the location in the hash table */
	unsigned long  rhash;      /* long hash */
} dns_hash_loc_t;

/* A type for position specification for fetch_first and fetch_next */
typedef struct {
	int            bucket;     /* bucket chain we are in */
	dns_hash_ent_t *ent;       /* entry */
} dns_hash_pos_t;

inline static void mk_dns_hash()  __attribute__((always_inline));
inline static void mk_dns_hash()
{
	int i;
	for(i=0;i<HASH_NUM_BUCKETS;i++)
		hash_buckets[i]=NULL;
}

dns_cent_t *dns_lookup(const unsigned char *key, dns_hash_loc_t *loc);
int add_dns_hash(dns_cent_t *data, dns_hash_loc_t *loc);
dns_cent_t *del_dns_hash_ent(dns_hash_loc_t *loc);
dns_cent_t *del_dns_hash(const unsigned char *key);
void free_dns_hash_bucket(int i);
void free_dns_hash_selected(int i, slist_array sla);
void free_dns_hash();

dns_cent_t *fetch_first(dns_hash_pos_t *pos);
dns_cent_t *fetch_next(dns_hash_pos_t *pos);

#ifdef DEBUG_HASH
void dumphash();
#endif

#endif
