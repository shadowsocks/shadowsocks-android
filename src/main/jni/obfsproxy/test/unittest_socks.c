/* Copyright 2011 Nick Mathewson, George Kadianakis
   See LICENSE for other credits and copying information
*/

#include "util.h"
#include "tinytest_macros.h"

#define SOCKS_PRIVATE
#include "socks.h"
#include "crypt.h"

#include <event2/buffer.h>

/* All the tests below use this test environment.  Some of them
   do not need both evbuffers, but we give them two anyway.  */
struct test_socks_state
{
  struct evbuffer *dest;
  struct evbuffer *source;
  socks_state_t *state;
};

static int
cleanup_socks_state(const struct testcase_t *unused, void *data)
{
  struct test_socks_state *s = (struct test_socks_state *)data;

  if (s->dest)
    evbuffer_free(s->dest);
  if (s->source)
    evbuffer_free(s->source);
  if (s->state)
    socks_state_free(s->state);
  free(data);
  return 1;
}

static void *
setup_socks_state(const struct testcase_t *unused)
{
  struct test_socks_state *s = xzalloc(sizeof(struct test_socks_state));

  s->dest = evbuffer_new();
  tt_assert(s->dest);
  s->source = evbuffer_new();
  tt_assert(s->source);
  s->state = socks_state_new();
  tt_assert(s->state);
  return s;

 end:
  cleanup_socks_state(NULL, s);
  return NULL;
}

static const struct testcase_setup_t socks_fixture =
  { setup_socks_state, cleanup_socks_state };

/**
   This function tests the negotiation phase of the SOCKS5 protocol.
   It sends broken 'Method Negotiation Packets' and it verifies that
   the SOCKS server detected the errors. It also sends some correct
   packets and it expects the server to like them.
*/
static void
test_socks_socks5_send_negotiation(void *data)
{
  struct test_socks_state *s = (struct test_socks_state *)data;

  /* First test:
     Only one method: NOAUTH.
     SOCKS proxy should like this. */
  uchar req1[2];
  req1[0] = 1;
  req1[1] = 0;

  evbuffer_add(s->source, req1, 2);

  tt_int_op(SOCKS_GOOD, ==, socks5_handle_negotiation(s->source,s->dest,s->state));
  tt_int_op(0, ==, evbuffer_get_length(s->source));

  uchar rep1[2];
  tt_int_op(2, ==, evbuffer_remove(s->dest,rep1,2));
  tt_assert(rep1[0] == 5);
  tt_assert(rep1[1] == 0);

  /* Second test:
     Ten methods: One of them NOAUTH */
  uchar req2[10];
  req2[0] = 9;
  memset(req2+1,0x42,8);
  req2[9] = 0;

  evbuffer_add(s->source, req2, 10);

  tt_int_op(SOCKS_GOOD, ==, socks5_handle_negotiation(s->source,s->dest,s->state));
  tt_int_op(0, ==, evbuffer_get_length(s->source));

  uchar rep2[2];
  tt_int_op(2, ==, evbuffer_remove(s->dest,rep2,2));
  tt_assert(rep2[0] == 5);
  tt_assert(rep2[1] == 0);

  /* Third test:
     100 methods: No NOAUTH */
  uchar req3[100];
  req3[0] = 99;
  memset(req3+1,0x42,99);

  evbuffer_add(s->source, req3, 100);

  tt_int_op(SOCKS_BROKEN, ==, socks5_handle_negotiation(s->source,s->dest,s->state));
  tt_int_op(0, ==, evbuffer_get_length(s->source)); /* all data removed */

  uchar rep3[2];
  tt_int_op(2, ==, evbuffer_remove(s->dest,rep3,2));
  tt_assert(rep3[0] == 5);
  tt_assert(rep3[1] == 0xff);

  /* Fourth test:
     nmethods field = 4 but 3 actual methods.

     should say "0" for "want more data!"
   */
  uchar req4[4];
  req4[0] = 4;
  memset(req4+1,0x0,3);

  evbuffer_add(s->source, req4, 4);

  tt_int_op(SOCKS_INCOMPLETE, ==, socks5_handle_negotiation(s->source,s->dest,s->state));
  tt_int_op(4, ==, evbuffer_get_length(s->source)); /* no bytes removed */
  evbuffer_drain(s->source, 4);

  /* Fifth test:
     nmethods field = 3 but 4 actual methods.
     Should be okay; the next byte is part of the request. */
  uchar req5[5];
  req5[0] = 3;
  memset(req5+1,0x0,4);

  evbuffer_add(s->source, req5, 5);

  tt_int_op(SOCKS_GOOD, ==, socks5_handle_negotiation(s->source,s->dest,s->state));
  tt_int_op(1, ==, evbuffer_get_length(s->source)); /* 4 bytes removed */
  evbuffer_drain(s->source, 1);

  uchar rep4[2];
  tt_int_op(2, ==, evbuffer_remove(s->dest,rep4,2));
  tt_assert(rep4[0] == 5);
  tt_assert(rep4[1] == 0);

 end:;
}

