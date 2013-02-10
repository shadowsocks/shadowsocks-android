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
#ifndef SPDYLAY_SSL_H
#define SPDYLAY_SSL_H

#include "spdylay_config.h"

#include <stdint.h>
#include <cstdlib>
#include <sys/time.h>
#include <poll.h>
#include <map>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <spdylay/spdylay.h>

namespace spdylay {

extern bool ssl_debug;

class Spdylay {
public:
  Spdylay(int fd, SSL *ssl, uint16_t version,
          const spdylay_session_callbacks *callbacks,
          void *user_data);
  ~Spdylay();
  int recv();
  int send();
  ssize_t send_data(const uint8_t *data, size_t len, int flags);
  ssize_t recv_data(uint8_t *data, size_t len, int flags);
  bool want_read();
  bool want_write();
  bool finish();
  int fd() const;
  int submit_request(const std::string& scheme,
                     const std::string& hostport, const std::string& path,
                     const std::map<std::string,std::string>& headers,
                     uint8_t pri,
                     const spdylay_data_provider *data_prd,
                     int64_t data_length,
                     void *stream_user_data);
  int submit_settings(int flags, spdylay_settings_entry *iv, size_t niv);
  bool would_block();
  void* user_data();
private:
  int fd_;
  SSL *ssl_;
  uint16_t version_;
  spdylay_session *session_;
  void *user_data_;
  uint8_t io_flags_;
  bool debug_;
};

int connect_to(const std::string& host, uint16_t port);

int nonblock_connect_to(const std::string& host, uint16_t port, int timeout);

int make_listen_socket(const std::string& host, uint16_t port, int family);

int make_non_block(int fd);

int set_tcp_nodelay(int fd);

ssize_t send_callback(spdylay_session *session,
                      const uint8_t *data, size_t len, int flags,
                      void *user_data);

ssize_t recv_callback(spdylay_session *session,
                      uint8_t *data, size_t len, int flags, void *user_data);

void print_nv(char **nv);

void on_ctrl_recv_callback
(spdylay_session *session, spdylay_frame_type type, spdylay_frame *frame,
 void *user_data);

void on_invalid_ctrl_recv_callback
(spdylay_session *session, spdylay_frame_type type, spdylay_frame *frame,
 uint32_t status_code, void *user_data);

void on_ctrl_recv_parse_error_callback(spdylay_session *session,
                                       spdylay_frame_type type,
                                       const uint8_t *head,
                                       size_t headlen,
                                       const uint8_t *payload,
                                       size_t payloadlen,
                                       int error_code, void *user_data);

void on_unknown_ctrl_recv_callback(spdylay_session *session,
                                   const uint8_t *head,
                                   size_t headlen,
                                   const uint8_t *payload,
                                   size_t payloadlen,
                                   void *user_data);

void on_ctrl_send_callback
(spdylay_session *session, spdylay_frame_type type, spdylay_frame *frame,
 void *user_data);

void on_data_recv_callback
(spdylay_session *session, uint8_t flags, int32_t stream_id, int32_t length,
 void *user_data);

void on_data_send_callback
(spdylay_session *session, uint8_t flags, int32_t stream_id, int32_t length,
 void *user_data);

void ctl_poll(pollfd *pollfd, Spdylay *sc);

int select_next_proto_cb(SSL* ssl,
                         unsigned char **out, unsigned char *outlen,
                         const unsigned char *in, unsigned int inlen,
                         void *arg);

void setup_ssl_ctx(SSL_CTX *ssl_ctx, void *next_proto_select_cb_arg);

int ssl_handshake(SSL *ssl, int fd);

int ssl_nonblock_handshake(SSL *ssl, int fd, int& timeout);

// Returns difference between |a| and |b| in milliseconds, assuming
// |a| is more recent than |b|.
int64_t time_delta(const timeval& a, const timeval& b);

void reset_timer();

void get_timer(timeval *tv);

int get_time(timeval *tv);

void print_timer();

enum {
  WANT_READ = 1,
  WANT_WRITE = 1 << 1
};

uint8_t get_ssl_io_demand(SSL *ssl, ssize_t r);

// Setting true will print characters with ANSI color escape codes
// when printing SPDY frames. This function changes a static variable.
void set_color_output(bool f);

} // namespace spdylay

#endif // SPDYLAY_SSL_H
