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
#include "spdylay_session.h"

#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <assert.h>

#include "spdylay_helper.h"
#include "spdylay_net.h"

/*
 * Returns non-zero if the number of outgoing opened streams is larger
 * than or equal to
 * remote_settings[SPDYLAY_SETTINGS_MAX_CONCURRENT_STREAMS].
 */
static int spdylay_session_is_outgoing_concurrent_streams_max
(spdylay_session *session)
{
  return session->remote_settings[SPDYLAY_SETTINGS_MAX_CONCURRENT_STREAMS]
    <= session->num_outgoing_streams;
}

/*
 * Returns non-zero if the number of incoming opened streams is larger
 * than or equal to
 * local_settings[SPDYLAY_SETTINGS_MAX_CONCURRENT_STREAMS].
 */
static int spdylay_session_is_incoming_concurrent_streams_max
(spdylay_session *session)
{
  return session->local_settings[SPDYLAY_SETTINGS_MAX_CONCURRENT_STREAMS]
    <= session->num_incoming_streams;
}

/*
 * Returns non-zero if |error| is non-fatal error.
 */
static int spdylay_is_non_fatal(int error)
{
  return error < 0 && error > SPDYLAY_ERR_FATAL;
}

/*
 * Returns non-zero if |error| is fatal error.
 */
static int spdylay_is_fatal(int error)
{
  return error < SPDYLAY_ERR_FATAL;
}

int spdylay_session_fail_session(spdylay_session *session,
                                 uint32_t status_code)
{
  session->goaway_flags |= SPDYLAY_GOAWAY_FAIL_ON_SEND;
  return spdylay_submit_goaway(session, status_code);
}

int spdylay_session_is_my_stream_id(spdylay_session *session,
                                    int32_t stream_id)
{
  int r;
  if(stream_id == 0) {
    return 0;
  }
  r = stream_id % 2;
  return (session->server && r == 0) || (!session->server && r == 1);
}

spdylay_stream* spdylay_session_get_stream(spdylay_session *session,
                                           int32_t stream_id)
{
  return (spdylay_stream*)spdylay_map_find(&session->streams, stream_id);
}

static int spdylay_outbound_item_compar(const void *lhsx, const void *rhsx)
{
  const spdylay_outbound_item *lhs, *rhs;
  lhs = (const spdylay_outbound_item*)lhsx;
  rhs = (const spdylay_outbound_item*)rhsx;
  if(lhs->pri == rhs->pri) {
    return (lhs->seq < rhs->seq) ? -1 : ((lhs->seq > rhs->seq) ? 1 : 0);
  } else {
    return lhs->pri-rhs->pri;
  }
}

static void spdylay_inbound_frame_reset(spdylay_inbound_frame *iframe)
{
  iframe->state = SPDYLAY_RECV_HEAD;
  iframe->payloadlen = iframe->buflen = iframe->off = 0;
  iframe->headbufoff = 0;
  spdylay_buffer_reset(&iframe->inflatebuf);
  iframe->error_code = 0;
}

/*
 * Returns the number of bytes before name/value header block for the
 * incoming frame. If the incoming frame does not have name/value
 * block, this function returns -1.
 */
static size_t spdylay_inbound_frame_payload_nv_offset
(spdylay_inbound_frame *iframe)
{
  uint16_t type, version;
  ssize_t offset;
  type = spdylay_get_uint16(&iframe->headbuf[2]);
  version = spdylay_get_uint16(&iframe->headbuf[0]) & SPDYLAY_VERSION_MASK;
  offset = spdylay_frame_nv_offset(type, version);
  if(offset != -1) {
    offset -= SPDYLAY_HEAD_LEN;
  }
  return offset;
}

static int spdylay_session_new(spdylay_session **session_ptr,
                               uint16_t version,
                               const spdylay_session_callbacks *callbacks,
                               void *user_data,
                               size_t cli_certvec_length,
                               int hd_comp)
{
  int r;
  if(version != SPDYLAY_PROTO_SPDY2 && version != SPDYLAY_PROTO_SPDY3) {
    return SPDYLAY_ERR_UNSUPPORTED_VERSION;
  }
  *session_ptr = malloc(sizeof(spdylay_session));
  if(*session_ptr == NULL) {
    r = SPDYLAY_ERR_NOMEM;
    goto fail_session;
  }
  memset(*session_ptr, 0, sizeof(spdylay_session));

  (*session_ptr)->version = version;

  /* next_stream_id, last_recv_stream_id and next_unique_id are
     initialized in either spdylay_session_client_new or
     spdylay_session_server_new */

  (*session_ptr)->flow_control =
    (*session_ptr)->version == SPDYLAY_PROTO_SPDY3;

  (*session_ptr)->last_ping_unique_id = 0;

  (*session_ptr)->next_seq = 0;

  (*session_ptr)->goaway_flags = SPDYLAY_GOAWAY_NONE;
  (*session_ptr)->last_good_stream_id = 0;

  (*session_ptr)->max_recv_ctrl_frame_buf = (1 << 24)-1;

  r = spdylay_zlib_deflate_hd_init(&(*session_ptr)->hd_deflater,
                                   hd_comp,
                                   (*session_ptr)->version);
  if(r != 0) {
    goto fail_hd_deflater;
  }
  r = spdylay_zlib_inflate_hd_init(&(*session_ptr)->hd_inflater,
                                   (*session_ptr)->version);
  if(r != 0) {
    goto fail_hd_inflater;
  }
  spdylay_map_init(&(*session_ptr)->streams);
  r = spdylay_pq_init(&(*session_ptr)->ob_pq, spdylay_outbound_item_compar);
  if(r != 0) {
    goto fail_ob_pq;
  }
  r = spdylay_pq_init(&(*session_ptr)->ob_ss_pq, spdylay_outbound_item_compar);
  if(r != 0) {
    goto fail_ob_ss_pq;
  }

  (*session_ptr)->aob.framebuf = malloc
    (SPDYLAY_INITIAL_OUTBOUND_FRAMEBUF_LENGTH);
  if((*session_ptr)->aob.framebuf == NULL) {
    r = SPDYLAY_ERR_NOMEM;
    goto fail_aob_framebuf;
  }
  (*session_ptr)->aob.framebufmax = SPDYLAY_INITIAL_OUTBOUND_FRAMEBUF_LENGTH;

  (*session_ptr)->nvbuf = malloc(SPDYLAY_INITIAL_NV_BUFFER_LENGTH);
  if((*session_ptr)->nvbuf == NULL) {
    r = SPDYLAY_ERR_NOMEM;
    goto fail_nvbuf;
  }
  (*session_ptr)->nvbuflen = SPDYLAY_INITIAL_NV_BUFFER_LENGTH;

  memset((*session_ptr)->remote_settings, 0,
         sizeof((*session_ptr)->remote_settings));
  (*session_ptr)->remote_settings[SPDYLAY_SETTINGS_MAX_CONCURRENT_STREAMS] =
    SPDYLAY_INITIAL_MAX_CONCURRENT_STREAMS;
  (*session_ptr)->remote_settings[SPDYLAY_SETTINGS_INITIAL_WINDOW_SIZE] =
    SPDYLAY_INITIAL_WINDOW_SIZE;

  memset((*session_ptr)->local_settings, 0,
         sizeof((*session_ptr)->local_settings));
  (*session_ptr)->local_settings[SPDYLAY_SETTINGS_MAX_CONCURRENT_STREAMS] =
    SPDYLAY_INITIAL_MAX_CONCURRENT_STREAMS;
  (*session_ptr)->local_settings[SPDYLAY_SETTINGS_INITIAL_WINDOW_SIZE] =
    SPDYLAY_INITIAL_WINDOW_SIZE;

  (*session_ptr)->callbacks = *callbacks;
  (*session_ptr)->user_data = user_data;

  (*session_ptr)->iframe.buf = malloc(SPDYLAY_INITIAL_INBOUND_FRAMEBUF_LENGTH);
  if((*session_ptr)->iframe.buf == NULL) {
    r = SPDYLAY_ERR_NOMEM;
    goto fail_iframe_buf;
  }
  (*session_ptr)->iframe.bufmax = SPDYLAY_INITIAL_INBOUND_FRAMEBUF_LENGTH;
  spdylay_buffer_init(&(*session_ptr)->iframe.inflatebuf, 4096);

  spdylay_inbound_frame_reset(&(*session_ptr)->iframe);

  r = spdylay_client_cert_vector_init(&(*session_ptr)->cli_certvec,
                                      cli_certvec_length);
  if(r != 0) {
    goto fail_client_cert_vector;
  }

  return 0;

 fail_client_cert_vector:
  free((*session_ptr)->iframe.buf);
 fail_iframe_buf:
  free((*session_ptr)->nvbuf);
 fail_nvbuf:
  free((*session_ptr)->aob.framebuf);
 fail_aob_framebuf:
  spdylay_pq_free(&(*session_ptr)->ob_ss_pq);
 fail_ob_ss_pq:
  spdylay_pq_free(&(*session_ptr)->ob_pq);
 fail_ob_pq:
  /* No need to free (*session_ptr)->streams) here. */
  spdylay_zlib_inflate_free(&(*session_ptr)->hd_inflater);
 fail_hd_inflater:
  spdylay_zlib_deflate_free(&(*session_ptr)->hd_deflater);
 fail_hd_deflater:
  free(*session_ptr);
 fail_session:
  return r;
}

int spdylay_session_client_new(spdylay_session **session_ptr,
                               uint16_t version,
                               const spdylay_session_callbacks *callbacks,
                               void *user_data)
{
  int r;
  /* For client side session, header compression is disabled. */
  r = spdylay_session_new(session_ptr, version, callbacks, user_data,
                          SPDYLAY_INITIAL_CLIENT_CERT_VECTOR_LENGTH, 0);
  if(r == 0) {
    /* IDs for use in client */
    (*session_ptr)->next_stream_id = 1;
    (*session_ptr)->last_recv_stream_id = 0;
    (*session_ptr)->next_unique_id = 1;
  }
  return r;
}

int spdylay_session_server_new(spdylay_session **session_ptr,
                               uint16_t version,
                               const spdylay_session_callbacks *callbacks,
                               void *user_data)
{
  int r;
  /* Enable header compression on server side. */
  r = spdylay_session_new(session_ptr, version, callbacks, user_data,
                          0, 1 /* hd_comp */);
  if(r == 0) {
    (*session_ptr)->server = 1;
    /* IDs for use in client */
    (*session_ptr)->next_stream_id = 2;
    (*session_ptr)->last_recv_stream_id = 0;
    (*session_ptr)->next_unique_id = 2;
  }
  return r;
}

static int spdylay_free_streams(spdylay_map_entry *entry, void *ptr)
{
  spdylay_stream_free((spdylay_stream*)entry);
  free(entry);
  return 0;
}

static void spdylay_session_ob_pq_free(spdylay_pq *pq)
{
  while(!spdylay_pq_empty(pq)) {
    spdylay_outbound_item *item = (spdylay_outbound_item*)spdylay_pq_top(pq);
    spdylay_outbound_item_free(item);
    free(item);
    spdylay_pq_pop(pq);
  }
  spdylay_pq_free(pq);
}

static void spdylay_active_outbound_item_reset
(spdylay_active_outbound_item *aob)
{
  spdylay_outbound_item_free(aob->item);
  free(aob->item);
  aob->item = NULL;
  aob->framebuflen = aob->framebufoff = 0;
}

void spdylay_session_del(spdylay_session *session)
{
  if(session == NULL) {
    return;
  }
  spdylay_map_each_free(&session->streams, spdylay_free_streams, NULL);
  spdylay_session_ob_pq_free(&session->ob_pq);
  spdylay_session_ob_pq_free(&session->ob_ss_pq);
  spdylay_zlib_deflate_free(&session->hd_deflater);
  spdylay_zlib_inflate_free(&session->hd_inflater);
  spdylay_active_outbound_item_reset(&session->aob);
  free(session->aob.framebuf);
  free(session->nvbuf);
  spdylay_buffer_free(&session->iframe.inflatebuf);
  free(session->iframe.buf);
  spdylay_client_cert_vector_free(&session->cli_certvec);
  free(session);
}

