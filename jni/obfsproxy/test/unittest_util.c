/* Copyright 2011 Nick Mathewson, George Kadianakis
   See LICENSE for other credits and copying information
*/

#include "util.h"
#include "protocol.h"
#include "tinytest_macros.h"

static void
test_util_asprintf(void *unused)
{
#define LOREMIPSUM                                              \
  "Lorem ipsum dolor sit amet, consectetur adipisicing elit"
  char *cp=NULL, *cp2=NULL;
  int r;

  /* empty string. */
  r = obfs_asprintf(&cp, "%s", "");
  tt_assert(cp);
  tt_int_op(r, ==, strlen(cp));
  tt_str_op(cp, ==, "");

  /* Short string with some printing in it. */
  r = obfs_asprintf(&cp2, "First=%d, Second=%d", 101, 202);
  tt_assert(cp2);
  tt_int_op(r, ==, strlen(cp2));
  tt_str_op(cp2, ==, "First=101, Second=202");
  tt_assert(cp != cp2);
  free(cp); cp = NULL;
  free(cp2); cp2 = NULL;

  /* Glass-box test: a string exactly 128 characters long. */
  r = obfs_asprintf(&cp, "Lorem1: %sLorem2: %s", LOREMIPSUM, LOREMIPSUM);
  tt_assert(cp);
  tt_int_op(r, ==, 128);
  tt_assert(cp[128] == '\0');
  tt_str_op(cp, ==,
            "Lorem1: "LOREMIPSUM"Lorem2: "LOREMIPSUM);
  free(cp); cp = NULL;

  /* String longer than 128 characters */
  r = obfs_asprintf(&cp, "1: %s 2: %s 3: %s",
                   LOREMIPSUM, LOREMIPSUM, LOREMIPSUM);
  tt_assert(cp);
  tt_int_op(r, ==, strlen(cp));
  tt_str_op(cp, ==, "1: "LOREMIPSUM" 2: "LOREMIPSUM" 3: "LOREMIPSUM);

 end:
  free(cp);
  free(cp2);
}

#define T(name) \
  { #name, test_util_##name, 0, NULL, NULL }

struct testcase_t util_tests[] = {
  T(asprintf),
  END_OF_TESTCASES
};
