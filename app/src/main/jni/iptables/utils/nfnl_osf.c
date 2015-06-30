/*
 * Copyright (c) 2005 Evgeniy Polyakov <johnpol@2ka.mxt.ru>
 * 
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/time.h>

#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>

#include <netinet/ip.h>
#include <netinet/tcp.h>

#include <linux/connector.h>
#include <linux/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/unistd.h>

#include <libnfnetlink/libnfnetlink.h>

#include <linux/netfilter/nfnetlink.h>
#include <linux/netfilter/xt_osf.h>

#define OPTDEL			','
#define OSFPDEL 		':'
#define MAXOPTSTRLEN		128

#ifndef NIPQUAD
#define NIPQUAD(addr) \
	((unsigned char *)&addr)[0], \
	((unsigned char *)&addr)[1], \
	((unsigned char *)&addr)[2], \
	((unsigned char *)&addr)[3]
#endif

static struct nfnl_handle *nfnlh;
static struct nfnl_subsys_handle *nfnlssh;

static struct xt_osf_opt IANA_opts[] = {
	{ .kind = 0, .length = 1,},
	{ .kind=1, .length=1,},
	{ .kind=2, .length=4,},
	{ .kind=3, .length=3,},
	{ .kind=4, .length=2,},
	{ .kind=5, .length=1,},		/* SACK length is not defined */
	{ .kind=6, .length=6,},
	{ .kind=7, .length=6,},
	{ .kind=8, .length=10,},
	{ .kind=9, .length=2,},
	{ .kind=10, .length=3,},
	{ .kind=11, .length=1,},		/* CC: Suppose 1 */
	{ .kind=12, .length=1,},		/* the same */
	{ .kind=13, .length=1,},		/* and here too */
	{ .kind=14, .length=3,},
	{ .kind=15, .length=1,},		/* TCP Alternate Checksum Data. Length is not defined */
	{ .kind=16, .length=1,},
	{ .kind=17, .length=1,},
	{ .kind=18, .length=3,},
	{ .kind=19, .length=18,},
	{ .kind=20, .length=1,},
	{ .kind=21, .length=1,},
	{ .kind=22, .length=1,},
	{ .kind=23, .length=1,},
	{ .kind=24, .length=1,},
	{ .kind=25, .length=1,},
	{ .kind=26, .length=1,},
};

static FILE *osf_log_stream;

static void uloga(const char *f, ...)
{
	va_list ap;

	if (!osf_log_stream)
		osf_log_stream = stdout;

	va_start(ap, f);
	vfprintf(osf_log_stream, f, ap);
	va_end(ap);

	fflush(osf_log_stream);
}

static void ulog(const char *f, ...)
{
	char str[64];
	struct tm tm;
	struct timeval tv;
	va_list ap;

	if (!osf_log_stream)
		osf_log_stream = stdout;

	gettimeofday(&tv, NULL);
	localtime_r((time_t *)&tv.tv_sec, &tm);
	strftime(str, sizeof(str), "%F %R:%S", &tm);

	fprintf(osf_log_stream, "%s.%lu %ld ", str, tv.tv_usec, syscall(__NR_gettid));

	va_start(ap, f);
	vfprintf(osf_log_stream, f, ap);
	va_end(ap);

	fflush(osf_log_stream);
}

#define ulog_err(f, a...) uloga(f ": %s [%d].\n", ##a, strerror(errno), errno)

static char *xt_osf_strchr(char *ptr, char c)
{
	char *tmp;

	tmp = strchr(ptr, c);
	if (tmp)
		*tmp = '\0';

	while (tmp && tmp + 1 && isspace(*(tmp + 1)))
		tmp++;

	return tmp;
}

