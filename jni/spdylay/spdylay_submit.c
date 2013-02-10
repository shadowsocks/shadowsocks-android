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
#include "spdylay_submit.h"

#include <string.h>

#include "spdylay_session.h"
#include "spdylay_frame.h"
#include "spdylay_helper.h"

static int spdylay_submit_syn_stream_shared
(spdylay_session *session,
 uint8_t flags,
 int32_t assoc_stream_id,
 uint8_t pri,
 const char **nv,
 const spdylay_data_provider *data_prd,
 void *stream_user_data)
{
  int r;
  spdylay_frame *frame;
  char **nv_copy;
  uint8_t flags_copy;
  spdylay_data_provider *data_prd_copy = NULL;
  spdylay_syn_stream_aux_data *aux_data;
  if(pri > spdylay_session_get_pri_lowest(session)) {
    return SPDYLAY_ERR_INVALID_ARGUMENT;
  }
  if(assoc_stream_id != 0) {
    if(session->server == 0) {
      assoc_stream_id = 0;
    } else if(spdylay_session_is_my_stream_id(session, assoc_stream_id)) {
      return SPDYLAY_ERR_INVALID_ARGUMENT;
    }
  }
  if(!spdylay_frame_nv_check_null(nv)) {
    return SPDYLAY_ERR_INVALID_ARGUMENT;
  }
  if(data_prd != NULL && data_prd->read_callback != NULL) {
    data_prd_copy = malloc(sizeof(spdylay_data_provider));
    if(data_prd_copy == NULL) {
      return SPDYLAY_ERR_NOMEM;
    }
    *data_prd_copy = *data_prd;
  }
  aux_data = malloc(sizeof(spdylay_syn_stream_aux_data));
  if(aux_data == NULL) {
    free(data_prd_copy);
    return SPDYLAY_ERR_NOMEM;
  }
  aux_data->data_prd = data_prd_copy;
  aux_data->stream_user_data = stream_user_data;

  frame = malloc(sizeof(spdylay_frame));
  if(frame == NULL) {
    free(aux_data);
    free(data_prd_copy);
    return SPDYLAY_ERR_NOMEM;
  }
  nv_copy = spdylay_frame_nv_norm_copy(nv);
  if(nv_copy == NULL) {
    free(frame);
    free(aux_data);
    free(data_prd_copy);
    return SPDYLAY_ERR_NOMEM;
  }
  flags_copy = 0;
  if(flags & SPDYLAY_CTRL_FLAG_FIN) {
    flags_copy |= SPDYLAY_CTRL_FLAG_FIN;
  }
  if(flags & SPDYLAY_CTRL_FLAG_UNIDIRECTIONAL) {
    flags_copy |= SPDYLAY_CTRL_FLAG_UNIDIRECTIONAL;
  }
  spdylay_frame_syn_stream_init(&frame->syn_stream,
                                session->version, flags_copy,
                                0, assoc_stream_id, pri, nv_copy);
  r = spdylay_session_add_frame(session, SPDYLAY_CTRL, frame,
                                aux_data);
  if(r != 0) {
    spdylay_frame_syn_stream_free(&frame->syn_stream);
    free(frame);
    free(aux_data);
    free(data_prd_copy);
  }
  return r;
}

int spdylay_submit_syn_stream(spdylay_session *session, uint8_t flags,
                              int32_t assoc_stream_id, uint8_t pri,
                              const char **nv, void *stream_user_data)
{
  return spdylay_submit_syn_stream_shared(session, flags, assoc_stream_id,
                                          pri, nv, NULL, stream_user_data);
}