int spdylay_session_add_frame(spdylay_session *session,
                              spdylay_frame_category frame_cat,
                              void *abs_frame,
                              void *aux_data)
{
  int r = 0;
  spdylay_outbound_item *item;
  item = malloc(sizeof(spdylay_outbound_item));
  if(item == NULL) {
    return SPDYLAY_ERR_NOMEM;
  }
  item->frame_cat = frame_cat;
  item->frame = abs_frame;
  item->aux_data = aux_data;
  item->seq = session->next_seq++;
  /* Set priority lowest at the moment. */
  item->pri = spdylay_session_get_pri_lowest(session);
  if(frame_cat == SPDYLAY_CTRL) {
    spdylay_frame *frame = (spdylay_frame*)abs_frame;
    spdylay_frame_type frame_type = frame->ctrl.hd.type;
    switch(frame_type) {
    case SPDYLAY_SYN_STREAM:
      item->pri = frame->syn_stream.pri;
      break;
    case SPDYLAY_SYN_REPLY: {
      spdylay_stream *stream = spdylay_session_get_stream
        (session, frame->syn_reply.stream_id);
      if(stream) {
        item->pri = stream->pri;
      }
      break;
    }
    case SPDYLAY_RST_STREAM: {
      spdylay_stream *stream = spdylay_session_get_stream
        (session, frame->rst_stream.stream_id);
      if(stream) {
        stream->state = SPDYLAY_STREAM_CLOSING;
        item->pri = stream->pri;
      }
      break;
    }
    case SPDYLAY_SETTINGS:
      /* Should SPDYLAY_SETTINGS have higher priority? */
      item->pri = -1;
      break;
    case SPDYLAY_NOOP:
      /* We don't have any public API to add NOOP, so here is
         unreachable. */
      assert(0);
    case SPDYLAY_PING:
      /* Ping has highest priority. */
      item->pri = SPDYLAY_OB_PRI_PING;
      break;
    case SPDYLAY_GOAWAY:
      /* Should GOAWAY have higher priority? */
      break;
    case SPDYLAY_HEADERS: {
      spdylay_stream *stream = spdylay_session_get_stream
        (session, frame->headers.stream_id);
      if(stream) {
        item->pri = stream->pri;
      }
      break;
    }
    case SPDYLAY_WINDOW_UPDATE: {
      spdylay_stream *stream = spdylay_session_get_stream
        (session, frame->window_update.stream_id);
      if(stream) {
        item->pri = stream->pri;
      }
      break;
    }
    case SPDYLAY_CREDENTIAL:
      item->pri = SPDYLAY_OB_PRI_CREDENTIAL;
      break;
    }
    if(frame_type == SPDYLAY_SYN_STREAM) {
      r = spdylay_pq_push(&session->ob_ss_pq, item);
    } else {
      r = spdylay_pq_push(&session->ob_pq, item);
    }
  } else if(frame_cat == SPDYLAY_DATA) {
    spdylay_data *data_frame = (spdylay_data*)abs_frame;
    spdylay_stream *stream = spdylay_session_get_stream(session,
                                                        data_frame->stream_id);
    if(stream) {
      item->pri = stream->pri;
    }
    r = spdylay_pq_push(&session->ob_pq, item);
  } else {
    /* Unreachable */
    assert(0);
  }
  if(r != 0) {
    free(item);
    return r;
  }
  return 0;
}

int spdylay_session_add_rst_stream(spdylay_session *session,
                                   int32_t stream_id, uint32_t status_code)
{
  int r;
  spdylay_frame *frame;
  frame = malloc(sizeof(spdylay_frame));
  if(frame == NULL) {
    return SPDYLAY_ERR_NOMEM;
  }
  spdylay_frame_rst_stream_init(&frame->rst_stream, session->version,
                                stream_id, status_code);
  r = spdylay_session_add_frame(session, SPDYLAY_CTRL, frame, NULL);
  if(r != 0) {
    spdylay_frame_rst_stream_free(&frame->rst_stream);
    free(frame);
    return r;
  }
  return 0;
}

spdylay_stream* spdylay_session_open_stream(spdylay_session *session,
                                            int32_t stream_id,
                                            uint8_t flags, uint8_t pri,
                                            spdylay_stream_state initial_state,
                                            void *stream_user_data)
{
  int r;
  spdylay_stream *stream = malloc(sizeof(spdylay_stream));
  if(stream == NULL) {
    return NULL;
  }
  spdylay_stream_init(stream, stream_id, flags, pri, initial_state,
                      session->remote_settings
                      [SPDYLAY_SETTINGS_INITIAL_WINDOW_SIZE],
                      stream_user_data);
  r = spdylay_map_insert(&session->streams, &stream->map_entry);
  if(r != 0) {
    free(stream);
    stream = NULL;
  }
  if(spdylay_session_is_my_stream_id(session, stream_id)) {
    ++session->num_outgoing_streams;
  } else {
    ++session->num_incoming_streams;
  }
  return stream;
}

int spdylay_session_close_stream(spdylay_session *session, int32_t stream_id,
                                 spdylay_status_code status_code)
{
  spdylay_stream *stream = spdylay_session_get_stream(session, stream_id);
  if(stream) {
    if(stream->state != SPDYLAY_STREAM_INITIAL &&
       session->callbacks.on_stream_close_callback) {
      session->callbacks.on_stream_close_callback(session, stream_id,
                                                  status_code,
                                                  session->user_data);
    }
    if(spdylay_session_is_my_stream_id(session, stream_id)) {
      --session->num_outgoing_streams;
    } else {
      --session->num_incoming_streams;
    }
    spdylay_map_remove(&session->streams, stream_id);
    spdylay_stream_free(stream);
    free(stream);
    return 0;
  } else {
    return SPDYLAY_ERR_INVALID_ARGUMENT;
  }
}

int spdylay_session_close_stream_if_shut_rdwr(spdylay_session *session,
                                              spdylay_stream *stream)
{
  if((stream->shut_flags & SPDYLAY_SHUT_RDWR) == SPDYLAY_SHUT_RDWR) {
    return spdylay_session_close_stream(session, stream->stream_id,
                                        SPDYLAY_OK);
  } else {
    return 0;
  }
}

void spdylay_session_close_pushed_streams(spdylay_session *session,
                                          int32_t stream_id,
                                          spdylay_status_code status_code)
{
  spdylay_stream *stream;
  stream = spdylay_session_get_stream(session, stream_id);
  if(stream) {
    size_t i;
    for(i = 0; i < stream->pushed_streams_length; ++i) {
      spdylay_session_close_stream(session, stream->pushed_streams[i],
                                   status_code);
    }
  }
}

static int spdylay_predicate_stream_for_send(spdylay_stream *stream)
{
  if(stream == NULL) {
    return SPDYLAY_ERR_STREAM_CLOSED;
  } else if(stream->shut_flags & SPDYLAY_SHUT_WR) {
    return SPDYLAY_ERR_STREAM_SHUT_WR;
  } else {
    return 0;
  }
}

/*
 * This function checks SYN_STREAM frame |frame| can be sent at this
 * time.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * SPDYLAY_ERR_STREAM_CLOSED
 *     The Associated-To-Stream is already closed or does not exist.
 * SPDYLAY_ERR_STREAM_ID_NOT_AVAILABLE
 *     Stream ID has reached the maximum value. Therefore no stream ID
 *     is available.
 */
static int spdylay_session_predicate_syn_stream_send
(spdylay_session *session,
 spdylay_syn_stream *frame)
{
  if(session->goaway_flags) {
    /* When GOAWAY is sent or received, peer must not send new
       SYN_STREAM. */
    return SPDYLAY_ERR_SYN_STREAM_NOT_ALLOWED;
  }
  /* All 32bit signed stream IDs are spent. */
  if(session->next_stream_id > INT32_MAX) {
    return SPDYLAY_ERR_STREAM_ID_NOT_AVAILABLE;
  }
  if(frame->assoc_stream_id != 0) {
    /* Check associated stream is active. */
    /* We assume here that if frame->assoc_stream_id != 0,
       session->server is always 1 and frame->assoc_stream_id is
       odd. */
    if(spdylay_session_get_stream(session, frame->assoc_stream_id) ==
       NULL) {
      return SPDYLAY_ERR_STREAM_CLOSED;
    }
  }
  return 0;
}

/*
 * This function checks SYN_REPLY with the stream ID |stream_id| can
 * be sent at this time.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * SPDYLAY_ERR_STREAM_CLOSED
 *     The stream is already closed or does not exist.
 * SPDYLAY_ERR_STREAM_SHUT_WR
 *     The transmission is not allowed for this stream (e.g., a frame
 *     with FIN flag set has already sent)
 * SPDYLAY_ERR_INVALID_STREAM_ID
 *     The stream ID is invalid.
 * SPDYLAY_ERR_STREAM_CLOSING
 *     RST_STREAM was queued for this stream.
 * SPDYLAY_ERR_INVALID_STREAM_STATE
 *     The state of the stream is not valid (e.g., SYN_REPLY has
 *     already sent).
 */
static int spdylay_session_predicate_syn_reply_send(spdylay_session *session,
                                                    int32_t stream_id)
{
  spdylay_stream *stream = spdylay_session_get_stream(session, stream_id);
  int r;
  r = spdylay_predicate_stream_for_send(stream);
  if(r != 0) {
    return r;
  }
  if(spdylay_session_is_my_stream_id(session, stream_id)) {
    return SPDYLAY_ERR_INVALID_STREAM_ID;
  } else {
    if(stream->state == SPDYLAY_STREAM_OPENING) {
      return 0;
    } else if(stream->state == SPDYLAY_STREAM_CLOSING) {
      return SPDYLAY_ERR_STREAM_CLOSING;
    } else {
      return SPDYLAY_ERR_INVALID_STREAM_STATE;
    }
  }
}

/*
 * This function checks HEADERS with the stream ID |stream_id| can
 * be sent at this time.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * SPDYLAY_ERR_STREAM_CLOSED
 *     The stream is already closed or does not exist.
 * SPDYLAY_ERR_STREAM_SHUT_WR
 *     The transmission is not allowed for this stream (e.g., a frame
 *     with FIN flag set has already sent)
 * SPDYLAY_ERR_STREAM_CLOSING
 *     RST_STREAM was queued for this stream.
 * SPDYLAY_ERR_INVALID_STREAM_STATE
 *     The state of the stream is not valid (e.g., if the local peer
 *     is receiving side and SYN_REPLY has not been sent).
 */
static int spdylay_session_predicate_headers_send(spdylay_session *session,
                                                  int32_t stream_id)
{
  spdylay_stream *stream = spdylay_session_get_stream(session, stream_id);
  int r;
  r = spdylay_predicate_stream_for_send(stream);
  if(r != 0) {
    return r;
  }
  if(spdylay_session_is_my_stream_id(session, stream_id)) {
    if(stream->state != SPDYLAY_STREAM_CLOSING) {
      return 0;
    } else {
      return SPDYLAY_ERR_STREAM_CLOSING;
    }
  } else {
    if(stream->state == SPDYLAY_STREAM_OPENED) {
      return 0;
    } else if(stream->state == SPDYLAY_STREAM_CLOSING) {
      return SPDYLAY_ERR_STREAM_CLOSING;
    } else {
      return SPDYLAY_ERR_INVALID_STREAM_STATE;
    }
  }
}

/*
 * This function checks WINDOW_UPDATE with the stream ID |stream_id|
 * can be sent at this time. Note that FIN flag of the previous frame
 * does not affect the transmission of the WINDOW_UPDATE frame.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * SPDYLAY_ERR_STREAM_CLOSED
 *     The stream is already closed or does not exist.
 * SPDYLAY_ERR_STREAM_CLOSING
 *     RST_STREAM was queued for this stream.
 */
static int spdylay_session_predicate_window_update_send
(spdylay_session *session,
 int32_t stream_id)
{
  spdylay_stream *stream = spdylay_session_get_stream(session, stream_id);
  if(stream == NULL) {
    return SPDYLAY_ERR_STREAM_CLOSED;
  }
  if(stream->state != SPDYLAY_STREAM_CLOSING) {
    return 0;
  } else {
    return SPDYLAY_ERR_STREAM_CLOSING;
  }
}

/*
 * Returns the maximum length of next data read. If the flow control
 * is enabled, the return value takes into account the current window
 * size.
 */
static size_t spdylay_session_next_data_read(spdylay_session *session,
                                             spdylay_stream *stream)
{
  if(session->flow_control == 0) {
    return SPDYLAY_DATA_PAYLOAD_LENGTH;
  } else if(stream->window_size > 0) {
    return stream->window_size < SPDYLAY_DATA_PAYLOAD_LENGTH ?
      stream->window_size : SPDYLAY_DATA_PAYLOAD_LENGTH;
  } else {
    return 0;
  }
}

/*
 * This function checks DATA with the stream ID |stream_id| can be
 * sent at this time.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * SPDYLAY_ERR_STREAM_CLOSED
 *     The stream is already closed or does not exist.
 * SPDYLAY_ERR_STREAM_SHUT_WR
 *     The transmission is not allowed for this stream (e.g., a frame
 *     with FIN flag set has already sent)
 * SPDYLAY_ERR_DEFERRED_DATA_EXIST
 *     Another DATA frame has already been deferred.
 * SPDYLAY_ERR_STREAM_CLOSING
 *     RST_STREAM was queued for this stream.
 * SPDYLAY_ERR_INVALID_STREAM_STATE
 *     The state of the stream is not valid (e.g., if the local peer
 *     is receiving side and SYN_REPLY has not been sent).
 */
static int spdylay_session_predicate_data_send(spdylay_session *session,
                                               int32_t stream_id)
{
  spdylay_stream *stream = spdylay_session_get_stream(session, stream_id);
  int r;
  r = spdylay_predicate_stream_for_send(stream);
  if(r != 0) {
    return r;
  }
  if(stream->deferred_data != NULL) {
    /* stream->deferred_data != NULL means previously queued DATA
       frame has not been sent. We don't allow new DATA frame is sent
       in this case. */
    return SPDYLAY_ERR_DEFERRED_DATA_EXIST;
  }
  if(spdylay_session_is_my_stream_id(session, stream_id)) {
    /* If stream->state is SPDYLAY_STREAM_CLOSING, RST_STREAM was
       queued but not yet sent. In this case, we won't send DATA
       frames. This is because in the current architecture, DATA and
       RST_STREAM in the same stream have same priority and DATA is
       small seq number. So RST_STREAM will not be sent until all DATA
       frames are sent. This is not desirable situation; we want to
       close stream as soon as possible. To achieve this, we remove
       DATA frame before RST_STREAM. */
    if(stream->state != SPDYLAY_STREAM_CLOSING) {
      return 0;
    } else {
      return SPDYLAY_ERR_STREAM_CLOSING;
    }
  } else {
    if(stream->state == SPDYLAY_STREAM_OPENED) {
      return 0;
    } else if(stream->state == SPDYLAY_STREAM_CLOSING) {
      return SPDYLAY_ERR_STREAM_CLOSING;
    } else {
      return SPDYLAY_ERR_INVALID_STREAM_STATE;
    }
  }
}

/*
 * Retrieves cryptographic proof for the given |origin| using callback
 * function and store it in |proof|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * SPDYLAY_ERR_NOMEM
 *     Out of memory.
 */
