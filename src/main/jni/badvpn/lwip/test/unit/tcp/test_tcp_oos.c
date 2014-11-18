#include "test_tcp_oos.h"

#include "lwip/tcp_impl.h"
#include "lwip/stats.h"
#include "tcp_helper.h"

#if !LWIP_STATS || !TCP_STATS || !MEMP_STATS
#error "This tests needs TCP- and MEMP-statistics enabled"
#endif
#if !TCP_QUEUE_OOSEQ
#error "This tests needs TCP_QUEUE_OOSEQ enabled"
#endif

/** CHECK_SEGMENTS_ON_OOSEQ:
 * 1: check count, seqno and len of segments on pcb->ooseq (strict)
 * 0: only check that bytes are received in correct order (less strict) */
#define CHECK_SEGMENTS_ON_OOSEQ 1

#if CHECK_SEGMENTS_ON_OOSEQ
#define EXPECT_OOSEQ(x) EXPECT(x)
#else
#define EXPECT_OOSEQ(x)
#endif

/* helper functions */

/** Get the numbers of segments on the ooseq list */
static int tcp_oos_count(struct tcp_pcb* pcb)
{
  int num = 0;
  struct tcp_seg* seg = pcb->ooseq;
  while(seg != NULL) {
    num++;
    seg = seg->next;
  }
  return num;
}

/** Get the numbers of pbufs on the ooseq list */
static int tcp_oos_pbuf_count(struct tcp_pcb* pcb)
{
  int num = 0;
  struct tcp_seg* seg = pcb->ooseq;
  while(seg != NULL) {
    num += pbuf_clen(seg->p);
    seg = seg->next;
  }
  return num;
}

/** Get the seqno of a segment (by index) on the ooseq list
 *
 * @param pcb the pcb to check for ooseq segments
 * @param seg_index index of the segment on the ooseq list
 * @return seqno of the segment
 */
static u32_t
tcp_oos_seg_seqno(struct tcp_pcb* pcb, int seg_index)
{
  int num = 0;
  struct tcp_seg* seg = pcb->ooseq;

  /* then check the actual segment */
  while(seg != NULL) {
    if(num == seg_index) {
      return seg->tcphdr->seqno;
    }
    num++;
    seg = seg->next;
  }
  fail();
  return 0;
}

/** Get the tcplen (datalen + SYN/FIN) of a segment (by index) on the ooseq list
 *
 * @param pcb the pcb to check for ooseq segments
 * @param seg_index index of the segment on the ooseq list
 * @return tcplen of the segment
 */
static int
tcp_oos_seg_tcplen(struct tcp_pcb* pcb, int seg_index)
{
  int num = 0;
  struct tcp_seg* seg = pcb->ooseq;

  /* then check the actual segment */
  while(seg != NULL) {
    if(num == seg_index) {
      return TCP_TCPLEN(seg);
    }
    num++;
    seg = seg->next;
  }
  fail();
  return -1;
}

/** Get the tcplen (datalen + SYN/FIN) of all segments on the ooseq list
 *
 * @param pcb the pcb to check for ooseq segments
 * @return tcplen of all segment
 */
static int
tcp_oos_tcplen(struct tcp_pcb* pcb)
{
  int len = 0;
  struct tcp_seg* seg = pcb->ooseq;

  /* then check the actual segment */
  while(seg != NULL) {
    len += TCP_TCPLEN(seg);
    seg = seg->next;
  }
  return len;
}

/* Setup/teardown functions */

static void
tcp_oos_setup(void)
{
  tcp_remove_all();
}

static void
tcp_oos_teardown(void)
{
  tcp_remove_all();
  netif_list = NULL;
  netif_default = NULL;
}



/* Test functions */

/** create multiple segments and pass them to tcp_input in a wrong
 * order to see if ooseq-caching works correctly
 * FIN is received in out-of-sequence segments only */
