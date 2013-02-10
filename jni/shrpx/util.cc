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
#include "util.h"

#include <time.h>

#include <cstdio>
#include <cstring>

namespace spdylay {

namespace util {

const char DEFAULT_STRIP_CHARSET[] = "\r\n\t ";

time_t timegm(struct tm *tm)
{
    time_t ret;
    char *tz;

    tz = getenv("TZ");
    setenv("TZ", "", 1);
    tzset();
    ret = mktime(tm);
    if (tz)
        setenv("TZ", tz, 1);
    else
        unsetenv("TZ");
    tzset();
    return ret;
}

bool isAlpha(const char c)
{
  return ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z');
}

bool isDigit(const char c)
{
  return '0' <= c && c <= '9';
}

bool isHexDigit(const char c)
{
  return isDigit(c) || ('A' <= c && c <= 'F') || ('a' <= c && c <= 'f');
}

bool inRFC3986UnreservedChars(const char c)
{
  static const char unreserved[] = { '-', '.', '_', '~' };
  return isAlpha(c) || isDigit(c) ||
    std::find(&unreserved[0], &unreserved[4], c) != &unreserved[4];
}

std::string percentEncode(const unsigned char* target, size_t len)
{
  std::string dest;
  for(size_t i = 0; i < len; ++i) {
    if(inRFC3986UnreservedChars(target[i])) {
      dest += target[i];
    } else {
      char temp[4];
      snprintf(temp, sizeof(temp), "%%%02X", target[i]);
      dest.append(temp);
      //dest.append(fmt("%%%02X", target[i]));
    }
  }
  return dest;
}

std::string percentEncode(const std::string& target)
{
  return percentEncode(reinterpret_cast<const unsigned char*>(target.c_str()),
                       target.size());
}

std::string percentDecode
(std::string::const_iterator first, std::string::const_iterator last)
{
  std::string result;
  for(; first != last; ++first) {
    if(*first == '%') {
      if(first+1 != last && first+2 != last &&
         isHexDigit(*(first+1)) && isHexDigit(*(first+2))) {
        std::string numstr(first+1, first+3);
        result += strtol(numstr.c_str(), 0, 16);
        first += 2;
      } else {
        result += *first;
      }
    } else {
      result += *first;
    }
  }
  return result;
}

std::string http_date(time_t t)
{
  char buf[32];
  tm* tms = gmtime(&t); // returned struct is statically allocated.
  size_t r = strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", tms);
  return std::string(&buf[0], &buf[r]);
}

time_t parse_http_date(const std::string& s)
{
  tm tm;
  memset(&tm, 0, sizeof(tm));
  char* r = strptime(s.c_str(), "%a, %d %b %Y %H:%M:%S GMT", &tm);
  if(r == 0) {
    return 0;
  }
  return timegm(&tm);
}

bool startsWith(const std::string& a, const std::string& b)
{
  return startsWith(a.begin(), a.end(), b.begin(), b.end());
}

bool istartsWith(const std::string& a, const std::string& b)
{
  return istartsWith(a.begin(), a.end(), b.begin(), b.end());
}

namespace {
void streq_advance(const char **ap, const char **bp)
{
  for(; **ap && **bp && lowcase(**ap) == lowcase(**bp); ++*ap, ++*bp);
}
} // namespace

bool istartsWith(const char *a, const char* b)
{
  if(!a || !b) {
    return false;
  }
  streq_advance(&a, &b);
  return !*b;
}

bool endsWith(const std::string& a, const std::string& b)
{
  return endsWith(a.begin(), a.end(), b.begin(), b.end());
}
bool strieq(const char *a, const char *b)
{
  if(!a || !b) {
    return false;
  }
  for(; *a && *b && lowcase(*a) == lowcase(*b); ++a, ++b);
  return !*a && !*b;
}

bool strifind(const char *a, const char *b)
{
  if(!a || !b) {
    return false;
  }
  for(size_t i = 0; a[i]; ++i) {
    const char *ap = &a[i], *bp = b;
    for(; *ap && *bp && lowcase(*ap) == lowcase(*bp); ++ap, ++bp);
    if(!*bp) {
      return true;
    }
  }
  return false;
}

char upcase(char c)
{
  if('a' <= c && c <= 'z') {
    return c-'a'+'A';
  } else {
    return c;
  }
}

} // namespace util

} // namespace spdylay
