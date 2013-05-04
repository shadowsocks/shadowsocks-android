/* Copyright (c) 2003-2004, Roger Dingledine
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2011, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file container.h
 * \brief Headers for container.c.
 **/

#ifndef CONTAINER_H
#define CONTAINER_H

#include <time.h>

/** A resizeable list of pointers, with associated helpful functionality.
 *
 * The members of this struct are exposed only so that macros and inlines can
 * use them; all access to smartlist internals should go through the functions
 * and macros defined here.
 **/
typedef struct smartlist_t {
  /** @{ */
  /** <b>list</b> has enough capacity to store exactly <b>capacity</b> elements
   * before it needs to be resized.  Only the first <b>num_used</b> (\<=
   * capacity) elements point to valid data.
   */
  void **list;
  int num_used;
  int capacity;
  /** @} */
} smartlist_t;

smartlist_t *smartlist_create(void);
void smartlist_free(smartlist_t *sl);
void smartlist_clear(smartlist_t *sl);
void smartlist_add(smartlist_t *sl, void *element);
void smartlist_add_all(smartlist_t *sl, const smartlist_t *s2);
void smartlist_remove(smartlist_t *sl, const void *element);
void *smartlist_pop_last(smartlist_t *sl);
void smartlist_reverse(smartlist_t *sl);
void smartlist_string_remove(smartlist_t *sl, const char *element);
int smartlist_isin(const smartlist_t *sl, const void *element) ATTR_PURE;
int smartlist_string_isin(const smartlist_t *sl, const char *element)
  ATTR_PURE;
int smartlist_string_pos(const smartlist_t *, const char *elt) ATTR_PURE;
int smartlist_string_isin_case(const smartlist_t *sl, const char *element)
  ATTR_PURE;
int smartlist_string_num_isin(const smartlist_t *sl, int num) ATTR_PURE;
int smartlist_strings_eq(const smartlist_t *sl1, const smartlist_t *sl2)
  ATTR_PURE;
int smartlist_digest_isin(const smartlist_t *sl, const char *element)
  ATTR_PURE;
int smartlist_overlap(const smartlist_t *sl1, const smartlist_t *sl2)
  ATTR_PURE;
void smartlist_intersect(smartlist_t *sl1, const smartlist_t *sl2);
void smartlist_subtract(smartlist_t *sl1, const smartlist_t *sl2);

#ifdef DEBUG_SMARTLIST
/** Return the number of items in sl.
 */
static inline int smartlist_len(const smartlist_t *sl) ATTR_PURE;
static inline int smartlist_len(const smartlist_t *sl) {
  tor_assert(sl);
  return (sl)->num_used;
}
/** Return the <b>idx</b>th element of sl.
 */
static inline void *smartlist_get(const smartlist_t *sl, int idx) ATTR_PURE;
static inline void *smartlist_get(const smartlist_t *sl, int idx) {
  tor_assert(sl);
  tor_assert(idx>=0);
  tor_assert(sl->num_used > idx);
  return sl->list[idx];
}
static inline void smartlist_set(smartlist_t *sl, int idx, void *val) {
  tor_assert(sl);
  tor_assert(idx>=0);
  tor_assert(sl->num_used > idx);
  sl->list[idx] = val;
}
#else
#define smartlist_len(sl) ((sl)->num_used)
#define smartlist_get(sl, idx) ((sl)->list[idx])
#define smartlist_set(sl, idx, val) ((sl)->list[idx] = (val))
#endif

/** Exchange the elements at indices <b>idx1</b> and <b>idx2</b> of the
 * smartlist <b>sl</b>. */
static inline void smartlist_swap(smartlist_t *sl, int idx1, int idx2)
{
  if (idx1 != idx2) {
    void *elt = smartlist_get(sl, idx1);
    smartlist_set(sl, idx1, smartlist_get(sl, idx2));
    smartlist_set(sl, idx2, elt);
  }
}

void smartlist_del(smartlist_t *sl, int idx);
void smartlist_del_keeporder(smartlist_t *sl, int idx);
void smartlist_insert(smartlist_t *sl, int idx, void *val);
void smartlist_sort(smartlist_t *sl,
                    int (*compare)(const void **a, const void **b));
void *smartlist_get_most_frequent(const smartlist_t *sl,
                    int (*compare)(const void **a, const void **b));
void smartlist_uniq(smartlist_t *sl,
                    int (*compare)(const void **a, const void **b),
                    void (*free_fn)(void *elt));

