/* Copyright 2011 Nick Mathewson, George Kadianakis
   See LICENSE for other credits and copying information
*/

/**
 * \file socks.c
 * \headerfile socks.h
 * \brief SOCKS{5,4,4a} server.
 *
 * \details
 * Every <b>circuit_t</b> has a <b>socks_state_t</b> attached
 * to it, representing the state of the SOCKS protocol on that
 * circuit.
 *
 * If we are a client using SOCKS, and we haven't completed the SOCKS
 * handshake, socks_read_cb() passes all incoming data to
 * handle_socks().  On a valid 'Client request' packet, we parse the
 * requested address into <b>struct parsereq</b> of
 * <b>socks_state_t</b>, and toggle SOCKS' state to ST_HAVE_ADDR. When
 * socks_read_cb() notices that we are in ST_HAVE_ADDR, it connects to
 * the requested address to set up the proxying tunnel. Upon a
 * successful connection, pending_socks_cb() sends back a 'Server
 * reply' and sets up the proxying tunnel.
 *
 * <em>SOCKS5 protocol handshake:</em>
 *
 * Client ------------------------> Server\n
 * Method Negotiation Packet (socks5_handle_negotiation())
 *
 * Client <------------------------ Server\n
 * Method Negotiation Reply (socks5_do_negotiation())
 *
 * Client ------------------------> Server\n
 * Client request (socks5_handle_request())
 *
 * Client <------------------------ Server\n
 * Server reply (socks5_send_reply())
 *
 * <em>SOCKS{4,4a} protocol handshake:</em>
 *
 * Client ------------------------> Server\n
 * Client Request (socks4_read_request())
 *
 * Client <------------------------ Server\n
 * Server reply (socks4_send_reply())
 **/

#include "util.h"

#define SOCKS_PRIVATE
#include "socks.h"

#include <errno.h>

#include <event2/buffer.h>


typedef unsigned char uchar;

/**
   Creates a new 'socks_state_t' object.
*/
socks_state_t *
socks_state_new(void)
{
  socks_state_t *state = xzalloc(sizeof(socks_state_t));
  state->state = ST_WAITING;
  return state;
}

/**
   Deallocates memory of socks_state_t 's'.
*/
void
socks_state_free(socks_state_t *s)
{
  memset(s,0x0b, sizeof(socks_state_t));
  free(s);
}

#ifdef _WIN32
#define ERR(e) WSA##e
#else
#define ERR(e) e
#endif

/**
   This function receives an errno(3) 'error' and returns the reply
   code that should be sent to the SOCKS client.
   If 'error' is 0 it means that no errors were encountered, and
   SOCKS{4,5}_SUCCESS is returned.
*/
static int
socks_errno_to_reply(socks_state_t *state, int error)
{
  if (state->version == SOCKS4_VERSION) {
    if (!error)
      return SOCKS4_SUCCESS;
    else
      return SOCKS4_FAILED;
  } else if (state->version == SOCKS5_VERSION) {
    if (!error)
      return SOCKS5_SUCCESS;
    else {
      switch (error) {
      case ERR(ENETUNREACH):
        return SOCKS5_FAILED_NETUNREACH;
      case ERR(EHOSTUNREACH):
        return SOCKS5_FAILED_HOSTUNREACH;
      case ERR(ECONNREFUSED):
        return SOCKS5_FAILED_REFUSED;
      default:
        return SOCKS5_FAILED_GENERAL;
      }
    }
  } else
    return -1;
}

#undef ERR

