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
#include "shrpx_spdy_upstream.h"

#include <netinet/tcp.h>
#include <assert.h>
#include <cerrno>
#include <sstream>

#include "shrpx_client_handler.h"
#include "shrpx_downstream.h"
#include "shrpx_downstream_connection.h"
#include "shrpx_config.h"
#include "shrpx_http.h"
#include "shrpx_accesslog.h"
#include "util.h"

using namespace spdylay;

namespace shrpx {

namespace {
const size_t SHRPX_SPDY_UPSTREAM_OUTPUT_UPPER_THRES = 64*1024;
} // namespace

namespace {
ssize_t send_callback(spdylay_session *session,
                      const uint8_t *data, size_t len, int flags,
                      void *user_data)
{
  int rv;
  SpdyUpstream *upstream = reinterpret_cast<SpdyUpstream*>(user_data);
  ClientHandler *handler = upstream->get_client_handler();
  bufferevent *bev = handler->get_bev();
  evbuffer *output = bufferevent_get_output(bev);
  // Check buffer length and return WOULDBLOCK if it is large enough.
  if(evbuffer_get_length(output) > SHRPX_SPDY_UPSTREAM_OUTPUT_UPPER_THRES) {
    return SPDYLAY_ERR_WOULDBLOCK;
  }

  rv = evbuffer_add(output, data, len);
  if(rv == -1) {
    ULOG(FATAL, upstream) << "evbuffer_add() failed";
    return SPDYLAY_ERR_CALLBACK_FAILURE;
  } else {
    return len;
  }
}
} // namespace

namespace {
ssize_t recv_callback(spdylay_session *session,
                      uint8_t *data, size_t len, int flags, void *user_data)
{
  SpdyUpstream *upstream = reinterpret_cast<SpdyUpstream*>(user_data);
  ClientHandler *handler = upstream->get_client_handler();
  bufferevent *bev = handler->get_bev();
  evbuffer *input = bufferevent_get_input(bev);
  int nread = evbuffer_remove(input, data, len);
  if(nread == -1) {
    return SPDYLAY_ERR_CALLBACK_FAILURE;
  } else if(nread == 0) {
    return SPDYLAY_ERR_WOULDBLOCK;
  } else {
    return nread;
  }
}
} // namespace

namespace {
void on_stream_close_callback
(spdylay_session *session, int32_t stream_id, spdylay_status_code status_code,
 void *user_data)
{
  SpdyUpstream *upstream = reinterpret_cast<SpdyUpstream*>(user_data);
  if(LOG_ENABLED(INFO)) {
    ULOG(INFO, upstream) << "Stream stream_id=" << stream_id
                         << " is being closed";
  }
  Downstream *downstream = upstream->find_downstream(stream_id);
  if(downstream) {
    if(downstream->get_request_state() == Downstream::CONNECT_FAIL) {
      upstream->remove_downstream(downstream);
      delete downstream;
    } else {
      downstream->set_request_state(Downstream::STREAM_CLOSED);
      if(downstream->get_response_state() == Downstream::MSG_COMPLETE) {
        // At this point, downstream response was read
        if(!downstream->tunnel_established() &&
           !downstream->get_response_connection_close()) {
          // Keep-alive
          DownstreamConnection *dconn;
          dconn = downstream->get_downstream_connection();
          if(dconn) {
            dconn->detach_downstream(downstream);
          }
        }
        upstream->remove_downstream(downstream);
        delete downstream;
      } else {
        // At this point, downstream read may be paused.

        // If shrpx_downstream::push_request_headers() failed, the
        // error is handled here.
        upstream->remove_downstream(downstream);
        delete downstream;
        // How to test this case? Request sufficient large download
        // and make client send RST_STREAM after it gets first DATA
        // frame chunk.
      }
    }
  }
}
} // namespace

namespace {
void on_ctrl_recv_callback
(spdylay_session *session, spdylay_frame_type type, spdylay_frame *frame,
 void *user_data)
{
  SpdyUpstream *upstream = reinterpret_cast<SpdyUpstream*>(user_data);
  switch(type) {
  case SPDYLAY_SYN_STREAM: {
    if(LOG_ENABLED(INFO)) {
      ULOG(INFO, upstream) << "Received upstream SYN_STREAM stream_id="
                           << frame->syn_stream.stream_id;
    }
    Downstream *downstream;
    downstream = new Downstream(upstream,
                                frame->syn_stream.stream_id,
                                frame->syn_stream.pri);
    upstream->add_downstream(downstream);
    downstream->init_response_body_buf();

    char **nv = frame->syn_stream.nv;
    const char *path = 0;
    const char *scheme = 0;
    const char *host = 0;
    const char *method = 0;
    for(size_t i = 0; nv[i]; i += 2) {
      if(strcmp(nv[i], ":path") == 0) {
        path = nv[i+1];
      } else if(strcmp(nv[i], ":scheme") == 0) {
        scheme = nv[i+1];
      } else if(strcmp(nv[i], ":method") == 0) {
        method = nv[i+1];
        downstream->set_request_method(nv[i+1]);
      } else if(strcmp(nv[i], ":host") == 0) {
        host = nv[i+1];
      } else if(nv[i][0] != ':') {
        downstream->add_request_header(nv[i], nv[i+1]);
      }
    }
    if(!path || !host || !method) {
      upstream->rst_stream(downstream, SPDYLAY_INTERNAL_ERROR);
      return;
    }
    // SpdyDownstreamConnection examines request path to find
    // scheme. We construct abs URI for spdy_bridge mode as well as
    // spdy_proxy mode.
    if((get_config()->spdy_proxy || get_config()->spdy_bridge) &&
       scheme && path[0] == '/') {
      std::string reqpath = scheme;
      reqpath += "://";
      reqpath += host;
      reqpath += path;
      downstream->set_request_path(reqpath);
    } else {
      downstream->set_request_path(path);
    }

    downstream->add_request_header("host", host);

    if(LOG_ENABLED(INFO)) {
      std::stringstream ss;
      for(size_t i = 0; nv[i]; i += 2) {
        ss << TTY_HTTP_HD << nv[i] << TTY_RST << ": " << nv[i+1] << "\n";
      }
      ULOG(INFO, upstream) << "HTTP request headers. stream_id="
                           << downstream->get_stream_id()
                           << "\n" << ss.str();
    }

    DownstreamConnection *dconn;
    dconn = upstream->get_client_handler()->get_downstream_connection();
    int rv = dconn->attach_downstream(downstream);
    if(rv != 0) {
      // If downstream connection fails, issue RST_STREAM.
      upstream->rst_stream(downstream, SPDYLAY_INTERNAL_ERROR);
      downstream->set_request_state(Downstream::CONNECT_FAIL);
      return;
    }
    rv = downstream->push_request_headers();
    if(rv != 0) {
      upstream->rst_stream(downstream, SPDYLAY_INTERNAL_ERROR);
      return;
    }
    downstream->set_request_state(Downstream::HEADER_COMPLETE);
    if(frame->syn_stream.hd.flags & SPDYLAY_CTRL_FLAG_FIN) {
      downstream->set_request_state(Downstream::MSG_COMPLETE);
    }
    break;
  }
  default:
    break;
  }
}
} // namespace

namespace {
void on_data_chunk_recv_callback(spdylay_session *session,
                                 uint8_t flags, int32_t stream_id,
                                 const uint8_t *data, size_t len,
                                 void *user_data)
{
  SpdyUpstream *upstream = reinterpret_cast<SpdyUpstream*>(user_data);
  Downstream *downstream = upstream->find_downstream(stream_id);
  if(downstream) {
    if(downstream->push_upload_data_chunk(data, len) != 0) {
      upstream->rst_stream(downstream, SPDYLAY_INTERNAL_ERROR);
      return;
    }
    if(upstream->get_flow_control()) {
      downstream->inc_recv_window_size(len);
      if(downstream->get_recv_window_size() >
         upstream->get_initial_window_size()) {
        if(LOG_ENABLED(INFO)) {
          ULOG(INFO, upstream) << "Flow control error: recv_window_size="
                               << downstream->get_recv_window_size()
                               << ", initial_window_size="
                               << upstream->get_initial_window_size();
        }
        upstream->rst_stream(downstream, SPDYLAY_FLOW_CONTROL_ERROR);
        return;
      }
    }
    if(flags & SPDYLAY_DATA_FLAG_FIN) {
      downstream->set_request_state(Downstream::MSG_COMPLETE);
    }
  }
}
} // namespace

namespace {
void on_ctrl_not_send_callback(spdylay_session *session,
                               spdylay_frame_type type,
                               spdylay_frame *frame,
                               int error_code, void *user_data)
{
  SpdyUpstream *upstream = reinterpret_cast<SpdyUpstream*>(user_data);
  ULOG(WARNING, upstream) << "Failed to send control frame type=" << type
                          << ", error_code=" << error_code << ":"
                          << spdylay_strerror(error_code);
  if(type == SPDYLAY_SYN_REPLY) {
    // To avoid stream hanging around, issue RST_STREAM.
    int32_t stream_id = frame->syn_reply.stream_id;
    Downstream *downstream = upstream->find_downstream(stream_id);
    if(downstream) {
      upstream->rst_stream(downstream, SPDYLAY_INTERNAL_ERROR);
    }
  }
}
} // namespace

namespace {
void on_ctrl_recv_parse_error_callback(spdylay_session *session,
                                       spdylay_frame_type type,
                                       const uint8_t *head, size_t headlen,
                                       const uint8_t *payload,
                                       size_t payloadlen, int error_code,
                                       void *user_data)
{
  SpdyUpstream *upstream = reinterpret_cast<SpdyUpstream*>(user_data);
  if(LOG_ENABLED(INFO)) {
    ULOG(INFO, upstream) << "Failed to parse received control frame. type="
                         << type
                         << ", error_code=" << error_code << ":"
                         << spdylay_strerror(error_code);
  }
}
} // namespace

namespace {
void on_unknown_ctrl_recv_callback(spdylay_session *session,
                                   const uint8_t *head, size_t headlen,
                                   const uint8_t *payload, size_t payloadlen,
                                   void *user_data)
{
  SpdyUpstream *upstream = reinterpret_cast<SpdyUpstream*>(user_data);
  if(LOG_ENABLED(INFO)) {
    ULOG(INFO, upstream) << "Received unknown control frame.";
  }
}
} // namespace

SpdyUpstream::SpdyUpstream(uint16_t version, ClientHandler *handler)
  : handler_(handler),
    session_(0)
{
  //handler->set_bev_cb(spdy_readcb, 0, spdy_eventcb);
  handler->set_upstream_timeouts(&get_config()->spdy_upstream_read_timeout,
                                 &get_config()->upstream_write_timeout);

  spdylay_session_callbacks callbacks;
  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.send_callback = send_callback;
  callbacks.recv_callback = recv_callback;
  callbacks.on_stream_close_callback = on_stream_close_callback;
  callbacks.on_ctrl_recv_callback = on_ctrl_recv_callback;
  callbacks.on_data_chunk_recv_callback = on_data_chunk_recv_callback;
  callbacks.on_ctrl_not_send_callback = on_ctrl_not_send_callback;
  callbacks.on_ctrl_recv_parse_error_callback =
    on_ctrl_recv_parse_error_callback;
  callbacks.on_unknown_ctrl_recv_callback = on_unknown_ctrl_recv_callback;

  int rv;
  rv = spdylay_session_server_new(&session_, version, &callbacks, this);
  assert(rv == 0);

  if(version == SPDYLAY_PROTO_SPDY3) {
    int val = 1;
    flow_control_ = true;
    initial_window_size_ = 1 << get_config()->spdy_upstream_window_bits;
    rv = spdylay_session_set_option(session_,
                                    SPDYLAY_OPT_NO_AUTO_WINDOW_UPDATE, &val,
                                    sizeof(val));
    assert(rv == 0);
  } else {
    flow_control_ = false;
    initial_window_size_ = 0;
  }
  // TODO Maybe call from outside?
  spdylay_settings_entry entry[2];
  entry[0].settings_id = SPDYLAY_SETTINGS_MAX_CONCURRENT_STREAMS;
  entry[0].value = get_config()->spdy_max_concurrent_streams;
  entry[0].flags = SPDYLAY_ID_FLAG_SETTINGS_NONE;

  entry[1].settings_id = SPDYLAY_SETTINGS_INITIAL_WINDOW_SIZE;
  entry[1].value = initial_window_size_;
  entry[1].flags = SPDYLAY_ID_FLAG_SETTINGS_NONE;

  rv = spdylay_submit_settings
    (session_, SPDYLAY_FLAG_SETTINGS_NONE,
     entry, sizeof(entry)/sizeof(spdylay_settings_entry));
  assert(rv == 0);
  // TODO Maybe call from outside?
  send();
}

SpdyUpstream::~SpdyUpstream()
{
  spdylay_session_del(session_);
}

int SpdyUpstream::on_read()
{
  int rv = 0;
  if((rv = spdylay_session_recv(session_)) < 0) {
    if(rv != SPDYLAY_ERR_EOF) {
      ULOG(ERROR, this) << "spdylay_session_recv() returned error: "
                        << spdylay_strerror(rv);
    }
  } else if((rv = spdylay_session_send(session_)) < 0) {
    ULOG(ERROR, this) << "spdylay_session_send() returned error: "
                      << spdylay_strerror(rv);
  }
  if(rv == 0) {
    if(spdylay_session_want_read(session_) == 0 &&
       spdylay_session_want_write(session_) == 0) {
      if(LOG_ENABLED(INFO)) {
        ULOG(INFO, this) << "No more read/write for this SPDY session";
      }
      rv = -1;
    }
  }
  return rv;
}

int SpdyUpstream::on_write()
{
  return send();
}

// After this function call, downstream may be deleted.
int SpdyUpstream::send()
{
  int rv = 0;
  if((rv = spdylay_session_send(session_)) < 0) {
    ULOG(ERROR, this) << "spdylay_session_send() returned error: "
                      << spdylay_strerror(rv);
  }
  if(rv == 0) {
    if(spdylay_session_want_read(session_) == 0 &&
       spdylay_session_want_write(session_) == 0) {
      if(LOG_ENABLED(INFO)) {
        ULOG(INFO, this) << "No more read/write for this SPDY session";
      }
      rv = -1;
    }
  }
  return rv;
}

int SpdyUpstream::on_event()
{
  return 0;
}

ClientHandler* SpdyUpstream::get_client_handler() const
{
  return handler_;
}

namespace {
void spdy_downstream_readcb(bufferevent *bev, void *ptr)
{
  DownstreamConnection *dconn = reinterpret_cast<DownstreamConnection*>(ptr);
  Downstream *downstream = dconn->get_downstream();
  SpdyUpstream *upstream;
  upstream = static_cast<SpdyUpstream*>(downstream->get_upstream());
  if(downstream->get_request_state() == Downstream::STREAM_CLOSED) {
    // If upstream SPDY stream was closed, we just close downstream,
    // because there is no consumer now. Downstream connection is also
    // closed in this case.
    upstream->remove_downstream(downstream);
    delete downstream;
    return;
  }

  if(downstream->get_response_state() == Downstream::MSG_RESET) {
    // The downstream stream was reset (canceled). In this case,
    // RST_STREAM to the upstream and delete downstream connection
    // here. Deleting downstream will be taken place at
    // on_stream_close_callback.
    upstream->rst_stream(downstream, SPDYLAY_CANCEL);
    downstream->set_downstream_connection(0);
    delete dconn;
    return;
  }

  int rv = downstream->on_read();
  if(rv != 0) {
    if(LOG_ENABLED(INFO)) {
      DCLOG(INFO, dconn) << "HTTP parser failure";
    }
    if(downstream->get_response_state() == Downstream::HEADER_COMPLETE) {
      upstream->rst_stream(downstream, SPDYLAY_INTERNAL_ERROR);
    } else {
      if(upstream->error_reply(downstream, 502) != 0) {
        delete upstream->get_client_handler();
        return;
      }
    }
    downstream->set_response_state(Downstream::MSG_COMPLETE);
    // Clearly, we have to close downstream connection on http parser
    // failure.
    downstream->set_downstream_connection(0);
    delete dconn;
    dconn = 0;
  }
  if(upstream->send() != 0) {
    delete upstream->get_client_handler();
    return;
  }
  // At this point, downstream may be deleted.
}
} // namespace

namespace {
void spdy_downstream_writecb(bufferevent *bev, void *ptr)
{
  if(evbuffer_get_length(bufferevent_get_output(bev)) > 0) {
    return;
  }
  DownstreamConnection *dconn = reinterpret_cast<DownstreamConnection*>(ptr);
  Downstream *downstream = dconn->get_downstream();
  SpdyUpstream *upstream;
  upstream = static_cast<SpdyUpstream*>(downstream->get_upstream());
  upstream->resume_read(SHRPX_NO_BUFFER, downstream);
}
} // namespace

namespace {
void spdy_downstream_eventcb(bufferevent *bev, short events, void *ptr)
{
  DownstreamConnection *dconn = reinterpret_cast<DownstreamConnection*>(ptr);
  Downstream *downstream = dconn->get_downstream();
  SpdyUpstream *upstream;
  upstream = static_cast<SpdyUpstream*>(downstream->get_upstream());
  if(events & BEV_EVENT_CONNECTED) {
    if(LOG_ENABLED(INFO)) {
      DCLOG(INFO, dconn) << "Connection established. stream_id="
                         << downstream->get_stream_id();
    }
    int fd = bufferevent_getfd(bev);
    int val = 1;
    if(setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
                  reinterpret_cast<char *>(&val), sizeof(val)) == -1) {
      DCLOG(WARNING, dconn) << "Setting option TCP_NODELAY failed: "
                            << strerror(errno);
    }
  } else if(events & BEV_EVENT_EOF) {
    if(LOG_ENABLED(INFO)) {
      DCLOG(INFO, dconn) << "EOF. stream_id=" << downstream->get_stream_id();
    }
    if(downstream->get_request_state() == Downstream::STREAM_CLOSED) {
      // If stream was closed already, we don't need to send reply at
      // the first place. We can delete downstream.
      upstream->remove_downstream(downstream);
      delete downstream;
    } else {
      // Delete downstream connection. If we don't delete it here, it
      // will be pooled in on_stream_close_callback.
      downstream->set_downstream_connection(0);
      delete dconn;
      dconn = 0;
      // downstream wil be deleted in on_stream_close_callback.
      if(downstream->get_response_state() == Downstream::HEADER_COMPLETE) {
        // Server may indicate the end of the request by EOF
        if(LOG_ENABLED(INFO)) {
          ULOG(INFO, upstream) << "Downstream body was ended by EOF";
        }
        downstream->set_response_state(Downstream::MSG_COMPLETE);
        if(downstream->tunnel_established()) {
          upstream->rst_stream(downstream, SPDYLAY_INTERNAL_ERROR);
        } else {
          upstream->on_downstream_body_complete(downstream);
        }
      } else if(downstream->get_response_state() == Downstream::MSG_COMPLETE) {
        // For SSL tunneling?
        upstream->rst_stream(downstream, SPDYLAY_INTERNAL_ERROR);
      } else {
        // If stream was not closed, then we set MSG_COMPLETE and let
        // on_stream_close_callback delete downstream.
        if(upstream->error_reply(downstream, 502) != 0) {
          upstream->get_client_handler();
          return;
        }
        downstream->set_response_state(Downstream::MSG_COMPLETE);
      }
      if(upstream->send() != 0) {
        delete upstream->get_client_handler();
        return;
      }
      // At this point, downstream may be deleted.
    }
  } else if(events & (BEV_EVENT_ERROR | BEV_EVENT_TIMEOUT)) {
    if(LOG_ENABLED(INFO)) {
      if(events & BEV_EVENT_ERROR) {
        DCLOG(INFO, dconn) << "Downstream network error: "
                           << evutil_socket_error_to_string
          (EVUTIL_SOCKET_ERROR());
      } else {
        DCLOG(INFO, dconn) << "Timeout";
      }
      if(downstream->tunnel_established()) {
        DCLOG(INFO, dconn) << "Note: this is tunnel connection";
      }
    }
    if(downstream->get_request_state() == Downstream::STREAM_CLOSED) {
      upstream->remove_downstream(downstream);
      delete downstream;
    } else {
      // Delete downstream connection. If we don't delete it here, it
      // will be pooled in on_stream_close_callback.
      downstream->set_downstream_connection(0);
      delete dconn;
      dconn = 0;
      if(downstream->get_response_state() == Downstream::MSG_COMPLETE) {
        // For SSL tunneling
        upstream->rst_stream(downstream, SPDYLAY_INTERNAL_ERROR);
      } else {
        if(downstream->get_response_state() == Downstream::HEADER_COMPLETE) {
          upstream->rst_stream(downstream, SPDYLAY_INTERNAL_ERROR);
        } else {
          int status;
          if(events & BEV_EVENT_TIMEOUT) {
            status = 504;
          } else {
            status = 502;
          }
          if(upstream->error_reply(downstream, status) != 0) {
            delete upstream->get_client_handler();
            return;
          }
        }
        downstream->set_response_state(Downstream::MSG_COMPLETE);
      }
      if(upstream->send() != 0) {
        delete upstream->get_client_handler();
        return;
      }
      // At this point, downstream may be deleted.
    }
  }
}
} // namespace