START_TEST(test_tcp_recv_ooseq_FIN_OOSEQ)
{
  struct test_tcp_counters counters;
  struct tcp_pcb* pcb;
  struct pbuf *p_8_9, *p_4_8, *p_4_10, *p_2_14, *p_fin, *pinseq;
  char data[] = {
     1,  2,  3,  4,
     5,  6,  7,  8,
     9, 10, 11, 12,
    13, 14, 15, 16};
  ip_addr_t remote_ip, local_ip, netmask;
  u16_t data_len;
  u16_t remote_port = 0x100, local_port = 0x101;
  struct netif netif;
  LWIP_UNUSED_ARG(_i);

  /* initialize local vars */
  memset(&netif, 0, sizeof(netif));
  IP4_ADDR(&local_ip, 192, 168, 1, 1);
  IP4_ADDR(&remote_ip, 192, 168, 1, 2);
  IP4_ADDR(&netmask,   255, 255, 255, 0);
  test_tcp_init_netif(&netif, NULL, &local_ip, &netmask);
  data_len = sizeof(data);
  /* initialize counter struct */
  memset(&counters, 0, sizeof(counters));
  counters.expected_data_len = data_len;
  counters.expected_data = data;

  /* create and initialize the pcb */
  pcb = test_tcp_new_counters_pcb(&counters);
  EXPECT_RET(pcb != NULL);
  tcp_set_state(pcb, ESTABLISHED, &local_ip, &remote_ip, local_port, remote_port);

  /* create segments */
  /* pinseq is sent as last segment! */
  pinseq = tcp_create_rx_segment(pcb, &data[0],  4, 0, 0, TCP_ACK);
  /* p1: 8 bytes before FIN */
  /*     seqno: 8..16 */
  p_8_9  = tcp_create_rx_segment(pcb, &data[8],  8, 8, 0, TCP_ACK|TCP_FIN);
  /* p2: 4 bytes before p1, including the first 4 bytes of p1 (partly duplicate) */
  /*     seqno: 4..11 */
  p_4_8  = tcp_create_rx_segment(pcb, &data[4],  8, 4, 0, TCP_ACK);
  /* p3: same as p2 but 2 bytes longer */
  /*     seqno: 4..13 */
  p_4_10 = tcp_create_rx_segment(pcb, &data[4], 10, 4, 0, TCP_ACK);
  /* p4: 14 bytes before FIN, includes data from p1 and p2, plus partly from pinseq */
  /*     seqno: 2..15 */
  p_2_14 = tcp_create_rx_segment(pcb, &data[2], 14, 2, 0, TCP_ACK);
  /* FIN, seqno 16 */
  p_fin  = tcp_create_rx_segment(pcb,     NULL,  0,16, 0, TCP_ACK|TCP_FIN);
  EXPECT(pinseq != NULL);
  EXPECT(p_8_9 != NULL);
  EXPECT(p_4_8 != NULL);
  EXPECT(p_4_10 != NULL);
  EXPECT(p_2_14 != NULL);
  EXPECT(p_fin != NULL);
  if ((pinseq != NULL) && (p_8_9 != NULL) && (p_4_8 != NULL) && (p_4_10 != NULL) && (p_2_14 != NULL) && (p_fin != NULL)) {
    /* pass the segment to tcp_input */
    test_tcp_input(p_8_9, &netif);
    /* check if counters are as expected */
    EXPECT(counters.close_calls == 0);
    EXPECT(counters.recv_calls == 0);
    EXPECT(counters.recved_bytes == 0);
    EXPECT(counters.err_calls == 0);
    /* check ooseq queue */
    EXPECT_OOSEQ(tcp_oos_count(pcb) == 1);
    EXPECT_OOSEQ(tcp_oos_seg_seqno(pcb, 0) == 8);
    EXPECT_OOSEQ(tcp_oos_seg_tcplen(pcb, 0) == 9); /* includes FIN */

    /* pass the segment to tcp_input */
    test_tcp_input(p_4_8, &netif);
    /* check if counters are as expected */
    EXPECT(counters.close_calls == 0);
    EXPECT(counters.recv_calls == 0);
    EXPECT(counters.recved_bytes == 0);
    EXPECT(counters.err_calls == 0);
    /* check ooseq queue */
    EXPECT_OOSEQ(tcp_oos_count(pcb) == 2);
    EXPECT_OOSEQ(tcp_oos_seg_seqno(pcb, 0) == 4);
    EXPECT_OOSEQ(tcp_oos_seg_tcplen(pcb, 0) == 4);
    EXPECT_OOSEQ(tcp_oos_seg_seqno(pcb, 1) == 8);
    EXPECT_OOSEQ(tcp_oos_seg_tcplen(pcb, 1) == 9); /* includes FIN */

    /* pass the segment to tcp_input */
    test_tcp_input(p_4_10, &netif);
    /* check if counters are as expected */
    EXPECT(counters.close_calls == 0);
    EXPECT(counters.recv_calls == 0);
    EXPECT(counters.recved_bytes == 0);
    EXPECT(counters.err_calls == 0);
    /* ooseq queue: unchanged */
    EXPECT_OOSEQ(tcp_oos_count(pcb) == 2);
    EXPECT_OOSEQ(tcp_oos_seg_seqno(pcb, 0) == 4);
    EXPECT_OOSEQ(tcp_oos_seg_tcplen(pcb, 0) == 4);
    EXPECT_OOSEQ(tcp_oos_seg_seqno(pcb, 1) == 8);
    EXPECT_OOSEQ(tcp_oos_seg_tcplen(pcb, 1) == 9); /* includes FIN */

    /* pass the segment to tcp_input */
    test_tcp_input(p_2_14, &netif);
    /* check if counters are as expected */
    EXPECT(counters.close_calls == 0);
    EXPECT(counters.recv_calls == 0);
    EXPECT(counters.recved_bytes == 0);
    EXPECT(counters.err_calls == 0);
    /* check ooseq queue */
    EXPECT_OOSEQ(tcp_oos_count(pcb) == 1);
    EXPECT_OOSEQ(tcp_oos_seg_seqno(pcb, 0) == 2);
    EXPECT_OOSEQ(tcp_oos_seg_tcplen(pcb, 0) == 15); /* includes FIN */

    /* pass the segment to tcp_input */
    test_tcp_input(p_fin, &netif);
    /* check if counters are as expected */
    EXPECT(counters.close_calls == 0);
    EXPECT(counters.recv_calls == 0);
    EXPECT(counters.recved_bytes == 0);
    EXPECT(counters.err_calls == 0);
    /* ooseq queue: unchanged */
    EXPECT_OOSEQ(tcp_oos_count(pcb) == 1);
    EXPECT_OOSEQ(tcp_oos_seg_seqno(pcb, 0) == 2);
    EXPECT_OOSEQ(tcp_oos_seg_tcplen(pcb, 0) == 15); /* includes FIN */

    /* pass the segment to tcp_input */
    test_tcp_input(pinseq, &netif);
    /* check if counters are as expected */
    EXPECT(counters.close_calls == 1);
    EXPECT(counters.recv_calls == 1);
    EXPECT(counters.recved_bytes == data_len);
    EXPECT(counters.err_calls == 0);
    EXPECT(pcb->ooseq == NULL);
  }

  /* make sure the pcb is freed */
  EXPECT(lwip_stats.memp[MEMP_TCP_PCB].used == 1);
  tcp_abort(pcb);
  EXPECT(lwip_stats.memp[MEMP_TCP_PCB].used == 0);
}
END_TEST


