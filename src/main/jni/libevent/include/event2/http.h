/*
 * Copyright (c) 2000-2007 Niels Provos <provos@citi.umich.edu>
 * Copyright (c) 2007-2012 Niels Provos and Nick Mathewson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _EVENT2_HTTP_H_
#define _EVENT2_HTTP_H_

/* For int types. */
#include <event2/util.h>

#ifdef __cplusplus
extern "C" {
#endif

/* In case we haven't included the right headers yet. */
struct evbuffer;
struct event_base;

/** @file event2/http.h
 *
 * Basic support for HTTP serving.
 *
 * As Libevent is a library for dealing with event notification and most
 * interesting applications are networked today, I have often found the
 * need to write HTTP code.  The following prototypes and definitions provide
 * an application with a minimal interface for making HTTP requests and for
 * creating a very simple HTTP server.
 */

/* Response codes */
#define HTTP_OK			200	/**< request completed ok */
#define HTTP_NOCONTENT		204	/**< request does not have content */
#define HTTP_MOVEPERM		301	/**< the uri moved permanently */
#define HTTP_MOVETEMP		302	/**< the uri moved temporarily */
#define HTTP_NOTMODIFIED	304	/**< page was not modified from last */
#define HTTP_BADREQUEST		400	/**< invalid http request was made */
#define HTTP_NOTFOUND		404	/**< could not find content for uri */
#define HTTP_BADMETHOD		405 	/**< method not allowed for this uri */
#define HTTP_ENTITYTOOLARGE	413	/**<  */
#define HTTP_EXPECTATIONFAILED	417	/**< we can't handle this expectation */
#define HTTP_INTERNAL           500     /**< internal error */
#define HTTP_NOTIMPLEMENTED     501     /**< not implemented */
#define HTTP_SERVUNAVAIL	503	/**< the server is not available */

struct evhttp;
struct evhttp_request;
struct evkeyvalq;
struct evhttp_bound_socket;
struct evconnlistener;

/**
 * Create a new HTTP server.
 *
 * @param base (optional) the event base to receive the HTTP events
 * @return a pointer to a newly initialized evhttp server structure
 * @see evhttp_free()
 */
struct evhttp *evhttp_new(struct event_base *base);

/**
 * Binds an HTTP server on the specified address and port.
 *
 * Can be called multiple times to bind the same http server
 * to multiple different ports.
 *
 * @param http a pointer to an evhttp object
 * @param address a string containing the IP address to listen(2) on
 * @param port the port number to listen on
 * @return 0 on success, -1 on failure.
 * @see evhttp_accept_socket()
 */
int evhttp_bind_socket(struct evhttp *http, const char *address, ev_uint16_t port);

/**
 * Like evhttp_bind_socket(), but returns a handle for referencing the socket.
 *
 * The returned pointer is not valid after \a http is freed.
 *
 * @param http a pointer to an evhttp object
 * @param address a string containing the IP address to listen(2) on
 * @param port the port number to listen on
 * @return Handle for the socket on success, NULL on failure.
 * @see evhttp_bind_socket(), evhttp_del_accept_socket()
 */
struct evhttp_bound_socket *evhttp_bind_socket_with_handle(struct evhttp *http, const char *address, ev_uint16_t port);

/**
 * Makes an HTTP server accept connections on the specified socket.
 *
 * This may be useful to create a socket and then fork multiple instances
 * of an http server, or when a socket has been communicated via file
 * descriptor passing in situations where an http servers does not have
 * permissions to bind to a low-numbered port.
 *
 * Can be called multiple times to have the http server listen to
 * multiple different sockets.
 *
 * @param http a pointer to an evhttp object
 * @param fd a socket fd that is ready for accepting connections
 * @return 0 on success, -1 on failure.
 * @see evhttp_bind_socket()
 */
int evhttp_accept_socket(struct evhttp *http, evutil_socket_t fd);

/**
 * Like evhttp_accept_socket(), but returns a handle for referencing the socket.
 *
 * The returned pointer is not valid after \a http is freed.
 *
 * @param http a pointer to an evhttp object
 * @param fd a socket fd that is ready for accepting connections
 * @return Handle for the socket on success, NULL on failure.
 * @see evhttp_accept_socket(), evhttp_del_accept_socket()
 */
struct evhttp_bound_socket *evhttp_accept_socket_with_handle(struct evhttp *http, evutil_socket_t fd);

/**
 * The most low-level evhttp_bind/accept method: takes an evconnlistener, and
 * returns an evhttp_bound_socket.  The listener will be freed when the bound
 * socket is freed.
 */
struct evhttp_bound_socket *evhttp_bind_listener(struct evhttp *http, struct evconnlistener *listener);

/**
 * Return the listener used to implement a bound socket.
 */
struct evconnlistener *evhttp_bound_socket_get_listener(struct evhttp_bound_socket *bound);

/**
 * Makes an HTTP server stop accepting connections on the specified socket
 *
 * This may be useful when a socket has been sent via file descriptor passing
 * and is no longer needed by the current process.
 *
 * If you created this bound socket with evhttp_bind_socket_with_handle or
 * evhttp_accept_socket_with_handle, this function closes the fd you provided.
 * If you created this bound socket with evhttp_bind_listener, this function
 * frees the listener you provided.
 *
 * \a bound_socket is an invalid pointer after this call returns.
 *
 * @param http a pointer to an evhttp object
 * @param bound_socket a handle returned by evhttp_{bind,accept}_socket_with_handle
 * @see evhttp_bind_socket_with_handle(), evhttp_accept_socket_with_handle()
 */
void evhttp_del_accept_socket(struct evhttp *http, struct evhttp_bound_socket *bound_socket);

/**
 * Get the raw file descriptor referenced by an evhttp_bound_socket.
 *
 * @param bound_socket a handle returned by evhttp_{bind,accept}_socket_with_handle
 * @return the file descriptor used by the bound socket
 * @see evhttp_bind_socket_with_handle(), evhttp_accept_socket_with_handle()
 */
evutil_socket_t evhttp_bound_socket_get_fd(struct evhttp_bound_socket *bound_socket);

/**
 * Free the previously created HTTP server.
 *
 * Works only if no requests are currently being served.
 *
 * @param http the evhttp server object to be freed
 * @see evhttp_start()
 */
void evhttp_free(struct evhttp* http);

/** XXX Document. */
void evhttp_set_max_headers_size(struct evhttp* http, ev_ssize_t max_headers_size);
/** XXX Document. */
void evhttp_set_max_body_size(struct evhttp* http, ev_ssize_t max_body_size);

/**
  Sets the what HTTP methods are supported in requests accepted by this
  server, and passed to user callbacks.

  If not supported they will generate a "405 Method not allowed" response.

  By default this includes the following methods: GET, POST, HEAD, PUT, DELETE

  @param http the http server on which to set the methods
  @param methods bit mask constructed from evhttp_cmd_type values
*/
void evhttp_set_allowed_methods(struct evhttp* http, ev_uint16_t methods);

/**
   Set a callback for a specified URI

   @param http the http sever on which to set the callback
   @param path the path for which to invoke the callback
   @param cb the callback function that gets invoked on requesting path
   @param cb_arg an additional context argument for the callback
   @return 0 on success, -1 if the callback existed already, -2 on failure
*/
int evhttp_set_cb(struct evhttp *http, const char *path,
    void (*cb)(struct evhttp_request *, void *), void *cb_arg);

/** Removes the callback for a specified URI */
int evhttp_del_cb(struct evhttp *, const char *);

/**
    Set a callback for all requests that are not caught by specific callbacks

    Invokes the specified callback for all requests that do not match any of
    the previously specified request paths.  This is catchall for requests not
    specifically configured with evhttp_set_cb().

    @param http the evhttp server object for which to set the callback
    @param cb the callback to invoke for any unmatched requests
    @param arg an context argument for the callback
*/
void evhttp_set_gencb(struct evhttp *http,
    void (*cb)(struct evhttp_request *, void *), void *arg);

/**
   Adds a virtual host to the http server.

   A virtual host is a newly initialized evhttp object that has request
   callbacks set on it via evhttp_set_cb() or evhttp_set_gencb().  It
   most not have any listing sockets associated with it.

   If the virtual host has not been removed by the time that evhttp_free()
   is called on the main http server, it will be automatically freed, too.

   It is possible to have hierarchical vhosts.  For example: A vhost
   with the pattern *.example.com may have other vhosts with patterns
   foo.example.com and bar.example.com associated with it.

   @param http the evhttp object to which to add a virtual host
   @param pattern the glob pattern against which the hostname is matched.
     The match is case insensitive and follows otherwise regular shell
     matching.
   @param vhost the virtual host to add the regular http server.
   @return 0 on success, -1 on failure
   @see evhttp_remove_virtual_host()
*/
int evhttp_add_virtual_host(struct evhttp* http, const char *pattern,
    struct evhttp* vhost);

/**
   Removes a virtual host from the http server.

   @param http the evhttp object from which to remove the virtual host
   @param vhost the virtual host to remove from the regular http server.
   @return 0 on success, -1 on failure
   @see evhttp_add_virtual_host()
*/
int evhttp_remove_virtual_host(struct evhttp* http, struct evhttp* vhost);

/**
   Add a server alias to an http object. The http object can be a virtual
   host or the main server.

   @param http the evhttp object
   @param alias the alias to add
   @see evhttp_add_remove_alias()
*/
int evhttp_add_server_alias(struct evhttp *http, const char *alias);

/**
   Remove a server alias from an http object.

   @param http the evhttp object
   @param alias the alias to remove
   @see evhttp_add_server_alias()
*/
int evhttp_remove_server_alias(struct evhttp *http, const char *alias);

/**
 * Set the timeout for an HTTP request.
 *
 * @param http an evhttp object
 * @param timeout_in_secs the timeout, in seconds
 */
void evhttp_set_timeout(struct evhttp *http, int timeout_in_secs);

/* Request/Response functionality */

/**
 * Send an HTML error message to the client.
 *
 * @param req a request object
 * @param error the HTTP error code
 * @param reason a brief explanation of the error.  If this is NULL, we'll
 *    just use the standard meaning of the error code.
 */
void evhttp_send_error(struct evhttp_request *req, int error,
    const char *reason);

/**
 * Send an HTML reply to the client.
 *
 * The body of the reply consists of the data in databuf.  After calling
 * evhttp_send_reply() databuf will be empty, but the buffer is still
 * owned by the caller and needs to be deallocated by the caller if
 * necessary.
 *
 * @param req a request object
 * @param code the HTTP response code to send
 * @param reason a brief message to send with the response code
 * @param databuf the body of the response
 */
void evhttp_send_reply(struct evhttp_request *req, int code,
    const char *reason, struct evbuffer *databuf);

/* Low-level response interface, for streaming/chunked replies */

/**
   Initiate a reply that uses Transfer-Encoding chunked.

   This allows the caller to stream the reply back to the client and is
   useful when either not all of the reply data is immediately available
   or when sending very large replies.

   The caller needs to supply data chunks with evhttp_send_reply_chunk()
   and complete the reply by calling evhttp_send_reply_end().

   @param req a request object
   @param code the HTTP response code to send
   @param reason a brief message to send with the response code
*/
void evhttp_send_reply_start(struct evhttp_request *req, int code,
    const char *reason);

/**
   Send another data chunk as part of an ongoing chunked reply.

   The reply chunk consists of the data in databuf.  After calling
   evhttp_send_reply_chunk() databuf will be empty, but the buffer is
   still owned by the caller and needs to be deallocated by the caller
   if necessary.

   @param req a request object
   @param databuf the data chunk to send as part of the reply.
*/
void evhttp_send_reply_chunk(struct evhttp_request *req,
    struct evbuffer *databuf);
/**
   Complete a chunked reply, freeing the request as appropriate.

   @param req a request object
*/
void evhttp_send_reply_end(struct evhttp_request *req);

/*
 * Interfaces for making requests
 */

/** The different request types supported by evhttp.  These are as specified
 * in RFC2616, except for PATCH which is specified by RFC5789.
 *
 * By default, only some of these methods are accepted and passed to user
 * callbacks; use evhttp_set_allowed_methods() to change which methods
 * are allowed.
 */
enum evhttp_cmd_type {
	EVHTTP_REQ_GET     = 1 << 0,
	EVHTTP_REQ_POST    = 1 << 1,
	EVHTTP_REQ_HEAD    = 1 << 2,
	EVHTTP_REQ_PUT     = 1 << 3,
	EVHTTP_REQ_DELETE  = 1 << 4,
	EVHTTP_REQ_OPTIONS = 1 << 5,
	EVHTTP_REQ_TRACE   = 1 << 6,
	EVHTTP_REQ_CONNECT = 1 << 7,
	EVHTTP_REQ_PATCH   = 1 << 8
};

/** a request object can represent either a request or a reply */
enum evhttp_request_kind { EVHTTP_REQUEST, EVHTTP_RESPONSE };

/**
 * Creates a new request object that needs to be filled in with the request
 * parameters.  The callback is executed when the request completed or an
 * error occurred.
 */
struct evhttp_request *evhttp_request_new(
	void (*cb)(struct evhttp_request *, void *), void *arg);

/**
 * Enable delivery of chunks to requestor.
 * @param cb will be called after every read of data with the same argument
 *           as the completion callback. Will never be called on an empty
 *           response. May drain the input buffer; it will be drained
 *           automatically on return.
 */
void evhttp_request_set_chunked_cb(struct evhttp_request *,
    void (*cb)(struct evhttp_request *, void *));

/** Frees the request object and removes associated events. */
void evhttp_request_free(struct evhttp_request *req);

struct evdns_base;

/**
 * A connection object that can be used to for making HTTP requests.  The
 * connection object tries to resolve address and establish the connection
 * when it is given an http request object.
 *
 * @param base the event_base to use for handling the connection
 * @param dnsbase the dns_base to use for resolving host names; if not
 *     specified host name resolution will block.
 * @param address the address to which to connect
 * @param port the port to connect to
 * @return an evhttp_connection object that can be used for making requests
 */
struct evhttp_connection *evhttp_connection_base_new(
	struct event_base *base, struct evdns_base *dnsbase,
	const char *address, unsigned short port);

/**
 * Return the bufferevent that an evhttp_connection is using.
 */
struct bufferevent *evhttp_connection_get_bufferevent(
	struct evhttp_connection *evcon);

/** Takes ownership of the request object
 *
 * Can be used in a request callback to keep onto the request until
 * evhttp_request_free() is explicitly called by the user.
 */
void evhttp_request_own(struct evhttp_request *req);

/** Returns 1 if the request is owned by the user */
int evhttp_request_is_owned(struct evhttp_request *req);

/**
 * Returns the connection object associated with the request or NULL
 *
 * The user needs to either free the request explicitly or call
 * evhttp_send_reply_end().
 */
struct evhttp_connection *evhttp_request_get_connection(struct evhttp_request *req);

/**
 * Returns the underlying event_base for this connection
 */
struct event_base *evhttp_connection_get_base(struct evhttp_connection *req);

void evhttp_connection_set_max_headers_size(struct evhttp_connection *evcon,
    ev_ssize_t new_max_headers_size);

void evhttp_connection_set_max_body_size(struct evhttp_connection* evcon,
    ev_ssize_t new_max_body_size);

/** Frees an http connection */
void evhttp_connection_free(struct evhttp_connection *evcon);

/** sets the ip address from which http connections are made */
void evhttp_connection_set_local_address(struct evhttp_connection *evcon,
    const char *address);

/** sets the local port from which http connections are made */
void evhttp_connection_set_local_port(struct evhttp_connection *evcon,
    ev_uint16_t port);

/** Sets the timeout for events related to this connection */
void evhttp_connection_set_timeout(struct evhttp_connection *evcon,
    int timeout_in_secs);

/** Sets the retry limit for this connection - -1 repeats indefinitely */
void evhttp_connection_set_retries(struct evhttp_connection *evcon,
    int retry_max);

/** Set a callback for connection close. */
void evhttp_connection_set_closecb(struct evhttp_connection *evcon,
    void (*)(struct evhttp_connection *, void *), void *);

/** Get the remote address and port associated with this connection. */
void evhttp_connection_get_peer(struct evhttp_connection *evcon,
    char **address, ev_uint16_t *port);

/**
    Make an HTTP request over the specified connection.

    The connection gets ownership of the request.  On failure, the
    request object is no longer valid as it has been freed.

    @param evcon the evhttp_connection object over which to send the request
    @param req the previously created and configured request object
    @param type the request type EVHTTP_REQ_GET, EVHTTP_REQ_POST, etc.
    @param uri the URI associated with the request
    @return 0 on success, -1 on failure
    @see evhttp_cancel_request()
*/
int evhttp_make_request(struct evhttp_connection *evcon,
    struct evhttp_request *req,
    enum evhttp_cmd_type type, const char *uri);

/**
   Cancels a pending HTTP request.

   Cancels an ongoing HTTP request.  The callback associated with this request
   is not executed and the request object is freed.  If the request is
   currently being processed, e.g. it is ongoing, the corresponding
   evhttp_connection object is going to get reset.

   A request cannot be canceled if its callback has executed already. A request
   may be canceled reentrantly from its chunked callback.

   @param req the evhttp_request to cancel; req becomes invalid after this call.
*/
void evhttp_cancel_request(struct evhttp_request *req);

/**
 * A structure to hold a parsed URI or Relative-Ref conforming to RFC3986.
 */
struct evhttp_uri;

/** Returns the request URI */
const char *evhttp_request_get_uri(const struct evhttp_request *req);
/** Returns the request URI (parsed) */
const struct evhttp_uri *evhttp_request_get_evhttp_uri(const struct evhttp_request *req);
/** Returns the request command */
enum evhttp_cmd_type evhttp_request_get_command(const struct evhttp_request *req);

int evhttp_request_get_response_code(const struct evhttp_request *req);

/** Returns the input headers */
struct evkeyvalq *evhttp_request_get_input_headers(struct evhttp_request *req);
/** Returns the output headers */
struct evkeyvalq *evhttp_request_get_output_headers(struct evhttp_request *req);
/** Returns the input buffer */
struct evbuffer *evhttp_request_get_input_buffer(struct evhttp_request *req);
/** Returns the output buffer */
struct evbuffer *evhttp_request_get_output_buffer(struct evhttp_request *req);
/** Returns the host associated with the request. If a client sends an absolute
    URI, the host part of that is preferred. Otherwise, the input headers are
    searched for a Host: header. NULL is returned if no absolute URI or Host:
    header is provided. */
const char *evhttp_request_get_host(struct evhttp_request *req);

/* Interfaces for dealing with HTTP headers */

/**
   Finds the value belonging to a header.

   @param headers the evkeyvalq object in which to find the header
   @param key the name of the header to find
   @returns a pointer to the value for the header or NULL if the header
     count not be found.
   @see evhttp_add_header(), evhttp_remove_header()
*/
const char *evhttp_find_header(const struct evkeyvalq *headers,
    const char *key);

/**
   Removes a header from a list of existing headers.

   @param headers the evkeyvalq object from which to remove a header
   @param key the name of the header to remove
   @returns 0 if the header was removed, -1  otherwise.
   @see evhttp_find_header(), evhttp_add_header()
*/
int evhttp_remove_header(struct evkeyvalq *headers, const char *key);

/**
   Adds a header to a list of existing headers.

   @param headers the evkeyvalq object to which to add a header
   @param key the name of the header
   @param value the value belonging to the header
   @returns 0 on success, -1  otherwise.
   @see evhttp_find_header(), evhttp_clear_headers()
*/
int evhttp_add_header(struct evkeyvalq *headers, const char *key, const char *value);

/**
   Removes all headers from the header list.

   @param headers the evkeyvalq object from which to remove all headers
*/
void evhttp_clear_headers(struct evkeyvalq *headers);

/* Miscellaneous utility functions */


/**
   Helper function to encode a string for inclusion in a URI.  All
   characters are replaced by their hex-escaped (%22) equivalents,
   except for characters explicitly unreserved by RFC3986 -- that is,
   ASCII alphanumeric characters, hyphen, dot, underscore, and tilde.

   The returned string must be freed by the caller.

   @param str an unencoded string
   @return a newly allocated URI-encoded string or NULL on failure
 */
char *evhttp_encode_uri(const char *str);

/**
   As evhttp_encode_uri, but if 'size' is nonnegative, treat the string
   as being 'size' bytes long.  This allows you to encode strings that
   may contain 0-valued bytes.

   The returned string must be freed by the caller.

   @param str an unencoded string
   @param size the length of the string to encode, or -1 if the string
      is NUL-terminated
   @param space_to_plus if true, space characters in 'str' are encoded
      as +, not %20.
   @return a newly allocate URI-encoded string, or NULL on failure.
 */
char *evhttp_uriencode(const char *str, ev_ssize_t size, int space_to_plus);

/**
  Helper function to sort of decode a URI-encoded string.  Unlike
  evhttp_get_decoded_uri, it decodes all plus characters that appear
  _after_ the first question mark character, but no plusses that occur
  before.  This is not a good way to decode URIs in whole or in part.

  The returned string must be freed by the caller

  @deprecated  This function is deprecated; you probably want to use
     evhttp_get_decoded_uri instead.

  @param uri an encoded URI
  @return a newly allocated unencoded URI or NULL on failure
 */
char *evhttp_decode_uri(const char *uri);

/**
  Helper function to decode a URI-escaped string or HTTP parameter.

  If 'decode_plus' is 1, then we decode the string as an HTTP parameter
  value, and convert all plus ('+') characters to spaces.  If
  'decode_plus' is 0, we leave all plus characters unchanged.

  The returned string must be freed by the caller.

  @param uri a URI-encode encoded URI
  @param decode_plus determines whether we convert '+' to sapce.
  @param size_out if size_out is not NULL, *size_out is set to the size of the
     returned string
  @return a newly allocated unencoded URI or NULL on failure
 */
char *evhttp_uridecode(const char *uri, int decode_plus,
    size_t *size_out);

/**
   Helper function to parse out arguments in a query.

   Parsing a URI like

      http://foo.com/?q=test&s=some+thing

   will result in two entries in the key value queue.

   The first entry is: key="q", value="test"
   The second entry is: key="s", value="some thing"

   @deprecated This function is deprecated as of Libevent 2.0.9.  Use
     evhttp_uri_parse and evhttp_parse_query_str instead.

   @param uri the request URI
   @param headers the head of the evkeyval queue
   @return 0 on success, -1 on failure
 */
int evhttp_parse_query(const char *uri, struct evkeyvalq *headers);

/**
   Helper function to parse out arguments from the query portion of an
   HTTP URI.

   Parsing a query string like

     q=test&s=some+thing

   will result in two entries in the key value queue.

   The first entry is: key="q", value="test"
   The second entry is: key="s", value="some thing"

   @param query_parse the query portion of the URI
   @param headers the head of the evkeyval queue
   @return 0 on success, -1 on failure
 */
int evhttp_parse_query_str(const char *uri, struct evkeyvalq *headers);

/**
 * Escape HTML character entities in a string.
 *
 * Replaces <, >, ", ' and & with &lt;, &gt;, &quot;,
 * &#039; and &amp; correspondingly.
 *
 * The returned string needs to be freed by the caller.
 *
 * @param html an unescaped HTML string
 * @return an escaped HTML string or NULL on error
 */
char *evhttp_htmlescape(const char *html);

/**
 * Return a new empty evhttp_uri with no fields set.
 */
struct evhttp_uri *evhttp_uri_new(void);

/**
 * Changes the flags set on a given URI.  See EVHTTP_URI_* for
 * a list of flags.
 **/
void evhttp_uri_set_flags(struct evhttp_uri *uri, unsigned flags);

/** Return the scheme of an evhttp_uri, or NULL if there is no scheme has
 * been set and the evhttp_uri contains a Relative-Ref. */
const char *evhttp_uri_get_scheme(const struct evhttp_uri *uri);
/**
 * Return the userinfo part of an evhttp_uri, or NULL if it has no userinfo
 * set.
 */
const char *evhttp_uri_get_userinfo(const struct evhttp_uri *uri);
/**
 * Return the host part of an evhttp_uri, or NULL if it has no host set.
 * The host may either be a regular hostname (conforming to the RFC 3986
 * "regname" production), or an IPv4 address, or the empty string, or a
 * bracketed IPv6 address, or a bracketed 'IP-Future' address.
 *
 * Note that having a NULL host means that the URI has no authority
 * section, but having an empty-string host means that the URI has an
 * authority section with no host part.  For example,
 * "mailto:user@example.com" has a host of NULL, but "file:///etc/motd"
 * has a host of "".
 */
const char *evhttp_uri_get_host(const struct evhttp_uri *uri);
/** Return the port part of an evhttp_uri, or -1 if there is no port set. */
int evhttp_uri_get_port(const struct evhttp_uri *uri);
/** Return the path part of an evhttp_uri, or NULL if it has no path set */
const char *evhttp_uri_get_path(const struct evhttp_uri *uri);
/** Return the query part of an evhttp_uri (excluding the leading "?"), or
 * NULL if it has no query set */
const char *evhttp_uri_get_query(const struct evhttp_uri *uri);
/** Return the fragment part of an evhttp_uri (excluding the leading "#"),
 * or NULL if it has no fragment set */
const char *evhttp_uri_get_fragment(const struct evhttp_uri *uri);

/** Set the scheme of an evhttp_uri, or clear the scheme if scheme==NULL.
 * Returns 0 on success, -1 if scheme is not well-formed. */
int evhttp_uri_set_scheme(struct evhttp_uri *uri, const char *scheme);
/** Set the userinfo of an evhttp_uri, or clear the userinfo if userinfo==NULL.
 * Returns 0 on success, -1 if userinfo is not well-formed. */
int evhttp_uri_set_userinfo(struct evhttp_uri *uri, const char *userinfo);
/** Set the host of an evhttp_uri, or clear the host if host==NULL.
 * Returns 0 on success, -1 if host is not well-formed. */
int evhttp_uri_set_host(struct evhttp_uri *uri, const char *host);
/** Set the port of an evhttp_uri, or clear the port if port==-1.
 * Returns 0 on success, -1 if port is not well-formed. */
int evhttp_uri_set_port(struct evhttp_uri *uri, int port);
/** Set the path of an evhttp_uri, or clear the path if path==NULL.
 * Returns 0 on success, -1 if path is not well-formed. */
int evhttp_uri_set_path(struct evhttp_uri *uri, const char *path);
/** Set the query of an evhttp_uri, or clear the query if query==NULL.
 * The query should not include a leading "?".
 * Returns 0 on success, -1 if query is not well-formed. */
int evhttp_uri_set_query(struct evhttp_uri *uri, const char *query);
/** Set the fragment of an evhttp_uri, or clear the fragment if fragment==NULL.
 * The fragment should not include a leading "#".
 * Returns 0 on success, -1 if fragment is not well-formed. */
int evhttp_uri_set_fragment(struct evhttp_uri *uri, const char *fragment);

/**
 * Helper function to parse a URI-Reference as specified by RFC3986.
 *
 * This function matches the URI-Reference production from RFC3986,
 * which includes both URIs like
 *
 *    scheme://[[userinfo]@]foo.com[:port]]/[path][?query][#fragment]
 *
 *  and relative-refs like
 *
 *    [path][?query][#fragment]
 *
 * Any optional elements portions not present in the original URI are
 * left set to NULL in the resulting evhttp_uri.  If no port is
 * specified, the port is set to -1.
 *
 * Note that no decoding is performed on percent-escaped characters in
 * the string; if you want to parse them, use evhttp_uridecode or
 * evhttp_parse_query_str as appropriate.
 *
 * Note also that most URI schemes will have additional constraints that
 * this function does not know about, and cannot check.  For example,
 * mailto://www.example.com/cgi-bin/fortune.pl is not a reasonable
 * mailto url, http://www.example.com:99999/ is not a reasonable HTTP
 * URL, and ftp:username@example.com is not a reasonable FTP URL.
 * Nevertheless, all of these URLs conform to RFC3986, and this function
 * accepts all of them as valid.
 *
 * @param source_uri the request URI
 * @param flags Zero or more EVHTTP_URI_* flags to affect the behavior
 *              of the parser.
 * @return uri container to hold parsed data, or NULL if there is error
 * @see evhttp_uri_free()
 */
struct evhttp_uri *evhttp_uri_parse_with_flags(const char *source_uri,
    unsigned flags);

/** Tolerate URIs that do not conform to RFC3986.
 *
 * Unfortunately, some HTTP clients generate URIs that, according to RFC3986,
 * are not conformant URIs.  If you need to support these URIs, you can
 * do so by passing this flag to evhttp_uri_parse_with_flags.
 *
 * Currently, these changes are:
 * <ul>
 *   <li> Nonconformant URIs are allowed to contain otherwise unreasonable
 *        characters in their path, query, and fragment components.
 * </ul>
 */
#define EVHTTP_URI_NONCONFORMANT 0x01

/** Alias for evhttp_uri_parse_with_flags(source_uri, 0) */
struct evhttp_uri *evhttp_uri_parse(const char *source_uri);

/**
 * Free all memory allocated for a parsed uri.  Only use this for URIs
 * generated by evhttp_uri_parse.
 *
 * @param uri container with parsed data
 * @see evhttp_uri_parse()
 */
void evhttp_uri_free(struct evhttp_uri *uri);

/**
 * Join together the uri parts from parsed data to form a URI-Reference.
 *
 * Note that no escaping of reserved characters is done on the members
 * of the evhttp_uri, so the generated string might not be a valid URI
 * unless the members of evhttp_uri are themselves valid.
 *
 * @param uri container with parsed data
 * @param buf destination buffer
 * @param limit destination buffer size
 * @return an joined uri as string or NULL on error
 * @see evhttp_uri_parse()
 */
char *evhttp_uri_join(struct evhttp_uri *uri, char *buf, size_t limit);

#ifdef __cplusplus
}
#endif

#endif /* _EVENT2_HTTP_H_ */
