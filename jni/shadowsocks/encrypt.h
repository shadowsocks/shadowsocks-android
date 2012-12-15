#pragma once

#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

unsigned char encrypt_table[256];
unsigned char decrypt_table[256];
void get_table(const unsigned char* key);
void encrypt(char *buf, int len);
void decrypt(char *buf, int len);
int send_encrypt(int sock, char *buf, int len, int flags);
int recv_decrypt(int sock, char *buf, int len, int flags);

