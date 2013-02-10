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
#include "shrpx_thread_event_receiver.h"

#include <unistd.h>

#include "shrpx_ssl.h"
#include "shrpx_log.h"
#include "shrpx_client_handler.h"
#include "shrpx_spdy_session.h"

namespace shrpx {

ThreadEventReceiver::ThreadEventReceiver(SSL_CTX *ssl_ctx, SpdySession *spdy)
  : ssl_ctx_(ssl_ctx),
    spdy_(spdy)
{}

ThreadEventReceiver::~ThreadEventReceiver()
{}

void ThreadEventReceiver::on_read(bufferevent *bev)
{
  evbuffer *input = bufferevent_get_input(bev);
  while(evbuffer_get_length(input) >= sizeof(WorkerEvent)) {
    WorkerEvent wev;
    int nread = evbuffer_remove(input, &wev, sizeof(wev));
    if(nread != sizeof(wev)) {
      TLOG(FATAL, this) << "evbuffer_remove() removed fewer bytes. Expected:"
                        << sizeof(wev) << " Actual:" << nread;
      continue;
    }
    if(LOG_ENABLED(INFO)) {
      TLOG(INFO, this) << "WorkerEvent: client_fd=" << wev.client_fd
                       << ", addrlen=" << wev.client_addrlen;
    }
    event_base *evbase = bufferevent_get_base(bev);
    ClientHandler *client_handler;
    client_handler = ssl::accept_connection(evbase, ssl_ctx_,
                                            wev.client_fd,
                                            &wev.client_addr.sa,
                                            wev.client_addrlen);
    if(client_handler) {
      client_handler->set_spdy_session(spdy_);
      if(LOG_ENABLED(INFO)) {
        TLOG(INFO, this) << "CLIENT_HANDLER:" << client_handler << " created";
      }
    } else {
      if(LOG_ENABLED(INFO)) {
        TLOG(ERROR, this) << "ClientHandler creation failed";
      }
      close(wev.client_fd);
    }
  }
}

} // namespace shrpx
