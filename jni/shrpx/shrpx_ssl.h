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
#ifndef SHRPX_SSL_H
#define SHRPX_SSL_H

#include "shrpx.h"

#include <vector>
#include <string>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <event.h>

namespace shrpx {

class ClientHandler;

namespace ssl {

SSL_CTX* create_ssl_context(const char *private_key_file,
                            const char *cert_file);

SSL_CTX* create_ssl_client_context();

ClientHandler* accept_connection(event_base *evbase, SSL_CTX *ssl_ctx,
                                 evutil_socket_t fd,
                                 sockaddr *addr, int addrlen);

bool numeric_host(const char *hostname);

int check_cert(SSL *ssl);

void setup_ssl_lock();

void teardown_ssl_lock();

// Retrieves DNS and IP address in subjectAltNames and commonName from
// the |cert|.
void get_altnames(X509 *cert,
                  std::vector<std::string>& dns_names,
                  std::vector<std::string>& ip_addrs,
                  std::string& common_name);

// CertLookupTree forms lookup tree to get SSL_CTX whose DNS or
// commonName matches hostname in query. The tree is trie data
// structure form from the tail of the hostname pattern. Each CertNode
// contains one ASCII character in the c member and the next member
// contains the following CertNode pointers ('following' means
// character before the current one). The CertNode where a hostname
// pattern ends contains its SSL_CTX pointer in the ssl_ctx member.
// For wildcard hostname pattern, we store the its pattern and SSL_CTX
// in CertNode one before first "*" found from the tail.
//
// When querying SSL_CTX with particular hostname, we match from its
// tail in our lookup tree. If the query goes to the first character
// of the hostname and current CertNode has non-NULL ssl_ctx member,
// then it is the exact match. The ssl_ctx member is returned.  Along
// the way, if CertNode which contains non-empty wildcard_certs member
// is encountered, wildcard hostname matching is performed against
// them. If there is a match, its SSL_CTX is returned. If none
// matches, query is continued to the next character.

struct CertNode {
  // SSL_CTX for exact match
  SSL_CTX *ssl_ctx;
  // list of wildcard domain name and its SSL_CTX pair, the wildcard
  // '*' appears in this position.
  std::vector<std::pair<char*, SSL_CTX*> > wildcard_certs;
  // ASCII byte in this position
  char c;
  // Next CertNode index of CertLookupTree::nodes
  std::vector<CertNode*> next;
};

struct CertLookupTree {
  std::vector<SSL_CTX*> certs;
  CertNode *root;
};

CertLookupTree* cert_lookup_tree_new();
void cert_lookup_tree_del(CertLookupTree *lt);

// Adds |ssl_ctx| with hostname pattern |hostname| with length |len|
// to the lookup tree |lt|. The |hostname| must be NULL-terminated.
void cert_lookup_tree_add_cert(CertLookupTree *lt, SSL_CTX *ssl_ctx,
                               const char *hostname, size_t len);

// Looks up SSL_CTX using the given |hostname| with length |len|. If
// more than one SSL_CTX which matches the query, it is undefined
// which one is returned. The |hostname| must be NULL-terminated. If
// no matching SSL_CTX found, returns NULL.
SSL_CTX* cert_lookup_tree_lookup(CertLookupTree *lt, const char *hostname,
                                 size_t len);

// Adds |ssl_ctx| to lookup tree |lt| using hostnames read from
// |certfile|. The subjectAltNames and commonName are considered as
// eligible hostname. This function returns 0 if it succeeds, or -1.
// Even if no ssl_ctx is added to tree, this function returns 0.
int cert_lookup_tree_add_cert_from_file(CertLookupTree *lt, SSL_CTX *ssl_ctx,
                                        const char *certfile);

} // namespace ssl

} // namespace shrpx

#endif // SHRPX_SSL_H
