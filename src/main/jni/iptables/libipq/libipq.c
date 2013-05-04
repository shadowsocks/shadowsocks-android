/*
 * libipq.c
 *
 * IPQ userspace library.
 *
 * Please note that this library is still developmental, and there may
 * be some API changes.
 *
 * Author: James Morris <jmorris@intercode.com.au>
 *
 * 07-11-2001 Modified by Fernando Anton to add support for IPv6.
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>

#include <libipq/libipq.h>
#include <netinet/in.h>
#include <linux/netfilter.h>

/****************************************************************************
 *
 * Private interface
 *
 ****************************************************************************/

enum {
	IPQ_ERR_NONE = 0,
	IPQ_ERR_IMPL,
	IPQ_ERR_HANDLE,
	IPQ_ERR_SOCKET,
	IPQ_ERR_BIND,
	IPQ_ERR_BUFFER,
	IPQ_ERR_RECV,
	IPQ_ERR_NLEOF,
	IPQ_ERR_ADDRLEN,
	IPQ_ERR_STRUNC,
	IPQ_ERR_RTRUNC,
	IPQ_ERR_NLRECV,
	IPQ_ERR_SEND,
	IPQ_ERR_SUPP,
	IPQ_ERR_RECVBUF,
	IPQ_ERR_TIMEOUT,
        IPQ_ERR_PROTOCOL
};
#define IPQ_MAXERR IPQ_ERR_PROTOCOL

struct ipq_errmap_t {
	int errcode;
	char *message;
} ipq_errmap[] = {
	{ IPQ_ERR_NONE, "Unknown error" },
	{ IPQ_ERR_IMPL, "Implementation error" },
	{ IPQ_ERR_HANDLE, "Unable to create netlink handle" },
	{ IPQ_ERR_SOCKET, "Unable to create netlink socket" },
	{ IPQ_ERR_BIND, "Unable to bind netlink socket" },
	{ IPQ_ERR_BUFFER, "Unable to allocate buffer" },
	{ IPQ_ERR_RECV, "Failed to receive netlink message" },
	{ IPQ_ERR_NLEOF, "Received EOF on netlink socket" },
	{ IPQ_ERR_ADDRLEN, "Invalid peer address length" },
	{ IPQ_ERR_STRUNC, "Sent message truncated" },
	{ IPQ_ERR_RTRUNC, "Received message truncated" },
	{ IPQ_ERR_NLRECV, "Received error from netlink" },
	{ IPQ_ERR_SEND, "Failed to send netlink message" },
	{ IPQ_ERR_SUPP, "Operation not supported" },
	{ IPQ_ERR_RECVBUF, "Receive buffer size invalid" },
	{ IPQ_ERR_TIMEOUT, "Timeout"},
	{ IPQ_ERR_PROTOCOL, "Invalid protocol specified" }
};

static int ipq_errno = IPQ_ERR_NONE;

static ssize_t ipq_netlink_sendto(const struct ipq_handle *h,
                                  const void *msg, size_t len);

static ssize_t ipq_netlink_recvfrom(const struct ipq_handle *h,
                                    unsigned char *buf, size_t len,
                                    int timeout);

static ssize_t ipq_netlink_sendmsg(const struct ipq_handle *h,
                                   const struct msghdr *msg,
                                   unsigned int flags);

static char *ipq_strerror(int errcode);

static ssize_t ipq_netlink_sendto(const struct ipq_handle *h,
                                  const void *msg, size_t len)
{
	int status = sendto(h->fd, msg, len, 0,
	                    (struct sockaddr *)&h->peer, sizeof(h->peer));
	if (status < 0)
		ipq_errno = IPQ_ERR_SEND;
	return status;
}

static ssize_t ipq_netlink_sendmsg(const struct ipq_handle *h,
                                   const struct msghdr *msg,
                                   unsigned int flags)
{
	int status = sendmsg(h->fd, msg, flags);
	if (status < 0)
		ipq_errno = IPQ_ERR_SEND;
	return status;
}

