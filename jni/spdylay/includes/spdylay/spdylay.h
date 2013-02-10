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
#ifndef SPDYLAY_H
#define SPDYLAY_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>

#include <spdylay/spdylayver.h>

struct spdylay_session;
/**
 * @struct
 *
 * The primary structure to hold the resources needed for a SPDY
 * session. The details of this structure are intentionally hidden
 * from the public API.
 */
typedef struct spdylay_session spdylay_session;

/**
 * @enum
 *
 * The SPDY protocol version.
 */
typedef enum {
  /**
   * SPDY protocol version 2
   */
  SPDYLAY_PROTO_SPDY2 = 2,
  /**
   * SPDY protocol version 3
   */
  SPDYLAY_PROTO_SPDY3 = 3
} spdylay_proto_version;

/**
 * @enum
 *
 * Error codes used in the Spdylay library. The code range is [-999,
 * -500], inclusive. The following values are defined:
 */
typedef enum {
  /**
   * Invalid argument passed.
   */
  SPDYLAY_ERR_INVALID_ARGUMENT = -501,
  /**
   * Zlib error.
   */
  SPDYLAY_ERR_ZLIB = -502,
  /**
   * The specified protocol version is not supported.
   */
  SPDYLAY_ERR_UNSUPPORTED_VERSION = -503,
  /**
   * Used as a return value from :type:`spdylay_send_callback` and
   * :type:`spdylay_recv_callback` to indicate that the operation
   * would block.
   */
  SPDYLAY_ERR_WOULDBLOCK = -504,
  /**
   * General protocol error
   */
  SPDYLAY_ERR_PROTO = -505,
  /**
   * The frame is invalid.
   */
  SPDYLAY_ERR_INVALID_FRAME = -506,
  /**
   * The peer performed a shutdown on the connection.
   */
  SPDYLAY_ERR_EOF = -507,
  /**
   * Used as a return value from
   * :func:`spdylay_data_source_read_callback` to indicate that data
   * transfer is postponed. See
   * :func:`spdylay_data_source_read_callback` for details.
   */
  SPDYLAY_ERR_DEFERRED = -508,
  /**
   * Stream ID has reached the maximum value. Therefore no stream ID
   * is available.
   */
  SPDYLAY_ERR_STREAM_ID_NOT_AVAILABLE = -509,
  /**
   * The stream is already closed; or the stream ID is invalid.
   */
  SPDYLAY_ERR_STREAM_CLOSED = -510,
  /**
   * RST_STREAM has been added to the outbound queue. The stream is in
   * closing state.
   */
  SPDYLAY_ERR_STREAM_CLOSING = -511,
  /**
   * The transmission is not allowed for this stream (e.g., a frame
   * with FLAG_FIN flag set has already sent).
   */
  SPDYLAY_ERR_STREAM_SHUT_WR = -512,
  /**
   * The stream ID is invalid.
   */
  SPDYLAY_ERR_INVALID_STREAM_ID = -513,
  /**
   * The state of the stream is not valid (e.g., SYN_REPLY cannot be
   * sent to the stream if SYN_REPLY has already been sent).
   */
  SPDYLAY_ERR_INVALID_STREAM_STATE = -514,
  /**
   * Another DATA frame has already been deferred.
   */
  SPDYLAY_ERR_DEFERRED_DATA_EXIST = -515,
  /**
   * SYN_STREAM is not allowed. (e.g., GOAWAY has been sent and/or
   * received.
   */
  SPDYLAY_ERR_SYN_STREAM_NOT_ALLOWED = -516,
  /**
   * GOAWAY has already been sent.
   */
  SPDYLAY_ERR_GOAWAY_ALREADY_SENT = -517,
  /**
   * The received frame contains the invalid header block. (e.g.,
   * There are duplicate header names; or the header names are not
   * encoded in US-ASCII character set and not lower cased; or the
   * header name is zero-length string; or the header value contains
   * multiple in-sequence NUL bytes).
   */
  SPDYLAY_ERR_INVALID_HEADER_BLOCK = -518,
  /**
   * Indicates that the context is not suitable to perform the
   * requested operation.
   */
  SPDYLAY_ERR_INVALID_STATE = -519,
  /**
   * The gzip error.
   */
  SPDYLAY_ERR_GZIP = -520,
  /**
   * The user callback function failed due to the temporal error.
   */
  SPDYLAY_ERR_TEMPORAL_CALLBACK_FAILURE = -521,
  /**
   * The length of the frame is too large.
   */
  SPDYLAY_ERR_FRAME_TOO_LARGE = -522,
  /**
   * The errors < :enum:`SPDYLAY_ERR_FATAL` mean that the library is
   * under unexpected condition and cannot process any further data
   * reliably (e.g., out of memory).
   */
  SPDYLAY_ERR_FATAL = -900,
  /**
   * Out of memory. This is a fatal error.
   */
  SPDYLAY_ERR_NOMEM = -901,
  /**
   * The user callback function failed. This is a fatal error.
   */
  SPDYLAY_ERR_CALLBACK_FAILURE = -902
} spdylay_error;

typedef enum {
  SPDYLAY_MSG_MORE
} spdylay_io_flag;

/**
 * @enum
 * The control frame types in SPDY protocol.
 */
typedef enum {
  /**
   * The SYN_STREAM control frame.
   */
  SPDYLAY_SYN_STREAM = 1,
  /**
   * The SYN_REPLY control frame.
   */
  SPDYLAY_SYN_REPLY = 2,
  /**
   * The RST_STREAM control frame.
   */
  SPDYLAY_RST_STREAM = 3,
  /**
   * The SETTINGS control frame.
   */
  SPDYLAY_SETTINGS = 4,
  /**
   * The NOOP control frame. This was deprecated in SPDY/3.
   */
  SPDYLAY_NOOP = 5,
  /**
   * The PING control frame.
   */
  SPDYLAY_PING = 6,
  /**
   * The GOAWAY control frame.
   */
  SPDYLAY_GOAWAY = 7,
  /**
   * The HEADERS control frame.
   */
  SPDYLAY_HEADERS = 8,
  /**
   * The WINDOW_UPDATE control frame. This first appeared in SPDY/3.
   */
  SPDYLAY_WINDOW_UPDATE = 9,
  /**
   * The CREDENTIAL control frame. This first appeared in SPDY/3.
   */
  SPDYLAY_CREDENTIAL = 10
} spdylay_frame_type;

/**
 * @enum
 *
 * The flags for a control frame.
 */
typedef enum {
  /**
   * No flag set.
   */
  SPDYLAY_CTRL_FLAG_NONE = 0,
  /**
   * FLAG_FIN flag.
   */
  SPDYLAY_CTRL_FLAG_FIN = 0x1,
  /**
   * FLAG_UNIDIRECTIONAL flag.
   */
  SPDYLAY_CTRL_FLAG_UNIDIRECTIONAL = 0x2
} spdylay_ctrl_flag;

/**
 * @enum
 * The flags for a DATA frame.
 */
typedef enum {
  /**
   * No flag set.
   */
  SPDYLAY_DATA_FLAG_NONE = 0,
  /**
   * FLAG_FIN flag.
   */
  SPDYLAY_DATA_FLAG_FIN = 0x1
} spdylay_data_flag;

/**
 * @enum
 * The flags for the SETTINGS control frame.
 */
typedef enum {
  /**
   * No flag set.
   */
  SPDYLAY_FLAG_SETTINGS_NONE = 0,
  /**
   * SETTINGS_CLEAR_SETTINGS flag.
   */
  SPDYLAY_FLAG_SETTINGS_CLEAR_SETTINGS = 1
} spdylay_settings_flag;

/**
 * @enum
 * The flags for SETTINGS ID/value pair.
 */
typedef enum {
  /**
   * No flag set.
   */
  SPDYLAY_ID_FLAG_SETTINGS_NONE = 0,
  /**
   * FLAG_SETTINGS_PERSIST_VALUE flag.
   */
  SPDYLAY_ID_FLAG_SETTINGS_PERSIST_VALUE = 1,
  /**
   * FLAG_SETTINGS_PERSISTED flag.
   */
  SPDYLAY_ID_FLAG_SETTINGS_PERSISTED = 2
} spdylay_settings_id_flag;

/**
 * @enum
 * The SETTINGS ID.
 */
