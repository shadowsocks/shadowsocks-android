/* Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2011, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "util.h"
#include "tinytest_macros.h"

#include "container.h"
#include "crypt.h"

/** Helper: return a tristate based on comparing the strings in *<b>a</b> and
 * *<b>b</b>. */
static int
_compare_strs(const void **a, const void **b)
{
  const char *s1 = *a, *s2 = *b;
  return strcmp(s1, s2);
}

/** Helper: return a tristate based on comparing the strings in *<b>a</b> and
 * *<b>b</b>, excluding a's first character, and ignoring case. */
static int
_compare_without_first_ch(const void *a, const void **b)
{
  const char *s1 = a, *s2 = *b;
  return strcasecmp(s1+1, s2);
}

/** Run unit tests for basic dynamic-sized array functionality. */
static void
test_container_smartlist_basic(void *unused)
{
  smartlist_t *sl;

  /* XXXX test sort_digests, uniq_strings, uniq_digests */

  /* Test smartlist add, del_keeporder, insert, get. */
  sl = smartlist_create();
  smartlist_add(sl, (void*)1);
  smartlist_add(sl, (void*)2);
  smartlist_add(sl, (void*)3);
  smartlist_add(sl, (void*)4);
  smartlist_del_keeporder(sl, 1);
  smartlist_insert(sl, 1, (void*)22);
  smartlist_insert(sl, 0, (void*)0);
  smartlist_insert(sl, 5, (void*)555);
  tt_ptr_op(smartlist_get(sl,0), ==, (void*)0);
  tt_ptr_op(smartlist_get(sl,1), ==, (void*)1);
  tt_ptr_op(smartlist_get(sl,2), ==, (void*)22);
  tt_ptr_op(smartlist_get(sl,3), ==, (void*)3);
  tt_ptr_op(smartlist_get(sl,4), ==, (void*)4);
  tt_ptr_op(smartlist_get(sl,5), ==, (void*)555);
  /* Try deleting in the middle. */
  smartlist_del(sl, 1);
  tt_ptr_op(smartlist_get(sl, 1), ==, (void*)555);
  /* Try deleting at the end. */
  smartlist_del(sl, 4);
  tt_int_op(smartlist_len(sl), ==, 4);

  /* test isin. */
  tt_assert(smartlist_isin(sl, (void*)3));
  tt_assert(!smartlist_isin(sl, (void*)99));

 end:
  smartlist_free(sl);
}