static ssize_t ipq_netlink_recvfrom(const struct ipq_handle *h,
                                    unsigned char *buf, size_t len,
                                    int timeout)
{
	unsigned int addrlen;
	int status;
	struct nlmsghdr *nlh;

	if (len < sizeof(struct nlmsgerr)) {
		ipq_errno = IPQ_ERR_RECVBUF;
		return -1;
	}
	addrlen = sizeof(h->peer);

	if (timeout != 0) {
		int ret;
		struct timeval tv;
		fd_set read_fds;
		
		if (timeout < 0) {
			/* non-block non-timeout */
			tv.tv_sec = 0;
			tv.tv_usec = 0;
		} else {
			tv.tv_sec = timeout / 1000000;
			tv.tv_usec = timeout % 1000000;
		}

		FD_ZERO(&read_fds);
		FD_SET(h->fd, &read_fds);
		ret = select(h->fd+1, &read_fds, NULL, NULL, &tv);
		if (ret < 0) {
			if (errno == EINTR) {
				return 0;
			} else {
				ipq_errno = IPQ_ERR_RECV;
				return -1;
			}
		}
		if (!FD_ISSET(h->fd, &read_fds)) {
			ipq_errno = IPQ_ERR_TIMEOUT;
			return 0;
		}
	}
	status = recvfrom(h->fd, buf, len, 0,
	                      (struct sockaddr *)&h->peer, &addrlen);
	if (status < 0) {
		ipq_errno = IPQ_ERR_RECV;
		return status;
	}
	if (addrlen != sizeof(h->peer)) {
		ipq_errno = IPQ_ERR_RECV;
		return -1;
	}
	if (h->peer.nl_pid != 0) {
		ipq_errno = IPQ_ERR_RECV;
		return -1;
	}
	if (status == 0) {
		ipq_errno = IPQ_ERR_NLEOF;
		return -1;
	}
	nlh = (struct nlmsghdr *)buf;
	if (nlh->nlmsg_flags & MSG_TRUNC || nlh->nlmsg_len > status) {
		ipq_errno = IPQ_ERR_RTRUNC;
		return -1;
	}
	return status;
}

static char *ipq_strerror(int errcode)
{
	if (errcode < 0 || errcode > IPQ_MAXERR)
		errcode = IPQ_ERR_IMPL;
	return ipq_errmap[errcode].message;
}

/****************************************************************************
 *
 * Public interface
 *
 ****************************************************************************/

/*
 * Create and initialise an ipq handle.
 */
struct ipq_handle *ipq_create_handle(uint32_t flags, uint32_t protocol)
{
	int status;
	struct ipq_handle *h;

	h = (struct ipq_handle *)malloc(sizeof(struct ipq_handle));
	if (h == NULL) {
		ipq_errno = IPQ_ERR_HANDLE;
		return NULL;
	}
	
	memset(h, 0, sizeof(struct ipq_handle));
	
        if (protocol == NFPROTO_IPV4)
                h->fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_FIREWALL);
        else if (protocol == NFPROTO_IPV6)
                h->fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_IP6_FW);
        else {
		ipq_errno = IPQ_ERR_PROTOCOL;
		free(h);
		return NULL;
        }
        
	if (h->fd == -1) {
		ipq_errno = IPQ_ERR_SOCKET;
		close(h->fd);
		free(h);
		return NULL;
	}
	memset(&h->local, 0, sizeof(struct sockaddr_nl));
	h->local.nl_family = AF_NETLINK;
	h->local.nl_pid = getpid();
	h->local.nl_groups = 0;
	status = bind(h->fd, (struct sockaddr *)&h->local, sizeof(h->local));
	if (status == -1) {
		ipq_errno = IPQ_ERR_BIND;
		close(h->fd);
		free(h);
		return NULL;
	}
	memset(&h->peer, 0, sizeof(struct sockaddr_nl));
	h->peer.nl_family = AF_NETLINK;
	h->peer.nl_pid = 0;
	h->peer.nl_groups = 0;
	return h;
}

