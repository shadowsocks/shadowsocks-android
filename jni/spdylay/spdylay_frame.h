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
#ifndef SPDYLAY_FRAME_H
#define SPDYLAY_FRAME_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <spdylay/spdylay.h>
#include "spdylay_zlib.h"
#include "spdylay_buffer.h"
#include "spdylay_client_cert_vector.h"

#define SPDYLAY_STREAM_ID_MASK 0x7fffffff
/* This is actually the maximum length of a control frame in SPDY/2
   and 3. */
#define SPDYLAY_LENGTH_MASK 0xffffff
#define SPDYLAY_VERSION_MASK 0x7fff
#define SPDYLAY_DELTA_WINDOW_SIZE_MASK 0x7fffffff
#define SPDYLAY_SETTINGS_ID_MASK 0xffffff

/* The length of DATA frame payload. */
#define SPDYLAY_DATA_PAYLOAD_LENGTH 4096

/* The number of bytes of frame header. */
#define SPDYLAY_FRAME_HEAD_LENGTH 8

/* The offset to the name/value header block in the frame (including
   frame header) */
#define SPDYLAY_SYN_STREAM_NV_OFFSET 18

#define SPDYLAY_SPDY2_SYN_REPLY_NV_OFFSET 14
#define SPDYLAY_SPDY3_SYN_REPLY_NV_OFFSET 12

#define SPDYLAY_SPDY2_HEADERS_NV_OFFSET 14
#define SPDYLAY_SPDY3_HEADERS_NV_OFFSET 12

#define spdylay_frame_get_nv_len(RED, LEN_SIZE)                   \
  (LEN_SIZE == 2 ? spdylay_buffer_reader_uint16(RED) :            \
   spdylay_buffer_reader_uint32(RED))

#define spdylay_frame_put_nv_len(OUT, VAL, LEN_SIZE)                    \
  (LEN_SIZE == 2 ?                                                      \
   spdylay_put_uint16be(OUT, VAL) : spdylay_put_uint32be(OUT, VAL))

/* Category of SPDY frames. */
typedef enum {
  /* Control frame */
  SPDYLAY_CTRL,
  /* DATA frame */
  SPDYLAY_DATA
} spdylay_frame_category;

/**
 * @struct
 * The DATA frame. It has the following members:
 */
typedef struct {
  /**
   * The stream ID.
   */
  int32_t stream_id;
  /**
   * The DATA frame flags. See :type:`spdylay_data_flag`.
   */
  uint8_t flags;
  /**
   * The flag to indicate whether EOF was reached or not. Initially
   * |eof| is 0. It becomes 1 after all data were read.
   */
  uint8_t eof;
  /**
   * The data to be sent for this DATA frame.
   */
  spdylay_data_provider data_prd;
} spdylay_data;

/*
 * Returns the number of bytes in length of name/value pair for the
 * given protocol version |version|. If |version| is not supported,
 * returns 0.
 */
size_t spdylay_frame_get_len_size(uint16_t version);

/*
 * Packs SYN_STREAM frame |frame| in wire format and store it in
 * |*buf_ptr|.  The capacity of |*buf_ptr| is |*buflen_ptr| bytes.
 * The |*nvbuf_ptr| is used to store inflated name/value pairs in wire
 * format temporarily. Its length is |*nvbuflen_ptr| bytes.  This
 * function expands |*buf_ptr| and |*nvbuf_ptr| as necessary to store
 * frame and name/value pairs. When expansion occurred, memory
 * previously pointed by |*buf_ptr| and |*nvbuf_ptr| is freed.
 * |*buf_ptr|, |*buflen_ptr|, |*nvbuf_ptr| and |*nvbuflen_ptr| are
 * updated accordingly.
 *
 * frame->hd.length is assigned after length is determined during
 * packing process.
 *
 * This function returns the size of packed frame if it succeeds, or
 * returns one of the following negative error codes:
 *
 * SPDYLAY_ERR_UNSUPPORTED_VERSION
 *     The version is not supported.
 * SPDYLAY_ERR_ZLIB
 *     The deflate operation failed.
 * SPDYLAY_ERR_FRAME_TOO_LARGE
 *     The length of the frame is too large.
 * SPDYLAY_ERR_NOMEM
 *     Out of memory.
 */