static void xt_osf_parse_opt(struct xt_osf_opt *opt, __u16 *optnum, char *obuf, int olen)
{
	int i, op;
	char *ptr, wc;
	unsigned long val;

	ptr = &obuf[0];
	i = 0;
	while (ptr != NULL && i < olen && *ptr != 0) {
		val = 0;
		op = 0;
		wc = OSF_WSS_PLAIN;
		switch (obuf[i]) {
		case 'N':
			op = OSFOPT_NOP;
			ptr = xt_osf_strchr(&obuf[i], OPTDEL);
			if (ptr) {
				*ptr = '\0';
				ptr++;
				i += (int)(ptr - &obuf[i]);
			} else
				i++;
			break;
		case 'S':
			op = OSFOPT_SACKP;
			ptr = xt_osf_strchr(&obuf[i], OPTDEL);
			if (ptr) {
				*ptr = '\0';
				ptr++;
				i += (int)(ptr - &obuf[i]);
			} else
				i++;
			break;
		case 'T':
			op = OSFOPT_TS;
			ptr = xt_osf_strchr(&obuf[i], OPTDEL);
			if (ptr) {
				*ptr = '\0';
				ptr++;
				i += (int)(ptr - &obuf[i]);
			} else
				i++;
			break;
		case 'W':
			op = OSFOPT_WSO;
			ptr = xt_osf_strchr(&obuf[i], OPTDEL);
			if (ptr) {
				switch (obuf[i + 1]) {
				case '%':
					wc = OSF_WSS_MODULO;
					break;
				case 'S':
					wc = OSF_WSS_MSS;
					break;
				case 'T':
					wc = OSF_WSS_MTU;
					break;
				default:
					wc = OSF_WSS_PLAIN;
					break;
				}

				*ptr = '\0';
				ptr++;
				if (wc)
					val = strtoul(&obuf[i + 2], NULL, 10);
				else
					val = strtoul(&obuf[i + 1], NULL, 10);
				i += (int)(ptr - &obuf[i]);

			} else
				i++;
			break;
		case 'M':
			op = OSFOPT_MSS;
			ptr = xt_osf_strchr(&obuf[i], OPTDEL);
			if (ptr) {
				if (obuf[i + 1] == '%')
					wc = OSF_WSS_MODULO;
				*ptr = '\0';
				ptr++;
				if (wc)
					val = strtoul(&obuf[i + 2], NULL, 10);
				else
					val = strtoul(&obuf[i + 1], NULL, 10);
				i += (int)(ptr - &obuf[i]);
			} else
				i++;
			break;
		case 'E':
			op = OSFOPT_EOL;
			ptr = xt_osf_strchr(&obuf[i], OPTDEL);
			if (ptr) {
				*ptr = '\0';
				ptr++;
				i += (int)(ptr - &obuf[i]);
			} else
				i++;
			break;
		default:
			op = OSFOPT_EMPTY;
			ptr = xt_osf_strchr(&obuf[i], OPTDEL);
			if (ptr) {
				ptr++;
				i += (int)(ptr - &obuf[i]);
			} else
				i++;
			break;
		}

		if (op != OSFOPT_EMPTY) {
			opt[*optnum].kind = IANA_opts[op].kind;
			opt[*optnum].length = IANA_opts[op].length;
			opt[*optnum].wc.wc = wc;
			opt[*optnum].wc.val = val;
			(*optnum)++;
		}
	}
}