int SpdyUpstream::rst_stream(Downstream *downstream, int status_code)
{
  if(LOG_ENABLED(INFO)) {
    ULOG(INFO, this) << "RST_STREAM stream_id="
                     << downstream->get_stream_id();
  }
  int rv;
  rv = spdylay_submit_rst_stream(session_, downstream->get_stream_id(),
                                 status_code);
  if(rv < SPDYLAY_ERR_FATAL) {
    ULOG(FATAL, this) << "spdylay_submit_rst_stream() failed: "
                      << spdylay_strerror(rv);
    DIE();
  }
  return 0;
}

int SpdyUpstream::window_update(Downstream *downstream)
{
  int rv;
  rv = spdylay_submit_window_update(session_, downstream->get_stream_id(),
                                    downstream->get_recv_window_size());
  downstream->set_recv_window_size(0);
  if(rv < SPDYLAY_ERR_FATAL) {
    ULOG(FATAL, this) << "spdylay_submit_window_update() failed: "
                      << spdylay_strerror(rv);
    DIE();
  }
  return 0;
}

namespace {
ssize_t spdy_data_read_callback(spdylay_session *session,
                                int32_t stream_id,
                                uint8_t *buf, size_t length,
                                int *eof,
                                spdylay_data_source *source,
                                void *user_data)
{
  Downstream *downstream = reinterpret_cast<Downstream*>(source->ptr);
  evbuffer *body = downstream->get_response_body_buf();
  assert(body);
  int nread = evbuffer_remove(body, buf, length);
  // For tunneling, DATA stream is endless
  if(!downstream->tunnel_established() &&
     nread == 0 &&
     downstream->get_response_state() == Downstream::MSG_COMPLETE) {
    *eof = 1;
  }
  if(nread == 0 && *eof != 1) {
    return SPDYLAY_ERR_DEFERRED;
  }
  return nread;
}
} // namespace

