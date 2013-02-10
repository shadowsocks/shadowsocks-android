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
#ifndef SHRPX_HTTPS_UPSTREAM_H
#define SHRPX_HTTPS_UPSTREAM_H

#include "shrpx.h"

#include <stdint.h>

#include "http-parser/http_parser.h"

#include "shrpx_upstream.h"

namespace shrpx {

class ClientHandler;

class HttpsUpstream : public Upstream {
public:
  HttpsUpstream(ClientHandler *handler);
  virtual ~HttpsUpstream();
  virtual int on_read();
  virtual int on_write();
  virtual int on_event();
  //int send();
  virtual ClientHandler* get_client_handler() const;
  virtual bufferevent_data_cb get_downstream_readcb();
  virtual bufferevent_data_cb get_downstream_writecb();
  virtual bufferevent_event_cb get_downstream_eventcb();
  void attach_downstream(Downstream *downstream);
  void delete_downstream();
  Downstream* get_downstream() const;
  int error_reply(int status_code);

  virtual void pause_read(IOCtrlReason reason);
  virtual int resume_read(IOCtrlReason reason, Downstream *downstream);

  virtual int on_downstream_header_complete(Downstream *downstream);
  virtual int on_downstream_body(Downstream *downstream,
                                 const uint8_t *data, size_t len);
  virtual int on_downstream_body_complete(Downstream *downstream);

  void reset_current_header_length();
private:
  ClientHandler *handler_;
  http_parser *htp_;
  size_t current_header_length_;
  Downstream *downstream_;
  IOControl ioctrl_;
};

} // namespace shrpx

#endif // SHRPX_HTTPS_UPSTREAM_H