/** create multiple segments and pass them to tcp_input in a wrong
 * order to see if ooseq-caching works correctly
 * FIN is received IN-SEQUENCE at the end */
START_TEST(test_tcp_recv_ooseq_FIN_INSEQ)
{
  struct test_tcp_counters counters;
  struct tcp_pcb* pcb;
  struct pbuf *p_1_2, *p_4_8, *p_3_11, *p_2_12, *p_15_1, *p_15_1a, *pinseq, *pinseqFIN;
  char data[] = {
     1,  2,  3,  4,
     5,  6,  7,  8,
     9, 10, 11, 12,
    13, 14, 15, 16};
  ip_addr_t remote_ip, local_ip, netmask;
  u16_t data_len;
  u16_t remote_port = 0x100, local_port = 0x101;
  struct netif netif;
  LWIP_UNUSED_ARG(_i);

  /* initialize local vars */
  memset(&netif, 0, sizeof(netif));
  IP4_ADDR(&local_ip, 192, 168, 1, 1);
  IP4_ADDR(&remote_ip, 192, 168, 1, 2);
  IP4_ADDR(&netmask,   255, 255, 255, 0);
  test_tcp_init_netif(&netif, NULL, &local_ip, &netmask);
  data_len = sizeof(data);
  /* initialize counter struct */
  memset(&counters, 0, sizeof(counters));
  counters.expected_data_len = data_len;
  counters.expected_data = data;

  /* create and initialize the pcb */
  pcb = test_tcp_new_counters_pcb(&counters);
  EXPECT_RET(pcb != NULL);
  tcp_set_state(pcb, ESTABLISHED, &local_ip, &remote_ip, local_port, remote_port);

  /* create segments */
  /* p1: 7 bytes - 2 before FIN */
  /*     seqno: 1..2 */
  p_1_2  = tcp_create_rx_segment(pcb, &data[1],  2, 1, 0, TCP_ACK);
  /* p2: 4 bytes before p1, including the first 4 bytes of p1 (partly duplicate) */
  /*     seqno: 4..11 */
  p_4_8  = tcp_create_rx_segment(pcb, &data[4],  8, 4, 0, TCP_ACK);
  /* p3: same as p2 but 2 bytes longer and one byte more at the front */
  /*     seqno: 3..13 */
  p_3_11 = tcp_create_rx_segment(pcb, &data[3], 11, 3, 0, TCP_ACK);
  /* p4: 13 bytes - 2 before FIN - should be ignored as contained in p1 and p3 */
  /*     seqno: 2..13 */
  p_2_12 = tcp_create_rx_segment(pcb, &data[2], 12, 2, 0, TCP_ACK);
  /* pinseq is the first segment that is held back to create ooseq! */
  /*     seqno: 0..3 */
  pinseq = tcp_create_rx_segment(pcb, &data[0],  4, 0, 0, TCP_ACK);
  /* p5: last byte before FIN */
  /*     seqno: 15 */
  p_15_1 = tcp_create_rx_segment(pcb, &data[15], 1, 15, 0, TCP_ACK);
  /* p6: same as p5, should be ignored */
  p_15_1a= tcp_create_rx_segment(pcb, &data[15], 1, 15, 0, TCP_ACK);
  /* pinseqFIN: last 2 bytes plus FIN */
  /*     only segment containing seqno 14 and FIN */
  pinseqFIN = tcp_create_rx_segment(pcb,  &data[14], 2, 14, 0, TCP_ACK|TCP_FIN);
  EXPECT(pinseq != NULL);
  EXPECT(p_1_2 != NULL);
  EXPECT(p_4_8 != NULL);
  EXPECT(p_3_11 != NULL);
  EXPECT(p_2_12 != NULL);
  EXPECT(p_15_1 != NULL);
  EXPECT(p_15_1a != NULL);
  EXPECT(pinseqFIN != NULL);
  if ((pinseq != NULL) && (p_1_2 != NULL) && (p_4_8 != NULL) && (p_3_11 != NULL) && (p_2_12 != NULL)
    && (p_15_1 != NULL) && (p_15_1a != NULL) && (pinseqFIN != NULL)) {
    /* pass the segment to tcp_input */
    test_tcp_input(p_1_2, &netif);
    /* check if counters are as expected */
    EXPECT(counters.close_calls == 0);
    EXPECT(counters.recv_calls == 0);
    EXPECT(counters.recved_bytes == 0);
    EXPECT(counters.err_calls == 0);
    /* check ooseq queue */
    EXPECT_OOSEQ(tcp_oos_count(pcb) == 1);
    EXPECT_OOSEQ(tcp_oos_seg_seqno(pcb, 0) == 1);
    EXPECT_OOSEQ(tcp_oos_seg_tcplen(pcb, 0) == 2);

    /* pass the segment to tcp_input */
    test_tcp_input(p_4_8, &netif);
    /* check if counters are as expected */
    EXPECT(counters.close_calls == 0);
    EXPECT(counters.recv_calls == 0);
    EXPECT(counters.recved_bytes == 0);
    EXPECT(counters.err_calls == 0);
    /* check ooseq queue */
    EXPECT_OOSEQ(tcp_oos_count(pcb) == 2);
    EXPECT_OOSEQ(tcp_oos_seg_seqno(pcb, 0) == 1);
    EXPECT_OOSEQ(tcp_oos_seg_tcplen(pcb, 0) == 2);
    EXPECT_OOSEQ(tcp_oos_seg_seqno(pcb, 1) == 4);
    EXPECT_OOSEQ(tcp_oos_seg_tcplen(pcb, 1) == 8);

    /* pass the segment to tcp_input */
    test_tcp_input(p_3_11, &netif);
    /* check if counters are as expected */
    EXPECT(counters.close_calls == 0);
    EXPECT(counters.recv_calls == 0);
    EXPECT(counters.recved_bytes == 0);
    EXPECT(counters.err_calls == 0);
    /* check ooseq queue */
    EXPECT_OOSEQ(tcp_oos_count(pcb) == 2);
    EXPECT_OOSEQ(tcp_oos_seg_seqno(pcb, 0) == 1);
    EXPECT_OOSEQ(tcp_oos_seg_tcplen(pcb, 0) == 2);
    /* p_3_11 has removed p_4_8 from ooseq */
    EXPECT_OOSEQ(tcp_oos_seg_seqno(pcb, 1) == 3);
    EXPECT_OOSEQ(tcp_oos_seg_tcplen(pcb, 1) == 11);

    /* pass the segment to tcp_input */
    test_tcp_input(p_2_12, &netif);
    /* check if counters are as expected */
    EXPECT(counters.close_calls == 0);
    EXPECT(counters.recv_calls == 0);
    EXPECT(counters.recved_bytes == 0);
    EXPECT(counters.err_calls == 0);
    /* check ooseq queue */
    EXPECT_OOSEQ(tcp_oos_count(pcb) == 2);
    EXPECT_OOSEQ(tcp_oos_seg_seqno(pcb, 0) == 1);
    EXPECT_OOSEQ(tcp_oos_seg_tcplen(pcb, 0) == 1);
    EXPECT_OOSEQ(tcp_oos_seg_seqno(pcb, 1) == 2);
    EXPECT_OOSEQ(tcp_oos_seg_tcplen(pcb, 1) == 12);

    /* pass the segment to tcp_input */
    test_tcp_input(pinseq, &netif);
    /* check if counters are as expected */
    EXPECT(counters.close_calls == 0);
    EXPECT(counters.recv_calls == 1);
    EXPECT(counters.recved_bytes == 14);
    EXPECT(counters.err_calls == 0);
    EXPECT(pcb->ooseq == NULL);

    /* pass the segment to tcp_input */
    test_tcp_input(p_15_1, &netif);
    /* check if counters are as expected */
    EXPECT(counters.close_calls == 0);
    EXPECT(counters.recv_calls == 1);
    EXPECT(counters.recved_bytes == 14);
    EXPECT(counters.err_calls == 0);
    /* check ooseq queue */
    EXPECT_OOSEQ(tcp_oos_count(pcb) == 1);
    EXPECT_OOSEQ(tcp_oos_seg_seqno(pcb, 0) == 15);
    EXPECT_OOSEQ(tcp_oos_seg_tcplen(pcb, 0) == 1);

    /* pass the segment to tcp_input */
    test_tcp_input(p_15_1a, &netif);
    /* check if counters are as expected */
    EXPECT(counters.close_calls == 0);
    EXPECT(counters.recv_calls == 1);
    EXPECT(counters.recved_bytes == 14);
    EXPECT(counters.err_calls == 0);
    /* check ooseq queue: unchanged */
    EXPECT_OOSEQ(tcp_oos_count(pcb) == 1);
    EXPECT_OOSEQ(tcp_oos_seg_seqno(pcb, 0) == 15);
    EXPECT_OOSEQ(tcp_oos_seg_tcplen(pcb, 0) == 1);

    /* pass the segment to tcp_input */
    test_tcp_input(pinseqFIN, &netif);
    /* check if counters are as expected */
    EXPECT(counters.close_calls == 1);
    EXPECT(counters.recv_calls == 2);
    EXPECT(counters.recved_bytes == data_len);
    EXPECT(counters.err_calls == 0);
    EXPECT(pcb->ooseq == NULL);
  }

  /* make sure the pcb is freed */
  EXPECT(lwip_stats.memp[MEMP_TCP_PCB].used == 1);
  tcp_abort(pcb);
  EXPECT(lwip_stats.memp[MEMP_TCP_PCB].used == 0);
}
END_TEST

