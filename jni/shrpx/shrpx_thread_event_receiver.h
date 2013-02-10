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
#ifndef SHRPX_THREAD_EVENT_RECEIVER_H
#define SHRPX_THREAD_EVENT_RECEIVER_H

#include "shrpx.h"

#include <openssl/ssl.h>

#include <event2/bufferevent.h>

#include "shrpx_config.h"

namespace shrpx {

class SpdySession;

struct WorkerEvent {
  evutil_socket_t client_fd;
  sockaddr_union client_addr;
  size_t client_addrlen;
};

class ThreadEventReceiver {
public:
  ThreadEventReceiver(SSL_CTX *ssl_ctx, SpdySession *spdy);
  ~ThreadEventReceiver();
  void on_read(bufferevent *bev);
private:
  SSL_CTX *ssl_ctx_;
  // Shared SPDY session for each thread. NULL if not client mode. Not
  // deleted by this object.
  SpdySession *spdy_;
};

} // namespace shrpx

#endif // SHRPX_THREAD_EVENT_RECEIVER_H