int SpdyUpstream::error_reply(Downstream *downstream, int status_code)
{
  int rv;
  std::string html = http::create_error_html(status_code);
  downstream->init_response_body_buf();
  evbuffer *body = downstream->get_response_body_buf();
  rv = evbuffer_add(body, html.c_str(), html.size());
  if(rv == -1) {
    ULOG(FATAL, this) << "evbuffer_add() failed";
    return -1;
  }
  downstream->set_response_state(Downstream::MSG_COMPLETE);

  spdylay_data_provider data_prd;
  data_prd.source.ptr = downstream;
  data_prd.read_callback = spdy_data_read_callback;

  std::string content_length = util::utos(html.size());

  const char *nv[] = {
    ":status", http::get_status_string(status_code),
    ":version", "http/1.1",
    "content-type", "text/html; charset=UTF-8",
    "server", get_config()->server_name,
    "content-length", content_length.c_str(),
    0
  };

  rv = spdylay_submit_response(session_, downstream->get_stream_id(), nv,
                               &data_prd);
  if(rv < SPDYLAY_ERR_FATAL) {
    ULOG(FATAL, this) << "spdylay_submit_response() failed: "
                      << spdylay_strerror(rv);
    DIE();
  }
  if(get_config()->accesslog) {
    upstream_response(get_client_handler()->get_ipaddr(),
                      status_code, downstream);
  }
  return 0;
}