static char data_full_wnd[TCP_WND];

/** create multiple segments and pass them to tcp_input with the first segment missing
 * to simulate overruning the rxwin with ooseq queueing enabled */
START_TEST(test_tcp_recv_ooseq_overrun_rxwin)
{
#if !TCP_OOSEQ_MAX_BYTES && !TCP_OOSEQ_MAX_PBUFS
  int i, k;
  struct test_tcp_counters counters;
  struct tcp_pcb* pcb;
  struct pbuf *pinseq, *p_ovr;
  ip_addr_t remote_ip, local_ip, netmask;
  u16_t remote_port = 0x100, local_port = 0x101;
  struct netif netif;
  int datalen = 0;
  int datalen2;

  for(i = 0; i < sizeof(data_full_wnd); i++) {
    data_full_wnd[i] = (char)i;
  }

  /* initialize local vars */
  memset(&netif, 0, sizeof(netif));
  IP4_ADDR(&local_ip, 192, 168, 1, 1);
  IP4_ADDR(&remote_ip, 192, 168, 1, 2);
  IP4_ADDR(&netmask,   255, 255, 255, 0);
  test_tcp_init_netif(&netif, NULL, &local_ip, &netmask);
  /* initialize counter struct */
  memset(&counters, 0, sizeof(counters));
  counters.expected_data_len = TCP_WND;
  counters.expected_data = data_full_wnd;

  /* create and initialize the pcb */
  pcb = test_tcp_new_counters_pcb(&counters);
  EXPECT_RET(pcb != NULL);
  tcp_set_state(pcb, ESTABLISHED, &local_ip, &remote_ip, local_port, remote_port);
  pcb->rcv_nxt = 0x8000;

  /* create segments */
  /* pinseq is sent as last segment! */
  pinseq = tcp_create_rx_segment(pcb, &data_full_wnd[0],  TCP_MSS, 0, 0, TCP_ACK);

  for(i = TCP_MSS, k = 0; i < TCP_WND; i += TCP_MSS, k++) {
    int count, expected_datalen;
    struct pbuf *p = tcp_create_rx_segment(pcb, &data_full_wnd[TCP_MSS*(k+1)],
                                           TCP_MSS, TCP_MSS*(k+1), 0, TCP_ACK);
    EXPECT_RET(p != NULL);
    /* pass the segment to tcp_input */
    test_tcp_input(p, &netif);
    /* check if counters are as expected */
    EXPECT(counters.close_calls == 0);
    EXPECT(counters.recv_calls == 0);
    EXPECT(counters.recved_bytes == 0);
    EXPECT(counters.err_calls == 0);
    /* check ooseq queue */
    count = tcp_oos_count(pcb);
    EXPECT_OOSEQ(count == k+1);
    datalen = tcp_oos_tcplen(pcb);
    if (i + TCP_MSS < TCP_WND) {
      expected_datalen = (k+1)*TCP_MSS;
    } else {
      expected_datalen = TCP_WND - TCP_MSS;
    }
    if (datalen != expected_datalen) {
      EXPECT_OOSEQ(datalen == expected_datalen);
    }
  }

  /* pass in one more segment, cleary overrunning the rxwin */
  p_ovr = tcp_create_rx_segment(pcb, &data_full_wnd[TCP_MSS*(k+1)], TCP_MSS, TCP_MSS*(k+1), 0, TCP_ACK);
  EXPECT_RET(p_ovr != NULL);
  /* pass the segment to tcp_input */
  test_tcp_input(p_ovr, &netif);
  /* check if counters are as expected */
  EXPECT(counters.close_calls == 0);
  EXPECT(counters.recv_calls == 0);
  EXPECT(counters.recved_bytes == 0);
  EXPECT(counters.err_calls == 0);
  /* check ooseq queue */
  EXPECT_OOSEQ(tcp_oos_count(pcb) == k);
  datalen2 = tcp_oos_tcplen(pcb);
  EXPECT_OOSEQ(datalen == datalen2);

  /* now pass inseq */
  test_tcp_input(pinseq, &netif);
  EXPECT(pcb->ooseq == NULL);

  /* make sure the pcb is freed */
  EXPECT(lwip_stats.memp[MEMP_TCP_PCB].used == 1);
  tcp_abort(pcb);
  EXPECT(lwip_stats.memp[MEMP_TCP_PCB].used == 0);
#endif /* !TCP_OOSEQ_MAX_BYTES && !TCP_OOSEQ_MAX_PBUFS */
  LWIP_UNUSED_ARG(_i);
}
END_TEST

