/* Copyright (c) 2003-2004, Roger Dingledine
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2011, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file container.c
 * \brief Implements a smartlist (a resizable array) along
 * with helper functions to use smartlists.  Also includes
 * hash table implementations of a string-to-void* map, and of
 * a digest-to-void* map.
 **/

#include "util.h"
#include "container.h"
#include "crypt.h"
#include "ht.h"

/** All newly allocated smartlists have this capacity. */
#define SMARTLIST_DEFAULT_CAPACITY 16

/** Allocate and return an empty smartlist.
 */
smartlist_t *
smartlist_create(void)
{
  smartlist_t *sl = xmalloc(sizeof(smartlist_t));
  sl->num_used = 0;
  sl->capacity = SMARTLIST_DEFAULT_CAPACITY;
  sl->list = xmalloc(sizeof(void *) * sl->capacity);
  return sl;
}

/** Deallocate a smartlist.  Does not release storage associated with the
 * list's elements.
 */
void
smartlist_free(smartlist_t *sl)
{
  if (!sl)
    return;
  free(sl->list);
  free(sl);
}

/** Remove all elements from the list.
 */
void
smartlist_clear(smartlist_t *sl)
{
  sl->num_used = 0;
}

/** Make sure that <b>sl</b> can hold at least <b>size</b> entries. */
static inline void
smartlist_ensure_capacity(smartlist_t *sl, int size)
{
#if SIZEOF_SIZE_T > SIZEOF_INT
#define MAX_CAPACITY (INT_MAX)
#else
#define MAX_CAPACITY (int)((SIZE_MAX / (sizeof(void*))))
#endif
  if (size > sl->capacity) {
    int higher = sl->capacity;
    if (size > MAX_CAPACITY/2) {
      obfs_assert(size <= MAX_CAPACITY);
      higher = MAX_CAPACITY;
    } else {
      while (size > higher)
        higher *= 2;
    }
    sl->capacity = higher;
    sl->list = xrealloc(sl->list, sizeof(void*)*((size_t)sl->capacity));
  }
}

/** Append element to the end of the list. */
void
smartlist_add(smartlist_t *sl, void *element)
{
  smartlist_ensure_capacity(sl, sl->num_used+1);
  sl->list[sl->num_used++] = element;
}

/** Append each element from S2 to the end of S1. */
void
smartlist_add_all(smartlist_t *s1, const smartlist_t *s2)
{
  int new_size = s1->num_used + s2->num_used;
  obfs_assert(new_size >= s1->num_used); /* check for overflow. */
  smartlist_ensure_capacity(s1, new_size);
  memcpy(s1->list + s1->num_used, s2->list, s2->num_used*sizeof(void*));
  s1->num_used = new_size;
}

/** Remove all elements E from sl such that E==element.  Preserve
 * the order of any elements before E, but elements after E can be
 * rearranged.
 */
void
smartlist_remove(smartlist_t *sl, const void *element)
{
  int i;
  if (element == NULL)
    return;
  for (i=0; i < sl->num_used; i++)
    if (sl->list[i] == element) {
      sl->list[i] = sl->list[--sl->num_used]; /* swap with the end */
      i--; /* so we process the new i'th element */
    }
}

/** If <b>sl</b> is nonempty, remove and return the final element.  Otherwise,
 * return NULL. */
void *
smartlist_pop_last(smartlist_t *sl)
{
  obfs_assert(sl);
  if (sl->num_used)
    return sl->list[--sl->num_used];
  else
    return NULL;
}

/** Reverse the order of the items in <b>sl</b>. */
void
smartlist_reverse(smartlist_t *sl)
{
  int i, j;
  void *tmp;
  obfs_assert(sl);
  for (i = 0, j = sl->num_used-1; i < j; ++i, --j) {
    tmp = sl->list[i];
    sl->list[i] = sl->list[j];
    sl->list[j] = tmp;
  }
}

/** If there are any strings in sl equal to element, remove and free them.
 * Does not preserve order. */
void
smartlist_string_remove(smartlist_t *sl, const char *element)
{
  int i;
  obfs_assert(sl);
  obfs_assert(element);
  for (i = 0; i < sl->num_used; ++i) {
    if (!strcmp(element, sl->list[i])) {
      free(sl->list[i]);
      sl->list[i] = sl->list[--sl->num_used]; /* swap with the end */
      i--; /* so we process the new i'th element */
    }
  }
}

/** Return true iff some element E of sl has E==element.
 */
int
smartlist_isin(const smartlist_t *sl, const void *element)
{
  int i;
  for (i=0; i < sl->num_used; i++)
    if (sl->list[i] == element)
      return 1;
  return 0;
}

/** Return true iff <b>sl</b> has some element E such that
 * !strcmp(E,<b>element</b>)
 */
int
smartlist_string_isin(const smartlist_t *sl, const char *element)
{
  int i;
  if (!sl) return 0;
  for (i=0; i < sl->num_used; i++)
    if (strcmp((const char*)sl->list[i],element)==0)
      return 1;
  return 0;
}

/** If <b>element</b> is equal to an element of <b>sl</b>, return that
 * element's index.  Otherwise, return -1. */
int
smartlist_string_pos(const smartlist_t *sl, const char *element)
{
  int i;
  if (!sl) return -1;
  for (i=0; i < sl->num_used; i++)
    if (strcmp((const char*)sl->list[i],element)==0)
      return i;
  return -1;
}

/** Return true iff <b>sl</b> has some element E such that
 * !strcasecmp(E,<b>element</b>)
 */
int
smartlist_string_isin_case(const smartlist_t *sl, const char *element)
{
  int i;
  if (!sl) return 0;
  for (i=0; i < sl->num_used; i++)
    if (strcasecmp((const char*)sl->list[i],element)==0)
      return 1;
  return 0;
}