/*
 * No error condition is checked here at this stage, but it may happen
 * if/when reliable messaging is implemented.
 */
int ipq_destroy_handle(struct ipq_handle *h)
{
	if (h) {
		close(h->fd);
		free(h);
	}
	return 0;
}

int ipq_set_mode(const struct ipq_handle *h,
                 uint8_t mode, size_t range)
{
	struct {
		struct nlmsghdr nlh;
		ipq_peer_msg_t pm;
	} req;

	memset(&req, 0, sizeof(req));
	req.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(req));
	req.nlh.nlmsg_flags = NLM_F_REQUEST;
	req.nlh.nlmsg_type = IPQM_MODE;
	req.nlh.nlmsg_pid = h->local.nl_pid;
	req.pm.msg.mode.value = mode;
	req.pm.msg.mode.range = range;
	return ipq_netlink_sendto(h, (void *)&req, req.nlh.nlmsg_len);
}

/*
 * timeout is in microseconds (1 second is 1000000 (1 million) microseconds)
 *
 */
ssize_t ipq_read(const struct ipq_handle *h,
                 unsigned char *buf, size_t len, int timeout)
{
	return ipq_netlink_recvfrom(h, buf, len, timeout);
}

int ipq_message_type(const unsigned char *buf)
{
	return ((struct nlmsghdr*)buf)->nlmsg_type;
}

int ipq_get_msgerr(const unsigned char *buf)
{
	struct nlmsghdr *h = (struct nlmsghdr *)buf;
	struct nlmsgerr *err = (struct nlmsgerr*)NLMSG_DATA(h);
	return -err->error;
}

ipq_packet_msg_t *ipq_get_packet(const unsigned char *buf)
{
	return NLMSG_DATA((struct nlmsghdr *)(buf));
}

int ipq_set_verdict(const struct ipq_handle *h,
                    ipq_id_t id,
                    unsigned int verdict,
                    size_t data_len,
                    unsigned char *buf)
{
	unsigned char nvecs;
	size_t tlen;
	struct nlmsghdr nlh;
	ipq_peer_msg_t pm;
	struct iovec iov[3];
	struct msghdr msg;

	memset(&nlh, 0, sizeof(nlh));
	nlh.nlmsg_flags = NLM_F_REQUEST;
	nlh.nlmsg_type = IPQM_VERDICT;
	nlh.nlmsg_pid = h->local.nl_pid;
	memset(&pm, 0, sizeof(pm));
	pm.msg.verdict.value = verdict;
	pm.msg.verdict.id = id;
	pm.msg.verdict.data_len = data_len;
	iov[0].iov_base = &nlh;
	iov[0].iov_len = sizeof(nlh);
	iov[1].iov_base = &pm;
	iov[1].iov_len = sizeof(pm);
	tlen = sizeof(nlh) + sizeof(pm);
	nvecs = 2;
	if (data_len && buf) {
		iov[2].iov_base = buf;
		iov[2].iov_len = data_len;
		tlen += data_len;
		nvecs++;
	}
	msg.msg_name = (void *)&h->peer;
	msg.msg_namelen = sizeof(h->peer);
	msg.msg_iov = iov;
	msg.msg_iovlen = nvecs;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags = 0;
	nlh.nlmsg_len = tlen;
	return ipq_netlink_sendmsg(h, &msg, 0);
}

/* Not implemented yet */
int ipq_ctl(const struct ipq_handle *h, int request, ...)
{
	return 1;
}

char *ipq_errstr(void)
{
	return ipq_strerror(ipq_errno);
}

void ipq_perror(const char *s)
{
	if (s)
		fputs(s, stderr);
	else
		fputs("ERROR", stderr);
	if (ipq_errno)
		fprintf(stderr, ": %s", ipq_errstr());
	if (errno)
		fprintf(stderr, ": %s", strerror(errno));
	fputc('\n', stderr);
}
