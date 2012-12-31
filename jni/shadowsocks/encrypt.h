#pragma once

#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <openssl/md5.h>
#include <openssl/rc4.h>
#include <endian.h>

#define BUF_SIZE 4096

#define TABLE_ENC 0
#define RC4_ENC   1

unsigned char encrypt_table[256];
unsigned char decrypt_table[256];
RC4_KEY rc4_key;

void get_table(const char* key);
void encrypt(char *buf, int len);
void decrypt(char *buf, int len);
int send_encrypt(int sock, char *buf, int len, int flags);
int recv_decrypt(int sock, char *buf, int len, int flags);

unsigned int _i;
unsigned long long _a;
int _method;