ssize_t spdylay_frame_pack_syn_stream(uint8_t **buf_ptr,
                                      size_t *buflen_ptr,
                                      uint8_t **nvbuf_ptr,
                                      size_t *nvbuflen_ptr,
                                      spdylay_syn_stream *frame,
                                      spdylay_zlib *deflater);

/*
 * Unpacks SYN_STREAM frame byte sequence into |frame|.  The control
 * frame header is given in |head| with |headlen| length. In spdy/3
 * spec, headlen is 8 bytes. |payload| is the data after length field
 * of the header and just before name/value header block.
 *
 * The |inflatebuf| contains inflated name/value header block in wire
 * foramt.
 *
 * This function also validates the name/value pairs. If unpacking
 * succeeds but validation fails, it is indicated by returning
 * SPDYLAY_ERR_INVALID_HEADER_BLOCK.
 *
 * This function returns 0 if it succeeds or one of the following
 * negative error codes:
 *
 * SPDYLAY_ERR_INVALID_HEADER_BLOCK
 *     Unpacking succeeds but the header block is invalid.
 * SPDYLAY_ERR_INVALID_FRAME
 *     The input data are invalid.
 * SPDYLAY_ERR_UNSUPPORTED_VERSION
 *     The version is not supported.
 * SPDYLAY_ERR_NOMEM
 *     Out of memory.
 */
int spdylay_frame_unpack_syn_stream(spdylay_syn_stream *frame,
                                    const uint8_t *head, size_t headlen,
                                    const uint8_t *payload, size_t payloadlen,
                                    spdylay_buffer *inflatebuf);

/*
 * Unpacks SYN_STREAM frame byte sequence into |frame|. This function
 * only unapcks bytes that come before name/value header block.
 *
 * This function returns 0 if it succeeds or one of the following
 * negative error codes:
 *
 * SPDYLAY_ERR_INVALID_FRAME
 *     The input data are invalid.
 */
int spdylay_frame_unpack_syn_stream_without_nv(spdylay_syn_stream *frame,
                                               const uint8_t *head,
                                               size_t headlen,
                                               const uint8_t *payload,
                                               size_t payloadlen);

/*
 * Packs SYN_REPLY frame |frame| in wire frame format and store it in
 * |*buf_ptr|.  The capacity of |*buf_ptr| is |*buflen_ptr| bytes.
 * The |*nvbuf_ptr| is used to store inflated name/value pairs in wire
 * format temporarily. Its length is |*nvbuflen_ptr| bytes.  This
 * function expands |*buf_ptr| and |*nvbuf_ptr| as necessary to store
 * frame and name/value pairs. When expansion occurred, memory
 * previously pointed by |*buf_ptr| and |*nvbuf_ptr| is freed.
 * |*buf_ptr|, |*buflen_ptr|, |*nvbuf_ptr| and |*nvbuflen_ptr| are
 * updated accordingly.
 *
 * frame->hd.length is assigned after length is determined during
 * packing process.
 *
 * This function returns the size of packed frame if it succeeds, or
 * returns one of the following negative error codes:
 *
 * SPDYLAY_ERR_UNSUPPORTED_VERSION
 *     The version is not supported.
 * SPDYLAY_ERR_ZLIB
 *     The deflate operation failed.
 * SPDYLAY_ERR_FRAME_TOO_LARGE
 *     The length of the frame is too large.
 * SPDYLAY_ERR_NOMEM
 *     Out of memory.
 */
ssize_t spdylay_frame_pack_syn_reply(uint8_t **buf_ptr,
                                     size_t *buflen_ptr,
                                     uint8_t **nvbuf_ptr,
                                     size_t *nvbuflen_ptr,
                                     spdylay_syn_reply *frame,
                                     spdylay_zlib *deflater);

/*
 * Unpacks SYN_REPLY frame byte sequence into |frame|.
 *
 * The |inflatebuf| contains inflated name/value header block in wire
 * foramt.
 *
 * This function also validates the name/value pairs. If unpacking
 * succeeds but validation fails, it is indicated by returning
 * SPDYLAY_ERR_INVALID_HEADER_BLOCK.
 *
 * This function returns 0 if it succeeds or one of the following
 * negative error codes:
 *
 * SPDYLAY_ERR_INVALID_HEADER_BLOCK
 *     Unpacking succeeds but the header block is invalid.
 * SPDYLAY_ERR_UNSUPPORTED_VERSION
 *     The version is not supported.
 * SPDYLAY_ERR_INVALID_FRAME
 *     The input data are invalid.
 * SPDYLAY_ERR_NOMEM
 *     Out of memory.
 */
