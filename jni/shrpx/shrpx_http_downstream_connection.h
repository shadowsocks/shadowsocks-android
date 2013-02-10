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
#ifndef SHRPX_HTTP_DOWNSTREAM_CONNECTION_H
#define SHRPX_HTTP_DOWNSTREAM_CONNECTION_H

#include "shrpx.h"

#include <event.h>
#include <event2/bufferevent.h>

#include "http-parser/http_parser.h"

#include "shrpx_downstream_connection.h"
#include "shrpx_io_control.h"

namespace shrpx {

class HttpDownstreamConnection : public DownstreamConnection {
public:
  HttpDownstreamConnection(ClientHandler *client_handler);
  virtual ~HttpDownstreamConnection();
  virtual int attach_downstream(Downstream *downstream);
  virtual void detach_downstream(Downstream *downstream);

  virtual int push_request_headers();
  virtual int push_upload_data_chunk(const uint8_t *data, size_t datalen);
  virtual int end_upload_data();

  virtual void pause_read(IOCtrlReason reason);
  virtual int resume_read(IOCtrlReason reason);
  virtual void force_resume_read();

  virtual bool get_output_buffer_full();

  virtual int on_read();
  virtual int on_write();

  bufferevent* get_bev();
private:
  bufferevent *bev_;
  IOControl ioctrl_;
  http_parser *response_htp_;
};

} // namespace shrpx

#endif // SHRPX_HTTP_DOWNSTREAM_CONNECTION_H
