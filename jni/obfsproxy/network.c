/* Copyright 2011 Nick Mathewson, George Kadianakis
   See LICENSE for other credits and copying information
*/

/**
 * \file network.c
 * \headerfile network.h
 * \brief Networking side of obfspoxy. Creates listeners, accepts
 * connections, reads/writes data, and calls transport callbacks when
 * applicable.
 *
 * \details Terminology:
 *
 * A <em>connection</em> is a bidirectional communications channel,
 * usually backed by a network socket, and represented in this layer
 * by a conn_t, wrapping a libevent bufferevent.
 *
 * A <em>circuit</em> is a <b>pair</b> of connections, referred to as the
 * <em>upstream</em> and <em>downstream</em> connections.  A circuit
 * is represented by a circuit_t.  The upstream connection of a
 * circuit communicates in cleartext with the higher-level program
 * that wishes to make use of our obfuscation service.  The
 * downstream connection commmunicates in an obfuscated fashion with
 * the remote peer that the higher-level client wishes to contact.
 *
 * A <em>listener</em> is a listening socket bound to a particular
 * obfuscation protocol, represented in this layer by a listener_t
 * and its config_t.  Connecting to a listener creates one
 * connection of a circuit, and causes this program to initiate the
 * other connection (possibly after receiving in-band instructions
 * about where to connect to).  A listener is said to be a
 * <em>client</em> listener if connecting to it creates the
 * <b>upstream</b> connection, and a <em>server</em> listener if
 * connecting to it creates the <b>downstream</b> connection.
 *
 * There are two kinds of client listeners: a <em>simple</em> client
 * listener always connects to the same remote peer every time it
 * needs to initiate a downstream connection; a <em>socks</em>
 * client listener can be told to connect to an arbitrary remote
 * peer using the SOCKS protocol (version 4 or 5).
 *
 * The diagram below might help demonstrate the relationship between
 * connections and circuits:
 *
 *\verbatim
                                      downstream

         'circuit_t C'        'conn_t CD'      'conn_t SD'        'circuit_t S'
                        +-----------+             +-----------+
 upstream           ----|obfsproxy c|-------------|obfsproxy s|----    upstream
                    |   +-----------+             +-----------+   |
         'conn_t CU'|                                             |'conn_t SU'
              +------------+                              +--------------+
              | Tor Client |                              |  Tor Bridge  |
              +------------+                              +--------------+

 \endverbatim
 *
 * In the above diagram, <b>obfsproxy c</b> is the client-side
 * obfsproxy, and <b>obfsproxy s</b> is the server-side
 * obfsproxy. <b>conn_t CU</b> is the Client's Upstream connection,
 * the communication channel between tor and obfsproxy.  <b>conn_t
 * CD</b> is the Client's Downstream connection, the communication
 * channel between obfsproxy and the remote peer. These two
 * connections form <b>circuit_t C</b>.
 **/

#include "util.h"

#define NETWORK_PRIVATE
#include "network.h"

#include "container.h"
#include "main.h"
#include "socks.h"
#include "protocol.h"

#include "status.h"

#include <errno.h>

#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_struct.h>
#include <event2/listener.h>

/**
  This struct defines the state of a listener on a particular address.
 */
typedef struct listener_t {
  config_t *cfg;
  struct evconnlistener *listener;
  char *address;
} listener_t;

/** All our listeners. */
static smartlist_t *listeners;

/** All active connections.  */
static smartlist_t *connections;

/** Flag toggled when obfsproxy is shutting down. It blocks new
    connections and shutdowns when the last connection is closed. */
static int shutting_down=0;

static void listener_free(listener_t *lsn);

static void listener_cb(struct evconnlistener *evcl, evutil_socket_t fd,
                        struct sockaddr *sourceaddr, int socklen,
                        void *closure);

static void simple_client_listener_cb(conn_t *conn);
static void socks_client_listener_cb(conn_t *conn);
static void simple_server_listener_cb(conn_t *conn);

static void conn_free(conn_t *conn);
static void conn_free_on_flush(struct bufferevent *bev, void *arg);