int spdylay_frame_unpack_syn_reply(spdylay_syn_reply *frame,
                                   const uint8_t *head, size_t headlen,
                                   const uint8_t *payload, size_t payloadlen,
                                   spdylay_buffer *inflatebuf);

/*
 * Unpacks SYN_REPLY frame byte sequence into |frame|. This function
 * only unapcks bytes that come before name/value header block.
 *
 * This function returns 0 if it succeeds or one of the following
 * negative error codes:
 *
 * SPDYLAY_ERR_INVALID_FRAME
 *     The input data are invalid.
 */
int spdylay_frame_unpack_syn_reply_without_nv(spdylay_syn_reply *frame,
                                              const uint8_t *head,
                                              size_t headlen,
                                              const uint8_t *payload,
                                              size_t payloadlen);

/*
 * Packs PING frame |frame| in wire format and store it in
 * |*buf_ptr|. The capacity of |*buf_ptr| is |*buflen_ptr|
 * length. This function expands |*buf_ptr| as necessary to store
 * given |frame|.
 *
 * This function returns 0 if it succeeds or one of the following
 * negative error codes:
 *
 * SPDYLAY_ERR_NOMEM
 *     Out of memory.
 */
ssize_t spdylay_frame_pack_ping(uint8_t **buf_ptr, size_t *buflen_ptr,
                                spdylay_ping *frame);

/*
 * Unpacks PING wire format into |frame|.
 *
 * This function returns 0 if it succeeds or one of the following
 * negative error codes:
 *
 * SPDYLAY_ERR_INVALID_FRAME
 *     The input data are invalid.
 */
int spdylay_frame_unpack_ping(spdylay_ping *frame,
                              const uint8_t *head, size_t headlen,
                              const uint8_t *payload, size_t payloadlen);

/*
 * Packs GOAWAY frame |frame | in wire format and store it in
 * |*buf_ptr|. The capacity of |*buf_ptr| is |*buflen_ptr|
 * length. This function expands |*buf_ptr| as necessary to store
 * given |frame|.
 *
 * This function returns 0 if it succeeds or one of the following
 * negative error codes:
 *
 * SPDYLAY_ERR_UNSUPPORTED_VERSION
 *     The version is not supported.
 * SPDYLAY_ERR_NOMEM
 *     Out of memory.
 */
ssize_t spdylay_frame_pack_goaway(uint8_t **buf_ptr, size_t *buflen_ptr,
                                  spdylay_goaway *frame);

/*
 * Unpacks GOAWAY wire format into |frame|.
 *
 * This function returns 0 if it succeeds or one of the following
 * negative error codes:
 *
 * SPDYLAY_ERR_UNSUPPORTED_VERSION
 *     The version is not supported.
 * SPDYLAY_ERR_INVALID_FRAME
 *     The input data are invalid.
 */
int spdylay_frame_unpack_goaway(spdylay_goaway *frame,
                                const uint8_t *head, size_t headlen,
                                const uint8_t *payload, size_t payloadlen);

/*
 * Packs HEADERS frame |frame| in wire format and store it in
 * |*buf_ptr|.  The capacity of |*buf_ptr| is |*buflen_ptr| bytes.
 * The |*nvbuf_ptr| is used to store inflated name/value pairs in wire
 * format temporarily. Its length is |*nvbuflen_ptr| bytes.  This
 * function expands |*buf_ptr| and |*nvbuf_ptr| as necessary to store
 * frame and name/value pairs. When expansion occurred, memory
 * previously pointed by |*buf_ptr| and |*nvbuf_ptr| is freed.
 * |*buf_ptr|, |*buflen_ptr|, |*nvbuf_ptr| and |*nvbuflen_ptr| are
 * updated accordingly.
 *
 * frame->hd.length is assigned after length is determined during
 * packing process.
 *
 * This function returns the size of packed frame if it succeeds, or
 * returns one of the following negative error codes:
 *
 * SPDYLAY_ERR_UNSUPPORTED_VERSION
 *     The version is not supported.
 * SPDYLAY_ERR_ZLIB
 *     The deflate operation failed.
 * SPDYLAY_ERR_FRAME_TOO_LARGE
 *     The length of the frame is too large.
 * SPDYLAY_ERR_NOMEM
 *     Out of memory.
 */
