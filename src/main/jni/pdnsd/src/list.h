/* list.h - Dynamic array and list handling

   Copyright (C) 2001 Thomas Moestl
   Copyright (C) 2002, 2003, 2007, 2009, 2011 Paul A. Rombouts

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


#ifndef LIST_H
#define LIST_H

#include <stdlib.h>
#include <string.h>
#include "pdnsd_assert.h"


typedef struct {size_t nel;} *darray;

/* used in type declarations */
#define DYNAMIC_ARRAY(typ)  struct {size_t nel; typ elem[0];}
#define DA_TYP_OFFSET(atyp) ((size_t)((atyp)0)->elem)
#define DA_OFFSET(a) DA_TYP_OFFSET(typeof (a))

#define DA_CREATE(atyp,n) ((atyp)da_resize(NULL,DA_TYP_OFFSET(atyp),sizeof(((atyp)0)->elem[0]),n,NULL))
#define DA_INDEX(a,i) ((a)->elem[i])
/* Used often, so make special-case macro here */
#define DA_LAST(a) ((a)->elem[(a)->nel-1])

#define DA_GROW1(a) ((typeof (a))da_grow1((darray)(a),DA_OFFSET(a),sizeof((a)->elem[0]),NULL))
#define DA_GROW1_F(a,cleanup) ((typeof (a))da_grow1((darray)(a),DA_OFFSET(a),sizeof((a)->elem[0]),cleanup))
#define DA_RESIZE(a,n) ((typeof (a))da_resize((darray)(a),DA_OFFSET(a),sizeof((a)->elem[0]),n,NULL))
#define DA_NEL(a) da_nel((darray)(a))

/*
 * Some or all of these should be inline.
 * They aren't macros for type safety.
 */

darray da_grow1(darray a, size_t headsz, size_t elemsz, void (*cleanuproutine) (void *));
darray da_resize(darray a, size_t headsz, size_t elemsz, size_t n, void (*cleanuproutine) (void *));

inline static unsigned int da_nel(darray a)
  __attribute__((always_inline));
inline static unsigned int da_nel(darray a)
{
  if (a==NULL)
    return 0;
  return a->nel;
}

/* alloc/free debug code.*/
#ifdef ALLOC_DEBUG
void   DBGda_free(darray a, size_t headsz, size_t elemsz, char *file, int line);
#define da_free(a)	DBGda_free((darray)(a),DA_OFFSET(a),sizeof((a)->elem[0]), __FILE__, __LINE__)
#else
#define da_free		free
#endif


/* This dynamic "list" structure is useful if the items are not all the same size.
   The elements can only be read back in sequential order, not indexed as with the dynamic arrays.
*/
struct _dynamic_list_head {
	size_t last,lastsz;
	char data[0];
};

typedef struct _dynamic_list_head  *dlist;

inline static void *dlist_first(dlist a)
  __attribute__((always_inline));
inline static void *dlist_first(dlist a)
{
  return a?&a->data[sizeof(size_t)]:NULL;
}

/* dlist_next() returns a reference to the next item in the list, or NULL is there is no next item.
   ref should be properly aligned.
   If the dlist was grown with dlist_grow(), this should be OK.
*/
inline static void *dlist_next(void *ref)
  __attribute__((always_inline));
inline static void *dlist_next(void *ref)
{
  size_t incr= *(((size_t *)ref)-1);
  return incr?((char *)ref)+incr:NULL;
}

/* dlist_last() returns a reference to the last item. */
inline static void *dlist_last(dlist a)
  __attribute__((always_inline));
inline static void *dlist_last(dlist a)
{
  return a?&a->data[a->last+sizeof(size_t)]:NULL;
}

dlist dlist_grow(dlist a, size_t len);

#define dlist_free free


/* linked list data type. */
struct llistnode_s {
  struct llistnode_s *next;
  char *data[0];
};

typedef struct {
  struct llistnode_s *first, *last;
}
  llist;

inline static void llist_init(llist *a)
  __attribute__((always_inline));
inline static void llist_init(llist *a)
{
  a->first=NULL;
  a->last= NULL;
}

inline static int llist_isempty(llist *a)
  __attribute__((always_inline));
inline static int llist_isempty(llist *a)
{
  return a->first==NULL;
}

inline static void *llist_first(llist *a)
  __attribute__((always_inline));
inline static void *llist_first(llist *a)
{
  struct llistnode_s *p= a->first;
  return p?p->data:NULL;
}

inline static void *llist_next(void *ref)
  __attribute__((always_inline));
inline static void *llist_next(void *ref)
{
  struct llistnode_s *next= *(((struct llistnode_s **)ref)-1);
  return next?next->data:NULL;
}

inline static void *llist_last(llist *a)
  __attribute__((always_inline));
inline static void *llist_last(llist *a)
{
  struct llistnode_s *p= a->last;
  return p?p->data:NULL;
}

int llist_grow(llist *a, size_t len);
void llist_free(llist *a);

#endif /* def LIST_H */