bufferevent_data_cb SpdyUpstream::get_downstream_readcb()
{
  return spdy_downstream_readcb;
}

bufferevent_data_cb SpdyUpstream::get_downstream_writecb()
{
  return spdy_downstream_writecb;
}

bufferevent_event_cb SpdyUpstream::get_downstream_eventcb()
{
  return spdy_downstream_eventcb;
}

void SpdyUpstream::add_downstream(Downstream *downstream)
{
  downstream_queue_.add(downstream);
}

void SpdyUpstream::remove_downstream(Downstream *downstream)
{
  downstream_queue_.remove(downstream);
}

Downstream* SpdyUpstream::find_downstream(int32_t stream_id)
{
  return downstream_queue_.find(stream_id);
}

spdylay_session* SpdyUpstream::get_spdy_session()
{
  return session_;
}

// WARNING: Never call directly or indirectly spdylay_session_send or
// spdylay_session_recv. These calls may delete downstream.
int SpdyUpstream::on_downstream_header_complete(Downstream *downstream)
{
  if(LOG_ENABLED(INFO)) {
    DLOG(INFO, downstream) << "HTTP response header completed";
  }
  size_t nheader = downstream->get_response_headers().size();
  // 6 means :status, :version and possible via header field.
  const char **nv = new const char*[nheader * 2 + 6 + 1];
  size_t hdidx = 0;
  std::string via_value;
  nv[hdidx++] = ":status";
  nv[hdidx++] = http::get_status_string(downstream->get_response_http_status());
  nv[hdidx++] = ":version";
  nv[hdidx++] = "HTTP/1.1";
  for(Headers::const_iterator i = downstream->get_response_headers().begin();
      i != downstream->get_response_headers().end(); ++i) {
    if(util::strieq((*i).first.c_str(), "transfer-encoding") ||
       util::strieq((*i).first.c_str(), "keep-alive") || // HTTP/1.0?
       util::strieq((*i).first.c_str(), "connection") ||
       util:: strieq((*i).first.c_str(), "proxy-connection")) {
      // These are ignored
    } else if(!get_config()->no_via &&
              util::strieq((*i).first.c_str(), "via")) {
      via_value = (*i).second;
    } else {
      nv[hdidx++] = (*i).first.c_str();
      nv[hdidx++] = (*i).second.c_str();
    }
  }
  if(!get_config()->no_via) {
    if(!via_value.empty()) {
      via_value += ", ";
    }
    via_value += http::create_via_header_value
      (downstream->get_response_major(), downstream->get_response_minor());
    nv[hdidx++] = "via";
    nv[hdidx++] = via_value.c_str();
  }
  nv[hdidx++] = 0;
  if(LOG_ENABLED(INFO)) {
    std::stringstream ss;
    for(size_t i = 0; nv[i]; i += 2) {
      ss << TTY_HTTP_HD << nv[i] << TTY_RST << ": " << nv[i+1] << "\n";
    }
    ULOG(INFO, this) << "HTTP response headers. stream_id="
                     << downstream->get_stream_id() << "\n"
                     << ss.str();
  }
  spdylay_data_provider data_prd;
  data_prd.source.ptr = downstream;
  data_prd.read_callback = spdy_data_read_callback;

  int rv;
  rv = spdylay_submit_response(session_, downstream->get_stream_id(), nv,
                               &data_prd);
  delete [] nv;
  if(rv != 0) {
    ULOG(FATAL, this) << "spdylay_submit_response() failed";
    return -1;
  }
  if(get_config()->accesslog) {
    upstream_response(get_client_handler()->get_ipaddr(),
                      downstream->get_response_http_status(),
                      downstream);
  }
  return 0;
}

