#include "test_tcp.h"

#include "lwip/tcp_impl.h"
#include "lwip/stats.h"
#include "tcp_helper.h"

#ifdef _MSC_VER
#pragma warning(disable: 4307) /* we explicitly wrap around TCP seqnos */
#endif

#if !LWIP_STATS || !TCP_STATS || !MEMP_STATS
#error "This tests needs TCP- and MEMP-statistics enabled"
#endif
#if TCP_SND_BUF <= TCP_WND
#error "This tests needs TCP_SND_BUF to be > TCP_WND"
#endif

static u8_t test_tcp_timer;

/* our own version of tcp_tmr so we can reset fast/slow timer state */
static void
test_tcp_tmr(void)
{
  tcp_fasttmr();
  if (++test_tcp_timer & 1) {
    tcp_slowtmr();
  }
}

/* Setups/teardown functions */

static void
tcp_setup(void)
{
  /* reset iss to default (6510) */
  tcp_ticks = 0;
  tcp_ticks = 0 - (tcp_next_iss() - 6510);
  tcp_next_iss();
  tcp_ticks = 0;

  test_tcp_timer = 0;
  tcp_remove_all();
}

static void
tcp_teardown(void)
{
  tcp_remove_all();
  netif_list = NULL;
  netif_default = NULL;
}


/* Test functions */

/** Call tcp_new() and tcp_abort() and test memp stats */
START_TEST(test_tcp_new_abort)
{
  struct tcp_pcb* pcb;
  LWIP_UNUSED_ARG(_i);

  fail_unless(lwip_stats.memp[MEMP_TCP_PCB].used == 0);

  pcb = tcp_new();
  fail_unless(pcb != NULL);
  if (pcb != NULL) {
    fail_unless(lwip_stats.memp[MEMP_TCP_PCB].used == 1);
    tcp_abort(pcb);
    fail_unless(lwip_stats.memp[MEMP_TCP_PCB].used == 0);
  }
}
END_TEST

/** Create an ESTABLISHED pcb and check if receive callback is called */
START_TEST(test_tcp_recv_inseq)
{
  struct test_tcp_counters counters;
  struct tcp_pcb* pcb;
  struct pbuf* p;
  char data[] = {1, 2, 3, 4};
  ip_addr_t remote_ip, local_ip, netmask;
  u16_t data_len;
  u16_t remote_port = 0x100, local_port = 0x101;
  struct netif netif;
  struct test_tcp_txcounters txcounters;
  LWIP_UNUSED_ARG(_i);

  /* initialize local vars */
  memset(&netif, 0, sizeof(netif));
  IP4_ADDR(&local_ip, 192, 168, 1, 1);
  IP4_ADDR(&remote_ip, 192, 168, 1, 2);
  IP4_ADDR(&netmask,   255, 255, 255, 0);
  test_tcp_init_netif(&netif, &txcounters, &local_ip, &netmask);
  data_len = sizeof(data);
  /* initialize counter struct */
  memset(&counters, 0, sizeof(counters));
  counters.expected_data_len = data_len;
  counters.expected_data = data;

  /* create and initialize the pcb */
  pcb = test_tcp_new_counters_pcb(&counters);
  EXPECT_RET(pcb != NULL);
  tcp_set_state(pcb, ESTABLISHED, &local_ip, &remote_ip, local_port, remote_port);

  /* create a segment */
  p = tcp_create_rx_segment(pcb, counters.expected_data, data_len, 0, 0, 0);
  EXPECT(p != NULL);
  if (p != NULL) {
    /* pass the segment to tcp_input */
    test_tcp_input(p, &netif);
    /* check if counters are as expected */
    EXPECT(counters.close_calls == 0);
    EXPECT(counters.recv_calls == 1);
    EXPECT(counters.recved_bytes == data_len);
    EXPECT(counters.err_calls == 0);
  }

  /* make sure the pcb is freed */
  EXPECT(lwip_stats.memp[MEMP_TCP_PCB].used == 1);
  tcp_abort(pcb);
  EXPECT(lwip_stats.memp[MEMP_TCP_PCB].used == 0);
}
END_TEST

/** Provoke fast retransmission by duplicate ACKs and then recover by ACKing all sent data.
 * At the end, send more data. */