START_TEST(test_tcp_recv_ooseq_max_bytes)
{
#if TCP_OOSEQ_MAX_BYTES && (TCP_OOSEQ_MAX_BYTES < (TCP_WND + 1)) && (PBUF_POOL_BUFSIZE >= (TCP_MSS + PBUF_LINK_HLEN + PBUF_IP_HLEN + PBUF_TRANSPORT_HLEN))
  int i, k;
  struct test_tcp_counters counters;
  struct tcp_pcb* pcb;
  struct pbuf *p_ovr;
  ip_addr_t remote_ip, local_ip, netmask;
  u16_t remote_port = 0x100, local_port = 0x101;
  struct netif netif;
  int datalen = 0;
  int datalen2;

  for(i = 0; i < sizeof(data_full_wnd); i++) {
    data_full_wnd[i] = (char)i;
  }

  /* initialize local vars */
  memset(&netif, 0, sizeof(netif));
  IP4_ADDR(&local_ip, 192, 168, 1, 1);
  IP4_ADDR(&remote_ip, 192, 168, 1, 2);
  IP4_ADDR(&netmask,   255, 255, 255, 0);
  test_tcp_init_netif(&netif, NULL, &local_ip, &netmask);
  /* initialize counter struct */
  memset(&counters, 0, sizeof(counters));
  counters.expected_data_len = TCP_WND;
  counters.expected_data = data_full_wnd;

  /* create and initialize the pcb */
  pcb = test_tcp_new_counters_pcb(&counters);
  EXPECT_RET(pcb != NULL);
  tcp_set_state(pcb, ESTABLISHED, &local_ip, &remote_ip, local_port, remote_port);
  pcb->rcv_nxt = 0x8000;

  /* don't 'recv' the first segment (1 byte) so that all other segments will be ooseq */

  /* create segments and 'recv' them */
  for(k = 1, i = 1; k < TCP_OOSEQ_MAX_BYTES; k += TCP_MSS, i++) {
    int count;
    struct pbuf *p = tcp_create_rx_segment(pcb, &data_full_wnd[k],
                                           TCP_MSS, k, 0, TCP_ACK);
    EXPECT_RET(p != NULL);
    EXPECT_RET(p->next == NULL);
    /* pass the segment to tcp_input */
    test_tcp_input(p, &netif);
    /* check if counters are as expected */
    EXPECT(counters.close_calls == 0);
    EXPECT(counters.recv_calls == 0);
    EXPECT(counters.recved_bytes == 0);
    EXPECT(counters.err_calls == 0);
    /* check ooseq queue */
    count = tcp_oos_pbuf_count(pcb);
    EXPECT_OOSEQ(count == i);
    datalen = tcp_oos_tcplen(pcb);
    EXPECT_OOSEQ(datalen == (i * TCP_MSS));
  }

  /* pass in one more segment, overrunning the limit */
  p_ovr = tcp_create_rx_segment(pcb, &data_full_wnd[k+1], 1, k+1, 0, TCP_ACK);
  EXPECT_RET(p_ovr != NULL);
  /* pass the segment to tcp_input */
  test_tcp_input(p_ovr, &netif);
  /* check if counters are as expected */
  EXPECT(counters.close_calls == 0);
  EXPECT(counters.recv_calls == 0);
  EXPECT(counters.recved_bytes == 0);
  EXPECT(counters.err_calls == 0);
  /* check ooseq queue (ensure the new segment was not accepted) */
  EXPECT_OOSEQ(tcp_oos_count(pcb) == (i-1));
  datalen2 = tcp_oos_tcplen(pcb);
  EXPECT_OOSEQ(datalen2 == ((i-1) * TCP_MSS));

  /* make sure the pcb is freed */
  EXPECT(lwip_stats.memp[MEMP_TCP_PCB].used == 1);
  tcp_abort(pcb);
  EXPECT(lwip_stats.memp[MEMP_TCP_PCB].used == 0);
#endif /* TCP_OOSEQ_MAX_BYTES && (TCP_OOSEQ_MAX_BYTES < (TCP_WND + 1)) && (PBUF_POOL_BUFSIZE >= (TCP_MSS + PBUF_LINK_HLEN + PBUF_IP_HLEN + PBUF_TRANSPORT_HLEN)) */
  LWIP_UNUSED_ARG(_i);
}
END_TEST