static int osf_load_line(char *buffer, int len, int del)
{
	int i, cnt = 0;
	char obuf[MAXOPTSTRLEN];
	struct xt_osf_user_finger f;
	char *pbeg, *pend;
	char buf[NFNL_HEADER_LEN + NFA_LENGTH(sizeof(struct xt_osf_user_finger))];
	struct nlmsghdr *nmh = (struct nlmsghdr *) buf;

	memset(&f, 0, sizeof(struct xt_osf_user_finger));

	ulog("Loading '%s'.\n", buffer);

	for (i = 0; i < len && buffer[i] != '\0'; ++i) {
		if (buffer[i] == ':')
			cnt++;
	}

	if (cnt != 8) {
		ulog("Wrong input line '%s': cnt: %d, must be 8, i: %d, must be %d.\n", buffer, cnt, i, len);
		return -EINVAL;
	}

	memset(obuf, 0, sizeof(obuf));

	pbeg = buffer;
	pend = xt_osf_strchr(pbeg, OSFPDEL);
	if (pend) {
		*pend = '\0';
		if (pbeg[0] == 'S') {
			f.wss.wc = OSF_WSS_MSS;
			if (pbeg[1] == '%')
				f.wss.val = strtoul(&pbeg[2], NULL, 10);
			else if (pbeg[1] == '*')
				f.wss.val = 0;
			else
				f.wss.val = strtoul(&pbeg[1], NULL, 10);
		} else if (pbeg[0] == 'T') {
			f.wss.wc = OSF_WSS_MTU;
			if (pbeg[1] == '%')
				f.wss.val = strtoul(&pbeg[2], NULL, 10);
			else if (pbeg[1] == '*')
				f.wss.val = 0;
			else
				f.wss.val = strtoul(&pbeg[1], NULL, 10);
		} else if (pbeg[0] == '%') {
			f.wss.wc = OSF_WSS_MODULO;
			f.wss.val = strtoul(&pbeg[1], NULL, 10);
		} else if (isdigit(pbeg[0])) {
			f.wss.wc = OSF_WSS_PLAIN;
			f.wss.val = strtoul(&pbeg[0], NULL, 10);
		}

		pbeg = pend + 1;
	}
	pend = xt_osf_strchr(pbeg, OSFPDEL);
	if (pend) {
		*pend = '\0';
		f.ttl = strtoul(pbeg, NULL, 10);
		pbeg = pend + 1;
	}
	pend = xt_osf_strchr(pbeg, OSFPDEL);
	if (pend) {
		*pend = '\0';
		f.df = strtoul(pbeg, NULL, 10);
		pbeg = pend + 1;
	}
	pend = xt_osf_strchr(pbeg, OSFPDEL);
	if (pend) {
		*pend = '\0';
		f.ss = strtoul(pbeg, NULL, 10);
		pbeg = pend + 1;
	}

	pend = xt_osf_strchr(pbeg, OSFPDEL);
	if (pend) {
		*pend = '\0';
		cnt = snprintf(obuf, sizeof(obuf), "%s,", pbeg);
		pbeg = pend + 1;
	}

	pend = xt_osf_strchr(pbeg, OSFPDEL);
	if (pend) {
		*pend = '\0';
		if (pbeg[0] == '@' || pbeg[0] == '*')
			cnt = snprintf(f.genre, sizeof(f.genre), "%s", pbeg + 1);
		else
			cnt = snprintf(f.genre, sizeof(f.genre), "%s", pbeg);
		pbeg = pend + 1;
	}

	pend = xt_osf_strchr(pbeg, OSFPDEL);
	if (pend) {
		*pend = '\0';
		cnt = snprintf(f.version, sizeof(f.version), "%s", pbeg);
		pbeg = pend + 1;
	}

	pend = xt_osf_strchr(pbeg, OSFPDEL);
	if (pend) {
		*pend = '\0';
		cnt =
		    snprintf(f.subtype, sizeof(f.subtype), "%s", pbeg);
		pbeg = pend + 1;
	}

	xt_osf_parse_opt(f.opt, &f.opt_num, obuf, sizeof(obuf));

	memset(buf, 0, sizeof(buf));

	if (del)
		nfnl_fill_hdr(nfnlssh, nmh, 0, AF_UNSPEC, 0, OSF_MSG_REMOVE, NLM_F_REQUEST);
	else
		nfnl_fill_hdr(nfnlssh, nmh, 0, AF_UNSPEC, 0, OSF_MSG_ADD, NLM_F_REQUEST | NLM_F_CREATE);

	nfnl_addattr_l(nmh, sizeof(buf), OSF_ATTR_FINGER, &f, sizeof(struct xt_osf_user_finger));

	return nfnl_talk(nfnlh, nmh, 0, 0, NULL, NULL, NULL);
}

static int osf_load_entries(char *path, int del)
{
	FILE *inf;
	int err = 0;
	char buf[1024];

	inf = fopen(path, "r");
	if (!inf) {
		ulog_err("Failed to open file '%s'", path);
		return -1;
	}

	while(fgets(buf, sizeof(buf), inf)) {
		int len;

		if (buf[0] == '#' || buf[0] == '\n' || buf[0] == '\r')
			continue;

		len = strlen(buf) - 1;

		if (len <= 0)
			continue;

		buf[len] = '\0';

		err = osf_load_line(buf, len, del);
		if (err)
			break;

		memset(buf, 0, sizeof(buf));
	}

	fclose(inf);
	return err;
}

int main(int argc, char *argv[])
{
	int ch, del = 0, err;
	char *fingerprints = NULL;

	while ((ch = getopt(argc, argv, "f:dh")) != -1) {
		switch (ch) {
			case 'f':
				fingerprints = optarg;
				break;
			case 'd':
				del = 1;
				break;
			default:
				fprintf(stderr,
					"Usage: %s -f fingerprints -d <del rules> -h\n",
					argv[0]);
				return -1;
		}
	}

	if (!fingerprints) {
		err = -ENOENT;
		goto err_out_exit;
	}

	nfnlh = nfnl_open();
	if (!nfnlh) {
		err = -EINVAL;
		ulog_err("Failed to create nfnl handler");
		goto err_out_exit;
	}

#ifndef NFNL_SUBSYS_OSF
#define NFNL_SUBSYS_OSF	5
#endif

	nfnlssh = nfnl_subsys_open(nfnlh, NFNL_SUBSYS_OSF, OSF_MSG_MAX, 0);
	if (!nfnlssh) {
		err = -EINVAL;
		ulog_err("Faied to create nfnl subsystem");
		goto err_out_close;
	}

	err = osf_load_entries(fingerprints, del);
	if (err)
		goto err_out_close_subsys;

	nfnl_subsys_close(nfnlssh);
	nfnl_close(nfnlh);

	return 0;

err_out_close_subsys:
	nfnl_subsys_close(nfnlssh);
err_out_close:
	nfnl_close(nfnlh);
err_out_exit:
	return err;
}
