/* Copyright 2011 Nick Mathewson, George Kadianakis
   See LICENSE for other credits and copying information
*/

#include "util.h"
#include "tinytest_macros.h"

#define PROTOCOL_OBFS2_PRIVATE
#define CRYPT_PRIVATE
#define NETWORK_PRIVATE
#include "protocols/obfs2.h"
#include "crypt.h"

#include <event2/buffer.h>

#define ALEN(x) (sizeof x/sizeof x[0])

static inline obfs2_conn_t *
downcast(conn_t *proto)
{
  return DOWNCAST(obfs2_conn_t, super, proto);
}

static inline obfs2_circuit_t *
downcast_circuit(circuit_t *p)
{
  return DOWNCAST(obfs2_circuit_t, super, p);
}

static inline obfs2_state_t *
get_state_from_conn(conn_t *conn)
{
  return downcast_circuit(conn->circuit)->state;
}

static void
test_obfs2_option_parsing(void *unused)
{
  struct option_parsing_case {
    config_t *result;
    short should_succeed;
    short n_opts;
    const char *const opts[6];
  };
  static struct option_parsing_case cases[] = {
    /** good option list */
    { 0, 1, 4, {"obfs2", "--shared-secret=a", "socks", "127.0.0.1:0"} },
    /** two --dest. */
    { 0, 0, 5, {"obfs2", "--dest=127.0.0.1:5555", "--dest=a",
                "server", "127.0.0.1:5552"} },
    /** unknown arg */
    { 0, 0, 4, {"obfs2", "--gabura=a", "server", "127.0.0.1:5552"} },
    /** too many args */
    { 0, 0, 6, {"obfs2", "1", "2", "3", "4", "5" } },
    /** wrong mode  */
    { 0, 0, 4, {"obfs2", "--dest=1:1", "gladiator", "127.0.0.1:5552"} },
    /** bad listen addr */
    { 0, 0, 4, {"obfs2", "--dest=1:1", "server", "127.0.0.1:a"} },
    /** bad dest addr */
    { 0, 0, 4, {"obfs2", "--dest=1:b", "server", "127.0.0.1:1"} },
    /** socks with dest */
    { 0, 0, 4, {"obfs2", "--dest=1:2", "socks", "127.0.0.1:1"} },
    /** server without dest */
    { 0, 0, 4, {"obfs2", "--shared-secret=a", "server", "127.0.0.1:1"} },

    { 0, 0, 0, {0} }
  };

  /* Suppress logs for the duration of this function. */
  log_set_method(LOG_METHOD_NULL, NULL);

  struct option_parsing_case *c;
  for (c = cases; c->n_opts; c++) {
    c->result = config_create(c->n_opts, c->opts);
    if (c->should_succeed)
      tt_ptr_op(c->result, !=, NULL);
    else
      tt_ptr_op(c->result, ==, NULL);
  }

 end:
  for (c = cases; c->n_opts; c++)
    if (c->result)
      config_free(c->result);

  /* Unsuspend logging */
  log_set_method(LOG_METHOD_STDERR, NULL);
}

/* All the tests below use this test environment: */
struct test_obfs2_state
{
  config_t *cfg_client;
  config_t *cfg_server;
  conn_t *conn_client;
  conn_t *conn2_client; /* dummy conn to build a circuit */
  conn_t *conn_server;
  conn_t *conn2_server; /* dummy conn to build a circuit */
  struct evbuffer *output_buffer;
  struct evbuffer *dummy_buffer;
};

static int
cleanup_obfs2_state(const struct testcase_t *unused, void *state)
{
  struct test_obfs2_state *s = (struct test_obfs2_state *)state;

  proto_circuit_free(s->conn_client->circuit, s->cfg_client);
  if (s->conn_client)
    proto_conn_free(s->conn_client);
  if (s->conn2_client)
    proto_conn_free(s->conn2_client);

  proto_circuit_free(s->conn_server->circuit, s->cfg_server);
  if (s->conn_server)
    proto_conn_free(s->conn_server);
  if (s->conn2_server)
    proto_conn_free(s->conn2_server);

  if (s->cfg_client)
    config_free(s->cfg_client);
  if (s->cfg_server)
    config_free(s->cfg_server);

  if (s->output_buffer)
    evbuffer_free(s->output_buffer);
  if (s->dummy_buffer)
    evbuffer_free(s->dummy_buffer);

  free(state);
  return 1;
}

static const char *const options_client[] =
  {"obfs2", "--shared-secret=hahaha", "socks", "127.0.0.1:1800"};

static const char *const options_server[] =
  {"obfs2", "--shared-secret=hahaha",
   "--dest=127.0.0.1:1500", "server", "127.0.0.1:1800"};