/** Run unit tests for smartlist-of-strings functionality. */
static void
test_container_smartlist_strings(void *unused)
{
  smartlist_t *sl = smartlist_create();
  char *cp=NULL, *cp_alloc=NULL;
  size_t sz;

  /* Test split and join */
  tt_int_op(smartlist_len(sl), ==, 0);
  smartlist_split_string(sl, "abc", ":", 0, 0);
  tt_int_op(smartlist_len(sl), ==, 1);
  tt_str_op(smartlist_get(sl, 0), ==, "abc");
  smartlist_split_string(sl, "a::bc::", "::", 0, 0);
  tt_int_op(smartlist_len(sl), ==, 4);
  tt_str_op(smartlist_get(sl, 1), ==, "a");
  tt_str_op(smartlist_get(sl, 2), ==, "bc");
  tt_str_op(smartlist_get(sl, 3), ==, "");
  cp_alloc = smartlist_join_strings(sl, "", 0, NULL);
  tt_str_op(cp_alloc, ==, "abcabc");
  free(cp_alloc);
  cp_alloc = smartlist_join_strings(sl, "!", 0, NULL);
  tt_str_op(cp_alloc, ==, "abc!a!bc!");
  free(cp_alloc);
  cp_alloc = smartlist_join_strings(sl, "XY", 0, NULL);
  tt_str_op(cp_alloc, ==, "abcXYaXYbcXY");
  free(cp_alloc);
  cp_alloc = smartlist_join_strings(sl, "XY", 1, NULL);
  tt_str_op(cp_alloc, ==, "abcXYaXYbcXYXY");
  free(cp_alloc);
  cp_alloc = smartlist_join_strings(sl, "", 1, NULL);
  tt_str_op(cp_alloc, ==, "abcabc");
  free(cp_alloc);

  smartlist_split_string(sl, "/def/  /ghijk", "/", 0, 0);
  tt_int_op(smartlist_len(sl), ==, 8);
  tt_str_op(smartlist_get(sl, 4), ==, "");
  tt_str_op(smartlist_get(sl, 5), ==, "def");
  tt_str_op(smartlist_get(sl, 6), ==, "  ");
  tt_str_op(smartlist_get(sl, 7), ==, "ghijk");
  SMARTLIST_FOREACH(sl, char *, cp, free(cp));
  smartlist_clear(sl);

  smartlist_split_string(sl, "a,bbd,cdef", ",", SPLIT_SKIP_SPACE, 0);
  tt_int_op(smartlist_len(sl), ==, 3);
  tt_str_op(smartlist_get(sl,0), ==, "a");
  tt_str_op(smartlist_get(sl,1), ==, "bbd");
  tt_str_op(smartlist_get(sl,2), ==, "cdef");
  smartlist_split_string(sl, " z <> zhasd <>  <> bnud<>   ", "<>",
                         SPLIT_SKIP_SPACE, 0);
  tt_int_op(smartlist_len(sl), ==, 8);
  tt_str_op(smartlist_get(sl,3), ==, "z");
  tt_str_op(smartlist_get(sl,4), ==, "zhasd");
  tt_str_op(smartlist_get(sl,5), ==, "");
  tt_str_op(smartlist_get(sl,6), ==, "bnud");
  tt_str_op(smartlist_get(sl,7), ==, "");

  SMARTLIST_FOREACH(sl, char *, cp, free(cp));
  smartlist_clear(sl);

  smartlist_split_string(sl, " ab\tc \td ef  ", NULL,
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
  tt_int_op(smartlist_len(sl), ==, 4);
  tt_str_op(smartlist_get(sl,0), ==, "ab");
  tt_str_op(smartlist_get(sl,1), ==, "c");
  tt_str_op(smartlist_get(sl,2), ==, "d");
  tt_str_op(smartlist_get(sl,3), ==, "ef");
  smartlist_split_string(sl, "ghi\tj", NULL,
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
  tt_int_op(smartlist_len(sl), ==, 6);
  tt_str_op(smartlist_get(sl,4), ==, "ghi");
  tt_str_op(smartlist_get(sl,5), ==, "j");

  SMARTLIST_FOREACH(sl, char *, cp, free(cp));
  smartlist_clear(sl);

  cp_alloc = smartlist_join_strings(sl, "XY", 0, NULL);
  tt_str_op(cp_alloc, ==, "");
  free(cp_alloc);
  cp_alloc = smartlist_join_strings(sl, "XY", 1, NULL);
  tt_str_op(cp_alloc, ==, "XY");
  free(cp_alloc);

  smartlist_split_string(sl, " z <> zhasd <>  <> bnud<>   ", "<>",
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
  tt_int_op(smartlist_len(sl), ==, 3);
  tt_str_op(smartlist_get(sl, 0), ==, "z");
  tt_str_op(smartlist_get(sl, 1), ==, "zhasd");
  tt_str_op(smartlist_get(sl, 2), ==, "bnud");
  smartlist_split_string(sl, " z <> zhasd <>  <> bnud<>   ", "<>",
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 2);
  tt_int_op(smartlist_len(sl), ==, 5);
  tt_str_op(smartlist_get(sl, 3), ==, "z");
  tt_str_op(smartlist_get(sl, 4), ==, "zhasd <>  <> bnud<>");
  SMARTLIST_FOREACH(sl, char *, cp, free(cp));
  smartlist_clear(sl);

  smartlist_split_string(sl, "abcd\n", "\n",
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
  tt_int_op(smartlist_len(sl), ==, 1);
  tt_str_op(smartlist_get(sl, 0), ==, "abcd");
  smartlist_split_string(sl, "efgh", "\n",
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
  tt_int_op(smartlist_len(sl), ==, 2);
  tt_str_op(smartlist_get(sl, 1), ==, "efgh");

  SMARTLIST_FOREACH(sl, char *, cp, free(cp));
  smartlist_clear(sl);

  /* Test swapping, shuffling, and sorting. */
  smartlist_split_string(sl, "the,onion,router,by,arma,and,nickm", ",", 0, 0);
  tt_int_op(smartlist_len(sl), ==, 7);
  smartlist_sort(sl, _compare_strs);
  cp_alloc = smartlist_join_strings(sl, ",", 0, NULL);
  tt_str_op(cp_alloc, ==, "and,arma,by,nickm,onion,router,the");
  free(cp_alloc);
  smartlist_swap(sl, 1, 5);
  cp_alloc = smartlist_join_strings(sl, ",", 0, NULL);
  tt_str_op(cp_alloc, ==, "and,router,by,nickm,onion,arma,the");
  free(cp_alloc);
  smartlist_shuffle(sl);
  tt_int_op(smartlist_len(sl), ==, 7);
  tt_assert(smartlist_string_isin(sl, "and"));
  tt_assert(smartlist_string_isin(sl, "router"));
  tt_assert(smartlist_string_isin(sl, "by"));
  tt_assert(smartlist_string_isin(sl, "nickm"));
  tt_assert(smartlist_string_isin(sl, "onion"));
  tt_assert(smartlist_string_isin(sl, "arma"));
  tt_assert(smartlist_string_isin(sl, "the"));

  /* Test bsearch. */
  smartlist_sort(sl, _compare_strs);
  tt_str_op(smartlist_bsearch(sl, "zNicKM",
                              _compare_without_first_ch), ==, "nickm");
  tt_str_op(smartlist_bsearch(sl, " AND", _compare_without_first_ch), ==, "and");
  tt_ptr_op(smartlist_bsearch(sl, " ANz", _compare_without_first_ch), ==, NULL);

  /* Test bsearch_idx */
  {
    int f;
    tt_int_op(smartlist_bsearch_idx(sl," aaa",_compare_without_first_ch,&f),
              ==, 0);
    tt_int_op(f, ==, 0);
    tt_int_op(smartlist_bsearch_idx(sl," and",_compare_without_first_ch,&f),
              ==, 0);
    tt_int_op(f, ==, 1);
    tt_int_op(smartlist_bsearch_idx(sl," arm",_compare_without_first_ch,&f),
              ==, 1);
    tt_int_op(f, ==, 0);
    tt_int_op(smartlist_bsearch_idx(sl," arma",_compare_without_first_ch,&f),
              ==, 1);
    tt_int_op(f, ==, 1);
    tt_int_op(smartlist_bsearch_idx(sl," armb",_compare_without_first_ch,&f),
              ==, 2);
    tt_int_op(f, ==, 0);
    tt_int_op(smartlist_bsearch_idx(sl," zzzz",_compare_without_first_ch,&f),
              ==, 7);
    tt_int_op(f, ==, 0);
  }

  /* Test reverse() and pop_last() */
  smartlist_reverse(sl);
  cp_alloc = smartlist_join_strings(sl, ",", 0, NULL);
  tt_str_op(cp_alloc, ==, "the,router,onion,nickm,by,arma,and");
  free(cp_alloc);
  cp_alloc = smartlist_pop_last(sl);
  tt_str_op(cp_alloc, ==, "and");
  free(cp_alloc);
  tt_int_op(smartlist_len(sl), ==, 6);
  SMARTLIST_FOREACH(sl, char *, cp, free(cp));
  smartlist_clear(sl);
  cp_alloc = smartlist_pop_last(sl);
  tt_ptr_op(cp_alloc, ==, NULL);

  /* Test uniq() */
  smartlist_split_string(sl,
                     "50,noon,radar,a,man,a,plan,a,canal,panama,radar,noon,50",
                     ",", 0, 0);
  smartlist_sort(sl, _compare_strs);
  smartlist_uniq(sl, _compare_strs, free);
  cp_alloc = smartlist_join_strings(sl, ",", 0, NULL);
  tt_str_op(cp_alloc, ==, "50,a,canal,man,noon,panama,plan,radar");
  free(cp_alloc);

  /* Test string_isin and isin_case and num_isin */
  tt_assert(smartlist_string_isin(sl, "noon"));
  tt_assert(!smartlist_string_isin(sl, "noonoon"));
  tt_assert(smartlist_string_isin_case(sl, "nOOn"));
  tt_assert(!smartlist_string_isin_case(sl, "nooNooN"));
  tt_assert(smartlist_string_num_isin(sl, 50));
  tt_assert(!smartlist_string_num_isin(sl, 60));

  /* Test smartlist_choose */
  {
    int i;
    int allsame = 1;
    int allin = 1;
    void *first = smartlist_choose(sl);
    tt_assert(smartlist_isin(sl, first));
    for (i = 0; i < 100; ++i) {
      void *second = smartlist_choose(sl);
      if (second != first)
        allsame = 0;
      if (!smartlist_isin(sl, second))
        allin = 0;
    }
    tt_assert(!allsame);
    tt_assert(allin);
  }
  SMARTLIST_FOREACH(sl, char *, cp, free(cp));
  smartlist_clear(sl);

  /* Test string_remove and remove and join_strings2 */
  smartlist_split_string(sl,
                    "Some say the Earth will end in ice and some in fire",
                    " ", 0, 0);
  cp = smartlist_get(sl, 4);
  tt_str_op(cp, ==, "will");
  smartlist_add(sl, cp);
  smartlist_remove(sl, cp);
  free(cp);
  cp_alloc = smartlist_join_strings(sl, ",", 0, NULL);
  tt_str_op(cp_alloc, ==, "Some,say,the,Earth,fire,end,in,ice,and,some,in");
  free(cp_alloc);
  smartlist_string_remove(sl, "in");
  cp_alloc = smartlist_join_strings2(sl, "+XX", 1, 0, &sz);
  tt_str_op(cp_alloc, ==, "Some+say+the+Earth+fire+end+some+ice+and");
  tt_int_op((int)sz, ==, 40);

 end:

  SMARTLIST_FOREACH(sl, char *, cp, free(cp));
  smartlist_free(sl);
  free(cp_alloc);
}

/** Run unit tests for smartlist set manipulation functions. */
static void
test_container_smartlist_overlap(void *unused)
{
  smartlist_t *sl = smartlist_create();
  smartlist_t *ints = smartlist_create();
  smartlist_t *odds = smartlist_create();
  smartlist_t *evens = smartlist_create();
  smartlist_t *primes = smartlist_create();
  int i;
  for (i=1; i < 10; i += 2)
    smartlist_add(odds, (void*)(uintptr_t)i);
  for (i=0; i < 10; i += 2)
    smartlist_add(evens, (void*)(uintptr_t)i);

  /* add_all */
  smartlist_add_all(ints, odds);
  smartlist_add_all(ints, evens);
  tt_int_op(smartlist_len(ints), ==, 10);

  smartlist_add(primes, (void*)2);
  smartlist_add(primes, (void*)3);
  smartlist_add(primes, (void*)5);
  smartlist_add(primes, (void*)7);

  /* overlap */
  tt_assert(smartlist_overlap(ints, odds));
  tt_assert(smartlist_overlap(odds, primes));
  tt_assert(smartlist_overlap(evens, primes));
  tt_assert(!smartlist_overlap(odds, evens));

  /* intersect */
  smartlist_add_all(sl, odds);
  smartlist_intersect(sl, primes);
  tt_int_op(smartlist_len(sl), ==, 3);
  tt_assert(smartlist_isin(sl, (void*)3));
  tt_assert(smartlist_isin(sl, (void*)5));
  tt_assert(smartlist_isin(sl, (void*)7));

  /* subtract */
  smartlist_add_all(sl, primes);
  smartlist_subtract(sl, odds);
  tt_int_op(smartlist_len(sl), ==, 1);
  tt_assert(smartlist_isin(sl, (void*)2));

 end:
  smartlist_free(odds);
  smartlist_free(evens);
  smartlist_free(ints);
  smartlist_free(primes);
  smartlist_free(sl);
}

/** Run unit tests for smartlist-of-digests functions. */
static void
test_container_smartlist_digests(void *unused)
{
  smartlist_t *sl = smartlist_create();

  /* digest_isin. */
  smartlist_add(sl, xmemdup("AAAAAAAAAAAAAAAAAAAA", SHA256_LENGTH));
  smartlist_add(sl, xmemdup("\00090AAB2AAAAaasdAAAAA", SHA256_LENGTH));
  smartlist_add(sl, xmemdup("\00090AAB2AAAAaasdAAAAA", SHA256_LENGTH));
  tt_int_op(smartlist_digest_isin(NULL, "AAAAAAAAAAAAAAAAAAAA"), ==, 0);
  tt_assert(smartlist_digest_isin(sl, "AAAAAAAAAAAAAAAAAAAA"));
  tt_assert(smartlist_digest_isin(sl, "\00090AAB2AAAAaasdAAAAA"));
  tt_int_op(smartlist_digest_isin(sl, "\00090AAB2AAABaasdAAAAA"), ==, 0);

  /* sort digests */
  smartlist_sort_digests(sl);
  tt_mem_op(smartlist_get(sl, 0), ==, "\00090AAB2AAAAaasdAAAAA", SHA256_LENGTH);
  tt_mem_op(smartlist_get(sl, 1), ==, "\00090AAB2AAAAaasdAAAAA", SHA256_LENGTH);
  tt_mem_op(smartlist_get(sl, 2), ==, "AAAAAAAAAAAAAAAAAAAA", SHA256_LENGTH);
  tt_int_op(smartlist_len(sl), ==, 3);

  /* uniq_digests */
  smartlist_uniq_digests(sl);
  tt_int_op(smartlist_len(sl), ==, 2);
  tt_mem_op(smartlist_get(sl, 0), ==, "\00090AAB2AAAAaasdAAAAA", SHA256_LENGTH);
  tt_mem_op(smartlist_get(sl, 1), ==, "AAAAAAAAAAAAAAAAAAAA", SHA256_LENGTH);

 end:
  SMARTLIST_FOREACH(sl, char *, cp, free(cp));
  smartlist_free(sl);
}

/** Run unit tests for concatenate-a-smartlist-of-strings functions. */
static void
test_container_smartlist_join(void *unused)
{
  smartlist_t *sl = smartlist_create();
  smartlist_t *sl2 = smartlist_create(), *sl3 = smartlist_create(),
    *sl4 = smartlist_create();
  char *joined=NULL;
  /* unique, sorted. */
  smartlist_split_string(sl,
                         "Abashments Ambush Anchorman Bacon Banks Borscht "
                         "Bunks Inhumane Insurance Knish Know Manners "
                         "Maraschinos Stamina Sunbonnets Unicorns Wombats",
                         " ", 0, 0);
  /* non-unique, sorted. */
  smartlist_split_string(sl2,
                         "Ambush Anchorman Anchorman Anemias Anemias Bacon "
                         "Crossbowmen Inhumane Insurance Knish Know Manners "
                         "Manners Maraschinos Wombats Wombats Work",
                         " ", 0, 0);
  SMARTLIST_FOREACH_JOIN(sl, char *, cp1,
                         sl2, char *, cp2,
                         strcmp(cp1,cp2),
                         smartlist_add(sl3, cp2)) {
    tt_str_op(cp1, ==, cp2);
    smartlist_add(sl4, cp1);
  } SMARTLIST_FOREACH_JOIN_END(cp1, cp2);

  SMARTLIST_FOREACH(sl3, const char *, cp,
                    tt_assert(smartlist_isin(sl2, cp) &&
                                !smartlist_string_isin(sl, cp)));
  SMARTLIST_FOREACH(sl4, const char *, cp,
                    tt_assert(smartlist_isin(sl, cp) &&
                                smartlist_string_isin(sl2, cp)));
  joined = smartlist_join_strings(sl3, ",", 0, NULL);
  tt_str_op(joined, ==, "Anemias,Anemias,Crossbowmen,Work");
  free(joined);
  joined = smartlist_join_strings(sl4, ",", 0, NULL);
  tt_str_op(joined, ==,
            "Ambush,Anchorman,Anchorman,Bacon,Inhumane,Insurance,"
            "Knish,Know,Manners,Manners,Maraschinos,Wombats,Wombats");

 end:
  smartlist_free(sl4);
  smartlist_free(sl3);
  SMARTLIST_FOREACH(sl2, char *, cp, free(cp));
  smartlist_free(sl2);
  SMARTLIST_FOREACH(sl, char *, cp, free(cp));
  smartlist_free(sl);
  free(joined);
}

/** Run unit tests for bitarray code */
static void
test_container_bitarray(void *unused)
{
  bitarray_t *ba = NULL;
  int i, j;

  ba = bitarray_init_zero(1);
  tt_assert(ba);
  tt_assert(! bitarray_is_set(ba, 0));
  bitarray_set(ba, 0);
  tt_assert(bitarray_is_set(ba, 0));
  bitarray_clear(ba, 0);
  tt_assert(! bitarray_is_set(ba, 0));
  bitarray_free(ba);

  ba = bitarray_init_zero(1023);
  for (i = 1; i < 64; ) {
    for (j = 0; j < 1023; ++j) {
      if (j % i)
        bitarray_set(ba, j);
      else
        bitarray_clear(ba, j);
    }
    for (j = 0; j < 1023; ++j) {
      tt_bool_op(bitarray_is_set(ba, j), ==, j%i);
    }

    if (i < 7)
      ++i;
    else if (i == 28)
      i = 32;
    else
      i += 7;
  }

 end:
  if (ba)
    bitarray_free(ba);
}

/** Run unit tests for digest set code (implemented as a hashtable or as a
 * bloom filter) */
static void
test_container_digestset(void *unused)
{
  smartlist_t *included = smartlist_create();
  char d[SHA256_LENGTH];
  int i;
  int ok = 1;
  int false_positives = 0;
  digestset_t *set = NULL;

  for (i = 0; i < 1000; ++i) {
    random_bytes((uchar *)d, SHA256_LENGTH);
    smartlist_add(included, xmemdup(d, SHA256_LENGTH));
  }
  set = digestset_new(1000);
  SMARTLIST_FOREACH(included, const char *, cp,
                    if (digestset_isin(set, cp))
                      ok = 0);
  tt_assert(ok);
  SMARTLIST_FOREACH(included, const char *, cp,
                    digestset_add(set, cp));
  SMARTLIST_FOREACH(included, const char *, cp,
                    if (!digestset_isin(set, cp))
                      ok = 0);
  tt_assert(ok);
  for (i = 0; i < 1000; ++i) {
    random_bytes((uchar *)d, SHA256_LENGTH);
    if (digestset_isin(set, d))
      ++false_positives;
  }
  tt_assert(false_positives < 50); /* Should be far lower. */

 end:
  if (set)
    digestset_free(set);
  SMARTLIST_FOREACH(included, char *, cp, free(cp));
  smartlist_free(included);
}

typedef struct pq_entry_t {
  const char *val;
  int idx;
} pq_entry_t;

/** Helper: return a tristate based on comparing two pq_entry_t values. */
static int
_compare_strings_for_pqueue(const void *p1, const void *p2)
{
  const pq_entry_t *e1=p1, *e2=p2;
  return strcmp(e1->val, e2->val);
}

/** Run unit tests for heap-based priority queue functions. */
static void
test_container_pqueue(void *unused)
{
  smartlist_t *sl = smartlist_create();
  int (*cmp)(const void *, const void*);
  const int offset = offsetof(pq_entry_t, idx);
#define ENTRY(s) pq_entry_t s = { #s, -1 }
  ENTRY(cows);
  ENTRY(zebras);
  ENTRY(fish);
  ENTRY(frogs);
  ENTRY(apples);
  ENTRY(squid);
  ENTRY(daschunds);
  ENTRY(eggplants);
  ENTRY(weissbier);
  ENTRY(lobsters);
  ENTRY(roquefort);
  ENTRY(chinchillas);
  ENTRY(fireflies);

#define OK() smartlist_pqueue_assert_ok(sl, cmp, offset)

  cmp = _compare_strings_for_pqueue;
  smartlist_pqueue_add(sl, cmp, offset, &cows);
  smartlist_pqueue_add(sl, cmp, offset, &zebras);
  smartlist_pqueue_add(sl, cmp, offset, &fish);
  smartlist_pqueue_add(sl, cmp, offset, &frogs);
  smartlist_pqueue_add(sl, cmp, offset, &apples);
  smartlist_pqueue_add(sl, cmp, offset, &squid);
  smartlist_pqueue_add(sl, cmp, offset, &daschunds);
  smartlist_pqueue_add(sl, cmp, offset, &eggplants);
  smartlist_pqueue_add(sl, cmp, offset, &weissbier);
  smartlist_pqueue_add(sl, cmp, offset, &lobsters);
  smartlist_pqueue_add(sl, cmp, offset, &roquefort);

  OK();

  tt_int_op(smartlist_len(sl), ==, 11);
  tt_ptr_op(smartlist_get(sl, 0), ==, &apples);
  tt_ptr_op(smartlist_pqueue_pop(sl, cmp, offset), ==, &apples);
  tt_int_op(smartlist_len(sl), ==, 10);
  OK();
  tt_ptr_op(smartlist_pqueue_pop(sl, cmp, offset), ==, &cows);
  tt_ptr_op(smartlist_pqueue_pop(sl, cmp, offset), ==, &daschunds);
  smartlist_pqueue_add(sl, cmp, offset, &chinchillas);
  OK();
  smartlist_pqueue_add(sl, cmp, offset, &fireflies);
  OK();
  tt_ptr_op(smartlist_pqueue_pop(sl, cmp, offset), ==, &chinchillas);
  tt_ptr_op(smartlist_pqueue_pop(sl, cmp, offset), ==, &eggplants);
  tt_ptr_op(smartlist_pqueue_pop(sl, cmp, offset), ==, &fireflies);
  OK();
  tt_ptr_op(smartlist_pqueue_pop(sl, cmp, offset), ==, &fish);
  tt_ptr_op(smartlist_pqueue_pop(sl, cmp, offset), ==, &frogs);
  tt_ptr_op(smartlist_pqueue_pop(sl, cmp, offset), ==, &lobsters);
  tt_ptr_op(smartlist_pqueue_pop(sl, cmp, offset), ==, &roquefort);
  OK();
  tt_int_op(smartlist_len(sl), ==, 3);
  tt_ptr_op(smartlist_pqueue_pop(sl, cmp, offset), ==, &squid);
  tt_ptr_op(smartlist_pqueue_pop(sl, cmp, offset), ==, &weissbier);
  tt_ptr_op(smartlist_pqueue_pop(sl, cmp, offset), ==, &zebras);
  tt_int_op(smartlist_len(sl), ==, 0);
  OK();

  /* Now test remove. */
  smartlist_pqueue_add(sl, cmp, offset, &cows);
  smartlist_pqueue_add(sl, cmp, offset, &fish);
  smartlist_pqueue_add(sl, cmp, offset, &frogs);
  smartlist_pqueue_add(sl, cmp, offset, &apples);
  smartlist_pqueue_add(sl, cmp, offset, &squid);
  smartlist_pqueue_add(sl, cmp, offset, &zebras);
  tt_int_op(smartlist_len(sl), ==, 6);
  OK();
  smartlist_pqueue_remove(sl, cmp, offset, &zebras);
  tt_int_op(smartlist_len(sl), ==, 5);
  OK();
  smartlist_pqueue_remove(sl, cmp, offset, &cows);
  tt_int_op(smartlist_len(sl), ==, 4);
  OK();
  smartlist_pqueue_remove(sl, cmp, offset, &apples);
  tt_int_op(smartlist_len(sl), ==, 3);
  OK();
  tt_ptr_op(smartlist_pqueue_pop(sl, cmp, offset), ==, &fish);
  tt_ptr_op(smartlist_pqueue_pop(sl, cmp, offset), ==, &frogs);
  tt_ptr_op(smartlist_pqueue_pop(sl, cmp, offset), ==, &squid);
  tt_int_op(smartlist_len(sl), ==, 0);
  OK();

#undef OK

 end:

  smartlist_free(sl);
}

/** Run unit tests for string-to-void* map functions */
static void
test_container_strmap(void *unused)
{
  strmap_t *map;
  strmap_iter_t *iter;
  const char *k;
  void *v;
  char *visited = NULL;
  smartlist_t *found_keys = NULL;

  map = strmap_new();
  tt_assert(map);
  tt_int_op(strmap_size(map), ==, 0);
  tt_assert(strmap_isempty(map));
  v = strmap_set(map, "K1", (void*)99);
  tt_ptr_op(v, ==, NULL);
  tt_assert(!strmap_isempty(map));
  v = strmap_set(map, "K2", (void*)101);
  tt_ptr_op(v, ==, NULL);
  v = strmap_set(map, "K1", (void*)100);
  tt_int_op((void*)99, ==, v);
  tt_ptr_op(strmap_get(map, "K1"), ==, (void*)100);
  tt_ptr_op(strmap_get(map, "K2"), ==, (void*)101);
  tt_ptr_op(strmap_get(map, "K-not-there"), ==, NULL);
  strmap_assert_ok(map);

  v = strmap_remove(map,"K2");
  strmap_assert_ok(map);
  tt_ptr_op(v, ==, (void*)101);
  tt_ptr_op(strmap_get(map, "K2"), ==, NULL);
  tt_ptr_op(strmap_remove(map, "K2"), ==, NULL);

  strmap_set(map, "K2", (void*)101);
  strmap_set(map, "K3", (void*)102);
  strmap_set(map, "K4", (void*)103);
  tt_int_op(strmap_size(map), ==, 4);
  strmap_assert_ok(map);
  strmap_set(map, "K5", (void*)104);
  strmap_set(map, "K6", (void*)105);
  strmap_assert_ok(map);

  /* Test iterator. */
  iter = strmap_iter_init(map);
  found_keys = smartlist_create();
  while (!strmap_iter_done(iter)) {
    strmap_iter_get(iter,&k,&v);
    smartlist_add(found_keys, xstrdup(k));
    tt_ptr_op(v, ==, strmap_get(map, k));

    if (!strcmp(k, "K2")) {
      iter = strmap_iter_next_rmv(map,iter);
    } else {
      iter = strmap_iter_next(map,iter);
    }
  }

  /* Make sure we removed K2, but not the others. */
  tt_ptr_op(strmap_get(map, "K2"), ==, NULL);
  tt_ptr_op(strmap_get(map, "K5"), ==, (void*)104);
  /* Make sure we visited everyone once */
  smartlist_sort_strings(found_keys);
  visited = smartlist_join_strings(found_keys, ":", 0, NULL);
  tt_str_op(visited, ==, "K1:K2:K3:K4:K5:K6");

  strmap_assert_ok(map);
  /* Clean up after ourselves. */
  strmap_free(map, NULL);
  map = NULL;

  /* Now try some lc functions. */
  map = strmap_new();
  strmap_set_lc(map,"Ab.C", (void*)1);
  tt_ptr_op(strmap_get(map, "ab.c"), ==, (void*)1);
  strmap_assert_ok(map);
  tt_ptr_op(strmap_get_lc(map, "AB.C"), ==, (void*)1);
  tt_ptr_op(strmap_get(map, "AB.C"), ==, NULL);
  tt_ptr_op(strmap_remove_lc(map, "aB.C"), ==, (void*)1);
  strmap_assert_ok(map);
  tt_ptr_op(strmap_get_lc(map, "AB.C"), ==, NULL);

 end:
  if (map)
    strmap_free(map,NULL);
  if (found_keys) {
    SMARTLIST_FOREACH(found_keys, char *, cp, free(cp));
    smartlist_free(found_keys);
  }
  free(visited);
}

/** Run unit tests for getting the median of a list. */
static void
test_container_order_functions(void *unused)
{
  int lst[25], n = 0;
  //  int a=12,b=24,c=25,d=60,e=77;

#define median() median_int(lst, n)

  lst[n++] = 12;
  tt_int_op(median(), ==, 12); /* 12 */
  lst[n++] = 77;
  //smartlist_shuffle(sl);
  tt_int_op(median(), ==, 12); /* 12, 77 */
  lst[n++] = 77;
  //smartlist_shuffle(sl);
  tt_int_op(median(), ==, 77); /* 12, 77, 77 */
  lst[n++] = 24;
  tt_int_op(median(), ==, 24); /* 12,24,77,77 */
  lst[n++] = 60;
  lst[n++] = 12;
  lst[n++] = 25;
  //smartlist_shuffle(sl);
  tt_int_op(median(), ==, 25); /* 12,12,24,25,60,77,77 */
#undef median

 end:
  ;
}

#define T(name)                                          \
  { #name, test_container_##name, 0, NULL, NULL }

struct testcase_t container_tests[] = {
  T(smartlist_basic),
  T(smartlist_strings),
  T(smartlist_overlap),
  T(smartlist_digests),
  T(smartlist_join),
  T(bitarray),
  T(digestset),
  T(strmap),
  T(pqueue),
  T(order_functions),
  END_OF_TESTCASES
};