typedef enum {
  /**
   * SETTINGS_UPLOAD_BANDWIDTH
   */
  SPDYLAY_SETTINGS_UPLOAD_BANDWIDTH = 1,
  /**
   * SETTINGS_DOWNLOAD_BANDWIDTH
   */
  SPDYLAY_SETTINGS_DOWNLOAD_BANDWIDTH = 2,
  /**
   * SETTINGS_ROUND_TRIP_TIME
   */
  SPDYLAY_SETTINGS_ROUND_TRIP_TIME = 3,
  /**
   * SETTINGS_MAX_CONCURRENT_STREAMS
   */
  SPDYLAY_SETTINGS_MAX_CONCURRENT_STREAMS = 4,
  /**
   * SETTINGS_CURRENT_CWND
   */
  SPDYLAY_SETTINGS_CURRENT_CWND = 5,
  /**
   * SETTINGS_DOWNLOAD_RETRANS_RATE
   */
  SPDYLAY_SETTINGS_DOWNLOAD_RETRANS_RATE = 6,
  /**
   * SETTINGS_INITIAL_WINDOW_SIZE
   */
  SPDYLAY_SETTINGS_INITIAL_WINDOW_SIZE = 7,
  /**
   * SETTINGS_CLIENT_CERTIFICATE_VECTOR_SIZE. This first appeared in
   * SPDY/3.
   */
  SPDYLAY_SETTINGS_CLIENT_CERTIFICATE_VECTOR_SIZE = 8,
  /**
   * Maximum ID of :type:`spdylay_settings_id`.
   */
  SPDYLAY_SETTINGS_MAX = 8
} spdylay_settings_id;

/**
 * @macro
 * Default maximum concurrent streams.
 */
#define SPDYLAY_INITIAL_MAX_CONCURRENT_STREAMS 100

/**
 * @enum
 * The status codes for the RST_STREAM control frame.
 */
typedef enum {
  /**
   * SPDYLAY_OK is not valid status code for RST_STREAM. It is defined
   * just for spdylay library use.
   */
  SPDYLAY_OK = 0,
  /**
   * PROTOCOL_ERROR
   */
  SPDYLAY_PROTOCOL_ERROR = 1,
  /**
   * INVALID_STREAM
   */
  SPDYLAY_INVALID_STREAM = 2,
  /**
   * REFUSED_STREAM
   */
  SPDYLAY_REFUSED_STREAM = 3,
  /**
   * UNSUPPORTED_VERSION
   */
  SPDYLAY_UNSUPPORTED_VERSION = 4,
  /**
   * CANCEL
   */
  SPDYLAY_CANCEL = 5,
  /**
   * INTERNAL_ERROR
   */
  SPDYLAY_INTERNAL_ERROR = 6,
  /**
   * FLOW_CONTROL_ERROR
   */
  SPDYLAY_FLOW_CONTROL_ERROR = 7,
  /* Following status codes were introduced in SPDY/3 */
  /**
   * STREAM_IN_USE
   */
  SPDYLAY_STREAM_IN_USE = 8,
  /**
   * STREAM_ALREADY_CLOSED
   */
  SPDYLAY_STREAM_ALREADY_CLOSED = 9,
  /**
   * INVALID_CREDENTIALS
   */
  SPDYLAY_INVALID_CREDENTIALS = 10,
  /**
   * FRAME_TOO_LARGE
   */
  SPDYLAY_FRAME_TOO_LARGE = 11
} spdylay_status_code;

/**
 * @enum
 * The status codes for GOAWAY, introduced in SPDY/3.
 */
typedef enum {
  /**
   * OK. This indicates a normal session teardown.
   */
  SPDYLAY_GOAWAY_OK = 0,
  /**
   * PROTOCOL_ERROR
   */
  SPDYLAY_GOAWAY_PROTOCOL_ERROR = 1,
  /**
   * INTERNAL_ERROR
   */
  SPDYLAY_GOAWAY_INTERNAL_ERROR = 2
} spdylay_goaway_status_code;

/**
 * @struct
 * The control frame header.
 */
typedef struct {
  /**
   * SPDY protocol version.
   */
  uint16_t version;
  /**
   * The type of this control frame.
   */
  uint16_t type;
  /**
   * The control frame flags.
   */
  uint8_t flags;
  /**
   * The length field of this control frame.
   */
  int32_t length;
} spdylay_ctrl_hd;

/**
 * @struct
 * The SYN_STREAM control frame. It has the following members:
 */
typedef struct {
  /**
   * The control frame header.
   */
  spdylay_ctrl_hd hd;
  /**
   * The stream ID.
   */
  int32_t stream_id;
  /**
   * The associated-to-stream ID. 0 if this frame has no
   * associated-to-stream.
   */
  int32_t assoc_stream_id;
  /**
   * The priority of this frame. 0 is the highest priority value. Use
   * `spdylay_session_get_pri_lowest()` to know the lowest priority
   * value.
   */
  uint8_t pri;
  /**
   * The index in server's CREDENTIAL vector of the client certificate.
   * This was introduced in SPDY/3.
   */
  uint8_t slot;
  /**
   * The name/value pairs. For i >= 0, ``nv[2*i]`` contains a pointer
   * to the name string and ``nv[2*i+1]`` contains a pointer to the
   * value string. The one beyond last value must be ``NULL``. That
   * is, if the |nv| contains N name/value pairs, ``nv[2*N]`` must be
   * ``NULL``. This member may be ``NULL``.
   */
  char **nv;
} spdylay_syn_stream;

/**
 * @struct
 * The SYN_REPLY control frame. It has the following members:
 */
typedef struct {
  /**
   * The control frame header.
   */
  spdylay_ctrl_hd hd;
  /**
   * The stream ID.
   */
  int32_t stream_id;
  /**
   * The name/value pairs. For i >= 0, ``nv[2*i]`` contains a pointer
   * to the name string and ``nv[2*i+1]`` contains a pointer to the
   * value string. The one beyond last value must be ``NULL``. That
   * is, if the |nv| contains N name/value pairs, ``nv[2*N]`` must be
   * ``NULL``. This member may be ``NULL``.
   */
  char **nv;
} spdylay_syn_reply;

/**
 * @struct
 * The HEADERS control frame. It has the following members:
 */
typedef struct {
  /**
   * The control frame header.
   */
  spdylay_ctrl_hd hd;
  /**
   * The stream ID.
   */
  int32_t stream_id;
  /**
   * The name/value pairs. For i >= 0, ``nv[2*i]`` contains a pointer
   * to the name string and ``nv[2*i+1]`` contains a pointer to the
   * value string. The one beyond last value must be ``NULL``. That
   * is, if the |nv| contains N name/value pairs, ``nv[2*N]`` must be
   * ``NULL``. This member may be ``NULL``.
   */
  char **nv;
} spdylay_headers;

/**
 * @struct
 * The RST_STREAM control frame. It has the following members:
 */
typedef struct {
  /**
   * The control frame header.
   */
  spdylay_ctrl_hd hd;
  /**
   * The stream ID.
   */
  int32_t stream_id;
  /**
   * The status code. See :type:`spdylay_status_code`.
   */
  uint32_t status_code;
} spdylay_rst_stream;

/**
 * @struct
 * The SETTINGS ID/Value pair. It has the following members:
 */
typedef struct {
  /**
   * The SETTINGS ID. See :type:`spdylay_settings_id`.
   */
  int32_t settings_id;
  /**
   * The flags. See :type:`spdylay_settings_id_flag`.
   */
  uint8_t flags;
  /**
   * The value of this entry.
   */
  uint32_t value;
} spdylay_settings_entry;

/**
 * @struct
 * The SETTINGS control frame. It has the following members:
 */
typedef struct {
  /**
   * The control frame header.
   */
  spdylay_ctrl_hd hd;
  /**
   * The number of SETTINGS ID/Value pairs in |iv|.
   */
  size_t niv;
  /**
   * The pointer to the array of SETTINGS ID/Value pair.
   */
  spdylay_settings_entry *iv;
} spdylay_settings;

/**
 * @struct
 * The PING control frame. It has the following members:
 */
typedef struct {
  /**
   * The control frame header.
   */
  spdylay_ctrl_hd hd;
  /**
   * The unique ID.
   */
  uint32_t unique_id;
} spdylay_ping;

/**
 * @struct
 * The GOAWAY control frame. It has the following members:
 */
typedef struct {
  /**
   * The control frame header.
   */
  spdylay_ctrl_hd hd;
  /**
   * The last-good-stream ID.
   */
  int32_t last_good_stream_id;
  /**
   * The status code. This first appeared in SPDY/3. See
   * :type:`spdylay_goaway_status_code`.
   */
  uint32_t status_code;
} spdylay_goaway;