/**
   This function tests the 'Client Request' phase of the SOCKS5
   protocol.
   It sends broken 'Client Request' packets and it verifies that
   the SOCKS server detected the errors. It also sends some correct
   packets and it expects the server to like them.
*/
static void
test_socks_socks5_request(void *data)
{
  struct test_socks_state *s = (struct test_socks_state *)data;

  const uint32_t addr_ipv4 = htonl(0x7f000001); /* 127.0.0.1 */
  const uint8_t addr_ipv6[16] = {0,13,0,1,0,5,0,14,0,10,0,5,0,14,0,0}; /* d:1:5:e:a:5:e:0 */
  const uint16_t port = htons(80);    /* 80 */

  /* First test:
     Broken IPV4 req packet with no destport */
  struct parsereq pr1;
  uchar req1[7];
  req1[0] = 1;
  req1[1] = 0;
  req1[2] = 1;
  memcpy(req1+3,&addr_ipv4,4);

  evbuffer_add(s->source, "\x05", 1);
  evbuffer_add(s->source, req1, 7);
  tt_int_op(SOCKS_INCOMPLETE, ==, socks5_handle_request(s->source,&pr1)); /* 0: want more data*/

  /* emptying source buffer before next test  */
  size_t buffer_len = evbuffer_get_length(s->source);
  tt_int_op(0, ==, evbuffer_drain(s->source, buffer_len));

  /* Second test:
     Broken FQDN req packet with no destport */
  uchar req2[7];
  req2[0] = 1;
  req2[1] = 0;
  req2[2] = 3;
  req2[3] = 15;
  memcpy(req1+4,&addr_ipv4,3);

  evbuffer_add(s->source, "\x05", 1);
  evbuffer_add(s->source, req2, 7);
  tt_int_op(SOCKS_INCOMPLETE, ==, socks5_handle_request(s->source,&pr1)); /* 0: want more data*/

  /* emptying source buffer before next test  */
  buffer_len = evbuffer_get_length(s->source);
  tt_int_op(0, ==, evbuffer_drain(s->source, buffer_len));

  /* Third test:
     Correct IPv4 req packet. */
  uchar req3[9];
  req3[0] = 1;
  req3[1] = 0;
  req3[2] = 1;
  memcpy(req3+3,&addr_ipv4,4);
  memcpy(req3+7,&port,2);

  evbuffer_add(s->source, "\x05", 1);
  evbuffer_add(s->source, req3, 9);
  tt_int_op(SOCKS_GOOD, ==, socks5_handle_request(s->source,&pr1));
  tt_str_op(pr1.addr, ==, "127.0.0.1");
  tt_int_op(pr1.port, ==, 80);

  /* emptying source buffer before next test  */
  buffer_len = evbuffer_get_length(s->source);
  tt_int_op(0, ==, evbuffer_drain(s->source, buffer_len));

  /* Fourth test:
     Correct IPv6 req packet. */
  uchar req4[24];
  req4[0] = 5;
  req4[1] = 1;
  req4[2] = 0;
  req4[3] = SOCKS5_ATYP_IPV6;
  memcpy(req4+4,&addr_ipv6,16);
  memcpy(req4+20,&port,2);

  evbuffer_add(s->source,req4,22);
  tt_int_op(SOCKS_GOOD, ==, socks5_handle_request(s->source,&pr1));
  tt_str_op(pr1.addr, ==, "d:1:5:e:a:5:e:0");
  tt_int_op(pr1.port, ==, 80);

  /* emptying source buffer before next test  */
  buffer_len = evbuffer_get_length(s->source);
  tt_int_op(0, ==, evbuffer_drain(s->source, buffer_len));

  /* Fifth test:
     Correct FQDN req packet. */
  const char fqdn[17] = "www.test.example";
  uchar req5[24];
  req5[0] = 5;
  req5[1] = 1;
  req5[2] = 0;
  req5[3] = 3;
  req5[4] = strlen(fqdn);
  strcpy((char *)req5+5,fqdn);
  memcpy(req5+5+strlen(fqdn),&port,2);

  evbuffer_add(s->source, req5, 24);
  tt_int_op(SOCKS_GOOD, ==, socks5_handle_request(s->source,&pr1));
  tt_str_op(pr1.addr, ==, "www.test.example");
  tt_int_op(pr1.port, ==, 80);

  /* Sixth test:
     Small request packet */
  uchar req6[3];
  req6[0] = 5;
  req6[1] = 1;
  req6[2] = 0;

  evbuffer_add(s->source,req6,3);
  tt_int_op(SOCKS_INCOMPLETE, ==, socks5_handle_request(s->source,&pr1));

  /* emptying source buffer before next test  */
  buffer_len = evbuffer_get_length(s->source);
  tt_int_op(0, ==, evbuffer_drain(s->source, buffer_len));

  /* Seventh test:
     Wrong Reserved field */
  uchar req7[5];
  req7[0] = 5;
  req7[1] = 1;
  req7[2] = 1;
  req7[3] = 42;
  req7[4] = 42;

  evbuffer_add(s->source,req7,5);
  tt_int_op(SOCKS_BROKEN, ==, socks5_handle_request(s->source,&pr1));

  /* emptying source buffer before next test  */
  buffer_len = evbuffer_get_length(s->source);
  tt_int_op(0, ==, evbuffer_drain(s->source, buffer_len));

  /* Eigth test:
     Everything is dreamy... if only the requested command was CONNECT... */
  uchar req8[9];
  req8[0] = 2; /* BIND CMD */
  req8[1] = 0;
  req8[2] = 1;
  memcpy(req8+3,&addr_ipv4,4);
  memcpy(req8+7,&port,2);

  evbuffer_add(s->source, "\x05", 1);
  evbuffer_add(s->source, req8, 9);
  /* '-2' means that we don't support the requested command. */
  tt_int_op(SOCKS_CMD_NOT_CONNECT, ==, socks5_handle_request(s->source,&pr1));

 end:;
}

