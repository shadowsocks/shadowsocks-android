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
#include "spdylay_outbound_item.h"

#include <assert.h>

void spdylay_outbound_item_free(spdylay_outbound_item *item)
{
  if(item == NULL) {
    return;
  }
  if(item->frame_cat == SPDYLAY_CTRL) {
    spdylay_frame_type frame_type;
    spdylay_frame *frame;
    frame_type = spdylay_outbound_item_get_ctrl_frame_type(item);
    frame = spdylay_outbound_item_get_ctrl_frame(item);
    switch(frame_type) {
    case SPDYLAY_SYN_STREAM:
      spdylay_frame_syn_stream_free(&frame->syn_stream);
      free(((spdylay_syn_stream_aux_data*)item->aux_data)->data_prd);
      break;
    case SPDYLAY_SYN_REPLY:
      spdylay_frame_syn_reply_free(&frame->syn_reply);
      break;
    case SPDYLAY_RST_STREAM:
      spdylay_frame_rst_stream_free(&frame->rst_stream);
      break;
    case SPDYLAY_SETTINGS:
      spdylay_frame_settings_free(&frame->settings);
      break;
    case SPDYLAY_NOOP:
      /* We don't have any public API to add NOOP, so here is
         unreachable. */
      assert(0);
    case SPDYLAY_PING:
      spdylay_frame_ping_free(&frame->ping);
      break;
    case SPDYLAY_GOAWAY:
      spdylay_frame_goaway_free(&frame->goaway);
      break;
    case SPDYLAY_HEADERS:
      spdylay_frame_headers_free(&frame->headers);
      break;
    case SPDYLAY_WINDOW_UPDATE:
      spdylay_frame_window_update_free(&frame->window_update);
      break;
    case SPDYLAY_CREDENTIAL:
      spdylay_frame_credential_free(&frame->credential);
      break;
    }
  } else if(item->frame_cat == SPDYLAY_DATA) {
    spdylay_data *data_frame;
    data_frame = spdylay_outbound_item_get_data_frame(item);
    spdylay_frame_data_free(data_frame);
  } else {
    /* Unreachable */
    assert(0);
  }
  free(item->frame);
  free(item->aux_data);
}