static int spdylay_session_get_credential_proof(spdylay_session *session,
                                                const spdylay_origin *origin,
                                                spdylay_mem_chunk *proof)
{
  proof->length = session->callbacks.get_credential_proof(session,
                                                          origin,
                                                          NULL, 0,
                                                          session->user_data);
  proof->data = malloc(proof->length);
  if(proof->data == NULL) {
    return SPDYLAY_ERR_NOMEM;
  }
  session->callbacks.get_credential_proof(session, origin,
                                          proof->data, proof->length,
                                          session->user_data);
  return 0;
}

/*
 * Retrieves client certificate chain for the given |origin| using
 * callback functions and store the pointer to the allocated buffer
 * containing certificate chain to |*certs_ptr|. The length of
 * certificate chain is exactly |ncerts|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * SPDYLAY_ERR_NOMEM
 *     Out of memory.
 */
static int spdylay_session_get_credential_cert(spdylay_session *session,
                                               const spdylay_origin *origin,
                                               spdylay_mem_chunk **certs_ptr,
                                               size_t ncerts)
{
  spdylay_mem_chunk *certs;
  size_t i, j;
  certs = malloc(sizeof(spdylay_mem_chunk)*ncerts);
  if(certs == NULL) {
    return SPDYLAY_ERR_NOMEM;
  }
  for(i = 0; i < ncerts; ++i) {
    certs[i].length = session->callbacks.get_credential_cert
      (session,
       origin,
       i, NULL, 0,
       session->user_data);
    certs[i].data = malloc(certs[i].length);
    if(certs[i].data == NULL) {
      goto fail;
    }
    session->callbacks.get_credential_cert(session, origin, i,
                                           certs[i].data, certs[i].length,
                                           session->user_data);
  }
  *certs_ptr = certs;
  return 0;
 fail:
  for(j = 0; j < i; ++j) {
    free(certs[j].data);
  }
  free(certs);
  return SPDYLAY_ERR_NOMEM;
}

int spdylay_session_prep_credential(spdylay_session *session,
                                    spdylay_syn_stream *syn_stream)
{
  int rv;
  spdylay_origin origin;
  spdylay_frame *frame;
  spdylay_mem_chunk proof;
  if(session->cli_certvec.size == 0) {
    return 0;
  }
  rv = spdylay_frame_nv_set_origin(syn_stream->nv, &origin);
  if(rv == 0) {
    size_t slot;
    slot = spdylay_client_cert_vector_find(&session->cli_certvec, &origin);
    if(slot == 0) {
      ssize_t ncerts;
      ncerts = session->callbacks.get_credential_ncerts(session, &origin,
                                                        session->user_data);
      if(ncerts > 0) {
        spdylay_mem_chunk *certs;
        spdylay_origin *origin_copy;
        frame = malloc(sizeof(spdylay_frame));
        if(frame == NULL) {
          return SPDYLAY_ERR_NOMEM;
        }
        origin_copy = malloc(sizeof(spdylay_origin));
        if(origin_copy == NULL) {
          goto fail_after_frame;
        }
        *origin_copy = origin;
        slot = spdylay_client_cert_vector_put(&session->cli_certvec,
                                              origin_copy);

        rv = spdylay_session_get_credential_proof(session, &origin, &proof);
        if(rv != 0) {
          goto fail_after_frame;
        }
        rv = spdylay_session_get_credential_cert(session, &origin,
                                                 &certs, ncerts);
        if(rv != 0) {
          goto fail_after_proof;
        }
        spdylay_frame_credential_init(&frame->credential,
                                      session->version,
                                      slot, &proof, certs, ncerts);
        rv = spdylay_session_add_frame(session, SPDYLAY_CTRL, frame, NULL);
        if(rv != 0) {
          spdylay_frame_credential_free(&frame->credential);
          free(frame);
          return rv;
        }
        return SPDYLAY_ERR_CREDENTIAL_PENDING;
      }
    } else {
      return slot;
    }
  }
  return 0;
 fail_after_proof:
  free(proof.data);
 fail_after_frame:
  free(frame);
  return rv;

}

static ssize_t spdylay_session_prep_frame(spdylay_session *session,
                                          spdylay_outbound_item *item)
{
  ssize_t framebuflen = 0;
  if(item->frame_cat == SPDYLAY_CTRL) {
    spdylay_frame *frame;
    spdylay_frame_type frame_type;
    frame = spdylay_outbound_item_get_ctrl_frame(item);
    frame_type = spdylay_outbound_item_get_ctrl_frame_type(item);
    switch(frame_type) {
    case SPDYLAY_SYN_STREAM: {
      int32_t stream_id;
      spdylay_syn_stream_aux_data *aux_data;
      int r;
      r = spdylay_session_predicate_syn_stream_send(session,
                                                    &frame->syn_stream);
      if(r != 0) {
        return r;
      }
      if(session->version == SPDYLAY_PROTO_SPDY3 &&
         session->callbacks.get_credential_proof &&
         session->callbacks.get_credential_cert &&
         !session->server) {
        int slot_index;
        slot_index = spdylay_session_prep_credential(session,
                                                     &frame->syn_stream);
        if(slot_index == SPDYLAY_ERR_CREDENTIAL_PENDING) {
          /* CREDENTIAL frame has been queued. SYN_STREAM has to be
             sent after that. Change the priority of this item to
             achieve this. */
          item->pri = SPDYLAY_OB_PRI_AFTER_CREDENTIAL;
          r = spdylay_pq_push(&session->ob_ss_pq, item);
          if(r == 0) {
            return SPDYLAY_ERR_CREDENTIAL_PENDING;
          } else {
            return r;
          }
        }
        frame->syn_stream.slot = slot_index;
      }
      stream_id = session->next_stream_id;
      frame->syn_stream.stream_id = stream_id;
      session->next_stream_id += 2;
      if(session->version == SPDYLAY_PROTO_SPDY2) {
        spdylay_frame_nv_3to2(frame->syn_stream.nv);
        spdylay_frame_nv_sort(frame->syn_stream.nv);
      }
      framebuflen = spdylay_frame_pack_syn_stream(&session->aob.framebuf,
                                                  &session->aob.framebufmax,
                                                  &session->nvbuf,
                                                  &session->nvbuflen,
                                                  &frame->syn_stream,
                                                  &session->hd_deflater);
      if(session->version == SPDYLAY_PROTO_SPDY2) {
        spdylay_frame_nv_2to3(frame->syn_stream.nv);
        spdylay_frame_nv_sort(frame->syn_stream.nv);
      }
      if(framebuflen < 0) {
        return framebuflen;
      }
      aux_data = (spdylay_syn_stream_aux_data*)item->aux_data;
      if(spdylay_session_open_stream(session, stream_id,
                                     frame->syn_stream.hd.flags,
                                     frame->syn_stream.pri,
                                     SPDYLAY_STREAM_INITIAL,
                                     aux_data->stream_user_data) == NULL) {
        return SPDYLAY_ERR_NOMEM;
      }
      break;
    }
    case SPDYLAY_SYN_REPLY: {
      int r;
      r = spdylay_session_predicate_syn_reply_send(session,
                                                   frame->syn_reply.stream_id);
      if(r != 0) {
        return r;
      }
      if(session->version == SPDYLAY_PROTO_SPDY2) {
        spdylay_frame_nv_3to2(frame->syn_reply.nv);
        spdylay_frame_nv_sort(frame->syn_reply.nv);
      }
      framebuflen = spdylay_frame_pack_syn_reply(&session->aob.framebuf,
                                                 &session->aob.framebufmax,
                                                 &session->nvbuf,
                                                 &session->nvbuflen,
                                                 &frame->syn_reply,
                                                 &session->hd_deflater);
      if(session->version == SPDYLAY_PROTO_SPDY2) {
        spdylay_frame_nv_2to3(frame->syn_reply.nv);
        spdylay_frame_nv_sort(frame->syn_reply.nv);
      }
      if(framebuflen < 0) {
        return framebuflen;
      }
      break;
    }
    case SPDYLAY_RST_STREAM:
      framebuflen = spdylay_frame_pack_rst_stream(&session->aob.framebuf,
                                                  &session->aob.framebufmax,
                                                  &frame->rst_stream);
      if(framebuflen < 0) {
        return framebuflen;
      }
      break;
    case SPDYLAY_SETTINGS:
      framebuflen = spdylay_frame_pack_settings(&session->aob.framebuf,
                                                &session->aob.framebufmax,
                                                &frame->settings);
      if(framebuflen < 0) {
        return framebuflen;
      }
      break;
    case SPDYLAY_NOOP:
      /* We don't have any public API to add NOOP, so here is
         unreachable. */
      assert(0);
    case SPDYLAY_PING:
      framebuflen = spdylay_frame_pack_ping(&session->aob.framebuf,
                                            &session->aob.framebufmax,
                                            &frame->ping);
      if(framebuflen < 0) {
        return framebuflen;
      }
      break;
    case SPDYLAY_HEADERS: {
      int r;
      r = spdylay_session_predicate_headers_send(session,
                                                 frame->headers.stream_id);
      if(r != 0) {
        return r;
      }
      if(session->version == SPDYLAY_PROTO_SPDY2) {
        spdylay_frame_nv_3to2(frame->headers.nv);
        spdylay_frame_nv_sort(frame->headers.nv);
      }
      framebuflen = spdylay_frame_pack_headers(&session->aob.framebuf,
                                               &session->aob.framebufmax,
                                               &session->nvbuf,
                                               &session->nvbuflen,
                                               &frame->headers,
                                               &session->hd_deflater);
      if(session->version == SPDYLAY_PROTO_SPDY2) {
        spdylay_frame_nv_2to3(frame->headers.nv);
        spdylay_frame_nv_sort(frame->headers.nv);
      }
      if(framebuflen < 0) {
        return framebuflen;
      }
      break;
    }
    case SPDYLAY_WINDOW_UPDATE: {
      int r;
      r = spdylay_session_predicate_window_update_send
        (session, frame->window_update.stream_id);
      if(r != 0) {
        return r;
      }
      framebuflen = spdylay_frame_pack_window_update(&session->aob.framebuf,
                                                     &session->aob.framebufmax,
                                                     &frame->window_update);
      if(framebuflen < 0) {
        return framebuflen;
      }
      break;
    }
    case SPDYLAY_GOAWAY:
      if(session->goaway_flags & SPDYLAY_GOAWAY_SEND) {
        /* TODO The spec does not mandate that both endpoints have to
           exchange GOAWAY. This implementation allows receiver of first
           GOAWAY can sent its own GOAWAY to tell the remote peer that
           last-good-stream-id. */
        return SPDYLAY_ERR_GOAWAY_ALREADY_SENT;
      }
      framebuflen = spdylay_frame_pack_goaway(&session->aob.framebuf,
                                              &session->aob.framebufmax,
                                              &frame->goaway);
      if(framebuflen < 0) {
        return framebuflen;
      }
      break;
    case SPDYLAY_CREDENTIAL: {
      framebuflen = spdylay_frame_pack_credential(&session->aob.framebuf,
                                                  &session->aob.framebufmax,
                                                  &frame->credential);
      if(framebuflen < 0) {
        return framebuflen;
      }
      break;
    }
    default:
      framebuflen = SPDYLAY_ERR_INVALID_ARGUMENT;
    }
  } else if(item->frame_cat == SPDYLAY_DATA) {
    size_t next_readmax;
    spdylay_stream *stream;
    spdylay_data *data_frame;
    int r;
    data_frame = spdylay_outbound_item_get_data_frame(item);
    r = spdylay_session_predicate_data_send(session, data_frame->stream_id);
    if(r != 0) {
      return r;
    }
    stream = spdylay_session_get_stream(session, data_frame->stream_id);
    /* Assuming stream is not NULL */
    assert(stream);
    next_readmax = spdylay_session_next_data_read(session, stream);
    if(next_readmax == 0) {
      spdylay_stream_defer_data(stream, item, SPDYLAY_DEFERRED_FLOW_CONTROL);
      return SPDYLAY_ERR_DEFERRED;
    }
    framebuflen = spdylay_session_pack_data(session,
                                            &session->aob.framebuf,
                                            &session->aob.framebufmax,
                                            next_readmax,
                                            data_frame);
    if(framebuflen == SPDYLAY_ERR_DEFERRED) {
      spdylay_stream_defer_data(stream, item, SPDYLAY_DEFERRED_NONE);
      return SPDYLAY_ERR_DEFERRED;
    } else if(framebuflen == SPDYLAY_ERR_TEMPORAL_CALLBACK_FAILURE) {
      r = spdylay_session_add_rst_stream(session, data_frame->stream_id,
                                         SPDYLAY_INTERNAL_ERROR);
      if(r == 0) {
        return framebuflen;
      } else {
        return r;
      }
    } else if(framebuflen < 0) {
      return framebuflen;
    }
  } else {
    /* Unreachable */
    assert(0);
  }
  return framebuflen;
}

spdylay_outbound_item* spdylay_session_get_ob_pq_top
(spdylay_session *session)
{
  return (spdylay_outbound_item*)spdylay_pq_top(&session->ob_pq);
}