/**
 * @struct
 *
 * The WINDOW_UPDATE control frame. This first appeared in SPDY/3.  It
 * has the following members:
 */
typedef struct {
  /**
   * The control frame header.
   */
  spdylay_ctrl_hd hd;
  /**
   * The stream ID.
   */
  int32_t stream_id;
  /**
   * The delta-window-size.
   */
  int32_t delta_window_size;
} spdylay_window_update;

/**
 * @struct
 *
 * The structure to hold chunk of memory.
 */
typedef struct {
  /**
   * The pointer to the data.
   */
  uint8_t *data;
  /**
   * The length of the data.
   */
  size_t length;
} spdylay_mem_chunk;

/**
 * @struct
 *
 * The CREDENTIAL control frame. This first appeared in SPDY/3. It has
 * the following members:
 */
typedef struct {
  /**
   * The control frame header.
   */
  spdylay_ctrl_hd hd;
  /**
   * The index in the client certificate vector.
   */
  uint16_t slot;
  /**
   * Cryptographic proof that the client has possession of the private
   * key associated with the certificate.
   */
  spdylay_mem_chunk proof;
  /**
   * The certificate chain. The certs[0] is the leaf certificate.
   */
  spdylay_mem_chunk *certs;
  /**
   * The number of certificates in |certs|.
   */
  size_t ncerts;
} spdylay_credential;

/**
 * @struct
 *
 * Convenient structure to inspect control frame header.  It is useful
 * to get the frame type.
 */
typedef struct {
  /**
   * The control frame header.
   */
  spdylay_ctrl_hd hd;
} spdylay_ctrl_frame;

/**
 * @union
 *
 * This union represents the some kind of data source passed to
 * :type:`spdylay_data_source_read_callback`.
 */
typedef union {
  /**
   * The integer field, suitable for a file descriptor.
   */
  int fd;
  /**
   * The pointer to an arbitrary object.
   */
  void *ptr;
} spdylay_data_source;

/**
 * @functypedef
 *
 * Callback function invoked when the library wants to read data from
 * the |source|. The read data is sent in the stream |stream_id|. The
 * implementation of this function must read at most |length| bytes of
 * data from |source| (or possibly other places) and store them in
 * |buf| and return number of data stored in |buf|. If EOF is reached,
 * set |*eof| to 1.  If the application wants to postpone DATA frames,
 * (e.g., asynchronous I/O, or reading data blocks for long time), it
 * is achieved by returning :enum:`SPDYLAY_ERR_DEFERRED` without
 * reading any data in this invocation.  The library removes DATA
 * frame from the outgoing queue temporarily.  To move back deferred
 * DATA frame to outgoing queue, call `spdylay_session_resume_data()`.
 * In case of error, there are 2 choices. Returning
 * :enum:`SPDYLAY_ERR_TEMPORAL_CALLBACK_FAILURE` will close the stream
 * by issuing RST_STREAM with :enum:`SPDYLAY_INTERNAL_ERROR`.
 * Returning :enum:`SPDYLAY_ERR_CALLBACK_FAILURE` will signal the
 * entire session failure.
 */
typedef ssize_t (*spdylay_data_source_read_callback)
(spdylay_session *session, int32_t stream_id,
 uint8_t *buf, size_t length, int *eof,
 spdylay_data_source *source, void *user_data);

/**
 * @struct
 *
 * This struct represents the data source and the way to read a chunk
 * of data from it.
 */
typedef struct {
  /**
   * The data source.
   */
  spdylay_data_source source;
  /**
   * The callback function to read a chunk of data from the |source|.
   */
  spdylay_data_source_read_callback read_callback;
} spdylay_data_provider;

/**
 * @union
 *
 * This union includes all control frames to pass them
 * to various function calls as spdylay_frame type.
 */
typedef union {
  /**
   * Convenient structure to inspect control frame header.
   */
  spdylay_ctrl_frame ctrl;
  /**
   * The SYN_STREAM control frame.
   */
  spdylay_syn_stream syn_stream;
  /**
   * The SYN_REPLY control frame.
   */
  spdylay_syn_reply syn_reply;
  /**
   * The RST_STREAM control frame.
   */
  spdylay_rst_stream rst_stream;
  /**
   * The SETTINGS control frame.
   */
  spdylay_settings settings;
  /**
   * The PING control frame.
   */
  spdylay_ping ping;
  /**
   * The GOAWAY control frame.
   */
  spdylay_goaway goaway;
  /**
   * The HEADERS control frame.
   */
  spdylay_headers headers;
  /**
   * The WINDOW_UPDATE control frame.
   */
  spdylay_window_update window_update;
  /**
   * The CREDENTIAL control frame.
   */
  spdylay_credential credential;
} spdylay_frame;

/**
 * @functypedef
 *
 * Callback function invoked when |session| wants to send data to the
 * remote peer. The implementation of this function must send at most
 * |length| bytes of data stored in |data|. It must return the number
 * of bytes sent if it succeeds.  If it cannot send any single byte
 * without blocking, it must return
 * :enum:`SPDYLAY_ERR_WOULDBLOCK`. For other errors, it must return
 * :enum:`SPDYLAY_ERR_CALLBACK_FAILURE`.
 */
typedef ssize_t (*spdylay_send_callback)
(spdylay_session *session,
 const uint8_t *data, size_t length, int flags, void *user_data);

/**
 * @functypedef
 *
 * Callback function invoked when |session| wants to receive data from
 * the remote peer. The implementation of this function must read at
 * most |length| bytes of data and store it in |buf|. It must return
 * the number of bytes written in |buf| if it succeeds. If it cannot
 * read any single byte without blocking, it must return
 * :enum:`SPDYLAY_ERR_WOULDBLOCK`. If it gets EOF before it reads any
 * single byte, it must return :enum:`SPDYLAY_ERR_EOF`. For other
 * errors, it must return :enum:`SPDYLAY_ERR_CALLBACK_FAILURE`.
 */
typedef ssize_t (*spdylay_recv_callback)
(spdylay_session *session,
 uint8_t *buf, size_t length, int flags, void *user_data);

/**
 * @functypedef
 *
 * Callback function invoked by `spdylay_session_recv()` when a
 * control frame is received.
 */
typedef void (*spdylay_on_ctrl_recv_callback)
(spdylay_session *session, spdylay_frame_type type, spdylay_frame *frame,
 void *user_data);

/**
 * @functypedef
 *
 * Callback function invoked by `spdylay_session_recv()` when an
 * invalid control frame is received. The |status_code| is one of the
 * :enum:`spdylay_status_code` and indicates the error. When this
 * callback function is invoked, the library automatically submits
 * either RST_STREAM or GOAWAY frame.
 */
typedef void (*spdylay_on_invalid_ctrl_recv_callback)
(spdylay_session *session, spdylay_frame_type type, spdylay_frame *frame,
 uint32_t status_code, void *user_data);

/**
 * @functypedef
 *
 * Callback function invoked when a chunk of data in DATA frame is
 * received. The |stream_id| is the stream ID this DATA frame belongs
 * to. The |flags| is the flags of DATA frame which this data chunk is
 * contained. ``(flags & SPDYLAY_DATA_FLAG_FIN) != 0`` does not
 * necessarily mean this chunk of data is the last one in the
 * stream. You should use :type:`spdylay_on_data_recv_callback` to
 * know all data frames are received.
 */
typedef void (*spdylay_on_data_chunk_recv_callback)
(spdylay_session *session, uint8_t flags, int32_t stream_id,
 const uint8_t *data, size_t len, void *user_data);

/**
 * @functypedef
 *
 * Callback function invoked when DATA frame is received. The actual
 * data it contains are received by
 * :type:`spdylay_on_data_chunk_recv_callback`.
 */
typedef void (*spdylay_on_data_recv_callback)
(spdylay_session *session, uint8_t flags, int32_t stream_id, int32_t length,
 void *user_data);

/**
 * @functypedef
 *
 * Callback function invoked before the control frame |frame| of type
 * |type| is sent. This may be useful, for example, to know the stream
 * ID of SYN_STREAM frame (see also
 * `spdylay_session_get_stream_user_data()`), which is not assigned
 * when it was queued.
 */
typedef void (*spdylay_before_ctrl_send_callback)
(spdylay_session *session, spdylay_frame_type type, spdylay_frame *frame,
 void *user_data);

