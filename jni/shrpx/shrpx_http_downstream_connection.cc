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
#include "shrpx_http_downstream_connection.h"

#include "shrpx_client_handler.h"
#include "shrpx_upstream.h"
#include "shrpx_downstream.h"
#include "shrpx_config.h"
#include "shrpx_error.h"
#include "shrpx_http.h"
#include "util.h"

using namespace spdylay;

namespace shrpx {

// Workaround for the inability for Bufferevent to remove timeout from
// bufferevent. Specify this long timeout instead of removing.
namespace {
timeval max_timeout = { 86400, 0 };
} // namespace

HttpDownstreamConnection::HttpDownstreamConnection
(ClientHandler *client_handler)
  : DownstreamConnection(client_handler),
    bev_(0),
    ioctrl_(0),
    response_htp_(new http_parser())
{}

HttpDownstreamConnection::~HttpDownstreamConnection()
{
  delete response_htp_;
  if(bev_) {
    bufferevent_disable(bev_, EV_READ | EV_WRITE);
    bufferevent_free(bev_);
  }
  // Downstream and DownstreamConnection may be deleted
  // asynchronously.
  if(downstream_) {
    downstream_->set_downstream_connection(0);
  }
}

int HttpDownstreamConnection::attach_downstream(Downstream *downstream)
{
  if(LOG_ENABLED(INFO)) {
    DCLOG(INFO, this) << "Attaching to DOWNSTREAM:" << downstream;
  }
  Upstream *upstream = downstream->get_upstream();
  if(!bev_) {
    event_base *evbase = client_handler_->get_evbase();
    bev_ = bufferevent_socket_new
      (evbase, -1,
       BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);
    int rv = bufferevent_socket_connect
      (bev_,
       // TODO maybe not thread-safe?
       const_cast<sockaddr*>(&get_config()->downstream_addr.sa),
       get_config()->downstream_addrlen);
    if(rv != 0) {
      bufferevent_free(bev_);
      bev_ = 0;
      return SHRPX_ERR_NETWORK;
    }
    if(LOG_ENABLED(INFO)) {
      DCLOG(INFO, this) << "Connecting to downstream server";
    }
  }
  downstream->set_downstream_connection(this);
  downstream_ = downstream;

  ioctrl_.set_bev(bev_);

  http_parser_init(response_htp_, HTTP_RESPONSE);
  response_htp_->data = downstream_;

  bufferevent_setwatermark(bev_, EV_READ, 0, SHRPX_READ_WARTER_MARK);
  bufferevent_enable(bev_, EV_READ);
  bufferevent_setcb(bev_,
                    upstream->get_downstream_readcb(),
                    upstream->get_downstream_writecb(),
                    upstream->get_downstream_eventcb(), this);
  // HTTP request/response model, we first issue request to downstream
  // server, so just enable write timeout here.
  bufferevent_set_timeouts(bev_,
                           &max_timeout,
                           &get_config()->downstream_write_timeout);
  return 0;
}

int HttpDownstreamConnection::push_request_headers()
{
  std::string hdrs = downstream_->get_request_method();
  hdrs += " ";
  hdrs += downstream_->get_request_path();
  hdrs += " ";
  hdrs += "HTTP/1.1\r\n";
  bool connection_upgrade = false;
  std::string via_value;
  std::string xff_value;
  const Headers& request_headers = downstream_->get_request_headers();
  for(Headers::const_iterator i = request_headers.begin();
      i != request_headers.end(); ++i) {
    if(util::strieq((*i).first.c_str(), "connection")) {
      if(util::strifind((*i).second.c_str(), "upgrade")) {
        connection_upgrade = true;
      }
    } else if(util::strieq((*i).first.c_str(), "x-forwarded-proto") ||
       util::strieq((*i).first.c_str(), "keep-alive") ||
       util::strieq((*i).first.c_str(), "proxy-connection")) {
      continue;
    }
    if(!get_config()->no_via && util::strieq((*i).first.c_str(), "via")) {
      via_value = (*i).second;
      continue;
    }
    if(util::strieq((*i).first.c_str(), "x-forwarded-for")) {
      xff_value = (*i).second;
      continue;
    }
    if(util::strieq((*i).first.c_str(), "expect") &&
       util::strifind((*i).second.c_str(), "100-continue")) {
      continue;
    }
    hdrs += (*i).first;
    http::capitalize(hdrs, hdrs.size()-(*i).first.size());
    hdrs += ": ";
    hdrs += (*i).second;
    hdrs += "\r\n";
  }
  if(downstream_->get_request_connection_close()) {
    hdrs += "Connection: close\r\n";
  } else if(connection_upgrade) {
    hdrs += "Connection: upgrade\r\n";
  }
  if(get_config()->add_x_forwarded_for) {
    hdrs += "X-Forwarded-For: ";
    if(!xff_value.empty()) {
      hdrs += xff_value;
      hdrs += ", ";
    }
    hdrs += downstream_->get_upstream()->get_client_handler()->get_ipaddr();
    hdrs += "\r\n";
  } else if(!xff_value.empty()) {
    hdrs += "X-Forwarded-For: ";
    hdrs += xff_value;
    hdrs += "\r\n";
  }
  if(downstream_->get_request_method() != "CONNECT") {
    hdrs += "X-Forwarded-Proto: ";
    if(util::istartsWith(downstream_->get_request_path(), "http:")) {
      hdrs += "http";
    } else {
      hdrs += "https";
    }
    hdrs += "\r\n";
  }
  if(!get_config()->no_via) {
    hdrs += "Via: ";
    if(!via_value.empty()) {
      hdrs += via_value;
      hdrs += ", ";
    }
    hdrs += http::create_via_header_value(downstream_->get_request_major(),
                                          downstream_->get_request_minor());
    hdrs += "\r\n";
  }

  hdrs += "\r\n";
  if(LOG_ENABLED(INFO)) {
    const char *hdrp;
    std::string nhdrs;
    if(get_config()->tty) {
      nhdrs = http::colorizeHeaders(hdrs.c_str());
      hdrp = nhdrs.c_str();
    } else {
      hdrp = hdrs.c_str();
    }
    DCLOG(INFO, this) << "HTTP request headers. stream_id="
                      << downstream_->get_stream_id() << "\n" << hdrp;
  }
  evbuffer *output = bufferevent_get_output(bev_);
  int rv;
  rv = evbuffer_add(output, hdrs.c_str(), hdrs.size());
  if(rv != 0) {
    return -1;
  }

  // When downstream request is issued, set read timeout. We don't
  // know when the request is completely received by the downstream
  // server. This function may be called before that happens. Overall
  // it does not cause problem for most of the time.  If the
  // downstream server is too slow to recv/send, the connection will
  // be dropped by read timeout.
  bufferevent_set_timeouts(bev_,
                           &get_config()->downstream_read_timeout,
                           &get_config()->downstream_write_timeout);

  return 0;
}

int HttpDownstreamConnection::push_upload_data_chunk
(const uint8_t *data, size_t datalen)
{
  ssize_t res = 0;
  int rv;
  int chunked = downstream_->get_chunked_request();
  evbuffer *output = bufferevent_get_output(bev_);
  if(chunked) {
    char chunk_size_hex[16];
    rv = snprintf(chunk_size_hex, sizeof(chunk_size_hex), "%X\r\n",
                  static_cast<unsigned int>(datalen));
    res += rv;
    rv = evbuffer_add(output, chunk_size_hex, rv);
    if(rv == -1) {
      DCLOG(FATAL, this) << "evbuffer_add() failed";
      return -1;
    }
  }
  rv = evbuffer_add(output, data, datalen);
  if(rv == -1) {
    DCLOG(FATAL, this) << "evbuffer_add() failed";
    return -1;
  }
  res += rv;
  if(chunked) {
    rv = evbuffer_add(output, "\r\n", 2);
    if(rv == -1) {
      DCLOG(FATAL, this) << "evbuffer_add() failed";
      return -1;
    }
    res += 2;
  }
  return res;
}

int HttpDownstreamConnection::end_upload_data()
{
  if(downstream_->get_chunked_request()) {
    evbuffer *output = bufferevent_get_output(bev_);
    if(evbuffer_add(output, "0\r\n\r\n", 5) != 0) {
      DCLOG(FATAL, this) << "evbuffer_add() failed";
      return -1;
    }
  }
  return 0;
}

namespace {
// Gets called when DownstreamConnection is pooled in ClientHandler.
void idle_eventcb(bufferevent *bev, short events, void *arg)
{
  HttpDownstreamConnection *dconn;
  dconn = reinterpret_cast<HttpDownstreamConnection*>(arg);
  if(events & BEV_EVENT_CONNECTED) {
    // Downstream was detached before connection established?
    // This may be safe to be left.
    if(LOG_ENABLED(INFO)) {
      DCLOG(INFO, dconn) << "Idle connection connected?";
    }
    return;
  }
  if(events & BEV_EVENT_EOF) {
    if(LOG_ENABLED(INFO)) {
      DCLOG(INFO, dconn) << "Idle connection EOF";
    }
  } else if(events & BEV_EVENT_TIMEOUT) {
    if(LOG_ENABLED(INFO)) {
      DCLOG(INFO, dconn) << "Idle connection timeout";
    }
  } else if(events & BEV_EVENT_ERROR) {
    if(LOG_ENABLED(INFO)) {
      DCLOG(INFO, dconn) << "Idle connection network error";
    }
  }
  ClientHandler *client_handler = dconn->get_client_handler();
  client_handler->remove_downstream_connection(dconn);
  delete dconn;
}
} // namespace

void HttpDownstreamConnection::detach_downstream(Downstream *downstream)
{
  if(LOG_ENABLED(INFO)) {
    DCLOG(INFO, this) << "Detaching from DOWNSTREAM:" << downstream;
  }
  downstream->set_downstream_connection(0);
  downstream_ = 0;
  ioctrl_.force_resume_read();
  bufferevent_enable(bev_, EV_READ);
  bufferevent_setcb(bev_, 0, 0, idle_eventcb, this);
  // On idle state, just enable read timeout. Normally idle downstream
  // connection will get EOF from the downstream server and closed.
  bufferevent_set_timeouts(bev_,
                           &get_config()->downstream_idle_read_timeout,
                           &get_config()->downstream_write_timeout);
  client_handler_->pool_downstream_connection(this);
}

bufferevent* HttpDownstreamConnection::get_bev()
{
  return bev_;
}

void HttpDownstreamConnection::pause_read(IOCtrlReason reason)
{
  ioctrl_.pause_read(reason);
}

int HttpDownstreamConnection::resume_read(IOCtrlReason reason)
{
  return ioctrl_.resume_read(reason) ? 0 : -1;
}

void HttpDownstreamConnection::force_resume_read()
{
  ioctrl_.force_resume_read();
}

bool HttpDownstreamConnection::get_output_buffer_full()
{
  evbuffer *output = bufferevent_get_output(bev_);
  return evbuffer_get_length(output) >= Downstream::OUTPUT_UPPER_THRES;
}

namespace {
int htp_hdrs_completecb(http_parser *htp)
{
  Downstream *downstream;
  downstream = reinterpret_cast<Downstream*>(htp->data);
  downstream->set_response_http_status(htp->status_code);
  downstream->set_response_major(htp->http_major);
  downstream->set_response_minor(htp->http_minor);
  downstream->set_response_connection_close(!http_should_keep_alive(htp));
  downstream->set_response_state(Downstream::HEADER_COMPLETE);
  if(downstream->get_upstream()->on_downstream_header_complete(downstream)
     != 0) {
    return -1;
  }
  unsigned int status = downstream->get_response_http_status();
  // Ignore the response body. HEAD response may contain
  // Content-Length or Transfer-Encoding: chunked.  Some server send
  // 304 status code with nonzero Content-Length, but without response
  // body. See
  // http://tools.ietf.org/html/draft-ietf-httpbis-p1-messaging-20#section-3.3
  return downstream->get_request_method() == "HEAD" ||
    (100 <= status && status <= 199) || status == 204 ||
    status == 304 ? 1 : 0;
}
} // namespace

namespace {
int htp_hdr_keycb(http_parser *htp, const char *data, size_t len)
{
  Downstream *downstream;
  downstream = reinterpret_cast<Downstream*>(htp->data);
  if(downstream->get_response_header_key_prev()) {
    downstream->append_last_response_header_key(data, len);
  } else {
    downstream->add_response_header(std::string(data, len), "");
  }
  return 0;
}
} // namespace

namespace {
int htp_hdr_valcb(http_parser *htp, const char *data, size_t len)
{
  Downstream *downstream;
  downstream = reinterpret_cast<Downstream*>(htp->data);
  if(downstream->get_response_header_key_prev()) {
    downstream->set_last_response_header_value(std::string(data, len));
  } else {
    downstream->append_last_response_header_value(data, len);
  }
  return 0;
}
} // namespace

namespace {
int htp_bodycb(http_parser *htp, const char *data, size_t len)
{
  Downstream *downstream;
  downstream = reinterpret_cast<Downstream*>(htp->data);

  return downstream->get_upstream()->on_downstream_body
    (downstream, reinterpret_cast<const uint8_t*>(data), len);
}
} // namespace

namespace {
int htp_msg_completecb(http_parser *htp)
{
  Downstream *downstream;
  downstream = reinterpret_cast<Downstream*>(htp->data);

  downstream->set_response_state(Downstream::MSG_COMPLETE);
  return downstream->get_upstream()->on_downstream_body_complete(downstream);
}
} // namespace

namespace {
http_parser_settings htp_hooks = {
  0, /*http_cb      on_message_begin;*/
  0, /*http_data_cb on_url;*/
  0, /*http_cb on_status_complete */
  htp_hdr_keycb, /*http_data_cb on_header_field;*/
  htp_hdr_valcb, /*http_data_cb on_header_value;*/
  htp_hdrs_completecb, /*http_cb      on_headers_complete;*/
  htp_bodycb, /*http_data_cb on_body;*/
  htp_msg_completecb /*http_cb      on_message_complete;*/
};
} // namespace

int HttpDownstreamConnection::on_read()
{
  evbuffer *input = bufferevent_get_input(bev_);
  unsigned char *mem = evbuffer_pullup(input, -1);

  size_t nread = http_parser_execute(response_htp_, &htp_hooks,
                                     reinterpret_cast<const char*>(mem),
                                     evbuffer_get_length(input));

  evbuffer_drain(input, nread);
  http_errno htperr = HTTP_PARSER_ERRNO(response_htp_);
  if(htperr == HPE_OK) {
    return 0;
  } else {
    if(LOG_ENABLED(INFO)) {
      DCLOG(INFO, this) << "HTTP parser failure: "
                        << "(" << http_errno_name(htperr) << ") "
                        << http_errno_description(htperr);
    }
    return SHRPX_ERR_HTTP_PARSE;
  }
}

int HttpDownstreamConnection::on_write()
{
  return 0;
}

} // namespace shrpx
