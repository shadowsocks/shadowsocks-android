/*
 * Spdylay - SPDY Library
 *
 * Copyright (c) 2012 Tatsuhiro Tsujikawa
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef SHRPX_CONFIG_H
#define SHRPX_CONFIG_H

#include "shrpx.h"

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <vector>
#include <string>

#include <openssl/ssl.h>

namespace shrpx {

namespace ssl {

struct CertLookupTree;

} // namespace ssl

extern const char SHRPX_OPT_PRIVATE_KEY_FILE[];
extern const char SHRPX_OPT_PRIVATE_KEY_PASSWD_FILE[];
extern const char SHRPX_OPT_CERTIFICATE_FILE[];
extern const char SHRPX_OPT_SUBCERT[];
extern const char SHRPX_OPT_BACKEND[];
extern const char SHRPX_OPT_FRONTEND[];
extern const char SHRPX_OPT_WORKERS[];
extern const char SHRPX_OPT_SPDY_MAX_CONCURRENT_STREAMS[];
extern const char SHRPX_OPT_LOG_LEVEL[];
extern const char SHRPX_OPT_DAEMON[];
extern const char SHRPX_OPT_SPDY_PROXY[];
extern const char SHRPX_OPT_SPDY_BRIDGE[];
extern const char SHRPX_OPT_CLIENT_PROXY[];
extern const char SHRPX_OPT_ADD_X_FORWARDED_FOR[];
extern const char SHRPX_OPT_NO_VIA[];
extern const char SHRPX_OPT_FRONTEND_SPDY_READ_TIMEOUT[];
extern const char SHRPX_OPT_FRONTEND_READ_TIMEOUT[];
extern const char SHRPX_OPT_FRONTEND_WRITE_TIMEOUT[];
extern const char SHRPX_OPT_BACKEND_READ_TIMEOUT[];
extern const char SHRPX_OPT_BACKEND_WRITE_TIMEOUT[];
extern const char SHRPX_OPT_ACCESSLOG[];
extern const char SHRPX_OPT_BACKEND_KEEP_ALIVE_TIMEOUT[];
extern const char SHRPX_OPT_FRONTEND_SPDY_WINDOW_BITS[];
extern const char SHRPX_OPT_BACKEND_SPDY_WINDOW_BITS[];
extern const char SHRPX_OPT_PID_FILE[];
extern const char SHRPX_OPT_USER[];
extern const char SHRPX_OPT_SYSLOG[];
extern const char SHRPX_OPT_SYSLOG_FACILITY[];
extern const char SHRPX_OPT_BACKLOG[];
extern const char SHRPX_OPT_CIPHERS[];
extern const char SHRPX_OPT_CLIENT[];
extern const char SHRPX_OPT_INSECURE[];
extern const char SHRPX_OPT_CACERT[];
extern const char SHRPX_OPT_BACKEND_IPV4[];
extern const char SHRPX_OPT_BACKEND_IPV6[];
extern const char SHRPX_OPT_BACKEND_HTTP_PROXY_URI[];

union sockaddr_union {
  sockaddr sa;
  sockaddr_storage storage;
  sockaddr_in6 in6;
  sockaddr_in in;
};

struct Config {
  bool verbose;
  bool daemon;
  char *host;
  uint16_t port;
  char *private_key_file;
  char *private_key_passwd;
  char *cert_file;
  SSL_CTX *default_ssl_ctx;
  ssl::CertLookupTree *cert_tree;
  bool verify_client;
  const char *server_name;
  char *downstream_host;
  uint16_t downstream_port;
  char *downstream_hostport;
  sockaddr_union downstream_addr;
  size_t downstream_addrlen;
  timeval spdy_upstream_read_timeout;
  timeval upstream_read_timeout;
  timeval upstream_write_timeout;
  timeval downstream_read_timeout;
  timeval downstream_write_timeout;
  timeval downstream_idle_read_timeout;
  size_t num_worker;
  size_t spdy_max_concurrent_streams;
  bool spdy_proxy;
  bool spdy_bridge;
  bool client_proxy;
  bool add_x_forwarded_for;
  bool no_via;
  bool accesslog;
  size_t spdy_upstream_window_bits;
  size_t spdy_downstream_window_bits;
  char *pid_file;
  uid_t uid;
  gid_t gid;
  char *conf_path;
  bool syslog;
  int syslog_facility;
  // This member finally decides syslog is used or not
  bool use_syslog;
  int backlog;
  char *ciphers;
  bool client;
  // true if --client or --client-proxy are enabled.
  bool client_mode;
  bool insecure;
  char *cacert;
  bool backend_ipv4;
  bool backend_ipv6;
  // true if stderr refers to a terminal.
  bool tty;
  // userinfo in http proxy URI, not percent-encoded form
  char *downstream_http_proxy_userinfo;
  // host in http proxy URI
  char *downstream_http_proxy_host;
  // port in http proxy URI
  uint16_t downstream_http_proxy_port;
  // binary form of http proxy host and port
  sockaddr_union downstream_http_proxy_addr;
  // actual size of downstream_http_proxy_addr
  size_t downstream_http_proxy_addrlen;
};

const Config* get_config();
Config* mod_config();
void create_config();

// Parses option name |opt| and value |optarg|.  The results are
// stored into statically allocated Config object. This function
// returns 0 if it succeeds, or -1.
int parse_config(const char *opt, const char *optarg);

// Loads configurations from |filename| and stores them in statically
// allocated Config object. This function returns 0 if it succeeds, or
// -1.
int load_config(const char *filename);

// Read passwd from |filename|
std::string read_passwd_from_file(const char *filename);

// Copies NULL-terminated string |val| to |*destp|. If |*destp| is not
// NULL, it is freed before copying.
void set_config_str(char **destp, const char *val);

// Returns string for syslog |facility|.
const char* str_syslog_facility(int facility);

// Returns integer value of syslog |facility| string.
int int_syslog_facility(const char *strfacility);

} // namespace shrpx

#endif // SHRPX_CONFIG_H