START_TEST(test_tcp_fast_retx_recover)
{
  struct netif netif;
  struct test_tcp_txcounters txcounters;
  struct test_tcp_counters counters;
  struct tcp_pcb* pcb;
  struct pbuf* p;
  char data1[] = { 1,  2,  3,  4};
  char data2[] = { 5,  6,  7,  8};
  char data3[] = { 9, 10, 11, 12};
  char data4[] = {13, 14, 15, 16};
  char data5[] = {17, 18, 19, 20};
  char data6[] = {21, 22, 23, 24};
  ip_addr_t remote_ip, local_ip, netmask;
  u16_t remote_port = 0x100, local_port = 0x101;
  err_t err;
  LWIP_UNUSED_ARG(_i);

  /* initialize local vars */
  IP4_ADDR(&local_ip,  192, 168,   1, 1);
  IP4_ADDR(&remote_ip, 192, 168,   1, 2);
  IP4_ADDR(&netmask,   255, 255, 255, 0);
  test_tcp_init_netif(&netif, &txcounters, &local_ip, &netmask);
  memset(&counters, 0, sizeof(counters));

  /* create and initialize the pcb */
  pcb = test_tcp_new_counters_pcb(&counters);
  EXPECT_RET(pcb != NULL);
  tcp_set_state(pcb, ESTABLISHED, &local_ip, &remote_ip, local_port, remote_port);
  pcb->mss = TCP_MSS;
  /* disable initial congestion window (we don't send a SYN here...) */
  pcb->cwnd = pcb->snd_wnd;

  /* send data1 */
  err = tcp_write(pcb, data1, sizeof(data1), TCP_WRITE_FLAG_COPY);
  EXPECT_RET(err == ERR_OK);
  err = tcp_output(pcb);
  EXPECT_RET(err == ERR_OK);
  EXPECT_RET(txcounters.num_tx_calls == 1);
  EXPECT_RET(txcounters.num_tx_bytes == sizeof(data1) + sizeof(struct tcp_hdr) + sizeof(struct ip_hdr));
  memset(&txcounters, 0, sizeof(txcounters));
 /* "recv" ACK for data1 */
  p = tcp_create_rx_segment(pcb, NULL, 0, 0, 4, TCP_ACK);
  EXPECT_RET(p != NULL);
  test_tcp_input(p, &netif);
  EXPECT_RET(txcounters.num_tx_calls == 0);
  EXPECT_RET(pcb->unacked == NULL);
  /* send data2 */
  err = tcp_write(pcb, data2, sizeof(data2), TCP_WRITE_FLAG_COPY);
  EXPECT_RET(err == ERR_OK);
  err = tcp_output(pcb);
  EXPECT_RET(err == ERR_OK);
  EXPECT_RET(txcounters.num_tx_calls == 1);
  EXPECT_RET(txcounters.num_tx_bytes == sizeof(data2) + sizeof(struct tcp_hdr) + sizeof(struct ip_hdr));
  memset(&txcounters, 0, sizeof(txcounters));
  /* duplicate ACK for data1 (data2 is lost) */
  p = tcp_create_rx_segment(pcb, NULL, 0, 0, 0, TCP_ACK);
  EXPECT_RET(p != NULL);
  test_tcp_input(p, &netif);
  EXPECT_RET(txcounters.num_tx_calls == 0);
  EXPECT_RET(pcb->dupacks == 1);
  /* send data3 */
  err = tcp_write(pcb, data3, sizeof(data3), TCP_WRITE_FLAG_COPY);
  EXPECT_RET(err == ERR_OK);
  err = tcp_output(pcb);
  EXPECT_RET(err == ERR_OK);
  /* nagle enabled, no tx calls */
  EXPECT_RET(txcounters.num_tx_calls == 0);
  EXPECT_RET(txcounters.num_tx_bytes == 0);
  memset(&txcounters, 0, sizeof(txcounters));
  /* 2nd duplicate ACK for data1 (data2 and data3 are lost) */
  p = tcp_create_rx_segment(pcb, NULL, 0, 0, 0, TCP_ACK);
  EXPECT_RET(p != NULL);
  test_tcp_input(p, &netif);
  EXPECT_RET(txcounters.num_tx_calls == 0);
  EXPECT_RET(pcb->dupacks == 2);
  /* queue data4, don't send it (unsent-oversize is != 0) */
  err = tcp_write(pcb, data4, sizeof(data4), TCP_WRITE_FLAG_COPY);
  EXPECT_RET(err == ERR_OK);
  /* 3nd duplicate ACK for data1 (data2 and data3 are lost) -> fast retransmission */
  p = tcp_create_rx_segment(pcb, NULL, 0, 0, 0, TCP_ACK);
  EXPECT_RET(p != NULL);
  test_tcp_input(p, &netif);
  /*EXPECT_RET(txcounters.num_tx_calls == 1);*/
  EXPECT_RET(pcb->dupacks == 3);
  memset(&txcounters, 0, sizeof(txcounters));
  /* TODO: check expected data?*/
  
  /* send data5, not output yet */
  err = tcp_write(pcb, data5, sizeof(data5), TCP_WRITE_FLAG_COPY);
  EXPECT_RET(err == ERR_OK);
  /*err = tcp_output(pcb);
  EXPECT_RET(err == ERR_OK);*/
  EXPECT_RET(txcounters.num_tx_calls == 0);
  EXPECT_RET(txcounters.num_tx_bytes == 0);
  memset(&txcounters, 0, sizeof(txcounters));
  {
    int i = 0;
    do
    {
      err = tcp_write(pcb, data6, TCP_MSS, TCP_WRITE_FLAG_COPY);
      i++;
    }while(err == ERR_OK);
    EXPECT_RET(err != ERR_OK);
  }
  err = tcp_output(pcb);
  EXPECT_RET(err == ERR_OK);
  /*EXPECT_RET(txcounters.num_tx_calls == 0);
  EXPECT_RET(txcounters.num_tx_bytes == 0);*/
  memset(&txcounters, 0, sizeof(txcounters));

  /* send even more data */
  err = tcp_write(pcb, data5, sizeof(data5), TCP_WRITE_FLAG_COPY);
  EXPECT_RET(err == ERR_OK);
  err = tcp_output(pcb);
  EXPECT_RET(err == ERR_OK);
  /* ...and even more data */
  err = tcp_write(pcb, data5, sizeof(data5), TCP_WRITE_FLAG_COPY);
  EXPECT_RET(err == ERR_OK);
  err = tcp_output(pcb);
  EXPECT_RET(err == ERR_OK);
  /* ...and even more data */
  err = tcp_write(pcb, data5, sizeof(data5), TCP_WRITE_FLAG_COPY);
  EXPECT_RET(err == ERR_OK);
  err = tcp_output(pcb);
  EXPECT_RET(err == ERR_OK);
  /* ...and even more data */
  err = tcp_write(pcb, data5, sizeof(data5), TCP_WRITE_FLAG_COPY);
  EXPECT_RET(err == ERR_OK);
  err = tcp_output(pcb);
  EXPECT_RET(err == ERR_OK);

  /* send ACKs for data2 and data3 */
  p = tcp_create_rx_segment(pcb, NULL, 0, 0, 12, TCP_ACK);
  EXPECT_RET(p != NULL);
  test_tcp_input(p, &netif);
  /*EXPECT_RET(txcounters.num_tx_calls == 0);*/

  /* ...and even more data */
  err = tcp_write(pcb, data5, sizeof(data5), TCP_WRITE_FLAG_COPY);
  EXPECT_RET(err == ERR_OK);
  err = tcp_output(pcb);
  EXPECT_RET(err == ERR_OK);
  /* ...and even more data */
  err = tcp_write(pcb, data5, sizeof(data5), TCP_WRITE_FLAG_COPY);
  EXPECT_RET(err == ERR_OK);
  err = tcp_output(pcb);
  EXPECT_RET(err == ERR_OK);

#if 0
  /* create expected segment */
  p1 = tcp_create_rx_segment(pcb, counters.expected_data, data_len, 0, 0, 0);
  EXPECT_RET(p != NULL);
  if (p != NULL) {
    /* pass the segment to tcp_input */
    test_tcp_input(p, &netif);
    /* check if counters are as expected */
    EXPECT_RET(counters.close_calls == 0);
    EXPECT_RET(counters.recv_calls == 1);
    EXPECT_RET(counters.recved_bytes == data_len);
    EXPECT_RET(counters.err_calls == 0);
  }
#endif
  /* make sure the pcb is freed */
  EXPECT_RET(lwip_stats.memp[MEMP_TCP_PCB].used == 1);
  tcp_abort(pcb);
  EXPECT_RET(lwip_stats.memp[MEMP_TCP_PCB].used == 0);
}
END_TEST

