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
#include "shrpx_downstream.h"

#include <cassert>

#include "shrpx_upstream.h"
#include "shrpx_client_handler.h"
#include "shrpx_config.h"
#include "shrpx_error.h"
#include "shrpx_downstream_connection.h"
#include "util.h"

using namespace spdylay;

namespace shrpx {

Downstream::Downstream(Upstream *upstream, int stream_id, int priority)
  : upstream_(upstream),
    dconn_(0),
    stream_id_(stream_id),
    priority_(priority),
    downstream_stream_id_(-1),
    request_state_(INITIAL),
    request_major_(1),
    request_minor_(1),
    chunked_request_(false),
    request_connection_close_(false),
    request_expect_100_continue_(false),
    request_header_key_prev_(false),
    response_state_(INITIAL),
    response_http_status_(0),
    response_major_(1),
    response_minor_(1),
    chunked_response_(false),
    response_connection_close_(false),
    response_header_key_prev_(false),
    response_body_buf_(0),
    recv_window_size_(0)
{}

Downstream::~Downstream()
{
  if(LOG_ENABLED(INFO)) {
    DLOG(INFO, this) << "Deleting";
  }
  if(response_body_buf_) {
    // Passing NULL to evbuffer_free() causes segmentation fault.
    evbuffer_free(response_body_buf_);
  }
  if(dconn_) {
    delete dconn_;
  }
  if(LOG_ENABLED(INFO)) {
    DLOG(INFO, this) << "Deleted";
  }
}

void Downstream::set_downstream_connection(DownstreamConnection *dconn)
{
  dconn_ = dconn;
}

DownstreamConnection* Downstream::get_downstream_connection()
{
  return dconn_;
}

void Downstream::pause_read(IOCtrlReason reason)
{
  if(dconn_) {
    dconn_->pause_read(reason);
  }
}

int Downstream::resume_read(IOCtrlReason reason)
{
  if(dconn_) {
    return dconn_->resume_read(reason);
  } else {
    return 0;
  }
}

void Downstream::force_resume_read()
{
  if(dconn_) {
    dconn_->force_resume_read();
  }
}

namespace {
void check_header_field(bool *result, const Headers::value_type &item,
                        const char *name, const char *value)
{
  if(util::strieq(item.first.c_str(), name)) {
    if(util::strifind(item.second.c_str(), value)) {
      *result = true;
    }
  }
}
} // namespace

namespace {
void check_transfer_encoding_chunked(bool *chunked,
                                     const Headers::value_type &item)
{
  return check_header_field(chunked, item, "transfer-encoding", "chunked");
}
} // namespace

namespace {
void check_expect_100_continue(bool *res,
                               const Headers::value_type& item)
{
  return check_header_field(res, item, "expect", "100-continue");
}
} // namespace

namespace {
void check_connection_close(bool *connection_close,
                            const Headers::value_type &item)
{
  if(util::strieq(item.first.c_str(), "connection")) {
    if(util::strifind(item.second.c_str(), "close")) {
      *connection_close = true;
    } else if(util::strifind(item.second.c_str(), "keep-alive")) {
      *connection_close = false;
    }
  }
}
} // namespace

const Headers& Downstream::get_request_headers() const
{
  return request_headers_;
}

void Downstream::add_request_header(const std::string& name,
                                    const std::string& value)
{
  request_header_key_prev_ = true;
  request_headers_.push_back(std::make_pair(name, value));
}

void Downstream::set_last_request_header_value(const std::string& value)
{
  request_header_key_prev_ = false;
  Headers::value_type &item = request_headers_.back();
  item.second = value;
  check_transfer_encoding_chunked(&chunked_request_, item);
  check_expect_100_continue(&request_expect_100_continue_, item);
  //check_connection_close(&request_connection_close_, item);
}

bool Downstream::get_request_header_key_prev() const
{
  return request_header_key_prev_;
}

void Downstream::append_last_request_header_key(const char *data, size_t len)
{
  assert(request_header_key_prev_);
  Headers::value_type &item = request_headers_.back();
  item.first.append(data, len);
}

void Downstream::append_last_request_header_value(const char *data, size_t len)
{
  assert(!request_header_key_prev_);
  Headers::value_type &item = request_headers_.back();
  item.second.append(data, len);
}

void Downstream::set_request_method(const std::string& method)
{
  request_method_ = method;
}

const std::string& Downstream::get_request_method() const
{
  return request_method_;
}

void Downstream::set_request_path(const std::string& path)
{
  request_path_ = path;
}

void Downstream::append_request_path(const char *data, size_t len)
{
  request_path_.append(data, len);
}

const std::string& Downstream::get_request_path() const
{
  return request_path_;
}

void Downstream::set_request_major(int major)
{
  request_major_ = major;
}

void Downstream::set_request_minor(int minor)
{
  request_minor_ = minor;
}

int Downstream::get_request_major() const
{
  return request_major_;
}

int Downstream::get_request_minor() const
{
  return request_minor_;
}

Upstream* Downstream::get_upstream() const
{
  return upstream_;
}

int32_t Downstream::get_stream_id() const
{
  return stream_id_;
}

void Downstream::set_request_state(int state)
{
  request_state_ = state;
}

int Downstream::get_request_state() const
{
  return request_state_;
}

bool Downstream::get_chunked_request() const
{
  return chunked_request_;
}

bool Downstream::get_request_connection_close() const
{
  return request_connection_close_;
}

void Downstream::set_request_connection_close(bool f)
{
  request_connection_close_ = f;
}

bool Downstream::get_expect_100_continue() const
{
  return request_expect_100_continue_;
}

bool Downstream::get_output_buffer_full()
{
  if(dconn_) {
    return dconn_->get_output_buffer_full();
  } else {
    return false;
  }
}

// Call this function after this object is attached to
// Downstream. Otherwise, the program will crash.
int Downstream::push_request_headers()
{
  return dconn_->push_request_headers();
}

int Downstream::push_upload_data_chunk(const uint8_t *data, size_t datalen)
{
  // Assumes that request headers have already been pushed to output
  // buffer using push_request_headers().
  if(!dconn_) {
    DLOG(WARNING, this) << "dconn_ is NULL";
    return 0;
  }
  return dconn_->push_upload_data_chunk(data, datalen);
}

int Downstream::end_upload_data()
{
  return dconn_->end_upload_data();
}

const Headers& Downstream::get_response_headers() const
{
  return response_headers_;
}

void Downstream::add_response_header(const std::string& name,
                                     const std::string& value)
{
  response_header_key_prev_ = true;
  response_headers_.push_back(std::make_pair(name, value));
  check_transfer_encoding_chunked(&chunked_response_,
                                  response_headers_.back());
}

void Downstream::set_last_response_header_value(const std::string& value)
{
  response_header_key_prev_ = false;
  Headers::value_type &item = response_headers_.back();
  item.second = value;
  check_transfer_encoding_chunked(&chunked_response_, item);
  //check_connection_close(&response_connection_close_, item);
}

bool Downstream::get_response_header_key_prev() const
{
  return response_header_key_prev_;
}

void Downstream::append_last_response_header_key(const char *data, size_t len)
{
  assert(response_header_key_prev_);
  Headers::value_type &item = response_headers_.back();
  item.first.append(data, len);
}

void Downstream::append_last_response_header_value(const char *data,
                                                   size_t len)
{
  assert(!response_header_key_prev_);
  Headers::value_type &item = response_headers_.back();
  item.second.append(data, len);
}

unsigned int Downstream::get_response_http_status() const
{
  return response_http_status_;
}

void Downstream::set_response_http_status(unsigned int status)
{
  response_http_status_ = status;
}

void Downstream::set_response_major(int major)
{
  response_major_ = major;
}

void Downstream::set_response_minor(int minor)
{
  response_minor_ = minor;
}

int Downstream::get_response_major() const
{
  return response_major_;
}

int Downstream::get_response_minor() const
{
  return response_minor_;
}

int Downstream::get_response_version() const
{
  return response_major_*100+response_minor_;
}

bool Downstream::get_chunked_response() const
{
  return chunked_response_;
}

void Downstream::set_chunked_response(bool f)
{
  chunked_response_ = f;
}

bool Downstream::get_response_connection_close() const
{
  return response_connection_close_;
}

void Downstream::set_response_connection_close(bool f)
{
  response_connection_close_ = f;
}

int Downstream::on_read()
{
  return dconn_->on_read();
}

void Downstream::set_response_state(int state)
{
  response_state_ = state;
}

int Downstream::get_response_state() const
{
  return response_state_;
}

namespace {
void body_buf_cb(evbuffer *body, size_t oldlen, size_t newlen, void *arg)
{
  Downstream *downstream = reinterpret_cast<Downstream*>(arg);
  if(newlen == 0) {
    if(downstream->resume_read(SHRPX_NO_BUFFER) == -1) {
      DLOG(WARNING, downstream) << "Sending WINDOW_UPDATE failed";
    }
  }
}
} // namespace

int Downstream::init_response_body_buf()
{
  if(!response_body_buf_) {
    response_body_buf_ = evbuffer_new();
    if(response_body_buf_ == 0) {
      DIE();
    }
    evbuffer_setcb(response_body_buf_, body_buf_cb, this);
  }
  return 0;
}

evbuffer* Downstream::get_response_body_buf()
{
  return response_body_buf_;
}

void Downstream::set_priority(int pri)
{
  priority_ = pri;
}

int32_t Downstream::get_recv_window_size() const
{
  return recv_window_size_;
}

void Downstream::inc_recv_window_size(int32_t amount)
{
  recv_window_size_ += amount;
}

void Downstream::set_recv_window_size(int32_t new_size)
{
  recv_window_size_ = new_size;
}

bool Downstream::tunnel_established() const
{
  return request_method_ == "CONNECT" &&
    200 <= response_http_status_ && response_http_status_ < 300;
}

void Downstream::set_downstream_stream_id(int32_t stream_id)
{
  downstream_stream_id_ = stream_id;
}

int32_t Downstream::get_downstream_stream_id() const
{
  return downstream_stream_id_;
}

} // namespace shrpx
