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
#include "spdylay_helper.h"

#include <string.h>

#include "spdylay_net.h"

void spdylay_put_uint16be(uint8_t *buf, uint16_t n)
{
  uint16_t x = htons(n);
  memcpy(buf, &x, sizeof(uint16_t));
}

void spdylay_put_uint32be(uint8_t *buf, uint32_t n)
{
  uint32_t x = htonl(n);
  memcpy(buf, &x, sizeof(uint32_t));
}

uint16_t spdylay_get_uint16(const uint8_t *data)
{
  uint16_t n;
  memcpy(&n, data, sizeof(uint16_t));
  return ntohs(n);
}

uint32_t spdylay_get_uint32(const uint8_t *data)
{
  uint32_t n;
  memcpy(&n, data, sizeof(uint32_t));
  return ntohl(n);
}

int spdylay_reserve_buffer(uint8_t **buf_ptr, size_t *buflen_ptr,
                           size_t min_length)
{
  if(min_length > *buflen_ptr) {
    uint8_t *temp;
    min_length = (min_length+4095)/4096*4096;
    temp = malloc(min_length);
    if(temp == NULL) {
      return SPDYLAY_ERR_NOMEM;
    } else {
      free(*buf_ptr);
      *buf_ptr = temp;
      *buflen_ptr = min_length;
    }
  }
  return 0;
}

const char* spdylay_strerror(int error_code)
{
  switch(error_code) {
  case 0:
    return "Success";
  case SPDYLAY_ERR_INVALID_ARGUMENT:
    return "Invalid argument";
  case SPDYLAY_ERR_ZLIB:
    return "Zlib error";
  case SPDYLAY_ERR_UNSUPPORTED_VERSION:
    return "Unsupported SPDY version";
  case SPDYLAY_ERR_WOULDBLOCK:
    return "Operation would block";
  case SPDYLAY_ERR_PROTO:
    return "Protocol error";
  case SPDYLAY_ERR_INVALID_FRAME:
    return "Invalid frame octets";
  case SPDYLAY_ERR_EOF:
    return "EOF";
  case SPDYLAY_ERR_DEFERRED:
    return "Data transfer deferred";
  case SPDYLAY_ERR_STREAM_ID_NOT_AVAILABLE:
    return "No more Stream ID available";
  case SPDYLAY_ERR_STREAM_CLOSED:
    return "Stream was already closed or invalid";
  case SPDYLAY_ERR_STREAM_CLOSING:
    return "Stream is closing";
  case SPDYLAY_ERR_STREAM_SHUT_WR:
    return "The transmission is not allowed for this stream";
  case SPDYLAY_ERR_INVALID_STREAM_ID:
    return "Stream ID is invalid";
  case SPDYLAY_ERR_INVALID_STREAM_STATE:
    return "Invalid stream state";
  case SPDYLAY_ERR_DEFERRED_DATA_EXIST:
    return "Another DATA frame has already been deferred";
  case SPDYLAY_ERR_SYN_STREAM_NOT_ALLOWED:
    return "SYN_STREAM is not allowed";
  case SPDYLAY_ERR_GOAWAY_ALREADY_SENT:
    return "GOAWAY has already been sent";
  case SPDYLAY_ERR_INVALID_HEADER_BLOCK:
    return "Invalid header block";
  case SPDYLAY_ERR_INVALID_STATE:
    return "Invalid state";
  case SPDYLAY_ERR_GZIP:
    return "Gzip error";
  case SPDYLAY_ERR_TEMPORAL_CALLBACK_FAILURE:
    return "The user callback function failed due to the temporal error";
  case SPDYLAY_ERR_FRAME_TOO_LARGE:
    return "The length of the frame is too large";
  case SPDYLAY_ERR_NOMEM:
    return "Out of memory";
  case SPDYLAY_ERR_CALLBACK_FAILURE:
    return "The user callback function failed";
  default:
    return "Unknown error code";
  }
}
