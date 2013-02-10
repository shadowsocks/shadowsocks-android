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
#include "shrpx_accesslog.h"

#include <syslog.h>

#include <ctime>
#include <cstdio>
#include <cstring>

#include "shrpx_config.h"
#include "shrpx_downstream.h"

namespace shrpx {

namespace {
void get_datestr(char *buf)
{
  time_t now = time(0);
  if(ctime_r(&now, buf) == 0) {
    buf[0] = '\0';
  } else {
    size_t len = strlen(buf);
    if(len == 0) {
      buf[0] = '\0';
    } else {
      buf[strlen(buf)-1] = '\0';
    }
  }
}
} // namespace

void upstream_connect(const std::string& client_ip)
{
  char datestr[64];
  get_datestr(datestr);
  fprintf(stderr, "%s [%s] ACCEPT\n", client_ip.c_str(), datestr);
  fflush(stderr);
  if(get_config()->use_syslog) {
    syslog(LOG_INFO, "%s ACCEPT\n", client_ip.c_str());
  }
}

namespace {
const char* status_code_color(int status_code)
{
  if(status_code <= 199) {
    return "\033[1;36m";
  } else if(status_code <= 299) {
    return "\033[1;32m";
  } else if(status_code <= 399) {
    return "\033[1;34m";
  } else if(status_code <= 499) {
    return "\033[1;31m";
  } else if(status_code <= 599) {
    return "\033[1;35m";
  } else {
    return "";
  }
}
} // namespace

void upstream_response(const std::string& client_ip, int status_code,
                       Downstream *downstream)
{
  char datestr[64];
  get_datestr(datestr);
  if(downstream) {
    fprintf(stderr, "%s%s [%s] %d%s %d \"%s %s HTTP/%u.%u\"\n",
            get_config()->tty ? status_code_color(status_code) : "",
            client_ip.c_str(), datestr,
            status_code,
            get_config()->tty ? "\033[0m" : "",
            downstream->get_stream_id(),
            downstream->get_request_method().c_str(),
            downstream->get_request_path().c_str(),
            downstream->get_request_major(),
            downstream->get_request_minor());
    fflush(stderr);
    if(get_config()->use_syslog) {
      syslog(LOG_INFO, "%s %d %d \"%s %s HTTP/%u.%u\"\n",
            client_ip.c_str(),
            status_code,
            downstream->get_stream_id(),
            downstream->get_request_method().c_str(),
            downstream->get_request_path().c_str(),
            downstream->get_request_major(),
            downstream->get_request_minor());
    }
  } else {
    fprintf(stderr, "%s%s [%s] %d%s 0 \"-\"\n",
            get_config()->tty ? status_code_color(status_code) : "",
            client_ip.c_str(), datestr,
            status_code,
            get_config()->tty ? "\033[0m" : "");
    if(get_config()->use_syslog) {
      syslog(LOG_INFO, "%s %d 0 \"-\"\n", client_ip.c_str(), status_code);
    }
    fflush(stderr);
  }
}

} // namespace shrpx