static void *
setup_obfs2_state(const struct testcase_t *unused)
{
  struct test_obfs2_state *s = xzalloc(sizeof(struct test_obfs2_state));

  s->cfg_client =
    config_create(ALEN(options_client), options_client);
  tt_assert(s->cfg_client);

  s->cfg_server =
    config_create(ALEN(options_server), options_server);
  tt_assert(s->cfg_server);

  s->conn_client = proto_conn_create(s->cfg_client);
  tt_assert(s->conn_client);

  s->conn_server = proto_conn_create(s->cfg_server);
  tt_assert(s->conn_server);

  s->conn2_client = proto_conn_create(s->cfg_client);
  tt_assert(s->conn2_client);

  s->conn2_server = proto_conn_create(s->cfg_server);
  tt_assert(s->conn2_server);

  tt_assert(!(circuit_create(s->conn_client, s->conn2_client)<0));
  tt_assert(!(circuit_create(s->conn_server, s->conn2_server)<0));

  s->output_buffer = evbuffer_new();
  tt_assert(s->output_buffer);

  s->dummy_buffer = evbuffer_new();
  tt_assert(s->dummy_buffer);

  return s;

 end:
  cleanup_obfs2_state(NULL, s);
  return NULL;
}

static const struct testcase_setup_t obfs2_fixture =
  { setup_obfs2_state, cleanup_obfs2_state };

static void
test_obfs2_handshake(void *state)
{
  struct test_obfs2_state *s = (struct test_obfs2_state *)state;
  obfs2_state_t *client_state = get_state_from_conn(s->conn_client);
  obfs2_state_t *server_state = get_state_from_conn(s->conn_server);

  /* We create a client handshake message and pass it to output_buffer */
  tt_int_op(0, <=, proto_handshake(s->conn_client, s->output_buffer));

  /* We simulate the server receiving and processing the client's
     handshake message, by using proto_recv() on the output_buffer */
  tt_assert(RECV_GOOD == proto_recv(s->conn_server, s->output_buffer,
                                    s->dummy_buffer));


  /* Now, we create the server's handshake and pass it to output_buffer */
  tt_int_op(0, <=, proto_handshake(s->conn_server, s->output_buffer));

  /* We simulate the client receiving and processing the server's handshake */
  tt_assert(RECV_GOOD == proto_recv(s->conn_client, s->output_buffer,
                                    s->dummy_buffer));

  /* The handshake is now complete. We should have:
     client's send_crypto == server's recv_crypto
     server's send_crypto == client's recv_crypto . */
  tt_mem_op(client_state->send_crypto, ==, server_state->recv_crypto,
            sizeof(crypt_t));

  tt_mem_op(client_state->recv_crypto, ==, server_state->send_crypto,
            sizeof(crypt_t));

 end:;
}

static void
test_obfs2_transfer(void *state)
{
  struct test_obfs2_state *s = (struct test_obfs2_state *)state;
  int n;
  struct evbuffer_iovec v[2];

  /* Handshake */
  tt_int_op(0, <=, proto_handshake(s->conn_client, s->output_buffer));
  tt_assert(RECV_GOOD == proto_recv(s->conn_server, s->output_buffer,
                                    s->dummy_buffer));
  tt_int_op(0, <=, proto_handshake(s->conn_server, s->output_buffer));
  tt_assert(RECV_GOOD == proto_recv(s->conn_client, s->output_buffer,
                                    s->dummy_buffer));
  /* End of Handshake */

  /* Now let's pass some data around. */
  const char *msg1 = "this is a 54-byte message passed from client to server";
  const char *msg2 = "this is a 55-byte message passed from server to client!";

  /* client -> server */
  evbuffer_add(s->dummy_buffer, msg1, 54);
  proto_send(s->conn_client, s->dummy_buffer, s->output_buffer);

  tt_assert(RECV_GOOD == proto_recv(s->conn_server, s->output_buffer,
                                    s->dummy_buffer));

  n = evbuffer_peek(s->dummy_buffer, -1, NULL, &v[0], 2);
  tt_int_op(n, ==, 1); /* expect contiguous data */
  tt_int_op(0, ==, strncmp(msg1, v[0].iov_base, 54));

  /* emptying dummy_buffer before next test  */
  size_t buffer_len = evbuffer_get_length(s->dummy_buffer);
  tt_int_op(0, ==, evbuffer_drain(s->dummy_buffer, buffer_len));

  /* client <- server */
  evbuffer_add(s->dummy_buffer, msg2, 55);
  tt_int_op(0, <=, proto_send(s->conn_server, s->dummy_buffer,
                              s->output_buffer));

  tt_assert(RECV_GOOD == proto_recv(s->conn_client, s->output_buffer,
                                    s->dummy_buffer));

  n = evbuffer_peek(s->dummy_buffer, -1, NULL, &v[1], 2);
  tt_int_op(n, ==, 1); /* expect contiguous data */
  tt_int_op(0, ==, strncmp(msg2, v[1].iov_base, 55));

 end:;
}

