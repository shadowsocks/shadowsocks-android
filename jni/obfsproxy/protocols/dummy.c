/* Copyright 2011 Nick Mathewson, George Kadianakis
   See LICENSE for other credits and copying information
*/

/**
 * \file dummy.c
 * \headerfile dummy.h
 * \brief Implements the 'dummy' pluggable transport. A testing-only
 * pluggable transport that leaves traffic intact
 **/

#include "util.h"

#define PROTOCOL_DUMMY_PRIVATE
#include "dummy.h"

#include <event2/buffer.h>

/* type-safe downcast wrappers */
static inline dummy_config_t *
downcast_config(config_t *p)
{
  return DOWNCAST(dummy_config_t, super, p);
}

static inline dummy_conn_t *
downcast_conn(conn_t *p)
{
  return DOWNCAST(dummy_conn_t, super, p);
}

static inline dummy_circuit_t *
downcast_circuit(circuit_t *p)
{
  return DOWNCAST(dummy_circuit_t, super, p);
}

/**
   Helper: Parses 'options' and fills 'cfg'.
*/
static int
parse_and_set_options(int n_options, const char *const *options,
                      dummy_config_t *cfg)
{
  const char* defport;

  if (n_options < 1)
    return -1;

  if (!strcmp(options[0], "client")) {
    defport = "48988"; /* bf5c */
    cfg->mode = LSN_SIMPLE_CLIENT;
  } else if (!strcmp(options[0], "socks")) {
    defport = "23548"; /* 5bf5 */
    cfg->mode = LSN_SOCKS_CLIENT;
  } else if (!strcmp(options[0], "server")) {
    defport = "11253"; /* 2bf5 */
    cfg->mode = LSN_SIMPLE_SERVER;
  } else
    return -1;

  if (n_options != (cfg->mode == LSN_SOCKS_CLIENT ? 2 : 3))
      return -1;

  cfg->listen_addr = resolve_address_port(options[1], 1, 1, defport);
  if (!cfg->listen_addr)
    return -1;

  if (cfg->mode != LSN_SOCKS_CLIENT) {
    cfg->target_addr = resolve_address_port(options[2], 1, 0, NULL);
    if (!cfg->target_addr)
      return -1;
  }

  return 0;
}

/* Deallocate 'cfg'. */
static void
dummy_config_free(config_t *c)
{
  dummy_config_t *cfg = downcast_config(c);
  if (cfg->listen_addr)
    evutil_freeaddrinfo(cfg->listen_addr);
  if (cfg->target_addr)
    evutil_freeaddrinfo(cfg->target_addr);
  free(cfg);
}

/**
   Populate 'cfg' according to 'options', which is an array like this:
   {"socks","127.0.0.1:6666"}
*/
static config_t *
dummy_config_create(int n_options, const char *const *options)
{
  dummy_config_t *cfg = xzalloc(sizeof(dummy_config_t));
  cfg->super.vtable = &dummy_vtable;

  if (parse_and_set_options(n_options, options, cfg) == 0)
    return &cfg->super;

  dummy_config_free(&cfg->super);
  log_warn("dummy syntax:\n"
           "\tdummy <mode> <listen_address> [<target_address>]\n"
           "\t\tmode ~ server|client|socks\n"
           "\t\tlisten_address, target_address ~ host:port\n"
           "\ttarget_address is required for server and client mode,\n"
           "\tand forbidden for socks mode.\n"
           "Examples:\n"
           "\tobfsproxy dummy socks 127.0.0.1:5000\n"
           "\tobfsproxy dummy client 127.0.0.1:5000 192.168.1.99:11253\n"
           "\tobfsproxy dummy server 192.168.1.99:11253 127.0.0.1:9005");
  return NULL;
}

/**
   Return a config_t for a managed proxy listener.
*/
static config_t *
dummy_config_create_managed(int is_server, const char *protocol,
                            const char *bindaddr, const char *orport)
{
  const char* defport;

  dummy_config_t *cfg = xzalloc(sizeof(dummy_config_t));
  cfg->super.vtable = &dummy_vtable;

  if (is_server) {
    defport = "11253"; /* 2bf5 */
    cfg->mode = LSN_SIMPLE_SERVER;
  } else {
    defport = "23548"; /* 5bf5 */
    cfg->mode = LSN_SOCKS_CLIENT;
  }

  cfg->listen_addr = resolve_address_port(bindaddr, 1, 1, defport);
  if (!cfg->listen_addr)
    goto err;

  if (is_server) {
    cfg->target_addr = resolve_address_port(orport, 1, 0, NULL);
    if (!cfg->target_addr)
      goto err;
  }

  return &cfg->super;

 err:
  dummy_config_free(&cfg->super);
  return NULL;
}

/** Retrieve the 'n'th set of listen addresses for this configuration. */
static struct evutil_addrinfo *
dummy_config_get_listen_addrs(config_t *cfg, size_t n)
{
  if (n > 0)
    return 0;
  return downcast_config(cfg)->listen_addr;
}

/* Retrieve the target address for this configuration. */
static struct evutil_addrinfo *
dummy_config_get_target_addr(config_t *cfg)
{
  return downcast_config(cfg)->target_addr;
}

/*
  This is called everytime we get a connection for the dummy
  protocol.
*/

static conn_t *
dummy_conn_create(config_t *cfg)
{
  dummy_conn_t *proto = xzalloc(sizeof(dummy_conn_t));
  proto->super.cfg = cfg;
  proto->super.mode = downcast_config(cfg)->mode;
  return &proto->super;
}

static void
dummy_conn_free(conn_t *proto)
{
  free(downcast_conn(proto));
}

static circuit_t *
dummy_circuit_create(config_t *cfg)
{
  dummy_circuit_t *circuit = xzalloc(sizeof(dummy_circuit_t));
  return &circuit->super;
}

static void
dummy_circuit_free(circuit_t *circ)
{
  free(downcast_circuit(circ));
}

/** Dummy has no handshake */
static int
dummy_handshake(conn_t *proto, struct evbuffer *buf)
{
  return 0;
}

/** send, receive - just copy */
static int
dummy_send(conn_t *proto, struct evbuffer *source, struct evbuffer *dest)
{
  return evbuffer_add_buffer(dest,source);
}

static enum recv_ret
dummy_recv(conn_t *proto, struct evbuffer *source, struct evbuffer *dest)
{
  if (evbuffer_add_buffer(dest,source)<0)
    return RECV_BAD;
  else
    return RECV_GOOD;
}

DEFINE_PROTOCOL_VTABLE(dummy);