/**
   Takes a SOCKS5 command request from 'source', it evaluates it and
   if it's legit it parses it into 'parsereq'.

   It returns SOCKS_GOOD if everything went fine.
   It returns SOCKS_INCOMPLETE if we need more data from the client.
   It returns SOCKS_BROKEN if we didn't like something.
   It returns SOCKS_CMD_NOT_CONNECT if the client asked for something
   else other than CONNECT.  If that's the case we should send a reply
   back to the client telling him that we don't support it.

*/
enum socks_ret
socks5_handle_request(struct evbuffer *source, struct parsereq *parsereq)
{
  /** XXX: max FQDN size is 255. */
  /* #define MAXFQDN */
  char destaddr[255+1]; /* Dest address */
  uint16_t destport;    /* Dest port */

  unsigned int buflength = evbuffer_get_length(source);

  if (buflength < SIZEOF_SOCKS5_STATIC_REQ+1) {
    log_debug("socks: request packet is too small (1).");
    return SOCKS_INCOMPLETE;
  }

  /* We only need the socks5_req and an extra byte to get
     the addrlen in an FQDN request.
  */
  uchar p[SIZEOF_SOCKS5_STATIC_REQ+1];
  if (evbuffer_copyout(source, p, SIZEOF_SOCKS5_STATIC_REQ+1) < 0)
    goto err;

  /* p[0] = Version
     p[1] = Command field
     p[2] = Reserved field */
  if (p[0] != SOCKS5_VERSION || p[2] != 0x00) {
    log_debug("socks: Corrupted packet. Discarding.");
    goto err;
  }

  if (p[1] != SOCKS5_CMD_CONNECT)
    return SOCKS_CMD_NOT_CONNECT; /* We must send reply to the client. */

  unsigned int addrlen,af,extralen=0;
  /* p[3] is Address type field */
  switch(p[3]) {
  case SOCKS5_ATYP_IPV4:
    addrlen = 4;
    af = AF_INET;
    /* minimum packet size:
       socks5_req - <version byte> + addrlen + port */
    break;
  case SOCKS5_ATYP_IPV6:
    addrlen = 16;
    af = AF_INET6;
    break;
  case SOCKS5_ATYP_FQDN:  /* Do we actually need FQDN support? */
    addrlen = p[4];
    extralen = 1;
    af = AF_UNSPEC;
    /* as above, but we also have the addrlen field byte */
    break;
  default:
    log_debug("socks: Address type not supported. Go away.");
    goto err;
  }

  int minsize = SIZEOF_SOCKS5_STATIC_REQ + addrlen + extralen + 2;
  if (buflength < minsize) {
    log_debug("socks: request packet too small %d:%d (2)", buflength, minsize);
    return SOCKS_INCOMPLETE;
  }

  /* Drain data from the buffer to get to the good part, the actual
     address and port. */
  if (evbuffer_drain(source, SIZEOF_SOCKS5_STATIC_REQ) == -1)
    goto err;
  /* If it is an FQDN request, drain the addrlen byte as well. */
  if (af == AF_UNSPEC)
    if (evbuffer_drain(source, 1) == -1)
      goto err;

  if (evbuffer_remove(source, destaddr, addrlen) != addrlen)
    obfs_abort();

  if (evbuffer_remove(source, (char *)&destport, 2) != 2)
    obfs_abort();

  destaddr[addrlen] = '\0';

  if (af == AF_UNSPEC) {
    obfs_assert(addrlen < sizeof(parsereq->addr));
    memcpy(parsereq->addr, destaddr, addrlen+1);
  } else {
    char a[16];
    obfs_assert(addrlen <= 16);
    memcpy(a, destaddr, addrlen);
    if (evutil_inet_ntop(af, destaddr, parsereq->addr,
                         sizeof(parsereq->addr)) == NULL)
      goto err;
  }

  parsereq->port = ntohs(destport);
  parsereq->af = af;

  return SOCKS_GOOD;

 err:
  return SOCKS_BROKEN;
}

/**
   This sends the appropriate SOCKS5 reply to the client on
   'reply_dest', according to 'status'.

   Server Reply (Server -> Client):
   | version | rep | rsv | atyp | destaddr           | destport
     1b         1b    1b    1b       4b/16b/1+Nb         2b
*/
void
socks5_send_reply(struct evbuffer *reply_dest, socks_state_t *state,
                  int status)
{
  uchar p[4];
  uchar addr[16];
  const char *extra = NULL;
  int addrlen;
  uint16_t port;
  /* We either failed or succeded.
     Either way, we should send something back to the client */
  p[0] = SOCKS5_VERSION;    /* Version field */
  /* Reply field */
  p[1] = status;
  p[2] = 0;                 /* Reserved */

  /* If status is SOCKS5_FAILED_UNSUPPORTED, it means that we failed
     before populating state->parsereq. We will just fill the rest
     of the reply packet with zeroes and ship it off.
   */
  if (status == SOCKS5_FAILED_UNSUPPORTED) {
    addrlen = 4;
    p[3] = SOCKS5_ATYP_IPV4;
    memset(addr,'\x00',4);
    port = 0;
  } else { /* state->parsereq is fine. move on. */
    if (state->parsereq.af == AF_UNSPEC) {
      addrlen = 1;
      addr[0] = strlen(state->parsereq.addr);
      extra = state->parsereq.addr;
      p[3] = SOCKS5_ATYP_FQDN;
    } else {
      addrlen = (state->parsereq.af == AF_INET) ? 4 : 16;
      p[3] = (state->parsereq.af == AF_INET)
        ? SOCKS5_ATYP_IPV4 : SOCKS5_ATYP_IPV6;
      evutil_inet_pton(state->parsereq.af, state->parsereq.addr, addr);
    }
    port = htons(state->parsereq.port);
  }

  evbuffer_add(reply_dest, p, 4);
  evbuffer_add(reply_dest, addr, addrlen);
  if (extra)
    evbuffer_add(reply_dest, extra, strlen(extra));
  evbuffer_add(reply_dest, &port, 2);

  state->state = ST_SENT_REPLY; /* SOCKS phase is now done. */
}

