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
#ifndef SPDY_SERVER_H
#define SPDY_SERVER_H

#include "spdylay_config.h"

#include <stdint.h>
#include <sys/types.h>

#include <cstdlib>

#include <string>
#include <vector>
#include <map>

#include <openssl/ssl.h>

#include <spdylay/spdylay.h>

namespace spdylay {

struct Config {
  std::string htdocs;
  bool verbose;
  bool daemon;
  std::string host;
  uint16_t port;
  std::string private_key_file;
  std::string cert_file;
  spdylay_on_request_recv_callback on_request_recv_callback;
  void *data_ptr;
  uint16_t version;
  bool verify_client;
  bool no_tls;
  Config();
};

class Sessions;

class EventHandler {
public:
  EventHandler(const Config *config);
  virtual ~EventHandler() {}
  virtual int execute(Sessions *sessions) = 0;
  virtual bool want_read() = 0;
  virtual bool want_write() = 0;
  virtual int fd() const = 0;
  virtual bool finish() = 0;
  const Config* config() const
  {
    return config_;
  }
  bool mark_del()
  {
    return mark_del_;
  }
  void mark_del(bool d)
  {
    mark_del_ = d;
  }
private:
  const Config *config_;
  bool mark_del_;
};

struct Request {
  int32_t stream_id;
  std::vector<std::pair<std::string, std::string> > headers;
  int file;
  std::pair<std::string, size_t> response_body;
  Request(int32_t stream_id);
  ~Request();
};

class SpdyEventHandler : public EventHandler {
public:
  SpdyEventHandler(const Config* config,
                   int fd, SSL *ssl, uint16_t version,
                   const spdylay_session_callbacks *callbacks,
                   int64_t session_id);
  virtual ~SpdyEventHandler();
  virtual int execute(Sessions *sessions);
  virtual bool want_read();
  virtual bool want_write();
  virtual int fd() const;
  virtual bool finish();

  uint16_t version() const;

  ssize_t send_data(const uint8_t *data, size_t len, int flags);

  ssize_t recv_data(uint8_t *data, size_t len, int flags);

  bool would_block() const;

  int submit_file_response(const std::string& status,
                           int32_t stream_id,
                           time_t last_modified,
                           off_t file_length,
                           spdylay_data_provider *data_prd);

  int submit_response(const std::string& status,
                      int32_t stream_id,
                      spdylay_data_provider *data_prd);

  int submit_response
  (const std::string& status,
   int32_t stream_id,
   const std::vector<std::pair<std::string, std::string> >& headers,
   spdylay_data_provider *data_prd);

  void add_stream(int32_t stream_id, Request *req);
  void remove_stream(int32_t stream_id);
  Request* get_stream(int32_t stream_id);
  int64_t session_id() const;
private:
  spdylay_session *session_;
  int fd_;
  SSL* ssl_;
  uint16_t version_;
  int64_t session_id_;
  uint8_t io_flags_;
  std::map<int32_t, Request*> id2req_;
};

class SpdyServer {
public:
  SpdyServer(const Config* config);
  ~SpdyServer();
  int listen();
  int run();
private:
  const Config *config_;
  int sfd_[2];
};

void htdocs_on_request_recv_callback
(spdylay_session *session, int32_t stream_id, void *user_data);

ssize_t file_read_callback
(spdylay_session *session, int32_t stream_id,
 uint8_t *buf, size_t length, int *eof,
 spdylay_data_source *source, void *user_data);

} // namespace spdylay

#endif // SPDY_SERVER_H
