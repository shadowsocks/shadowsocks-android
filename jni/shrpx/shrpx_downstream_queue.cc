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
#include "shrpx_downstream_queue.h"

#include <cassert>

#include "shrpx_downstream.h"

namespace shrpx {

DownstreamQueue::DownstreamQueue()
{}

DownstreamQueue::~DownstreamQueue()
{
  for(std::map<int32_t, Downstream*>::iterator i = downstreams_.begin();
      i != downstreams_.end(); ++i) {
    delete (*i).second;
  }
}

void DownstreamQueue::add(Downstream *downstream)
{
  downstreams_[downstream->get_stream_id()] = downstream;
}

void DownstreamQueue::remove(Downstream *downstream)
{
  downstreams_.erase(downstream->get_stream_id());
}

Downstream* DownstreamQueue::find(int32_t stream_id)
{
  std::map<int32_t, Downstream*>::iterator i = downstreams_.find(stream_id);
  if(i == downstreams_.end()) {
    return 0;
  } else {
    return (*i).second;
  }
}

} // namespace shrpx