// WARNING: Never call directly or indirectly spdylay_session_send or
// spdylay_session_recv. These calls may delete downstream.
int SpdyUpstream::on_downstream_body(Downstream *downstream,
                                     const uint8_t *data, size_t len)
{
  evbuffer *body = downstream->get_response_body_buf();
  int rv = evbuffer_add(body, data, len);
  if(rv != 0) {
    ULOG(FATAL, this) << "evbuffer_add() failed";
    return -1;
  }
  spdylay_session_resume_data(session_, downstream->get_stream_id());

  size_t bodylen = evbuffer_get_length(body);
  if(bodylen > SHRPX_SPDY_UPSTREAM_OUTPUT_UPPER_THRES) {
    downstream->pause_read(SHRPX_NO_BUFFER);
  }

  return 0;
}

// WARNING: Never call directly or indirectly spdylay_session_send or
// spdylay_session_recv. These calls may delete downstream.
int SpdyUpstream::on_downstream_body_complete(Downstream *downstream)
{
  if(LOG_ENABLED(INFO)) {
    DLOG(INFO, downstream) << "HTTP response completed";
  }
  spdylay_session_resume_data(session_, downstream->get_stream_id());
  return 0;
}

bool SpdyUpstream::get_flow_control() const
{
  return flow_control_;
}

int32_t SpdyUpstream::get_initial_window_size() const
{
  return initial_window_size_;
}

void SpdyUpstream::pause_read(IOCtrlReason reason)
{}

int SpdyUpstream::resume_read(IOCtrlReason reason, Downstream *downstream)
{
  if(get_flow_control()) {
    if(downstream->get_recv_window_size() >= get_initial_window_size()/2) {
      window_update(downstream);
    }
  }
  return 0;
}

} // namespace shrpx