/**
   This function sends a method negotiation reply to 'dest'.
   If 'neg_was_success' is true send a positive response,
   otherwise send a negative one.
   It returns -1 if no suitable negotiation methods were found,
   or if there was an error during replying.

   Method Negotiation Reply (Server -> Client):
   | version | method selected |
       1b           1b
*/
static enum socks_ret
socks5_do_negotiation(struct evbuffer *dest, unsigned int neg_was_success)
{
  uchar reply[2];
  reply[0] = SOCKS5_VERSION;

  reply[1] = neg_was_success ? SOCKS5_METHOD_NOAUTH : SOCKS5_METHOD_FAIL;

  if (evbuffer_add(dest, reply, 2) == -1 || !neg_was_success)
    return SOCKS_BROKEN;
  else
    return SOCKS_GOOD;
}

/**
   This function handles the initial SOCKS5 packet in 'source' sent by
   the client which negotiates the version and method of SOCKS.  If
   the packet is actually valid, we reply to 'dest'.

   Method Negotiation Packet (Client -> Server):
   nmethods | methods[nmethods] |
       b           1-255b
*/
enum socks_ret
socks5_handle_negotiation(struct evbuffer *source,
                          struct evbuffer *dest, socks_state_t *state)
{
  unsigned int found_noauth, i;
  uchar nmethods;
  uchar methods[0xFF];

  evbuffer_copyout(source, &nmethods, 1);

  if (evbuffer_get_length(source) < nmethods + 1) {
    return SOCKS_INCOMPLETE; /* need more data */
  }

  evbuffer_drain(source, 1);

  if (evbuffer_remove(source, methods, nmethods) < 0)
    obfs_abort();

  for (found_noauth=0, i=0; i<nmethods ; i++) {
    if (methods[i] == SOCKS5_METHOD_NOAUTH) {
      found_noauth = 1;
      break;
    }
  }

  return socks5_do_negotiation(dest,found_noauth);
}

/**
   Takes a SOCKS4/SOCKS4a command request from 'source', it evaluates
   it and if it's legit it parses it into 'parsereq'.

   It returns SOCKS_GOOD if everything went fine.
   It returns SOCKS_INCOMPLETE if we need more data from the client.
   It returns SOCKS_BROKEN if we didn't like something.
*/

