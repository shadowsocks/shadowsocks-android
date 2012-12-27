#include "encrypt.h"
#include "android.h"

#include <openssl/md5.h>

void encrypt(char *buf, int len) {
    char *end = buf + len;
    while (buf < end) {
        *buf = (char)encrypt_table[(unsigned char)*buf];
        buf++;
    }
}

void decrypt(char *buf, int len) {
    char *end = buf + len;
    while (buf < end) {
        *buf = (char)decrypt_table[(unsigned char)*buf];
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

static int random_compare(const void *_x, const void *_y) {
    unsigned int i = _i;
    unsigned long long a = _a;
    unsigned char x = *((unsigned char*) _x);
    unsigned char y = *((unsigned char*) _y);
    return (a % (x + i) - a % (y + i));
}

void get_table(const char* key) {
    unsigned char *table = encrypt_table;
    unsigned char *tmp_hash;
    tmp_hash = MD5((const unsigned char*)key, strlen(key), NULL);
    _a = *(unsigned long long *)tmp_hash;
    unsigned int i;

    for(i = 0; i < 256; ++i) {
        table[i] = i;
    }
    for(i = 1; i < 1024; ++i) {
        _i = i;
        qsort(table, 256, sizeof(unsigned char), random_compare);
    }
    for(i = 0; i < 256; ++i) {
        // gen decrypt table from encrypt table
        decrypt_table[encrypt_table[i]] = i;
    }
}
