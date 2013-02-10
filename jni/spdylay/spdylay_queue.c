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
#include "spdylay_queue.h"

#include <string.h>
#include <assert.h>

void spdylay_queue_init(spdylay_queue *queue)
{
  queue->front = queue->back = NULL;
}

void spdylay_queue_free(spdylay_queue *queue)
{
  if(!queue) {
    return;
  } else {
    spdylay_queue_cell *p = queue->front;
    while(p) {
      spdylay_queue_cell *next = p->next;
      free(p);
      p = next;
    }
  }
}

int spdylay_queue_push(spdylay_queue *queue, void *data)
{
  spdylay_queue_cell *new_cell = (spdylay_queue_cell*)malloc
    (sizeof(spdylay_queue_cell));
  if(!new_cell) {
    return SPDYLAY_ERR_NOMEM;
  }
  new_cell->data = data;
  new_cell->next = NULL;
  if(queue->back) {
    queue->back->next = new_cell;
    queue->back = new_cell;

  } else {
    queue->front = queue->back = new_cell;
  }
  return 0;
}

void spdylay_queue_pop(spdylay_queue *queue)
{
  spdylay_queue_cell *front = queue->front;
  assert(front);
  queue->front = front->next;
  if(front == queue->back) {
    queue->back = NULL;
  }
  free(front);
}

void* spdylay_queue_front(spdylay_queue *queue)
{
  assert(queue->front);
  return queue->front->data;
}

void* spdylay_queue_back(spdylay_queue *queue)
{
  assert(queue->back);
  return queue->back->data;
}

int spdylay_queue_empty(spdylay_queue *queue)
{
  return queue->front == NULL;
}