spdylay_outbound_item* spdylay_session_get_next_ob_item
(spdylay_session *session)
{
  if(spdylay_pq_empty(&session->ob_pq)) {
    if(spdylay_pq_empty(&session->ob_ss_pq)) {
      return NULL;
    } else {
      /* Return item only when concurrent connection limit is not
         reached */
      if(spdylay_session_is_outgoing_concurrent_streams_max(session)) {
        return NULL;
      } else {
        return spdylay_pq_top(&session->ob_ss_pq);
      }
    }
  } else {
    if(spdylay_pq_empty(&session->ob_ss_pq)) {
      return spdylay_pq_top(&session->ob_pq);
    } else {
      spdylay_outbound_item *item, *syn_stream_item;
      item = spdylay_pq_top(&session->ob_pq);
      syn_stream_item = spdylay_pq_top(&session->ob_ss_pq);
      if(spdylay_session_is_outgoing_concurrent_streams_max(session) ||
         item->pri < syn_stream_item->pri ||
         (item->pri == syn_stream_item->pri &&
          item->seq < syn_stream_item->seq)) {
        return item;
      } else {
        return syn_stream_item;
      }
    }
  }
}

spdylay_outbound_item* spdylay_session_pop_next_ob_item
(spdylay_session *session)
{
  if(spdylay_pq_empty(&session->ob_pq)) {
    if(spdylay_pq_empty(&session->ob_ss_pq)) {
      return NULL;
    } else {
      /* Pop item only when concurrent connection limit is not
         reached */
      if(spdylay_session_is_outgoing_concurrent_streams_max(session)) {
        return NULL;
      } else {
        spdylay_outbound_item *item;
        item = spdylay_pq_top(&session->ob_ss_pq);
        spdylay_pq_pop(&session->ob_ss_pq);
        return item;
      }
    }
  } else {
    if(spdylay_pq_empty(&session->ob_ss_pq)) {
      spdylay_outbound_item *item;
      item = spdylay_pq_top(&session->ob_pq);
      spdylay_pq_pop(&session->ob_pq);
      return item;
    } else {
      spdylay_outbound_item *item, *syn_stream_item;
      item = spdylay_pq_top(&session->ob_pq);
      syn_stream_item = spdylay_pq_top(&session->ob_ss_pq);
      if(spdylay_session_is_outgoing_concurrent_streams_max(session) ||
         item->pri < syn_stream_item->pri ||
         (item->pri == syn_stream_item->pri &&
          item->seq < syn_stream_item->seq)) {
        spdylay_pq_pop(&session->ob_pq);
        return item;
      } else {
        spdylay_pq_pop(&session->ob_ss_pq);
        return syn_stream_item;
      }
    }
  }
}

/*
 * Called after a frame is sent.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * SPDYLAY_ERR_NOMEM
 *     Out of memory.
 * SPDYLAY_ERR_CALLBACK_FAILURE
 *     The callback function failed.
 */
static int spdylay_session_after_frame_sent(spdylay_session *session)
{
  /* TODO handle FIN flag. */
  spdylay_outbound_item *item = session->aob.item;
  if(item->frame_cat == SPDYLAY_CTRL) {
    spdylay_frame *frame;
    spdylay_frame_type type;
    frame = spdylay_outbound_item_get_ctrl_frame(session->aob.item);
    type = spdylay_outbound_item_get_ctrl_frame_type(session->aob.item);
    if(session->callbacks.on_ctrl_send_callback) {
      session->callbacks.on_ctrl_send_callback
        (session, type, frame, session->user_data);
    }
    switch(type) {
    case SPDYLAY_SYN_STREAM: {
      spdylay_stream *stream =
        spdylay_session_get_stream(session, frame->syn_stream.stream_id);
      if(stream) {
        spdylay_syn_stream_aux_data *aux_data;
        stream->state = SPDYLAY_STREAM_OPENING;
        if(frame->syn_stream.hd.flags & SPDYLAY_CTRL_FLAG_FIN) {
          spdylay_stream_shutdown(stream, SPDYLAY_SHUT_WR);
        }
        if(frame->syn_stream.hd.flags & SPDYLAY_CTRL_FLAG_UNIDIRECTIONAL) {
          spdylay_stream_shutdown(stream, SPDYLAY_SHUT_RD);
        }
        spdylay_session_close_stream_if_shut_rdwr(session, stream);
        /* We assume aux_data is a pointer to spdylay_syn_stream_aux_data */
        aux_data = (spdylay_syn_stream_aux_data*)item->aux_data;
        if(aux_data->data_prd) {
          int r;
          /* spdylay_submit_data() makes a copy of aux_data->data_prd */
          r = spdylay_submit_data(session, frame->syn_stream.stream_id,
                                  SPDYLAY_DATA_FLAG_FIN, aux_data->data_prd);
          if(r != 0) {
            /* FATAL error */
            assert(r < SPDYLAY_ERR_FATAL);
            /* TODO If r is not FATAL, we should send RST_STREAM. */
            return r;
          }
        }
      }
      break;
    }
    case SPDYLAY_SYN_REPLY: {
      spdylay_stream *stream =
        spdylay_session_get_stream(session, frame->syn_reply.stream_id);
      if(stream) {
        stream->state = SPDYLAY_STREAM_OPENED;
        if(frame->syn_reply.hd.flags & SPDYLAY_CTRL_FLAG_FIN) {
          spdylay_stream_shutdown(stream, SPDYLAY_SHUT_WR);
        }
        spdylay_session_close_stream_if_shut_rdwr(session, stream);
        if(item->aux_data) {
          /* We assume aux_data is a pointer to spdylay_data_provider */
          spdylay_data_provider *data_prd =
            (spdylay_data_provider*)item->aux_data;
          int r;
          r = spdylay_submit_data(session, frame->syn_reply.stream_id,
                                  SPDYLAY_DATA_FLAG_FIN, data_prd);
          if(r != 0) {
            /* FATAL error */
            assert(r < SPDYLAY_ERR_FATAL);
            /* TODO If r is not FATAL, we should send RST_STREAM. */
            return r;
          }
        }
      }
      break;
    }
    case SPDYLAY_RST_STREAM:
      if(!session->server &&
         spdylay_session_is_my_stream_id(session,
                                         frame->rst_stream.stream_id) &&
         frame->rst_stream.status_code == SPDYLAY_CANCEL) {
        spdylay_session_close_pushed_streams(session,
                                             frame->rst_stream.stream_id,
                                             frame->rst_stream.status_code);
      }
      spdylay_session_close_stream(session, frame->rst_stream.stream_id,
                                   frame->rst_stream.status_code);
      break;
    case SPDYLAY_SETTINGS:
      /* nothing to do */
      break;
    case SPDYLAY_NOOP:
      /* We don't have any public API to add NOOP, so here is
         unreachable. */
      assert(0);
    case SPDYLAY_PING:
      /* We record the time now and show application code RTT when
         reply PING is received. */
      session->last_ping_unique_id = frame->ping.unique_id;
      break;
    case SPDYLAY_GOAWAY:
      session->goaway_flags |= SPDYLAY_GOAWAY_SEND;
      break;
    case SPDYLAY_HEADERS: {
      spdylay_stream *stream =
        spdylay_session_get_stream(session, frame->headers.stream_id);
      if(stream) {
        if(frame->headers.hd.flags & SPDYLAY_CTRL_FLAG_FIN) {
          spdylay_stream_shutdown(stream, SPDYLAY_SHUT_WR);
        }
        spdylay_session_close_stream_if_shut_rdwr(session, stream);
      }
      break;
    }
    case SPDYLAY_WINDOW_UPDATE:
      break;
    case SPDYLAY_CREDENTIAL:
      break;
    }
    spdylay_active_outbound_item_reset(&session->aob);
  } else if(item->frame_cat == SPDYLAY_DATA) {
    int r;
    spdylay_data *data_frame;
    data_frame = spdylay_outbound_item_get_data_frame(session->aob.item);
    if(session->callbacks.on_data_send_callback) {
      session->callbacks.on_data_send_callback
        (session,
         data_frame->eof ? data_frame->flags :
         (data_frame->flags & (~SPDYLAY_DATA_FLAG_FIN)),
         data_frame->stream_id,
         session->aob.framebuflen-SPDYLAY_HEAD_LEN, session->user_data);
    }
    if(data_frame->eof && (data_frame->flags & SPDYLAY_DATA_FLAG_FIN)) {
      spdylay_stream *stream =
        spdylay_session_get_stream(session, data_frame->stream_id);
      if(stream) {
        spdylay_stream_shutdown(stream, SPDYLAY_SHUT_WR);
        spdylay_session_close_stream_if_shut_rdwr(session, stream);
      }
    }
    /* If session is closed or RST_STREAM was queued, we won't send
       further data. */
    if(data_frame->eof ||
       spdylay_session_predicate_data_send(session,
                                           data_frame->stream_id) != 0) {
      spdylay_active_outbound_item_reset(&session->aob);
    } else {
      spdylay_outbound_item* next_item;
      next_item = spdylay_session_get_next_ob_item(session);
      /* If priority of this stream is higher or equal to other stream
         waiting at the top of the queue, we continue to send this
         data. */
      if(next_item == NULL || session->aob.item->pri <= next_item->pri) {
        size_t next_readmax;
        spdylay_stream *stream;
        stream = spdylay_session_get_stream(session, data_frame->stream_id);
        /* Assuming stream is not NULL */
        assert(stream);
        next_readmax = spdylay_session_next_data_read(session, stream);
        if(next_readmax == 0) {
          spdylay_stream_defer_data(stream, session->aob.item,
                                    SPDYLAY_DEFERRED_FLOW_CONTROL);
          session->aob.item = NULL;
          spdylay_active_outbound_item_reset(&session->aob);
          return 0;
        }
        r = spdylay_session_pack_data(session,
                                      &session->aob.framebuf,
                                      &session->aob.framebufmax,
                                      next_readmax,
                                      data_frame);
        if(r == SPDYLAY_ERR_DEFERRED) {
          spdylay_stream_defer_data(stream, session->aob.item,
                                    SPDYLAY_DEFERRED_NONE);
          session->aob.item = NULL;
          spdylay_active_outbound_item_reset(&session->aob);
        } else if(r == SPDYLAY_ERR_TEMPORAL_CALLBACK_FAILURE) {
          /* Stop DATA frame chain and issue RST_STREAM to close the
             stream.  We don't return
             SPDYLAY_ERR_TEMPORAL_CALLBACK_FAILURE intentionally. */
          r = spdylay_session_add_rst_stream(session, data_frame->stream_id,
                                             SPDYLAY_INTERNAL_ERROR);
          spdylay_active_outbound_item_reset(&session->aob);
          if(r != 0) {
            return r;
          }
        } else if(r < 0) {
          /* In this context, r is either SPDYLAY_ERR_NOMEM or
             SPDYLAY_ERR_CALLBACK_FAILURE */
          spdylay_active_outbound_item_reset(&session->aob);
          return r;
        } else {
          session->aob.framebuflen = r;
          session->aob.framebufoff = 0;
        }
      } else {
        r = spdylay_pq_push(&session->ob_pq, session->aob.item);
        if(r == 0) {
          session->aob.item = NULL;
          spdylay_active_outbound_item_reset(&session->aob);
        } else {
          /* FATAL error */
          assert(r < SPDYLAY_ERR_FATAL);
          spdylay_active_outbound_item_reset(&session->aob);
          return r;
        }
      }
    }
  } else {
    /* Unreachable */
    assert(0);
  }
  return 0;
}

int spdylay_session_send(spdylay_session *session)
{
  int r;
  while(1) {
    const uint8_t *data;
    size_t datalen;
    ssize_t sentlen;
    if(session->aob.item == NULL) {
      spdylay_outbound_item *item;
      ssize_t framebuflen;
      item = spdylay_session_pop_next_ob_item(session);
      if(item == NULL) {
        break;
      }
      framebuflen = spdylay_session_prep_frame(session, item);
      if(framebuflen == SPDYLAY_ERR_DEFERRED ||
         framebuflen == SPDYLAY_ERR_CREDENTIAL_PENDING) {
        continue;
      } else if(framebuflen < 0) {
        if(item->frame_cat == SPDYLAY_CTRL &&
           session->callbacks.on_ctrl_not_send_callback &&
           spdylay_is_non_fatal(framebuflen)) {
          /* The library is responsible for the transmission of
             WINDOW_UPDATE frame, so we don't call error callback for
             it. */
          spdylay_frame_type frame_type;
          frame_type = spdylay_outbound_item_get_ctrl_frame_type(item);
          if(frame_type != SPDYLAY_WINDOW_UPDATE) {
            session->callbacks.on_ctrl_not_send_callback
              (session,
               frame_type,
               spdylay_outbound_item_get_ctrl_frame(item),
               framebuflen,
               session->user_data);
          }
        }
        spdylay_outbound_item_free(item);
        free(item);
        if(spdylay_is_fatal(framebuflen)) {
          return framebuflen;
        } else {
          continue;
        }
      }
      session->aob.item = item;
      session->aob.framebuflen = framebuflen;
      /* Call before_send callback */
      if(item->frame_cat == SPDYLAY_CTRL &&
         session->callbacks.before_ctrl_send_callback) {
        session->callbacks.before_ctrl_send_callback
          (session,
           spdylay_outbound_item_get_ctrl_frame_type(item),
           spdylay_outbound_item_get_ctrl_frame(item),
           session->user_data);
      }
    }
    data = session->aob.framebuf + session->aob.framebufoff;
    datalen = session->aob.framebuflen - session->aob.framebufoff;
    sentlen = session->callbacks.send_callback(session, data, datalen, 0,
                                               session->user_data);
    if(sentlen < 0) {
      if(sentlen == SPDYLAY_ERR_WOULDBLOCK) {
        return 0;
      } else {
        return SPDYLAY_ERR_CALLBACK_FAILURE;
      }
    } else {
      session->aob.framebufoff += sentlen;
      if(session->flow_control &&
         session->aob.item->frame_cat == SPDYLAY_DATA) {
        spdylay_data *frame;
        spdylay_stream *stream;
        frame = spdylay_outbound_item_get_data_frame(session->aob.item);
        stream = spdylay_session_get_stream(session, frame->stream_id);
        if(stream) {
          stream->window_size -= spdylay_get_uint32(&session->aob.framebuf[4]);
        }
      }
      if(session->aob.framebufoff == session->aob.framebuflen) {
        /* Frame has completely sent */
        r = spdylay_session_after_frame_sent(session);
        if(r < 0) {
          /* FATAL */
          assert(r < SPDYLAY_ERR_FATAL);
          return r;
        }
      }
    }
  }
  return 0;
}