/** Return true iff <b>sl</b> has some element E such that E is equal
 * to the decimal encoding of <b>num</b>.
 */
int
smartlist_string_num_isin(const smartlist_t *sl, int num)
{
  char buf[32]; /* long enough for 64-bit int, and then some. */
  obfs_snprintf(buf,sizeof(buf),"%d", num);
  return smartlist_string_isin(sl, buf);
}

/** Return true iff the two lists contain the same strings in the same
 * order, or if they are both NULL. */
int
smartlist_strings_eq(const smartlist_t *sl1, const smartlist_t *sl2)
{
  if (sl1 == NULL)
    return sl2 == NULL;
  if (sl2 == NULL)
    return 0;
  if (smartlist_len(sl1) != smartlist_len(sl2))
    return 0;
  SMARTLIST_FOREACH(sl1, const char *, cp1, {
      const char *cp2 = smartlist_get(sl2, cp1_sl_idx);
      if (strcmp(cp1, cp2))
        return 0;
    });
  return 1;
}

/** Return true iff <b>sl</b> has some element E such that
 * !memcmp(E,<b>element</b>,SHA256_LENGTH)
 */
int
smartlist_digest_isin(const smartlist_t *sl, const char *element)
{
  int i;
  if (!sl) return 0;
  for (i=0; i < sl->num_used; i++)
    if (!memcmp((const char*)sl->list[i],element,SHA256_LENGTH))
      return 1;
  return 0;
}

/** Return true iff some element E of sl2 has smartlist_isin(sl1,E).
 */
int
smartlist_overlap(const smartlist_t *sl1, const smartlist_t *sl2)
{
  int i;
  for (i=0; i < sl2->num_used; i++)
    if (smartlist_isin(sl1, sl2->list[i]))
      return 1;
  return 0;
}

/** Remove every element E of sl1 such that !smartlist_isin(sl2,E).
 * Does not preserve the order of sl1.
 */
void
smartlist_intersect(smartlist_t *sl1, const smartlist_t *sl2)
{
  int i;
  for (i=0; i < sl1->num_used; i++)
    if (!smartlist_isin(sl2, sl1->list[i])) {
      sl1->list[i] = sl1->list[--sl1->num_used]; /* swap with the end */
      i--; /* so we process the new i'th element */
    }
}

/** Remove every element E of sl1 such that smartlist_isin(sl2,E).
 * Does not preserve the order of sl1.
 */
void
smartlist_subtract(smartlist_t *sl1, const smartlist_t *sl2)
{
  int i;
  for (i=0; i < sl2->num_used; i++)
    smartlist_remove(sl1, sl2->list[i]);
}

/** Remove the <b>idx</b>th element of sl; if idx is not the last
 * element, swap the last element of sl into the <b>idx</b>th space.
 */
void
smartlist_del(smartlist_t *sl, int idx)
{
  obfs_assert(sl);
  obfs_assert(idx>=0);
  obfs_assert(idx < sl->num_used);
  sl->list[idx] = sl->list[--sl->num_used];
}

/** Remove the <b>idx</b>th element of sl; if idx is not the last element,
 * moving all subsequent elements back one space. Return the old value
 * of the <b>idx</b>th element.
 */
void
smartlist_del_keeporder(smartlist_t *sl, int idx)
{
  obfs_assert(sl);
  obfs_assert(idx>=0);
  obfs_assert(idx < sl->num_used);
  --sl->num_used;
  if (idx < sl->num_used)
    memmove(sl->list+idx, sl->list+idx+1, sizeof(void*)*(sl->num_used-idx));
}

/** Insert the value <b>val</b> as the new <b>idx</b>th element of
 * <b>sl</b>, moving all items previously at <b>idx</b> or later
 * forward one space.
 */
void
smartlist_insert(smartlist_t *sl, int idx, void *val)
{
  obfs_assert(sl);
  obfs_assert(idx>=0);
  obfs_assert(idx <= sl->num_used);
  if (idx == sl->num_used) {
    smartlist_add(sl, val);
  } else {
    smartlist_ensure_capacity(sl, sl->num_used+1);
    /* Move other elements away */
    if (idx < sl->num_used)
      memmove(sl->list + idx + 1, sl->list + idx,
              sizeof(void*)*(sl->num_used-idx));
    sl->num_used++;
    sl->list[idx] = val;
  }
}

/**
 * Split a string <b>str</b> along all occurrences of <b>sep</b>,
 * appending the (newly allocated) split strings, in order, to
 * <b>sl</b>.  Return the number of strings added to <b>sl</b>.
 *
 * If <b>flags</b>&amp;SPLIT_SKIP_SPACE is true, remove initial and
 * trailing space from each entry.
 * If <b>flags</b>&amp;SPLIT_IGNORE_BLANK is true, remove any entries
 * of length 0.
 * If <b>flags</b>&amp;SPLIT_STRIP_SPACE is true, strip spaces from each
 * split string.
 *
 * If <b>max</b>\>0, divide the string into no more than <b>max</b> pieces. If
 * <b>sep</b> is NULL, split on any sequence of horizontal space.
 */
int
smartlist_split_string(smartlist_t *sl, const char *str, const char *sep,
                       int flags, int max)
{
  const char *cp, *end, *next;
  int n = 0;

  obfs_assert(sl);
  obfs_assert(str);

  cp = str;
  while (1) {
    if (flags&SPLIT_SKIP_SPACE) {
      while (ascii_isspace(*cp)) ++cp;
    }

    if (max>0 && n == max-1) {
      end = strchr(cp,'\0');
    } else if (sep) {
      end = strstr(cp,sep);
      if (!end)
        end = strchr(cp,'\0');
    } else {
      for (end = cp; *end && *end != '\t' && *end != ' '; ++end)
        ;
    }

    obfs_assert(end);

    if (!*end) {
      next = NULL;
    } else if (sep) {
      next = end+strlen(sep);
    } else {
      next = end+1;
      while (*next == '\t' || *next == ' ')
        ++next;
    }

    if (flags&SPLIT_SKIP_SPACE) {
      while (end > cp && ascii_isspace(*(end-1)))
        --end;
    }
    if (end != cp || !(flags&SPLIT_IGNORE_BLANK)) {
      char *string = xstrndup(cp, end-cp);
      if (flags&SPLIT_STRIP_SPACE)
        ascii_strstrip(string, " ");
      smartlist_add(sl, string);
      ++n;
    }
    if (!next)
      break;
    cp = next;
  }

  return n;
}