/**
 * @functypedef
 *
 * Callback function invoked after the control frame |frame| of type
 * |type| is sent.
 */
typedef void (*spdylay_on_ctrl_send_callback)
(spdylay_session *session, spdylay_frame_type type, spdylay_frame *frame,
 void *user_data);

/**
 * @functypedef
 *
 * Callback function invoked after the control frame |frame| of type
 * |type| is not sent because of the error. The error is indicated by
 * the |error_code|, which is one of the values defined in
 * :type:`spdylay_error`.
 */
typedef void (*spdylay_on_ctrl_not_send_callback)
(spdylay_session *session, spdylay_frame_type type, spdylay_frame *frame,
 int error_code, void *user_data);

/**
 * @functypedef
 *
 * Callback function invoked after DATA frame is sent.
 */
typedef void (*spdylay_on_data_send_callback)
(spdylay_session *session, uint8_t flags, int32_t stream_id, int32_t length,
 void *user_data);

/**
 * @functypedef
 *
 * Callback function invoked when the stream |stream_id| is
 * closed. The reason of closure is indicated by the
 * |status_code|. The stream_user_data, which was specified in
 * `spdylay_submit_request()` or `spdylay_submit_syn_stream()`, is
 * still available in this function.
 */
typedef void (*spdylay_on_stream_close_callback)
(spdylay_session *session, int32_t stream_id, spdylay_status_code status_code,
 void *user_data);

/**
 * @functypedef
 *
 * Callback function invoked when the request from the remote peer is
 * received.  In other words, the frame with FIN flag set is received.
 * In HTTP, this means HTTP request, including request body, is fully
 * received.
 */
typedef void (*spdylay_on_request_recv_callback)
(spdylay_session *session, int32_t stream_id, void *user_data);

/**
 * @functypedef
 *
 * Callback function invoked when the received control frame octets
 * could not be parsed correctly. The |type| indicates the type of
 * received control frame. The |head| is the pointer to the header of
 * the received frame. The |headlen| is the length of the
 * |head|. According to the SPDY spec, the |headlen| is always 8. In
 * other words, the |head| is the first 8 bytes of the received frame.
 * The |payload| is the pointer to the data portion of the received
 * frame.  The |payloadlen| is the length of the |payload|. This is
 * the data after the length field. The |error_code| is one of the
 * error code defined in :enum:`spdylay_error` and indicates the
 * error.
 */
typedef void (*spdylay_on_ctrl_recv_parse_error_callback)
(spdylay_session *session, spdylay_frame_type type,
 const uint8_t *head, size_t headlen,
 const uint8_t *payload, size_t payloadlen,
 int error_code, void *user_data);

/**
 * @functypedef
 *
 * Callback function invoked when the received control frame type is
 * unknown. The |head| is the pointer to the header of the received
 * frame. The |headlen| is the length of the |head|. According to the
 * SPDY spec, the |headlen| is always 8. In other words, the |head| is
 * the first 8 bytes of the received frame.  The |payload| is the
 * pointer to the data portion of the received frame.  The
 * |payloadlen| is the length of the |payload|. This is the data after
 * the length field.
 */
typedef void (*spdylay_on_unknown_ctrl_recv_callback)
(spdylay_session *session,
 const uint8_t *head, size_t headlen,
 const uint8_t *payload, size_t payloadlen,
 void *user_data);

#define SPDYLAY_MAX_SCHEME 255
#define SPDYLAY_MAX_HOSTNAME 255

struct spdylay_origin;

/**
 * @struct
 *
 * The Web origin structure. The origin is the tuple (scheme, host,
 * port). The details of this structure is intentionally hidden. To
 * access these members, use accessor functions below.
 */
typedef struct spdylay_origin spdylay_origin;

/**
 * @function
 *
 * Returns the scheme member of the |origin|.
 */
const char* spdylay_origin_get_scheme(const spdylay_origin *origin);

/**
 * @function
 *
 * Returns the host member of the |origin|.
 */
const char* spdylay_origin_get_host(const spdylay_origin *origin);

/**
 * @function
 *
 * Returns the port member of the |origin|.
 */
uint16_t spdylay_origin_get_port(const spdylay_origin *origin);

/**
 * @functypedef
 *
 * Callback function invoked when the library needs the cryptographic
 * proof that the client has possession of the private key associated
 * with the certificate for the given |origin|.  If called with
 * |prooflen| == 0, the implementation of this function must return
 * the length of the proof in bytes. If called with |prooflen| > 0,
 * write proof into |proof| exactly |prooflen| bytes and return 0.
 *
 * Because the client certificate vector has limited number of slots,
 * the application code may be required to pass the same proof more
 * than once.
 */
typedef ssize_t (*spdylay_get_credential_proof)
(spdylay_session *session, const spdylay_origin *origin,
 uint8_t *proof, size_t prooflen, void *user_data);

/**
 * @functypedef
 *
 * Callback function invoked when the library needs the length of the
 * client certificate chain for the given |origin|.  The
 * implementation of this function must return the length of the
 * client certificate chain.  If no client certificate is required for
 * the given |origin|, return 0.  If positive integer is returned,
 * :type:`spdylay_get_credential_proof` and
 * :type:`spdylay_get_credential_cert` callback functions will be used
 * to get the cryptographic proof and certificate respectively.
 */
typedef ssize_t (*spdylay_get_credential_ncerts)
(spdylay_session *session, const spdylay_origin *origin, void *user_data);

/**
 * @functypedef
 *
 * Callback function invoked when the library needs the client
 * certificate for the given |origin|. The |idx| is the index of the
 * certificate chain and 0 means the leaf certificate of the chain.
 * If called with |certlen| == 0, the implementation of this function
 * must return the length of the certificate in bytes. If called with
 * |certlen| > 0, write certificate into |cert| exactly |certlen|
 * bytes and return 0.
 */
typedef ssize_t (*spdylay_get_credential_cert)
(spdylay_session *session, const spdylay_origin *origin, size_t idx,
 uint8_t *cert, size_t certlen, void *user_data);

/**
 * @struct
 *
 * Callback functions.
 */
typedef struct {
  /**
   * Callback function invoked when the |session| wants to send data
   * to the remote peer.
   */
  spdylay_send_callback send_callback;
  /**
   * Callback function invoked when the |session| wants to receive
   * data from the remote peer.
   */
  spdylay_recv_callback recv_callback;
  /**
   * Callback function invoked by `spdylay_session_recv()` when a
   * control frame is received.
   */
  spdylay_on_ctrl_recv_callback on_ctrl_recv_callback;
  /**
   * Callback function invoked by `spdylay_session_recv()` when an
   * invalid control frame is received.
   */
  spdylay_on_invalid_ctrl_recv_callback on_invalid_ctrl_recv_callback;
  /**
   * Callback function invoked when a chunk of data in DATA frame is
   * received.
   */
  spdylay_on_data_chunk_recv_callback on_data_chunk_recv_callback;
  /**
   * Callback function invoked when DATA frame is received.
   */
  spdylay_on_data_recv_callback on_data_recv_callback;
  /**
   * Callback function invoked before the control frame is sent.
   */
  spdylay_before_ctrl_send_callback before_ctrl_send_callback;
  /**
   * Callback function invoked after the control frame is sent.
   */
  spdylay_on_ctrl_send_callback on_ctrl_send_callback;
  /**
   * The callback function invoked when a control frame is not sent
   * because of an error.
   */
  spdylay_on_ctrl_not_send_callback on_ctrl_not_send_callback;
  /**
   * Callback function invoked after DATA frame is sent.
   */
  spdylay_on_data_send_callback on_data_send_callback;
  /**
   * Callback function invoked when the stream is closed.
   */
  spdylay_on_stream_close_callback on_stream_close_callback;
  /**
   * Callback function invoked when request from the remote peer is
   * received.
   */
  spdylay_on_request_recv_callback on_request_recv_callback;
  /**
   * Callback function invoked when the library needs the
   * cryptographic proof that the client has possession of the private
   * key associated with the certificate.
   */
  spdylay_get_credential_proof get_credential_proof;
  /**
   * Callback function invoked when the library needs the length of the
   * client certificate chain.
   */
  spdylay_get_credential_ncerts get_credential_ncerts;
  /**
   * Callback function invoked when the library needs the client
   * certificate.
   */
  spdylay_get_credential_cert get_credential_cert;
  /**
   * Callback function invoked when the received control frame octets
   * could not be parsed correctly.
   */
  spdylay_on_ctrl_recv_parse_error_callback on_ctrl_recv_parse_error_callback;
  /**
   * Callback function invoked when the received control frame type is
   * unknown.
   */
  spdylay_on_unknown_ctrl_recv_callback on_unknown_ctrl_recv_callback;
} spdylay_session_callbacks;

