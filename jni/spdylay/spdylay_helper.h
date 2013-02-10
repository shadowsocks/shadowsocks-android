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
#ifndef SPDYLAY_HELPER_H
#define SPDYLAY_HELPER_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <spdylay/spdylay.h>

#define spdylay_min(A, B) ((A) < (B) ? (A) : (B))
#define spdylay_max(A, B) ((A) > (B) ? (A) : (B))

/*
 * Copies 2 byte unsigned integer |n| in host byte order to |buf| in
 * network byte order.
 */
void spdylay_put_uint16be(uint8_t *buf, uint16_t n);

/*
 * Copies 4 byte unsigned integer |n| in host byte order to |buf| in
 * network byte order.
 */
void spdylay_put_uint32be(uint8_t *buf, uint32_t n);

/*
 * Retrieves 2 byte unsigned integer stored in |data| in network byte
 * order and returns it in host byte order.
 */
uint16_t spdylay_get_uint16(const uint8_t *data);

/*
 * Retrieves 4 byte unsigned integer stored in |data| in network byte
 * order and returns it in host byte order.
 */
uint32_t spdylay_get_uint32(const uint8_t *data);

/*
 * Ensures that buffer |*buf_ptr| with |*buflen_ptr| length has at
 * least |min_length| bytes. If |min_length| > |*buflen_ptr|,
 * allocates new buffer having at least |min_length| bytes and assigns
 * its pointer to |*buf_ptr| and allocated number of bytes to
 * |*buflen_ptr|. The memory pointed by |*buf_ptr| previously is
 * freed. No memory copy is done between old and new buffer.
 * |*buf_ptr| and |*buflen_ptr| are only updated iff this function
 * succeeds.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * SPDYLAY_ERR_NOMEM
 *     Out of memory.
 */
int spdylay_reserve_buffer(uint8_t **buf_ptr, size_t *buflen_ptr,
                           size_t min_length);

#endif /* SPDYLAY_HELPER_H */