static void circuit_create_socks(conn_t *up);
static int circuit_add_down(circuit_t *circuit, conn_t *down);
static void circuit_free(circuit_t *circuit);

static void upstream_read_cb(struct bufferevent *bev, void *arg);
static void downstream_read_cb(struct bufferevent *bev, void *arg);
static void socks_read_cb(struct bufferevent *bev, void *arg);

static void error_cb(struct bufferevent *bev, short what, void *arg);
static void flush_error_cb(struct bufferevent *bev, short what, void *arg);
static void pending_conn_cb(struct bufferevent *bev, short what, void *arg);
static void pending_socks_cb(struct bufferevent *bev, short what, void *arg);

/**
   Return the evconnlistener that uses 'cfg'.
*/
struct evconnlistener *
get_evconnlistener_by_config(config_t *cfg)
{
  obfs_assert(listeners);

  SMARTLIST_FOREACH_BEGIN(listeners, listener_t *, l) {
    if (l->cfg == cfg)
      return l->listener;
  } SMARTLIST_FOREACH_END(l);

  return NULL;
}

/**
   Puts obfsproxy's networking subsystem on "closing time" mode. This
   means that we stop accepting new connections and we shutdown when
   the last connection is closed.

   If 'barbaric' is set, we forcefully close all open connections and
   finish shutdown.

   (Only called by signal handlers)
*/
void
start_shutdown(int barbaric)
{
  log_debug("Beginning %s shutdown.", barbaric ? "barbaric" : "normal");

  if (!shutting_down)
    shutting_down=1;

  if (!connections) {
    finish_shutdown();
  } else if (smartlist_len(connections) == 0) {
    smartlist_free(connections);
    connections = NULL;
    finish_shutdown();
  } else if (barbaric) {
    while (connections) /* last conn_free() will free connections smartlist */
      conn_free(smartlist_get(connections,0));
  }
}

/**
   This function opens listening sockets configured according to the
   provided 'config_t'.
   Returns 1 on success, 0 on failure.
 */
