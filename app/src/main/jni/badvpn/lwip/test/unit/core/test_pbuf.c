#include "test_pbuf.h"

#include "lwip/pbuf.h"
#include "lwip/stats.h"

#if !LWIP_STATS || !MEM_STATS ||!MEMP_STATS
#error "This tests needs MEM- and MEMP-statistics enabled"
#endif
#if LWIP_DNS
#error "This test needs DNS turned off (as it mallocs on init)"
#endif

/* Setups/teardown functions */

static void
pbuf_setup(void)
{
}

static void
pbuf_teardown(void)
{
}


/* Test functions */

/** Call pbuf_copy on a pbuf with zero length */
START_TEST(test_pbuf_copy_zero_pbuf)
{
  struct pbuf *p1, *p2, *p3;
  err_t err;
  LWIP_UNUSED_ARG(_i);

  fail_unless(lwip_stats.mem.used == 0);
  fail_unless(lwip_stats.memp[MEMP_PBUF_POOL].used == 0);

  p1 = pbuf_alloc(PBUF_RAW, 1024, PBUF_RAM);
  fail_unless(p1 != NULL);
  fail_unless(p1->ref == 1);

  p2 = pbuf_alloc(PBUF_RAW, 2, PBUF_POOL);
  fail_unless(p2 != NULL);
  fail_unless(p2->ref == 1);
  p2->len = p2->tot_len = 0;

  pbuf_cat(p1, p2);
  fail_unless(p1->ref == 1);
  fail_unless(p2->ref == 1);

  p3 = pbuf_alloc(PBUF_RAW, p1->tot_len, PBUF_POOL);
  err = pbuf_copy(p3, p1);
  fail_unless(err == ERR_VAL);

  pbuf_free(p1);
  pbuf_free(p3);
  fail_unless(lwip_stats.mem.used == 0);

  fail_unless(lwip_stats.mem.used == 0);
  fail_unless(lwip_stats.memp[MEMP_PBUF_POOL].used == 0);
}
END_TEST


/** Create the suite including all tests for this module */
Suite *
pbuf_suite(void)
{
  TFun tests[] = {
    test_pbuf_copy_zero_pbuf
  };
  return create_suite("PBUF", tests, sizeof(tests)/sizeof(TFun), pbuf_setup, pbuf_teardown);
}