START_TEST(test_tcp_recv_ooseq_max_pbufs)
{
#if TCP_OOSEQ_MAX_PBUFS && (TCP_OOSEQ_MAX_PBUFS < ((TCP_WND / TCP_MSS) + 1)) && (PBUF_POOL_BUFSIZE >= (TCP_MSS + PBUF_LINK_HLEN + PBUF_IP_HLEN + PBUF_TRANSPORT_HLEN))
  int i;
  struct test_tcp_counters counters;
  struct tcp_pcb* pcb;
  struct pbuf *p_ovr;
  ip_addr_t remote_ip, local_ip, netmask;
  u16_t remote_port = 0x100, local_port = 0x101;
  struct netif netif;
  int datalen = 0;
  int datalen2;

  for(i = 0; i < sizeof(data_full_wnd); i++) {
    data_full_wnd[i] = (char)i;
  }

  /* initialize local vars */
  memset(&netif, 0, sizeof(netif));
  IP4_ADDR(&local_ip, 192, 168, 1, 1);
  IP4_ADDR(&remote_ip, 192, 168, 1, 2);
  IP4_ADDR(&netmask,   255, 255, 255, 0);
  test_tcp_init_netif(&netif, NULL, &local_ip, &netmask);
  /* initialize counter struct */
  memset(&counters, 0, sizeof(counters));
  counters.expected_data_len = TCP_WND;
  counters.expected_data = data_full_wnd;

  /* create and initialize the pcb */
  pcb = test_tcp_new_counters_pcb(&counters);
  EXPECT_RET(pcb != NULL);
  tcp_set_state(pcb, ESTABLISHED, &local_ip, &remote_ip, local_port, remote_port);
  pcb->rcv_nxt = 0x8000;

  /* don't 'recv' the first segment (1 byte) so that all other segments will be ooseq */

  /* create segments and 'recv' them */
  for(i = 1; i <= TCP_OOSEQ_MAX_PBUFS; i++) {
    int count;
    struct pbuf *p = tcp_create_rx_segment(pcb, &data_full_wnd[i],
                                           1, i, 0, TCP_ACK);
    EXPECT_RET(p != NULL);
    EXPECT_RET(p->next == NULL);
    /* pass the segment to tcp_input */
    test_tcp_input(p, &netif);
    /* check if counters are as expected */
    EXPECT(counters.close_calls == 0);
    EXPECT(counters.recv_calls == 0);
    EXPECT(counters.recved_bytes == 0);
    EXPECT(counters.err_calls == 0);
    /* check ooseq queue */
    count = tcp_oos_pbuf_count(pcb);
    EXPECT_OOSEQ(count == i);
    datalen = tcp_oos_tcplen(pcb);
    EXPECT_OOSEQ(datalen == i);
  }

  /* pass in one more segment, overrunning the limit */
  p_ovr = tcp_create_rx_segment(pcb, &data_full_wnd[i+1], 1, i+1, 0, TCP_ACK);
  EXPECT_RET(p_ovr != NULL);
  /* pass the segment to tcp_input */
  test_tcp_input(p_ovr, &netif);
  /* check if counters are as expected */
  EXPECT(counters.close_calls == 0);
  EXPECT(counters.recv_calls == 0);
  EXPECT(counters.recved_bytes == 0);
  EXPECT(counters.err_calls == 0);
  /* check ooseq queue (ensure the new segment was not accepted) */
  EXPECT_OOSEQ(tcp_oos_count(pcb) == (i-1));
  datalen2 = tcp_oos_tcplen(pcb);
  EXPECT_OOSEQ(datalen2 == (i-1));

  /* make sure the pcb is freed */
  EXPECT(lwip_stats.memp[MEMP_TCP_PCB].used == 1);
  tcp_abort(pcb);
  EXPECT(lwip_stats.memp[MEMP_TCP_PCB].used == 0);
#endif /* TCP_OOSEQ_MAX_PBUFS && (TCP_OOSEQ_MAX_BYTES < (TCP_WND + 1)) && (PBUF_POOL_BUFSIZE >= (TCP_MSS + PBUF_LINK_HLEN + PBUF_IP_HLEN + PBUF_TRANSPORT_HLEN)) */
  LWIP_UNUSED_ARG(_i);
}
END_TEST

