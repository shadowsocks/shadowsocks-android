/* hash.c - Manage hashes for cached dns records

   Copyright (C) 2000, 2001 Thomas Moestl
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

#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "hash.h"
#include "cache.h"
#include "error.h"
#include "helpers.h"
#include "consts.h"


/* This is not a perfect hash, but I hope it holds. It is designed for 1024 hash
 * buckets, and hashes strings with case-insensitivity.
 * It is position-aware in a limited way.
 * It is exactly seen a two-way hash: because I do not want to exaggerate
 * the hash buckets (i do have 1024), but I hash strings and string-comparisons
 * are expensive, I save another 32 bit hash in each hash element that is checked
 * before the string. The 32 bit hash is also used to order the entries in a hash chain.
 * I hope not to have all too much collision concentration.
 *
 * The ip hash was removed. I don't think it concentrated the collisions too much.
 * If it does, the hash algorithm needs to be changed, rather than using another
 * hash.
 * Some measurements seem to indicate that the hash algorithm is doing reasonable well.
 */

dns_hash_ent_t *hash_buckets[HASH_NUM_BUCKETS];


/*
 * Hash a dns name (length-byte string format) to HASH_SZ bit.
 * *rhash is set to a long int hash.
 */
static unsigned dns_hash(const unsigned char *str, unsigned long *rhash)
{
	unsigned s,i,lb,c;
	unsigned long r;
	s=0; r=0;
	i=0;
	while((lb=str[i])) {
		s+=lb<<(i%(HASH_SZ-5));
		r+=((unsigned long)lb)<<(i%(8*sizeof(unsigned long)-7));
		++i;
		do {
			c=toupper(str[i]);
			s+=c<<(i%(HASH_SZ-5));
			r+=((unsigned long)c)<<(i%(8*sizeof(unsigned long)-7));
			++i;
		} while(--lb);
	}
	s=(s&HASH_BITMASK)+((s&(~HASH_BITMASK))>>HASH_SZ);
	s=(s&HASH_BITMASK)+((s&(~HASH_BITMASK))>>HASH_SZ);
	s &= HASH_BITMASK;
#ifdef DEBUG_HASH
	{
		unsigned char buf[DNSNAMEBUFSIZE];
		printf("Diagnostic: hashes for %s: %03x,%04lx\n",rhn2str(str,buf,sizeof(buf)),s,r);
	}
#endif
	if(rhash) *rhash=r;
	return s;
}

/*
 * Initialize hash to hold a dns hash table
 */
/* This is now defined as an inline function in hash.h */
#if 0
void mk_dns_hash()
{
	int i;
	for(i=0;i<HASH_NUM_BUCKETS;i++)
		hash_buckets[i]=NULL;
}
#endif

/*
  Lookup in the hash table for key. If it is found, return the pointer to the cache entry.
  If no entry is found, return 0.
  If loc is not NULL, it will used to store information about the location within the hash table
  This can be used to add an entry with add_dns_hash() or delete the entry with del_dns_hash_ent().
*/
dns_cent_t *dns_lookup(const unsigned char *key, dns_hash_loc_t *loc)
{
	dns_cent_t *retval=NULL;
	unsigned idx;
	unsigned long rh;
	dns_hash_ent_t **hep,*he;

	idx = dns_hash(key,&rh);
	hep = &hash_buckets[idx];
	while ((he= *hep) && he->rhash<=rh) {
		if (he->rhash==rh && rhnicmp(key,he->data->qname)) {
			retval = he->data;
			break;
		}
		hep = &he->next;
	}
	if(loc) {
		loc->pos = hep;
		loc->rhash = rh;
	}
	return retval;
}

/*
  Add a cache entry to the hash table.

  loc must contain the location where the the new entry should be inserted
  (this location can be obtained with dns_lookup).

  add_dns_hash returns 1 on success, or 0 if out of memory.
*/
int add_dns_hash(dns_cent_t *data, dns_hash_loc_t *loc)
{
	dns_hash_ent_t *he = malloc(sizeof(dns_hash_ent_t));

	if(!he)
		return 0;

	he->next = *(loc->pos);
	he->rhash = loc->rhash;
	he->data = data;
	*(loc->pos) = he;

	return 1;
}

