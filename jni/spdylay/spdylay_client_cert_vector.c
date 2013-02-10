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
#include "spdylay_client_cert_vector.h"

#include <string.h>

#include "spdylay_helper.h"

int spdylay_origin_equal(const spdylay_origin *lhs, const spdylay_origin *rhs)
{
  return strcmp(lhs->scheme, rhs->scheme) == 0 &&
    strcmp(lhs->host, rhs->host) == 0 && lhs->port == rhs->port;
}

int spdylay_origin_set(spdylay_origin *origin,
                       const char *scheme, const char *host, uint16_t port)
{
  if(strlen(scheme) > SPDYLAY_MAX_SCHEME ||
     strlen(host) > SPDYLAY_MAX_HOSTNAME) {
    return SPDYLAY_ERR_INVALID_ARGUMENT;
  }
  strcpy(origin->scheme, scheme);
  strcpy(origin->host, host);
  origin->port = port;
  return 0;
}

const char* spdylay_origin_get_scheme(const spdylay_origin *origin)
{
  return origin->scheme;
}

const char* spdylay_origin_get_host(const spdylay_origin *origin)
{
  return origin->host;
}

uint16_t spdylay_origin_get_port(const spdylay_origin *origin)
{
  return origin->port;
}

int spdylay_client_cert_vector_init(spdylay_client_cert_vector *certvec,
                                    size_t size)
{
  certvec->size = certvec->capacity = size;
  certvec->last_slot = 0;
  if(certvec->capacity) {
    size_t vec_size = sizeof(spdylay_origin*)*certvec->capacity;
    certvec->vector = malloc(vec_size);
    if(certvec->vector == NULL) {
      return SPDYLAY_ERR_NOMEM;
    }
    memset(certvec->vector, 0, vec_size);
  } else {
    certvec->vector = NULL;
  }
  return 0;
}

void spdylay_client_cert_vector_free(spdylay_client_cert_vector *certvec)
{
  size_t i;
  for(i = 0; i < certvec->size; ++i) {
    free(certvec->vector[i]);
  }
  free(certvec->vector);
}

int spdylay_client_cert_vector_resize(spdylay_client_cert_vector *certvec,
                                      size_t size)
{
  if(certvec->capacity < size) {
    spdylay_origin **vector = realloc(certvec->vector,
                                      sizeof(spdylay_origin*)*size);
    if(vector == NULL) {
      return SPDYLAY_ERR_NOMEM;
    }
    memset(vector + certvec->capacity, 0,
           sizeof(spdylay_origin*)*(size - certvec->capacity));
    certvec->vector = vector;
    certvec->size = certvec->capacity = size;
  } else {
    size_t i;
    for(i = size; i < certvec->size; ++i) {
      free(certvec->vector[i]);
      certvec->vector[i] = NULL;
    }
    certvec->size = spdylay_min(certvec->size, size);
    if(certvec->last_slot > certvec->size) {
      certvec->last_slot = certvec->size;
    }
  }
  return 0;
}

size_t spdylay_client_cert_vector_find(spdylay_client_cert_vector *certvec,
                                       const spdylay_origin *origin)
{
  size_t i;
  for(i = 0; i < certvec->size && certvec->vector[i]; ++i) {
    if(spdylay_origin_equal(origin, certvec->vector[i])) {
      return i+1;
    }
  }
  return 0;
}

size_t spdylay_client_cert_vector_put(spdylay_client_cert_vector *certvec,
                                      spdylay_origin *origin)
{
  if(certvec->size == 0) {
    return 0;
  }
  if(certvec->last_slot == certvec->size) {
    certvec->last_slot = 1;
  } else {
    ++certvec->last_slot;
  }
  free(certvec->vector[certvec->last_slot-1]);
  certvec->vector[certvec->last_slot-1] = origin;
  return certvec->last_slot;
}

const spdylay_origin* spdylay_client_cert_vector_get_origin
(spdylay_client_cert_vector *certvec,
 size_t slot)
{
  if(slot == 0 || slot > certvec->size || !certvec->vector[slot-1]) {
    return NULL;
  } else {
    return certvec->vector[slot-1];
  }
}