static ssize_t spdylay_recv(spdylay_session *session, uint8_t *buf, size_t len)
{
  ssize_t r;
  r = session->callbacks.recv_callback
    (session, buf, len, 0, session->user_data);
  if(r > 0) {
    if((size_t)r > len) {
      return SPDYLAY_ERR_CALLBACK_FAILURE;
    }
  } else if(r < 0) {
    if(r != SPDYLAY_ERR_WOULDBLOCK && r != SPDYLAY_ERR_EOF) {
      r = SPDYLAY_ERR_CALLBACK_FAILURE;
    }
  }
  return r;
}

static void spdylay_session_call_on_request_recv
(spdylay_session *session, int32_t stream_id)
{
  if(session->callbacks.on_request_recv_callback) {
    session->callbacks.on_request_recv_callback(session, stream_id,
                                                session->user_data);
  }
}

static void spdylay_session_call_on_ctrl_frame_received
(spdylay_session *session, spdylay_frame_type type, spdylay_frame *frame)
{
  if(session->callbacks.on_ctrl_recv_callback) {
    session->callbacks.on_ctrl_recv_callback
      (session, type, frame, session->user_data);
  }
}

/*
 * Checks whether received stream_id is valid.
 * This function returns 1 if it succeeds, or 0.
 */
static int spdylay_session_is_new_peer_stream_id(spdylay_session *session,
                                                 int32_t stream_id)
{
  if(stream_id == 0) {
    return 0;
  }
  if(session->server) {
    return stream_id % 2 == 1 && session->last_recv_stream_id < stream_id;
  } else {
    return stream_id % 2 == 0 && session->last_recv_stream_id < stream_id;
  }
}

/*
 * Returns non-zero iff version == session->version
 */
static int spdylay_session_check_version(spdylay_session *session,
                                         uint16_t version)
{
  return session->version == version;
}

/*
 * Validates received SYN_STREAM frame |frame|.  This function returns
 * 0 if it succeeds, or non-zero spdylay_status_code.
 */
static int spdylay_session_validate_syn_stream(spdylay_session *session,
                                               spdylay_syn_stream *frame)
{
  if(!spdylay_session_check_version(session, frame->hd.version)) {
    return SPDYLAY_UNSUPPORTED_VERSION;
  }
  if(session->server) {
    if(frame->assoc_stream_id != 0) {
      return SPDYLAY_PROTOCOL_ERROR;
    }
  } else {
    if(frame->assoc_stream_id == 0) {
      /* spdy/2 spec: When a client receives a SYN_STREAM from the
         server with an Associated-To-Stream-ID of 0, it must reply with
         a RST_STREAM with error code INVALID_STREAM. */
      return SPDYLAY_INVALID_STREAM;
    }
    if((frame->hd.flags & SPDYLAY_CTRL_FLAG_UNIDIRECTIONAL) == 0 ||
       frame->assoc_stream_id % 2 == 0 ||
       spdylay_session_get_stream(session, frame->assoc_stream_id) == NULL) {
      /* It seems spdy/2 spec does not say which status code should be
         returned in these cases. */
      return SPDYLAY_PROTOCOL_ERROR;
    }
  }
  if(spdylay_session_is_incoming_concurrent_streams_max(session)) {
    /* spdy/2 spec does not clearly say what to do when max concurrent
       streams number is reached. The mod_spdy sends
       SPDYLAY_REFUSED_STREAM and we think it is reasonable. So we
       follow it. */
    return SPDYLAY_REFUSED_STREAM;
  }
  return 0;
}


static int spdylay_session_handle_invalid_stream
(spdylay_session *session,
 int32_t stream_id,
 spdylay_frame_type type,
 spdylay_frame *frame,
 spdylay_status_code status_code)
{
  int r;
  r = spdylay_session_add_rst_stream(session, stream_id, status_code);
  if(r != 0) {
    return r;
  }
  if(session->callbacks.on_invalid_ctrl_recv_callback) {
    session->callbacks.on_invalid_ctrl_recv_callback
      (session, type, frame, status_code, session->user_data);
  }
  return 0;
}

int spdylay_session_on_syn_stream_received(spdylay_session *session,
                                           spdylay_frame *frame)
{
  int r = 0;
  int status_code;
  if(session->goaway_flags) {
    /* We don't accept SYN_STREAM after GOAWAY is sent or received. */
    return 0;
  }
  if(session->last_recv_stream_id == frame->syn_stream.stream_id) {
    /* SPDY/3 spec says if an endpoint receives same stream ID twice,
       it MUST issue a stream error with status code
       PROTOCOL_ERROR. */
    status_code = SPDYLAY_PROTOCOL_ERROR;
  } else if(!spdylay_session_is_new_peer_stream_id
            (session, frame->syn_stream.stream_id)) {
    /* SPDY/3 spec says if an endpoint receives a SYN_STREAM with a
       stream ID which is less than any previously received
       SYN_STREAM, it MUST issue a session error with status
       PROTOCOL_ERROR */
    if(session->callbacks.on_invalid_ctrl_recv_callback) {
      session->callbacks.on_invalid_ctrl_recv_callback(session,
                                                       SPDYLAY_SYN_STREAM,
                                                       frame,
                                                       SPDYLAY_PROTOCOL_ERROR,
                                                       session->user_data);
    }
    return spdylay_session_fail_session(session, SPDYLAY_GOAWAY_PROTOCOL_ERROR);
  } else {
    session->last_recv_stream_id = frame->syn_stream.stream_id;
    status_code = spdylay_session_validate_syn_stream(session,
                                                      &frame->syn_stream);
  }
  if(status_code == 0) {
    uint8_t flags = frame->syn_stream.hd.flags;
    if((flags & SPDYLAY_CTRL_FLAG_FIN) &&
       (flags & SPDYLAY_CTRL_FLAG_UNIDIRECTIONAL)) {
      /* If the stream is UNIDIRECTIONAL and FIN bit set, we can close
         stream upon receiving SYN_STREAM. So, the stream needs not to
         be opened. */
    } else {
      spdylay_stream *stream;
      stream = spdylay_session_open_stream(session, frame->syn_stream.stream_id,
                                           frame->syn_stream.hd.flags,
                                           frame->syn_stream.pri,
                                           SPDYLAY_STREAM_OPENING,
                                           NULL);
      if(stream) {
        if(flags & SPDYLAY_CTRL_FLAG_FIN) {
          spdylay_stream_shutdown(stream, SPDYLAY_SHUT_RD);
        }
        if(flags & SPDYLAY_CTRL_FLAG_UNIDIRECTIONAL) {
          spdylay_stream_shutdown(stream, SPDYLAY_SHUT_WR);
        }
        /* We don't call spdylay_session_close_stream_if_shut_rdwr()
           here because either SPDYLAY_CTRL_FLAG_FIN or
           SPDYLAY_CTRL_FLAG_UNIDIRECTIONAL is not set here. */
      }
    }
    spdylay_session_call_on_ctrl_frame_received(session, SPDYLAY_SYN_STREAM,
                                                frame);
    if(flags & SPDYLAY_CTRL_FLAG_FIN) {
      spdylay_session_call_on_request_recv(session,
                                           frame->syn_stream.stream_id);
      if(flags & SPDYLAY_CTRL_FLAG_UNIDIRECTIONAL) {
        /* Note that we call on_stream_close_callback without opening
           stream. */
        if(session->callbacks.on_stream_close_callback) {
          session->callbacks.on_stream_close_callback
            (session, frame->syn_stream.stream_id, SPDYLAY_OK,
             session->user_data);
        }
      }
    }
  } else {
    r = spdylay_session_handle_invalid_stream
      (session, frame->syn_stream.stream_id, SPDYLAY_SYN_STREAM, frame,
       status_code);
  }
  return r;
}

int spdylay_session_on_syn_reply_received(spdylay_session *session,
                                          spdylay_frame *frame)
{
  int r = 0;
  int valid = 0;
  int status_code = SPDYLAY_PROTOCOL_ERROR;
  spdylay_stream *stream;
  if(!spdylay_session_check_version(session, frame->syn_reply.hd.version)) {
    return 0;
  }
  if((stream = spdylay_session_get_stream(session,
                                          frame->syn_reply.stream_id)) &&
     (stream->shut_flags & SPDYLAY_SHUT_RD) == 0) {
    if(spdylay_session_is_my_stream_id(session, frame->syn_reply.stream_id)) {
      if(stream->state == SPDYLAY_STREAM_OPENING) {
        valid = 1;
        stream->state = SPDYLAY_STREAM_OPENED;
        spdylay_session_call_on_ctrl_frame_received(session, SPDYLAY_SYN_REPLY,
                                                    frame);
        if(frame->syn_reply.hd.flags & SPDYLAY_CTRL_FLAG_FIN) {
          /* This is the last frame of this stream, so disallow
             further receptions. */
          spdylay_stream_shutdown(stream, SPDYLAY_SHUT_RD);
          spdylay_session_close_stream_if_shut_rdwr(session, stream);
        }
      } else if(stream->state == SPDYLAY_STREAM_CLOSING) {
        /* This is race condition. SPDYLAY_STREAM_CLOSING indicates
           that we queued RST_STREAM but it has not been sent. It will
           eventually sent, so we just ignore this frame. */
        valid = 1;
      } else {
        if(session->version == SPDYLAY_PROTO_SPDY3) {
          /* SPDY/3 spec says if multiple SYN_REPLY frames for the
             same active stream ID are received, the receiver must
             issue a stream error with the status code
             STREAM_IN_USE. */
          status_code = SPDYLAY_STREAM_IN_USE;
        }
      }
    }
  }
  if(!valid) {
    r = spdylay_session_handle_invalid_stream
      (session, frame->syn_reply.stream_id, SPDYLAY_SYN_REPLY, frame,
       status_code);
  }
  return r;
}

int spdylay_session_on_rst_stream_received(spdylay_session *session,
                                           spdylay_frame *frame)
{
  if(!spdylay_session_check_version(session, frame->rst_stream.hd.version)) {
    return 0;
  }
  spdylay_session_call_on_ctrl_frame_received(session, SPDYLAY_RST_STREAM,
                                              frame);
  if(session->server &&
     !spdylay_session_is_my_stream_id(session, frame->rst_stream.stream_id) &&
     frame->rst_stream.status_code == SPDYLAY_CANCEL) {
    spdylay_session_close_pushed_streams(session, frame->rst_stream.stream_id,
                                         frame->rst_stream.status_code);
  }
  spdylay_session_close_stream(session, frame->rst_stream.stream_id,
                               frame->rst_stream.status_code);
  return 0;
}

static int spdylay_update_initial_window_size_func(spdylay_map_entry *entry,
                                                   void *ptr)
{
  spdylay_update_window_size_arg *arg;
  spdylay_stream *stream;
  arg = (spdylay_update_window_size_arg*)ptr;
  stream = (spdylay_stream*)entry;
  spdylay_stream_update_initial_window_size(stream,
                                            arg->new_window_size,
                                            arg->old_window_size);
  /* If window size gets positive, push deferred DATA frame to
     outbound queue. */
  if(stream->window_size > 0 &&
     stream->deferred_data &&
     (stream->deferred_flags & SPDYLAY_DEFERRED_FLOW_CONTROL)) {
    int rv;
    rv = spdylay_pq_push(&arg->session->ob_pq, stream->deferred_data);
    if(rv == 0) {
      spdylay_stream_detach_deferred_data(stream);
    } else {
      /* FATAL */
      assert(rv < SPDYLAY_ERR_FATAL);
      return rv;
    }
  }
  return 0;
}

/*
 * Updates the initial window size of all active streams.
 * If error occurs, all streams may not be updated.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * SPDYLAY_ERR_NOMEM
 *     Out of memory.
 */
static int spdylay_session_update_initial_window_size
(spdylay_session *session,
 int32_t new_initial_window_size)
{
  spdylay_update_window_size_arg arg;
  arg.session = session;
  arg.new_window_size = new_initial_window_size;
  arg.old_window_size =
    session->remote_settings[SPDYLAY_SETTINGS_INITIAL_WINDOW_SIZE];
  return spdylay_map_each(&session->streams,
                          spdylay_update_initial_window_size_func,
                          &arg);
}

void spdylay_session_update_local_settings(spdylay_session *session,
                                           spdylay_settings_entry *iv,
                                           size_t niv)
{
  size_t i;
  for(i = 0; i < niv; ++i) {
    assert(iv[i].settings_id > 0 && iv[i].settings_id <= SPDYLAY_SETTINGS_MAX);
    session->local_settings[iv[i].settings_id] = iv[i].value;
  }
}