/* XXXX rename to socks4_handle_request or something. */
enum socks_ret
socks4_read_request(struct evbuffer *source, socks_state_t *state)
{
  /* Format is:
       1 byte: Socks version (==4, already read)
       1 byte: command code [== 1 for connect]
       2 bytes: port
       4 bytes: IPv4 address
       X bytes: userID, terminated with NUL
       Optional: X bytes: domain, terminated with NUL */
  uchar header[7];
  int is_v4a;
  uint16_t portnum;
  uint32_t ipaddr;
  struct evbuffer_ptr end_of_user, end_of_hostname;
  size_t user_len, hostname_len=0;
  if (evbuffer_get_length(source) < 7)
    return SOCKS_INCOMPLETE; /* more bytes needed */
  evbuffer_copyout(source, (char*)header, 7);
  if (header[0] != 1) {
    log_debug("socks: Only CONNECT supported.");
    return SOCKS_BROKEN;
  }
  memcpy(&portnum, header+1, 2);
  memcpy(&ipaddr, header+3, 4);
  portnum = ntohs(portnum);
  ipaddr = ntohl(ipaddr);
  is_v4a = (ipaddr & 0xff) != 0 && (ipaddr & 0xffffff00)==0;

  evbuffer_ptr_set(source, &end_of_user, 7, EVBUFFER_PTR_SET);
  end_of_user = evbuffer_search(source, "\0", 1, &end_of_user);
  if (end_of_user.pos == -1) {
    if (evbuffer_get_length(source) > SOCKS4_MAX_LENGTH)
      return SOCKS_BROKEN;
    return SOCKS_INCOMPLETE;
  }
  user_len = end_of_user.pos - 7;
  if (is_v4a) {
    if (end_of_user.pos == evbuffer_get_length(source)-1)
      return SOCKS_INCOMPLETE; /*more data needed */
    end_of_hostname = end_of_user;
    evbuffer_ptr_set(source, &end_of_hostname, 1, EVBUFFER_PTR_ADD);
    end_of_hostname = evbuffer_search(source, "\0", 1, &end_of_hostname);
    if (end_of_hostname.pos == -1) {
      if (evbuffer_get_length(source) > SOCKS4_MAX_LENGTH)
        return SOCKS_BROKEN;
      return SOCKS_INCOMPLETE;
    }
    hostname_len = end_of_hostname.pos - end_of_user.pos - 1;
    if (hostname_len >= sizeof(state->parsereq.addr)) {
      log_debug("socks4a: Hostname too long");
      return SOCKS_BROKEN;
    }
  }

  /* Okay.  If we get here, all the data is available. */
  evbuffer_drain(source, 7+user_len+1); /* discard username */
  state->parsereq.af = AF_INET; /* SOCKS4a is IPv4 only */
  state->parsereq.port = portnum;
  if (is_v4a) {
    evbuffer_remove(source, state->parsereq.addr, hostname_len);
    state->parsereq.addr[hostname_len] = '\0';
    evbuffer_drain(source, 1);
  } else {
    struct in_addr in;
    in.s_addr = htonl(ipaddr);
    if (evutil_inet_ntop(AF_INET, &in, state->parsereq.addr,
                         sizeof(state->parsereq.addr)) == NULL)
      return SOCKS_BROKEN;
  }

  return SOCKS_GOOD;
}

/**
   This sends the appropriate SOCKS4 reply to the client on
   'reply_dest', according to 'status'.
*/
void
socks4_send_reply(struct evbuffer *dest, socks_state_t *state, int status)
{
  uint16_t portnum;
  struct in_addr in;
  uchar msg[8];
  portnum = htons(state->parsereq.port);
  if (evutil_inet_pton(AF_INET, state->parsereq.addr, &in)!=1)
    in.s_addr = 0;

  /* Nul byte */
  msg[0] = 0;
  /* convert to socks4 status */
  msg[1] = status;
  memcpy(msg+2, &portnum, 2);
  /* ASN: What should we do here in the case of an FQDN request? */
  memcpy(msg+4, &in.s_addr, 4);
  evbuffer_add(dest, msg, 8);
}