void smartlist_shuffle(smartlist_t *sl);
void *smartlist_choose(const smartlist_t *sl);

void smartlist_sort_strings(smartlist_t *sl);
void smartlist_sort_digests(smartlist_t *sl);

char *smartlist_get_most_frequent_string(smartlist_t *sl);

void smartlist_uniq_strings(smartlist_t *sl);
void smartlist_uniq_digests(smartlist_t *sl);
void *smartlist_bsearch(smartlist_t *sl, const void *key,
                        int (*compare)(const void *key, const void **member))
  ATTR_PURE;
int smartlist_bsearch_idx(const smartlist_t *sl, const void *key,
                          int (*compare)(const void *key, const void **member),
                          int *found_out);

void smartlist_pqueue_add(smartlist_t *sl,
                          int (*compare)(const void *a, const void *b),
                          int idx_field_offset,
                          void *item);
void *smartlist_pqueue_pop(smartlist_t *sl,
                           int (*compare)(const void *a, const void *b),
                           int idx_field_offset);
void smartlist_pqueue_remove(smartlist_t *sl,
                             int (*compare)(const void *a, const void *b),
                             int idx_field_offset,
                             void *item);
void smartlist_pqueue_assert_ok(smartlist_t *sl,
                                int (*compare)(const void *a, const void *b),
                                int idx_field_offset);

#define SPLIT_SKIP_SPACE   0x01
#define SPLIT_IGNORE_BLANK 0x02
#define SPLIT_STRIP_SPACE  0x04
int smartlist_split_string(smartlist_t *sl, const char *str, const char *sep,
                           int flags, int max);
char *smartlist_join_strings(smartlist_t *sl, const char *join, int terminate,
                             size_t *len_out) ATTR_MALLOC;
char *smartlist_join_strings2(smartlist_t *sl, const char *join,
                              size_t join_len, int terminate, size_t *len_out)
  ATTR_MALLOC;

/** Iterate over the items in a smartlist <b>sl</b>, in order.  For each item,
 * assign it to a new local variable of type <b>type</b> named <b>var</b>, and
 * execute the statement <b>cmd</b>.  Inside the loop, the loop index can
 * be accessed as <b>var</b>_sl_idx and the length of the list can be accessed
 * as <b>var</b>_sl_len.
 *
 * NOTE: Do not change the length of the list while the loop is in progress,
 * unless you adjust the _sl_len variable correspondingly.  See second example
 * below.
 *
 * Example use:
 * <pre>
 *   smartlist_t *list = smartlist_split("A:B:C", ":", 0, 0);
 *   SMARTLIST_FOREACH(list, char *, cp,
 *   {
 *     printf("%d: %s\n", cp_sl_idx, cp);
 *     free(cp);
 *   });
 *   smartlist_free(list);
 * </pre>
 *
 * Example use (advanced):
 * <pre>
 *   SMARTLIST_FOREACH(list, char *, cp,
 *   {
 *     if (!strcmp(cp, "junk")) {
 *       free(cp);
 *       SMARTLIST_DEL_CURRENT(list, cp);
 *     }
 *   });
 * </pre>
 */
/* Note: these macros use token pasting, and reach into smartlist internals.
 * This can make them a little daunting. Here's the approximate unpacking of
 * the above examples, for entertainment value:
 *
 * <pre>
 * smartlist_t *list = smartlist_split("A:B:C", ":", 0, 0);
 * {
 *   int cp_sl_idx, cp_sl_len = smartlist_len(list);
 *   char *cp;
 *   for (cp_sl_idx = 0; cp_sl_idx < cp_sl_len; ++cp_sl_idx) {
 *     cp = smartlist_get(list, cp_sl_idx);
 *     printf("%d: %s\n", cp_sl_idx, cp);
 *     free(cp);
 *   }
 * }
 * smartlist_free(list);
 * </pre>
 *
 * <pre>
 * {
 *   int cp_sl_idx, cp_sl_len = smartlist_len(list);
 *   char *cp;
 *   for (cp_sl_idx = 0; cp_sl_idx < cp_sl_len; ++cp_sl_idx) {
 *     cp = smartlist_get(list, cp_sl_idx);
 *     if (!strcmp(cp, "junk")) {
 *       free(cp);
 *       smartlist_del(list, cp_sl_idx);
 *       --cp_sl_idx;
 *       --cp_sl_len;
 *     }
 *   }
 * }
 * </pre>
 */