/**
 * @function
 *
 * Initializes |*session_ptr| for client use, using the protocol
 * version |version|. The all members of |callbacks| are copied to
 * |*session_ptr|. Therefore |*session_ptr| does not store
 * |callbacks|. |user_data| is an arbitrary user supplied data, which
 * will be passed to the callback functions.
 *
 * The :member:`spdylay_session_callbacks.send_callback` must be
 * specified.  If the application code uses `spdylay_session_recv()`,
 * the :member:`spdylay_session_callbacks.recv_callback` must be
 * specified. The other members of |callbacks| can be ``NULL``.  To
 * use CREDENTIAL frame, specify :macro:`SPDYLAY_PROTO_SPDY3` in
 * |version| and specify
 * :member:`spdylay_session_callbacks.get_credential_ncerts`,
 * :member:`spdylay_session_callbacks.get_credential_cert` and
 * :member:`spdylay_session_callbacks.get_credential_proof`.  See also
 * `spdylay_session_set_initial_client_cert_origin()`.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`SPDYLAY_ERR_NOMEM`
 *     Out of memory.
 * :enum:`SPDYLAY_ERR_ZLIB`
 *     The z_stream initialization failed.
 * :enum:`SPDYLAY_ERR_UNSUPPORTED_VERSION`
 *     The version is not supported.
 */
int spdylay_session_client_new(spdylay_session **session_ptr,
                               uint16_t version,
                               const spdylay_session_callbacks *callbacks,
                               void *user_data);

/**
 * @function
 *
 * Initializes |*session_ptr| for server use, using the protocol
 * version |version|. The all members of |callbacks| are copied to
 * |*session_ptr|. Therefore |*session_ptr| does not store
 * |callbacks|. |user_data| is an arbitrary user supplied data, which
 * will be passed to the callback functions.
 *
 * The :member:`spdylay_session_callbacks.send_callback` must be
 * specified.  If the application code uses `spdylay_session_recv()`,
 * the :member:`spdylay_session_callbacks.recv_callback` must be
 * specified. The other members of |callbacks| can be ``NULL``.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`SPDYLAY_ERR_NOMEM`
 *     Out of memory.
 * :enum:`SPDYLAY_ERR_ZLIB`
 *     The z_stream initialization failed.
 * :enum:`SPDYLAY_ERR_UNSUPPORTED_VERSION`
 *     The version is not supported.
 */
int spdylay_session_server_new(spdylay_session **session_ptr,
                               uint16_t version,
                               const spdylay_session_callbacks *callbacks,
                               void *user_data);

/**
 * @function
 *
 * Frees any resources allocated for |session|. If |session| is
 * ``NULL``, this function does nothing.
 */
void spdylay_session_del(spdylay_session *session);

/**
 * @enum
 *
 * Configuration options for :type:`spdylay_session`.
 */
typedef enum {
  /**
   * This option prevents the library from sending WINDOW_UPDATE
   * automatically. If this option is set, the application is
   * responsible for sending WINDOW_UPDATE using
   * `spdylay_submit_window_update`.
   */
  SPDYLAY_OPT_NO_AUTO_WINDOW_UPDATE = 1,
  /**
   * This option sets maximum receive buffer size for incoming control
   * frame.
   */
  SPDYLAY_OPT_MAX_RECV_CTRL_FRAME_BUFFER = 2
} spdylay_opt;

/**
 * @function
 *
 * Sets the configuration option for the |session|.  The |optname| is
 * one of :type:`spdylay_opt`. The |optval| is the pointer to the
 * option value and the |optlen| is the size of |*optval|. The
 * required type of |optval| varies depending on the |optname|. See
 * below.
 *
 * The following |optname| are supported:
 *
 * :enum:`SPDYLAY_OPT_NO_AUTO_WINDOW_UPDATE`
 *     The |optval| must be a pointer to ``int``. If the |*optval| is
 *     nonzero, the library will not send WINDOW_UPDATE automatically.
 *     Therefore, the application is responsible for sending
 *     WINDOW_UPDATE using `spdylay_submit_window_update`. This option
 *     defaults to 0.
 *
 * :enum:`SPDYLAY_OPT_MAX_RECV_CTRL_FRAME_BUFFER`
 *     The |optval| must be a pointer to ``uint32_t``. The |*optval|
 *     must be in the range [(1 << 13), (1 << 24)-1], inclusive. This
 *     option defaults to (1 << 24)-1.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`SPDYLAY_ERR_INVALID_ARGUMENT`
 *     The |optname| is not supported; or the |optval| and/or the
 *     |optlen| are invalid.
 */
int spdylay_session_set_option(spdylay_session *session,
                               int optname, void *optval, size_t optlen);

/**
 * @function
 *
 * Sets the origin tuple (|scheme|, |host| and |port|) that the
 * connection is made to and the client certificate is sent in the
 * first TLS handshake. This function must be called before any call
 * of `spdylay_session_send()` and `spdylay_session_recv()` and be
 * called only once per session. This function must not be called if
 * the |session| is initialized for server use. If the client did not
 * provide the client certificate in the first TLS handshake, this
 * function must not be called.
 *
 * This function stores the given origin at the slot 1 in the client
 * certificate vector.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`SPDYLAY_ERR_NOMEM`
 *     Out of memory
 * :enum:`SPDYLAY_ERR_INVALID_STATE`
 *     The |session| is initialized for server use; or the client
 *     certificate vector size is 0.
 */
int spdylay_session_set_initial_client_cert_origin(spdylay_session *session,
                                                   const char *scheme,
                                                   const char *host,
                                                   uint16_t port);

/**
 * @function
 *
 * Returns the origin at the index |slot| in the client certificate
 * vector. If there is no origin at the given |slot|, this function
 * returns ``NULL``.
 *
 * This function must not be called if the |session| is initialized
 * for server use.
 */
const spdylay_origin* spdylay_session_get_client_cert_origin
(spdylay_session *session,
 size_t slot);

/**
 * @function
 *
 * Sends pending frames to the remote peer.
 *
 * This function retrieves the highest prioritized frame from the
 * outbound queue and sends it to the remote peer. It does this as
 * many as possible until the user callback
 * :member:`spdylay_session_callbacks.send_callback` returns
 * :enum:`SPDYLAY_ERR_WOULDBLOCK` or the outbound queue becomes empty.
 * This function calls several callback functions which are passed
 * when initializing the |session|. Here is the simple time chart
 * which tells when each callback is invoked:
 *
 * 1. Get the next frame to send from outbound queue.
 * 2. Prepare transmission of the frame.
 * 3. If the control frame cannot be sent because some preconditions
 *    are not met (e.g., SYN_STREAM cannot be sent after GOAWAY),
 *    :member:`spdylay_session_callbacks.on_ctrl_not_send_callback` is
 *    invoked. Abort the following steps.
 * 4. If the frame is SYN_STREAM, the stream is opened here.
 *    If the |session| is initialized for client use and the protocol
 *    version is :macro:`SPDYLAY_PROTO_SPDY3` and the library needs
 *    the client certificate for the origin,
 *    :member:`spdylay_session_callbacks.get_credential_ncerts` is
 *    invoked. If the result is more than zero,
 *    :member:`spdylay_session_callbacks.get_credential_proof` and
 *    :member:`spdylay_session_callbacks.get_credential_cert` are also
 *    invoked.
 * 5. :member:`spdylay_session_callbacks.before_ctrl_send_callback` is
 *    invoked.
 * 6. :member:`spdylay_session_callbacks.send_callback` is invoked one
 *    or more times to send the frame.
 * 7. If the frame is a control frame,
 *    :member:`spdylay_session_callbacks.on_ctrl_send_callback` is
 *    invoked.
 * 8. If the frame is a DATA frame,
 *    :member:`spdylay_session_callbacks.on_data_send_callback` is
 *    invoked.
 * 9. If the transmission of the frame triggers closure of the stream,
 *    the stream is closed and
 *    :member:`spdylay_session_callbacks.on_stream_close_callback` is
 *    invoked.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`SPDYLAY_ERR_NOMEM`
 *     Out of memory.
 * :enum:`SPDYLAY_ERR_CALLBACK_FAILURE`
 *     The callback function failed.
 */