/* We are going to split client's handshake into:
   msgclient_1 = [OBFUSCATE_SEED_LENGTH + 8 + <one fourth of padding>]
   and msgclient_2 = [<rest of padding>].

   We are then going to split server's handshake into:
   msgserver_1 = [OBFUSCATE_SEED_LENGTH + 8]
   and msgserver_2 = [<all padding>].

   Afterwards we will verify that they both got the correct keys.
   That's right, this unit test is loco . */
static void
test_obfs2_split_handshake(void *state)
{
  struct test_obfs2_state *s = (struct test_obfs2_state *)state;
  obfs2_state_t *client_state = get_state_from_conn(s->conn_client);
  obfs2_state_t *server_state = get_state_from_conn(s->conn_server);

  uint32_t magic = htonl(OBFUSCATE_MAGIC_VALUE);
  uint32_t plength1, plength1_msg1, plength1_msg2, send_plength1;
  const uchar *seed1;

  /* generate padlen */
  tt_int_op(0, <=, random_bytes((uchar*)&plength1, 4));

  plength1 %= OBFUSCATE_MAX_PADDING;

  plength1_msg1 = plength1 / 4;
  plength1_msg2 = plength1 - plength1_msg1;

  send_plength1 = htonl(plength1);

  uchar msgclient_1[OBFUSCATE_MAX_PADDING + OBFUSCATE_SEED_LENGTH + 8];
  uchar msgclient_2[OBFUSCATE_MAX_PADDING];

  seed1 = client_state->initiator_seed;

  memcpy(msgclient_1, seed1, OBFUSCATE_SEED_LENGTH);
  memcpy(msgclient_1+OBFUSCATE_SEED_LENGTH, &magic, 4);
  memcpy(msgclient_1+OBFUSCATE_SEED_LENGTH+4, &send_plength1, 4);
  tt_int_op(0, <=, random_bytes(msgclient_1+OBFUSCATE_SEED_LENGTH+8,
                                plength1_msg1));

  stream_crypt(client_state->send_padding_crypto,
               msgclient_1+OBFUSCATE_SEED_LENGTH, 8+plength1_msg1);

  /* Client sends handshake part 1 */
  evbuffer_add(s->output_buffer, msgclient_1,
               OBFUSCATE_SEED_LENGTH+8+plength1_msg1);

  /* Server receives handshake part 1 */
  tt_assert(RECV_INCOMPLETE == proto_recv(s->conn_server, s->output_buffer,
                                          s->dummy_buffer));

  tt_assert(server_state->state == ST_WAIT_FOR_PADDING);

  /* Preparing client's handshake part 2 */
  tt_int_op(0, <=, random_bytes(msgclient_2, plength1_msg2));
  stream_crypt(client_state->send_padding_crypto, msgclient_2, plength1_msg2);

  /* Client sends handshake part 2 */
  evbuffer_add(s->output_buffer, msgclient_2, plength1_msg2);

  /* Server receives handshake part 2 */
  tt_assert(RECV_GOOD == proto_recv(s->conn_server, s->output_buffer,
                                    s->dummy_buffer));

  tt_assert(server_state->state == ST_OPEN);

  /* Since everything went right, let's do a server to client handshake now! */
  uint32_t plength2, send_plength2;
  const uchar *seed2;

  /* generate padlen */
  tt_int_op(0, <=, random_bytes((uchar*)&plength2, 4));

  plength2 %= OBFUSCATE_MAX_PADDING;
  send_plength2 = htonl(plength2);

  uchar msgserver_1[OBFUSCATE_SEED_LENGTH + 8];
  uchar msgserver_2[OBFUSCATE_MAX_PADDING];

  seed2 = server_state->responder_seed;

  memcpy(msgserver_1, seed2, OBFUSCATE_SEED_LENGTH);
  memcpy(msgserver_1+OBFUSCATE_SEED_LENGTH, &magic, 4);
  memcpy(msgserver_1+OBFUSCATE_SEED_LENGTH+4, &send_plength2, 4);

  stream_crypt(server_state->send_padding_crypto,
               msgserver_1+OBFUSCATE_SEED_LENGTH, 8);

  /* Server sends handshake part 1 */
  evbuffer_add(s->output_buffer, msgserver_1, OBFUSCATE_SEED_LENGTH+8);

  /* Client receives handshake part 1 */
  tt_assert(RECV_INCOMPLETE == proto_recv(s->conn_client, s->output_buffer,
                                          s->dummy_buffer));

  tt_assert(client_state->state == ST_WAIT_FOR_PADDING);

  /* Preparing client's handshake part 2 */
  tt_int_op(0, <=, random_bytes(msgserver_2, plength2));
  stream_crypt(server_state->send_padding_crypto, msgserver_2, plength2);

  /* Server sends handshake part 2 */
  evbuffer_add(s->output_buffer, msgserver_2, plength2);

  /* Client receives handshake part 2 */
  tt_assert(RECV_GOOD == proto_recv(s->conn_client, s->output_buffer,
                                    s->dummy_buffer));

  tt_assert(client_state->state == ST_OPEN);

  /* The handshake is finally complete. We should have: */
  /*    client's send_crypto == server's recv_crypto */
  /*    server's send_crypto == client's recv_crypto . */
  tt_mem_op(client_state->send_crypto, ==, server_state->recv_crypto,
            sizeof(crypt_t));

  tt_mem_op(client_state->recv_crypto, ==, server_state->send_crypto,
            sizeof(crypt_t));

 end:;
}

