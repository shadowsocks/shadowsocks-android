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
#ifndef SHRPX_IO_CONTROL_H
#define SHRPX_IO_CONTROL_H

#include "shrpx.h"

#include <vector>

#include <event.h>
#include <event2/bufferevent.h>

namespace shrpx {

enum IOCtrlReason {
  SHRPX_NO_BUFFER = 1 << 0,
  SHRPX_MSG_BLOCK = 1 << 1
};

class IOControl {
public:
  IOControl(bufferevent *bev);
  ~IOControl();
  void set_bev(bufferevent *bev);
  void pause_read(IOCtrlReason reason);
  // Returns true if read operation is enabled after this call
  bool resume_read(IOCtrlReason reason);
  // Clear all pause flags and enable read
  void force_resume_read();
private:
  bufferevent *bev_;
  uint32_t rdbits_;
};

} // namespace shrpx

#endif // SHRPX_IO_CONTROL_H