/** Allocate and return a new string containing the concatenation of
 * the elements of <b>sl</b>, in order, separated by <b>join</b>.  If
 * <b>terminate</b> is true, also terminate the string with <b>join</b>.
 * If <b>len_out</b> is not NULL, set <b>len_out</b> to the length of
 * the returned string. Requires that every element of <b>sl</b> is
 * NUL-terminated string.
 */
char *
smartlist_join_strings(smartlist_t *sl, const char *join,
                       int terminate, size_t *len_out)
{
  return smartlist_join_strings2(sl,join,strlen(join),terminate,len_out);
}

/** As smartlist_join_strings, but instead of separating/terminated with a
 * NUL-terminated string <b>join</b>, uses the <b>join_len</b>-byte sequence
 * at <b>join</b>.  (Useful for generating a sequence of NUL-terminated
 * strings.)
 */
char *
smartlist_join_strings2(smartlist_t *sl, const char *join,
                        size_t join_len, int terminate, size_t *len_out)
{
  int i;
  size_t n = 0;
  char *r = NULL, *dst, *src;

  obfs_assert(sl);
  obfs_assert(join);

  if (terminate)
    n = join_len;

  for (i = 0; i < sl->num_used; ++i) {
    n += strlen(sl->list[i]);
    if (i+1 < sl->num_used) /* avoid double-counting the last one */
      n += join_len;
  }
  dst = r = xmalloc(n+1);
  for (i = 0; i < sl->num_used; ) {
    for (src = sl->list[i]; *src; )
      *dst++ = *src++;
    if (++i < sl->num_used) {
      memcpy(dst, join, join_len);
      dst += join_len;
    }
  }
  if (terminate) {
    memcpy(dst, join, join_len);
    dst += join_len;
  }
  *dst = '\0';

  if (len_out)
    *len_out = dst-r;
  return r;
}

/** Sort the members of <b>sl</b> into an order defined by
 * the ordering function <b>compare</b>, which returns less then 0 if a
 * precedes b, greater than 0 if b precedes a, and 0 if a 'equals' b.
 */
void
smartlist_sort(smartlist_t *sl, int (*compare)(const void **a, const void **b))
{
  if (!sl->num_used)
    return;
  qsort(sl->list, sl->num_used, sizeof(void*),
        (int (*)(const void *,const void*))compare);
}

/** Given a smartlist <b>sl</b> sorted with the function <b>compare</b>,
 * return the most frequent member in the list.  Break ties in favor of
 * later elements.  If the list is empty, return NULL.
 */
void *
smartlist_get_most_frequent(const smartlist_t *sl,
                            int (*compare)(const void **a, const void **b))
{
  const void *most_frequent = NULL;
  int most_frequent_count = 0;

  const void *cur = NULL;
  int i, count=0;

  if (!sl->num_used)
    return NULL;
  for (i = 0; i < sl->num_used; ++i) {
    const void *item = sl->list[i];
    if (cur && 0 == compare(&cur, &item)) {
      ++count;
    } else {
      if (cur && count >= most_frequent_count) {
        most_frequent = cur;
        most_frequent_count = count;
      }
      cur = item;
      count = 1;
    }
  }
  if (cur && count >= most_frequent_count) {
    most_frequent = cur;
    most_frequent_count = count;
  }
  return (void*)most_frequent;
}

/** Given a sorted smartlist <b>sl</b> and the comparison function used to
 * sort it, remove all duplicate members.  If free_fn is provided, calls
 * free_fn on each duplicate.  Otherwise, just removes them.  Preserves order.
 */
void
smartlist_uniq(smartlist_t *sl,
               int (*compare)(const void **a, const void **b),
               void (*free_fn)(void *a))
{
  int i;
  for (i=1; i < sl->num_used; ++i) {
    if (compare((const void **)&(sl->list[i-1]),
                (const void **)&(sl->list[i])) == 0) {
      if (free_fn)
        free_fn(sl->list[i]);
      smartlist_del_keeporder(sl, i--);
    }
  }
}

/** Return a randomly chosen element of <b>sl</b>; or NULL if <b>sl</b>
 * is empty. */

void *
smartlist_choose(const smartlist_t *sl)
{
  int len = smartlist_len(sl);
  if (len)
    return smartlist_get(sl, random_int(len));
  return NULL; /* no elements to choose from */
}

/** Scramble the elements of <b>sl</b> into a random order. */
void
smartlist_shuffle(smartlist_t *sl)
{
  int i;

  /* From the end of the list to the front, choose at random from the
     positions we haven't looked at yet, and swap that position into the
     current position.  Remember to give "no swap" the same probability as
     any other swap. */
  for (i = smartlist_len(sl)-1; i > 0; --i) {
    int j = random_int(i+1);
    smartlist_swap(sl, i, j);
  }
}


/** Assuming the members of <b>sl</b> are in order, return a pointer to the
 * member that matches <b>key</b>.  Ordering and matching are defined by a
 * <b>compare</b> function that returns 0 on a match; less than 0 if key is
 * less than member, and greater than 0 if key is greater then member.
 */