int spdylay_session_on_settings_received(spdylay_session *session,
                                         spdylay_frame *frame)
{
  int rv;
  size_t i;
  int check[SPDYLAY_SETTINGS_MAX+1];
  if(!spdylay_session_check_version(session, frame->settings.hd.version)) {
    return 0;
  }
  /* Check ID/value pairs and persist them if necessary. */
  memset(check, 0, sizeof(check));
  for(i = 0; i < frame->settings.niv; ++i) {
    spdylay_settings_entry *entry = &frame->settings.iv[i];
    /* SPDY/3 spec says if the multiple values for the same ID were
       found, use the first one and ignore the rest. */
    if(entry->settings_id > SPDYLAY_SETTINGS_MAX || entry->settings_id == 0 ||
       check[entry->settings_id] == 1) {
      continue;
    }
    check[entry->settings_id] = 1;
    if(entry->settings_id == SPDYLAY_SETTINGS_INITIAL_WINDOW_SIZE &&
       session->flow_control) {
      /* Update the initial window size of the all active streams */
      /* Check that initial_window_size < (1u << 31) */
      if(entry->value < (1u << 31)) {
        rv = spdylay_session_update_initial_window_size(session, entry->value);
        if(rv != 0) {
          return rv;
        }
      }
    } else if(entry->settings_id ==
              SPDYLAY_SETTINGS_CLIENT_CERTIFICATE_VECTOR_SIZE) {
      if(!session->server) {
        /* Limit certificate vector length in the reasonable size. */
        entry->value = spdylay_min(entry->value,
                                   SPDYLAY_MAX_CLIENT_CERT_VECTOR_LENGTH);
        rv = spdylay_client_cert_vector_resize(&session->cli_certvec,
                                               entry->value);
        if(rv != 0) {
          return rv;
        }
      }
    }
    session->remote_settings[entry->settings_id] = entry->value;
  }
  spdylay_session_call_on_ctrl_frame_received(session, SPDYLAY_SETTINGS, frame);
  return 0;
}

int spdylay_session_on_ping_received(spdylay_session *session,
                                     spdylay_frame *frame)
{
  int r = 0;
  if(!spdylay_session_check_version(session, frame->ping.hd.version)) {
    return 0;
  }
  if(frame->ping.unique_id != 0) {
    if(session->last_ping_unique_id == frame->ping.unique_id) {
      /* This is ping reply from peer */
      /* Assign 0 to last_ping_unique_id so that we can ignore same
         ID. */
      session->last_ping_unique_id = 0;
      spdylay_session_call_on_ctrl_frame_received(session, SPDYLAY_PING, frame);
    } else if((session->server && frame->ping.unique_id % 2 == 1) ||
              (!session->server && frame->ping.unique_id % 2 == 0)) {
      /* Peer sent ping, so ping it back */
      r = spdylay_session_add_ping(session, frame->ping.unique_id);
      spdylay_session_call_on_ctrl_frame_received(session, SPDYLAY_PING, frame);
    }
  }
  return r;
}

int spdylay_session_on_goaway_received(spdylay_session *session,
                                       spdylay_frame *frame)
{
  if(!spdylay_session_check_version(session, frame->goaway.hd.version)) {
    return 0;
  }
  session->last_good_stream_id = frame->goaway.last_good_stream_id;
  session->goaway_flags |= SPDYLAY_GOAWAY_RECV;
  spdylay_session_call_on_ctrl_frame_received(session, SPDYLAY_GOAWAY, frame);
  return 0;
}

int spdylay_session_on_window_update_received(spdylay_session *session,
                                              spdylay_frame *frame)
{
  spdylay_stream *stream;
  if(!spdylay_session_check_version(session, frame->window_update.hd.version)) {
    return 0;
  }
  if(!session->flow_control) {
    return 0;
  }
  stream = spdylay_session_get_stream(session, frame->window_update.stream_id);
  if(stream) {
    if(INT32_MAX-frame->window_update.delta_window_size < stream->window_size) {
      int r;
      r = spdylay_session_handle_invalid_stream
        (session, frame->window_update.stream_id, SPDYLAY_WINDOW_UPDATE, frame,
         SPDYLAY_FLOW_CONTROL_ERROR);
      return r;
    } else {
      stream->window_size += frame->window_update.delta_window_size;
      if(stream->window_size > 0 &&
         stream->deferred_data != NULL &&
         (stream->deferred_flags & SPDYLAY_DEFERRED_FLOW_CONTROL)) {
        int r;
        r = spdylay_pq_push(&session->ob_pq, stream->deferred_data);
        if(r == 0) {
          spdylay_stream_detach_deferred_data(stream);
        } else if(r < 0) {
          /* FATAL */
          assert(r < SPDYLAY_ERR_FATAL);
          return r;
        }
      }
      spdylay_session_call_on_ctrl_frame_received(session,
                                                  SPDYLAY_WINDOW_UPDATE, frame);
    }
  }
  return 0;
}

int spdylay_session_on_credential_received(spdylay_session *session,
                                           spdylay_frame *frame)
{
  if(!spdylay_session_check_version(session, frame->credential.hd.version)) {
    return 0;
  }
  /* We don't care about the body of the CREDENTIAL frame. It is left
     to the application code to decide it is invalid or not. */
  spdylay_session_call_on_ctrl_frame_received(session, SPDYLAY_CREDENTIAL,
                                              frame);
  return 0;
}

int spdylay_session_on_headers_received(spdylay_session *session,
                                        spdylay_frame *frame)
{
  int r = 0;
  int valid = 0;
  spdylay_stream *stream;
  if(!spdylay_session_check_version(session, frame->headers.hd.version)) {
    return 0;
  }
  if((stream = spdylay_session_get_stream(session,
                                          frame->headers.stream_id)) &&
     (stream->shut_flags & SPDYLAY_SHUT_RD) == 0) {
    if(spdylay_session_is_my_stream_id(session, frame->headers.stream_id)) {
      if(stream->state == SPDYLAY_STREAM_OPENED) {
        valid = 1;
        spdylay_session_call_on_ctrl_frame_received(session, SPDYLAY_HEADERS,
                                                    frame);
        if(frame->headers.hd.flags & SPDYLAY_CTRL_FLAG_FIN) {
          spdylay_stream_shutdown(stream, SPDYLAY_SHUT_RD);
          spdylay_session_close_stream_if_shut_rdwr(session, stream);
        }
      } else if(stream->state == SPDYLAY_STREAM_CLOSING) {
        /* This is race condition. SPDYLAY_STREAM_CLOSING indicates
           that we queued RST_STREAM but it has not been sent. It will
           eventually sent, so we just ignore this frame. */
        valid = 1;
      }
    } else {
      /* If this is remote peer initiated stream, it is OK unless it
         have sent FIN frame already. But if stream is in
         SPDYLAY_STREAM_CLOSING, we discard the frame. This is a race
         condition. */
      valid = 1;
      if(stream->state != SPDYLAY_STREAM_CLOSING) {
        spdylay_session_call_on_ctrl_frame_received(session, SPDYLAY_HEADERS,
                                                    frame);
        if(frame->headers.hd.flags & SPDYLAY_CTRL_FLAG_FIN) {
          spdylay_session_call_on_request_recv(session,
                                               frame->headers.stream_id);
          spdylay_stream_shutdown(stream, SPDYLAY_SHUT_RD);
          spdylay_session_close_stream_if_shut_rdwr(session, stream);
        }
      }
    }
  }
  if(!valid) {
    r = spdylay_session_handle_invalid_stream
      (session, frame->headers.stream_id, SPDYLAY_HEADERS, frame,
       SPDYLAY_PROTOCOL_ERROR);
  }
  return r;
}

static void spdylay_session_handle_parse_error(spdylay_session *session,
                                               spdylay_frame_type type,
                                               int error_code)
{
  if(session->callbacks.on_ctrl_recv_parse_error_callback) {
    session->callbacks.on_ctrl_recv_parse_error_callback
      (session,
       type,
       session->iframe.headbuf,
       sizeof(session->iframe.headbuf),
       session->iframe.buf,
       session->iframe.buflen,
       error_code,
       session->user_data);
  }
}

static int spdylay_get_status_code_from_error_code(int error_code)
{
  switch(error_code) {
  case(SPDYLAY_ERR_FRAME_TOO_LARGE):
    return SPDYLAY_FRAME_TOO_LARGE;
  default:
    return SPDYLAY_PROTOCOL_ERROR;
  }
}

/* For errors, this function only returns FATAL error. */
static int spdylay_session_process_ctrl_frame(spdylay_session *session)
{
  int r = 0;
  uint16_t type;
  spdylay_frame frame;
  type = spdylay_get_uint16(&session->iframe.headbuf[2]);
  switch(type) {
  case SPDYLAY_SYN_STREAM:
    if(session->iframe.error_code == 0) {
      r = spdylay_frame_unpack_syn_stream(&frame.syn_stream,
                                          session->iframe.headbuf,
                                          sizeof(session->iframe.headbuf),
                                          session->iframe.buf,
                                          session->iframe.buflen,
                                          &session->iframe.inflatebuf);
    } else if(session->iframe.error_code == SPDYLAY_ERR_FRAME_TOO_LARGE) {
      r = spdylay_frame_unpack_syn_stream_without_nv
        (&frame.syn_stream,
         session->iframe.headbuf, sizeof(session->iframe.headbuf),
         session->iframe.buf, session->iframe.buflen);
      if(r == 0) {
        r = session->iframe.error_code;
      }
    } else {
      r = session->iframe.error_code;
    }
    if(r == 0) {
      if(session->version == SPDYLAY_PROTO_SPDY2) {
        spdylay_frame_nv_2to3(frame.syn_stream.nv);
      }
      r = spdylay_session_on_syn_stream_received(session, &frame);
      spdylay_frame_syn_stream_free(&frame.syn_stream);
    } else if(r == SPDYLAY_ERR_INVALID_HEADER_BLOCK ||
              r == SPDYLAY_ERR_FRAME_TOO_LARGE) {
      r = spdylay_session_handle_invalid_stream
        (session, frame.syn_stream.stream_id, SPDYLAY_SYN_STREAM, &frame,
         spdylay_get_status_code_from_error_code(r));
      spdylay_frame_syn_stream_free(&frame.syn_stream);
    } else if(spdylay_is_non_fatal(r)) {
      spdylay_session_handle_parse_error(session, type, r);
      r = spdylay_session_fail_session(session, SPDYLAY_GOAWAY_PROTOCOL_ERROR);
    }
    break;
  case SPDYLAY_SYN_REPLY:
    if(session->iframe.error_code == 0) {
      r = spdylay_frame_unpack_syn_reply(&frame.syn_reply,
                                         session->iframe.headbuf,
                                         sizeof(session->iframe.headbuf),
                                         session->iframe.buf,
                                         session->iframe.buflen,
                                         &session->iframe.inflatebuf);
    } else if(session->iframe.error_code == SPDYLAY_ERR_FRAME_TOO_LARGE) {
      r = spdylay_frame_unpack_syn_reply_without_nv
        (&frame.syn_reply,
         session->iframe.headbuf, sizeof(session->iframe.headbuf),
         session->iframe.buf, session->iframe.buflen);
      if(r == 0) {
        r = session->iframe.error_code;
      }
    } else {
      r = session->iframe.error_code;
    }
    if(r == 0) {
      if(session->version == SPDYLAY_PROTO_SPDY2) {
        spdylay_frame_nv_2to3(frame.syn_reply.nv);
      }
      r = spdylay_session_on_syn_reply_received(session, &frame);
      spdylay_frame_syn_reply_free(&frame.syn_reply);
    } else if(r == SPDYLAY_ERR_INVALID_HEADER_BLOCK ||
              r == SPDYLAY_ERR_FRAME_TOO_LARGE) {
      r = spdylay_session_handle_invalid_stream
        (session, frame.syn_reply.stream_id, SPDYLAY_SYN_REPLY, &frame,
         spdylay_get_status_code_from_error_code(r));
      spdylay_frame_syn_reply_free(&frame.syn_reply);
    } else if(spdylay_is_non_fatal(r)) {
      spdylay_session_handle_parse_error(session, type, r);
      r = spdylay_session_fail_session(session, SPDYLAY_GOAWAY_PROTOCOL_ERROR);
    }
    break;
  case SPDYLAY_RST_STREAM:
    r = spdylay_frame_unpack_rst_stream(&frame.rst_stream,
                                        session->iframe.headbuf,
                                        sizeof(session->iframe.headbuf),
                                        session->iframe.buf,
                                        session->iframe.buflen);
    if(r == 0) {
      r = spdylay_session_on_rst_stream_received(session, &frame);
      spdylay_frame_rst_stream_free(&frame.rst_stream);
    } else if(spdylay_is_non_fatal(r)) {
      spdylay_session_handle_parse_error(session, type, r);
      r = spdylay_session_fail_session(session, SPDYLAY_GOAWAY_PROTOCOL_ERROR);
    }
    break;
  case SPDYLAY_SETTINGS:
    r = spdylay_frame_unpack_settings(&frame.settings,
                                      session->iframe.headbuf,
                                      sizeof(session->iframe.headbuf),
                                      session->iframe.buf,
                                      session->iframe.buflen);
    if(r == 0) {
      r = spdylay_session_on_settings_received(session, &frame);
      spdylay_frame_settings_free(&frame.settings);
    } else if(spdylay_is_non_fatal(r)) {
      spdylay_session_handle_parse_error(session, type, r);
      r = spdylay_session_fail_session(session, SPDYLAY_GOAWAY_PROTOCOL_ERROR);
    }
    break;
  case SPDYLAY_NOOP:
    break;
  case SPDYLAY_PING:
    r = spdylay_frame_unpack_ping(&frame.ping,
                                  session->iframe.headbuf,
                                  sizeof(session->iframe.headbuf),
                                  session->iframe.buf,
                                  session->iframe.buflen);
    if(r == 0) {
      r = spdylay_session_on_ping_received(session, &frame);
      spdylay_frame_ping_free(&frame.ping);
    } else if(spdylay_is_non_fatal(r)) {
      spdylay_session_handle_parse_error(session, type, r);
      r = spdylay_session_fail_session(session, SPDYLAY_GOAWAY_PROTOCOL_ERROR);
    }
    break;
  case SPDYLAY_GOAWAY:
    r = spdylay_frame_unpack_goaway(&frame.goaway,
                                    session->iframe.headbuf,
                                    sizeof(session->iframe.headbuf),
                                    session->iframe.buf,
                                    session->iframe.buflen);
    if(r == 0) {
      r = spdylay_session_on_goaway_received(session, &frame);
      spdylay_frame_goaway_free(&frame.goaway);
    } else if(spdylay_is_non_fatal(r)) {
      spdylay_session_handle_parse_error(session, type, r);
      r = spdylay_session_fail_session(session, SPDYLAY_GOAWAY_PROTOCOL_ERROR);
    }
    break;
  case SPDYLAY_HEADERS:
    if(session->iframe.error_code == 0) {
      r = spdylay_frame_unpack_headers(&frame.headers,
                                       session->iframe.headbuf,
                                       sizeof(session->iframe.headbuf),
                                       session->iframe.buf,
                                       session->iframe.buflen,
                                       &session->iframe.inflatebuf);
    } else if(session->iframe.error_code == SPDYLAY_ERR_FRAME_TOO_LARGE) {
      r = spdylay_frame_unpack_headers_without_nv
        (&frame.headers,
         session->iframe.headbuf, sizeof(session->iframe.headbuf),
         session->iframe.buf, session->iframe.buflen);
      if(r == 0) {
        r = session->iframe.error_code;
      }
    } else {
      r = session->iframe.error_code;
    }
    if(r == 0) {
      if(session->version == SPDYLAY_PROTO_SPDY2) {
        spdylay_frame_nv_2to3(frame.headers.nv);
      }
      r = spdylay_session_on_headers_received(session, &frame);
      spdylay_frame_headers_free(&frame.headers);
    } else if(r == SPDYLAY_ERR_INVALID_HEADER_BLOCK ||
              r == SPDYLAY_ERR_FRAME_TOO_LARGE) {
      r = spdylay_session_handle_invalid_stream
        (session, frame.headers.stream_id, SPDYLAY_HEADERS, &frame,
         spdylay_get_status_code_from_error_code(r));
      spdylay_frame_headers_free(&frame.headers);
    } else if(spdylay_is_non_fatal(r)) {
      spdylay_session_handle_parse_error(session, type, r);
      r = spdylay_session_fail_session(session, SPDYLAY_GOAWAY_PROTOCOL_ERROR);
    }
    break;
  case SPDYLAY_WINDOW_UPDATE:
    r = spdylay_frame_unpack_window_update(&frame.window_update,
                                           session->iframe.headbuf,
                                           sizeof(session->iframe.headbuf),
                                           session->iframe.buf,
                                           session->iframe.buflen);
    if(r == 0) {
      r = spdylay_session_on_window_update_received(session, &frame);
      spdylay_frame_window_update_free(&frame.window_update);
    } else if(spdylay_is_non_fatal(r)) {
      spdylay_session_handle_parse_error(session, type, r);
      r = spdylay_session_fail_session(session, SPDYLAY_GOAWAY_PROTOCOL_ERROR);
    }
    break;
  case SPDYLAY_CREDENTIAL:
    r = spdylay_frame_unpack_credential(&frame.credential,
                                        session->iframe.headbuf,
                                        sizeof(session->iframe.headbuf),
                                        session->iframe.buf,
                                        session->iframe.buflen);
    if(r == 0) {
      r = spdylay_session_on_credential_received(session, &frame);
      spdylay_frame_credential_free(&frame.credential);
    } else if(spdylay_is_non_fatal(r)) {
      spdylay_session_handle_parse_error(session, type, r);
      r = spdylay_session_fail_session(session, SPDYLAY_GOAWAY_PROTOCOL_ERROR);
    }
    break;
  default:
    /* Unknown frame */
    if(session->callbacks.on_unknown_ctrl_recv_callback) {
      session->callbacks.on_unknown_ctrl_recv_callback
        (session,
         session->iframe.headbuf,
         sizeof(session->iframe.headbuf),
         session->iframe.buf,
         session->iframe.buflen,
         session->user_data);
    }
  }
  if(spdylay_is_fatal(r)) {
    return r;
  } else {
    return 0;
  }
}

