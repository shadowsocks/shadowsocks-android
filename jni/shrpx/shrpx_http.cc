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
#include "shrpx_http.h"

#include "shrpx_config.h"
#include "shrpx_log.h"
#include "util.h"

using namespace spdylay;

namespace shrpx {

namespace http {

const char* get_status_string(int status_code)
{
  switch(status_code) {
  case 100: return "100 Continue";
  case 101: return "101 Switching Protocols";
  case 200: return "200 OK";
  case 201: return "201 Created";
  case 202: return "202 Accepted";
  case 203: return "203 Non-Authoritative Information";
  case 204: return "204 No Content";
  case 205: return "205 Reset Content";
  case 206: return "206 Partial Content";
  case 300: return "300 Multiple Choices";
  case 301: return "301 Moved Permanently";
  case 302: return "302 Found";
  case 303: return "303 See Other";
  case 304: return "304 Not Modified";
  case 305: return "305 Use Proxy";
    // case 306: return "306 (Unused)";
  case 307: return "307 Temporary Redirect";
  case 400: return "400 Bad Request";
  case 401: return "401 Unauthorized";
  case 402: return "402 Payment Required";
  case 403: return "403 Forbidden";
  case 404: return "404 Not Found";
  case 405: return "405 Method Not Allowed";
  case 406: return "406 Not Acceptable";
  case 407: return "407 Proxy Authentication Required";
  case 408: return "408 Request Timeout";
  case 409: return "409 Conflict";
  case 410: return "410 Gone";
  case 411: return "411 Length Required";
  case 412: return "412 Precondition Failed";
  case 413: return "413 Request Entity Too Large";
  case 414: return "414 Request-URI Too Long";
  case 415: return "415 Unsupported Media Type";
  case 416: return "416 Requested Range Not Satisfiable";
  case 417: return "417 Expectation Failed";
  case 500: return "500 Internal Server Error";
  case 501: return "501 Not Implemented";
  case 502: return "502 Bad Gateway";
  case 503: return "503 Service Unavailable";
  case 504: return "504 Gateway Timeout";
  case 505: return "505 HTTP Version Not Supported";
  default: return "";
  }
}

std::string create_error_html(int status_code)
{
  std::string res;
  res.reserve(512);
  const char *status = http::get_status_string(status_code);
  res += "<html><head><title>";
  res += status;
  res += "</title></head><body><h1>";
  res += status;
  res += "</h1><hr><address>";
  res += get_config()->server_name;
  res += " at port ";
  res += util::utos(get_config()->port);
  res += "</address>";
  res += "</body></html>";
  return res;
}

std::string create_via_header_value(int major, int minor)
{
  std::string hdrs;
  hdrs += static_cast<char>(major+'0');
  hdrs += ".";
  hdrs += static_cast<char>(minor+'0');
  hdrs += " shrpx";
  return hdrs;
}

void capitalize(std::string& s, size_t offset)
{
  s[offset] = util::upcase(s[offset]);
  for(size_t i = offset+1, eoi = s.size(); i < eoi; ++i) {
    if(s[i-1] == '-') {
      s[i] = util::upcase(s[i]);
    } else {
      s[i] = util::lowcase(s[i]);
    }
  }
}

std::string colorizeHeaders(const char *hdrs)
{
  std::string nhdrs;
  const char *p = strchr(hdrs, '\n');
  if(!p) {
    // Not valid HTTP header
    return hdrs;
  }
  nhdrs.append(hdrs, p+1);
  ++p;
  while(1) {
    const char* np = strchr(p, ':');
    if(!np) {
      nhdrs.append(p);
      break;
    }
    nhdrs += TTY_HTTP_HD;
    nhdrs.append(p, np);
    nhdrs += TTY_RST;
    p = np;
    np = strchr(p, '\n');
    if(!np) {
      nhdrs.append(p);
      break;
    }
    nhdrs.append(p, np+1);
    p = np+1;
  }
  return nhdrs;
}

void copy_url_component(std::string& dest, http_parser_url *u, int field,
                        const char* url)
{
  if(u->field_set & (1 << field)) {
    dest.assign(url+u->field_data[field].off, u->field_data[field].len);
  }
}

} // namespace http

} // namespace shrpx