void *
smartlist_bsearch(smartlist_t *sl, const void *key,
                  int (*compare)(const void *key, const void **member))
{
  int found, idx;
  idx = smartlist_bsearch_idx(sl, key, compare, &found);
  return found ? smartlist_get(sl, idx) : NULL;
}

/** Assuming the members of <b>sl</b> are in order, return the index of the
 * member that matches <b>key</b>.  If no member matches, return the index of
 * the first member greater than <b>key</b>, or smartlist_len(sl) if no member
 * is greater than <b>key</b>.  Set <b>found_out</b> to true on a match, to
 * false otherwise.  Ordering and matching are defined by a <b>compare</b>
 * function that returns 0 on a match; less than 0 if key is less than member,
 * and greater than 0 if key is greater then member.
 */
int
smartlist_bsearch_idx(const smartlist_t *sl, const void *key,
                      int (*compare)(const void *key, const void **member),
                      int *found_out)
{
  int hi = smartlist_len(sl) - 1, lo = 0, cmp, mid;

  while (lo <= hi) {
    mid = (lo + hi) / 2;
    cmp = compare(key, (const void**) &(sl->list[mid]));
    if (cmp>0) { /* key > sl[mid] */
      lo = mid+1;
    } else if (cmp<0) { /* key < sl[mid] */
      hi = mid-1;
    } else { /* key == sl[mid] */
      *found_out = 1;
      return mid;
    }
  }
  /* lo > hi. */
  {
    obfs_assert(lo >= 0);
    if (lo < smartlist_len(sl)) {
      cmp = compare(key, (const void**) &(sl->list[lo]));
      obfs_assert(cmp < 0);
    } else if (smartlist_len(sl)) {
      cmp = compare(key, (const void**) &(sl->list[smartlist_len(sl)-1]));
      obfs_assert(cmp > 0);
    }
  }
  *found_out = 0;
  return lo;
}

/** Helper: compare two const char **s. */
static int
_compare_string_ptrs(const void **_a, const void **_b)
{
  return strcmp((const char*)*_a, (const char*)*_b);
}

/** Sort a smartlist <b>sl</b> containing strings into lexically ascending
 * order. */
void
smartlist_sort_strings(smartlist_t *sl)
{
  smartlist_sort(sl, _compare_string_ptrs);
}

/** Return the most frequent string in the sorted list <b>sl</b> */
char *
smartlist_get_most_frequent_string(smartlist_t *sl)
{
  return smartlist_get_most_frequent(sl, _compare_string_ptrs);
}

/** Remove duplicate strings from a sorted list, and free them with free().
 */
void
smartlist_uniq_strings(smartlist_t *sl)
{
  smartlist_uniq(sl, _compare_string_ptrs, free);
}

/* Heap-based priority queue implementation for O(lg N) insert and remove.
 * Recall that the heap property is that, for every index I, h[I] <
 * H[LEFT_CHILD[I]] and h[I] < H[RIGHT_CHILD[I]].
 *
 * For us to remove items other than the topmost item, each item must store
 * its own index within the heap.  When calling the pqueue functions, tell
 * them about the offset of the field that stores the index within the item.
 *
 * Example:
 *
 *   typedef struct timer_t {
 *     struct timeval tv;
 *     int heap_index;
 *   } timer_t;
 *
 *   static int compare(const void *p1, const void *p2) {
 *     const timer_t *t1 = p1, *t2 = p2;
 *     if (t1->tv.tv_sec < t2->tv.tv_sec) {
 *        return -1;
 *     } else if (t1->tv.tv_sec > t2->tv.tv_sec) {
 *        return 1;
 *     } else {
 *        return t1->tv.tv_usec - t2->tv_usec;
 *     }
 *   }
 *
 *   void timer_heap_insert(smartlist_t *heap, timer_t *timer) {
 *      smartlist_pqueue_add(heap, compare, STRUCT_OFFSET(timer_t, heap_index),
 *         timer);
 *   }
 *
 *   void timer_heap_pop(smartlist_t *heap) {
 *      return smartlist_pqueue_pop(heap, compare,
 *         STRUCT_OFFSET(timer_t, heap_index));
 *   }
 */

/** @{ */
/** Functions to manipulate heap indices to find a node's parent and children.
 *
 * For a 1-indexed array, we would use LEFT_CHILD[x] = 2*x and RIGHT_CHILD[x]
 *   = 2*x + 1.  But this is C, so we have to adjust a little. */
//#define LEFT_CHILD(i)  ( ((i)+1)*2 - 1)
//#define RIGHT_CHILD(i) ( ((i)+1)*2 )
//#define PARENT(i)      ( ((i)+1)/2 - 1)
#define LEFT_CHILD(i)  ( 2*(i) + 1 )
#define RIGHT_CHILD(i) ( 2*(i) + 2 )
#define PARENT(i)      ( ((i)-1) / 2 )
/** }@ */

/** @{ */
/** Helper macros for heaps: Given a local variable <b>idx_field_offset</b>
 * set to the offset of an integer index within the heap element structure,
 * IDX_OF_ITEM(p) gives you the index of p, and IDXP(p) gives you a pointer to
 * where p's index is stored.  Given additionally a local smartlist <b>sl</b>,
 * UPDATE_IDX(i) sets the index of the element at <b>i</b> to the correct
 * value (that is, to <b>i</b>).
 */
#define IDXP(p) ((int*)( ((char*)(p)) + idx_field_offset ))

#define UPDATE_IDX(i)  do {                            \
    void *updated = sl->list[i];                       \
    *IDXP(updated) = i;                                \
  } while (0)

#define IDX_OF_ITEM(p) (*IDXP(p))
/** @} */

/** Helper. <b>sl</b> may have at most one violation of the heap property:
 * the item at <b>idx</b> may be greater than one or both of its children.
 * Restore the heap property. */
