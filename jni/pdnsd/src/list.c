/* list.c - Dynamic array and list handling

   Copyright (C) 2001 Thomas Moestl
   Copyright (C) 2002, 2003, 2007, 2011 Paul A. Rombouts

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
#include "helpers.h"
#include "error.h"
#include "list.h"


/* Grow a dynamic array to hold one extra element.
   This could be done using da_resize(), but this is such a common operation
   it is has been given its own optimized implementation.
   da_grow1() returns a pointer to the new (possibly reallocated) array if
   successful, otherwise it frees the old array (after freeing all the array
   elements if a clean-up routine is supplied) and returns NULL.
*/
darray da_grow1(darray a, size_t headsz, size_t elemsz, void (*cleanuproutine) (void *))
{
	size_t k = (a?a->nel:0);
	if(!a || (k!=0 && (k&7)==0)) {
		darray tmp=(darray)realloc(a, headsz+elemsz*(k+8));
		if (!tmp && a) {
			if(cleanuproutine) {
				size_t i;
				for(i=0;i<k;++i)
					cleanuproutine(((char *)a)+headsz+elemsz*i);
			}
			free(a);
		}
		a=tmp;
	}
	if(a) a->nel=k+1;
	return a;
}

inline static size_t alloc_nel(size_t n)
{
  return n==0 ? 8 : (n+7)&(~7);
}

/* da_resize() allows you to grow (or shrink) a dynamic array to an arbitrary length n,
   but is otherwise similar to da_grow1().
*/
darray da_resize(darray a, size_t headsz, size_t elemsz, size_t n, void (*cleanuproutine) (void *))
{
	size_t ael = (a?alloc_nel(a->nel):0);
	size_t new_ael = alloc_nel(n);
	if(new_ael != ael) {
		/* adjust alloced space. */
		darray tmp=(darray)realloc(a, headsz+elemsz*new_ael);
		if (!tmp && a) {
			if(cleanuproutine) {
				size_t i,k=a->nel;
				for(i=0;i<k;++i)
					cleanuproutine(((char *)a)+headsz+elemsz*i);
			}
			free(a);
		}
		a=tmp;
	}
	if(a) a->nel=n;
	return a;
}

#ifdef ALLOC_DEBUG
void DBGda_free(darray a, size_t headsz, size_t elemsz, char *file, int line)
{
	if (a==NULL)
		{DEBUG_MSG("- da_free, %s:%d, not initialized\n", file, line);}
	else
		{DEBUG_MSG("- da_free, %s:%d, %lu bytes\n", file, line,
			   (unsigned long)(headsz+elemsz*alloc_nel(a->nel)));}
	free(a);
}
#endif


#define DLISTALIGN(len) (((len) + (sizeof(size_t)-1)) & ~(sizeof(size_t)-1))
/* This mask corresponds to a chunk size of 1024. */
#define DLISTCHUNKSIZEMASK ((size_t)0x3ff)

/* Add space for a new item of size len to the list a.
   dlist_grow() returns a pointer to the new (possibly reallocated) list structure if
   successful, otherwise it frees the old list and returns NULL.
*/
dlist dlist_grow(dlist a, size_t len)
{
	size_t sz=0, allocsz=0, szincr, newsz;
	if(a) {
		sz=a->last+a->lastsz;
		allocsz = (sz+DLISTCHUNKSIZEMASK)&(~DLISTCHUNKSIZEMASK);
		*((size_t *)&a->data[a->last])=a->lastsz;
	}
	szincr=DLISTALIGN(len+sizeof(size_t));
	newsz=sz+szincr;
	if(newsz>allocsz) {
		dlist tmp;
		allocsz = (newsz+DLISTCHUNKSIZEMASK)&(~DLISTCHUNKSIZEMASK);
		tmp=realloc(a, sizeof(struct _dynamic_list_head)+allocsz);
		if (!tmp)
			free(a);
		a=tmp;
	}
	if(a) {
		a->last=sz;
		a->lastsz=szincr;
		*((size_t *)&a->data[sz])=0;
	}
	return a;
}


/* Add a new node, capable of holding data of size len, at the end of a linked list.
   llist_grow() returns 1 if successful, otherwise it frees the entire linked list
   and returns 0.
 */
int llist_grow(llist *a, size_t len)
{
  struct llistnode_s *new= (struct llistnode_s *)malloc(sizeof(struct llistnode_s)+len);

  if(!new) {
    llist_free(a);
    return 0;
  }

  new->next=NULL;

  if(!a->first)
    a->first=new;
  else
    a->last->next=new;

  a->last=new;

  return 1;
}

void llist_free(llist *a)
{
  struct llistnode_s *p= a->first;

  while(p) {
    struct llistnode_s *next= p->next;
    free(p);
    p=next;
  }

  a->first=NULL;
  a->last= NULL;
}
