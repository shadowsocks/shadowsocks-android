/*
 * libipq.h
 *
 * IPQ library for userspace.
 *
 * Author: James Morris <jmorris@intercode.com.au>
 *
 * Copyright (c) 2000-2001 Netfilter Core Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef _LIBIPQ_H
#define _LIBIPQ_H

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <asm/types.h>
#include <linux/netlink.h>

#include <linux/netfilter_ipv4/ip_queue.h>
typedef unsigned long ipq_id_t;

#ifdef DEBUG_LIBIPQ
#include <stdio.h>
#define LDEBUG(x...) fprintf(stderr, ## x)
#else
#define LDEBUG(x...)
#endif	/* DEBUG_LIBIPQ */

/* FIXME: glibc sucks */
#ifndef MSG_TRUNC
#define MSG_TRUNC 0x20
#endif

struct ipq_handle
{
	int fd;
	u_int8_t blocking;
	struct sockaddr_nl local;
	struct sockaddr_nl peer;
};

struct ipq_handle *ipq_create_handle(u_int32_t flags, u_int32_t protocol);

int ipq_destroy_handle(struct ipq_handle *h);

ssize_t ipq_read(const struct ipq_handle *h,
                unsigned char *buf, size_t len, int timeout);

int ipq_set_mode(const struct ipq_handle *h, u_int8_t mode, size_t len);

ipq_packet_msg_t *ipq_get_packet(const unsigned char *buf);

int ipq_message_type(const unsigned char *buf);

int ipq_get_msgerr(const unsigned char *buf);

int ipq_set_verdict(const struct ipq_handle *h,
                    ipq_id_t id,
                    unsigned int verdict,
                    size_t data_len,
                    unsigned char *buf);

int ipq_ctl(const struct ipq_handle *h, int request, ...);

char *ipq_errstr(void);
void ipq_perror(const char *s);

#endif	/* _LIBIPQ_H */