static inline void
smartlist_heapify(smartlist_t *sl,
                  int (*compare)(const void *a, const void *b),
                  int idx_field_offset,
                  int idx)
{
  while (1) {
    int left_idx = LEFT_CHILD(idx);
    int best_idx;

    if (left_idx >= sl->num_used)
      return;
    if (compare(sl->list[idx],sl->list[left_idx]) < 0)
      best_idx = idx;
    else
      best_idx = left_idx;
    if (left_idx+1 < sl->num_used &&
        compare(sl->list[left_idx+1],sl->list[best_idx]) < 0)
      best_idx = left_idx + 1;

    if (best_idx == idx) {
      return;
    } else {
      void *tmp = sl->list[idx];
      sl->list[idx] = sl->list[best_idx];
      sl->list[best_idx] = tmp;
      UPDATE_IDX(idx);
      UPDATE_IDX(best_idx);

      idx = best_idx;
    }
  }
}

/** Insert <b>item</b> into the heap stored in <b>sl</b>, where order is
 * determined by <b>compare</b> and the offset of the item in the heap is
 * stored in an int-typed field at position <b>idx_field_offset</b> within
 * item.
 */
void
smartlist_pqueue_add(smartlist_t *sl,
                     int (*compare)(const void *a, const void *b),
                     int idx_field_offset,
                     void *item)
{
  int idx;
  smartlist_add(sl,item);
  UPDATE_IDX(sl->num_used-1);

  for (idx = sl->num_used - 1; idx; ) {
    int parent = PARENT(idx);
    if (compare(sl->list[idx], sl->list[parent]) < 0) {
      void *tmp = sl->list[parent];
      sl->list[parent] = sl->list[idx];
      sl->list[idx] = tmp;
      UPDATE_IDX(parent);
      UPDATE_IDX(idx);
      idx = parent;
    } else {
      return;
    }
  }
}

/** Remove and return the top-priority item from the heap stored in <b>sl</b>,
 * where order is determined by <b>compare</b> and the item's position is
 * stored at position <b>idx_field_offset</b> within the item.  <b>sl</b> must
 * not be empty. */
void *
smartlist_pqueue_pop(smartlist_t *sl,
                     int (*compare)(const void *a, const void *b),
                     int idx_field_offset)
{
  void *top;
  obfs_assert(sl->num_used);

  top = sl->list[0];
  *IDXP(top)=-1;
  if (--sl->num_used) {
    sl->list[0] = sl->list[sl->num_used];
    UPDATE_IDX(0);
    smartlist_heapify(sl, compare, idx_field_offset, 0);
  }
  return top;
}

/** Remove the item <b>item</b> from the heap stored in <b>sl</b>,
 * where order is determined by <b>compare</b> and the item's position is
 * stored at position <b>idx_field_offset</b> within the item.  <b>sl</b> must
 * not be empty. */
void
smartlist_pqueue_remove(smartlist_t *sl,
                        int (*compare)(const void *a, const void *b),
                        int idx_field_offset,
                        void *item)
{
  int idx = IDX_OF_ITEM(item);
  obfs_assert(idx >= 0);
  obfs_assert(sl->list[idx] == item);
  --sl->num_used;
  *IDXP(item) = -1;
  if (idx == sl->num_used) {
    return;
  } else {
    sl->list[idx] = sl->list[sl->num_used];
    UPDATE_IDX(idx);
    smartlist_heapify(sl, compare, idx_field_offset, idx);
  }
}

/** Assert that the heap property is correctly maintained by the heap stored
 * in <b>sl</b>, where order is determined by <b>compare</b>. */
void
smartlist_pqueue_assert_ok(smartlist_t *sl,
                           int (*compare)(const void *a, const void *b),
                           int idx_field_offset)
{
  int i;
  for (i = sl->num_used - 1; i >= 0; --i) {
    if (i>0)
      obfs_assert(compare(sl->list[PARENT(i)], sl->list[i]) <= 0);
    obfs_assert(IDX_OF_ITEM(sl->list[i]) == i);
  }
}

/** Helper: compare two SHA256_LENGTH digests. */
static int
_compare_digests(const void **_a, const void **_b)
{
  return memcmp((const char*)*_a, (const char*)*_b, SHA256_LENGTH);
}

/** Sort the list of SHA256_LENGTH-byte digests into ascending order. */
void
smartlist_sort_digests(smartlist_t *sl)
{
  smartlist_sort(sl, _compare_digests);
}

/** Remove duplicate digests from a sorted list, and free them with free().
 */
void
smartlist_uniq_digests(smartlist_t *sl)
{
  smartlist_uniq(sl, _compare_digests, free);
}

/** Helper: Declare an entry type and a map type to implement a mapping using
 * ht.h.  The map type will be called <b>maptype</b>.  The key part of each
 * entry is declared using the C declaration <b>keydecl</b>.  All functions
 * and types associated with the map get prefixed with <b>prefix</b> */
