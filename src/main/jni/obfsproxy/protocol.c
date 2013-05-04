/* Copyright 2011 Nick Mathewson, George Kadianakis
   See LICENSE for other credits and copying information
*/

/**
 * \file protocol.c
 * \headerfile protocol.h
 * \brief Pluggable transports-related functions.
 *
 * \details Acts as an intermediary between generic obfsproxy code,
 * and pluggable transports code. Provides wrappers for
 * transport-specific configuration, and callbacks.
 *
 * Note that in obfsproxy code, 'protocol' is a synonym to 'pluggable
 * transport'.
 **/

#include "util.h"
#include "network.h"
#include "protocol.h"

#include "protocols/dummy.h"
#include "protocols/obfs2.h"

/**
    All supported protocols should be put in this array.
    It's used by main.c.
*/
const protocol_vtable *const supported_protocols[] =
{
  &dummy_vtable,
  &obfs2_vtable
};
const size_t n_supported_protocols =
  sizeof(supported_protocols)/sizeof(supported_protocols[0]);

/**
   This function dispatches (by name) creation of a |config_t|
   to the appropriate protocol-specific initalization function.
 */
config_t *
config_create(int n_options, const char *const *options)
{
  size_t i;
  for (i = 0; i < n_supported_protocols; i++)
    if (!strcmp(*options, supported_protocols[i]->name))
      /* Remove the first element of 'options' (which is always the
         protocol name) from the list passed to the init method. */
      return supported_protocols[i]->config_create(n_options - 1, options + 1);

  return NULL;
}

/**
   Return the transport name for 'cfg'.
*/
const char *
get_transport_name_from_config(config_t *cfg)
{
  obfs_assert(cfg);
  obfs_assert(cfg->vtable);
  obfs_assert(cfg->vtable->name);

  return cfg->vtable->name;
}

/**
   Create a config_t for a managed proxy listener.
*/
config_t *
config_create_managed(int is_server, const char *protocol,
                      const char *bindaddr, const char *orport)
{
  size_t i;
  for (i = 0; i < n_supported_protocols; i++)
    if (!strcmp(protocol, supported_protocols[i]->name)) {
      /* Remove the first element of 'options' (which is always the
         protocol name) from the list passed to the init method. */
      return supported_protocols[i]->config_create_managed(is_server,
                                                           protocol,
                                                           bindaddr, orport);
    }

  return NULL;
}

/**
   This function destroys the protocol-specific part of a listener object.
*/
void
config_free(config_t *cfg)
{
  obfs_assert(cfg);
  obfs_assert(cfg->vtable);
  obfs_assert(cfg->vtable->config_free);
  cfg->vtable->config_free(cfg);
}

struct evutil_addrinfo *
config_get_listen_addrs(config_t *cfg, size_t n)
{
  obfs_assert(cfg);
  obfs_assert(cfg->vtable);
  obfs_assert(cfg->vtable->config_get_listen_addrs);
  return cfg->vtable->config_get_listen_addrs(cfg, n);
}

struct evutil_addrinfo *
config_get_target_addr(config_t *cfg)
{
  obfs_assert(cfg);
  obfs_assert(cfg->vtable);
  obfs_assert(cfg->vtable->config_get_target_addr);
  return cfg->vtable->config_get_target_addr(cfg);
}

/**
   This function is called once per connection and creates a protocol
   object to be used during the session.

   Return a 'protocol_t' if successful, NULL otherwise.
*/
conn_t *
proto_conn_create(config_t *cfg)
{
  obfs_assert(cfg);
  obfs_assert(cfg->vtable);
  obfs_assert(cfg->vtable->conn_create);
  return cfg->vtable->conn_create(cfg);
}

/**
   This function does the protocol handshake.
   Not all protocols have a handshake.
*/
int
proto_handshake(conn_t *conn, void *buf) {
  obfs_assert(conn);
  obfs_assert(conn->cfg);
  obfs_assert(conn->cfg->vtable);
  obfs_assert(conn->cfg->vtable->handshake);
  return conn->cfg->vtable->handshake(conn, buf);
}

/**
   This function is responsible for sending protocol data.
*/
int
proto_send(conn_t *conn, void *source, void *dest) {
  obfs_assert(conn);
  obfs_assert(conn->cfg);
  obfs_assert(conn->cfg->vtable);
  obfs_assert(conn->cfg->vtable->send);
  return conn->cfg->vtable->send(conn, source, dest);
}

/**
   This function is responsible for receiving protocol data.
*/
enum recv_ret
proto_recv(conn_t *conn, void *source, void *dest) {
  obfs_assert(conn);
  obfs_assert(conn->cfg);
  obfs_assert(conn->cfg->vtable);
  obfs_assert(conn->cfg->vtable->recv);
  return conn->cfg->vtable->recv(conn, source, dest);
}

/**
   This function destroys 'conn'.
   It's called everytime we close a connection.
*/
void
proto_conn_free(conn_t *conn) {
  obfs_assert(conn);
  obfs_assert(conn->cfg);
  obfs_assert(conn->cfg->vtable);
  obfs_assert(conn->cfg->vtable->conn_free);
  conn->cfg->vtable->conn_free(conn);
}

/**
   This protocol is called everytime a circuit has to be created.
   Return a circuit_t on success or a NULL on fail.
*/
circuit_t *
proto_circuit_create(config_t *cfg)
{
  obfs_assert(cfg);
  obfs_assert(cfg->vtable);
  obfs_assert(cfg->vtable->circuit_create);
  return cfg->vtable->circuit_create(cfg);
}

/**
   This function destroys 'circuit', using the vtable off 'cfg'.
*/
void
proto_circuit_free(circuit_t *circuit, config_t *cfg) {
  obfs_assert(cfg);
  obfs_assert(cfg->vtable);
  obfs_assert(cfg->vtable->circuit_free);
  cfg->vtable->circuit_free(circuit);
}