int spdylay_submit_syn_reply(spdylay_session *session, uint8_t flags,
                             int32_t stream_id, const char **nv)
{
  int r;
  spdylay_frame *frame;
  char **nv_copy;
  uint8_t flags_copy;
  if(!spdylay_frame_nv_check_null(nv)) {
    return SPDYLAY_ERR_INVALID_ARGUMENT;
  }
  frame = malloc(sizeof(spdylay_frame));
  if(frame == NULL) {
    return SPDYLAY_ERR_NOMEM;
  }
  nv_copy = spdylay_frame_nv_norm_copy(nv);
  if(nv_copy == NULL) {
    free(frame);
    return SPDYLAY_ERR_NOMEM;
  }
  flags_copy = 0;
  if(flags & SPDYLAY_CTRL_FLAG_FIN) {
    flags_copy |= SPDYLAY_CTRL_FLAG_FIN;
  }
  spdylay_frame_syn_reply_init(&frame->syn_reply, session->version, flags_copy,
                               stream_id, nv_copy);
  r = spdylay_session_add_frame(session, SPDYLAY_CTRL, frame, NULL);
  if(r != 0) {
    spdylay_frame_syn_reply_free(&frame->syn_reply);
    free(frame);
  }
  return r;
}

int spdylay_submit_headers(spdylay_session *session, uint8_t flags,
                           int32_t stream_id, const char **nv)
{
  int r;
  spdylay_frame *frame;
  char **nv_copy;
  uint8_t flags_copy;
  if(!spdylay_frame_nv_check_null(nv)) {
    return SPDYLAY_ERR_INVALID_ARGUMENT;
  }
  frame = malloc(sizeof(spdylay_frame));
  if(frame == NULL) {
    return SPDYLAY_ERR_NOMEM;
  }
  nv_copy = spdylay_frame_nv_norm_copy(nv);
  if(nv_copy == NULL) {
    free(frame);
    return SPDYLAY_ERR_NOMEM;
  }
  flags_copy = 0;
  if(flags & SPDYLAY_CTRL_FLAG_FIN) {
    flags_copy |= SPDYLAY_CTRL_FLAG_FIN;
  }
  spdylay_frame_headers_init(&frame->headers, session->version, flags_copy,
                             stream_id, nv_copy);
  r = spdylay_session_add_frame(session, SPDYLAY_CTRL, frame, NULL);
  if(r != 0) {
    spdylay_frame_headers_free(&frame->headers);
    free(frame);
  }
  return r;
}

int spdylay_submit_ping(spdylay_session *session)
{
  return spdylay_session_add_ping(session,
                                  spdylay_session_get_next_unique_id(session));
}

int spdylay_submit_rst_stream(spdylay_session *session, int32_t stream_id,
                              uint32_t status_code)
{
  return spdylay_session_add_rst_stream(session, stream_id, status_code);
}

int spdylay_submit_goaway(spdylay_session *session, uint32_t status_code)
{
  return spdylay_session_add_goaway(session, session->last_recv_stream_id,
                                    status_code);
}

int spdylay_submit_settings(spdylay_session *session, uint8_t flags,
                            const spdylay_settings_entry *iv, size_t niv)
{
  spdylay_frame *frame;
  spdylay_settings_entry *iv_copy;
  int check[SPDYLAY_SETTINGS_MAX+1];
  size_t i;
  int r;
  memset(check, 0, sizeof(check));
  for(i = 0; i < niv; ++i) {
    if(iv[i].settings_id > SPDYLAY_SETTINGS_MAX || iv[i].settings_id == 0 ||
       check[iv[i].settings_id] == 1) {
      return SPDYLAY_ERR_INVALID_ARGUMENT;
    } else {
      check[iv[i].settings_id] = 1;
    }
  }
  frame = malloc(sizeof(spdylay_frame));
  if(frame == NULL) {
    return SPDYLAY_ERR_NOMEM;
  }
  iv_copy = spdylay_frame_iv_copy(iv, niv);
  if(iv_copy == NULL) {
    free(frame);
    return SPDYLAY_ERR_NOMEM;
  }
  spdylay_frame_iv_sort(iv_copy, niv);
  spdylay_frame_settings_init(&frame->settings, session->version,
                              flags, iv_copy, niv);
  r = spdylay_session_add_frame(session, SPDYLAY_CTRL, frame, NULL);
  if(r == 0) {
    spdylay_session_update_local_settings(session, iv_copy, niv);
  } else {
    spdylay_frame_settings_free(&frame->settings);
    free(frame);
  }
  return r;
}