/*
  Erroneous handshake test:
  Wrong magic value.
*/
static void
test_obfs2_wrong_handshake_magic(void *state)
{
  struct test_obfs2_state *s = (struct test_obfs2_state *)state;
  obfs2_state_t *client_state = get_state_from_conn(s->conn_client);
  obfs2_state_t *server_state = get_state_from_conn(s->conn_server);

  uint32_t wrong_magic = 0xD15EA5E;

  uint32_t plength, send_plength;
  const uchar *seed;
  uchar msg[OBFUSCATE_MAX_PADDING + OBFUSCATE_SEED_LENGTH + 8];

  tt_int_op(0, >=, random_bytes((uchar*)&plength, 4));
  plength %= OBFUSCATE_MAX_PADDING;
  send_plength = htonl(plength);

  seed = client_state->initiator_seed;
  memcpy(msg, seed, OBFUSCATE_SEED_LENGTH);
  memcpy(msg+OBFUSCATE_SEED_LENGTH, &wrong_magic, 4);
  memcpy(msg+OBFUSCATE_SEED_LENGTH+4, &send_plength, 4);
  tt_int_op(0, >=, random_bytes(msg+OBFUSCATE_SEED_LENGTH+8, plength));

  stream_crypt(client_state->send_padding_crypto,
               msg+OBFUSCATE_SEED_LENGTH, 8+plength);

  evbuffer_add(s->output_buffer, msg, OBFUSCATE_SEED_LENGTH+8+plength);

  tt_assert(RECV_BAD == proto_recv(s->conn_server, s->output_buffer,
                                   s->dummy_buffer));

  tt_assert(server_state->state == ST_WAIT_FOR_KEY);

 end:;
}

/* Erroneous handshake test:
   plength field larger than OBFUSCATE_MAX_PADDING
*/
static void
test_obfs2_wrong_handshake_plength(void *state)
{
  struct test_obfs2_state *s = (struct test_obfs2_state *)state;
  obfs2_state_t *client_state = get_state_from_conn(s->conn_client);
  obfs2_state_t *server_state = get_state_from_conn(s->conn_server);

  uchar msg[OBFUSCATE_MAX_PADDING + OBFUSCATE_SEED_LENGTH + 8 + 1];
  uint32_t magic = htonl(OBFUSCATE_MAGIC_VALUE);
  uint32_t plength, send_plength;
  const uchar *seed;
  seed = client_state->initiator_seed;

  plength = OBFUSCATE_MAX_PADDING + 1U;
  send_plength = htonl(plength);

  memcpy(msg, seed, OBFUSCATE_SEED_LENGTH);
  memcpy(msg+OBFUSCATE_SEED_LENGTH, &magic, 4);
  memcpy(msg+OBFUSCATE_SEED_LENGTH+4, &send_plength, 4);
  tt_int_op(0, >=, random_bytes(msg+OBFUSCATE_SEED_LENGTH+8, plength));

  stream_crypt(client_state->send_padding_crypto,
               msg+OBFUSCATE_SEED_LENGTH, 8+plength);

  evbuffer_add(s->output_buffer, msg, OBFUSCATE_SEED_LENGTH+8+plength);


  tt_assert(RECV_BAD == proto_recv(s->conn_server, s->output_buffer,
                                   s->dummy_buffer));

  tt_assert(server_state->state == ST_WAIT_FOR_KEY);

 end:;
}

#define T(name) \
  { #name, test_obfs2_##name, 0, NULL, NULL }

#define TF(name) \
  { #name, test_obfs2_##name, 0, &obfs2_fixture, NULL }

struct testcase_t obfs2_tests[] = {
  T(option_parsing),
  TF(handshake),
  TF(transfer),
  TF(split_handshake),
  TF(wrong_handshake_magic),
  TF(wrong_handshake_plength),
  END_OF_TESTCASES
};
