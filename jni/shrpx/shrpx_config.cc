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
#include "shrpx_config.h"

#include <pwd.h>
#include <netdb.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <cerrno>
#include <limits>
#include <fstream>

#include "shrpx_log.h"
#include "shrpx_ssl.h"
#include "shrpx_http.h"
#include "util.h"

using namespace spdylay;

namespace shrpx {

const char SHRPX_OPT_PRIVATE_KEY_FILE[] = "private-key-file";
const char SHRPX_OPT_PRIVATE_KEY_PASSWD_FILE[] = "private-key-passwd-file";
const char SHRPX_OPT_CERTIFICATE_FILE[] = "certificate-file";
const char SHRPX_OPT_SUBCERT[] = "subcert";

const char SHRPX_OPT_BACKEND[] = "backend";
const char SHRPX_OPT_FRONTEND[] = "frontend";
const char SHRPX_OPT_WORKERS[] = "workers";
const char
SHRPX_OPT_SPDY_MAX_CONCURRENT_STREAMS[] = "spdy-max-concurrent-streams";
const char SHRPX_OPT_LOG_LEVEL[] = "log-level";
const char SHRPX_OPT_DAEMON[] = "daemon";
const char SHRPX_OPT_SPDY_PROXY[] = "spdy-proxy";
const char SHRPX_OPT_SPDY_BRIDGE[] = "spdy-bridge";
const char SHRPX_OPT_CLIENT_PROXY[] = "client-proxy";
const char SHRPX_OPT_ADD_X_FORWARDED_FOR[] = "add-x-forwarded-for";
const char SHRPX_OPT_NO_VIA[] = "no-via";
const char
SHRPX_OPT_FRONTEND_SPDY_READ_TIMEOUT[] = "frontend-spdy-read-timeout";
const char SHRPX_OPT_FRONTEND_READ_TIMEOUT[] = "frontend-read-timeout";
const char SHRPX_OPT_FRONTEND_WRITE_TIMEOUT[] = "frontend-write-timeout";
const char SHRPX_OPT_BACKEND_READ_TIMEOUT[] = "backend-read-timeout";
const char SHRPX_OPT_BACKEND_WRITE_TIMEOUT[] = "backend-write-timeout";
const char SHRPX_OPT_ACCESSLOG[] = "accesslog";
const char
SHRPX_OPT_BACKEND_KEEP_ALIVE_TIMEOUT[] = "backend-keep-alive-timeout";
const char SHRPX_OPT_FRONTEND_SPDY_WINDOW_BITS[] = "frontend-spdy-window-bits";
const char SHRPX_OPT_BACKEND_SPDY_WINDOW_BITS[] = "backend-spdy-window-bits";
const char SHRPX_OPT_PID_FILE[] = "pid-file";
const char SHRPX_OPT_USER[] = "user";
const char SHRPX_OPT_SYSLOG[] = "syslog";
const char SHRPX_OPT_SYSLOG_FACILITY[] = "syslog-facility";
const char SHRPX_OPT_BACKLOG[] = "backlog";
const char SHRPX_OPT_CIPHERS[] = "ciphers";
const char SHRPX_OPT_CLIENT[] = "client";
const char SHRPX_OPT_INSECURE[] = "insecure";
const char SHRPX_OPT_CACERT[] = "cacert";
const char SHRPX_OPT_BACKEND_IPV4[] = "backend-ipv4";
const char SHRPX_OPT_BACKEND_IPV6[] = "backend-ipv6";
const char SHRPX_OPT_BACKEND_HTTP_PROXY_URI[] = "backend-http-proxy-uri";

namespace {
Config *config = 0;
} // namespace

const Config* get_config()
{
  return config;
}

Config* mod_config()
{
  return config;
}

void create_config()
{
  config = new Config();
}

namespace {
int split_host_port(char *host, size_t hostlen, uint16_t *port_ptr,
                    const char *hostport)
{
  // host and port in |hostport| is separated by single ','.
  const char *p = strchr(hostport, ',');
  if(!p) {
    LOG(ERROR) << "Invalid host, port: " << hostport;
    return -1;
  }
  size_t len = p-hostport;
  if(hostlen < len+1) {
    LOG(ERROR) << "Hostname too long: " << hostport;
    return -1;
  }
  memcpy(host, hostport, len);
  host[len] = '\0';

  errno = 0;
  unsigned long d = strtoul(p+1, 0, 10);
  if(errno == 0 && 1 <= d && d <= std::numeric_limits<uint16_t>::max()) {
    *port_ptr = d;
    return 0;
  } else {
    LOG(ERROR) << "Port is invalid: " << p+1;
    return -1;
  }
}
} // namespace

namespace {
bool is_secure(const char *filename)
{
  struct stat buf;
  int rv = stat(filename, &buf);
  if (rv == 0) {
    if ((buf.st_mode & S_IRWXU) &&
        !(buf.st_mode & S_IRWXG) &&
        !(buf.st_mode & S_IRWXO)) {
      return true;
    }
  }

  return false;
}
} // namespace

std::string read_passwd_from_file(const char *filename)
{
  std::string line;

  if (!is_secure(filename)) {
    LOG(ERROR) << "Private key passwd file " << filename
               << " has insecure mode.";
    return line;
  }

  std::ifstream in(filename, std::ios::binary);
  if(!in) {
    LOG(ERROR) << "Could not open key passwd file " << filename;
    return line;
  }

  std::getline(in, line);
  return line;
}

void set_config_str(char **destp, const char *val)
{
  if(*destp) {
    free(*destp);
  }
  *destp = strdup(val);
}

int parse_config(const char *opt, const char *optarg)
{
  char host[NI_MAXHOST];
  uint16_t port;
  if(util::strieq(opt, SHRPX_OPT_BACKEND)) {
    if(split_host_port(host, sizeof(host), &port, optarg) == -1) {
      return -1;
    } else {
      set_config_str(&mod_config()->downstream_host, host);
      mod_config()->downstream_port = port;
    }
  } else if(util::strieq(opt, SHRPX_OPT_FRONTEND)) {
    if(split_host_port(host, sizeof(host), &port, optarg) == -1) {
      return -1;
    } else {
      set_config_str(&mod_config()->host, host);
      mod_config()->port = port;
    }
  } else if(util::strieq(opt, SHRPX_OPT_WORKERS)) {
    mod_config()->num_worker = strtol(optarg, 0, 10);
  } else if(util::strieq(opt, SHRPX_OPT_SPDY_MAX_CONCURRENT_STREAMS)) {
    mod_config()->spdy_max_concurrent_streams = strtol(optarg, 0, 10);
  } else if(util::strieq(opt, SHRPX_OPT_LOG_LEVEL)) {
    if(Log::set_severity_level_by_name(optarg) == -1) {
      LOG(ERROR) << "Invalid severity level: " << optarg;
      return -1;
    }
  } else if(util::strieq(opt, SHRPX_OPT_DAEMON)) {
    mod_config()->daemon = util::strieq(optarg, "yes");
  } else if(util::strieq(opt, SHRPX_OPT_SPDY_PROXY)) {
    mod_config()->spdy_proxy = util::strieq(optarg, "yes");
  } else if(util::strieq(opt, SHRPX_OPT_SPDY_BRIDGE)) {
    mod_config()->spdy_bridge = util::strieq(optarg, "yes");
  } else if(util::strieq(opt, SHRPX_OPT_CLIENT_PROXY)) {
    mod_config()->client_proxy = util::strieq(optarg, "yes");
  } else if(util::strieq(opt, SHRPX_OPT_ADD_X_FORWARDED_FOR)) {
    mod_config()->add_x_forwarded_for = util::strieq(optarg, "yes");
  } else if(util::strieq(opt, SHRPX_OPT_NO_VIA)) {
    mod_config()->no_via = util::strieq(optarg, "yes");
  } else if(util::strieq(opt, SHRPX_OPT_FRONTEND_SPDY_READ_TIMEOUT)) {
    timeval tv = {strtol(optarg, 0, 10), 0};
    mod_config()->spdy_upstream_read_timeout = tv;
  } else if(util::strieq(opt, SHRPX_OPT_FRONTEND_READ_TIMEOUT)) {
    timeval tv = {strtol(optarg, 0, 10), 0};
    mod_config()->upstream_read_timeout = tv;
  } else if(util::strieq(opt, SHRPX_OPT_FRONTEND_WRITE_TIMEOUT)) {
    timeval tv = {strtol(optarg, 0, 10), 0};
    mod_config()->upstream_write_timeout = tv;
  } else if(util::strieq(opt, SHRPX_OPT_BACKEND_READ_TIMEOUT)) {
    timeval tv = {strtol(optarg, 0, 10), 0};
    mod_config()->downstream_read_timeout = tv;
  } else if(util::strieq(opt, SHRPX_OPT_BACKEND_WRITE_TIMEOUT)) {
    timeval tv = {strtol(optarg, 0, 10), 0};
    mod_config()->downstream_write_timeout = tv;
  } else if(util::strieq(opt, SHRPX_OPT_ACCESSLOG)) {
    mod_config()->accesslog = util::strieq(optarg, "yes");
  } else if(util::strieq(opt, SHRPX_OPT_BACKEND_KEEP_ALIVE_TIMEOUT)) {
    timeval tv = {strtol(optarg, 0, 10), 0};
    mod_config()->downstream_idle_read_timeout = tv;
  } else if(util::strieq(opt, SHRPX_OPT_FRONTEND_SPDY_WINDOW_BITS) ||
            util::strieq(opt, SHRPX_OPT_BACKEND_SPDY_WINDOW_BITS)) {
    size_t *resp;
    const char *optname;
    if(util::strieq(opt, SHRPX_OPT_FRONTEND_SPDY_WINDOW_BITS)) {
      resp = &mod_config()->spdy_upstream_window_bits;
      optname = SHRPX_OPT_FRONTEND_SPDY_WINDOW_BITS;
    } else {
      resp = &mod_config()->spdy_downstream_window_bits;
      optname = SHRPX_OPT_BACKEND_SPDY_WINDOW_BITS;
    }
    errno = 0;
    unsigned long int n = strtoul(optarg, 0, 10);
    if(errno == 0 && n < 31) {
      *resp = n;
    } else {
      LOG(ERROR) << "--" << optname
                 << " specify the integer in the range [0, 30], inclusive";
      return -1;
    }
  } else if(util::strieq(opt, SHRPX_OPT_PID_FILE)) {
    set_config_str(&mod_config()->pid_file, optarg);
  } else if(util::strieq(opt, SHRPX_OPT_USER)) {
    passwd *pwd = getpwnam(optarg);
    if(pwd == 0) {
      LOG(ERROR) << "--user: failed to get uid from " << optarg
                 << ": " << strerror(errno);
      return -1;
    }
    mod_config()->uid = pwd->pw_uid;
    mod_config()->gid = pwd->pw_gid;
  } else if(util::strieq(opt, SHRPX_OPT_PRIVATE_KEY_FILE)) {
    set_config_str(&mod_config()->private_key_file, optarg);
  } else if(util::strieq(opt, SHRPX_OPT_PRIVATE_KEY_PASSWD_FILE)) {
    std::string passwd = read_passwd_from_file(optarg);
    if (passwd.empty()) {
      LOG(ERROR) << "Couldn't read key file's passwd from " << optarg;
      return -1;
    }
    set_config_str(&mod_config()->private_key_passwd, passwd.c_str());
  } else if(util::strieq(opt, SHRPX_OPT_CERTIFICATE_FILE)) {
    set_config_str(&mod_config()->cert_file, optarg);
  } else if(util::strieq(opt, SHRPX_OPT_SUBCERT)) {
    // Private Key file and certificate file separated by ':'.
    const char *sp = strchr(optarg, ':');
    if(sp) {
      std::string keyfile(optarg, sp);
      // TODO Do we need private key for subcert?
      SSL_CTX *ssl_ctx = ssl::create_ssl_context(keyfile.c_str(), sp+1);
      if(!get_config()->cert_tree) {
        mod_config()->cert_tree = ssl::cert_lookup_tree_new();
      }
      if(ssl::cert_lookup_tree_add_cert_from_file(get_config()->cert_tree,
                                                  ssl_ctx, sp+1) == -1) {
        return -1;
      }
    }
  } else if(util::strieq(opt, SHRPX_OPT_SYSLOG)) {
    mod_config()->syslog = util::strieq(optarg, "yes");
  } else if(util::strieq(opt, SHRPX_OPT_SYSLOG_FACILITY)) {
    int facility = int_syslog_facility(optarg);
    if(facility == -1) {
      LOG(ERROR) << "Unknown syslog facility: " << optarg;
      return -1;
    }
    mod_config()->syslog_facility = facility;
  } else if(util::strieq(opt, SHRPX_OPT_BACKLOG)) {
    mod_config()->backlog = strtol(optarg, 0, 10);
  } else if(util::strieq(opt, SHRPX_OPT_CIPHERS)) {
    set_config_str(&mod_config()->ciphers, optarg);
  } else if(util::strieq(opt, SHRPX_OPT_CLIENT)) {
    mod_config()->client = util::strieq(optarg, "yes");
  } else if(util::strieq(opt, SHRPX_OPT_INSECURE)) {
    mod_config()->insecure = util::strieq(optarg, "yes");
  } else if(util::strieq(opt, SHRPX_OPT_CACERT)) {
    set_config_str(&mod_config()->cacert, optarg);
  } else if(util::strieq(opt, SHRPX_OPT_BACKEND_IPV4)) {
    mod_config()->backend_ipv4 = util::strieq(optarg, "yes");
  } else if(util::strieq(opt, SHRPX_OPT_BACKEND_IPV6)) {
    mod_config()->backend_ipv6 = util::strieq(optarg, "yes");
  } else if(util::strieq(opt, SHRPX_OPT_BACKEND_HTTP_PROXY_URI)) {
    // parse URI and get hostname, port and optionally userinfo.
    http_parser_url u;
    memset(&u, 0, sizeof(u));
    int rv = http_parser_parse_url(optarg, strlen(optarg), 0, &u);
    if(rv == 0) {
      std::string val;
      if(u.field_set & UF_USERINFO) {
        http::copy_url_component(val, &u, UF_USERINFO, optarg);
        val = util::percentDecode(val.begin(), val.end());
        set_config_str(&mod_config()->downstream_http_proxy_userinfo,
                       val.c_str());
      }
      if(u.field_set & UF_HOST) {
        http::copy_url_component(val, &u, UF_HOST, optarg);
        set_config_str(&mod_config()->downstream_http_proxy_host, val.c_str());
      } else {
        LOG(ERROR) << "backend-http-proxy-uri does not contain hostname";
        return -1;
      }
      if(u.field_set & UF_PORT) {
        mod_config()->downstream_http_proxy_port = u.port;
      } else {
        LOG(ERROR) << "backend-http-proxy-uri does not contain port";
        return -1;
      }
    } else {
      LOG(ERROR) << "Could not parse backend-http-proxy-uri";
        return -1;
    }
  } else if(util::strieq(opt, "conf")) {
    LOG(WARNING) << "conf is ignored";
  } else {
    LOG(ERROR) << "Unknown option: " << opt;
    return -1;
  }
  if(get_config()->cert_file && get_config()->private_key_file) {
    mod_config()->default_ssl_ctx =
      ssl::create_ssl_context(get_config()->private_key_file,
                              get_config()->cert_file);
    if(get_config()->cert_tree) {
      if(ssl::cert_lookup_tree_add_cert_from_file(get_config()->cert_tree,
                                                  get_config()->default_ssl_ctx,
                                                  get_config()->cert_file)
         == -1) {
        return -1;
      }
    }
  }
  return 0;
}

int load_config(const char *filename)
{
  std::ifstream in(filename, std::ios::binary);
  if(!in) {
    LOG(ERROR) << "Could not open config file " << filename;
    return -1;
  }
  std::string line;
  int linenum = 0;
  while(std::getline(in, line)) {
    ++linenum;
    if(line.empty() || line[0] == '#') {
      continue;
    }
    size_t i;
    size_t size = line.size();
    for(i = 0; i < size && line[i] != '='; ++i);
    if(i == size) {
      LOG(ERROR) << "Bad configuration format at line " << linenum;
      return -1;
    }
    line[i] = '\0';
    const char *s = line.c_str();
    if(parse_config(s, s+i+1) == -1) {
      return -1;
    }
  }
  return 0;
}

const char* str_syslog_facility(int facility)
{
  switch(facility) {
  case(LOG_AUTH):
    return "auth";
  case(LOG_AUTHPRIV):
    return "authpriv";
  case(LOG_CRON):
    return "cron";
  case(LOG_DAEMON):
    return "daemon";
  case(LOG_FTP):
    return "ftp";
  case(LOG_KERN):
    return "kern";
  case(LOG_LOCAL0):
    return "local0";
  case(LOG_LOCAL1):
    return "local1";
  case(LOG_LOCAL2):
    return "local2";
  case(LOG_LOCAL3):
    return "local3";
  case(LOG_LOCAL4):
    return "local4";
  case(LOG_LOCAL5):
    return "local5";
  case(LOG_LOCAL6):
    return "local6";
  case(LOG_LOCAL7):
    return "local7";
  case(LOG_LPR):
    return "lpr";
  case(LOG_MAIL):
    return "mail";
  case(LOG_SYSLOG):
    return "syslog";
  case(LOG_USER):
    return "user";
  case(LOG_UUCP):
    return "uucp";
  default:
    return "(unknown)";
  }
}

int int_syslog_facility(const char *strfacility)
{
  if(util::strieq(strfacility, "auth")) {
    return LOG_AUTH;
  } else if(util::strieq(strfacility, "authpriv")) {
    return LOG_AUTHPRIV;
  } else if(util::strieq(strfacility, "cron")) {
    return LOG_CRON;
  } else if(util::strieq(strfacility, "daemon")) {
    return LOG_DAEMON;
  } else if(util::strieq(strfacility, "ftp")) {
    return LOG_FTP;
  } else if(util::strieq(strfacility, "kern")) {
    return LOG_KERN;
  } else if(util::strieq(strfacility, "local0")) {
    return LOG_LOCAL0;
  } else if(util::strieq(strfacility, "local1")) {
    return LOG_LOCAL1;
  } else if(util::strieq(strfacility, "local2")) {
    return LOG_LOCAL2;
  } else if(util::strieq(strfacility, "local3")) {
    return LOG_LOCAL3;
  } else if(util::strieq(strfacility, "local4")) {
    return LOG_LOCAL4;
  } else if(util::strieq(strfacility, "local5")) {
    return LOG_LOCAL5;
  } else if(util::strieq(strfacility, "local6")) {
    return LOG_LOCAL6;
  } else if(util::strieq(strfacility, "local7")) {
    return LOG_LOCAL7;
  } else if(util::strieq(strfacility, "lpr")) {
    return LOG_LPR;
  } else if(util::strieq(strfacility, "mail")) {
    return LOG_MAIL;
  } else if(util::strieq(strfacility, "news")) {
    return LOG_NEWS;
  } else if(util::strieq(strfacility, "syslog")) {
    return LOG_SYSLOG;
  } else if(util::strieq(strfacility, "user")) {
    return LOG_USER;
  } else if(util::strieq(strfacility, "uucp")) {
    return LOG_UUCP;
  } else {
    return -1;
  }
}

} // namespace shrpx
