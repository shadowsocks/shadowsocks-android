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
#ifndef SPDYLAY_QUEUE_H
#define SPDYLAY_QUEUE_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */

#include <spdylay/spdylay.h>

typedef struct spdylay_queue_cell {
  void *data;
  struct spdylay_queue_cell *next;
} spdylay_queue_cell;

typedef struct {
  spdylay_queue_cell *front, *back;
} spdylay_queue;

void spdylay_queue_init(spdylay_queue *queue);
void spdylay_queue_free(spdylay_queue *queue);
int spdylay_queue_push(spdylay_queue *queue, void *data);
void spdylay_queue_pop(spdylay_queue *queue);
void* spdylay_queue_front(spdylay_queue *queue);
void* spdylay_queue_back(spdylay_queue *queue);
int spdylay_queue_empty(spdylay_queue *queue);

#endif /* SPDYLAY_QUEUE_H */