int spdylay_session_send(spdylay_session *session);

/**
 * @function
 *
 * Receives frames from the remote peer.
 *
 * This function receives as many frames as possible until the user
 * callback :member:`spdylay_session_callbacks.recv_callback` returns
 * :enum:`SPDYLAY_ERR_WOULDBLOCK`. This function calls several
 * callback functions which are passed when initializing the
 * |session|. Here is the simple time chart which tells when each
 * callback is invoked:
 *
 * 1. :member:`spdylay_session_callbacks.recv_callback` is invoked one
 *    or more times to receive frame header.
 * 2. If the frame is DATA frame:
 *
 *   2.1. :member:`spdylay_session_callbacks.recv_callback` is invoked
 *        to receive DATA payload. For each chunk of data,
 *        :member:`spdylay_session_callbacks.on_data_chunk_recv_callback`
 *        is invoked.
 *   2.2. If one DATA frame is completely received,
 *        :member:`spdylay_session_callbacks.on_data_recv_callback` is
 *        invoked.  If the frame is the final frame of the request,
 *        :member:`spdylay_session_callbacks.on_request_recv_callback`
 *        is invoked.  If the reception of the frame triggers the
 *        closure of the stream,
 *        :member:`spdylay_session_callbacks.on_stream_close_callback`
 *        is invoked.
 *
 * 3. If the frame is the control frame:
 *
 *   3.1. :member:`spdylay_session_callbacks.recv_callback` is invoked
 *        one or more times to receive whole frame.
 *   3.2. If the received frame is valid,
 *        :member:`spdylay_session_callbacks.on_ctrl_recv_callback` is
 *        invoked.  If the frame is the final frame of the request,
 *        :member:`spdylay_session_callbacks.on_request_recv_callback`
 *        is invoked.  If the reception of the frame triggers the
 *        closure of the stream,
 *        :member:`spdylay_session_callbacks.on_stream_close_callback`
 *        is invoked.
 *   3.3. If the received frame is unpacked but is interpreted as
 *        invalid,
 *        :member:`spdylay_session_callbacks.on_invalid_ctrl_recv_callback`
 *        is invoked.
 *   3.4. If the received frame could not be unpacked correctly,
 *        :member:`spdylay_session_callbacks.on_ctrl_recv_parse_error_callback`
 *        is invoked.
 *   3.5. If the received frame type is unknown,
 *        :member:`spdylay_session_callbacks.on_unknown_ctrl_recv_callback`
 *        is invoked.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`SPDYLAY_ERR_EOF`
 *     The remote peer did shutdown on the connection.
 * :enum:`SPDYLAY_ERR_NOMEM`
 *     Out of memory.
 * :enum:`SPDYLAY_ERR_CALLBACK_FAILURE`
 *     The callback function failed.
 */
int spdylay_session_recv(spdylay_session *session);

/**
 * @function
 *
 * Processes data |in| as an input from the remote endpoint. The
 * |inlen| indicates the number of bytes in the |in|.
 *
 * This function behaves like `spdylay_session_recv()` except that it
 * does not use :member:`spdylay_session_callbacks.recv_callback` to
 * receive data; the |in| is the only data for the invocation of this
 * function. If all bytes are processed, this function returns. The
 * other callbacks are called in the same way as they are in
 * `spdylay_session_recv()`.
 *
 * In the current implementation, this function always tries to
 * processes all input data unless an error occurs.
 *
 * This function returns the number of processed bytes, or one of the
 * following negative error codes:
 *
 * :enum:`SPDYLAY_ERR_NOMEM`
 *     Out of memory.
 */
ssize_t spdylay_session_mem_recv(spdylay_session *session,
                                 const uint8_t *in, size_t inlen);

/**
 * @function
 *
 * Puts back previously deferred DATA frame in the stream |stream_id|
 * to the outbound queue.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`SPDYLAY_ERR_INVALID_ARGUMENT`
 *     The stream does not exist or no deferred data exist.
 * :enum:`SPDYLAY_ERR_NOMEM`
 *     Out of memory.
 */
int spdylay_session_resume_data(spdylay_session *session, int32_t stream_id);

/**
 * @function
 *
 * Returns nonzero value if |session| wants to receive data from the
 * remote peer.
 *
 * If both `spdylay_session_want_read()` and
 * `spdylay_session_want_write()` return 0, the application should
 * drop the connection.
 */
int spdylay_session_want_read(spdylay_session *session);

/**
 * @function
 *
 * Returns nonzero value if |session| wants to send data to the remote
 * peer.
 *
 * If both `spdylay_session_want_read()` and
 * `spdylay_session_want_write()` return 0, the application should
 * drop the connection.
 */
int spdylay_session_want_write(spdylay_session *session);

/**
 * @function
 *
 * Returns stream_user_data for the stream |stream_id|. The
 * stream_user_data is provided by `spdylay_submit_request()` or
 * `spdylay_submit_syn_stream()`.  If the stream is initiated by the
 * remote endpoint, stream_user_data is always ``NULL``. If the stream
 * is initiated by the local endpoint and ``NULL`` is given in
 * `spdylay_submit_request()` or `spdylay_submit_syn_stream()`, then
 * this function returns ``NULL``. If the stream does not exist, this
 * function returns ``NULL``.
 */
void* spdylay_session_get_stream_user_data(spdylay_session *session,
                                           int32_t stream_id);

/**
 * @function
 *
 * Returns the number of frames in the outbound queue. This does not
 * include the deferred DATA frames.
 */
size_t spdylay_session_get_outbound_queue_size(spdylay_session *session);

/**
 * @function
 *
 * Returns lowest priority value for the |session|.
 */
uint8_t spdylay_session_get_pri_lowest(spdylay_session *session);

/**
 * @function
 *
 * Submits GOAWAY frame.  The status code |status_code| is ignored if
 * the protocol version is :macro:`SPDYLAY_PROTO_SPDY2`.
 *
 * This function should be called when the connection should be
 * terminated after sending GOAWAY. If the remaining streams should be
 * processed after GOAWAY, use `spdylay_submit_goaway()` instead.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`SPDYLAY_ERR_NOMEM`
 *     Out of memory.
 */
int spdylay_session_fail_session(spdylay_session *session,
                                 uint32_t status_code);

/**
 * @function
 *
 * Returns string describing the |error_code|. The |error_code| must
 * be one of the :enum:`spdylay_error`.
 */
const char* spdylay_strerror(int error_code);

/**
 * @function
 *
 * Submits SYN_STREAM frame and optionally one or more DATA
 * frames.
 *
 * The |pri| is priority of this request. 0 is the highest priority
 * value. Use `spdylay_session_get_pri_lowest()` to know the lowest
 * priority value for this |session|.
 *
 * The |nv| contains the name/value pairs. For i >= 0, ``nv[2*i]``
 * contains a pointer to the name string and ``nv[2*i+1]`` contains a
 * pointer to the value string. The one beyond last value must be
 * ``NULL``. That is, if the |nv| contains N name/value pairs,
 * ``nv[2*N]`` must be ``NULL``.
 *
 * The |nv| must include following name/value pairs:
 *
 * ``:method``
 *     HTTP method (e.g., ``GET``, ``POST``, ``HEAD``, etc)
 * ``:scheme``
 *     URI scheme (e.g., ``https``)
 * ``:path``
 *     Absolute path and parameters of this request (e.g., ``/foo``,
 *     ``/foo;bar;haz?h=j&y=123``)
 * ``:version``
 *     HTTP version (e.g., ``HTTP/1.1``)
 * ``:host``
 *     The hostport portion of the URI for this request (e.g.,
 *     ``example.org:443``). This is the same as the HTTP "Host" header
 *     field.
 *
 * If the |session| is initialized with the version
 * :macro:`SPDYLAY_PROTO_SPDY2`, the above names are translated to
 * ``method``, ``scheme``, ``url``, ``version`` and ``host``
 * respectively.
 *
 * This function creates copies of all name/value pairs in |nv|.  It
 * also lower-cases all names in |nv|.
 *
 * If |data_prd| is not ``NULL``, it provides data which will be sent
 * in subsequent DATA frames. In this case, a method that allows
 * request message bodies
 * (http://www.w3.org/Protocols/rfc2616/rfc2616-sec9.html#sec9) must
 * be specified with ``:method`` key in |nv| (e.g. ``POST``). This
 * function does not take ownership of the |data_prd|. The function
 * copies the members of the |data_prd|. If |data_prd| is ``NULL``,
 * SYN_STREAM have FLAG_FIN set. The |stream_user_data| is data
 * associated to the stream opened by this request and can be an
 * arbitrary pointer, which can be retrieved later by
 * `spdylay_session_get_stream_user_data()`.
 *
 * Since the library reorders the frames and tries to send the highest
 * prioritized one first and the SPDY specification requires the
 * stream ID must be strictly increasing, the stream ID of this
 * request cannot be known until it is about to sent.  To know the
 * stream ID of the request, the application can use
 * :member:`spdylay_session_callbacks.before_ctrl_send_callback`. This
 * callback is called just before the frame is sent. For SYN_STREAM
 * frame, the argument frame has the stream ID assigned. Also since
 * the stream is already opened,
 * `spdylay_session_get_stream_user_data()` can be used to get
 * |stream_user_data| to identify which SYN_STREAM we are processing.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`SPDYLAY_ERR_INVALID_ARGUMENT`
 *     The |pri| is invalid; or the |nv| includes empty name or NULL
 *     value.
 * :enum:`SPDYLAY_ERR_NOMEM`
 *     Out of memory.
 */
