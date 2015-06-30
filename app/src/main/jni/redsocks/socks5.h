#ifndef SOCKS5_H
#define SOCKS5_H

#include <stdint.h>
#include "utils.h"

typedef struct socks5_method_req_t {
	uint8_t ver;
	uint8_t num_methods;
	uint8_t methods[1]; // at least one
} PACKED socks5_method_req;

typedef struct socks5_method_reply_t {
	uint8_t ver;
	uint8_t method;
} PACKED socks5_method_reply;

static const int socks5_ver = 5;

static const int socks5_auth_none = 0x00;
static const int socks5_auth_gssapi = 0x01;
static const int socks5_auth_password = 0x02;
static const int socks5_auth_invalid = 0xFF;

typedef struct socks5_auth_reply_t {
	uint8_t ver;
	uint8_t status;
} PACKED socks5_auth_reply;

static const int socks5_password_ver = 0x01;
static const int socks5_password_passed = 0x00;


typedef struct socks5_addr_ipv4_t {
	uint32_t addr;
	uint16_t port;
} PACKED socks5_addr_ipv4;

typedef struct socks5_addr_domain_t {
	uint8_t size;
	uint8_t more[1];
	/* uint16_t port; */
} PACKED socks5_addr_domain;

typedef struct socks5_addr_ipv6_t {
	uint8_t addr[16];
	uint16_t port;
} PACKED socks5_addr_ipv6;

typedef struct socks5_req_t {
	uint8_t ver;
	uint8_t cmd;
	uint8_t reserved;
	uint8_t addrtype;
	/* socks5_addr_* */
} PACKED socks5_req;

typedef struct socks5_reply_t {
	uint8_t ver;
	uint8_t status;
	uint8_t reserved;
	uint8_t addrtype;
	/* socks5_addr_* */
} PACKED socks5_reply;

typedef struct socks5_udp_preabmle_t {
	uint16_t reserved;
	uint8_t  frag_no;
	uint8_t  addrtype;   /* 0x01 for IPv4 */
	/* socks5_addr_* */
	socks5_addr_ipv4 ip; /* I support only IPv4 at the moment */
} PACKED socks5_udp_preabmle;

static const int socks5_reply_maxlen = 512; // as domain name can't be longer than 256 bytes
static const int socks5_addrtype_ipv4 = 1;
static const int socks5_addrtype_domain = 3;
static const int socks5_addrtype_ipv6 = 4;
static const int socks5_status_succeeded = 0;
static const int socks5_status_server_failure = 1;
static const int socks5_status_connection_not_allowed_by_ruleset = 2;
static const int socks5_status_Network_unreachable = 3;
static const int socks5_status_Host_unreachable = 4;
static const int socks5_status_Connection_refused = 5;
static const int socks5_status_TTL_expired = 6;
static const int socks5_status_Command_not_supported = 7;
static const int socks5_status_Address_type_not_supported = 8;


const char* socks5_status_to_str(int socks5_status);
int socks5_is_valid_cred(const char *login, const char *password);

struct evbuffer *socks5_mkmethods_plain(int do_password);
struct evbuffer *socks5_mkpassword_plain(const char *login, const char *password);
const char* socks5_is_known_auth_method(socks5_method_reply *reply, int do_password);

static const int socks5_cmd_connect = 1;
static const int socks5_cmd_bind = 2;
static const int socks5_cmd_udp_associate = 3;
struct evbuffer *socks5_mkcommand_plain(int socks5_cmd, const struct sockaddr_in *destaddr);


/* vim:set tabstop=4 softtabstop=4 shiftwidth=4: */
/* vim:set foldmethod=marker foldlevel=32 foldmarker={,}: */
#endif /* SOCKS5_H */
