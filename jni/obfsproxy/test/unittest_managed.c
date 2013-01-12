/* Copyright 2011 Nick Mathewson, George Kadianakis
   See LICENSE for other credits and copying information
*/

#include "util.h"

#define MANAGED_PRIVATE
#include "managed.h"

#include "tinytest_macros.h"

static void
test_managed_bindaddr_validation(void *unused)
{
  struct option_parsing_case {
    int result;
    int should_succeed;
    const char *bindaddrs;
    const char *transports;
  };
  static struct option_parsing_case cases[] = {
    /* correct */
    { 0, 1, "april-127.0.0.1:2,paris-127.0.0.1:3", "april,paris"},
    /* correct */
    { 0, 1, "april-127.0.0.1:2", "april" },
    /* wrong; transport names */
    { 0, 0, "april-127.0.0.1:2,paris-127.0.0.1:3", "april,pari"},
    /* wrong; no port */
    { 0, 0, "april-127.0.0.1:2,paris-127.0.0.1", "april,paris"},
    /* wrong; wrong port */
    { 0, 0, "april-127.0.0.1:2,paris-127.0.0.1:a", "april,paris"},
    /* wrong; wrong address */
    { 0, 0, "april-127.0.0.1:2,paris-address:5555", "april,paris"},
    /* wrong; bad balance */
    { 0, 0, "april-127.0.0.1:2,paris-127.0.01:3", "april"},
    /* wrong; funky bindaddr  */
    { 0, 0, "april-127.0.0.1:2-in,paris-127.0.01:3", "april,paris"},
    /* wrong; funky bindaddr  */
    { 0, 0, "april,paris-127.0.01:3", "april,paris"},

    { 0, 0, NULL, NULL }
  };

  /* Suppress logs for the duration of this function. */
  log_set_method(LOG_METHOD_NULL, NULL);

  struct option_parsing_case *c;
  for (c = cases; c->bindaddrs; c++) {
    c->result = validate_bindaddrs(c->bindaddrs, c->transports);
    tt_int_op(c->result, ==, c->should_succeed ? 0 : -1);
  }

 end:
  /* Unsuspend logging */
  log_set_method(LOG_METHOD_STDERR, NULL);
}

#define T(name) \
  { #name, test_managed_##name, 0, NULL, NULL }

struct testcase_t managed_tests[] = {
  T(bindaddr_validation),
  END_OF_TESTCASES
};