/**
   This function tests the 'Server reply' phase of the SOCKS5
   protocol.
   We ask the server to send us 'Server reply' packets to different
   requests and with different status codes, and we check if the server
   composed the packets well.
*/
static void
test_socks_socks5_request_reply(void *data)
{
  struct test_socks_state *s = (struct test_socks_state *)data;

  s->state->parsereq.af = AF_INET;
  strcpy(s->state->parsereq.addr, "127.0.0.1");
  s->state->parsereq.port = 7357;

  /* First test:
     We ask the server to send us a reply on an IPv4 request with
     succesful status. */
  socks5_send_reply(s->dest,s->state, SOCKS5_SUCCESS);

  uchar rep1[255];
  evbuffer_remove(s->dest,rep1,255); /* yes, this is dirty */

  tt_assert(rep1[3] == SOCKS5_ATYP_IPV4);
  /* check address */
  tt_mem_op(rep1+4, ==, "\x7f\x00\x00\x01", 4);
  /* check port */
  tt_mem_op(rep1+4+4, ==, "\x1c\xbd", 2);

  /* emptying s->dest buffer before next test  */
  size_t buffer_len = evbuffer_get_length(s->dest);
  tt_int_op(0, ==, evbuffer_drain(s->dest, buffer_len));

  /* Second test:
     We ask the server to send us a reply on an IPv6 request with
     succesful status. */
  s->state->parsereq.af = AF_INET6;
  strcpy(s->state->parsereq.addr, "d:1:5:e:a:5:e:0");

  socks5_send_reply(s->dest,s->state, SOCKS5_SUCCESS);

  uchar rep2[255];
  evbuffer_remove(s->dest,rep2,255);

  tt_assert(rep2[3] = SOCKS5_ATYP_IPV6);
  /* Test returned address against inet_pton(d:1:5:e:a:5:e:0) */
  tt_mem_op(rep2+4, ==,
            "\x00\x0d\x00\x01\x00\x05\x00\x0e\x00"
            "\x0a\x00\x05\x00\x0e\x00\x00",
            16);
  tt_mem_op(rep2+4+16, ==, "\x1c\xbd", 2);

  /* emptying dest buffer before next test  */
  buffer_len = evbuffer_get_length(s->dest);
  tt_int_op(0, ==, evbuffer_drain(s->dest, buffer_len));

  /* Third test :
     We ask the server to send us a reply on an FQDN request with
     failure status. */
  const char *fqdn = "www.test.example";
  s->state->parsereq.af = AF_UNSPEC;
  strcpy(s->state->parsereq.addr, fqdn);

  socks5_send_reply(s->dest, s->state, SOCKS5_FAILED_GENERAL);

  uchar rep3[255];
  evbuffer_remove(s->dest,rep3,255);

  tt_assert(rep3[3] == SOCKS5_ATYP_FQDN);
  tt_assert(rep3[4] == strlen(fqdn));
  /* check fqdn */
  tt_mem_op(rep3+5, ==, fqdn,strlen(fqdn));
  /* check port */
  tt_mem_op(rep3+5+strlen(fqdn), ==, "\x1c\xbd",2);

  /* Fourth test:
     We ask the server while having an empty parsereq and with a
     SOCKS5_FAILED_UNSUPPORTED status. */
  memset(&s->state->parsereq,'\x00',sizeof(struct parsereq));

  socks5_send_reply(s->dest,s->state, SOCKS5_FAILED_UNSUPPORTED);
  uchar rep4[255];
  evbuffer_remove(s->dest,rep4,255);

  tt_assert(rep4[3] == SOCKS5_ATYP_IPV4);
  tt_mem_op(rep4+4, ==, "\x00\x00\x00\x00",4);
  tt_mem_op(rep4+4+4, ==, "\x00\x00", 2);

 end:;
}

