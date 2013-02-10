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
#include "shrpx_log.h"

#include <syslog.h>

#include <cstdio>
#include <cstring>

#include "shrpx_config.h"

namespace shrpx {

namespace {
const char *SEVERITY_STR[] = {
  "INFO", "WARN", "ERROR", "FATAL"
};
} // namespace

namespace {
const char *SEVERITY_COLOR[] = {
  "\033[1;32m", // INFO
  "\033[1;33m", // WARN
  "\033[1;31m", // ERROR
  "\033[1;35m", // FATAL
};
} // namespace

int Log::severity_thres_ = WARNING;

void Log::set_severity_level(int severity)
{
  severity_thres_ = severity;
}

int Log::set_severity_level_by_name(const char *name)
{
  for(size_t i = 0, max = sizeof(SEVERITY_STR)/sizeof(char*); i < max;  ++i) {
    if(strcmp(SEVERITY_STR[i], name) == 0) {
      severity_thres_ = i;
      return 0;
    }
  }
  return -1;
}

int severity_to_syslog_level(int severity)
{
  switch(severity) {
  case(INFO):
    return LOG_INFO;
  case(WARNING):
    return LOG_WARNING;
  case(ERROR):
    return LOG_ERR;
  case(FATAL):
    return LOG_CRIT;
  default:
    return -1;
  }
}

Log::Log(int severity, const char *filename, int linenum)
  : severity_(severity),
    filename_(filename),
    linenum_(linenum)
{}

Log::~Log()
{
}

} // namespace shrpx
