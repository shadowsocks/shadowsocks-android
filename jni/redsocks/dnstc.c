/* redsocks - transparent TCP-to-proxy redirector
 * Copyright (C) 2007-2011 Leonid Evdokimov <leon@darkk.net.ru>
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License.  You may obtain a copy
 * of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>

#include "list.h"
#include "log.h"
#include "parser.h"
#include "main.h"
#include "redsocks.h"
#include "dnstc.h"
#include "utils.h"

#define dnstc_log_error(prio, msg...) \
	redsocks_log_write_plain(__FILE__, __LINE__, __func__, 0, &clientaddr, &self->config.bindaddr, prio, ## msg)
#define dnstc_log_errno(prio, msg...) \
	redsocks_log_write_plain(__FILE__, __LINE__, __func__, 1, &clientaddr, &self->config.bindaddr, prio, ## msg)

static void dnstc_fini_instance(dnstc_instance *instance);
static int dnstc_fini();

typedef struct dns_header_t {
	uint16_t id;
	uint8_t qr_opcode_aa_tc_rd;
	uint8_t ra_z_rcode;
	uint16_t qdcount;
	uint16_t ancount;
	uint16_t nscount;
	uint16_t arcount;
} PACKED dns_header;

#define DNS_QR 0x80
#define DNS_TC 0x02
#define DNS_Z  0x70

/***********************************************************************
 * Logic
 */
static void dnstc_pkt_from_client(int fd, short what, void *_arg)
{
	dnstc_instance *self = _arg;
	struct sockaddr_in clientaddr;
	union {
		char raw[0xFFFF]; // UDP packet can't be larger then that
		dns_header h;
	} buf;
	ssize_t pktlen, outgoing;

	assert(fd == EVENT_FD(&self->listener));
	pktlen = red_recv_udp_pkt(fd, buf.raw, sizeof(buf), &clientaddr, NULL);
	if (pktlen == -1)
		return;

	if (pktlen <= sizeof(dns_header)) {
		dnstc_log_error(LOG_INFO, "incomplete DNS request");
		return;
	}

	if (1
		&& (buf.h.qr_opcode_aa_tc_rd & DNS_QR) == 0 /* query */
		&& (buf.h.ra_z_rcode & DNS_Z) == 0 /* Z is Zero */
		&& buf.h.qdcount /* some questions */
		&& !buf.h.ancount && !buf.h.nscount && !buf.h.arcount /* no answers */
	) {
		buf.h.qr_opcode_aa_tc_rd |= DNS_QR;
		buf.h.qr_opcode_aa_tc_rd |= DNS_TC;
		outgoing = sendto(fd, buf.raw, pktlen, 0,
		                  (struct sockaddr*)&clientaddr, sizeof(clientaddr));
		if (outgoing != pktlen)
			dnstc_log_errno(LOG_WARNING, "sendto: I was sending %zd bytes, but only %zd were sent.",
			                pktlen, outgoing);
		else
			dnstc_log_error(LOG_INFO, "sent truncated DNS reply");
	}
	else {
		dnstc_log_error(LOG_INFO, "malformed DNS request");
	}
}

/***********************************************************************
 * Init / shutdown
 */
static parser_entry dnstc_entries[] =
{
	{ .key = "local_ip",   .type = pt_in_addr },
	{ .key = "local_port", .type = pt_uint16 },
	{ }
};

static list_head instances = LIST_HEAD_INIT(instances);

static int dnstc_onenter(parser_section *section)
{
	dnstc_instance *instance = calloc(1, sizeof(*instance));
	if (!instance) {
		parser_error(section->context, "Not enough memory");
		return -1;
	}

	INIT_LIST_HEAD(&instance->list);
	instance->config.bindaddr.sin_family = AF_INET;
	instance->config.bindaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	for (parser_entry *entry = &section->entries[0]; entry->key; entry++)
		entry->addr =
			(strcmp(entry->key, "local_ip") == 0)   ? (void*)&instance->config.bindaddr.sin_addr :
			(strcmp(entry->key, "local_port") == 0) ? (void*)&instance->config.bindaddr.sin_port :
			NULL;
	section->data = instance;
	return 0;
}

static int dnstc_onexit(parser_section *section)
{
	dnstc_instance *instance = section->data;

	section->data = NULL;
	for (parser_entry *entry = &section->entries[0]; entry->key; entry++)
		entry->addr = NULL;

	instance->config.bindaddr.sin_port = htons(instance->config.bindaddr.sin_port);

	list_add(&instance->list, &instances);

	return 0;
}

static int dnstc_init_instance(dnstc_instance *instance)
{
	/* FIXME: dnstc_fini_instance is called in case of failure, this
	 *        function will remove instance from instances list - result
	 *        looks ugly.
	 */
	int error;
	int fd = -1;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd == -1) {
		log_errno(LOG_ERR, "socket");
		goto fail;
	}

	error = bind(fd, (struct sockaddr*)&instance->config.bindaddr, sizeof(instance->config.bindaddr));
	if (error) {
		log_errno(LOG_ERR, "bind");
		goto fail;
	}

	error = fcntl_nonblock(fd);
	if (error) {
		log_errno(LOG_ERR, "fcntl");
		goto fail;
	}

	event_set(&instance->listener, fd, EV_READ | EV_PERSIST, dnstc_pkt_from_client, instance);
	error = event_add(&instance->listener, NULL);
	if (error) {
		log_errno(LOG_ERR, "event_add");
		goto fail;
	}

	return 0;

fail:
	dnstc_fini_instance(instance);

	if (fd != -1) {
		if (close(fd) != 0)
			log_errno(LOG_WARNING, "close");
	}

	return -1;
}

/* Drops instance completely, freeing its memory and removing from
 * instances list.
 */
static void dnstc_fini_instance(dnstc_instance *instance)
{
	if (event_initialized(&instance->listener)) {
		if (event_del(&instance->listener) != 0)
			log_errno(LOG_WARNING, "event_del");
		if (close(EVENT_FD(&instance->listener)) != 0)
			log_errno(LOG_WARNING, "close");
		memset(&instance->listener, 0, sizeof(instance->listener));
	}

	list_del(&instance->list);

	memset(instance, 0, sizeof(*instance));
	free(instance);
}

static int dnstc_init()
{
	dnstc_instance *tmp, *instance = NULL;

	// TODO: init debug_dumper

	list_for_each_entry_safe(instance, tmp, &instances, list) {
		if (dnstc_init_instance(instance) != 0)
			goto fail;
	}

	return 0;

fail:
	dnstc_fini();
	return -1;
}

static int dnstc_fini()
{
	dnstc_instance *tmp, *instance = NULL;

	list_for_each_entry_safe(instance, tmp, &instances, list)
		dnstc_fini_instance(instance);

	return 0;
}

static parser_section dnstc_conf_section =
{
	.name    = "dnstc",
	.entries = dnstc_entries,
	.onenter = dnstc_onenter,
	.onexit  = dnstc_onexit
};

app_subsys dnstc_subsys =
{
	.init = dnstc_init,
	.fini = dnstc_fini,
	.conf_section = &dnstc_conf_section,
};

/* vim:set tabstop=4 softtabstop=4 shiftwidth=4: */
/* vim:set foldmethod=marker foldlevel=32 foldmarker={,}: */
