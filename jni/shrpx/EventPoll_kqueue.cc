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
#include "EventPoll_kqueue.h"

#include <unistd.h>

#include <cerrno>
#include <cassert>

#ifdef KEVENT_UDATA_INTPTR_T
# define PTR_TO_UDATA(X) (reinterpret_cast<intptr_t>(X))
#else // !KEVENT_UDATA_INTPTR_T
# define PTR_TO_UDATA(X) (X)
#endif // !KEVENT_UDATA_INTPTR_T

namespace spdylay {

EventPoll::EventPoll(size_t max_events)
  : max_events_(max_events), num_events_(0)
{
  kq_ = kqueue();
  assert(kq_ != -1);
  evlist_ = new struct kevent[max_events_];
}

EventPoll::~EventPoll()
{
  if(kq_ != -1) {
    close(kq_);
  }
  delete [] evlist_;
}

int EventPoll::poll(int timeout)
{
  timespec ts, *ts_ptr;
  if(timeout == -1) {
    ts_ptr = 0;
  } else {
    ts.tv_sec = timeout/1000;
    ts.tv_nsec = (timeout%1000)*1000000;
    ts_ptr = &ts;
  }
  num_events_ = 0;
  int n;
  while((n = kevent(kq_, evlist_, 0, evlist_, max_events_, ts_ptr)) == -1 &&
        errno == EINTR);
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
  return reinterpret_cast<void*>(evlist_[p].udata);
}

int EventPoll::get_events(size_t p)
{
  int events = 0;
  short filter = evlist_[p].filter;
  if(filter == EVFILT_READ) {
    events |= EP_POLLIN;
  }
  if(filter == EVFILT_WRITE) {
    events |= EP_POLLOUT;
  }
  return events;
}

namespace {
int update_event(int kq, int fd, int events, void *user_data)
{
  struct kevent changelist[2];
  EV_SET(&changelist[0], fd, EVFILT_READ,
         EV_ADD | ((events & EP_POLLIN) ? EV_ENABLE : EV_DISABLE),
         0, 0, PTR_TO_UDATA(user_data));
  EV_SET(&changelist[1], fd, EVFILT_WRITE,
         EV_ADD | ((events & EP_POLLOUT) ? EV_ENABLE : EV_DISABLE),
         0, 0, PTR_TO_UDATA(user_data));
  timespec ts = { 0, 0 };
  return kevent(kq, changelist, 2, changelist, 0, &ts);
}
} // namespace

int EventPoll::ctl_event(int op, int fd, int events, void *user_data)
{
  return update_event(kq_, fd, events, user_data);
}

} // namespace spdylay
