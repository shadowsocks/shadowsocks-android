/* Copyright 2011 Nick Mathewson, George Kadianakis
   See LICENSE for other credits and copying information
*/

/**
 * \file protocol.h
 * \brief Headers for protocol.c.
 **/

#ifndef PROTOCOL_H
#define PROTOCOL_H

/**
   This struct defines a "configuration" of the proxy.
   A configuration is a set of addresses to listen on, and what to do
   when connections are received.  Almost all of a configuration is
   protocol-private data, stored in the larger structure in which this
   struct is embedded.
 */
struct config_t
{
  const struct protocol_vtable *vtable;
};

const char *get_transport_name_from_config(config_t *cfg);

/**
   This struct defines a protocol and its methods; note that not all
   of them are methods on the same object in the C++ sense.

   A filled-in, statically allocated protocol_vtable object is the
   principal interface between each individual protocol and generic
   code.  At present there is a static list of these objects in protocol.c.
 */
struct protocol_vtable
{
  /** The short name of this protocol. */
  const char *name;

  /** Allocate a 'config_t' object and fill it in from the provided
      'options' array. */
  config_t *(*config_create)(int n_options, const char *const *options);

  config_t *(*config_create_managed)(int is_server, const char *protocol,
                                     const char *bindaddr, const char *orport);

  /** Destroy the provided 'config_t' object.  */
  void (*config_free)(config_t *cfg);

  /** Return a set of addresses to listen on, in the form of an
      'evutil_addrinfo' linked list.  There may be more than one list;
      users of this function should call it repeatedly with successive
      values of N, starting from zero, until it returns NULL, and
      create listeners for every address returned. */
  struct evutil_addrinfo *(*config_get_listen_addrs)(config_t *cfg, size_t n);

  /** Return a set of addresses to attempt an outbound connection to,
      in the form of an 'evutil_addrinfo' linked list.  There is only
      one such list. */
  struct evutil_addrinfo *(*config_get_target_addr)(config_t *cfg);

  /** A connection has just been made to one of 'cfg's listener
      addresses.  Return an extended 'conn_t' object, filling in the
      'cfg' and 'mode' fields of the generic structure.  */
  conn_t *(*conn_create)(config_t *cfg);

  /** Destroy per-connection, protocol-specific state.  */
  void (*conn_free)(conn_t *state);

  /** A 'circuit_t' needs to be created. */
  circuit_t *(*circuit_create)(config_t *cfg);

  /** Destroy a 'circuit_t'.  */
  void (*circuit_free)(circuit_t *circuit);

  /** Perform a connection handshake. Not all protocols have a handshake. */
  int (*handshake)(conn_t *state, struct evbuffer *buf);

  /** Send data coming downstream from 'source' along to 'dest'. */
  int (*send)(conn_t *state,
              struct evbuffer *source,
              struct evbuffer *dest);

  /** Receive data from 'source' and pass it upstream to 'dest'. */
  enum recv_ret (*recv)(conn_t *state,
                        struct evbuffer *source,
                        struct evbuffer *dest);

};

/**
   Use this macro to define protocol_vtable objects; it ensures all
   the methods are in the correct order and enforces a consistent
   naming convention on protocol implementations.
 */
#define DEFINE_PROTOCOL_VTABLE(name)            \
  const protocol_vtable name##_vtable = {       \
    #name,                                      \
    name##_config_create,                       \
    name##_config_create_managed,               \
    name##_config_free,                         \
    name##_config_get_listen_addrs,             \
    name##_config_get_target_addr,              \
    name##_conn_create,                         \
    name##_conn_free,                           \
    name##_circuit_create,                      \
    name##_circuit_free,                        \
    name##_handshake, name##_send, name##_recv  \
  }

config_t *config_create(int n_options, const char *const *options);
config_t *config_create_managed(int is_server, const char *protocol,
                                const char *bindaddr, const char *orport);

void config_free(config_t *cfg);

struct evutil_addrinfo *config_get_listen_addrs(config_t *cfg, size_t n);
struct evutil_addrinfo *config_get_target_addr(config_t *cfg);

conn_t *proto_conn_create(config_t *cfg);
void proto_conn_free(conn_t *conn);

circuit_t *proto_circuit_create(config_t *cfg);
void proto_circuit_free(circuit_t *conn, config_t *cfg);


int proto_handshake(conn_t *conn, void *buf);
int proto_send(conn_t *conn, void *source, void *dest);
enum recv_ret proto_recv(conn_t *conn, void *source, void *dest);

extern const protocol_vtable *const supported_protocols[];
extern const size_t n_supported_protocols;

#endif