int spdylay_submit_request(spdylay_session *session, uint8_t pri,
                           const char **nv,
                           const spdylay_data_provider *data_prd,
                           void *stream_user_data);

/**
 * @function
 *
 * Submits SYN_REPLY frame and optionally one or more DATA frames
 * against the stream |stream_id|.
 *
 * The |nv| contains the name/value pairs. For i >= 0, ``nv[2*i]``
 * contains a pointer to the name string and ``nv[2*i+1]`` contains a
 * pointer to the value string. The one beyond last value must be
 * ``NULL``. That is, if the |nv| contains N name/value pairs,
 * ``nv[2*N]`` must be ``NULL``.
 *
 * The |nv| must include following name/value pairs:
 *
 * ``:status``
 *     HTTP status code (e.g., ``200`` or ``200 OK``)
 * ``:version``
 *     HTTP response version (e.g., ``HTTP/1.1``)
 *
 * If the |session| is initialized with the version
 * :macro:`SPDYLAY_PROTO_SPDY2`, the above names are translated to
 * ``status`` and ``version`` respectively.
 *
 * This function creates copies of all name/value pairs in |nv|.  It
 * also lower-cases all names in |nv|.
 *
 * If |data_prd| is not ``NULL``, it provides data which will be sent
 * in subsequent DATA frames.  This function does not take ownership
 * of the |data_prd|. The function copies the members of the
 * |data_prd|.  If |data_prd| is ``NULL``, SYN_REPLY will have
 * FLAG_FIN set.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`SPDYLAY_ERR_INVALID_ARGUMENT`
 *     The |nv| includes empty name or NULL value.
 * :enum:`SPDYLAY_ERR_NOMEM`
 *     Out of memory.
 */
int spdylay_submit_response(spdylay_session *session,
                            int32_t stream_id, const char **nv,
                            const spdylay_data_provider *data_prd);

/**
 * @function
 *
 * Submits SYN_STREAM frame. The |flags| is bitwise OR of the
 * following values:
 *
 * * :enum:`SPDYLAY_CTRL_FLAG_FIN`
 * * :enum:`SPDYLAY_CTRL_FLAG_UNIDIRECTIONAL`
 *
 * If |flags| includes :enum:`SPDYLAY_CTRL_FLAG_FIN`, this frame has
 * FLAG_FIN flag set.
 *
 * The |assoc_stream_id| is used for server-push. Specify 0 if this
 * stream is not server-push. If |session| is initialized for client
 * use, |assoc_stream_id| is ignored.
 *
 * The |pri| is priority of this request. 0 is the highest priority
 * value. Use `spdylay_session_get_pri_lowest()` to know the lowest
 * priority value for this |session|.
 *
 * The |nv| contains the name/value pairs. For i >= 0, ``nv[2*i]``
 * contains a pointer to the name string and ``nv[2*i+1]`` contains a
 * pointer to the value string. The one beyond last value must be
 * ``NULL``. That is, if the |nv| contains N name/value pairs,
 * ``nv[2*N]`` must be ``NULL``.
 *
 * This function creates copies of all name/value pairs in |nv|.  It
 * also lower-cases all names in |nv|.
 *
 * The |stream_user_data| is a pointer to an arbitrary
 * data which is associated to the stream this frame will open.
 *
 * This function is low-level in a sense that the application code can
 * specify flags and the Associated-To-Stream-ID directly. For usual
 * HTTP request, `spdylay_submit_request()` is useful.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`SPDYLAY_ERR_INVALID_ARGUMENT`
 *     The |pri| is invalid; or the |assoc_stream_id| is invalid; or
 *     the |nv| includes empty name or NULL value.
 * :enum:`SPDYLAY_ERR_NOMEM`
 *     Out of memory.
 */
int spdylay_submit_syn_stream(spdylay_session *session, uint8_t flags,
                              int32_t assoc_stream_id, uint8_t pri,
                              const char **nv, void *stream_user_data);

/**
 * @function
 *
 * Submits SYN_REPLY frame. The |flags| is bitwise OR of the following
 * values:
 *
 * * :enum:`SPDYLAY_CTRL_FLAG_FIN`
 *
 * If |flags| includes :enum:`SPDYLAY_CTRL_FLAG_FIN`, this frame has
 * FLAG_FIN flag set.
 *
 * The stream which this frame belongs to is given in the
 * |stream_id|. The |nv| is the name/value pairs in this frame.
 *
 * The |nv| contains the name/value pairs. For i >= 0, ``nv[2*i]``
 * contains a pointer to the name string and ``nv[2*i+1]`` contains a
 * pointer to the value string. The one beyond last value must be
 * ``NULL``. That is, if the |nv| contains N name/value pairs,
 * ``nv[2*N]`` must be ``NULL``.
 *
 * This function creates copies of all name/value pairs in |nv|.  It
 * also lower-cases all names in |nv|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`SPDYLAY_ERR_INVALID_ARGUMENT`
 *     The |nv| includes empty name or NULL value.
 * :enum:`SPDYLAY_ERR_NOMEM`
 *     Out of memory.
 */
int spdylay_submit_syn_reply(spdylay_session *session, uint8_t flags,
                             int32_t stream_id, const char **nv);

/**
 * @function
 *
 * Submits HEADERS frame. The |flags| is bitwise OR of the following
 * values:
 *
 * * :enum:`SPDYLAY_CTRL_FLAG_FIN`
 *
 * If |flags| includes :enum:`SPDYLAY_CTRL_FLAG_FIN`, this frame has
 * FLAG_FIN flag set.
 *
 * The stream which this frame belongs to is given in the
 * |stream_id|. The |nv| is the name/value pairs in this frame.
 *
 * The |nv| contains the name/value pairs. For i >= 0, ``nv[2*i]``
 * contains a pointer to the name string and ``nv[2*i+1]`` contains a
 * pointer to the value string. The one beyond last value must be
 * ``NULL``. That is, if the |nv| contains N name/value pairs,
 * ``nv[2*N]`` must be ``NULL``.
 *
 * This function creates copies of all name/value pairs in |nv|.  It
 * also lower-cases all names in |nv|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`SPDYLAY_ERR_INVALID_ARGUMENT`
 *     The |nv| includes empty name or NULL value.
 * :enum:`SPDYLAY_ERR_NOMEM`
 *     Out of memory.
 */
int spdylay_submit_headers(spdylay_session *session, uint8_t flags,
                           int32_t stream_id, const char **nv);

/**
 * @function
 *
 * Submits one or more DATA frames to the stream |stream_id|.  The
 * data to be sent are provided by |data_prd|. If |flags| contains
 * :enum:`SPDYLAY_DATA_FLAG_FIN`, the last DATA frame has FLAG_FIN
 * set.
 *
 * This function does not take ownership of the |data_prd|. The
 * function copies the members of the |data_prd|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`SPDYLAY_ERR_NOMEM`
 *     Out of memory.
 */
int spdylay_submit_data(spdylay_session *session, int32_t stream_id,
                        uint8_t flags, const spdylay_data_provider *data_prd);

/**
 * @function
 *
 * Submits RST_STREAM frame to cancel/reject the stream |stream_id|
 * with the status code |status_code|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`SPDYLAY_ERR_NOMEM`
 *     Out of memory.
 */