#define SMARTLIST_FOREACH_BEGIN(sl, type, var)  \
  do {                                                          \
    int var ## _sl_idx, var ## _sl_len=(sl)->num_used;          \
    type var;                                                   \
    for (var ## _sl_idx = 0; var ## _sl_idx < var ## _sl_len;   \
         ++var ## _sl_idx) {                                    \
      var = (sl)->list[var ## _sl_idx];

#define SMARTLIST_FOREACH_END(var)              \
    var = NULL;                                 \
  } } while (0)

#define SMARTLIST_FOREACH(sl, type, var, cmd)                   \
  SMARTLIST_FOREACH_BEGIN(sl,type,var) {                        \
    cmd;                                                        \
  } SMARTLIST_FOREACH_END(var)

/** Helper: While in a SMARTLIST_FOREACH loop over the list <b>sl</b> indexed
 * with the variable <b>var</b>, remove the current element in a way that
 * won't confuse the loop. */
#define SMARTLIST_DEL_CURRENT(sl, var)          \
  do {                                          \
    smartlist_del(sl, var ## _sl_idx);          \
    --var ## _sl_idx;                           \
    --var ## _sl_len;                           \
  } while (0)

/** Helper: While in a SMARTLIST_FOREACH loop over the list <b>sl</b> indexed
 * with the variable <b>var</b>, replace the current element with <b>val</b>.
 * Does not deallocate the current value of <b>var</b>.
 */
#define SMARTLIST_REPLACE_CURRENT(sl, var, val) \
  do {                                          \
    smartlist_set(sl, var ## _sl_idx, val);     \
  } while (0)

/* Helper: Given two lists of items, possibly of different types, such that
 * both lists are sorted on some common field (as determined by a comparison
 * expression <b>cmpexpr</b>), and such that one list (<b>sl1</b>) has no
 * duplicates on the common field, loop through the lists in lockstep, and
 * execute <b>unmatched_var2</b> on items in var2 that do not appear in
 * var1.
 *
 * WARNING: It isn't safe to add remove elements from either list while the
 * loop is in progress.
 *
 * Example use:
 *  SMARTLIST_FOREACH_JOIN(routerstatus_list, routerstatus_t *, rs,
 *                     routerinfo_list, routerinfo_t *, ri,
 *                    memcmp(rs->identity_digest, ri->identity_digest, 20),
 *                     log_info(LD_GENERAL,"No match for %s", ri->nickname)) {
 *    log_info(LD_GENERAL, "%s matches routerstatus %p", ri->nickname, rs);
 * } SMARTLIST_FOREACH_JOIN_END(rs, ri);
 **/
/* The example above unpacks (approximately) to:
 *  int rs_sl_idx = 0, rs_sl_len = smartlist_len(routerstatus_list);
 *  int ri_sl_idx, ri_sl_len = smartlist_len(routerinfo_list);
 *  int rs_ri_cmp;
 *  routerstatus_t *rs;
 *  routerinfo_t *ri;
 *  for (; ri_sl_idx < ri_sl_len; ++ri_sl_idx) {
 *    ri = smartlist_get(routerinfo_list, ri_sl_idx);
 *    while (rs_sl_idx < rs_sl_len) {
 *      rs = smartlist_get(routerstatus_list, rs_sl_idx);
 *      rs_ri_cmp = memcmp(rs->identity_digest, ri->identity_digest, 20);
 *      if (rs_ri_cmp > 0) {
 *        break;
 *      } else if (rs_ri_cmp == 0) {
 *        goto matched_ri;
 *      } else {
 *        ++rs_sl_idx;
 *      }
 *    }
 *    log_info(LD_GENERAL,"No match for %s", ri->nickname);
 *    continue;
 *   matched_ri: {
 *    log_info(LD_GENERAL,"%s matches with routerstatus %p",ri->nickname,rs);
 *    }
 *  }
 */
#define SMARTLIST_FOREACH_JOIN(sl1, type1, var1, sl2, type2, var2,      \
                                cmpexpr, unmatched_var2)                \
  do {                                                                  \
  int var1 ## _sl_idx = 0, var1 ## _sl_len=(sl1)->num_used;             \
  int var2 ## _sl_idx = 0, var2 ## _sl_len=(sl2)->num_used;             \
  int var1 ## _ ## var2 ## _cmp;                                        \
  type1 var1;                                                           \
  type2 var2;                                                           \
  for (; var2##_sl_idx < var2##_sl_len; ++var2##_sl_idx) {              \
    var2 = (sl2)->list[var2##_sl_idx];                                  \
    while (var1##_sl_idx < var1##_sl_len) {                             \
      var1 = (sl1)->list[var1##_sl_idx];                                \
      var1##_##var2##_cmp = (cmpexpr);                                  \
      if (var1##_##var2##_cmp > 0) {                                    \
        break;                                                          \
      } else if (var1##_##var2##_cmp == 0) {                            \
        goto matched_##var2;                                            \
      } else {                                                          \
        ++var1##_sl_idx;                                                \
      }                                                                 \
    }                                                                   \
    /* Ran out of v1, or no match for var2. */                          \
    unmatched_var2;                                                     \
    continue;                                                           \
    matched_##var2: ;                                                   \

#define SMARTLIST_FOREACH_JOIN_END(var1, var2)  \
  }                                             \
  } while (0)

#define DECLARE_MAP_FNS(maptype, keytype, prefix)                       \
  typedef struct maptype maptype;                                       \
  typedef struct prefix##entry_t *prefix##iter_t;                       \
  maptype* prefix##new(void);                                           \
  void* prefix##set(maptype *map, keytype key, void *val);              \
  void* prefix##get(const maptype *map, keytype key);                   \
  void* prefix##remove(maptype *map, keytype key);                      \
  void prefix##free(maptype *map, void (*free_val)(void*));             \
  int prefix##isempty(const maptype *map);                              \
  int prefix##size(const maptype *map);                                 \
  prefix##iter_t *prefix##iter_init(maptype *map);                      \
  prefix##iter_t *prefix##iter_next(maptype *map, prefix##iter_t *iter); \
  prefix##iter_t *prefix##iter_next_rmv(maptype *map, prefix##iter_t *iter); \
  void prefix##iter_get(prefix##iter_t *iter, keytype *keyp, void **valp); \
  int prefix##iter_done(prefix##iter_t *iter);                          \
  void prefix##assert_ok(const maptype *map)

/* Map from const char * to void *. Implemented with a hash table. */
DECLARE_MAP_FNS(strmap_t, const char *, strmap_);
/* Map from const char[DIGEST_LEN] to void *. Implemented with a hash table. */
DECLARE_MAP_FNS(digestmap_t, const char *, digestmap_);

#undef DECLARE_MAP_FNS

/** Iterates over the key-value pairs in a map <b>map</b> in order.
 * <b>prefix</b> is as for DECLARE_MAP_FNS (i.e., strmap_ or digestmap_).
 * The map's keys and values are of type keytype and valtype respectively;
 * each iteration assigns them to keyvar and valvar.
 *
 * Example use:
 *   MAP_FOREACH(digestmap_, m, const char *, k, routerinfo_t *, r) {
 *     // use k and r
 *   } MAP_FOREACH_END.
 */
/* Unpacks to, approximately:
 * {
 *   digestmap_iter_t *k_iter;
 *   for (k_iter = digestmap_iter_init(m); !digestmap_iter_done(k_iter);
 *        k_iter = digestmap_iter_next(m, k_iter)) {
 *     const char *k;
 *     void *r_voidp;
 *     routerinfo_t *r;
 *     digestmap_iter_get(k_iter, &k, &r_voidp);
 *     r = r_voidp;
 *     // use k and r
 *   }
 * }
 */
#define MAP_FOREACH(prefix, map, keytype, keyvar, valtype, valvar)      \
  do {                                                                  \
    prefix##iter_t *keyvar##_iter;                                      \
    for (keyvar##_iter = prefix##iter_init(map);                        \
         !prefix##iter_done(keyvar##_iter);                             \
         keyvar##_iter = prefix##iter_next(map, keyvar##_iter)) {       \
      keytype keyvar;                                                   \
      void *valvar##_voidp;                                             \
      valtype valvar;                                                   \
      prefix##iter_get(keyvar##_iter, &keyvar, &valvar##_voidp);        \
      valvar = valvar##_voidp;

/** As MAP_FOREACH, except allows members to be removed from the map
 * during the iteration via MAP_DEL_CURRENT.  Example use:
 *
 * Example use:
 *   MAP_FOREACH(digestmap_, m, const char *, k, routerinfo_t *, r) {
 *      if (is_very_old(r))
 *       MAP_DEL_CURRENT(k);
 *   } MAP_FOREACH_END.
 **/
/* Unpacks to, approximately:
 * {
 *   digestmap_iter_t *k_iter;
 *   int k_del=0;
 *   for (k_iter = digestmap_iter_init(m); !digestmap_iter_done(k_iter);
 *        k_iter = k_del ? digestmap_iter_next(m, k_iter)
 *                       : digestmap_iter_next_rmv(m, k_iter)) {
 *     const char *k;
 *     void *r_voidp;
 *     routerinfo_t *r;
 *     k_del=0;
 *     digestmap_iter_get(k_iter, &k, &r_voidp);
 *     r = r_voidp;
 *     if (is_very_old(r)) {
 *       k_del = 1;
 *     }
 *   }
 * }
 */
#define MAP_FOREACH_MODIFY(prefix, map, keytype, keyvar, valtype, valvar) \
  do {                                                                  \
    prefix##iter_t *keyvar##_iter;                                      \
    int keyvar##_del=0;                                                 \
    for (keyvar##_iter = prefix##iter_init(map);                        \
         !prefix##iter_done(keyvar##_iter);                             \
         keyvar##_iter = keyvar##_del ?                                 \
           prefix##iter_next_rmv(map, keyvar##_iter) :                  \
           prefix##iter_next(map, keyvar##_iter)) {                     \
      keytype keyvar;                                                   \
      void *valvar##_voidp;                                             \
      valtype valvar;                                                   \
      keyvar##_del=0;                                                   \
      prefix##iter_get(keyvar##_iter, &keyvar, &valvar##_voidp);        \
      valvar = valvar##_voidp;

/** Used with MAP_FOREACH_MODIFY to remove the currently-iterated-upon
 * member of the map.  */
#define MAP_DEL_CURRENT(keyvar)                   \
  do {                                            \
    keyvar##_del = 1;                             \
  } while (0)

/** Used to end a MAP_FOREACH() block. */
#define MAP_FOREACH_END } while (0)

/** As MAP_FOREACH, but does not require declaration of prefix or keytype.
 * Example use:
 *   DIGESTMAP_FOREACH(m, k, routerinfo_t *, r) {
 *     // use k and r
 *   } DIGESTMAP_FOREACH_END.
 */
#define DIGESTMAP_FOREACH(map, keyvar, valtype, valvar)                 \
  MAP_FOREACH(digestmap_, map, const char *, keyvar, valtype, valvar)

/** As MAP_FOREACH_MODIFY, but does not require declaration of prefix or
 * keytype.
 * Example use:
 *   DIGESTMAP_FOREACH_MODIFY(m, k, routerinfo_t *, r) {
 *      if (is_very_old(r))
 *       MAP_DEL_CURRENT(k);
 *   } DIGESTMAP_FOREACH_END.
 */
#define DIGESTMAP_FOREACH_MODIFY(map, keyvar, valtype, valvar)          \
  MAP_FOREACH_MODIFY(digestmap_, map, const char *, keyvar, valtype, valvar)
/** Used to end a DIGESTMAP_FOREACH() block. */
#define DIGESTMAP_FOREACH_END MAP_FOREACH_END

#define STRMAP_FOREACH(map, keyvar, valtype, valvar)                 \
  MAP_FOREACH(strmap_, map, const char *, keyvar, valtype, valvar)
#define STRMAP_FOREACH_MODIFY(map, keyvar, valtype, valvar)          \
  MAP_FOREACH_MODIFY(strmap_, map, const char *, keyvar, valtype, valvar)
#define STRMAP_FOREACH_END MAP_FOREACH_END

void* strmap_set_lc(strmap_t *map, const char *key, void *val);
void* strmap_get_lc(const strmap_t *map, const char *key);
void* strmap_remove_lc(strmap_t *map, const char *key);

#define DECLARE_TYPED_DIGESTMAP_FNS(prefix, maptype, valtype)           \
  typedef struct maptype maptype;                                       \
  typedef struct prefix##iter_t prefix##iter_t;                         \
  static inline maptype* prefix##new(void)                              \
  {                                                                     \
    return (maptype*)digestmap_new();                                   \
  }                                                                     \
  static inline digestmap_t* prefix##to_digestmap(maptype *map)         \
  {                                                                     \
    return (digestmap_t*)map;                                           \
  }                                                                     \
  static inline valtype* prefix##get(maptype *map, const char *key)     \
  {                                                                     \
    return (valtype*)digestmap_get((digestmap_t*)map, key);             \
  }                                                                     \
  static inline valtype* prefix##set(maptype *map, const char *key,     \
                                     valtype *val)                      \
  {                                                                     \
    return (valtype*)digestmap_set((digestmap_t*)map, key, val);        \
  }                                                                     \
  static inline valtype* prefix##remove(maptype *map, const char *key)  \
  {                                                                     \
    return (valtype*)digestmap_remove((digestmap_t*)map, key);          \
  }                                                                     \
  static inline void prefix##free(maptype *map, void (*free_val)(void*)) \
  {                                                                     \
    digestmap_free((digestmap_t*)map, free_val);                        \
  }                                                                     \
  static inline int prefix##isempty(maptype *map)                       \
  {                                                                     \
    return digestmap_isempty((digestmap_t*)map);                        \
  }                                                                     \
  static inline int prefix##size(maptype *map)                          \
  {                                                                     \
    return digestmap_size((digestmap_t*)map);                           \
  }                                                                     \
  static inline prefix##iter_t *prefix##iter_init(maptype *map)         \
  {                                                                     \
    return (prefix##iter_t*) digestmap_iter_init((digestmap_t*)map);    \
  }                                                                     \
  static inline prefix##iter_t *prefix##iter_next(maptype *map,         \
                                                  prefix##iter_t *iter) \
  {                                                                     \
    return (prefix##iter_t*) digestmap_iter_next(                       \
                       (digestmap_t*)map, (digestmap_iter_t*)iter);     \
  }                                                                     \
  static inline prefix##iter_t *prefix##iter_next_rmv(maptype *map,     \
                                                  prefix##iter_t *iter) \
  {                                                                     \
    return (prefix##iter_t*) digestmap_iter_next_rmv(                   \
                       (digestmap_t*)map, (digestmap_iter_t*)iter);     \
  }                                                                     \
  static inline void prefix##iter_get(prefix##iter_t *iter,             \
                                      const char **keyp,                \
                                      valtype **valp)                   \
  {                                                                     \
    void *v;                                                            \
    digestmap_iter_get((digestmap_iter_t*) iter, keyp, &v);             \
    *valp = v;                                                          \
  }                                                                     \
  static inline int prefix##iter_done(prefix##iter_t *iter)             \
  {                                                                     \
    return digestmap_iter_done((digestmap_iter_t*)iter);                \
  }

#if SIZEOF_INT == 4
#define BITARRAY_SHIFT 5
#elif SIZEOF_INT == 8
#define BITARRAY_SHIFT 6
#else
#error "int is neither 4 nor 8 bytes. I can't deal with that."
#endif
#define BITARRAY_MASK ((1u<<BITARRAY_SHIFT)-1)

/** A random-access array of one-bit-wide elements. */
typedef unsigned int bitarray_t;
/** Create a new bit array that can hold <b>n_bits</b> bits. */
static inline bitarray_t *
bitarray_init_zero(unsigned int n_bits)
{
  /* round up to the next int. */
  size_t sz = (n_bits+BITARRAY_MASK) >> BITARRAY_SHIFT;
  return xzalloc(sz*sizeof(unsigned int));
}
/** Expand <b>ba</b> from holding <b>n_bits_old</b> to <b>n_bits_new</b>,
 * clearing all new bits.  Returns a possibly changed pointer to the
 * bitarray. */
static inline bitarray_t *
bitarray_expand(bitarray_t *ba,
                unsigned int n_bits_old, unsigned int n_bits_new)
{
  size_t sz_old = (n_bits_old+BITARRAY_MASK) >> BITARRAY_SHIFT;
  size_t sz_new = (n_bits_new+BITARRAY_MASK) >> BITARRAY_SHIFT;
  char *ptr;
  if (sz_new <= sz_old)
    return ba;
  ptr = xrealloc(ba, sz_new*sizeof(unsigned int));
  /* This memset does nothing to the older excess bytes.  But they were
   * already set to 0 by bitarry_init_zero. */
  memset(ptr+sz_old*sizeof(unsigned int), 0,
         (sz_new-sz_old)*sizeof(unsigned int));
  return (bitarray_t*) ptr;
}
/** Free the bit array <b>ba</b>. */
static inline void
bitarray_free(bitarray_t *ba)
{
  free(ba);
}
/** Set the <b>bit</b>th bit in <b>b</b> to 1. */
static inline void
bitarray_set(bitarray_t *b, int bit)
{
  b[bit >> BITARRAY_SHIFT] |= (1u << (bit & BITARRAY_MASK));
}
/** Set the <b>bit</b>th bit in <b>b</b> to 0. */
static inline void
bitarray_clear(bitarray_t *b, int bit)
{
  b[bit >> BITARRAY_SHIFT] &= ~ (1u << (bit & BITARRAY_MASK));
}
/** Return true iff <b>bit</b>th bit in <b>b</b> is nonzero.  NOTE: does
 * not necessarily return 1 on true. */
static inline unsigned int
bitarray_is_set(bitarray_t *b, int bit)
{
  return b[bit >> BITARRAY_SHIFT] & (1u << (bit & BITARRAY_MASK));
}

/** A set of digests, implemented as a Bloom filter. */
typedef struct {
  int mask; /**< One less than the number of bits in <b>ba</b>; always one less
             * than a power of two. */
  bitarray_t *ba; /**< A bit array to implement the Bloom filter. */
} digestset_t;

#define BIT(n) ((n) & set->mask)
/** Add the digest <b>digest</b> to <b>set</b>. */
static inline void
digestset_add(digestset_t *set, const char *digest)
{
  const uint32_t *p = (const uint32_t *)digest;
  const uint32_t d1 = p[0] + (p[1]>>16);
  const uint32_t d2 = p[1] + (p[2]>>16);
  const uint32_t d3 = p[2] + (p[3]>>16);
  const uint32_t d4 = p[3] + (p[0]>>16);
  bitarray_set(set->ba, BIT(d1));
  bitarray_set(set->ba, BIT(d2));
  bitarray_set(set->ba, BIT(d3));
  bitarray_set(set->ba, BIT(d4));
}

/** If <b>digest</b> is in <b>set</b>, return nonzero.  Otherwise,
 * <em>probably</em> return zero. */
static inline int
digestset_isin(const digestset_t *set, const char *digest)
{
  const uint32_t *p = (const uint32_t *)digest;
  const uint32_t d1 = p[0] + (p[1]>>16);
  const uint32_t d2 = p[1] + (p[2]>>16);
  const uint32_t d3 = p[2] + (p[3]>>16);
  const uint32_t d4 = p[3] + (p[0]>>16);
  return bitarray_is_set(set->ba, BIT(d1)) &&
         bitarray_is_set(set->ba, BIT(d2)) &&
         bitarray_is_set(set->ba, BIT(d3)) &&
         bitarray_is_set(set->ba, BIT(d4));
}
#undef BIT

digestset_t *digestset_new(int max_elements);
void digestset_free(digestset_t* set);

/* These functions, given an <b>array</b> of <b>n_elements</b>, return the
 * <b>nth</b> lowest element. <b>nth</b>=0 gives the lowest element;
 * <b>n_elements</b>-1 gives the highest; and (<b>n_elements</b>-1) / 2 gives
 * the median.  As a side effect, the elements of <b>array</b> are sorted. */
int find_nth_int(int *array, int n_elements, int nth);
time_t find_nth_time(time_t *array, int n_elements, int nth);
double find_nth_double(double *array, int n_elements, int nth);
int32_t find_nth_int32(int32_t *array, int n_elements, int nth);
uint32_t find_nth_uint32(uint32_t *array, int n_elements, int nth);
long find_nth_long(long *array, int n_elements, int nth);
static inline int
median_int(int *array, int n_elements)
{
  return find_nth_int(array, n_elements, (n_elements-1)/2);
}
static inline time_t
median_time(time_t *array, int n_elements)
{
  return find_nth_time(array, n_elements, (n_elements-1)/2);
}
static inline double
median_double(double *array, int n_elements)
{
  return find_nth_double(array, n_elements, (n_elements-1)/2);
}
static inline uint32_t
median_uint32(uint32_t *array, int n_elements)
{
  return find_nth_uint32(array, n_elements, (n_elements-1)/2);
}
static inline int32_t
median_int32(int32_t *array, int n_elements)
{
  return find_nth_int32(array, n_elements, (n_elements-1)/2);
}
static inline long
median_long(long *array, int n_elements)
{
  return find_nth_long(array, n_elements, (n_elements-1)/2);
}

#endif