static void
check_rx_counters(struct tcp_pcb *pcb, struct test_tcp_counters *counters, u32_t exp_close_calls, u32_t exp_rx_calls,
                  u32_t exp_rx_bytes, u32_t exp_err_calls, int exp_oos_count, int exp_oos_len)
{
  int oos_len;
  EXPECT(counters->close_calls == exp_close_calls);
  EXPECT(counters->recv_calls == exp_rx_calls);
  EXPECT(counters->recved_bytes == exp_rx_bytes);
  EXPECT(counters->err_calls == exp_err_calls);
  /* check that pbuf is queued in ooseq */
  EXPECT_OOSEQ(tcp_oos_count(pcb) == exp_oos_count);
  oos_len = tcp_oos_tcplen(pcb);
  EXPECT_OOSEQ(exp_oos_len == oos_len);
}

/* this test uses 4 packets:
 * - data (len=TCP_MSS)
 * - FIN
 * - data after FIN (len=1) (invalid)
 * - 2nd FIN (invalid)
 *
 * the parameter 'delay_packet' is a bitmask that choses which on these packets is ooseq
 */
static void test_tcp_recv_ooseq_double_FINs(int delay_packet)
{
  int i, k;
  struct test_tcp_counters counters;
  struct tcp_pcb* pcb;
  struct pbuf *p_normal_fin, *p_data_after_fin, *p, *p_2nd_fin_ooseq;
  ip_addr_t remote_ip, local_ip, netmask;
  u16_t remote_port = 0x100, local_port = 0x101;
  struct netif netif;
  u32_t exp_rx_calls = 0, exp_rx_bytes = 0, exp_close_calls = 0, exp_oos_pbufs = 0, exp_oos_tcplen = 0;
  int first_dropped = 0xff;
  int last_dropped = 0;

  for(i = 0; i < sizeof(data_full_wnd); i++) {
    data_full_wnd[i] = (char)i;
  }

  /* initialize local vars */
  memset(&netif, 0, sizeof(netif));
  IP4_ADDR(&local_ip, 192, 168, 1, 1);
  IP4_ADDR(&remote_ip, 192, 168, 1, 2);
  IP4_ADDR(&netmask,   255, 255, 255, 0);
  test_tcp_init_netif(&netif, NULL, &local_ip, &netmask);
  /* initialize counter struct */
  memset(&counters, 0, sizeof(counters));
  counters.expected_data_len = TCP_WND;
  counters.expected_data = data_full_wnd;

  /* create and initialize the pcb */
  pcb = test_tcp_new_counters_pcb(&counters);
  EXPECT_RET(pcb != NULL);
  tcp_set_state(pcb, ESTABLISHED, &local_ip, &remote_ip, local_port, remote_port);
  pcb->rcv_nxt = 0x8000;

  /* create segments */
  p = tcp_create_rx_segment(pcb, &data_full_wnd[0], TCP_MSS, 0, 0, TCP_ACK);
  p_normal_fin = tcp_create_rx_segment(pcb, NULL, 0, TCP_MSS, 0, TCP_ACK|TCP_FIN);
  k = 1;
  p_data_after_fin = tcp_create_rx_segment(pcb, &data_full_wnd[TCP_MSS+1], k, TCP_MSS+1, 0, TCP_ACK);
  p_2nd_fin_ooseq = tcp_create_rx_segment(pcb, NULL, 0, TCP_MSS+1+k, 0, TCP_ACK|TCP_FIN);

  if(delay_packet & 1) {
    /* drop normal data */
    first_dropped = 1;
    last_dropped = 1;
  } else {
    /* send normal data */
    test_tcp_input(p, &netif);
    exp_rx_calls++;
    exp_rx_bytes += TCP_MSS;
  }
  /* check if counters are as expected */
  check_rx_counters(pcb, &counters, exp_close_calls, exp_rx_calls, exp_rx_bytes, 0, exp_oos_pbufs, exp_oos_tcplen);

  if(delay_packet & 2) {
    /* drop FIN */
    if(first_dropped > 2) {
      first_dropped = 2;
    }
    last_dropped = 2;
  } else {
    /* send FIN */
    test_tcp_input(p_normal_fin, &netif);
    if (first_dropped < 2) {
      /* already dropped packets, this one is ooseq */
      exp_oos_pbufs++;
      exp_oos_tcplen++;
    } else {
      /* inseq */
      exp_close_calls++;
    }
  }
  /* check if counters are as expected */
  check_rx_counters(pcb, &counters, exp_close_calls, exp_rx_calls, exp_rx_bytes, 0, exp_oos_pbufs, exp_oos_tcplen);

  if(delay_packet & 4) {
    /* drop data-after-FIN */
    if(first_dropped > 3) {
      first_dropped = 3;
    }
    last_dropped = 3;
  } else {
    /* send data-after-FIN */
    test_tcp_input(p_data_after_fin, &netif);
    if (first_dropped < 3) {
      /* already dropped packets, this one is ooseq */
      if (delay_packet & 2) {
        /* correct FIN was ooseq */
        exp_oos_pbufs++;
        exp_oos_tcplen += k;
      }
    } else {
      /* inseq: no change */
    }
  }
  /* check if counters are as expected */
  check_rx_counters(pcb, &counters, exp_close_calls, exp_rx_calls, exp_rx_bytes, 0, exp_oos_pbufs, exp_oos_tcplen);

  if(delay_packet & 8) {
    /* drop 2nd-FIN */
    if(first_dropped > 4) {
      first_dropped = 4;
    }
    last_dropped = 4;
  } else {
    /* send 2nd-FIN */
    test_tcp_input(p_2nd_fin_ooseq, &netif);
    if (first_dropped < 3) {
      /* already dropped packets, this one is ooseq */
      if (delay_packet & 2) {
        /* correct FIN was ooseq */
        exp_oos_pbufs++;
        exp_oos_tcplen++;
      }
    } else {
      /* inseq: no change */
    }
  }
  /* check if counters are as expected */
  check_rx_counters(pcb, &counters, exp_close_calls, exp_rx_calls, exp_rx_bytes, 0, exp_oos_pbufs, exp_oos_tcplen);

  if(delay_packet & 1) {
    /* dropped normal data before */
    test_tcp_input(p, &netif);
    exp_rx_calls++;
    exp_rx_bytes += TCP_MSS;
    if((delay_packet & 2) == 0) {
      /* normal FIN was NOT delayed */
      exp_close_calls++;
      exp_oos_pbufs = exp_oos_tcplen = 0;
    }
  }
  /* check if counters are as expected */
  check_rx_counters(pcb, &counters, exp_close_calls, exp_rx_calls, exp_rx_bytes, 0, exp_oos_pbufs, exp_oos_tcplen);

  if(delay_packet & 2) {
    /* dropped normal FIN before */
    test_tcp_input(p_normal_fin, &netif);
    exp_close_calls++;
    exp_oos_pbufs = exp_oos_tcplen = 0;
  }
  /* check if counters are as expected */
  check_rx_counters(pcb, &counters, exp_close_calls, exp_rx_calls, exp_rx_bytes, 0, exp_oos_pbufs, exp_oos_tcplen);

  if(delay_packet & 4) {
    /* dropped data-after-FIN before */
    test_tcp_input(p_data_after_fin, &netif);
  }
  /* check if counters are as expected */
  check_rx_counters(pcb, &counters, exp_close_calls, exp_rx_calls, exp_rx_bytes, 0, exp_oos_pbufs, exp_oos_tcplen);

  if(delay_packet & 8) {
    /* dropped 2nd-FIN before */
    test_tcp_input(p_2nd_fin_ooseq, &netif);
  }
  /* check if counters are as expected */
  check_rx_counters(pcb, &counters, exp_close_calls, exp_rx_calls, exp_rx_bytes, 0, exp_oos_pbufs, exp_oos_tcplen);

  /* check that ooseq data has been dumped */
  EXPECT(pcb->ooseq == NULL);

  /* make sure the pcb is freed */
  EXPECT(lwip_stats.memp[MEMP_TCP_PCB].used == 1);
  tcp_abort(pcb);
  EXPECT(lwip_stats.memp[MEMP_TCP_PCB].used == 0);
}

