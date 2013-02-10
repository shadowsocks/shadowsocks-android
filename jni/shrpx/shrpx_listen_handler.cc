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
#include "shrpx_listen_handler.h"

#include <unistd.h>
#include <pthread.h>
#include <string>

#include <cerrno>

#include <event2/bufferevent_ssl.h>

#include "shrpx_client_handler.h"
#include "shrpx_thread_event_receiver.h"
#include "shrpx_ssl.h"
#include "shrpx_worker.h"
#include "shrpx_config.h"
#include "shrpx_spdy_session.h"

namespace shrpx {

ListenHandler::ListenHandler(event_base *evbase, SSL_CTX *sv_ssl_ctx,
                             SSL_CTX *cl_ssl_ctx)
  : evbase_(evbase),
    sv_ssl_ctx_(sv_ssl_ctx),
    cl_ssl_ctx_(cl_ssl_ctx),
    worker_round_robin_cnt_(0),
    workers_(0),
    num_worker_(0),
    spdy_(0)
{}

ListenHandler::~ListenHandler()
{}

void ListenHandler::create_worker_thread(size_t num)
{
  workers_ = new WorkerInfo[num];
  num_worker_ = 0;
  for(size_t i = 0; i < num; ++i) {
    int rv;
    pthread_t thread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    WorkerInfo *info = &workers_[num_worker_];
    rv = socketpair(AF_UNIX, SOCK_STREAM, 0, info->sv);
    if(rv == -1) {
      LLOG(ERROR, this) << "socketpair() failed: " << strerror(errno);
      continue;
    }
    info->sv_ssl_ctx = sv_ssl_ctx_;
    info->cl_ssl_ctx = cl_ssl_ctx_;
    rv = pthread_create(&thread, &attr, start_threaded_worker, info);
    if(rv != 0) {
      LLOG(ERROR, this) << "pthread_create() failed: " << strerror(rv);
      for(size_t j = 0; j < 2; ++j) {
        close(info->sv[j]);
      }
      continue;
    }
    bufferevent *bev = bufferevent_socket_new(evbase_, info->sv[0],
                                              BEV_OPT_DEFER_CALLBACKS);
    info->bev = bev;
    if(LOG_ENABLED(INFO)) {
      LLOG(INFO, this) << "Created thread #" << num_worker_;
    }
    ++num_worker_;
  }
}

int ListenHandler::accept_connection(evutil_socket_t fd,
                                     sockaddr *addr, int addrlen)
{
  if(LOG_ENABLED(INFO)) {
    LLOG(INFO, this) << "Accepted connection. fd=" << fd;
  }
  if(num_worker_ == 0) {
    ClientHandler* client = ssl::accept_connection(evbase_, sv_ssl_ctx_,
                                                   fd, addr, addrlen);
    client->set_spdy_session(spdy_);
  } else {
    size_t idx = worker_round_robin_cnt_ % num_worker_;
    ++worker_round_robin_cnt_;
    WorkerEvent wev;
    memset(&wev, 0, sizeof(wev));
    wev.client_fd = fd;
    memcpy(&wev.client_addr, addr, addrlen);
    wev.client_addrlen = addrlen;
    evbuffer *output = bufferevent_get_output(workers_[idx].bev);
    if(evbuffer_add(output, &wev, sizeof(wev)) != 0) {
      LLOG(FATAL, this) << "evbuffer_add() failed";
      return -1;
    }
  }
  return 0;
}

event_base* ListenHandler::get_evbase() const
{
  return evbase_;
}

int ListenHandler::create_spdy_session()
{
  int rv;
  spdy_ = new SpdySession(evbase_, cl_ssl_ctx_);
  rv = spdy_->init_notification();
  return rv;
}

} // namespace shrpx