int spdylay_submit_rst_stream(spdylay_session *session, int32_t stream_id,
                              uint32_t status_code);

/**
 * @function
 *
 * Submits PING frame. You don't have to send PING back when you
 * received PING frame. The library automatically submits PING frame
 * in this case.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`SPDYLAY_ERR_NOMEM`
 *     Out of memory.
 */
int spdylay_submit_ping(spdylay_session *session);

/**
 * @function
 *
 * Submits GOAWAY frame. The status code |status_code| is ignored if
 * the protocol version is :macro:`SPDYLAY_PROTO_SPDY2`.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`SPDYLAY_ERR_NOMEM`
 *     Out of memory.
 */
int spdylay_submit_goaway(spdylay_session *session, uint32_t status_code);

/**
 * @function
 *
 * Stores local settings and submits SETTINGS frame. The |iv| is the
 * pointer to the array of :type:`spdylay_settings_entry`. The |niv|
 * indicates the number of :type:`spdylay_settings_entry`. The |flags|
 * is bitwise-OR of one or more values from
 * :type:`spdylay_settings_flag`.
 *
 * This function does not take ownership of the |iv|. This function
 * copies all the elements in the |iv|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`SPDYLAY_ERR_INVALID_ARGUMENT`
 *     The |iv| contains duplicate settings ID or invalid value.
 * :enum:`SPDYLAY_ERR_NOMEM`
 *     Out of memory.
 */
int spdylay_submit_settings(spdylay_session *session, uint8_t flags,
                            const spdylay_settings_entry *iv, size_t niv);

/**
 * @function
 *
 * Submits WINDOW_UPDATE frame. The effective range of the
 * |delta_window_size| is [1, (1 << 31)-1], inclusive. But the
 * application must be responsible to keep the resulting window size
 * <= (1 << 31)-1.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`SPDYLAY_ERR_INVALID_ARGUMENT`
 *     The |delta_window_size| is 0 or negative.
 * :enum:`SPDYLAY_ERR_STREAM_CLOSED`
 *     The stream is already closed or does not exist.
 * :enum:`SPDYLAY_ERR_NOMEM`
 *     Out of memory.
 */
int spdylay_submit_window_update(spdylay_session *session, int32_t stream_id,
                                 int32_t delta_window_size);

/**
 * @function
 *
 * A helper function for dealing with NPN in client side.  The |in|
 * contains server's protocol in preferable order.  The format of |in|
 * is length-prefixed and not null-terminated.  For example,
 * ``spdy/2`` and ``http/1.1`` stored in |in| like this::
 *
 *     in[0] = 6
 *     in[1..6] = "spdy/2"
 *     in[7] = 8
 *     in[8..15] = "http/1.1"
 *     inlen = 16
 *
 * The selection algorithm is as follows:
 *
 * 1. If server's list contains SPDY versions the spdylay library
 *    supports, this function selects one of them and returns its SPDY
 *    protocol version which can be used directly with
 *    `spdylay_session_client_new()` and
 *    `spdylay_session_server_new()` . The following steps are not
 *    taken.
 *
 * 2. If server's list contains ``http/1.1``, this function selects
 *    ``http/1.1`` and returns 0. The following step is not taken.
 *
 * 3. This function selects nothing and returns -1. (So called
 *    non-overlap case). In this case, |out| and |outlen| are left
 *    untouched.
 *
 * When spdylay supports updated version of SPDY in the future, this
 * function may select updated protocol and application code which
 * relies on spdylay for SPDY stuff needs not be modified.
 *
 * Selecting ``spdy/2`` means that ``spdy/2`` is written into |*out|
 * and length of ``spdy/2`` (which is 6) is assigned to |*outlen|.
 *
 * See http://technotes.googlecode.com/git/nextprotoneg.html for more
 * details about NPN.
 *
 * To use this method you should do something like::
 *
 *     static int select_next_proto_cb(SSL* ssl,
 *                                     unsigned char **out,
 *                                     unsigned char *outlen,
 *                                     const unsigned char *in,
 *                                     unsigned int inlen,
 *                                     void *arg)
 *     {
 *         int version;
 *         version = spdylay_select_next_protocol(out, outlen, in, inlen);
 *         if(version > 0) {
 *             ((MyType*)arg)->spdy_version = version;
 *         }
 *         return SSL_TLSEXT_ERR_OK;
 *     }
 *     ...
 *     SSL_CTX_set_next_proto_select_cb(ssl_ctx, select_next_proto_cb, my_obj);
 */
int spdylay_select_next_protocol(unsigned char **out, unsigned char *outlen,
                                 const unsigned char *in, unsigned int inlen);

/**
 * @struct
 *
 * This struct contains SPDY version information this library
 * supports.
 */
typedef struct {
  /**
   * SPDY protocol version name which can be used as TLS NPN protocol
   * string.
   */
  const unsigned char *proto;
  /**
   * The length of proto member.
   */
  uint8_t len;
  /**
   * The corresponding SPDY version constant which can be passed to
   * `spdylay_session_client_new()` and `spdylay_session_server_new()`
   * as version argument.
   */
  uint16_t version;
} spdylay_npn_proto;

/**
 * @function
 *
 * Returns a pointer to the supported SPDY version list. The number of
 * elements in the list will be assigned to the |*len_ptr|. It
 * contains all SPDY version information this library supports. The
 * application can use this information to configure NPN protocol
 * offerings/selection.
 */
const spdylay_npn_proto* spdylay_npn_get_proto_list(size_t *len_ptr);

/**
 * @function
 *
 * Returns spdy version which spdylay library supports from the given
 * protocol name. The |proto| is the pointer to the protocol name and
 * |protolen| is its length. Currently, ``spdy/2`` and ``spdy/3`` are
 * supported.
 *
 * This function returns nonzero spdy version if it succeeds, or 0.
 */
uint16_t spdylay_npn_get_version(const unsigned char *proto, size_t protolen);

struct spdylay_gzip;

/**
 * @struct
 *
 * The gzip stream to inflate data. The details of this structure are
 * intentionally hidden from the public API.
 */
typedef struct spdylay_gzip spdylay_gzip;

/**
 * @function
 *
 * A helper function to set up a per request gzip stream to inflate data.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`SPDYLAY_ERR_GZIP`
 *     The initialization of gzip stream failed.
 * :enum:`SPDYLAY_ERR_NOMEM`
 *     Out of memory.
 */
int spdylay_gzip_inflate_new(spdylay_gzip **inflater_ptr);

/**
 * @function
 *
 * Frees the inflate stream.  The |inflater| may be ``NULL``.
 */
void spdylay_gzip_inflate_del(spdylay_gzip *inflater);

/**
 * @function
 *
 * Inflates data in |in| with the length |*inlen_ptr| and stores the
 * inflated data to |out| which has allocated size at least
 * |*outlen_ptr|. On return, |*outlen_ptr| is updated to represent
 * the number of data written in |out|.  Similarly, |*inlen_ptr| is
 * updated to represent the number of input bytes processed.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`SPDYLAY_ERR_GZIP`
 *     The inflation of gzip stream failed.
 *
 * The example follows::
 *
 *     void on_data_chunk_recv_callback(spdylay_session *session,
 *                                      uint8_t flags,
 *                                      int32_t stream_id,
 *                                      const uint8_t *data, size_t len,
 *                                      void *user_data)
 *     {
 *         ...
 *         req = spdylay_session_get_stream_user_data(session, stream_id);
 *         spdylay_gzip *inflater = req->inflater;
 *         while(len > 0) {
 *             uint8_t out[MAX_OUTLEN];
 *             size_t outlen = MAX_OUTLEN;
 *             size_t tlen = len;
 *             int rv;
 *             rv = spdylay_gzip_inflate(inflater, out, &outlen, data, &tlen);
 *             if(rv != 0) {
 *                 spdylay_submit_rst_stream(session, stream_id,
 *                                           SPDYLAY_INTERNAL_ERROR);
 *                 break;
 *             }
 *             ... Do stuff ...
 *             data += tlen;
 *             len -= tlen;
 *         }
 *         ....
 *     }
 */
int spdylay_gzip_inflate(spdylay_gzip *inflater,
                         uint8_t *out, size_t *outlen_ptr,
                         const uint8_t *in, size_t *inlen_ptr);

#ifdef __cplusplus
}
#endif

#endif /* SPDYLAY_H */