static u8_t tx_data[TCP_WND*2];

static void
check_seqnos(struct tcp_seg *segs, int num_expected, u32_t *seqnos_expected)
{
  struct tcp_seg *s = segs;
  int i;
  for (i = 0; i < num_expected; i++, s = s->next) {
    EXPECT_RET(s != NULL);
    EXPECT(s->tcphdr->seqno == htonl(seqnos_expected[i]));
  }
  EXPECT(s == NULL);
}

/** Send data with sequence numbers that wrap around the u32_t range.
 * Then, provoke fast retransmission by duplicate ACKs and check that all
 * segment lists are still properly sorted. */
START_TEST(test_tcp_fast_rexmit_wraparound)
{
  struct netif netif;
  struct test_tcp_txcounters txcounters;
  struct test_tcp_counters counters;
  struct tcp_pcb* pcb;
  struct pbuf* p;
  ip_addr_t remote_ip, local_ip, netmask;
  u16_t remote_port = 0x100, local_port = 0x101;
  err_t err;
#define SEQNO1 (0xFFFFFF00 - TCP_MSS)
#define ISS    6510
  u16_t i, sent_total = 0;
  u32_t seqnos[] = {
    SEQNO1,
    SEQNO1 + (1 * TCP_MSS),
    SEQNO1 + (2 * TCP_MSS),
    SEQNO1 + (3 * TCP_MSS),
    SEQNO1 + (4 * TCP_MSS),
    SEQNO1 + (5 * TCP_MSS)};
  LWIP_UNUSED_ARG(_i);

  for (i = 0; i < sizeof(tx_data); i++) {
    tx_data[i] = (u8_t)i;
  }

  /* initialize local vars */
  IP4_ADDR(&local_ip,  192, 168,   1, 1);
  IP4_ADDR(&remote_ip, 192, 168,   1, 2);
  IP4_ADDR(&netmask,   255, 255, 255, 0);
  test_tcp_init_netif(&netif, &txcounters, &local_ip, &netmask);
  memset(&counters, 0, sizeof(counters));

  /* create and initialize the pcb */
  tcp_ticks = SEQNO1 - ISS;
  pcb = test_tcp_new_counters_pcb(&counters);
  EXPECT_RET(pcb != NULL);
  EXPECT(pcb->lastack == SEQNO1);
  tcp_set_state(pcb, ESTABLISHED, &local_ip, &remote_ip, local_port, remote_port);
  pcb->mss = TCP_MSS;
  /* disable initial congestion window (we don't send a SYN here...) */
  pcb->cwnd = 2*TCP_MSS;

  /* send 6 mss-sized segments */
  for (i = 0; i < 6; i++) {
    err = tcp_write(pcb, &tx_data[sent_total], TCP_MSS, TCP_WRITE_FLAG_COPY);
    EXPECT_RET(err == ERR_OK);
    sent_total += TCP_MSS;
  }
  check_seqnos(pcb->unsent, 6, seqnos);
  EXPECT(pcb->unacked == NULL);
  err = tcp_output(pcb);
  EXPECT(txcounters.num_tx_calls == 2);
  EXPECT(txcounters.num_tx_bytes == 2 * (TCP_MSS + 40U));
  memset(&txcounters, 0, sizeof(txcounters));

  check_seqnos(pcb->unacked, 2, seqnos);
  check_seqnos(pcb->unsent, 4, &seqnos[2]);

  /* ACK the first segment */
  p = tcp_create_rx_segment(pcb, NULL, 0, 0, TCP_MSS, TCP_ACK);
  test_tcp_input(p, &netif);
  /* ensure this didn't trigger a retransmission */
  EXPECT(txcounters.num_tx_calls == 1);
  EXPECT(txcounters.num_tx_bytes == TCP_MSS + 40U);
  memset(&txcounters, 0, sizeof(txcounters));
  check_seqnos(pcb->unacked, 2, &seqnos[1]);
  check_seqnos(pcb->unsent, 3, &seqnos[3]);

  /* 3 dupacks */
  EXPECT(pcb->dupacks == 0);
  p = tcp_create_rx_segment(pcb, NULL, 0, 0, 0, TCP_ACK);
  test_tcp_input(p, &netif);
  EXPECT(txcounters.num_tx_calls == 0);
  EXPECT(pcb->dupacks == 1);
  p = tcp_create_rx_segment(pcb, NULL, 0, 0, 0, TCP_ACK);
  test_tcp_input(p, &netif);
  EXPECT(txcounters.num_tx_calls == 0);
  EXPECT(pcb->dupacks == 2);
  /* 3rd dupack -> fast rexmit */
  p = tcp_create_rx_segment(pcb, NULL, 0, 0, 0, TCP_ACK);
  test_tcp_input(p, &netif);
  EXPECT(pcb->dupacks == 3);
  EXPECT(txcounters.num_tx_calls == 4);
  memset(&txcounters, 0, sizeof(txcounters));
  EXPECT(pcb->unsent == NULL);
  check_seqnos(pcb->unacked, 5, &seqnos[1]);

  /* make sure the pcb is freed */
  EXPECT_RET(lwip_stats.memp[MEMP_TCP_PCB].used == 1);
  tcp_abort(pcb);
  EXPECT_RET(lwip_stats.memp[MEMP_TCP_PCB].used == 0);
}
END_TEST

