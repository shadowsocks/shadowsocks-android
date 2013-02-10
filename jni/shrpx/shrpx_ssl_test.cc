/*
 * Spdylay - SPDY Library
 *
 * Copyright (c) 2013 Tatsuhiro Tsujikawa
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
#include "shrpx_ssl_test.h"

#include <CUnit/CUnit.h>

#include "shrpx_ssl.h"

namespace shrpx {

void test_shrpx_ssl_create_lookup_tree(void)
{
  ssl::CertLookupTree* tree = ssl::cert_lookup_tree_new();
  SSL_CTX *ctxs[] = {SSL_CTX_new(TLSv1_method()),
                     SSL_CTX_new(TLSv1_method()),
                     SSL_CTX_new(TLSv1_method()),
                     SSL_CTX_new(TLSv1_method()),
                     SSL_CTX_new(TLSv1_method())};
  const char *hostnames[] = { "example.com",
                              "www.example.org",
                              "*www.example.org",
                              "x*.host.domain",
                              "*yy.host.domain"};
  int num = sizeof(ctxs)/sizeof(ctxs[0]);
  for(int i = 0; i < num; ++i) {
    ssl::cert_lookup_tree_add_cert(tree, ctxs[i], hostnames[i],
                                   strlen(hostnames[i]));
  }

  CU_ASSERT(ctxs[0] == ssl::cert_lookup_tree_lookup(tree, hostnames[0],
                                                    strlen(hostnames[0])));
  CU_ASSERT(ctxs[1] == ssl::cert_lookup_tree_lookup(tree, hostnames[1],
                                                    strlen(hostnames[1])));
  const char h1[] = "2www.example.org";
  CU_ASSERT(ctxs[2] == ssl::cert_lookup_tree_lookup(tree, h1, strlen(h1)));
  const char h2[] = "www2.example.org";
  CU_ASSERT(0 == ssl::cert_lookup_tree_lookup(tree, h2, strlen(h2)));
  const char h3[] = "x1.host.domain";
  CU_ASSERT(ctxs[3] == ssl::cert_lookup_tree_lookup(tree, h3, strlen(h3)));
  // Does not match *yy.host.domain, because * must match at least 1
  // character.
  const char h4[] = "yy.host.domain";
  CU_ASSERT(0 == ssl::cert_lookup_tree_lookup(tree, h4, strlen(h4)));
  const char h5[] = "zyy.host.domain";
  CU_ASSERT(ctxs[4] == ssl::cert_lookup_tree_lookup(tree, h5, strlen(h5)));
  CU_ASSERT(0 == ssl::cert_lookup_tree_lookup(tree, "", 0));

  ssl::cert_lookup_tree_del(tree);
  for(int i = 0; i < num; ++i) {
    SSL_CTX_free(ctxs[i]);
  }
}

void test_shrpx_ssl_cert_lookup_tree_add_cert_from_file(void)
{
  int rv;
  ssl::CertLookupTree* tree = ssl::cert_lookup_tree_new();
  SSL_CTX *ssl_ctx = SSL_CTX_new(TLSv1_method());
  const char certfile[] = SPDYLAY_TESTS_DIR"/testdata/cacert.pem";
  rv = ssl::cert_lookup_tree_add_cert_from_file(tree, ssl_ctx, certfile);
  CU_ASSERT(0 == rv);
  const char localhost[] = "localhost";
  CU_ASSERT(ssl_ctx == ssl::cert_lookup_tree_lookup(tree, localhost,
                                                    sizeof(localhost)-1));
  ssl::cert_lookup_tree_del(tree);
  SSL_CTX_free(ssl_ctx);
}

} // namespace shrpx
