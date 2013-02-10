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
#ifndef EVENT_POLL_KQUEUE_H
#define EVENT_POLL_KQUEUE_H

#include "spdylay_config.h"

#include <cstdlib>

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

#include "EventPollEvent.h"

namespace spdylay {

class EventPoll {
public:
  EventPoll(size_t max_events);
  ~EventPoll();
  // Returns 0 if this function succeeds, or -1.
  // On success
  int poll(int timeout);
  // Returns number of events detected in previous call of poll().
  int get_num_events();
  // Returns events of p-eth event.
  int get_events(size_t p);
  // Returns user data of p-th event.
  void* get_user_data(size_t p);
  // Adds/Modifies event to watch.
  int ctl_event(int op, int fd, int events, void *user_data);
private:
  int kq_;
  size_t max_events_;
  struct kevent *evlist_;
  size_t num_events_;
};

} // namespace spdylay

#endif // EVENT_POLL_KQUEUE_H