/** Send data with sequence numbers that wrap around the u32_t range.
 * Then, provoke RTO retransmission and check that all
 * segment lists are still properly sorted. */
START_TEST(test_tcp_rto_rexmit_wraparound)
{
  struct netif netif;
  struct test_tcp_txcounters txcounters;
  struct test_tcp_counters counters;
  struct tcp_pcb* pcb;
  ip_addr_t remote_ip, local_ip, netmask;
  u16_t remote_port = 0x100, local_port = 0x101;
  err_t err;
#define SEQNO1 (0xFFFFFF00 - TCP_MSS)
#define ISS    6510
  u16_t i, sent_total = 0;
  u32_t seqnos[] = {
    SEQNO1,
    SEQNO1 + (1 * TCP_MSS),
    SEQNO1 + (2 * TCP_MSS),
    SEQNO1 + (3 * TCP_MSS),
    SEQNO1 + (4 * TCP_MSS),
    SEQNO1 + (5 * TCP_MSS)};
  LWIP_UNUSED_ARG(_i);

  for (i = 0; i < sizeof(tx_data); i++) {
    tx_data[i] = (u8_t)i;
  }

  /* initialize local vars */
  IP4_ADDR(&local_ip,  192, 168,   1, 1);
  IP4_ADDR(&remote_ip, 192, 168,   1, 2);
  IP4_ADDR(&netmask,   255, 255, 255, 0);
  test_tcp_init_netif(&netif, &txcounters, &local_ip, &netmask);
  memset(&counters, 0, sizeof(counters));

  /* create and initialize the pcb */
  tcp_ticks = 0;
  tcp_ticks = 0 - tcp_next_iss();
  tcp_ticks = SEQNO1 - tcp_next_iss();
  pcb = test_tcp_new_counters_pcb(&counters);
  EXPECT_RET(pcb != NULL);
  EXPECT(pcb->lastack == SEQNO1);
  tcp_set_state(pcb, ESTABLISHED, &local_ip, &remote_ip, local_port, remote_port);
  pcb->mss = TCP_MSS;
  /* disable initial congestion window (we don't send a SYN here...) */
  pcb->cwnd = 2*TCP_MSS;

  /* send 6 mss-sized segments */
  for (i = 0; i < 6; i++) {
    err = tcp_write(pcb, &tx_data[sent_total], TCP_MSS, TCP_WRITE_FLAG_COPY);
    EXPECT_RET(err == ERR_OK);
    sent_total += TCP_MSS;
  }
  check_seqnos(pcb->unsent, 6, seqnos);
  EXPECT(pcb->unacked == NULL);
  err = tcp_output(pcb);
  EXPECT(txcounters.num_tx_calls == 2);
  EXPECT(txcounters.num_tx_bytes == 2 * (TCP_MSS + 40U));
  memset(&txcounters, 0, sizeof(txcounters));

  check_seqnos(pcb->unacked, 2, seqnos);
  check_seqnos(pcb->unsent, 4, &seqnos[2]);

  /* call the tcp timer some times */
  for (i = 0; i < 10; i++) {
    test_tcp_tmr();
    EXPECT(txcounters.num_tx_calls == 0);
  }
  /* 11th call to tcp_tmr: RTO rexmit fires */
  test_tcp_tmr();
  EXPECT(txcounters.num_tx_calls == 1);
  check_seqnos(pcb->unacked, 1, seqnos);
  check_seqnos(pcb->unsent, 5, &seqnos[1]);

  /* fake greater cwnd */
  pcb->cwnd = pcb->snd_wnd;
  /* send more data */
  err = tcp_output(pcb);
  EXPECT(err == ERR_OK);
  /* check queues are sorted */
  EXPECT(pcb->unsent == NULL);
  check_seqnos(pcb->unacked, 6, seqnos);

  /* make sure the pcb is freed */
  EXPECT_RET(lwip_stats.memp[MEMP_TCP_PCB].used == 1);
  tcp_abort(pcb);
  EXPECT_RET(lwip_stats.memp[MEMP_TCP_PCB].used == 0);
}
END_TEST