/**
   This function tests the 'Server reply' phase of the SOCKS4
   *and* SOCKS4a protocol.
   It sends broken client request packets and it verifies that the
   SOCKS server detected the errors. It also sends some correct
   packets and it expects the server to like them.
*/
static void
test_socks_socks4_request(void *data)
{
  struct test_socks_state *s = (struct test_socks_state *)data;

  const uint32_t addr = htonl(0x7f000001); /* 127.0.0.1 */
  const uint16_t port = htons(80);    /* 80 */

  /* First test:
     Correct SOCKS4 req packet with nothing in the optional field. */
  struct parsereq pr1;
  memset(&pr1, 0, sizeof(struct parsereq));
  s->state->parsereq = pr1;
  uchar req1[8];
  req1[0] = 1;
  memcpy(req1+1,&port,2);
  memcpy(req1+3,&addr,4);
  req1[7] = '\x00';

  evbuffer_add(s->source,req1,8);

  tt_int_op(SOCKS_GOOD, ==, socks4_read_request(s->source,s->state));
  tt_str_op(s->state->parsereq.addr, ==, "127.0.0.1");
  tt_int_op(s->state->parsereq.port, ==, 80);

  /* emptying source buffer before next test  */
  size_t buffer_len = evbuffer_get_length(s->source);
  tt_int_op(0, ==, evbuffer_drain(s->source, buffer_len));

  /* Second test:
     Broken SOCKS4 req packet with incomplete optional field */
  char req2[10];
  req2[0] = 1;
  memcpy(req2+1,&port,2);
  memcpy(req2+3,&addr,4);
  strcpy(req2+7,"KO");

  evbuffer_add(s->source,req2,9);

  tt_int_op(SOCKS_INCOMPLETE, ==, socks4_read_request(s->source,s->state));

  /* emptying source buffer before next test  */
  buffer_len = evbuffer_get_length(s->source);
  tt_int_op(0, ==, evbuffer_drain(s->source, buffer_len));

  /* Third test:
     Correct SOCKS4 req packet with optional field. */
  char req3[16];
  req3[0] = 1;
  memcpy(req3+1,&port,2);
  memcpy(req3+3,&addr,4);
  strcpy(req3+7,"iamalive");

  evbuffer_add(s->source,req3,16);

  tt_int_op(SOCKS_GOOD, ==, socks4_read_request(s->source,s->state));
  tt_str_op(s->state->parsereq.addr, ==, "127.0.0.1");
  tt_int_op(s->state->parsereq.port, ==, 80);

  /* emptying source buffer before next test  */
  buffer_len = evbuffer_get_length(s->source);
  tt_int_op(0, ==, evbuffer_drain(s->source, buffer_len));

  /* Fourth test:
     Correct SOCKS4a req packet with optional field. */
  const uint32_t addr_4a = htonl(0x00000042); /* 127.0.0.1 */
  char req4[33];
  req4[0] = 1;
  memcpy(req4+1,&port,2);
  memcpy(req4+3,&addr_4a,4);
  strcpy(req4+7,"iamalive");
  strcpy(req4+16, "www.test.example");

  evbuffer_add(s->source,req4,33);

  tt_int_op(SOCKS_GOOD, ==, socks4_read_request(s->source,s->state));
  tt_str_op(s->state->parsereq.addr, ==, "www.test.example");
  tt_int_op(s->state->parsereq.port, ==, 80);

  /* emptying source buffer before next test  */
  buffer_len = evbuffer_get_length(s->source);
  tt_int_op(0, ==, evbuffer_drain(s->source, buffer_len));

  /* Fifth test:
     Broken SOCKS4a req packet with incomplete optional field. */
  char req5[33];
  req5[0] = 1;
  memcpy(req5+1,&port,2);
  memcpy(req5+3,&addr_4a,4);
  strcpy(req5+7,"iamalive");
  strcpy(req5+16, "www.test.example");

  /* Don't send it all. */
  evbuffer_add(s->source,req5,28);

  tt_int_op(SOCKS_INCOMPLETE, ==, socks4_read_request(s->source,s->state));

  /* emptying source buffer before next test  */
  buffer_len = evbuffer_get_length(s->source);
  tt_int_op(0, ==, evbuffer_drain(s->source, buffer_len));

  /* Sixth test:
     Broken SOCKS4a req packet with a HUGE domain name. */
  #define HUGE 256

  char req6[283];
  req6[0] = 1;
  memcpy(req6+1,&port,2);
  memcpy(req6+3,&addr_4a,4);
  strcpy(req6+7,"iamalive");
  memset(req6+16,'2', HUGE);
  req6[16+HUGE] = '\x00';

  evbuffer_add(s->source,req6,16+HUGE+1);

  tt_int_op(SOCKS_BROKEN, ==, socks4_read_request(s->source,s->state));
  #undef HUGE

 end:;
}

