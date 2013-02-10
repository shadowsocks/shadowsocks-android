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
#include <unistd.h>
#include <signal.h>
#include <getopt.h>

#include <cstdlib>
#include <cstring>
#include <cassert>
#include <string>
#include <iostream>
#include <string>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <spdylay/spdylay.h>

#include "spdylay_ssl.h"
#include "SpdyServer.h"

namespace spdylay {

extern bool ssl_debug;

namespace {
void print_usage(std::ostream& out)
{
  out << "Usage: spdyd [-23DVhv] [-d <PATH>] [--no-tls] <PORT> [<PRIVATE_KEY> <CERT>]"
      << std::endl;
}
} // namespace

namespace {
void print_help(std::ostream& out)
{
  print_usage(out);
  out << "\n"
      << "OPTIONS:\n"
      << "    -D, --daemon       Run in a background. If -D is used, the\n"
      << "                       current working directory is changed to '/'.\n"
      << "                       Therefore if this option is used, -d option\n"
      << "                       must be specified.\n"
      << "    -V, --verify-client\n"
      << "                       The server sends a client certificate\n"
      << "                       request. If the client did not return a\n"
      << "                       certificate, the handshake is terminated.\n"
      << "                       Currently, this option just requests a\n"
      << "                       client certificate and does not verify it.\n"
      << "    -d, --htdocs=<PATH>\n"
      << "                       Specify document root. If this option is\n"
      << "                       not specified, the document root is the\n"
      << "                       current working directory.\n"
      << "    -v, --verbose      Print debug information such as reception/\n"
      << "                       transmission of frames and name/value pairs.\n"
      << "    -2, --spdy2        Only use SPDY/2.\n"
      << "    -3, --spdy3        Only use SPDY/3.\n"
      << "    --no-tls           Disable SSL/TLS. Use -2 or -3 to specify\n"
      << "                       SPDY protocol version to use.\n"
      << "    -h, --help         Print this help.\n"
      << std::endl;
}
} // namespace

int main(int argc, char **argv)
{
  Config config;
  while(1) {
    int flag;
    static option long_options[] = {
      {"daemon", no_argument, 0, 'D' },
      {"htdocs", required_argument, 0, 'd' },
      {"help", no_argument, 0, 'h' },
      {"verbose", no_argument, 0, 'v' },
      {"spdy2", no_argument, 0, '2' },
      {"spdy3", no_argument, 0, '3' },
      {"verify-client", no_argument, 0, 'V' },
      {"no-tls", no_argument, &flag, 1 },
      {0, 0, 0, 0 }
    };
    int option_index = 0;
    int c = getopt_long(argc, argv, "DVd:hv23", long_options, &option_index);
    if(c == -1) {
      break;
    }
    switch(c) {
    case 'D':
      config.daemon = true;
      break;
    case 'V':
      config.verify_client = true;
      break;
    case 'd':
      config.htdocs = optarg;
      break;
    case 'h':
      print_help(std::cout);
      exit(EXIT_SUCCESS);
    case 'v':
      config.verbose = true;
      break;
    case '2':
      config.version = SPDYLAY_PROTO_SPDY2;
      break;
    case '3':
      config.version = SPDYLAY_PROTO_SPDY3;
      break;
    case '?':
      exit(EXIT_FAILURE);
    case 0:
      switch(flag) {
      case 1:
        // no-tls option
        config.no_tls = true;
        break;
      }
      break;
    default:
      break;
    }
  }
  if(argc-optind < (config.no_tls ? 1 : 3)) {
    print_usage(std::cerr);
    std::cerr << "Too few arguments" << std::endl;
    exit(EXIT_FAILURE);
  }

  config.port = strtol(argv[optind++], 0, 10);

  if(config.no_tls) {
    if(config.version == 0) {
      std::cerr << "Specify SPDY protocol version using either -2 or -3."
                << std::endl;
      exit(EXIT_FAILURE);
    }
  } else {
    config.private_key_file = argv[optind++];
    config.cert_file = argv[optind++];
  }

  if(config.daemon) {
    if(config.htdocs.empty()) {
      print_usage(std::cerr);
      std::cerr << "-d option must be specified when -D is used." << std::endl;
      exit(EXIT_FAILURE);
    }
    if(daemon(0, 0) == -1) {
      perror("daemon");
      exit(EXIT_FAILURE);
    }
  }
  if(config.htdocs.empty()) {
    config.htdocs = "./";
  }

  set_color_output(isatty(fileno(stdout)));

  struct sigaction act;
  memset(&act, 0, sizeof(struct sigaction));
  act.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &act, 0);
  OpenSSL_add_all_algorithms();
  SSL_load_error_strings();
  SSL_library_init();
  reset_timer();
  config.on_request_recv_callback = htdocs_on_request_recv_callback;
  ssl_debug = config.verbose;

  SpdyServer server(&config);
  if(server.listen() == 0) {
    server.run();
  }
  return 0;
}

} // namespace spdylay

int main(int argc, char **argv)
{
  return spdylay::main(argc, argv);
}
