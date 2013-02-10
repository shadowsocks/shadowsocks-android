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
#include "spdylay_buffer.h"

#include <assert.h>
#include <string.h>

#include "spdylay_net.h"
#include "spdylay_helper.h"

void spdylay_buffer_init(spdylay_buffer *buffer, size_t chunk_capacity)
{
  buffer->root.data = NULL;
  buffer->root.next = NULL;
  buffer->current = &buffer->root;
  buffer->capacity = chunk_capacity;
  buffer->len = 0;
  /*
   * Set last_offset to maximum so that first append adds new buffer
   * buffer.
   */
  buffer->last_offset = buffer->capacity;
}

void spdylay_buffer_free(spdylay_buffer *buffer)
{
  spdylay_buffer_chunk *p = buffer->root.next;
  while(p) {
    spdylay_buffer_chunk *next = p->next;
    free(p->data);
    free(p);
    p = next;
  }
}

int spdylay_buffer_alloc(spdylay_buffer *buffer)
{
  if(buffer->current->next == NULL) {
    spdylay_buffer_chunk *chunk;
    uint8_t *buf;
    chunk = malloc(sizeof(spdylay_buffer_chunk));
    if(chunk == NULL) {
      return SPDYLAY_ERR_NOMEM;
    }
    buf = malloc(buffer->capacity);
    if(buf == NULL) {
      free(chunk);
      return SPDYLAY_ERR_NOMEM;
    }
    chunk->data = buf;
    chunk->next = NULL;
    buffer->current->next = chunk;
    buffer->current = chunk;
  } else {
    buffer->current = buffer->current->next;
  }
  buffer->len += buffer->capacity-buffer->last_offset;
  buffer->last_offset = 0;
  return 0;
}

uint8_t* spdylay_buffer_get(spdylay_buffer *buffer)
{
  if(buffer->current->data == NULL) {
    return NULL;
  } else {
    return buffer->current->data+buffer->last_offset;
  }
}

size_t spdylay_buffer_avail(spdylay_buffer *buffer)
{
  return buffer->capacity-buffer->last_offset;
}

void spdylay_buffer_advance(spdylay_buffer *buffer, size_t amount)
{
  buffer->last_offset += amount;
  buffer->len += amount;
  assert(buffer->last_offset <= buffer->capacity);
}

int spdylay_buffer_write(spdylay_buffer *buffer, const uint8_t *data,
                          size_t len)
{
  int rv;
  while(len) {
    size_t writelen;
    if(spdylay_buffer_avail(buffer) == 0) {
      if((rv = spdylay_buffer_alloc(buffer)) != 0) {
        return rv;
      }
    }
    writelen = spdylay_min(spdylay_buffer_avail(buffer), len);
    memcpy(spdylay_buffer_get(buffer), data, writelen);
    data += writelen;
    len -= writelen;
    spdylay_buffer_advance(buffer, writelen);
  }
  return 0;
}

size_t spdylay_buffer_length(spdylay_buffer *buffer)
{
  return buffer->len;
}

size_t spdylay_buffer_capacity(spdylay_buffer *buffer)
{
  return buffer->capacity;
}

void spdylay_buffer_serialize(spdylay_buffer *buffer, uint8_t *buf)
{
  spdylay_buffer_chunk *p = buffer->root.next;
  for(; p; p = p->next) {
    size_t len;
    if(p == buffer->current) {
      len = buffer->last_offset;
    } else {
      len = buffer->capacity;
    }
    memcpy(buf, p->data, len);
    buf += len;
  }
}

void spdylay_buffer_reset(spdylay_buffer *buffer)
{
  buffer->current = &buffer->root;
  buffer->len = 0;
  buffer->last_offset = buffer->capacity;
}

void spdylay_buffer_reader_init(spdylay_buffer_reader *reader,
                                spdylay_buffer *buffer)
{
  reader->buffer = buffer;
  reader->current = buffer->root.next;
  reader->offset = 0;
}

uint8_t spdylay_buffer_reader_uint8(spdylay_buffer_reader *reader)
{
  uint8_t out;
  spdylay_buffer_reader_data(reader, &out, sizeof(uint8_t));
  return out;
}

uint16_t spdylay_buffer_reader_uint16(spdylay_buffer_reader *reader)
{
  uint16_t out;
  spdylay_buffer_reader_data(reader, (uint8_t*)&out, sizeof(uint16_t));
  return ntohs(out);
}

uint32_t spdylay_buffer_reader_uint32(spdylay_buffer_reader *reader)
{
  uint32_t out;
  spdylay_buffer_reader_data(reader, (uint8_t*)&out, sizeof(uint32_t));
  return ntohl(out);
}

void spdylay_buffer_reader_data(spdylay_buffer_reader *reader,
                                uint8_t *out, size_t len)
{
  while(len) {
    size_t remlen, readlen;
    remlen = reader->buffer->capacity - reader->offset;
    readlen = spdylay_min(remlen, len);
    memcpy(out, reader->current->data + reader->offset, readlen);
    out += readlen;
    len -= readlen;
    reader->offset += readlen;
    if(reader->buffer->capacity == reader->offset) {
      reader->current = reader->current->next;
      reader->offset = 0;
    }
  }
}

int spdylay_buffer_reader_count(spdylay_buffer_reader *reader,
                                size_t len, uint8_t c)
{
  int res = 0;
  while(len) {
    size_t remlen, readlen, i;
    uint8_t *p;
    remlen = reader->buffer->capacity - reader->offset;
    readlen = spdylay_min(remlen, len);
    p = reader->current->data + reader->offset;
    for(i = 0; i < readlen; ++i) {
      if(p[i] == c) {
        ++res;
      }
    }
    len -= readlen;
    reader->offset += readlen;
    if(reader->buffer->capacity == reader->offset) {
      reader->current = reader->current->next;
      reader->offset = 0;
    }
  }
  return res;
}

void spdylay_buffer_reader_advance(spdylay_buffer_reader *reader,
                                   size_t amount)
{
  while(amount) {
    size_t remlen, skiplen;
    remlen = reader->buffer->capacity - reader->offset;
    skiplen = spdylay_min(remlen, amount);
    amount -= skiplen;
    reader->offset += skiplen;
    if(reader->buffer->capacity == reader->offset) {
      reader->current = reader->current->next;
      reader->offset = 0;
    }
  }
}
