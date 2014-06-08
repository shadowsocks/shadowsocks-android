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
#include <assert.h>
#include "log.h"
#include "redsocks.h"

typedef enum socks4_state_t {
	socks4_new,
	socks4_request_sent,
	socks4_reply_came,
	socks4_MAX,
} socks4_state;

typedef struct socks4_req_t {
	uint8_t ver;
	uint8_t cmd;
	uint16_t port;
	uint32_t addr;
	char login[1]; // we need at least zero-byte
} PACKED socks4_req;

const int socks4_ver = 4;
const int socks4_cmd_connect = 1;
const int socks4_cmd_bind = 2;

typedef struct socks4_reply_t {
	uint8_t ver;
	uint8_t status;
	uint16_t port;
	uint32_t addr;
} PACKED socks4_reply;

const int socks4_status_ok = 90;
const int socks4_status_fail = 91;
const int socks4_status_no_ident = 92;
const int socks4_status_fake_ident = 93;


void socks4_client_init(redsocks_client *client)
{
	if (client->instance->config.password)
		redsocks_log_error(client, LOG_WARNING, "password is ignored for socks4 connections");

	client->state = socks4_new;
}


static void socks4_read_cb(struct bufferevent *buffev, void *_arg)
{
	redsocks_client *client = _arg;

	assert(client->state >= socks4_request_sent);

	redsocks_touch_client(client);

	if (client->state == socks4_request_sent) {
		socks4_reply reply;

		if (redsocks_read_expected(client, buffev->input, &reply, sizes_greater_equal, sizeof(reply)) < 0)
			return;

		client->state = socks4_reply_came;
		if (reply.ver != 0) {
			redsocks_log_error(client, LOG_NOTICE, "Socks4 server reported unexpected reply version...");
			redsocks_drop_client(client);
		}
		else if (reply.status == socks4_status_ok)
			redsocks_start_relay(client);
		else {
			redsocks_log_error(client, LOG_NOTICE, "Socks4 server status: %s (%i)",
				reply.status == socks4_status_fail ? "fail" :
				reply.status == socks4_status_no_ident ? "no ident" :
				reply.status == socks4_status_fake_ident ? "fake ident" : "?",
				reply.status);
			redsocks_drop_client(client);
		}
	}
}

static struct evbuffer *socks4_mkconnect(redsocks_client *client)
{
	const redsocks_config *config = &client->instance->config;
	const char *username = config->login ? config->login : "";
	// space for \0 comes from socks4_req->login
	size_t username_len = strlen(username);
	size_t len = sizeof(socks4_req) + username_len;
	socks4_req *req = calloc(1, len);

	req->ver = socks4_ver;
	req->cmd = socks4_cmd_connect;
	req->port = client->destaddr.sin_port;
	req->addr = client->destaddr.sin_addr.s_addr;
	memcpy(req->login, username, username_len + 1);

	struct evbuffer *ret = mkevbuffer(req, len);
	free(req);
	return ret;
}

static void socks4_write_cb(struct bufferevent *buffev, void *_arg)
{
	redsocks_client *client = _arg;

	redsocks_touch_client(client);

	if (client->state == socks4_new) {
		redsocks_write_helper(
			buffev, client,
			socks4_mkconnect, socks4_request_sent, sizeof(socks4_reply)
			);
	}
	else if (client->state >= socks4_request_sent) {
		bufferevent_disable(buffev, EV_WRITE);
	}
}


relay_subsys socks4_subsys =
{
	.name                 = "socks4",
	.payload_len          = 0,
	.instance_payload_len = 0,
	.readcb               = socks4_read_cb,
	.writecb              = socks4_write_cb,
	.init                 = socks4_client_init,
};


/* vim:set tabstop=4 softtabstop=4 shiftwidth=4: */
/* vim:set foldmethod=marker foldlevel=32 foldmarker={,}: */
