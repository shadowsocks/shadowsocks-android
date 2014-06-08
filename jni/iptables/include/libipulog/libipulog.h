#ifndef _LIBIPULOG_H
#define _LIBIPULOG_H

/* libipulog.h,v 1.3 2001/05/21 19:15:16 laforge Exp */

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <net/if.h>
#include <linux/netfilter_ipv4/ipt_ULOG.h>

/* FIXME: glibc sucks */
#ifndef MSG_TRUNC 
#define MSG_TRUNC	0x20
#endif

struct ipulog_handle;

u_int32_t ipulog_group2gmask(u_int32_t group);

struct ipulog_handle *ipulog_create_handle(u_int32_t gmask);

void ipulog_destroy_handle(struct ipulog_handle *h);

ssize_t ipulog_read(struct ipulog_handle *h,
		    unsigned char *buf, size_t len, int timeout);

ulog_packet_msg_t *ipulog_get_packet(struct ipulog_handle *h,
				     const unsigned char *buf,
				     size_t len);

void ipulog_perror(const char *s);

#endif /* _LIBULOG_H */