int spdylay_session_on_data_received(spdylay_session *session,
                                     uint8_t flags, int32_t length,
                                     int32_t stream_id)
{
  int r = 0;
  spdylay_status_code status_code = 0;
  spdylay_stream *stream;
  stream = spdylay_session_get_stream(session, stream_id);
  if(stream) {
    if((stream->shut_flags & SPDYLAY_SHUT_RD) == 0) {
      int valid = 0;
      if(spdylay_session_is_my_stream_id(session, stream_id)) {
        if(stream->state == SPDYLAY_STREAM_OPENED) {
          valid = 1;
          if(session->callbacks.on_data_recv_callback) {
            session->callbacks.on_data_recv_callback
              (session, flags, stream_id, length, session->user_data);
          }
        } else if(stream->state != SPDYLAY_STREAM_CLOSING) {
          status_code = SPDYLAY_PROTOCOL_ERROR;
        }
      } else if(stream->state != SPDYLAY_STREAM_CLOSING) {
        /* It is OK if this is remote peer initiated stream and we did
           not receive FIN unless stream is in SPDYLAY_STREAM_CLOSING
           state. This is a race condition. */
        valid = 1;
        if(session->callbacks.on_data_recv_callback) {
          session->callbacks.on_data_recv_callback
            (session, flags, stream_id, length, session->user_data);
        }
        if(flags & SPDYLAY_DATA_FLAG_FIN) {
          spdylay_session_call_on_request_recv(session, stream_id);
        }
      }
      if(valid) {
        if(flags & SPDYLAY_DATA_FLAG_FIN) {
          spdylay_stream_shutdown(stream, SPDYLAY_SHUT_RD);
          spdylay_session_close_stream_if_shut_rdwr(session, stream);
        }
      }
    } else {
      status_code = SPDYLAY_PROTOCOL_ERROR;
    }
  } else {
    status_code = SPDYLAY_INVALID_STREAM;
  }
  if(status_code != 0) {
    r = spdylay_session_add_rst_stream(session, stream_id, status_code);
  }
  return r;
}

/* For errors, this function only returns FATAL error. */
static int spdylay_session_process_data_frame(spdylay_session *session)
{
  uint8_t flags;
  int32_t length;
  int32_t stream_id;
  int r;
  stream_id = spdylay_get_uint32(session->iframe.headbuf) &
    SPDYLAY_STREAM_ID_MASK;
  flags = session->iframe.headbuf[4];
  length = spdylay_get_uint32(&session->iframe.headbuf[4]) &
    SPDYLAY_LENGTH_MASK;
  r = spdylay_session_on_data_received(session, flags, length, stream_id);
  if(spdylay_is_fatal(r)) {
    return r;
  } else {
    return 0;
  }
}

/*
 * Accumulates received bytes |delta_size| and decides whether to send
 * WINDOW_UPDATE. If SPDYLAY_OPT_NO_AUTO_WINDOW_UPDATE is set,
 * WINDOW_UPDATE will not be sent.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * SPDYLAY_ERR_NOMEM
 *     Out of memory.
 */
static int spdylay_session_update_recv_window_size(spdylay_session *session,
                                                   int32_t stream_id,
                                                   int32_t delta_size)
{
  spdylay_stream *stream;
  stream = spdylay_session_get_stream(session, stream_id);
  if(stream) {
    /* If SPDYLAY_OPT_NO_AUTO_WINDOW_UPDATE is set and the application
       does not send WINDOW_UPDATE and the remote endpoint keeps
       sending data, stream->recv_window_size will eventually
       overflow. */
    if(stream->recv_window_size > INT32_MAX - delta_size) {
      stream->recv_window_size = INT32_MAX;
    } else {
      stream->recv_window_size += delta_size;
    }
    if(!(session->opt_flags & SPDYLAY_OPTMASK_NO_AUTO_WINDOW_UPDATE)) {
      /* This is just a heuristics. */
      /* We have to use local_settings here because it is the constraint
         the remote endpoint should honor. */
      if((size_t)stream->recv_window_size*2 >=
         session->local_settings[SPDYLAY_SETTINGS_INITIAL_WINDOW_SIZE]) {
        int r;
        r = spdylay_session_add_window_update(session, stream_id,
                                              stream->recv_window_size);
        if(r == 0) {
          stream->recv_window_size = 0;
        } else {
          return r;
        }
      }
    }
  }
  return 0;
}

/*
 * Returns nonzero if the reception of DATA for stream |stream_id| is
 * allowed.
 */
static int spdylay_session_check_data_recv_allowed(spdylay_session *session,
                                                   int32_t stream_id)
{
  spdylay_stream *stream;
  stream = spdylay_session_get_stream(session, stream_id);
  if(stream) {
    if((stream->shut_flags & SPDYLAY_SHUT_RD) == 0) {
      if(spdylay_session_is_my_stream_id(session, stream_id)) {
        if(stream->state == SPDYLAY_STREAM_OPENED) {
          return 1;
        }
      } else if(stream->state != SPDYLAY_STREAM_CLOSING) {
        /* It is OK if this is remote peer initiated stream and we did
           not receive FIN unless stream is in SPDYLAY_STREAM_CLOSING
           state. This is a race condition. */
        return 1;
      }
    }
  }
  return 0;
}

ssize_t spdylay_session_mem_recv(spdylay_session *session,
                                 const uint8_t *in, size_t inlen)
{
  const uint8_t *inmark, *inlimit;
  inmark = in;
  inlimit = in+inlen;
  while(1) {
    ssize_t r;
    if(session->iframe.state == SPDYLAY_RECV_HEAD) {
      size_t remheadbytes;
      size_t readlen;
      size_t bufavail = inlimit-inmark;
      if(bufavail == 0) {
        break;
      }
      remheadbytes = SPDYLAY_HEAD_LEN-session->iframe.headbufoff;
      readlen = spdylay_min(remheadbytes, bufavail);
      memcpy(session->iframe.headbuf+session->iframe.headbufoff,
             inmark, readlen);
      inmark += readlen;
      session->iframe.headbufoff += readlen;
      if(session->iframe.headbufoff == SPDYLAY_HEAD_LEN) {
        session->iframe.state = SPDYLAY_RECV_PAYLOAD;
        session->iframe.payloadlen =
          spdylay_get_uint32(&session->iframe.headbuf[4]) &
          SPDYLAY_LENGTH_MASK;
        if(spdylay_frame_is_ctrl_frame(session->iframe.headbuf[0])) {
          /* control frame */
          ssize_t buflen;
          buflen = spdylay_inbound_frame_payload_nv_offset(&session->iframe);
          if(buflen == -1) {
            /* Check if payloadlen is small enough for buffering */
            if(session->iframe.payloadlen > session->max_recv_ctrl_frame_buf) {
              session->iframe.error_code = SPDYLAY_ERR_FRAME_TOO_LARGE;
              session->iframe.state = SPDYLAY_RECV_PAYLOAD_IGN;
              buflen = 0;
            } else {
              buflen = session->iframe.payloadlen;
            }
          } else if(buflen < (ssize_t)session->iframe.payloadlen) {
            if(session->iframe.payloadlen > session->max_recv_ctrl_frame_buf) {
              session->iframe.error_code = SPDYLAY_ERR_FRAME_TOO_LARGE;
            }
            /* We are going to receive payload even if the receiving
               frame is too large to synchronize zlib context. For
               name/value header block, we will just burn zlib cycle
               and discard outputs. */
            session->iframe.state = SPDYLAY_RECV_PAYLOAD_PRE_NV;
          }
          /* buflen >= session->iframe.payloadlen means frame is
             malformed. In this case, we just buffer these bytes and
             handle error later. */
          session->iframe.buflen = buflen;
          r = spdylay_reserve_buffer(&session->iframe.buf,
                                     &session->iframe.bufmax,
                                     buflen);
          if(r != 0) {
            /* FATAL */
            assert(r < SPDYLAY_ERR_FATAL);
            return r;
          }
        } else {
          /* Check stream is open. If it is not open or closing,
             ignore payload. */
          int32_t stream_id;
          stream_id = spdylay_get_uint32(session->iframe.headbuf);
          if(!spdylay_session_check_data_recv_allowed(session, stream_id)) {
            session->iframe.state = SPDYLAY_RECV_PAYLOAD_IGN;
          }
        }
      } else {
        break;
      }
    }
    if(session->iframe.state == SPDYLAY_RECV_PAYLOAD ||
       session->iframe.state == SPDYLAY_RECV_PAYLOAD_PRE_NV ||
       session->iframe.state == SPDYLAY_RECV_PAYLOAD_NV ||
       session->iframe.state == SPDYLAY_RECV_PAYLOAD_IGN) {
      size_t rempayloadlen;
      size_t bufavail, readlen;
      int32_t data_stream_id = 0;
      uint8_t data_flags = SPDYLAY_DATA_FLAG_NONE;

      rempayloadlen = session->iframe.payloadlen - session->iframe.off;
      bufavail = inlimit - inmark;
      if(rempayloadlen > 0 && bufavail == 0) {
        break;
      }
      readlen =  spdylay_min(bufavail, rempayloadlen);
      if(session->iframe.state == SPDYLAY_RECV_PAYLOAD_PRE_NV) {
        size_t pnvlen, rpnvlen, readpnvlen;
        pnvlen = spdylay_inbound_frame_payload_nv_offset(&session->iframe);
        rpnvlen = pnvlen - session->iframe.off;
        readpnvlen = spdylay_min(rpnvlen, readlen);

        memcpy(session->iframe.buf+session->iframe.off, inmark, readpnvlen);
        readlen -= readpnvlen;
        session->iframe.off += readpnvlen;
        inmark += readpnvlen;

        if(session->iframe.off == pnvlen) {
          session->iframe.state = SPDYLAY_RECV_PAYLOAD_NV;
        }
      }
      if(session->iframe.state == SPDYLAY_RECV_PAYLOAD_NV) {
        /* For frame with name/value header block, the compressed
           portion of the block is incrementally decompressed. The
           result is stored in inflatebuf. */
        if(session->iframe.error_code == 0 ||
           session->iframe.error_code == SPDYLAY_ERR_FRAME_TOO_LARGE) {
          ssize_t decomplen;
          if(session->iframe.error_code == SPDYLAY_ERR_FRAME_TOO_LARGE) {
            spdylay_buffer_reset(&session->iframe.inflatebuf);
          }
          decomplen = spdylay_zlib_inflate_hd(&session->hd_inflater,
                                              &session->iframe.inflatebuf,
                                              inmark, readlen);
          if(decomplen < 0) {
            /* We are going to overwrite error_code here if it is
               already set. But it is fine because the only possible
               nonzero error code here is SPDYLAY_ERR_FRAME_TOO_LARGE
               and zlib/fatal error can override it. */
            session->iframe.error_code = decomplen;
          } else if(spdylay_buffer_length(&session->iframe.inflatebuf)
                    > session->max_recv_ctrl_frame_buf) {
            /* If total length in inflatebuf exceeds certain limit,
               set TOO_LARGE_FRAME to error_code and issue RST_STREAM
               later. */
            session->iframe.error_code = SPDYLAY_ERR_FRAME_TOO_LARGE;
          }
        }
      } else if(spdylay_frame_is_ctrl_frame(session->iframe.headbuf[0])) {
        if(session->iframe.state != SPDYLAY_RECV_PAYLOAD_IGN) {
          memcpy(session->iframe.buf+session->iframe.off, inmark, readlen);
        }
      } else {
        /* For data frame, We don't buffer data. Instead, just pass
           received data to callback function. */
        data_stream_id = spdylay_get_uint32(session->iframe.headbuf) &
          SPDYLAY_STREAM_ID_MASK;
        data_flags = session->iframe.headbuf[4];
        if(session->iframe.state != SPDYLAY_RECV_PAYLOAD_IGN) {
          if(session->callbacks.on_data_chunk_recv_callback) {
            session->callbacks.on_data_chunk_recv_callback(session,
                                                           data_flags,
                                                           data_stream_id,
                                                           inmark,
                                                           readlen,
                                                           session->user_data);
          }
        }
      }
      session->iframe.off += readlen;
      inmark += readlen;

      if(session->flow_control &&
         session->iframe.state != SPDYLAY_RECV_PAYLOAD_IGN &&
         !spdylay_frame_is_ctrl_frame(session->iframe.headbuf[0])) {
        if(readlen > 0 &&
           (session->iframe.payloadlen != session->iframe.off ||
            (data_flags & SPDYLAY_DATA_FLAG_FIN) == 0)) {
          r = spdylay_session_update_recv_window_size(session,
                                                      data_stream_id,
                                                      readlen);
          if(r < 0) {
            /* FATAL */
            assert(r < SPDYLAY_ERR_FATAL);
            return r;
          }
        }
      }
      if(session->iframe.payloadlen == session->iframe.off) {
        if(spdylay_frame_is_ctrl_frame(session->iframe.headbuf[0])) {
          r = spdylay_session_process_ctrl_frame(session);
        } else {
          r = spdylay_session_process_data_frame(session);
        }
        if(r < 0) {
          /* FATAL */
          assert(r < SPDYLAY_ERR_FATAL);
          return r;
        }
        spdylay_inbound_frame_reset(&session->iframe);
      }
    }
  }
  return inmark-in;
}