/*
  Delete the hash entry indentified by the location returned by dns_lookup().
*/
dns_cent_t *del_dns_hash_ent(dns_hash_loc_t *loc)
{
	dns_hash_ent_t *he = *(loc->pos);
	dns_cent_t *data;

	*(loc->pos) = he->next;
	data = he->data;
	free(he);
	return data;
}

/*
 * Delete the first entry indexed by key from the hash. Returns the data field or NULL.
 * Since two cents are not allowed to be for the same host name, there will be only one.
 */
dns_cent_t *del_dns_hash(const unsigned char *key)
{
	unsigned idx;
	unsigned long rh;
	dns_hash_ent_t **hep,*he;
	dns_cent_t *data;

	idx = dns_hash(key,&rh);
	hep = &hash_buckets[idx];
	while ((he= *hep) && he->rhash<=rh) {
		if (he->rhash==rh && rhnicmp(key,he->data->qname)) {
			*hep = he->next;
			data = he->data;
			free(he);
			return data;
		}
		hep = &he->next;
	}
	return NULL;   /* not found */
}


/*
 * Delete all entries in a hash bucket.
 */
void free_dns_hash_bucket(int i)
{
	dns_hash_ent_t *he,*hen;

	he=hash_buckets[i];
	hash_buckets[i]=NULL;
	while (he) {
		hen=he->next;
		del_cent(he->data);
		free(he);
		he=hen;
	}
}

/*
 * Delete all entries in a hash bucket whose names match those in
 * an include/exclude list.
 */
void free_dns_hash_selected(int i, slist_array sla)
{
	dns_hash_ent_t **hep,*he,*hen;
	int j,m=DA_NEL(sla);

	hep= &hash_buckets[i];
	he= *hep;

	while (he) {
		unsigned char *name=he->data->qname;
		for(j=0;j<m;++j) {
			slist_t *sl=&DA_INDEX(sla,j);
			unsigned int nrem,lrem;
			domain_match(name,sl->domain,&nrem,&lrem);
			if(!lrem && (!sl->exact || !nrem)) {
				if(sl->rule==C_INCLUDED)
					goto delete_entry;
				else
					break;
			}
		}
		/* default policy is not to delete */
		hep= &he->next;
		he= *hep;
		continue;

	delete_entry:
		*hep=hen=he->next;;
		del_cent(he->data);
		free(he);
		he=hen;
	}
}

/*
 * Delete the whole hash table, freeing all memory
 */
void free_dns_hash()
{
	int i;
	dns_hash_ent_t *he,*hen;
	for (i=0;i<HASH_NUM_BUCKETS;i++) {
		he=hash_buckets[i];
		hash_buckets[i]=NULL;
		while (he) {
			hen=he->next;
			del_cent(he->data);
			free(he);
			he=hen;
		}
	}
}

/*
 * The following functions are for iterating over the hash.
 * fetch_first returns the data field of the first element (or NULL if there is none), and fills pos
 * for subsequent calls of fetch_next.
 * fetch_next returns the data field of the element after the element that was returned by the last
 * call with the same position argument (or NULL if there is none)
 *
 * Note that these are designed so that you may actually delete the elements you retrieved from the hash.
 */
dns_cent_t *fetch_first(dns_hash_pos_t *pos)
{
	int i;
	for (i=0;i<HASH_NUM_BUCKETS;i++) {
		dns_hash_ent_t *he=hash_buckets[i];
		if (he) {
			pos->bucket=i;
			pos->ent=he->next;
			return he->data;
		}
	}
	return NULL;
}

dns_cent_t *fetch_next(dns_hash_pos_t *pos)
{
	dns_hash_ent_t *he=pos->ent;
	int i;
	if (he) {
		pos->ent=he->next;
		return he->data;
	}

	for (i=pos->bucket+1;i<HASH_NUM_BUCKETS;i++) {
		he=hash_buckets[i];
		if (he) {
			pos->bucket=i;
			pos->ent=he->next;
			return he->data;
		}
	}
	return NULL;
}

#ifdef DEBUG_HASH
void dumphash()
{
	if(debug_p) {
		int i, j;
		dns_hash_ent_t *he;

		for (i=0; i<HASH_NUM_BUCKETS; i++) {
			for (j=0, he=hash_buckets[i]; he; he=he->next, j++) ;
			DEBUG_MSG("bucket %d: %d entries\n", i, j);
		}
	}
}
#endif
