#include "encrypt.h"
#include "android.h"

#define OFFSET_ROL(p, o) ((u_int64_t)(*(p + o)) << (8 * o))

static int random_compare(const void *_x, const void *_y) {
    uint32_t i = _i;
    uint64_t a = _a;
    uint8_t x = *((uint8_t *) _x);
    uint8_t y = *((uint8_t*) _y);
    return (a % (x + i) - a % (y + i));
}

static void md5(const unsigned char *text, unsigned char *digest) {
    md5_state_t state;
    md5_init(&state);
    md5_append(&state, text, strlen((char*)text));
    md5_finish(&state, digest);
}

static void merge(uint8_t *left, int llength, uint8_t *right, int rlength) {
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

static void merge_sort(uint8_t array[], int length) {
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

	merge_sort(left, llength);
	merge_sort(right, middle);
	merge(left, llength, right, middle);
}

void encrypt(char *buf, int len, struct rc4_state *ctx) {
    if (ctx != NULL) {
        rc4_crypt(ctx, (unsigned char*) buf, (unsigned char*) buf, len);
    } else {
        char *end = buf + len;
        while (buf < end) {
            *buf = (char)enc_ctx.table.encrypt_table[(uint8_t)*buf];
            buf++;
        }
    }
}

void decrypt(char *buf, int len, struct rc4_state *ctx) {
    if (ctx != NULL) {
        rc4_crypt(ctx, (unsigned char*) buf, (unsigned char*) buf, len);
    } else {
        char *end = buf + len;
        while (buf < end) {
            *buf = (char)enc_ctx.table.decrypt_table[(uint8_t)*buf];
            buf++;
        }
    }
}

void enc_ctx_init(struct rc4_state *ctx, int enc) {
    uint8_t *key = enc_ctx.rc4.key;
    int key_len = enc_ctx.rc4.key_len;
    rc4_init(ctx, key, key_len);
}

void enc_key_init(const char *pass) {
    enc_ctx.rc4.key_len = 16;
    enc_ctx.rc4.key = malloc(16);
    md5((const unsigned char*)pass, enc_ctx.rc4.key);
}

void get_table(const char *pass) {
    uint8_t *enc_table = malloc(256);
    uint8_t *dec_table = malloc(256);
    uint8_t digest[16];
    uint32_t i;

    md5((const unsigned char*)pass, digest);

    for (i = 0; i < 8; i++) {
        _a += OFFSET_ROL(digest, i);
    }

    for(i = 0; i < 256; ++i) {
        enc_table[i] = i;
    }
    for(i = 1; i < 1024; ++i) {
        _i = i;
        merge_sort(enc_table, 256);
    }
    for(i = 0; i < 256; ++i) {
        // gen decrypt table from encrypt table
        dec_table[enc_table[i]] = i;
    }

    enc_ctx.table.encrypt_table = enc_table;
    enc_ctx.table.decrypt_table = dec_table;
}