int spdylay_session_recv(spdylay_session *session)
{
  uint8_t buf[SPDYLAY_INBOUND_BUFFER_LENGTH];
  while(1) {
    ssize_t readlen;
    readlen = spdylay_recv(session, buf, sizeof(buf));
    if(readlen > 0) {
      ssize_t proclen = spdylay_session_mem_recv(session, buf, readlen);
      if(proclen < 0) {
        return proclen;
      }
      assert(proclen == readlen);
    } else if(readlen == 0 || readlen == SPDYLAY_ERR_WOULDBLOCK) {
      return 0;
    } else if(readlen == SPDYLAY_ERR_EOF) {
      return readlen;
    } else if(readlen < 0) {
      return SPDYLAY_ERR_CALLBACK_FAILURE;
    }
  }
}

int spdylay_session_want_read(spdylay_session *session)
{
  /* If these flags are set, we don't want to read. The application
     should drop the connection. */
  if((session->goaway_flags & SPDYLAY_GOAWAY_FAIL_ON_SEND) &&
     (session->goaway_flags & SPDYLAY_GOAWAY_SEND)) {
    return 0;
  }
  /* Unless GOAWAY is sent or received, we always want to read
     incoming frames. After GOAWAY is sent or received, we are only
     interested in active streams. */
  return !session->goaway_flags || spdylay_map_size(&session->streams) > 0;
}

int spdylay_session_want_write(spdylay_session *session)
{
  /* If these flags are set, we don't want to write any data. The
     application should drop the connection. */
  if((session->goaway_flags & SPDYLAY_GOAWAY_FAIL_ON_SEND) &&
     (session->goaway_flags & SPDYLAY_GOAWAY_SEND)) {
    return 0;
  }
  /*
   * Unless GOAWAY is sent or received, we want to write frames if
   * there is pending ones. If pending frame is SYN_STREAM and
   * concurrent stream limit is reached, we don't want to write
   * SYN_STREAM.  After GOAWAY is sent or received, we want to write
   * frames if there is pending ones AND there are active frames.
   */
  return (session->aob.item != NULL || !spdylay_pq_empty(&session->ob_pq) ||
          (!spdylay_pq_empty(&session->ob_ss_pq) &&
           !spdylay_session_is_outgoing_concurrent_streams_max(session))) &&
    (!session->goaway_flags || spdylay_map_size(&session->streams) > 0);
}

int spdylay_session_add_ping(spdylay_session *session, uint32_t unique_id)
{
  int r;
  spdylay_frame *frame;
  frame = malloc(sizeof(spdylay_frame));
  if(frame == NULL) {
    return SPDYLAY_ERR_NOMEM;
  }
  spdylay_frame_ping_init(&frame->ping, session->version, unique_id);
  r = spdylay_session_add_frame(session, SPDYLAY_CTRL, frame, NULL);
  if(r != 0) {
    spdylay_frame_ping_free(&frame->ping);
    free(frame);
  }
  return r;
}

int spdylay_session_add_goaway(spdylay_session *session,
                               int32_t last_good_stream_id,
                               uint32_t status_code)
{
  int r;
  spdylay_frame *frame;
  frame = malloc(sizeof(spdylay_frame));
  if(frame == NULL) {
    return SPDYLAY_ERR_NOMEM;
  }
  spdylay_frame_goaway_init(&frame->goaway, session->version,
                            last_good_stream_id, status_code);
  r = spdylay_session_add_frame(session, SPDYLAY_CTRL, frame, NULL);
  if(r != 0) {
    spdylay_frame_goaway_free(&frame->goaway);
    free(frame);
  }
  return r;
}

int spdylay_session_add_window_update(spdylay_session *session,
                                      int32_t stream_id,
                                      int32_t delta_window_size)
{
  int r;
  spdylay_frame *frame;
  frame = malloc(sizeof(spdylay_frame));
  if(frame == NULL) {
    return SPDYLAY_ERR_NOMEM;
  }
  spdylay_frame_window_update_init(&frame->window_update, session->version,
                                   stream_id, delta_window_size);
  r = spdylay_session_add_frame(session, SPDYLAY_CTRL, frame, NULL);
  if(r != 0) {
    spdylay_frame_window_update_free(&frame->window_update);
    free(frame);
  }
  return r;
}

ssize_t spdylay_session_pack_data(spdylay_session *session,
                                  uint8_t **buf_ptr, size_t *buflen_ptr,
                                  size_t datamax,
                                  spdylay_data *frame)
{
  ssize_t framelen = datamax+8, r;
  int eof_flags;
  uint8_t flags;
  r = spdylay_reserve_buffer(buf_ptr, buflen_ptr, framelen);
  if(r != 0) {
    return r;
  }
  eof_flags = 0;
  r = frame->data_prd.read_callback
    (session, frame->stream_id, (*buf_ptr)+8, datamax,
     &eof_flags, &frame->data_prd.source, session->user_data);
  if(r == SPDYLAY_ERR_DEFERRED || r == SPDYLAY_ERR_TEMPORAL_CALLBACK_FAILURE) {
    return r;
  } else if(r < 0 || datamax < (size_t)r) {
    /* This is the error code when callback is failed. */
    return SPDYLAY_ERR_CALLBACK_FAILURE;
  }
  memset(*buf_ptr, 0, SPDYLAY_HEAD_LEN);
  spdylay_put_uint32be(&(*buf_ptr)[0], frame->stream_id);
  spdylay_put_uint32be(&(*buf_ptr)[4], r);
  flags = 0;
  if(eof_flags) {
    frame->eof = 1;
    if(frame->flags & SPDYLAY_DATA_FLAG_FIN) {
      flags |= SPDYLAY_DATA_FLAG_FIN;
    }
  }
  (*buf_ptr)[4] = flags;
  return r+8;
}

uint32_t spdylay_session_get_next_unique_id(spdylay_session *session)
{
  uint32_t ret_id;
  if(session->next_unique_id > SPDYLAY_MAX_UNIQUE_ID) {
    if(session->server) {
      session->next_unique_id = 2;
    } else {
      session->next_unique_id = 1;
    }
  }
  ret_id = session->next_unique_id;
  session->next_unique_id += 2;
  return ret_id;
}

void* spdylay_session_get_stream_user_data(spdylay_session *session,
                                           int32_t stream_id)
{
  spdylay_stream *stream;
  stream = spdylay_session_get_stream(session, stream_id);
  if(stream) {
    return stream->stream_user_data;
  } else {
    return NULL;
  }
}

int spdylay_session_resume_data(spdylay_session *session, int32_t stream_id)
{
  int r;
  spdylay_stream *stream;
  stream = spdylay_session_get_stream(session, stream_id);
  if(stream == NULL || stream->deferred_data == NULL ||
     (stream->deferred_flags & SPDYLAY_DEFERRED_FLOW_CONTROL)) {
    return SPDYLAY_ERR_INVALID_ARGUMENT;
  }
  r = spdylay_pq_push(&session->ob_pq, stream->deferred_data);
  if(r == 0) {
    spdylay_stream_detach_deferred_data(stream);
  }
  return r;
}

uint8_t spdylay_session_get_pri_lowest(spdylay_session *session)
{
  if(session->version == SPDYLAY_PROTO_SPDY2) {
    return SPDYLAY_PRI_LOWEST_SPDY2;
  } else if(session->version == SPDYLAY_PROTO_SPDY3) {
    return SPDYLAY_PRI_LOWEST_SPDY3;
  } else {
    return 0;
  }
}

size_t spdylay_session_get_outbound_queue_size(spdylay_session *session)
{
  return spdylay_pq_size(&session->ob_pq)+spdylay_pq_size(&session->ob_ss_pq);
}

int spdylay_session_set_initial_client_cert_origin(spdylay_session *session,
                                                   const char *scheme,
                                                   const char *host,
                                                   uint16_t port)
{
  spdylay_origin *origin;
  size_t slot;
  if(strlen(scheme) > SPDYLAY_MAX_SCHEME ||
     strlen(host) > SPDYLAY_MAX_HOSTNAME) {
    return SPDYLAY_ERR_INVALID_ARGUMENT;
  }
  if(session->server ||
     (session->cli_certvec.size == 0 ||
      session->cli_certvec.last_slot != 0)) {
    return SPDYLAY_ERR_INVALID_STATE;
  }
  origin = malloc(sizeof(spdylay_origin));
  if(origin == NULL) {
    return SPDYLAY_ERR_NOMEM;
  }
  strcpy(origin->scheme, scheme);
  strcpy(origin->host, host);
  origin->port = port;
  slot = spdylay_client_cert_vector_put(&session->cli_certvec, origin);
  assert(slot == 1);
  return 0;
}

const spdylay_origin* spdylay_session_get_client_cert_origin
(spdylay_session *session,
 size_t slot)
{
  return spdylay_client_cert_vector_get_origin(&session->cli_certvec, slot);
}


int spdylay_session_set_option(spdylay_session *session,
                               int optname, void *optval, size_t optlen)
{
  switch(optname) {
  case SPDYLAY_OPT_NO_AUTO_WINDOW_UPDATE:
    if(optlen == sizeof(int)) {
      int intval = *(int*)optval;
      if(intval) {
        session->opt_flags |= SPDYLAY_OPTMASK_NO_AUTO_WINDOW_UPDATE;
      } else {
        session->opt_flags &= ~SPDYLAY_OPTMASK_NO_AUTO_WINDOW_UPDATE;
      }
    } else {
      return SPDYLAY_ERR_INVALID_ARGUMENT;
    }
    break;
  case SPDYLAY_OPT_MAX_RECV_CTRL_FRAME_BUFFER:
    if(optlen == sizeof(uint32_t)) {
      uint32_t intval = *(uint32_t*)optval;
      if((1 << 13) <= intval && intval < (1 << 24)) {
        session->max_recv_ctrl_frame_buf = intval;
      } else {
        return SPDYLAY_ERR_INVALID_ARGUMENT;
      }
    } else {
      return SPDYLAY_ERR_INVALID_ARGUMENT;
    }
    break;
  default:
    return SPDYLAY_ERR_INVALID_ARGUMENT;
  }
  return 0;
}