ssize_t spdylay_frame_pack_headers(uint8_t **buf_ptr, size_t *buflen_ptr,
                                   uint8_t **nvbuf_ptr, size_t *nvbuflen_ptr,
                                   spdylay_headers *frame,
                                   spdylay_zlib *deflater);

/*
 * Unpacks HEADERS wire format into |frame|.
 *
 * The |inflatebuf| contains inflated name/value header block in wire
 * foramt.
 *
 * This function also validates the name/value pairs. If unpacking
 * succeeds but validation fails, it is indicated by returning
 * SPDYLAY_ERR_INVALID_HEADER_BLOCK.
 *
 * This function returns 0 if it succeeds or one of the following
 * negative error codes:
 *
 * SPDYLAY_ERR_INVALID_HEADER_BLOCK
 *     Unpacking succeeds but the header block is invalid.
 * SPDYLAY_ERR_UNSUPPORTED_VERSION
 *     The version is not supported.
 * SPDYLAY_ERR_INVALID_FRAME
 *     The input data are invalid.
 * SPDYLAY_ERR_NOMEM
 *     Out of memory.
 */
int spdylay_frame_unpack_headers(spdylay_headers *frame,
                                 const uint8_t *head, size_t headlen,
                                 const uint8_t *payload, size_t payloadlen,
                                 spdylay_buffer *inflatebuf);

/*
 * Unpacks HEADERS frame byte sequence into |frame|. This function
 * only unapcks bytes that come before name/value header block.
 *
 * This function returns 0 if it succeeds or one of the following
 * negative error codes:
 *
 * SPDYLAY_ERR_INVALID_FRAME
 *     The input data are invalid.
 */
int spdylay_frame_unpack_headers_without_nv(spdylay_headers *frame,
                                            const uint8_t *head,
                                            size_t headlen,
                                            const uint8_t *payload,
                                            size_t payloadlen);

/*
 * Packs RST_STREAM frame |frame| in wire frame format and store it in
 * |*buf_ptr|. The capacity of |*buf_ptr| is |*buflen_ptr|
 * length. This function expands |*buf_ptr| as necessary to store
 * given |frame|. In spdy/2 spec, RST_STREAM wire format is always 16
 * bytes long.
 *
 * This function returns the size of packed frame if it succeeds, or
 * returns one of the following negative error codes:
 *
 * SPDYLAY_ERR_NOMEM
 *     Out of memory.
 */
ssize_t spdylay_frame_pack_rst_stream(uint8_t **buf_ptr, size_t *buflen_ptr,
                                      spdylay_rst_stream *frame);

/*
 * Unpacks RST_STREAM frame byte sequence into |frame|.
 *
 * This function returns 0 if it succeeds or one of the following
 * negative error codes:
 *
 * SPDYLAY_ERR_INVALID_FRAME
 *     The input data are invalid.
 */
int spdylay_frame_unpack_rst_stream(spdylay_rst_stream *frame,
                                    const uint8_t *head, size_t headlen,
                                    const uint8_t *payload, size_t payloadlen);


/*
 * Packs WINDOW_UPDATE frame |frame| in wire frame format and store it
 * in |*buf_ptr|. The capacity of |*buf_ptr| is |*buflen_ptr|
 * length. This function expands |*buf_ptr| as necessary to store
 * given |frame|. In SPDY/3 spec, WINDOW_UPDATE wire format is always 16
 * bytes long.
 *
 * This function returns the size of packed frame if it succeeds, or
 * returns one of the following negative error codes:
 *
 * SPDYLAY_ERR_NOMEM
 *     Out of memory.
 */
ssize_t spdylay_frame_pack_window_update(uint8_t **buf_ptr, size_t *buflen_ptr,
                                         spdylay_window_update *frame);

/*
 * Unpacks WINDOW_UPDATE frame byte sequence into |frame|.
 *
 * This function returns 0 if it succeeds or one of the following
 * negative error codes:
 *
 * SPDYLAY_ERR_INVALID_FRAME
 *     The input data are invalid.
 */
int spdylay_frame_unpack_window_update(spdylay_window_update *frame,
                                       const uint8_t *head, size_t headlen,
                                       const uint8_t *payload,
                                       size_t payloadlen);