/**
   We are given SOCKS data from the network.
   We figure out what what SOCKS version it is and act accordingly.

   It returns SOCKS_GOOD if everything went fine.
   It returns SOCKS_INCOMPLETE if we need more data from the client.
   It returns SOCKS_BROKEN if we didn't like something.
   It returns SOCKS_CMD_NOT_CONNECT if the client asked for something
   else other than CONNECT.  If that's the case we should send a reply
   back to the client telling him that we don't support it.
*/
enum socks_ret
handle_socks(struct evbuffer *source, struct evbuffer *dest,
             socks_state_t *socks_state)
{
  enum socks_ret r;

  if (socks_state->broken)
    return SOCKS_BROKEN;

  if (evbuffer_get_length(source) < MIN_SOCKS_PACKET) {
    log_debug("socks: Packet is too small.");
    return SOCKS_INCOMPLETE;
  }

  /* ST_SENT_REPLY connections shouldn't be here! */
  obfs_assert(socks_state->state != ST_SENT_REPLY &&
         socks_state->state != ST_HAVE_ADDR);

  if (socks_state->version == 0) {
    /* First byte of all SOCKS data is the version field. */
    evbuffer_remove(source, &socks_state->version, 1);
    if (socks_state->version != SOCKS5_VERSION &&
        socks_state->version != SOCKS4_VERSION) {
      log_debug("socks: unexpected version %d", (int)socks_state->version);
      goto broken;
    }
    log_debug("Got version %d",(int)socks_state->version);
  }

  switch(socks_state->version) {
  case SOCKS4_VERSION:
    if (socks_state->state == ST_WAITING) {
      r = socks4_read_request(source, socks_state);
      if (r == SOCKS_BROKEN)
        goto broken;
      else if (r == SOCKS_INCOMPLETE)
        return SOCKS_INCOMPLETE;
      socks_state->state = ST_HAVE_ADDR;
      return SOCKS_GOOD;
    }
    break;
  case SOCKS5_VERSION:
    if (socks_state->state == ST_WAITING) {
      /* We don't know this connection. We have to do method negotiation. */
      r = socks5_handle_negotiation(source,dest,socks_state);
      if (r == SOCKS_BROKEN)
        goto broken;
      else if (r == SOCKS_INCOMPLETE)
        return SOCKS_INCOMPLETE;
      else if (r == SOCKS_GOOD)
        socks_state->state = ST_NEGOTIATION_DONE;
    }
    if (socks_state->state == ST_NEGOTIATION_DONE) {
      /* We know this connection. Let's see what it wants. */
      r = socks5_handle_request(source,&socks_state->parsereq);
      if (r == SOCKS_GOOD) {
        socks_state->state = ST_HAVE_ADDR;
        return SOCKS_GOOD;
      } else if (r == SOCKS_INCOMPLETE)
        return SOCKS_INCOMPLETE;
      else if (r == SOCKS_CMD_NOT_CONNECT) {
        socks_state->broken = 1;
        return SOCKS_CMD_NOT_CONNECT;
      } else if (r == SOCKS_BROKEN)
        goto broken;
      obfs_abort();
    }
    break;
  default:
    goto broken;
  }

  return SOCKS_INCOMPLETE;
 broken:
  socks_state->broken = 1;
  return SOCKS_BROKEN;
}

/**
   Returns the protocol status of the SOCKS state 'state'.
*/
enum socks_status_t
socks_state_get_status(const socks_state_t *state)
{
  return state->state;
}

/**
   If we have previously parsed a SOCKS CONNECT request, this function
   places the corresponding address/port to 'af_out', 'addr_out' and
   'port_out'.

   It returns 0 on success and -1 if it was called unnecessarily.
*/
int
socks_state_get_address(const socks_state_t *state,
                        int *af_out,
                        const char **addr_out,
                        uint16_t *port_out)
{
  if (state->state != ST_HAVE_ADDR && state->state != ST_SENT_REPLY)
    return -1;
  *af_out = state->parsereq.af;
  *addr_out = (char*) state->parsereq.addr;
  *port_out = state->parsereq.port;
  return 0;
}

/**
   Places the address/port in 'sa' into the SOCKS state 'state' for
   later retrieval.
*/
int
socks_state_set_address(socks_state_t *state, const struct sockaddr *sa)
{
  uint16_t port;
  if (sa->sa_family == AF_INET) {
    const struct sockaddr_in *sin = (const struct sockaddr_in *)sa;
    port = sin->sin_port;
    if (evutil_inet_ntop(AF_INET, &sin->sin_addr, state->parsereq.addr,
                         sizeof(state->parsereq.addr)) == NULL)
      return -1;
  } else if (sa->sa_family == AF_INET6) {
    if (state->version == 4) {
      log_debug("Oops; socks4 doesn't allow ipv6 addresses");
      return -1;
    }
    const struct sockaddr_in6 *sin6 = (const struct sockaddr_in6 *)sa;
    port = sin6->sin6_port;
    if (evutil_inet_ntop(AF_INET6, &sin6->sin6_addr, state->parsereq.addr,
                         sizeof(state->parsereq.addr)) == NULL)
      return -1;
  } else {
    log_debug("Unknown address family %d", sa->sa_family);
    return -1;
  }

  state->parsereq.port = ntohs(port);
  state->parsereq.af = sa->sa_family;
  return 0;
}

/**
   This function sends a SOCKS{5,4} "Server Reply" to 'dest'.

   'error' is 0 if no errors were encountered during the SOCKS
   operation (normally a CONNECT with no errors means that the
   connect() was successful).
   If 'error' is not 0, it means that an error was encountered and
   error carries the errno(3).
*/
void
socks_send_reply(socks_state_t *state, struct evbuffer *dest, int error)
{
  int status = socks_errno_to_reply(state, error);
  if (state->version == SOCKS5_VERSION)
    socks5_send_reply(dest, state, status);
  else if (state->version == SOCKS4_VERSION)
    socks4_send_reply(dest, state, status);
}
