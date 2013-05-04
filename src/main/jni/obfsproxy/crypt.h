/* Copyright 2011 Nick Mathewson, George Kadianakis
   See LICENSE for other credits and copying information
*/

/**
 * \file crypt.h
 * \brief Headers for crypt.c.
 **/

#ifndef CRYPT_H
#define CRYPT_H

#define SHA256_LENGTH 32

/* Stream cipher state */
typedef struct crypt_t crypt_t;
/* Digest state */
typedef struct digest_t digest_t;

typedef unsigned char uchar;

/** Initialize global crypto state.  Returrn 0 on success, -1 on failure */
int initialize_crypto(void);
/** Clean up global crypto state */
void cleanup_crypto(void);

/** Return a newly allocated digest state; cannot fail. */
digest_t *digest_new(void);
/** Add n bytes from b to the digest state. */
void digest_update(digest_t *, const uchar *b, size_t n);
/** Get a digest from the digest state.  Put it in up the first n bytes of the
buffer b.  Return the number of bytes actually written.*/
size_t digest_getdigest(digest_t *, uchar *b, size_t n);
/** Clear and free a digest state */
void digest_free(digest_t *);

/** Return a new stream cipher state using 'key' as the symmetric key.
 * The data length must be exactly 16 bytes. Cannot fail. */
crypt_t *crypt_new(const uchar *key, size_t);
/* Set the IV of a stream-cipher state.  Cannot fail. */
void crypt_set_iv(crypt_t *, const uchar *iv, size_t ivlen);

/** Encrypt n bytes of data in the buffer b, in place. */
void stream_crypt(crypt_t *, uchar *b, size_t n);
/** Clear and free a stream cipher state. */
void crypt_free(crypt_t *);

/** Set b to contain n random bytes. */
int random_bytes(uchar *b, size_t n);

/** Return a random integer in the range [0, max).
 * 'max' must be between 1 and INT_MAX+1, inclusive.
 */
int random_int(unsigned int max);

#ifdef CRYPT_PRIVATE

#include <openssl/aes.h>

/* ==========
   These definitions are not part of the crypt interface.
   They're exposed here so that the unit tests can use them.
   ==========
*/
struct crypt_t {
  AES_KEY key;
  uchar ivec[AES_BLOCK_SIZE];
  uchar ecount_buf[AES_BLOCK_SIZE];
  unsigned int pos;
};
#endif

#endif
