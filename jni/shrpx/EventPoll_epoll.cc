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
#include "EventPoll_epoll.h"

#include <unistd.h>

#include <cassert>

namespace spdylay {

EventPoll::EventPoll(size_t max_events)
  : max_events_(max_events), num_events_(0)
{
  epfd_ = epoll_create(1);
  assert(epfd_ != -1);
  evlist_ = new epoll_event[max_events_];
}

EventPoll::~EventPoll()
{
  if(epfd_ != -1) {
    close(epfd_);
  }
  delete [] evlist_;
}

int EventPoll::poll(int timeout)
{
  num_events_ = 0;
  int n = epoll_wait(epfd_, evlist_, max_events_, timeout);
  if(n > 0) {
    num_events_ = n;
  }
  return n;
}

int EventPoll::get_num_events()
{
  return num_events_;
}

void* EventPoll::get_user_data(size_t p)
{
  return evlist_[p].data.ptr;
}

int EventPoll::get_events(size_t p)
{
  int events = 0;
  int revents = evlist_[p].events;
  if(revents & EPOLLIN) {
    events |= EP_POLLIN;
  }
  if(revents & EPOLLOUT) {
    events |= EP_POLLOUT;
  }
  if(revents & EPOLLHUP) {
    events |= EP_POLLHUP;
  }
  if(revents & EPOLLERR) {
    events |= EP_POLLERR;
  }
  return events;
}

namespace {
int update_event(int epfd, int op, int fd, int events, void *user_data)
{
  epoll_event ev;
  ev.events = 0;
  if(events & EP_POLLIN) {
    ev.events |= EPOLLIN;
  }
  if(events & EP_POLLOUT) {
    ev.events |= EPOLLOUT;
  }
  ev.data.ptr = user_data;
  return epoll_ctl(epfd, op, fd, &ev);
}
} // namespace

int EventPoll::ctl_event(int op, int fd, int events, void *user_data)
{
  if(op == EP_ADD) {
    op = EPOLL_CTL_ADD;
  } else if(op == EP_MOD) {
    op = EPOLL_CTL_MOD;
  } else {
    return -1;
  }
  return update_event(epfd_, op, fd, events, user_data);
}

} // namespace spdylay
