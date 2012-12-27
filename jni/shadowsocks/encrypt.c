#include "encrypt.h"
#include "android.h"

#include <openssl/md5.h>

typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

static int random_compare(const void *_x, const void *_y) {
    uint32_t i = _i;
    uint64_t a = _a;
    uint8_t x = *((uint8_t *) _x);
    uint8_t y = *((uint8_t*) _y);
    return (a % (x + i) - a % (y + i));
}


static void merge(uint8_t *left, int llength, uint8_t *right, int rlength)
{
	/* Temporary memory locations for the 2 segments of the array to merge. */
	uint8_t *ltmp = (uint8_t *) malloc(llength * sizeof(uint8_t));
	uint8_t *rtmp = (uint8_t *) malloc(rlength * sizeof(uint8_t));

	/*
	 * Pointers to the elements being sorted in the temporary memory locations.
	 */
	uint8_t *ll = ltmp;
	uint8_t *rr = rtmp;

	uint8_t *result = left;

	/*
	 * Copy the segment of the array to be merged into the temporary memory
	 * locations.
	 */
	memcpy(ltmp, left, llength * sizeof(uint8_t));
	memcpy(rtmp, right, rlength * sizeof(uint8_t));

	while (llength > 0 && rlength > 0) {
		if (random_compare(ll, rr) <= 0) {
			/*
			 * Merge the first element from the left back into the main array
			 * if it is smaller or equal to the one on the right.
			 */
			*result = *ll;
			++ll;
			--llength;
		} else {
			/*
			 * Merge the first element from the right back into the main array
			 * if it is smaller than the one on the left.
			 */
			*result = *rr;
			++rr;
			--rlength;
		}
		++result;
	}
	/*
	 * All the elements from either the left or the right temporary array
	 * segment have been merged back into the main array.  Append the remaining
	 * elements from the other temporary array back into the main array.
	 */
	if (llength > 0)
		while (llength > 0) {
			/* Appending the rest of the left temporary array. */
			*result = *ll;
			++result;
			++ll;
			--llength;
		}
	else
		while (rlength > 0) {
			/* Appending the rest of the right temporary array. */
			*result = *rr;
			++result;
			++rr;
			--rlength;
		}

	/* Release the memory used for the temporary arrays. */
	free(ltmp);
	free(rtmp);
}

static void mergesort(uint8_t array[], int length)
{
	/* This is the middle index and also the length of the right array. */
	uint8_t middle;

	/*
	 * Pointers to the beginning of the left and right segment of the array
	 * to be merged.
	 */
	uint8_t *left, *right;

	/* Length of the left segment of the array to be merged. */
	int llength;

	if (length <= 1)
		return;

	/* Let integer division truncate the value. */
	middle = length / 2;

	llength = length - middle;

	/*
	 * Set the pointers to the appropriate segments of the array to be merged.
	 */
	left = array;
	right = array + llength;

	mergesort(left, llength);
	mergesort(right, middle);
	merge(left, llength, right, middle);
}

void encrypt(char *buf, int len) {
    char *end = buf + len;
    while (buf < end) {
        *buf = (char)encrypt_table[(uint8_t)*buf];
        buf++;
    }
}

void decrypt(char *buf, int len) {
    char *end = buf + len;
    while (buf < end) {
        *buf = (char)decrypt_table[(uint8_t)*buf];
        buf++;
    }
}

int send_encrypt(int sock, char *buf, int len, int flags) {
    char mybuf[4096];
    memcpy(mybuf, buf, len);
    encrypt(mybuf, len);
    return send(sock, mybuf, len, flags);
}

int recv_decrypt(int sock, char *buf, int len, int flags) {
    char mybuf[4096];
    int result = recv(sock, mybuf, len, flags);
    memcpy(buf, mybuf, len);
    decrypt(buf, len);
    return result;
}

void get_table(const char* key) {
    uint8_t *table = encrypt_table;
    uint8_t *tmp_hash;
    tmp_hash = MD5((const uint8_t*)key, strlen(key), NULL);
    _a = *(uint64_t *)tmp_hash;
    uint32_t i;

    for(i = 0; i < 256; ++i) {
        table[i] = i;
    }
    for(i = 1; i < 1024; ++i) {
        _i = i;
        mergesort(table, 256);
    }
    for(i = 0; i < 256; ++i) {
        // gen decrypt table from encrypt table
        decrypt_table[encrypt_table[i]] = i;
    }
}
