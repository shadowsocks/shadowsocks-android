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
#ifndef SHRPX_LOG_H
#define SHRPX_LOG_H

#include "shrpx.h"

namespace shrpx {

#define ENABLE_LOG 1

#define LOG_ENABLED(SEVERITY) (ENABLE_LOG && Log::log_enabled(SEVERITY))

#define LOG(SEVERITY) Log(SEVERITY, __FILE__, __LINE__)

// Listener log
#define LLOG(SEVERITY, LISTEN)                                       \
  (Log(SEVERITY, __FILE__, __LINE__) << "[LISTEN:" << LISTEN         \
   << "] ")

// ThreadEventReceiver log
#define TLOG(SEVERITY, THREAD_RECV)                                     \
  (Log(SEVERITY, __FILE__, __LINE__) << "[THREAD_RECV:" << THREAD_RECV  \
   << "] ")

// ClientHandler log
#define CLOG(SEVERITY, CLIENT_HANDLER)                                  \
  (Log(SEVERITY, __FILE__, __LINE__) << "[CLIENT_HANDLER:" << CLIENT_HANDLER \
   << "] ")

// Upstream log
#define ULOG(SEVERITY, UPSTREAM)                                        \
  (Log(SEVERITY, __FILE__, __LINE__) << "[UPSTREAM:" << UPSTREAM << "] ")

// Downstream log
#define DLOG(SEVERITY, DOWNSTREAM)                                      \
  (Log(SEVERITY, __FILE__, __LINE__) << "[DOWNSTREAM:" << DOWNSTREAM << "] ")

// Downstream connection log
#define DCLOG(SEVERITY, DCONN)                                          \
  (Log(SEVERITY, __FILE__, __LINE__) << "[DCONN:" << DCONN << "] ")

// Downstream SPDY session log
#define SSLOG(SEVERITY, SPDY)                                           \
  (Log(SEVERITY, __FILE__, __LINE__) << "[DSPDY:" << SPDY << "] ")

enum SeverityLevel {
  INFO, WARNING, ERROR, FATAL
};

class Log {
public:
  Log(int severity, const char *filename, int linenum);
  ~Log();
  template<typename Type> Log& operator<<(Type s)
  {
    return *this;
  }
  static void set_severity_level(int severity);
  static int set_severity_level_by_name(const char *name);
  static bool log_enabled(int severity)
  {
    return severity >= severity_thres_;
  }
private:
  int severity_;
  const char *filename_;
  int linenum_;
  static int severity_thres_;
};

#define TTY_HTTP_HD (get_config()->tty ? "\033[1;34m" : "")
#define TTY_RST (get_config()->tty ? "\033[0m" : "")

} // namespace shrpx

#endif // SHRPX_LOG_H
