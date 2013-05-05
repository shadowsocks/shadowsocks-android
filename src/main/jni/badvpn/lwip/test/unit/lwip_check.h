#ifndef __LWIP_CHECK_H__
#define __LWIP_CHECK_H__

/* Common header file for lwIP unit tests using the check framework */

#include <config.h>
#include <check.h>
#include <stdlib.h>

#define FAIL_RET() do { fail(); return; } while(0)
#define EXPECT(x) fail_unless(x)
#define EXPECT_RET(x) do { fail_unless(x); if(!(x)) { return; }} while(0)
#define EXPECT_RETX(x, y) do { fail_unless(x); if(!(x)) { return y; }} while(0)
#define EXPECT_RETNULL(x) EXPECT_RETX(x, NULL)

/** typedef for a function returning a test suite */
typedef Suite* (suite_getter_fn)(void);

/** Create a test suite */
static Suite* create_suite(const char* name, TFun *tests, size_t num_tests, SFun setup, SFun teardown)
{
  size_t i;
  Suite *s = suite_create(name);

  for(i = 0; i < num_tests; i++) {
    /* Core test case */
    TCase *tc_core = tcase_create("Core");
    if ((setup != NULL) || (teardown != NULL)) {
      tcase_add_checked_fixture(tc_core, setup, teardown);
    }
    tcase_add_test(tc_core, tests[i]);
    suite_add_tcase(s, tc_core);
  }
  return s;
}

#endif /* __LWIP_CHECK_H__ */
