#include "encrypt.h"

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

inline int random_compare(unsigned char x, unsigned char y, unsigned int i, unsigned long long a) {
    return (a % (x + i) - a % (y + i));
}

void get_table(const unsigned char* key) {
    unsigned char *table = encrypt_table;
    unsigned char *tmp_hash;
    tmp_hash = MD5((const unsigned char*)key, strlen((const char*)key), NULL);
    unsigned long long a;
    a = *(unsigned long long *)tmp_hash;
    unsigned int i;

    for(i = 0; i < 256; ++i) {
        table[i] = i;
    }
    for(i = 1; i < 1024; ++i) {
        // use bubble sort in order to keep the array stable as in Python
        int k,j;
        unsigned char t;
        for(k = 256 - 2; k >= 0; --k)
        {
            for(j = 0;j <= k; ++j)
            {
                if(random_compare(table[j], table[j + 1], i, a) > 0)
                {
                    t=table[j];
                    table[j]=table[j + 1];
                    table[j + 1]=t;
                }
            }
        }
    }
    for(i = 0; i < 256; ++i) {
        // gen decrypt table from encrypt table
        decrypt_table[encrypt_table[i]] = i;
    }
}