int spdylay_submit_window_update(spdylay_session *session, int32_t stream_id,
                                 int32_t delta_window_size)
{
  spdylay_stream *stream;
  if(delta_window_size <= 0) {
    return SPDYLAY_ERR_INVALID_ARGUMENT;
  }
  stream = spdylay_session_get_stream(session, stream_id);
  if(stream) {
    stream->recv_window_size -= spdylay_min(delta_window_size,
                                            stream->recv_window_size);
    return spdylay_session_add_window_update(session, stream_id,
                                             delta_window_size);
  } else {
    return SPDYLAY_ERR_STREAM_CLOSED;
  }
}

int spdylay_submit_request(spdylay_session *session, uint8_t pri,
                           const char **nv,
                           const spdylay_data_provider *data_prd,
                           void *stream_user_data)
{
  int flags;
  flags = 0;
  if(data_prd == NULL || data_prd->read_callback == NULL) {
    flags |= SPDYLAY_CTRL_FLAG_FIN;
  }
  return spdylay_submit_syn_stream_shared(session, flags, 0, pri, nv, data_prd,
                                          stream_user_data);
}

int spdylay_submit_response(spdylay_session *session,
                            int32_t stream_id, const char **nv,
                            const spdylay_data_provider *data_prd)
{
  int r;
  spdylay_frame *frame;
  char **nv_copy;
  uint8_t flags = 0;
  spdylay_data_provider *data_prd_copy = NULL;
  if(!spdylay_frame_nv_check_null(nv)) {
    return SPDYLAY_ERR_INVALID_ARGUMENT;
  }
  if(data_prd != NULL && data_prd->read_callback != NULL) {
    data_prd_copy = malloc(sizeof(spdylay_data_provider));
    if(data_prd_copy == NULL) {
      return SPDYLAY_ERR_NOMEM;
    }
    *data_prd_copy = *data_prd;
  }
  frame = malloc(sizeof(spdylay_frame));
  if(frame == NULL) {
    free(data_prd_copy);
    return SPDYLAY_ERR_NOMEM;
  }
  nv_copy = spdylay_frame_nv_norm_copy(nv);
  if(nv_copy == NULL) {
    free(frame);
    free(data_prd_copy);
    return SPDYLAY_ERR_NOMEM;
  }
  if(data_prd_copy == NULL) {
    flags |= SPDYLAY_CTRL_FLAG_FIN;
  }
  spdylay_frame_syn_reply_init(&frame->syn_reply, session->version, flags,
                               stream_id, nv_copy);
  r = spdylay_session_add_frame(session, SPDYLAY_CTRL, frame,
                                data_prd_copy);
  if(r != 0) {
    spdylay_frame_syn_reply_free(&frame->syn_reply);
    free(frame);
    free(data_prd_copy);
  }
  return r;
}

int spdylay_submit_data(spdylay_session *session, int32_t stream_id,
                        uint8_t flags,
                        const spdylay_data_provider *data_prd)
{
  int r;
  spdylay_data *data_frame;
  uint8_t nflags = 0;
  data_frame = malloc(sizeof(spdylay_frame));
  if(data_frame == NULL) {
    return SPDYLAY_ERR_NOMEM;
  }
  if(flags & SPDYLAY_DATA_FLAG_FIN) {
    nflags |= SPDYLAY_DATA_FLAG_FIN;
  }
  spdylay_frame_data_init(data_frame, stream_id, nflags, data_prd);
  r = spdylay_session_add_frame(session, SPDYLAY_DATA, data_frame, NULL);
  if(r != 0) {
    spdylay_frame_data_free(data_frame);
    free(data_frame);
  }
  return r;
}