/**
   This function tests the 'Server reply' phase of the SOCKS4/SOCKS4a
   protocol.
   We ask the server to send us server reply packets to different
   requests and with different status codes, and we check if the server
   composed the packets well.
*/
static void
test_socks_socks4_request_reply(void *data)
{
  struct test_socks_state *s = (struct test_socks_state *)data;

  s->state->parsereq.af = AF_INET;
  strcpy(s->state->parsereq.addr, "127.0.0.1");
  s->state->parsereq.port = 7357;

  /* First test:
     We ask the server to send us a reply on an IPv4 request with
     succesful status. */
  socks4_send_reply(s->dest,s->state, SOCKS4_SUCCESS);

  uchar rep1[255];
  evbuffer_remove(s->dest,rep1,255); /* yes, this is dirty */

  tt_assert(rep1[0] == '\x00');
  tt_assert(rep1[1] == SOCKS4_SUCCESS);
  /* check port */
  tt_mem_op(rep1+2, ==, "\x1c\xbd",2);
  /* check address */
  tt_mem_op(rep1+2+2, ==, "\x7f\x00\x00\x01", 4);

  /* emptying dest buffer before next test  */
  size_t buffer_len = evbuffer_get_length(s->dest);
  tt_int_op(0, ==, evbuffer_drain(s->dest, buffer_len));

  /* Second test :
     We ask the server to send us a reply on an FQDN request with
     failure status. */
  const char *fqdn = "www.test.example";
  s->state->parsereq.af = AF_UNSPEC;
  strcpy(s->state->parsereq.addr, fqdn);

  socks4_send_reply(s->dest,s->state, SOCKS4_FAILED);

  uchar rep2[255];
  evbuffer_remove(s->dest,rep2,255);

  tt_assert(rep2[1] == SOCKS4_FAILED);
  /* check port */
  tt_mem_op(rep2+2, ==, "\x1c\xbd", 2);
  /* check address */
  /*  tt_str_op(rep1+2+2, ==, "www.test.example"); */

 end:;
}

#define T(name) \
  { #name, test_socks_##name, 0, &socks_fixture, NULL }

struct testcase_t socks_tests[] = {
  T(socks5_send_negotiation),
  T(socks5_request),
  T(socks5_request_reply),
  T(socks4_request),
  T(socks4_request_reply),
  END_OF_TESTCASES
};
