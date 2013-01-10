/* Copyright 2011 Nick Mathewson, George Kadianakis
   See LICENSE for other credits and copying information
*/

/**
 * \file obfs2.h
 * \brief Headers for obfs2.c.
 **/

#ifndef PROTOCOL_OBFS2_H
#define PROTOCOL_OBFS2_H

extern const protocol_vtable obfs2_vtable;

#ifdef PROTOCOL_OBFS2_PRIVATE

#include "crypt.h"
#include "protocol.h"
#include "network.h"

/* ==========
   These definitions are not part of the obfs2_protocol interface.
   They're exposed here so that the unit tests can use them.
   ==========
*/
/* from brl's obfuscated-ssh standard. */
//#define OBFUSCATE_MAGIC_VALUE        0x0BF5CA7E

/* our own, since we break brl's spec */
#define OBFUSCATE_MAGIC_VALUE        0x2BF5CA7E
#define OBFUSCATE_SEED_LENGTH        16
#define OBFUSCATE_MAX_PADDING        8192
#define OBFUSCATE_HASH_ITERATIONS     100000

#define INITIATOR_PAD_TYPE "Initiator obfuscation padding"
#define RESPONDER_PAD_TYPE "Responder obfuscation padding"
#define INITIATOR_SEND_TYPE "Initiator obfuscated data"
#define RESPONDER_SEND_TYPE "Responder obfuscated data"

#define SHARED_SECRET_LENGTH SHA256_LENGTH

typedef struct obfs2_state_t {
  /** Current protocol state.  We start out waiting for key information.  Then
      we have a key and wait for padding to arrive.  Finally, we are sending
      and receiving bytes on the connection.
  */
  enum {
    ST_WAIT_FOR_KEY,
    ST_WAIT_FOR_PADDING,
    ST_OPEN,
  } state;
  /** Random seed we generated for this stream */
  uchar initiator_seed[OBFUSCATE_SEED_LENGTH];
  /** Random seed the other side generated for this stream */
  uchar responder_seed[OBFUSCATE_SEED_LENGTH];
  /** Shared secret seed value. */
  uchar secret_seed[SHARED_SECRET_LENGTH];
  /** True iff we opened this connection */
  int we_are_initiator;

  /** key used to encrypt outgoing data */
  crypt_t *send_crypto;
  /** key used to encrypt outgoing padding */
  crypt_t *send_padding_crypto;
  /** key used to decrypt incoming data */
  crypt_t *recv_crypto;
  /** key used to decrypt incoming padding */
  crypt_t *recv_padding_crypto;

  /** Buffer full of data we'll send once the handshake is done. */
  struct evbuffer *pending_data_to_send;

  /** Number of padding bytes to read before we get to real data */
  int padding_left_to_read;
} obfs2_state_t;

/** config_t for obfs2.
 * \private \extends config_t
 */
typedef struct obfs2_config_t {
  /** Base <b>config_t</b>. */
  config_t super;
  /** Address to listen on. */
  struct evutil_addrinfo *listen_addr;
  /** Address to connect to. */
  struct evutil_addrinfo *target_addr;
  /** Listener mode. */
  enum listen_mode mode;
  /** obfs2 shared secret. */
  uchar shared_secret[SHARED_SECRET_LENGTH];
} obfs2_config_t;

/** conn_t for obfs2.
 * \private \extends conn_t
 */
typedef struct obfs2_conn_t {
  /** Base <b>conn_t</b>. */
  conn_t super;
} obfs2_conn_t;

/** circuit_t for obfs2.
 * \private \extends circuit_t
 */
typedef struct obfs2_circuit_t {
  /** Base <b>circuit_t</b>. */
  circuit_t super;
  /** State of obfs2 for this circuit. */
  obfs2_state_t *state;
} obfs2_circuit_t;

#endif

#endif