int
open_listeners(struct event_base *base, config_t *cfg)
{
  const unsigned flags =
    LEV_OPT_CLOSE_ON_FREE|LEV_OPT_CLOSE_ON_EXEC|LEV_OPT_REUSEABLE;
  size_t i;
  listener_t *lsn;
  struct evutil_addrinfo *addrs;

  /* If we don't have a listener list, create one now. */
  if (!listeners)
    listeners = smartlist_create();

  for (i = 0; ; i++) {
    addrs = config_get_listen_addrs(cfg, i);
    if (!addrs) break;
    do {
      lsn = xzalloc(sizeof(listener_t));
      lsn->cfg = cfg;
      lsn->address = printable_address(addrs->ai_addr, addrs->ai_addrlen);
      lsn->listener =
        evconnlistener_new_bind(base, listener_cb, lsn, flags, -1,
                                addrs->ai_addr, addrs->ai_addrlen);

      if (!lsn->listener) {
        log_warn("Failed to open listening socket on %s: %s",
                 safe_str(lsn->address),
                 evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
        listener_free(lsn);
        return 0;
      }

      smartlist_add(listeners, lsn);
      log_debug("Now listening on %s for protocol %s.",
                lsn->address, cfg->vtable->name);

      addrs = addrs->ai_next;
    } while (addrs);
  }

  return 1;
}

/**
   Deallocates listener_t 'lsn'.
*/
static void
listener_free(listener_t *lsn)
{
  if (lsn->listener)
    evconnlistener_free(lsn->listener);
  if (lsn->address)
    free(lsn->address);
  free(lsn);
}

/**
   Frees all active listeners.
*/
void
close_all_listeners(void)
{
  if (!listeners)
    return;
  log_info("Closing all listeners.");

  SMARTLIST_FOREACH(listeners, listener_t *, lsn,
                    { listener_free(lsn); });
  smartlist_free(listeners);
  listeners = NULL;
}

/**
   This function is called when any listener receives a connection.
 */
static void
listener_cb(struct evconnlistener *evcl, evutil_socket_t fd,
            struct sockaddr *peeraddr, int peerlen,
            void *closure)
{
  struct event_base *base = evconnlistener_get_base(evcl);
  listener_t *lsn = closure;
  char *peername = printable_address(peeraddr, peerlen);
  conn_t *conn = proto_conn_create(lsn->cfg);
  struct bufferevent *buf = bufferevent_socket_new(base, fd,
                                                   BEV_OPT_CLOSE_ON_FREE);

  if (!conn || !buf) {
    log_warn("%s: failed to set up new connection from %s.",
             safe_str(lsn->address), safe_str(peername));
    if (buf)
      bufferevent_free(buf);
    else
      evutil_closesocket(fd);
    if (conn)
      proto_conn_free(conn);
    free(peername);
    return;
  }

  if (!connections)
    connections = smartlist_create();
  smartlist_add(connections, conn);
  log_debug("%s: new connection from %s (%d total)",
	    safe_str(lsn->address),
	    safe_str(peername),
	    smartlist_len(connections));

  conn->peername = peername;
  conn->buffer = buf;
  switch (conn->mode) {
  case LSN_SIMPLE_CLIENT: simple_client_listener_cb(conn); break;
  case LSN_SOCKS_CLIENT:  socks_client_listener_cb(conn);  break;
  case LSN_SIMPLE_SERVER: simple_server_listener_cb(conn); break;
  default:
    obfs_abort();
  }
}

/** Given a connection <b>addr_conn</b> carrying addresses that we
    should connect to, use <b>connect_conn</b> to connect to
    them. Also, set the callbacks of the connecting bufferevent, with
    <b>readcb</b> being the read callback. */
static int
conn_connect(conn_t *addr_conn, conn_t *connect_conn,
             bufferevent_data_cb readcb)
{
  char *peername;
  struct evutil_addrinfo *addr = config_get_target_addr(addr_conn->cfg);
  struct bufferevent *buf = connect_conn->buffer;

  if (!addr) {
    log_warn("%s: no target addresses available", safe_str(addr_conn->peername));
    return -1;
  }

  bufferevent_setcb(buf, readcb, NULL, pending_conn_cb, connect_conn);

  do {
    peername = printable_address(addr->ai_addr, addr->ai_addrlen);

    if (bufferevent_socket_connect(buf, addr->ai_addr, addr->ai_addrlen) >= 0)
      goto success;
    log_info("%s: connection to %s failed: %s",
             safe_str(addr_conn->peername), safe_str(peername),
             evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
    free(peername);
    addr = addr->ai_next;
  } while (addr);

  log_warn("%s: all outbound connection attempts failed",
           addr_conn->peername);

  return -1;

 success:
  log_info("%s (%s): Successful outbound connection to '%s'.",
           safe_str(addr_conn->peername),
           addr_conn->cfg->vtable->name, safe_str(peername));
  bufferevent_enable(buf, EV_READ|EV_WRITE);
  connect_conn->peername = peername;

  /* If we are a server, we should note this connection and consider
     it in our heartbeat status messages. */
  if (addr_conn->mode == LSN_SIMPLE_SERVER)
    status_note_connection(addr_conn->peername);

  return 0;
}

/** Create the other side of connection <b>conn</b> on a circuit. */
static conn_t *
create_other_side_of_conn(const conn_t *conn)
{
  struct event_base *base = bufferevent_get_base(conn->buffer);
  struct bufferevent *buf;
  conn_t *newconn;

  buf = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
  if (!buf) {
    log_warn("%s: unable to create outbound socket buffer", safe_str(conn->peername));
    return NULL;
  }

  newconn = proto_conn_create(conn->cfg);
  if (!newconn) {
    log_warn("%s: failed to allocate state for outbound connection",
             safe_str(conn->peername));
    bufferevent_free(buf);
    return NULL;
  }

  /* associate bufferevent with the new connection. */
  newconn->buffer = buf;

  obfs_assert(connections);
  smartlist_add(connections, newconn);

  return newconn;
}

/**
   This function is called when an upstream client connects to us in
   simple client mode.
*/
static void
simple_client_listener_cb(conn_t *conn)
{
  obfs_assert(conn);
  obfs_assert(conn->mode == LSN_SIMPLE_CLIENT);
  log_debug("%s: simple client connection", safe_str(conn->peername));

  bufferevent_setcb(conn->buffer, upstream_read_cb, NULL, error_cb, conn);

  conn_t *newconn = create_other_side_of_conn(conn);
  if (!newconn) {
    conn_free(conn);
    return;
  }

  if (circuit_create(conn, newconn)) {
    conn_free(conn);
    return;
  }

  if (conn_connect(conn, newconn, downstream_read_cb) < 0) {
    conn_free(conn); /* circuit_free() should also free newconn. */
    return;
  }

  log_debug("%s: setup complete", safe_str(conn->peername));
}

/**
   This function is called when an upstream client connects to us in
   socks mode.
*/
static void
socks_client_listener_cb(conn_t *conn)
{
  obfs_assert(conn);
  obfs_assert(conn->mode == LSN_SOCKS_CLIENT);
  log_debug("%s: socks client connection", safe_str(conn->peername));

  circuit_create_socks(conn);

  bufferevent_setcb(conn->buffer, socks_read_cb, NULL, error_cb, conn);
  bufferevent_enable(conn->buffer, EV_READ|EV_WRITE);

  /* Do not create a circuit at this time; the socks handler will do
     it after it learns the remote peer address. */

  log_debug("%s: setup complete", safe_str(conn->peername));
}

/**
   This function is called when a remote client connects to us in
   server mode.
*/
static void
simple_server_listener_cb(conn_t *conn)
{
  obfs_assert(conn);
  obfs_assert(conn->mode == LSN_SIMPLE_SERVER);
  log_debug("%s: server connection", safe_str(conn->peername));

  bufferevent_setcb(conn->buffer, downstream_read_cb, NULL, error_cb, conn);

  conn_t *newconn = create_other_side_of_conn(conn);
  if (!newconn) {
    conn_free(conn);
    return;
  }

  if (circuit_create(newconn, conn)) {
    conn_free(conn);
    return;
  }

  if (conn_connect(conn, newconn, upstream_read_cb) < 0) {
    conn_free(conn);
    return;
  }

  log_debug("%s: setup complete", safe_str(conn->peername));
}

/**
   Deallocates conn_t 'conn'.
*/
static void
conn_free(conn_t *conn)
{
  if (conn->circuit)
    circuit_free(conn->circuit); /* will recurse and take care of us */
  else {
    if (connections) {
      smartlist_remove(connections, conn);
      log_debug("Closing connection with %s; %d remaining",
                safe_str(conn->peername) ? safe_str(conn->peername) : "'unknown'",
                smartlist_len(connections));
    }
    if (conn->peername)
      free(conn->peername);
    if (conn->buffer)
      bufferevent_free(conn->buffer);
    proto_conn_free(conn);

    /* If this was the last connection AND we are shutting down,
       finish shutdown. */
    if (shutting_down && connections && smartlist_len(connections) == 0) {
      smartlist_free(connections);
      connections = NULL;
      finish_shutdown();
    }
  }
}

/**
   Closes associated connection if the output evbuffer of 'bev' is
   empty.
*/
static void
conn_free_on_flush(struct bufferevent *bev, void *arg)
{
  conn_t *conn = arg;
  log_debug("%s for %s", __func__, safe_str(conn->peername));

  if (evbuffer_get_length(bufferevent_get_output(bev)) == 0)
    conn_free(conn);
}

int
circuit_create(conn_t *up, conn_t *down)
{
  if (!up || !down)
    return -1;

  obfs_assert(up->cfg == down->cfg);

  circuit_t *r = proto_circuit_create(up->cfg);
  r->upstream = up;
  r->downstream = down;
  up->circuit = r;
  down->circuit = r;
  return 0;
}

static void
circuit_create_socks(conn_t *up)
{
  obfs_assert(up);

  circuit_t *r = proto_circuit_create(up->cfg);
  r->upstream = up;
  r->socks_state = socks_state_new();
  up->circuit = r;
}

static int
circuit_add_down(circuit_t *circuit, conn_t *down)
{
  if (!down)
    return -1;
  circuit->downstream = down;
  down->circuit = circuit;
  return 0;
}

static void
circuit_free(circuit_t *circuit)
{
  /* keep the cfg so that we can use its vtable for proto_circuit_free() */
  config_t *cfg = circuit->upstream->cfg;
  obfs_assert(cfg);

  /* break the circular references before deallocating each side */
  circuit->upstream->circuit = NULL;
  conn_free(circuit->upstream);
  if (circuit->downstream) {
    circuit->downstream->circuit = NULL;
    conn_free(circuit->downstream);
  }
  if (circuit->socks_state)
    socks_state_free(circuit->socks_state);

  proto_circuit_free(circuit, cfg);
}

/**
    This callback is responsible for handling SOCKS traffic.
*/
static void
socks_read_cb(struct bufferevent *bev, void *arg)
{
  conn_t *conn = arg;
  socks_state_t *socks;
  enum socks_ret socks_ret;

  log_debug("%s: %s", safe_str(conn->peername), __func__);

  obfs_assert(conn->circuit);
  obfs_assert(conn->circuit->socks_state);
  socks = conn->circuit->socks_state;

  do {
    enum socks_status_t status = socks_state_get_status(socks);
    if (status == ST_SENT_REPLY) {
      /* We shouldn't be here. */
      obfs_abort();
    } else if (status == ST_HAVE_ADDR) {
      int af, r;
      uint16_t port;
      const char *addr=NULL;
      r = socks_state_get_address(socks, &af, &addr, &port);
      obfs_assert(r==0);

      log_info("%s: socks: trying to connect to %s:%u",
               safe_str(conn->peername), safe_str(addr), port);
      conn_t *newconn = create_other_side_of_conn(conn);
      if (!newconn) {
        conn_free(conn);
        return;
      }

      /* Set up a temporary FQDN peername. When we actually connect to the
         host, we will replace this peername with the IP address we
         connected to. */
      obfs_asprintf(&newconn->peername, "%s:%u", addr, port);

      if (circuit_add_down(conn->circuit, newconn)) {
        /* XXXX send socks reply */
        conn_free(conn);
        return;
      }

      bufferevent_setcb(newconn->buffer, downstream_read_cb, NULL, pending_socks_cb, newconn);

      /* further upstream data will be processed once the downstream
         side is established */
      bufferevent_disable(conn->buffer, EV_READ|EV_WRITE);

      if (bufferevent_socket_connect_hostname(newconn->buffer, get_evdns_base(),
                                              af, addr, port) < 0) {
        log_warn("%s: outbound connection to %s:%u failed: %s",
                 safe_str(conn->peername), addr, port,
                 evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
        conn_free(conn);
        return;
      }
      return;
    }

    socks_ret = handle_socks(bufferevent_get_input(bev),
                             bufferevent_get_output(bev),
                             socks);
  } while (socks_ret == SOCKS_GOOD);

  if (socks_ret == SOCKS_INCOMPLETE)
    return; /* need to read more data. */
  else if (socks_ret == SOCKS_BROKEN)
    conn_free(conn); /* XXXX send socks reply */
  else if (socks_ret == SOCKS_CMD_NOT_CONNECT) {
    bufferevent_enable(bev, EV_WRITE);
    bufferevent_disable(bev, EV_READ);
    socks5_send_reply(bufferevent_get_output(bev), socks,
                      SOCKS5_FAILED_UNSUPPORTED);
    bufferevent_setcb(bev, NULL,
                      conn_free_on_flush, flush_error_cb, conn);
    return;
  }
}

/**
   This callback is responsible for handling "upstream" traffic --
   traffic coming in from the higher-level client or server that needs
   to be obfuscated and transmitted.
 */
static void
upstream_read_cb(struct bufferevent *bev, void *arg)
{
  conn_t *up = arg;
  conn_t *down;
  log_debug("%s: %s, %lu bytes available", safe_str(up->peername), __func__,
            (unsigned long)evbuffer_get_length(bufferevent_get_input(bev)));

  obfs_assert(up->buffer == bev);
  obfs_assert(up->circuit);
  obfs_assert(up->circuit->downstream);
  obfs_assert(up->circuit->is_open);
  obfs_assert(!up->circuit->is_flushing);

  down = up->circuit->downstream;
  if (proto_send(up,
                 bufferevent_get_input(up->buffer),
                 bufferevent_get_output(down->buffer))) {
    log_debug("%s: error during transmit.",
              safe_str(up->peername));
    conn_free(up);
  } else {
    log_debug("%s: transmitted %lu bytes",
              safe_str(down->peername),
              (unsigned long)
              evbuffer_get_length(bufferevent_get_output(down->buffer)));
  }
}

/**
   This callback is responsible for handling "downstream" traffic --
   traffic coming in from our remote peer that needs to be deobfuscated
   and passed to the upstream client or server.
 */
static void
downstream_read_cb(struct bufferevent *bev, void *arg)
{
  conn_t *down = arg;
  conn_t *up;
  enum recv_ret r;

  log_debug("%s: %s, %lu bytes available",
            safe_str(down->peername), __func__,
            (unsigned long)evbuffer_get_length(bufferevent_get_input(bev)));

  obfs_assert(down->buffer == bev);
  obfs_assert(down->circuit);
  obfs_assert(down->circuit->upstream);
  obfs_assert(down->circuit->is_open);
  obfs_assert(!down->circuit->is_flushing);
  up = down->circuit->upstream;


  r = proto_recv(down,
                 bufferevent_get_input(down->buffer),
                 bufferevent_get_output(up->buffer));

  if (r == RECV_BAD) {
    log_debug("%s: error during receive.", safe_str(down->peername));
    conn_free(down);
  } else {
    log_debug("%s: forwarded %lu bytes", safe_str(down->peername),
              (unsigned long)
              evbuffer_get_length(bufferevent_get_output(up->buffer)));
    if (r == RECV_SEND_PENDING) {
      log_debug("%s: reply of %lu bytes", safe_str(down->peername),
                (unsigned long)
                evbuffer_get_length(bufferevent_get_input(up->buffer)));
      if (proto_send(up,
                     bufferevent_get_input(up->buffer),
                     bufferevent_get_output(down->buffer)) < 0) {
        log_debug("%s: error during reply.", safe_str(down->peername));
        conn_free(down);
      } else {
        log_debug("%s: transmitted %lu bytes", safe_str(down->peername),
                  (unsigned long)
                  evbuffer_get_length(bufferevent_get_output(down->buffer)));
      }
    }
  }
}

/**
   Something broke one side of the connection, or we reached EOF.
   We prepare the connection to be closed ASAP.
 */
static void
error_or_eof(conn_t *conn)
{
  circuit_t *circ = conn->circuit;
  conn_t *conn_flush;
  struct bufferevent *bev_err = conn->buffer;
  struct bufferevent *bev_flush;

  log_debug("%s for %s", __func__, safe_str(conn->peername));
  if (!circ || !circ->upstream || !circ->downstream || !circ->is_open
      || circ->is_flushing) {
    conn_free(conn);
    return;
  }

  conn_flush = (conn == circ->upstream) ? circ->downstream
                                        : circ->upstream;
  bev_flush = conn_flush->buffer;
  if (evbuffer_get_length(bufferevent_get_output(bev_flush)) == 0) {
    conn_free(conn);
    return;
  }

  circ->is_flushing = 1;

  /* Stop reading and writing; wait for the other side to flush if it has
   * data. */
  bufferevent_disable(bev_err, EV_READ|EV_WRITE);
  bufferevent_setcb(bev_err, NULL, NULL, flush_error_cb, conn);

  /* We can ignore any data that arrives; we should free the connection
   * when we're done flushing */
  bufferevent_setcb(bev_flush, NULL,
                    conn_free_on_flush, flush_error_cb, conn_flush);
}

/**
   Called when an "event" happens on an already-connected socket.
   This can only be an error or EOF.
*/
static void
error_cb(struct bufferevent *bev, short what, void *arg)
{
  conn_t *conn = arg;
  int errcode = EVUTIL_SOCKET_ERROR();
  if (what & BEV_EVENT_ERROR) {
    log_debug("%s for %s: what=0x%04x errno=%d", __func__,
              safe_str(conn->peername),
              what, errcode);
  } else {
    log_debug("%s for %s: what=0x%04x", __func__,
              safe_str(conn->peername), what);
  }

  /* It should be impossible to get here with BEV_EVENT_CONNECTED. */
  obfs_assert(!(what & BEV_EVENT_CONNECTED));

  if (what & BEV_EVENT_ERROR) {
    log_info("Error talking to %s: %s",
             safe_str(conn->peername),
             evutil_socket_error_to_string(errcode));
  } else if (what & BEV_EVENT_EOF) {
    log_info("EOF from %s", safe_str(conn->peername));
  } else if (what & BEV_EVENT_TIMEOUT) {
    log_info("Timeout talking to %s", safe_str(conn->peername));
  }
  error_or_eof(conn);
}

/**
   Called when an event happens on a socket that's in the process of
   being flushed and closed.  As above, this can only be an error.
*/
static void
flush_error_cb(struct bufferevent *bev, short what, void *arg)
{
  conn_t *conn = arg;
  int errcode = EVUTIL_SOCKET_ERROR();
  log_debug("%s for %s: what=0x%04x errno=%d", __func__, safe_str(conn->peername),
            what, errcode);

  /* It should be impossible to get here with BEV_EVENT_CONNECTED. */
  obfs_assert(!(what & BEV_EVENT_CONNECTED));

  obfs_assert(conn->circuit);
  obfs_assert(conn->circuit->is_flushing);

  if (what & BEV_EVENT_ERROR) {
    log_warn("Error during flush of connection with %s: %s",
             safe_str(conn->peername),
             evutil_socket_error_to_string(errcode));
  } else if (what & BEV_EVENT_EOF) {
    log_info("EOF during flush of connection with %s",
              safe_str(conn->peername));
  } else if (what & BEV_EVENT_TIMEOUT) {
    log_info("EOF during flush of connection with %s",
             safe_str(conn->peername));
  }

  conn_free(conn);
  return;
}

/**
   Called when an event happens on a socket that's still waiting to
   be connected.  We expect to get BEV_EVENT_CONNECTED, which
   indicates that the connection is now open, but we might also get
   errors as above.
*/
static void
pending_conn_cb(struct bufferevent *bev, short what, void *arg)
{
  conn_t *conn = arg;
  log_debug("%s: %s", safe_str(conn->peername), __func__);

  /* Upon successful connection, enable traffic on both sides of the
     connection, and replace this callback with the regular error_cb */
  if (what & BEV_EVENT_CONNECTED) {
    circuit_t *circ = conn->circuit;
    obfs_assert(circ);
    obfs_assert(!circ->is_flushing);

    circ->is_open = 1;

    log_debug("%s: Successful connection", safe_str(conn->peername));

    /* Queue handshake, if any. */
    if (proto_handshake(circ->downstream,
                        bufferevent_get_output(circ->downstream->buffer))<0) {
      log_debug("%s: Error during handshake", safe_str(conn->peername));
      conn_free(conn);
      return;
    }

    bufferevent_setcb(circ->upstream->buffer,
                      upstream_read_cb, NULL, error_cb, circ->upstream);
    bufferevent_setcb(circ->downstream->buffer,
                      downstream_read_cb, NULL, error_cb, circ->downstream);

    bufferevent_enable(circ->upstream->buffer, EV_READ|EV_WRITE);
    bufferevent_enable(circ->downstream->buffer, EV_READ|EV_WRITE);
    return;
  }

  /* Otherwise, must be an error */
  error_cb(bev, what, arg);
}

/**
   Called when an event happens on a socket in socks mode.
   Both connections and errors are possible; must generate
   appropriate socks messages on the upstream side.
 */
static void
pending_socks_cb(struct bufferevent *bev, short what, void *arg)
{
  conn_t *down = arg;
  circuit_t *circ = down->circuit;
  conn_t *up;
  socks_state_t *socks;

  log_debug("%s: %s", safe_str(down->peername), __func__);

  obfs_assert(circ);
  obfs_assert(circ->upstream);
  obfs_assert(circ->socks_state);
  obfs_assert(circ->downstream == down);

  up = circ->upstream;
  socks = circ->socks_state;

  /* If we got an error while in the ST_HAVE_ADDR state, chances are
     that we failed connecting to the host requested by the CONNECT
     call. This means that we should send a negative SOCKS reply back
     to the client and terminate the connection.
     XXX properly distinguish BEV_EVENT_EOF from BEV_EVENT_ERROR;
     errno isn't meaningful in that case...  */
  if ((what & (BEV_EVENT_EOF|BEV_EVENT_ERROR|BEV_EVENT_TIMEOUT))) {
    int err = EVUTIL_SOCKET_ERROR();
    if (what & BEV_EVENT_ERROR)
      log_warn("Connection error: %s", evutil_socket_error_to_string(err));
    else
      log_info("EOF or timeout while waiting for socks");
    if (socks_state_get_status(socks) == ST_HAVE_ADDR) {
      socks_send_reply(socks, bufferevent_get_output(up->buffer), err);
    }
    error_or_eof(down);
    return;
  }

  /* Additional work to do for BEV_EVENT_CONNECTED: send a happy
     response to the client and switch to the actual obfuscated
     protocol handlers. */
  if (what & BEV_EVENT_CONNECTED) {
    struct sockaddr_storage ss;
    struct sockaddr *sa = (struct sockaddr*)&ss;
    socklen_t slen = sizeof(ss);

    obfs_assert(!circ->is_flushing);

    if (getpeername(bufferevent_getfd(bev), sa, &slen) < 0) {
      log_warn("%s: getpeername() failed.", __func__);
      conn_free(down);
      return;
    }

    { /* We have to update our connection's peername.  If it was an
      FQDN SOCKS request, the current peername contains the FQDN
      string, and we want to replace it with the actual IP address we
      connected to. */

      char *peername = printable_address(sa, slen);

      if (down->peername) {
        log_debug("We connected to our SOCKS destination! "
                  "Replacing peername '%s' with '%s'",
                  safe_str(down->peername),
                  safe_str(peername));
        free(down->peername);
      }
      down->peername = peername;
    }

    if (socks_state_set_address(socks, sa) < 0) {
      log_warn("%s: socks_state_set_address() failed.", __func__);
      conn_free(down);
      return;
    }

    socks_send_reply(socks, bufferevent_get_output(up->buffer), 0);

    /* Switch to regular upstream behavior. */
    socks_state_free(socks);
    circ->socks_state = NULL;
    circ->is_open = 1;
    log_debug("%s: Successful outbound connection to %s",
              safe_str(up->peername),
              safe_str(down->peername));

    bufferevent_setcb(up->buffer, upstream_read_cb, NULL, error_cb, up);
    bufferevent_setcb(down->buffer, downstream_read_cb, NULL, error_cb, down);
    bufferevent_enable(up->buffer, EV_READ|EV_WRITE);
    bufferevent_enable(down->buffer, EV_READ|EV_WRITE);

    /* Queue handshake, if any. */
    if (proto_handshake(down, bufferevent_get_output(down->buffer))) {
      log_debug("%s: Error during handshake", safe_str(down->peername));
      conn_free(down);
      return;
    }

    if (evbuffer_get_length(bufferevent_get_input(up->buffer)) > 0)
      upstream_read_cb(up->buffer, up);
    return;
  }
}
