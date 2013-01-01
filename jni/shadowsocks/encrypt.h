#pragma once

#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <openssl/md5.h>
#include <openssl/evp.h>
#include <endian.h>

#define BUF_SIZE 4096

#define TABLE 0
#define RC4   1

unsigned char encrypt_table[256];
unsigned char decrypt_table[256];

void get_table(const char* key);
void encrypt(char *buf, int len, EVP_CIPHER_CTX *ctx);
void decrypt(char *buf, int len, EVP_CIPHER_CTX *ctx);
void enc_ctx_init(EVP_CIPHER_CTX *ctx, const char *pass, int enc);

unsigned int _i;
unsigned long long _a;
int _method;