/** Provoke fast retransmission by duplicate ACKs and then recover by ACKing all sent data.
 * At the end, send more data. */
static void test_tcp_tx_full_window_lost(u8_t zero_window_probe_from_unsent)
{
  struct netif netif;
  struct test_tcp_txcounters txcounters;
  struct test_tcp_counters counters;
  struct tcp_pcb* pcb;
  struct pbuf *p;
  ip_addr_t remote_ip, local_ip, netmask;
  u16_t remote_port = 0x100, local_port = 0x101;
  err_t err;
  u16_t sent_total, i;
  u8_t expected = 0xFE;

  for (i = 0; i < sizeof(tx_data); i++) {
    u8_t d = (u8_t)i;
    if (d == 0xFE) {
      d = 0xF0;
    }
    tx_data[i] = d;
  }
  if (zero_window_probe_from_unsent) {
    tx_data[TCP_WND] = expected;
  } else {
    tx_data[0] = expected;
  }

  /* initialize local vars */
  IP4_ADDR(&local_ip,  192, 168,   1, 1);
  IP4_ADDR(&remote_ip, 192, 168,   1, 2);
  IP4_ADDR(&netmask,   255, 255, 255, 0);
  test_tcp_init_netif(&netif, &txcounters, &local_ip, &netmask);
  memset(&counters, 0, sizeof(counters));
  memset(&txcounters, 0, sizeof(txcounters));

  /* create and initialize the pcb */
  pcb = test_tcp_new_counters_pcb(&counters);
  EXPECT_RET(pcb != NULL);
  tcp_set_state(pcb, ESTABLISHED, &local_ip, &remote_ip, local_port, remote_port);
  pcb->mss = TCP_MSS;
  /* disable initial congestion window (we don't send a SYN here...) */
  pcb->cwnd = pcb->snd_wnd;

  /* send a full window (minus 1 packets) of TCP data in MSS-sized chunks */
  sent_total = 0;
  if ((TCP_WND - TCP_MSS) % TCP_MSS != 0) {
    u16_t initial_data_len = (TCP_WND - TCP_MSS) % TCP_MSS;
    err = tcp_write(pcb, &tx_data[sent_total], initial_data_len, TCP_WRITE_FLAG_COPY);
    EXPECT_RET(err == ERR_OK);
    err = tcp_output(pcb);
    EXPECT_RET(err == ERR_OK);
    EXPECT(txcounters.num_tx_calls == 1);
    EXPECT(txcounters.num_tx_bytes == initial_data_len + 40U);
    memset(&txcounters, 0, sizeof(txcounters));
    sent_total += initial_data_len;
  }
  for (; sent_total < (TCP_WND - TCP_MSS); sent_total += TCP_MSS) {
    err = tcp_write(pcb, &tx_data[sent_total], TCP_MSS, TCP_WRITE_FLAG_COPY);
    EXPECT_RET(err == ERR_OK);
    err = tcp_output(pcb);
    EXPECT_RET(err == ERR_OK);
    EXPECT(txcounters.num_tx_calls == 1);
    EXPECT(txcounters.num_tx_bytes == TCP_MSS + 40U);
    memset(&txcounters, 0, sizeof(txcounters));
  }
  EXPECT(sent_total == (TCP_WND - TCP_MSS));

  /* now ACK the packet before the first */
  p = tcp_create_rx_segment(pcb, NULL, 0, 0, 0, TCP_ACK);
  test_tcp_input(p, &netif);
  /* ensure this didn't trigger a retransmission */
  EXPECT(txcounters.num_tx_calls == 0);
  EXPECT(txcounters.num_tx_bytes == 0);

  EXPECT(pcb->persist_backoff == 0);
  /* send the last packet, now a complete window has been sent */
  err = tcp_write(pcb, &tx_data[sent_total], TCP_MSS, TCP_WRITE_FLAG_COPY);
  sent_total += TCP_MSS;
  EXPECT_RET(err == ERR_OK);
  err = tcp_output(pcb);
  EXPECT_RET(err == ERR_OK);
  EXPECT(txcounters.num_tx_calls == 1);
  EXPECT(txcounters.num_tx_bytes == TCP_MSS + 40U);
  memset(&txcounters, 0, sizeof(txcounters));
  EXPECT(pcb->persist_backoff == 0);

  if (zero_window_probe_from_unsent) {
    /* ACK all data but close the TX window */
    p = tcp_create_rx_segment_wnd(pcb, NULL, 0, 0, TCP_WND, TCP_ACK, 0);
    test_tcp_input(p, &netif);
    /* ensure this didn't trigger any transmission */
    EXPECT(txcounters.num_tx_calls == 0);
    EXPECT(txcounters.num_tx_bytes == 0);
    EXPECT(pcb->persist_backoff == 1);
  }

  /* send one byte more (out of window) -> persist timer starts */
  err = tcp_write(pcb, &tx_data[sent_total], 1, TCP_WRITE_FLAG_COPY);
  EXPECT_RET(err == ERR_OK);
  err = tcp_output(pcb);
  EXPECT_RET(err == ERR_OK);
  EXPECT(txcounters.num_tx_calls == 0);
  EXPECT(txcounters.num_tx_bytes == 0);
  memset(&txcounters, 0, sizeof(txcounters));
  if (!zero_window_probe_from_unsent) {
    /* no persist timer unless a zero window announcement has been received */
    EXPECT(pcb->persist_backoff == 0);
  } else {
    EXPECT(pcb->persist_backoff == 1);

    /* call tcp_timer some more times to let persist timer count up */
    for (i = 0; i < 4; i++) {
      test_tcp_tmr();
      EXPECT(txcounters.num_tx_calls == 0);
      EXPECT(txcounters.num_tx_bytes == 0);
    }

    /* this should trigger the zero-window-probe */
    txcounters.copy_tx_packets = 1;
    test_tcp_tmr();
    txcounters.copy_tx_packets = 0;
    EXPECT(txcounters.num_tx_calls == 1);
    EXPECT(txcounters.num_tx_bytes == 1 + 40U);
    EXPECT(txcounters.tx_packets != NULL);
    if (txcounters.tx_packets != NULL) {
      u8_t sent;
      u16_t ret;
      ret = pbuf_copy_partial(txcounters.tx_packets, &sent, 1, 40U);
      EXPECT(ret == 1);
      EXPECT(sent == expected);
    }
    if (txcounters.tx_packets != NULL) {
      pbuf_free(txcounters.tx_packets);
      txcounters.tx_packets = NULL;
    }
  }

  /* make sure the pcb is freed */
  EXPECT_RET(lwip_stats.memp[MEMP_TCP_PCB].used == 1);
  tcp_abort(pcb);
  EXPECT_RET(lwip_stats.memp[MEMP_TCP_PCB].used == 0);
}

START_TEST(test_tcp_tx_full_window_lost_from_unsent)
{
  LWIP_UNUSED_ARG(_i);
  test_tcp_tx_full_window_lost(1);
}
END_TEST

START_TEST(test_tcp_tx_full_window_lost_from_unacked)
{
  LWIP_UNUSED_ARG(_i);
  test_tcp_tx_full_window_lost(0);
}
END_TEST

/** Create the suite including all tests for this module */
Suite *
tcp_suite(void)
{
  TFun tests[] = {
    test_tcp_new_abort,
    test_tcp_recv_inseq,
    test_tcp_fast_retx_recover,
    test_tcp_fast_rexmit_wraparound,
    test_tcp_rto_rexmit_wraparound,
    test_tcp_tx_full_window_lost_from_unacked,
    test_tcp_tx_full_window_lost_from_unsent
  };
  return create_suite("TCP", tests, sizeof(tests)/sizeof(TFun), tcp_setup, tcp_teardown);
}
