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
#include "spdylay_gzip.h"

#include <assert.h>

int spdylay_gzip_inflate_new(spdylay_gzip **inflater_ptr)
{
  int rv;
  *inflater_ptr = malloc(sizeof(spdylay_gzip));
  if(*inflater_ptr == NULL) {
    return SPDYLAY_ERR_NOMEM;
  }
  (*inflater_ptr)->zst.next_in = Z_NULL;
  (*inflater_ptr)->zst.avail_in = 0;
  (*inflater_ptr)->zst.zalloc = Z_NULL;
  (*inflater_ptr)->zst.zfree = Z_NULL;
  (*inflater_ptr)->zst.opaque = Z_NULL;
  rv = inflateInit2(&(*inflater_ptr)->zst, 47);
  if(rv != Z_OK) {
    free(*inflater_ptr);
    return SPDYLAY_ERR_GZIP;
  }
  return 0;
}

void spdylay_gzip_inflate_del(spdylay_gzip *inflater)
{
  if(inflater != NULL) {
    inflateEnd(&inflater->zst);
    free(inflater);
  }
}

int spdylay_gzip_inflate(spdylay_gzip *inflater,
                         uint8_t *out, size_t *outlen_ptr,
                         const uint8_t *in, size_t *inlen_ptr)
{
  int rv;
  inflater->zst.avail_in = *inlen_ptr;
  inflater->zst.next_in = (unsigned char*)in;
  inflater->zst.avail_out = *outlen_ptr;
  inflater->zst.next_out = out;

  rv = inflate(&inflater->zst, Z_NO_FLUSH);

  *inlen_ptr -= inflater->zst.avail_in;
  *outlen_ptr -= inflater->zst.avail_out;
  switch(rv) {
  case Z_OK:
  case Z_STREAM_END:
  case Z_BUF_ERROR:
    return 0;
  case Z_DATA_ERROR:
  case Z_STREAM_ERROR:
  case Z_NEED_DICT:
  case Z_MEM_ERROR:
    return SPDYLAY_ERR_GZIP;
  default:
    assert(0);
    /* We need this for some compilers */
    return 0;
  }
}