/*
 * Packs SETTINGS frame |frame| in wire format and store it in
 * |*buf_ptr|. The capacity of |*buf_ptr| is |*buflen_ptr|
 * length. This function expands |*buf_ptr| as necessary to store
 * given |frame|.
 *
 * This function returns the size of packed frame if it succeeds, or
 * returns one of the following negative error codes:
 *
 * SPDYLAY_ERR_UNSUPPORTED_VERSION
 *     The version is not supported.
 * SPDYLAY_ERR_NOMEM
 *     Out of memory.
 */
ssize_t spdylay_frame_pack_settings(uint8_t **buf_ptr, size_t *buflen_ptr,
                                    spdylay_settings *frame);

/*
 * Unpacks SETTINGS wire format into |frame|.
 *
 * This function returns 0 if it succeeds or one of the following
 * negative error codes:
 *
 * SPDYLAY_ERR_UNSUPPORTED_VERSION
 *     The version is not supported.
 * SPDYLAY_ERR_INVALID_FRAME
 *     The input data are invalid.
 * SPDYLAY_ERR_NOMEM
 *     Out of memory.
 */
int spdylay_frame_unpack_settings(spdylay_settings *frame,
                                  const uint8_t *head, size_t headlen,
                                  const uint8_t *payload, size_t payloadlen);

/*
 * Packs CREDENTIAL frame |frame| in wire format and store it in
 * |*buf_ptr|. The capacity of |*buf_ptr| is |*buflen_ptr|
 * length. This function expands |*buf_ptr| as necessary to store
 * given |frame|.
 *
 * This function returns the size of packed frame if it succeeds, or
 * returns one of the following negative error codes:
 *
 * SPDYLAY_ERR_NOMEM
 *     Out of memory.
 */
ssize_t spdylay_frame_pack_credential(uint8_t **buf_ptr, size_t *buflen_ptr,
                                      spdylay_credential *frame);

/*
 * Unpacks CREDENTIAL wire format into |frame|.
 *
 * This function returns 0 if it succeeds or one of the following
 * negative error codes:
 *
 * SPDYLAY_ERR_INVALID_FRAME
 *     The input data are invalid.
 * SPDYLAY_ERR_NOMEM
 *     Out of memory.
 */
int spdylay_frame_unpack_credential(spdylay_credential *frame,
                                    const uint8_t *head, size_t headlen,
                                    const uint8_t *payload, size_t payloadlen);
/*
 * Returns number of bytes to pack name/value pairs |nv|. This
 * function expects |nv| is sorted in ascending order of key.
 * |len_size| is the number of bytes in length of name/value pair and
 * it must be 2 or 4.
 *
 * This function can handles duplicate keys and concatenation of thier
 * values with '\0'.
 */
size_t spdylay_frame_count_nv_space(char **nv, size_t len_size);

/*
 * Packs name/value pairs in |nv| in |buf|. |buf| must have at least
 * spdylay_frame_count_nv_space(nv) bytes.  |len_size| is the number
 * of bytes in length of name/value pair and it must be 2 or 4.
 */
ssize_t spdylay_frame_pack_nv(uint8_t *buf, char **nv, size_t len_size);

/*
 * Packs name/value pairs in |nv| in |*buf_ptr| with offset
 * |nv_offset|.  It means first byte of packed name/value pairs is
 * stored in |*buf_ptr|+|nv_offset|.  |*buf_ptr| and |*nvbuf_ptr| are
 * expanded as necessary.
 *
 * This function returns the number of the bytes for the frame
 * containing this name/value pairs if it succeeds, or one of the
 * following negative error codes:
 *
 * SPDYLAY_ERR_ZLIB
 *     The deflate operation failed.
 * SPDYLAY_ERR_FRAME_TOO_LARGE
 *     The length of the frame is too large.
 * SPDYLAY_ERR_NOMEM
 *     Out of memory.
 */
ssize_t spdylay_frame_alloc_pack_nv(uint8_t **buf_ptr,
                                    size_t *buflen_ptr,
                                    uint8_t **nvbuf_ptr,
                                    size_t *nvbuflen_ptr,
                                    char **nv, size_t nv_offset,
                                    size_t len_size,
                                    spdylay_zlib *deflater);

