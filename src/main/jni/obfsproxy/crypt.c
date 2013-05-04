/* Copyright 2011 Nick Mathewson, George Kadianakis
   See LICENSE for other credits and copying information
*/

/**
 * \file crypt.c
 * \headerfile crypt.h
 * \brief Cryptographic functions.
 *
 * \details Most functions here are wrappers of OpenSSL functions.
 **/

#include "util.h"

#define CRYPT_PRIVATE
#include "crypt.h"

#include <fcntl.h>
#include <unistd.h>

#include <openssl/opensslv.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#if OPENSSL_VERSION_NUMBER >= 0x0090800f
#define USE_OPENSSL_RANDPOLL 1
#define USE_OPENSSL_SHA256 1
#include <openssl/sha.h>
#else
#include "sha256.h"
#endif

/**
   Initializes the crypto subsystem.
*/
int
initialize_crypto(void)
{
  ERR_load_crypto_strings();

#ifdef USE_OPENSSL_RANDPOLL
  return RAND_poll() == 1 ? 0 : -1;
#else
  /* XXX Or maybe fall back to the arc4random implementation in libevent2? */
  {
    char buf[32];
    int fd, n;
    fd = open("/dev/urandom", O_RDONLY);
    if (fd == -1) {
      perror("open");
      return -1;
    }
    n = read(fd, buf, sizeof(buf));
    if (n != sizeof(buf)) {
      close(fd);
      return -1;
    }
    RAND_seed(buf, sizeof(buf));
    close(fd);
    return 0;
  }
#endif
}

/**
   Cleans up the crypto subsystem.
*/
void
cleanup_crypto(void)
{
  ERR_free_strings();
}

/* =====
   Digests
   ===== */

#ifndef USE_OPENSSL_SHA256
#define SHA256_CTX sha256_state
#define SHA256_Init(ctx) sha256_init(ctx)
#define SHA256_Update(ctx, buf, len) sha256_process(ctx, buf, len)
#define SHA256_Final(buf, ctx) sha256_done(ctx, buf)
#endif

struct digest_t {
  SHA256_CTX ctx;
};

/**
   Returns a new SHA256 digest container.
*/
digest_t *
digest_new(void)
{
  digest_t *d = xmalloc(sizeof(digest_t));
  SHA256_Init(&d->ctx);
  return d;
}

/**
   Updates the contents of the SHA256 container 'd' with the first
   'len' bytes of 'buf'.
*/
void
digest_update(digest_t *d, const uchar *buf, size_t len)
{
  SHA256_Update(&d->ctx, buf, len);
}

/**
   Returns the digest stored in 'd' into 'buf' of length 'len'.
*/
size_t
digest_getdigest(digest_t *d, uchar *buf, size_t len)
{
  if (len >= SHA256_LENGTH) {
    SHA256_Final(buf, &d->ctx);
    return SHA256_LENGTH;
  } else {
    uchar tmp[SHA256_LENGTH];
    SHA256_Final(tmp, &d->ctx);
    memcpy(buf, tmp, len);
    memset(tmp, 0, SHA256_LENGTH);
    return len;
  }
}

void
digest_free(digest_t *d)
{
  memset(d, 0, sizeof(digest_t));
  free(d);
}

/* =====
   Stream crypto
   ===== */

/**
   Initializes the AES cipher with 'key'.
*/
crypt_t *
crypt_new(const uchar *key, size_t keylen)
{
  crypt_t *k;

  obfs_assert(keylen == AES_BLOCK_SIZE);
  k = xzalloc(sizeof(crypt_t));
  AES_set_encrypt_key(key, AES_BLOCK_SIZE * CHAR_BIT, &k->key);

  return k;
}

/**
   Sets the IV of 'key' to 'iv'.
*/
void
crypt_set_iv(crypt_t *key, const uchar *iv, size_t ivlen)
{
  obfs_assert(ivlen == sizeof(key->ivec));
  memcpy(key->ivec, iv, ivlen);
}

/*
  In-place encrypts 'buf' with 'key'.
*/
void
stream_crypt(crypt_t *key, uchar *buf, size_t len)
{
  AES_ctr128_encrypt(buf, buf, len,
                     &key->key, key->ivec, key->ecount_buf,
                     &key->pos);
}

/**
   Deallocates memory space of 'key'.
*/
void
crypt_free(crypt_t *key)
{
  memset(key, 0, sizeof(crypt_t));
  free(key);
}

/* =====
   PRNG
   ===== */

/**
   Fills 'buf' with 'buflen' random bytes and returns 0 on success.
   Returns -1 on failure.
*/
int
random_bytes(uchar *buf, size_t buflen)
{
  return RAND_bytes(buf, buflen) == 1 ? 0 : -1;
}


/** Return a pseudorandom integer, chosen uniformly from the values
 * between 0 and <b>max</b>-1 inclusive.  <b>max</b> must be between 1 and
 * INT_MAX+1, inclusive. */
int
random_int(unsigned int max)
{
  unsigned int val;
  unsigned int cutoff;
  obfs_assert(max <= ((unsigned int)INT_MAX)+1);
  obfs_assert(max > 0); /* don't div by 0 */

  /* We ignore any values that are >= 'cutoff,' to avoid biasing the
   * distribution with clipping at the upper end of unsigned int's
   * range.
   */
  cutoff = UINT_MAX - (UINT_MAX%max);
  while (1) {
    random_bytes((uchar*)&val, sizeof(val));
    if (val < cutoff)
      return val % max;
  }
}
