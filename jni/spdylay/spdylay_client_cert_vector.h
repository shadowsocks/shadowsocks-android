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
#ifndef SPDYLAY_CLIENT_CERT_VECTOR_H
#define SPDYLAY_CLIENT_CERT_VECTOR_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <spdylay/spdylay.h>

struct spdylay_origin {
  char scheme[SPDYLAY_MAX_SCHEME+1];
  char host[SPDYLAY_MAX_HOSTNAME+1];
  uint16_t port;
};

typedef struct {
  spdylay_origin **vector;
  /* The size of the vector. */
  size_t size;
  /* The real capacity of the vector. size <= capacity holds true. */
  size_t capacity;
  /* The last slot where origin is stored. The default value is 0. */
  size_t last_slot;
} spdylay_client_cert_vector;

/*
 * Returns nonzero if |lhs| and |rhs| are equal.  The equality is
 * defined such that each member is equal respectively.
 */
int spdylay_origin_equal(const spdylay_origin *lhs, const spdylay_origin *rhs);

/*
 * Convenient function to set members to the |origin|. The |origin|
 * must be allocated prior this call.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * SPDYLAY_ERR_INVALID_ARGUMENT
 *     The |scheme| is longer than SPDYLAY_MAX_SCHEME; or the |host|
 *     is longer than SPDYLAY_MAX_HOSTNAME.
 */
int spdylay_origin_set(spdylay_origin *origin,
                       const char *scheme, const char *host, uint16_t port);

/*
 * Initializes the client certificate vector with the vector size
 * |size|.
 */
int spdylay_client_cert_vector_init(spdylay_client_cert_vector *certvec,
                                    size_t size);

void spdylay_client_cert_vector_free(spdylay_client_cert_vector *certvec);

/*
 * Returns the slot of the |origin| in the client certificate vector.
 * If it is not found, returns 0.
 */
size_t spdylay_client_cert_vector_find(spdylay_client_cert_vector *certvec,
                                       const spdylay_origin *origin);

/*
 * Puts the |origin| to the |certvec|. This function takes ownership
 * of the |origin| on success.
 *
 * This function returns the positive slot index of the certificate
 * vector where the |origin| is stored if it succeeds, or 0.
 */
size_t spdylay_client_cert_vector_put(spdylay_client_cert_vector *certvec,
                                      spdylay_origin *origin);

/*
 * Resizes client certificate vector to the size |size|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * SPDYLAY_ERR_NOMEM
 *     Out of memory.
 */
int spdylay_client_cert_vector_resize(spdylay_client_cert_vector *certvec,
                                      size_t size);

const spdylay_origin* spdylay_client_cert_vector_get_origin
(spdylay_client_cert_vector *certvec,
 size_t slot);

#endif /* SPDYLAY_CLIENT_CERT_VECTOR_H */
