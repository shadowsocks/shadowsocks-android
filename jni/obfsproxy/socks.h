/* Copyright 2011 Nick Mathewson, George Kadianakis
   See LICENSE for other credits and copying information
*/

/**
 * \file socks.h
 * \brief Headers for socks.c.
 **/

#ifndef SOCKS_H
#define SOCKS_H

enum socks_status_t {
  /* Waiting for initial socks4 or socks5 message */
  ST_WAITING,
  /* Sent socks5 method negotiation message; waiting for request */
  ST_NEGOTIATION_DONE,
  /* Have received target address */
  ST_HAVE_ADDR,
  /* Have sent reply */
  ST_SENT_REPLY
};

enum socks_ret {
  SOCKS_GOOD=0,
  SOCKS_INCOMPLETE,
  SOCKS_CMD_NOT_CONNECT,
  SOCKS_BROKEN
};

enum socks_ret handle_socks(struct evbuffer *source,
                            struct evbuffer *dest,
                            socks_state_t *socks_state);
socks_state_t *socks_state_new(void); /* cannot fail */
void socks_state_free(socks_state_t *s);

enum socks_status_t socks_state_get_status(const socks_state_t *state);
int socks_state_get_address(const socks_state_t *state,
                            int *af_out,
                            const char **addr_out,
                            uint16_t *port_out);
int socks_state_set_address(socks_state_t *state, const struct sockaddr *sa);
void socks_send_reply(socks_state_t *state, struct evbuffer *dest, int error);
void socks5_send_reply(struct evbuffer *reply_dest,
                       socks_state_t *state, int status);

#define SOCKS5_SUCCESS            0x00
#define SOCKS5_FAILED_GENERAL     0x01
#define SOCKS5_FAILED_NOTALLOWED  0x02
#define SOCKS5_FAILED_NETUNREACH  0x03
#define SOCKS5_FAILED_HOSTUNREACH 0x04
#define SOCKS5_FAILED_REFUSED     0x05
#define SOCKS5_FAILED_TTLEXPIRED  0x06
#define SOCKS5_FAILED_UNSUPPORTED 0x07


#ifdef SOCKS_PRIVATE
#define SOCKS5_VERSION         0x05
#define SOCKS4_VERSION         0x04

#define SOCKS5_METHOD_NOAUTH   0x00
#define SOCKS5_METHOD_FAIL     0xff

#define SOCKS5_CMD_CONNECT     0x01

#define SOCKS5_ATYP_IPV4       0x01
#define SOCKS5_ATYP_FQDN       0x03
#define SOCKS5_ATYP_IPV6       0x04

/* Minimum SOCKS packet length is 3 bytes:
   Method Negotiation Packet with 1 method */
#define MIN_SOCKS_PACKET       3

/* Size of a SOCKS request reply packet.
   Yes, it's static. Yes, it's 10. */
#define SIZEOF_SOCKS5_REQ_REPLY   10

/* Size of the static part of a SOCKS request:
   | version | cmd | rsv | atyp |
       1b       1b   1b     1b
*/
#define SIZEOF_SOCKS5_STATIC_REQ 4

/* In theory, a socks4 request can be arbitrarily long.  We cut off at an
 * arbitrary point, since probably nobody has infinitely long usernames */
#define SOCKS4_MAX_LENGTH 8192

#define SOCKS4_SUCCESS 0x5a
#define SOCKS4_FAILED 0x5b

struct parsereq {
  int af; /* Address family */
  char addr[255+1]; /* Address as string */
  uint16_t port;
};
struct socks_state_t {
  enum socks_status_t state;
  unsigned char version;
  unsigned char broken;
  struct parsereq parsereq;
};

enum socks_ret socks5_handle_negotiation(struct evbuffer *source,
                              struct evbuffer *dest, socks_state_t *state);
enum socks_ret socks5_handle_request(struct evbuffer *source, struct parsereq *parsereq);


enum socks_ret socks4_read_request(struct evbuffer *source,
                                   socks_state_t *state);
void socks4_send_reply(struct evbuffer *dest,
                       socks_state_t *state, int status);

#endif /* SOCKS_PRIVATE */

#endif /* socks.h */
