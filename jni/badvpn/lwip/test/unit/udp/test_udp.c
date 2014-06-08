#include "test_udp.h"

#include "lwip/udp.h"
#include "lwip/stats.h"

#if !LWIP_STATS || !UDP_STATS || !MEMP_STATS
#error "This tests needs UDP- and MEMP-statistics enabled"
#endif

/* Helper functions */
static void
udp_remove_all(void)
{
  struct udp_pcb *pcb = udp_pcbs;
  struct udp_pcb *pcb2;

  while(pcb != NULL) {
    pcb2 = pcb;
    pcb = pcb->next;
    udp_remove(pcb2);
  }
  fail_unless(lwip_stats.memp[MEMP_UDP_PCB].used == 0);
}

/* Setups/teardown functions */

static void
udp_setup(void)
{
  udp_remove_all();
}

static void
udp_teardown(void)
{
  udp_remove_all();
}


/* Test functions */

START_TEST(test_udp_new_remove)
{
  struct udp_pcb* pcb;
  LWIP_UNUSED_ARG(_i);

  fail_unless(lwip_stats.memp[MEMP_UDP_PCB].used == 0);

  pcb = udp_new();
  fail_unless(pcb != NULL);
  if (pcb != NULL) {
    fail_unless(lwip_stats.memp[MEMP_UDP_PCB].used == 1);
    udp_remove(pcb);
    fail_unless(lwip_stats.memp[MEMP_UDP_PCB].used == 0);
  }
}
END_TEST


/** Create the suite including all tests for this module */
Suite *
udp_suite(void)
{
  TFun tests[] = {
    test_udp_new_remove,
  };
  return create_suite("UDP", tests, sizeof(tests)/sizeof(TFun), udp_setup, udp_teardown);
}
