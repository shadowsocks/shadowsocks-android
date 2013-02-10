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
#include "shrpx_ssl.h"

#include <sys/socket.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <pthread.h>

#include <vector>
#include <string>

#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <event2/bufferevent.h>
#include <event2/bufferevent_ssl.h>

#include <spdylay/spdylay.h>

#include "shrpx_log.h"
#include "shrpx_client_handler.h"
#include "shrpx_config.h"
#include "shrpx_accesslog.h"
#include "util.h"

using namespace spdylay;

namespace shrpx {

namespace ssl {

namespace {
std::pair<unsigned char*, size_t> next_proto;
unsigned char proto_list[23];
} // namespace

namespace {
int next_proto_cb(SSL *s, const unsigned char **data, unsigned int *len,
                  void *arg)
{
  std::pair<unsigned char*, size_t> *next_proto =
    reinterpret_cast<std::pair<unsigned char*, size_t>* >(arg);
  *data = next_proto->first;
  *len = next_proto->second;
  return SSL_TLSEXT_ERR_OK;
}
} // namespace

namespace {
int verify_callback(int preverify_ok, X509_STORE_CTX *ctx)
{
  // We don't verify the client certificate. Just request it for the
  // testing purpose.
  return 1;
}
} // namespace

namespace {
void set_npn_prefs(unsigned char *out, const char **protos, size_t len)
{
  unsigned char *ptr = out;
  for(size_t i = 0; i < len; ++i) {
    *ptr = strlen(protos[i]);
    memcpy(ptr+1, protos[i], *ptr);
    ptr += *ptr+1;
  }
}
} // namespace

namespace {
int ssl_pem_passwd_cb(char *buf, int size, int rwflag, void *user_data)
{
  Config *config = (Config *)user_data;
  int len = (int)strlen(config->private_key_passwd);
  if (size < len + 1) {
    LOG(ERROR) << "ssl_pem_passwd_cb: buf is too small " << size;
    return 0;
  }
  // Copy string including last '\0'.
  memcpy(buf, config->private_key_passwd, len+1);
  return len;
}
} // namespace

namespace {
int servername_callback(SSL *ssl, int *al, void *arg)
{
  if(get_config()->cert_tree) {
    const char *hostname = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
    if(hostname) {
      SSL_CTX *ssl_ctx = cert_lookup_tree_lookup(get_config()->cert_tree,
                                                 hostname, strlen(hostname));
      if(ssl_ctx) {
        SSL_set_SSL_CTX(ssl, ssl_ctx);
      }
    }
  }
  return SSL_TLSEXT_ERR_NOACK;
}
} // namespace

SSL_CTX* create_ssl_context(const char *private_key_file,
                            const char *cert_file)
{
  SSL_CTX *ssl_ctx;
  ssl_ctx = SSL_CTX_new(SSLv23_server_method());
  if(!ssl_ctx) {
    LOG(FATAL) << ERR_error_string(ERR_get_error(), 0);
    DIE();
  }
  SSL_CTX_set_options(ssl_ctx,
                      SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_COMPRESSION |
                      SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);

  const unsigned char sid_ctx[] = "shrpx";
  SSL_CTX_set_session_id_context(ssl_ctx, sid_ctx, sizeof(sid_ctx)-1);
  SSL_CTX_set_session_cache_mode(ssl_ctx, SSL_SESS_CACHE_SERVER);

  if(get_config()->ciphers) {
    if(SSL_CTX_set_cipher_list(ssl_ctx, get_config()->ciphers) == 0) {
      LOG(FATAL) << "SSL_CTX_set_cipher_list failed: "
                 << ERR_error_string(ERR_get_error(), NULL);
      DIE();
    }
  }

  SSL_CTX_set_mode(ssl_ctx, SSL_MODE_ENABLE_PARTIAL_WRITE);
  SSL_CTX_set_mode(ssl_ctx, SSL_MODE_AUTO_RETRY);
  SSL_CTX_set_mode(ssl_ctx, SSL_MODE_RELEASE_BUFFERS);
  if (get_config()->private_key_passwd) {
    SSL_CTX_set_default_passwd_cb(ssl_ctx, ssl_pem_passwd_cb);
    SSL_CTX_set_default_passwd_cb_userdata(ssl_ctx, (void *)get_config());
  }
  if(SSL_CTX_use_PrivateKey_file(ssl_ctx, private_key_file,
                                 SSL_FILETYPE_PEM) != 1) {
    LOG(FATAL) << "SSL_CTX_use_PrivateKey_file failed: "
               << ERR_error_string(ERR_get_error(), NULL);
    DIE();
  }
  if(SSL_CTX_use_certificate_chain_file(ssl_ctx, cert_file) != 1) {
    LOG(FATAL) << "SSL_CTX_use_certificate_file failed: "
               << ERR_error_string(ERR_get_error(), NULL);
    DIE();
  }
  if(SSL_CTX_check_private_key(ssl_ctx) != 1) {
    LOG(FATAL) << "SSL_CTX_check_private_key failed: "
               << ERR_error_string(ERR_get_error(), NULL);
    DIE();
  }
  if(get_config()->verify_client) {
    SSL_CTX_set_verify(ssl_ctx,
                       SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE |
                       SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                       verify_callback);
  }
  SSL_CTX_set_tlsext_servername_callback(ssl_ctx, servername_callback);

  // We speak "http/1.1", "spdy/2" and "spdy/3".
  const char *protos[] = { "spdy/3", "spdy/2", "http/1.1" };
  set_npn_prefs(proto_list, protos, 3);

  next_proto.first = proto_list;
  next_proto.second = sizeof(proto_list);
  SSL_CTX_set_next_protos_advertised_cb(ssl_ctx, next_proto_cb, &next_proto);
  return ssl_ctx;
}

namespace {
int select_next_proto_cb(SSL* ssl,
                         unsigned char **out, unsigned char *outlen,
                         const unsigned char *in, unsigned int inlen,
                         void *arg)
{
  if(spdylay_select_next_protocol(out, outlen, in, inlen) <= 0) {
    *out = (unsigned char*)"spdy/3";
    *outlen = 6;
  }
  return SSL_TLSEXT_ERR_OK;
}
} // namespace

SSL_CTX* create_ssl_client_context()
{
  SSL_CTX *ssl_ctx;
  ssl_ctx = SSL_CTX_new(SSLv23_client_method());
  if(!ssl_ctx) {
    LOG(FATAL) << ERR_error_string(ERR_get_error(), 0);
    DIE();
  }
  SSL_CTX_set_options(ssl_ctx,
                      SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_COMPRESSION |
                      SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);

  if(get_config()->ciphers) {
    if(SSL_CTX_set_cipher_list(ssl_ctx, get_config()->ciphers) == 0) {
      LOG(FATAL) << "SSL_CTX_set_cipher_list failed: "
                 << ERR_error_string(ERR_get_error(), NULL);
      DIE();
    }
  }

  SSL_CTX_set_mode(ssl_ctx, SSL_MODE_ENABLE_PARTIAL_WRITE);
  SSL_CTX_set_mode(ssl_ctx, SSL_MODE_AUTO_RETRY);
  SSL_CTX_set_mode(ssl_ctx, SSL_MODE_RELEASE_BUFFERS);

  if(SSL_CTX_set_default_verify_paths(ssl_ctx) != 1) {
    LOG(WARNING) << "Could not load system trusted ca certificates: "
                 << ERR_error_string(ERR_get_error(), NULL);
  }

  if(get_config()->cacert) {
    if(SSL_CTX_load_verify_locations(ssl_ctx, get_config()->cacert, 0) != 1) {
      LOG(FATAL) << "Could not load trusted ca certificates from "
                 << get_config()->cacert << ": "
                 << ERR_error_string(ERR_get_error(), NULL);
      DIE();
    }
  }

  SSL_CTX_set_next_proto_select_cb(ssl_ctx, select_next_proto_cb, 0);
  return ssl_ctx;
}

ClientHandler* accept_connection(event_base *evbase, SSL_CTX *ssl_ctx,
                                 evutil_socket_t fd,
                                 sockaddr *addr, int addrlen)
{
  char host[NI_MAXHOST];
  int rv;
  rv = getnameinfo(addr, addrlen, host, sizeof(host), 0, 0, NI_NUMERICHOST);
  if(rv == 0) {
    if(get_config()->accesslog) {
      upstream_connect(host);
    }

    int val = 1;
    rv = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
                    reinterpret_cast<char *>(&val), sizeof(val));
    if(rv == -1) {
      LOG(WARNING) << "Setting option TCP_NODELAY failed: "
                   << strerror(errno);
    }
    SSL *ssl = 0;
    bufferevent *bev;
    if(ssl_ctx) {
      ssl = SSL_new(ssl_ctx);
      if(!ssl) {
        LOG(ERROR) << "SSL_new() failed: "
                   << ERR_error_string(ERR_get_error(), NULL);
        return 0;
      }
      bev = bufferevent_openssl_socket_new
        (evbase, fd, ssl,
         BUFFEREVENT_SSL_ACCEPTING, BEV_OPT_DEFER_CALLBACKS);
    } else {
      bev = bufferevent_socket_new(evbase, fd, BEV_OPT_DEFER_CALLBACKS);
    }
    ClientHandler *client_handler = new ClientHandler(bev, fd, ssl, host);
    return client_handler;
  } else {
    LOG(ERROR) << "getnameinfo() failed: " << gai_strerror(rv);
    return 0;
  }
}

bool numeric_host(const char *hostname)
{
  struct addrinfo hints;
  struct addrinfo* res;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_flags = AI_NUMERICHOST;
  if(getaddrinfo(hostname, 0, &hints, &res)) {
    return false;
  }
  freeaddrinfo(res);
  return true;
}

namespace {
bool tls_hostname_match(const char *pattern, const char *hostname)
{
  const char *ptWildcard = strchr(pattern, '*');
  if(ptWildcard == 0) {
    return util::strieq(pattern, hostname);
  }
  const char *ptLeftLabelEnd = strchr(pattern, '.');
  bool wildcardEnabled = true;
  // Do case-insensitive match. At least 2 dots are required to enable
  // wildcard match. Also wildcard must be in the left-most label.
  // Don't attempt to match a presented identifier where the wildcard
  // character is embedded within an A-label.
  if(ptLeftLabelEnd == 0 || strchr(ptLeftLabelEnd+1, '.') == 0 ||
     ptLeftLabelEnd < ptWildcard || util::istartsWith(pattern, "xn--")) {
    wildcardEnabled = false;
  }
  if(!wildcardEnabled) {
    return util::strieq(pattern, hostname);
  }
  const char *hnLeftLabelEnd = strchr(hostname, '.');
  if(hnLeftLabelEnd == 0 || !util::strieq(ptLeftLabelEnd, hnLeftLabelEnd)) {
    return false;
  }
  // Perform wildcard match. Here '*' must match at least one
  // character.
  if(hnLeftLabelEnd - hostname < ptLeftLabelEnd - pattern) {
    return false;
  }
  return util::istartsWith(hostname, hnLeftLabelEnd, pattern, ptWildcard) &&
    util::iendsWith(hostname, hnLeftLabelEnd, ptWildcard+1, ptLeftLabelEnd);
}
} // namespace

namespace {
int verify_hostname(const char *hostname,
                    const sockaddr_union *su,
                    size_t salen,
                    const std::vector<std::string>& dns_names,
                    const std::vector<std::string>& ip_addrs,
                    const std::string& common_name)
{
  if(numeric_host(hostname)) {
    if(ip_addrs.empty()) {
      return util::strieq(common_name.c_str(), hostname) ? 0 : -1;
    }
    const void *saddr;
    switch(su->storage.ss_family) {
    case AF_INET:
      saddr = &su->in.sin_addr;
      break;
    case AF_INET6:
      saddr = &su->in6.sin6_addr;
      break;
    default:
      return -1;
    }
    for(size_t i = 0; i < ip_addrs.size(); ++i) {
      if(salen == ip_addrs[i].size() &&
         memcmp(saddr, ip_addrs[i].c_str(), salen) == 0) {
        return 0;
      }
    }
  } else {
    if(dns_names.empty()) {
      return tls_hostname_match(common_name.c_str(), hostname) ? 0 : -1;
    }
    for(size_t i = 0; i < dns_names.size(); ++i) {
      if(tls_hostname_match(dns_names[i].c_str(), hostname)) {
        return 0;
      }
    }
  }
  return -1;
}
} // namespace

void get_altnames(X509 *cert,
                  std::vector<std::string>& dns_names,
                  std::vector<std::string>& ip_addrs,
                  std::string& common_name)
{
  GENERAL_NAMES* altnames;
  altnames = reinterpret_cast<GENERAL_NAMES*>
    (X509_get_ext_d2i(cert, NID_subject_alt_name, 0, 0));
  if(altnames) {
    util::auto_delete<GENERAL_NAMES*> altnames_deleter(altnames,
                                                       GENERAL_NAMES_free);
    size_t n = sk_GENERAL_NAME_num(altnames);
    for(size_t i = 0; i < n; ++i) {
      const GENERAL_NAME *altname = sk_GENERAL_NAME_value(altnames, i);
      if(altname->type == GEN_DNS) {
        const char *name;
        name = reinterpret_cast<char*>(ASN1_STRING_data(altname->d.ia5));
        if(!name) {
          continue;
        }
        size_t len = ASN1_STRING_length(altname->d.ia5);
        if(std::find(name, name+len, '\0') != name+len) {
          // Embedded NULL is not permitted.
          continue;
        }
        dns_names.push_back(std::string(name, len));
      } else if(altname->type == GEN_IPADD) {
        const unsigned char *ip_addr = altname->d.iPAddress->data;
        if(!ip_addr) {
          continue;
        }
        size_t len = altname->d.iPAddress->length;
        ip_addrs.push_back(std::string(reinterpret_cast<const char*>(ip_addr),
                                      len));
      }
    }
  }
  X509_NAME *subjectname = X509_get_subject_name(cert);
  if(!subjectname) {
    LOG(WARNING) << "Could not get X509 name object from the certificate.";
    return;
  }
  int lastpos = -1;
  while(1) {
    lastpos = X509_NAME_get_index_by_NID(subjectname, NID_commonName,
                                         lastpos);
    if(lastpos == -1) {
      break;
    }
    X509_NAME_ENTRY *entry = X509_NAME_get_entry(subjectname, lastpos);
    unsigned char *out;
    int outlen = ASN1_STRING_to_UTF8(&out, X509_NAME_ENTRY_get_data(entry));
    if(outlen < 0) {
      continue;
    }
    if(std::find(out, out+outlen, '\0') != out+outlen) {
      // Embedded NULL is not permitted.
      continue;
    }
    common_name.assign(&out[0], &out[outlen]);
    OPENSSL_free(out);
    break;
  }
}

int check_cert(SSL *ssl)
{
  X509 *cert = SSL_get_peer_certificate(ssl);
  if(!cert) {
    LOG(ERROR) << "No certificate found";
    return -1;
  }
  util::auto_delete<X509*> cert_deleter(cert, X509_free);
  long verify_res = SSL_get_verify_result(ssl);
  if(verify_res != X509_V_OK) {
    LOG(ERROR) << "Certificate verification failed: "
               << X509_verify_cert_error_string(verify_res);
    return -1;
  }
  std::string common_name;
  std::vector<std::string> dns_names;
  std::vector<std::string> ip_addrs;
  get_altnames(cert, dns_names, ip_addrs, common_name);
  if(verify_hostname(get_config()->downstream_host,
                     &get_config()->downstream_addr,
                     get_config()->downstream_addrlen,
                     dns_names, ip_addrs, common_name) != 0) {
    LOG(ERROR) << "Certificate verification failed: hostname does not match";
    return -1;
  }
  return 0;
}

namespace {
pthread_mutex_t *ssl_locks;
} // namespace

namespace {
void ssl_locking_cb(int mode, int type, const char *file, int line)
{
  if(mode & CRYPTO_LOCK) {
    pthread_mutex_lock(&(ssl_locks[type]));
  } else {
    pthread_mutex_unlock(&(ssl_locks[type]));
  }
}
} // namespace

void setup_ssl_lock()
{
  ssl_locks = new pthread_mutex_t[CRYPTO_num_locks()];
  for(int i = 0; i < CRYPTO_num_locks(); ++i) {
    // Always returns 0
    pthread_mutex_init(&(ssl_locks[i]), 0);
  }
  //CRYPTO_set_id_callback(ssl_thread_id); OpenSSL manual says that if
  // threadid_func is not specified using
  // CRYPTO_THREADID_set_callback(), then default implementation is
  // used. We use this default one.
  CRYPTO_set_locking_callback(ssl_locking_cb);
}

void teardown_ssl_lock()
{
  for(int i = 0; i < CRYPTO_num_locks(); ++i) {
    pthread_mutex_destroy(&(ssl_locks[i]));
  }
  delete [] ssl_locks;
}

CertLookupTree* cert_lookup_tree_new()
{
  CertLookupTree *tree = new CertLookupTree();
  CertNode *root = new CertNode();
  root->ssl_ctx = 0;
  root->c = 0;
  tree->root = root;
  return tree;
}

namespace {
void cert_node_del(CertNode *node)
{
  for(std::vector<CertNode*>::iterator i = node->next.begin(),
        eoi = node->next.end(); i != eoi; ++i) {
    cert_node_del(*i);
  }
  for(std::vector<std::pair<char*, SSL_CTX*> >::iterator i =
        node->wildcard_certs.begin(), eoi = node->wildcard_certs.end();
      i != eoi; ++i) {
    delete [] (*i).first;
  }
  delete node;
}
} // namespace

void cert_lookup_tree_del(CertLookupTree *lt)
{
  cert_node_del(lt->root);
  delete lt;
}

namespace {
// The |offset| is the index in the hostname we are examining.
void cert_lookup_tree_add_cert(CertLookupTree *lt, CertNode *node,
                               SSL_CTX *ssl_ctx,
                               const char *hostname, size_t len, int offset)
{
  if(offset == -1) {
    if(!node->ssl_ctx) {
      node->ssl_ctx = ssl_ctx;
    }
    return;
  }
  int i, next_len = node->next.size();
  char c = util::lowcase(hostname[offset]);
  for(i = 0; i < next_len; ++i) {
    if(node->next[i]->c == c) {
      break;
    }
  }
  if(i == next_len) {
    CertNode *parent = node;
    int j;
    for(j = offset; j >= 0; --j) {
      if(hostname[j] == '*') {
        // We assume hostname as wildcard hostname when first '*' is
        // encountered. Note that as per RFC 6125 (6.4.3), there are
        // some restrictions for wildcard hostname. We just ignore
        // these rules here but do the proper check when we do the
        // match.
        char *hostcopy = strdup(hostname);
        for(int k = 0; hostcopy[k]; ++k) {
          hostcopy[k] = util::lowcase(hostcopy[k]);
        }
        parent->wildcard_certs.push_back(std::make_pair(hostcopy, ssl_ctx));
        break;
      }
      CertNode *new_node = new CertNode();
      new_node->ssl_ctx = 0;
      new_node->c = util::lowcase(hostname[j]);
      parent->next.push_back(new_node);
      parent = new_node;
    }
    if(j == -1) {
      // non-wildcard hostname, exact match case.
      parent->ssl_ctx = ssl_ctx;
    }
  } else {
    cert_lookup_tree_add_cert(lt, node->next[i], ssl_ctx,
                              hostname, len, offset-1);
  }
}
} // namespace

void cert_lookup_tree_add_cert(CertLookupTree *lt, SSL_CTX *ssl_ctx,
                               const char *hostname, size_t len)
{
  if(len == 0) {
    return;
  }
  cert_lookup_tree_add_cert(lt, lt->root, ssl_ctx, hostname, len, len-1);
}

namespace {

SSL_CTX* cert_lookup_tree_lookup(CertLookupTree *lt, CertNode *node,
                                 const char *hostname, size_t len, int offset)
{
  if(offset == -1) {
    if(node->ssl_ctx) {
      return node->ssl_ctx;
    } else {
      // Do not perform wildcard-match because '*' must match at least
      // one character.
      return 0;
    }
  }
  for(std::vector<std::pair<char*, SSL_CTX*> >::iterator i =
        node->wildcard_certs.begin(), eoi = node->wildcard_certs.end();
      i != eoi; ++i) {
    if(tls_hostname_match((*i).first, hostname)) {
      return (*i).second;
    }
  }
  char c = util::lowcase(hostname[offset]);
  for(std::vector<CertNode*>::iterator i = node->next.begin(),
        eoi = node->next.end(); i != eoi; ++i) {
    if((*i)->c == c) {
      return cert_lookup_tree_lookup(lt, *i, hostname, len, offset-1);
    }
  }
  return 0;
}
} // namespace

SSL_CTX* cert_lookup_tree_lookup(CertLookupTree *lt,
                                 const char *hostname, size_t len)
{
  return cert_lookup_tree_lookup(lt, lt->root, hostname, len, len-1);
}


int cert_lookup_tree_add_cert_from_file(CertLookupTree *lt, SSL_CTX *ssl_ctx,
                                        const char *certfile)
{
  BIO *bio = BIO_new(BIO_s_file());
  if(!bio) {
    LOG(ERROR) << "BIO_new failed";
    return -1;
  }
  util::auto_delete<BIO*> bio_deleter(bio, BIO_vfree);
  if(!BIO_read_filename(bio, certfile)) {
    LOG(ERROR) << "Could not read certificate file '" << certfile << "'";
    return -1;
  }
  X509 *cert = PEM_read_bio_X509(bio, 0, 0, 0);
  if(!cert) {
    LOG(ERROR) << "Could not read X509 structure from file '"
               << certfile << "'";
    return -1;
  }
  util::auto_delete<X509*> cert_deleter(cert, X509_free);
  std::string common_name;
  std::vector<std::string> dns_names;
  std::vector<std::string> ip_addrs;
  get_altnames(cert, dns_names, ip_addrs, common_name);
  for(std::vector<std::string>::iterator i = dns_names.begin(),
        eoi = dns_names.end(); i != eoi; ++i) {
    cert_lookup_tree_add_cert(lt, ssl_ctx, (*i).c_str(), (*i).size());
  }
  cert_lookup_tree_add_cert(lt, ssl_ctx, common_name.c_str(),
                            common_name.size());
  return 0;
}

} // namespace ssl

} // namespace shrpx