/** create multiple segments and pass them to tcp_input with the first segment missing
 * to simulate overruning the rxwin with ooseq queueing enabled */
#define FIN_TEST(name, num) \
  START_TEST(name) \
  { \
    LWIP_UNUSED_ARG(_i); \
    test_tcp_recv_ooseq_double_FINs(num); \
  } \
  END_TEST
FIN_TEST(test_tcp_recv_ooseq_double_FIN_0, 0)
FIN_TEST(test_tcp_recv_ooseq_double_FIN_1, 1)
FIN_TEST(test_tcp_recv_ooseq_double_FIN_2, 2)
FIN_TEST(test_tcp_recv_ooseq_double_FIN_3, 3)
FIN_TEST(test_tcp_recv_ooseq_double_FIN_4, 4)
FIN_TEST(test_tcp_recv_ooseq_double_FIN_5, 5)
FIN_TEST(test_tcp_recv_ooseq_double_FIN_6, 6)
FIN_TEST(test_tcp_recv_ooseq_double_FIN_7, 7)
FIN_TEST(test_tcp_recv_ooseq_double_FIN_8, 8)
FIN_TEST(test_tcp_recv_ooseq_double_FIN_9, 9)
FIN_TEST(test_tcp_recv_ooseq_double_FIN_10, 10)
FIN_TEST(test_tcp_recv_ooseq_double_FIN_11, 11)
FIN_TEST(test_tcp_recv_ooseq_double_FIN_12, 12)
FIN_TEST(test_tcp_recv_ooseq_double_FIN_13, 13)
FIN_TEST(test_tcp_recv_ooseq_double_FIN_14, 14)
FIN_TEST(test_tcp_recv_ooseq_double_FIN_15, 15)


/** Create the suite including all tests for this module */
Suite *
tcp_oos_suite(void)
{
  TFun tests[] = {
    test_tcp_recv_ooseq_FIN_OOSEQ,
    test_tcp_recv_ooseq_FIN_INSEQ,
    test_tcp_recv_ooseq_overrun_rxwin,
    test_tcp_recv_ooseq_max_bytes,
    test_tcp_recv_ooseq_max_pbufs,
    test_tcp_recv_ooseq_double_FIN_0,
    test_tcp_recv_ooseq_double_FIN_1,
    test_tcp_recv_ooseq_double_FIN_2,
    test_tcp_recv_ooseq_double_FIN_3,
    test_tcp_recv_ooseq_double_FIN_4,
    test_tcp_recv_ooseq_double_FIN_5,
    test_tcp_recv_ooseq_double_FIN_6,
    test_tcp_recv_ooseq_double_FIN_7,
    test_tcp_recv_ooseq_double_FIN_8,
    test_tcp_recv_ooseq_double_FIN_9,
    test_tcp_recv_ooseq_double_FIN_10,
    test_tcp_recv_ooseq_double_FIN_11,
    test_tcp_recv_ooseq_double_FIN_12,
    test_tcp_recv_ooseq_double_FIN_13,
    test_tcp_recv_ooseq_double_FIN_14,
    test_tcp_recv_ooseq_double_FIN_15
  };
  return create_suite("TCP_OOS", tests, sizeof(tests)/sizeof(TFun), tcp_oos_setup, tcp_oos_teardown);
}
