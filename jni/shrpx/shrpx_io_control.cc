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
#include "shrpx_io_control.h"

#include <algorithm>

namespace shrpx {

IOControl::IOControl(bufferevent *bev)
  : bev_(bev),
    rdbits_(0)
{}

IOControl::~IOControl()
{}

void IOControl::set_bev(bufferevent *bev)
{
  bev_ = bev;
}

void IOControl::pause_read(IOCtrlReason reason)
{
  rdbits_ |= reason;
  if(bev_) {
    bufferevent_disable(bev_, EV_READ);
  }
}

bool IOControl::resume_read(IOCtrlReason reason)
{
  rdbits_ &= ~reason;
  if(rdbits_ == 0) {
    if(bev_) {
      bufferevent_enable(bev_, EV_READ);
    }
    return true;
  } else {
    return false;
  }
}

void IOControl::force_resume_read()
{
  rdbits_ = 0;
  if(bev_) {
    bufferevent_enable(bev_, EV_READ);
  }
}

} // namespace shrpx