/*
 * Counts number of name/value pair in |in| and computes length of
 * buffers to store unpacked name/value pair and store them in
 * |*nvlen_ptr| and |*buflen_ptr| respectively. |len_size| is the
 * number of bytes in length of name/value pair and it must be 2 or
 * 4. We use folloing data structure in |*buflen_ptr| size.  First
 * part of the data is array of pointer to name/value pair.  Supporse
 * the buf pointer points to the data region and N is the number of
 * name/value pair.  First (N*2+1)*sizeof(char*) bytes contain array
 * of pointer to name/value pair and terminating NULL.  Each pointer
 * to name/value pair points to the string in remaining data.  For
 * each name/value pair, the name is copied to the remaining data with
 * terminating NULL character. The value is also copied to the
 * position after the data with terminating NULL character. The
 * corresponding index is assigned to these pointers. If the value
 * contains multiple values (delimited by single NULL), for each such
 * data, corresponding index is assigned to name/value pointers. In
 * this case, the name string is reused.
 *
 * With the above stragety, |*buflen_ptr| is calculated as
 * (N*2+1)*sizeof(char*)+sum(strlen(name)+1+strlen(value)+1){for each
 * name/value pair}.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * SPDYLAY_ERR_INVALID_FRAME
 *     The input data are invalid.
 */
int spdylay_frame_count_unpack_nv_space(size_t *nvlen_ptr, size_t *buflen_ptr,
                                        spdylay_buffer *in, size_t len_size);

/*
 * Unpacks name/value header block in wire format |in| and stores them
 * in |*nv_ptr|.  Thif function allocates enough memory to store
 * name/value pairs in |*nv_ptr|.  |len_size| is the number of bytes
 * in length of name/value pair and it must be 2 or 4.
 *
 * This function also validates the name/value pairs. If unpacking
 * succeeds but validation fails, it is indicated by returning
 * SPDYLAY_ERR_INVALID_HEADER_BLOCK.
 *
 * If error other than SPDYLAY_ERR_INVALID_HEADER_BLOCK is returned,
 * the |nv_ptr| is not assigned. In other words,
 * SPDYLAY_ERR_INVALID_HEADER_BLOCK means unpacking succeeded, but
 * header block validation failed.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * SPDYLAY_ERR_INVALID_HEADER_BLOCK
 *     Unpacking succeeds but the header block is invalid. The
 *     possible reasons are: There are duplicate header names; or the
 *     header names are not encoded in US-ASCII character set and not
 *     lower cased; or the header name is zero-length string.
 * SPDYLAY_ERR_NOMEM
 *     Out of memory.
 */
int spdylay_frame_unpack_nv(char ***nv_ptr, spdylay_buffer *in,
                            size_t len_size);

/*
 * Initializes SYN_STREAM frame |frame| with given values.  |frame|
 * takes ownership of |nv|, so caller must not free it. If stream_id
 * is not assigned yet, it must be 0.
 */
void spdylay_frame_syn_stream_init(spdylay_syn_stream *frame,
                                   uint16_t version, uint8_t flags,
                                   int32_t stream_id, int32_t assoc_stream_id,
                                   uint8_t pri, char **nv);

void spdylay_frame_syn_stream_free(spdylay_syn_stream *frame);

void spdylay_frame_syn_reply_init(spdylay_syn_reply *frame,
                                  uint16_t version, uint8_t flags,
                                  int32_t stream_id, char **nv);

void spdylay_frame_syn_reply_free(spdylay_syn_reply *frame);

void spdylay_frame_ping_init(spdylay_ping *frame, uint16_t version,
                             uint32_t unique_id);

void spdylay_frame_ping_free(spdylay_ping *frame);

/*
 * Initializes GOAWAY frame |frame| with given values.  The
 * |status_code| is ignored if |version| == SPDYLAY_PROTO_SPDY2.
 */
void spdylay_frame_goaway_init(spdylay_goaway *frame, uint16_t version,
                               int32_t last_good_stream_id,
                               uint32_t status_code);

void spdylay_frame_goaway_free(spdylay_goaway *frame);

void spdylay_frame_headers_init(spdylay_headers *frame, uint16_t version,
                                uint8_t flags,
                                int32_t stream_id, char **nv);

void spdylay_frame_headers_free(spdylay_headers *frame);

void spdylay_frame_rst_stream_init(spdylay_rst_stream *frame,
                                   uint16_t version,
                                   int32_t stream_id, uint32_t status_code);

void spdylay_frame_rst_stream_free(spdylay_rst_stream *frame);

void spdylay_frame_window_update_init(spdylay_window_update *frame,
                                      uint16_t version,
                                      int32_t stream_id,
                                      int32_t delta_window_size);

void spdylay_frame_window_update_free(spdylay_window_update *frame);

