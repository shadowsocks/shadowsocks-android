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
#ifndef SPDYLAY_OUTBOUND_ITEM_H
#define SPDYLAY_OUTBOUND_ITEM_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <spdylay/spdylay.h>
#include "spdylay_frame.h"

/* Priority for PING */
#define SPDYLAY_OB_PRI_PING -10
/* Priority for CREDENTIAL */
#define SPDYLAY_OB_PRI_CREDENTIAL -2
/* Priority for the frame which must be sent after CREDENTIAL */
#define SPDYLAY_OB_PRI_AFTER_CREDENTIAL -1

typedef struct {
  spdylay_data_provider *data_prd;
  void *stream_user_data;
} spdylay_syn_stream_aux_data;

typedef struct {
  /* Type of |frame|. SPDYLAY_CTRL: spdylay_frame*, SPDYLAY_DATA:
     spdylay_data* */
  spdylay_frame_category frame_cat;
  void *frame;
  void *aux_data;
  int pri;
  int64_t seq;
} spdylay_outbound_item;

/*
 * Deallocates resource for |item|. If |item| is NULL, this function
 * does nothing.
 */
void spdylay_outbound_item_free(spdylay_outbound_item *item);

/* Macros to cast spdylay_outbound_item.frame to the proper type. */
#define spdylay_outbound_item_get_ctrl_frame(ITEM) ((spdylay_frame*)ITEM->frame)
#define spdylay_outbound_item_get_ctrl_frame_type(ITEM) \
  (((spdylay_frame*)ITEM->frame)->ctrl.hd.type)
#define spdylay_outbound_item_get_data_frame(ITEM) ((spdylay_data*)ITEM->frame)

#endif /* SPDYLAY_OUTBOUND_ITEM_H */
