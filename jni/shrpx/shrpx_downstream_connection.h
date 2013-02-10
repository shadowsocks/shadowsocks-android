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
#ifndef SHRPX_DOWNSTREAM_CONNECTION_H
#define SHRPX_DOWNSTREAM_CONNECTION_H

#include "shrpx.h"

#include "shrpx_io_control.h"

namespace shrpx {

class ClientHandler;
class Downstream;

class DownstreamConnection {
public:
  DownstreamConnection(ClientHandler *client_handler);
  virtual ~DownstreamConnection();
  virtual int attach_downstream(Downstream *downstream) = 0;
  virtual void detach_downstream(Downstream *downstream) = 0;

  virtual int push_request_headers() = 0;
  virtual int push_upload_data_chunk(const uint8_t *data, size_t datalen) = 0;
  virtual int end_upload_data() = 0;

  virtual void pause_read(IOCtrlReason reason) = 0;
  virtual int resume_read(IOCtrlReason reason) = 0;
  virtual void force_resume_read() = 0;

  virtual bool get_output_buffer_full() = 0;

  virtual int on_read() = 0;
  virtual int on_write() = 0;

  ClientHandler* get_client_handler();
  Downstream* get_downstream();
protected:
  ClientHandler *client_handler_;
  Downstream *downstream_;
};

} // namespace shrpx

#endif // SHRPX_DOWNSTREAM_CONNECTION_H