#define DEFINE_MAP_STRUCTS(maptype, keydecl, prefix)      \
  typedef struct prefix ## entry_t {                      \
    HT_ENTRY(prefix ## entry_t) node;                     \
    void *val;                                            \
    keydecl;                                              \
  } prefix ## entry_t;                                    \
  struct maptype {                                        \
    HT_HEAD(prefix ## impl, prefix ## entry_t) head;      \
  }

DEFINE_MAP_STRUCTS(strmap_t, char *key, strmap_);
DEFINE_MAP_STRUCTS(digestmap_t, char key[SHA256_LENGTH], digestmap_);

/** Helper: compare strmap_entry_t objects by key value. */
static inline int
strmap_entries_eq(const strmap_entry_t *a, const strmap_entry_t *b)
{
  return !strcmp(a->key, b->key);
}

/** Helper: return a hash value for a strmap_entry_t. */
static inline unsigned int
strmap_entry_hash(const strmap_entry_t *a)
{
  return ht_string_hash(a->key);
}

/** Helper: compare digestmap_entry_t objects by key value. */
static inline int
digestmap_entries_eq(const digestmap_entry_t *a, const digestmap_entry_t *b)
{
  return !memcmp(a->key, b->key, SHA256_LENGTH);
}

/** Helper: return a hash value for a digest_map_t. */
static inline unsigned int
digestmap_entry_hash(const digestmap_entry_t *a)
{
#if SIZEOF_INT != 8
  const uint32_t *p = (const uint32_t*)a->key;
  return p[0] ^ p[1] ^ p[2] ^ p[3] ^ p[4];
#else
  const uint64_t *p = (const uint64_t*)a->key;
  return p[0] ^ p[1];
#endif
}

HT_PROTOTYPE(strmap_impl, strmap_entry_t, node, strmap_entry_hash,
             strmap_entries_eq)
HT_GENERATE(strmap_impl, strmap_entry_t, node, strmap_entry_hash,
            strmap_entries_eq, 0.6, malloc, realloc, free)

HT_PROTOTYPE(digestmap_impl, digestmap_entry_t, node, digestmap_entry_hash,
             digestmap_entries_eq)
HT_GENERATE(digestmap_impl, digestmap_entry_t, node, digestmap_entry_hash,
            digestmap_entries_eq, 0.6, malloc, realloc, free)

/** Constructor to create a new empty map from strings to void*'s.
 */
strmap_t *
strmap_new(void)
{
  strmap_t *result;
  result = xmalloc(sizeof(strmap_t));
  HT_INIT(strmap_impl, &result->head);
  return result;
}

/** Constructor to create a new empty map from digests to void*'s.
 */
digestmap_t *
digestmap_new(void)
{
  digestmap_t *result;
  result = xmalloc(sizeof(digestmap_t));
  HT_INIT(digestmap_impl, &result->head);
  return result;
}

/** Set the current value for <b>key</b> to <b>val</b>.  Returns the previous
 * value for <b>key</b> if one was set, or NULL if one was not.
 *
 * This function makes a copy of <b>key</b> if necessary, but not of
 * <b>val</b>.
 */
void *
strmap_set(strmap_t *map, const char *key, void *val)
{
  strmap_entry_t *resolve;
  strmap_entry_t search;
  void *oldval;
  obfs_assert(map);
  obfs_assert(key);
  obfs_assert(val);
  search.key = (char*)key;
  resolve = HT_FIND(strmap_impl, &map->head, &search);
  if (resolve) {
    oldval = resolve->val;
    resolve->val = val;
    return oldval;
  } else {
    resolve = xzalloc(sizeof(strmap_entry_t));
    resolve->key = xstrdup(key);
    resolve->val = val;
    obfs_assert(!HT_FIND(strmap_impl, &map->head, resolve));
    HT_INSERT(strmap_impl, &map->head, resolve);
    return NULL;
  }
}

#define OPTIMIZED_DIGESTMAP_SET

/** Like strmap_set() above but for digestmaps. */
void *
digestmap_set(digestmap_t *map, const char *key, void *val)
{
#ifndef OPTIMIZED_DIGESTMAP_SET
  digestmap_entry_t *resolve;
#endif
  digestmap_entry_t search;
  void *oldval;
  obfs_assert(map);
  obfs_assert(key);
  obfs_assert(val);
  memcpy(&search.key, key, SHA256_LENGTH);
#ifndef OPTIMIZED_DIGESTMAP_SET
  resolve = HT_FIND(digestmap_impl, &map->head, &search);
  if (resolve) {
    oldval = resolve->val;
    resolve->val = val;
    return oldval;
  } else {
    resolve = xzalloc(sizeof(digestmap_entry_t));
    memcpy(resolve->key, key, SHA256_LENGTH);
    resolve->val = val;
    HT_INSERT(digestmap_impl, &map->head, resolve);
    return NULL;
  }
#else
  /* We spend up to 5% of our time in this function, so the code below is
   * meant to optimize the check/alloc/set cycle by avoiding the two trips to
   * the hash table that we do in the unoptimized code above.  (Each of
   * HT_INSERT and HT_FIND calls HT_SET_HASH and HT_FIND_P.)
   */
  _HT_FIND_OR_INSERT(digestmap_impl, node, digestmap_entry_hash, &(map->head),
         digestmap_entry_t, &search, ptr,
         {
            /* we found an entry. */
            oldval = (*ptr)->val;
            (*ptr)->val = val;
            return oldval;
         },
         {
           /* We didn't find the entry. */
           digestmap_entry_t *newent =
             xzalloc(sizeof(digestmap_entry_t));
           memcpy(newent->key, key, SHA256_LENGTH);
           newent->val = val;
           _HT_FOI_INSERT(node, &(map->head), &search, newent, ptr);
           return NULL;
         });
#endif
}

/** Return the current value associated with <b>key</b>, or NULL if no
 * value is set.
 */
void *
strmap_get(const strmap_t *map, const char *key)
{
  strmap_entry_t *resolve;
  strmap_entry_t search;
  obfs_assert(map);
  obfs_assert(key);
  search.key = (char*)key;
  resolve = HT_FIND(strmap_impl, &map->head, &search);
  if (resolve) {
    return resolve->val;
  } else {
    return NULL;
  }
}

/** Like strmap_get() above but for digestmaps. */
void *
digestmap_get(const digestmap_t *map, const char *key)
{
  digestmap_entry_t *resolve;
  digestmap_entry_t search;
  obfs_assert(map);
  obfs_assert(key);
  memcpy(&search.key, key, SHA256_LENGTH);
  resolve = HT_FIND(digestmap_impl, &map->head, &search);
  if (resolve) {
    return resolve->val;
  } else {
    return NULL;
  }
}

/** Remove the value currently associated with <b>key</b> from the map.
 * Return the value if one was set, or NULL if there was no entry for
 * <b>key</b>.
 *
 * Note: you must free any storage associated with the returned value.
 */
void *
strmap_remove(strmap_t *map, const char *key)
{
  strmap_entry_t *resolve;
  strmap_entry_t search;
  void *oldval;
  obfs_assert(map);
  obfs_assert(key);
  search.key = (char*)key;
  resolve = HT_REMOVE(strmap_impl, &map->head, &search);
  if (resolve) {
    oldval = resolve->val;
    free(resolve->key);
    free(resolve);
    return oldval;
  } else {
    return NULL;
  }
}

/** Like strmap_remove() above but for digestmaps. */
void *
digestmap_remove(digestmap_t *map, const char *key)
{
  digestmap_entry_t *resolve;
  digestmap_entry_t search;
  void *oldval;
  obfs_assert(map);
  obfs_assert(key);
  memcpy(&search.key, key, SHA256_LENGTH);
  resolve = HT_REMOVE(digestmap_impl, &map->head, &search);
  if (resolve) {
    oldval = resolve->val;
    free(resolve);
    return oldval;
  } else {
    return NULL;
  }
}

/** Same as strmap_set, but first converts <b>key</b> to lowercase. */
void *
strmap_set_lc(strmap_t *map, const char *key, void *val)
{
  /* We could be a little faster by using strcasecmp instead, and a separate
   * type, but I don't think it matters. */
  void *v;
  char *lc_key = xstrdup(key);
  ascii_strlower(lc_key);
  v = strmap_set(map,lc_key,val);
  free(lc_key);
  return v;
}

/** Same as strmap_get, but first converts <b>key</b> to lowercase. */
void *
strmap_get_lc(const strmap_t *map, const char *key)
{
  void *v;
  char *lc_key = xstrdup(key);
  ascii_strlower(lc_key);
  v = strmap_get(map,lc_key);
  free(lc_key);
  return v;
}

/** Same as strmap_remove, but first converts <b>key</b> to lowercase */
void *
strmap_remove_lc(strmap_t *map, const char *key)
{
  void *v;
  char *lc_key = xstrdup(key);
  ascii_strlower(lc_key);
  v = strmap_remove(map,lc_key);
  free(lc_key);
  return v;
}

/** return an <b>iterator</b> pointer to the front of a map.
 *
 * Iterator example:
 *
 * \code
 * // uppercase values in "map", removing empty values.
 *
 * strmap_iter_t *iter;
 * const char *key;
 * void *val;
 * char *cp;
 *
 * for (iter = strmap_iter_init(map); !strmap_iter_done(iter); ) {
 *    strmap_iter_get(iter, &key, &val);
 *    cp = (char*)val;
 *    if (!*cp) {
 *       iter = strmap_iter_next_rmv(map,iter);
 *       free(val);
 *    } else {
 *       for (;*cp;cp++) *cp = TOR_TOUPPER(*cp);
 *       iter = strmap_iter_next(map,iter);
 *    }
 * }
 * \endcode
 *
 */
strmap_iter_t *
strmap_iter_init(strmap_t *map)
{
  obfs_assert(map);
  return HT_START(strmap_impl, &map->head);
}

/** Start iterating through <b>map</b>.  See strmap_iter_init() for example. */
digestmap_iter_t *
digestmap_iter_init(digestmap_t *map)
{
  obfs_assert(map);
  return HT_START(digestmap_impl, &map->head);
}

/** Advance the iterator <b>iter</b> for <b>map</b> a single step to the next
 * entry, and return its new value. */
strmap_iter_t *
strmap_iter_next(strmap_t *map, strmap_iter_t *iter)
{
  obfs_assert(map);
  obfs_assert(iter);
  return HT_NEXT(strmap_impl, &map->head, iter);
}

/** Advance the iterator <b>iter</b> for map a single step to the next entry,
 * and return its new value. */
digestmap_iter_t *
digestmap_iter_next(digestmap_t *map, digestmap_iter_t *iter)
{
  obfs_assert(map);
  obfs_assert(iter);
  return HT_NEXT(digestmap_impl, &map->head, iter);
}

/** Advance the iterator <b>iter</b> a single step to the next entry, removing
 * the current entry, and return its new value.
 */
strmap_iter_t *
strmap_iter_next_rmv(strmap_t *map, strmap_iter_t *iter)
{
  strmap_entry_t *rmv;
  obfs_assert(map);
  obfs_assert(iter);
  obfs_assert(*iter);
  rmv = *iter;
  iter = HT_NEXT_RMV(strmap_impl, &map->head, iter);
  free(rmv->key);
  free(rmv);
  return iter;
}

/** Advance the iterator <b>iter</b> a single step to the next entry, removing
 * the current entry, and return its new value.
 */
digestmap_iter_t *
digestmap_iter_next_rmv(digestmap_t *map, digestmap_iter_t *iter)
{
  digestmap_entry_t *rmv;
  obfs_assert(map);
  obfs_assert(iter);
  obfs_assert(*iter);
  rmv = *iter;
  iter = HT_NEXT_RMV(digestmap_impl, &map->head, iter);
  free(rmv);
  return iter;
}

/** Set *<b>keyp</b> and *<b>valp</b> to the current entry pointed to by
 * iter. */
void
strmap_iter_get(strmap_iter_t *iter, const char **keyp, void **valp)
{
  obfs_assert(iter);
  obfs_assert(*iter);
  obfs_assert(keyp);
  obfs_assert(valp);
  *keyp = (*iter)->key;
  *valp = (*iter)->val;
}

/** Set *<b>keyp</b> and *<b>valp</b> to the current entry pointed to by
 * iter. */
void
digestmap_iter_get(digestmap_iter_t *iter, const char **keyp, void **valp)
{
  obfs_assert(iter);
  obfs_assert(*iter);
  obfs_assert(keyp);
  obfs_assert(valp);
  *keyp = (*iter)->key;
  *valp = (*iter)->val;
}

/** Return true iff <b>iter</b> has advanced past the last entry of
 * <b>map</b>. */
int
strmap_iter_done(strmap_iter_t *iter)
{
  return iter == NULL;
}

/** Return true iff <b>iter</b> has advanced past the last entry of
 * <b>map</b>. */
int
digestmap_iter_done(digestmap_iter_t *iter)
{
  return iter == NULL;
}

/** Remove all entries from <b>map</b>, and deallocate storage for those
 * entries.  If free_val is provided, it is invoked on every value in
 * <b>map</b>.
 */
void
strmap_free(strmap_t *map, void (*free_val)(void*))
{
  strmap_entry_t **ent, **next, *this;
  if (!map)
    return;

  for (ent = HT_START(strmap_impl, &map->head); ent != NULL; ent = next) {
    this = *ent;
    next = HT_NEXT_RMV(strmap_impl, &map->head, ent);
    free(this->key);
    if (free_val)
      free_val(this->val);
    free(this);
  }
  obfs_assert(HT_EMPTY(&map->head));
  HT_CLEAR(strmap_impl, &map->head);
  free(map);
}

/** Remove all entries from <b>map</b>, and deallocate storage for those
 * entries.  If free_val is provided, it is invoked on every value in
 * <b>map</b>.
 */
void
digestmap_free(digestmap_t *map, void (*free_val)(void*))
{
  digestmap_entry_t **ent, **next, *this;
  if (!map)
    return;
  for (ent = HT_START(digestmap_impl, &map->head); ent != NULL; ent = next) {
    this = *ent;
    next = HT_NEXT_RMV(digestmap_impl, &map->head, ent);
    if (free_val)
      free_val(this->val);
    free(this);
  }
  obfs_assert(HT_EMPTY(&map->head));
  HT_CLEAR(digestmap_impl, &map->head);
  free(map);
}

/** Fail with an assertion error if anything has gone wrong with the internal
 * representation of <b>map</b>. */
void
strmap_assert_ok(const strmap_t *map)
{
  obfs_assert(!_strmap_impl_HT_REP_IS_BAD(&map->head));
}
/** Fail with an assertion error if anything has gone wrong with the internal
 * representation of <b>map</b>. */
void
digestmap_assert_ok(const digestmap_t *map)
{
  obfs_assert(!_digestmap_impl_HT_REP_IS_BAD(&map->head));
}

/** Return true iff <b>map</b> has no entries. */
int
strmap_isempty(const strmap_t *map)
{
  return HT_EMPTY(&map->head);
}

/** Return true iff <b>map</b> has no entries. */
int
digestmap_isempty(const digestmap_t *map)
{
  return HT_EMPTY(&map->head);
}

/** Return the number of items in <b>map</b>. */
int
strmap_size(const strmap_t *map)
{
  return HT_SIZE(&map->head);
}

/** Return the number of items in <b>map</b>. */
int
digestmap_size(const digestmap_t *map)
{
  return HT_SIZE(&map->head);
}

/** Declare a function called <b>funcname</b> that acts as a find_nth_FOO
 * function for an array of type <b>elt_t</b>*.
 *
 * NOTE: The implementation kind of sucks: It's O(n log n), whereas finding
 * the kth element of an n-element list can be done in O(n).  Then again, this
 * implementation is not in critical path, and it is obviously correct. */
#define IMPLEMENT_ORDER_FUNC(funcname, elt_t)                   \
  static int                                                    \
  _cmp_ ## elt_t(const void *_a, const void *_b)                \
  {                                                             \
    const elt_t *a = _a, *b = _b;                               \
    if (*a<*b)                                                  \
      return -1;                                                \
    else if (*a>*b)                                             \
      return 1;                                                 \
    else                                                        \
      return 0;                                                 \
  }                                                             \
  elt_t                                                         \
  funcname(elt_t *array, int n_elements, int nth)               \
  {                                                             \
    obfs_assert(nth >= 0);                                       \
    obfs_assert(nth < n_elements);                               \
    qsort(array, n_elements, sizeof(elt_t), _cmp_ ##elt_t);     \
    return array[nth];                                          \
  }

IMPLEMENT_ORDER_FUNC(find_nth_int, int)
IMPLEMENT_ORDER_FUNC(find_nth_time, time_t)
IMPLEMENT_ORDER_FUNC(find_nth_double, double)
IMPLEMENT_ORDER_FUNC(find_nth_uint32, uint32_t)
IMPLEMENT_ORDER_FUNC(find_nth_int32, int32_t)
IMPLEMENT_ORDER_FUNC(find_nth_long, long)

/** Return a newly allocated digestset_t, optimized to hold a total of
 * <b>max_elements</b> digests with a reasonably low false positive weight. */
digestset_t *
digestset_new(int max_elements)
{
  /* The probability of false positives is about P=(1 - exp(-kn/m))^k, where k
   * is the number of hash functions per entry, m is the bits in the array,
   * and n is the number of elements inserted.  For us, k==4, n<=max_elements,
   * and m==n_bits= approximately max_elements*32.  This gives
   *   P<(1-exp(-4*n/(32*n)))^4 == (1-exp(1/-8))^4 == .00019
   *
   * It would be more optimal in space vs false positives to get this false
   * positive rate by going for k==13, and m==18.5n, but we also want to
   * conserve CPU, and k==13 is pretty big.
   */
  int n_bits = 1u << (ui64_log2(max_elements)+5);
  digestset_t *r = xmalloc(sizeof(digestset_t));
  r->mask = n_bits - 1;
  r->ba = bitarray_init_zero(n_bits);
  return r;
}

/** Free all storage held in <b>set</b>. */
void
digestset_free(digestset_t *set)
{
  if (!set)
    return;
  bitarray_free(set->ba);
  free(set);
}