/*
 * Initializes SETTINGS frame |frame| with given values. |frame| takes
 * ownership of |iv|, so caller must not free it. The |flags| are
 * bitwise-OR of one or more of spdylay_settings_flag.
 */
void spdylay_frame_settings_init(spdylay_settings *frame,
                                 uint16_t version, uint8_t flags,
                                 spdylay_settings_entry *iv, size_t niv);

void spdylay_frame_settings_free(spdylay_settings *frame);

/*
 * Initializes CREDENTIAL frame |frame| with given values.  This
 * function takes ownership of |proof->data| and |certs| on success.
 * Note that the ownership of |proof| is not taken.
 */
void spdylay_frame_credential_init(spdylay_credential *frame,
                                   uint16_t version, uint16_t slot,
                                   spdylay_mem_chunk *proof,
                                   spdylay_mem_chunk *certs,
                                   size_t ncerts);

void spdylay_frame_credential_free(spdylay_credential *frame);

void spdylay_frame_data_init(spdylay_data *frame, int32_t stream_id,
                             uint8_t flags,
                             const spdylay_data_provider *data_prd);

void spdylay_frame_data_free(spdylay_data *frame);

/*
 * Returns 1 if the first byte of this frame indicates it is a control
 * frame.
 */
int spdylay_frame_is_ctrl_frame(uint8_t first_byte);

/*
 * Deallocates memory of name/value pair |nv|.
 */
void spdylay_frame_nv_del(char **nv);

/*
 * Makes a deep copy of |nv| and returns the copy.  This function
 * returns the pointer to the copy if it succeeds, or NULL.  To free
 * allocated memory, use spdylay_frame_nv_del().
 */
char** spdylay_frame_nv_copy(const char **nv);

/*
 * Sorts |nv| in the ascending order of name.
 */
void spdylay_frame_nv_sort(char **nv);

/*
 * Makes names in |nv| lower cased.
 */
void spdylay_frame_nv_downcase(char **nv);

/*
 * This function first makes a copy of |nv| using
 * spdylay_frame_nv_copy().  If it succeeds, then call
 * spdylay_frame_nv_downcase() and spdylay_frame_nv_sort() with the
 * copied name/value pairs.
 *
 * This function returns the copied name/value pairs if it succeeds,
 * or NULL.
 */
char** spdylay_frame_nv_norm_copy(const char **nv);

/*
 * Translates the |nv| in SPDY/3 header names into SPDY/2.
 */
void spdylay_frame_nv_3to2(char **nv);

/*
 * Translates the |nv| in SPDY/2 header names into SPDY/3.
 */
void spdylay_frame_nv_2to3(char **nv);

/*
 * Assigns the members of the |origin| using ":scheme" and ":host"
 * values in |nv|.
 *
 * If ":host" value contains ':', this function parses the chracters
 * after ':' as integer and uses it as port number.
 *
 * If ':' is missing in :host value, the default port number is used.
 * The only defined default port number is 443.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error code:
 *
 * SPDYLAY_ERR_INVALID_ARGUMENT
 *     The |nv| lacks either :scheme or :host, or both.
 */
int spdylay_frame_nv_set_origin(char **nv, spdylay_origin *origin);

/*
 * Makes copy of |iv| and return the copy. The |niv| is the number of
 * entries in |iv|. This function returns the pointer to the copy if
 * it succeeds, or NULL.
 */
spdylay_settings_entry* spdylay_frame_iv_copy(const spdylay_settings_entry *iv,
                                              size_t niv);

/*
 * Sorts the |iv| with the ascending order of the settings_id member.
 * The number of the element in the array pointed by the |iv| is given
 * by the |niv|.
 */
void spdylay_frame_iv_sort(spdylay_settings_entry *iv, size_t niv);

/*
 * Returns the offset of the name/header block in the frame, including
 * frame header. If |type| is neither SPDYLAY_SYN_STREAM,
 * SPDYLAY_SYN_REPLY nor SPDYLAY_HEADERS, this function returns -1.
 * If |version| is unknown, this function returns -1.
 */
ssize_t spdylay_frame_nv_offset(spdylay_frame_type type, uint16_t version);

/*
 * Checks names are not empty string and values are not NULL.
 *
 * This function returns nonzero if it succeeds, or 0.
 */
int spdylay_frame_nv_check_null(const char **nv);

#endif /* SPDYLAY_FRAME_H */
