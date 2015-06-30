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
#include <sys/socket.h>
#include "utils.h"
#include "log.h"
#include "redsocks.h"
#include "socks5.h"

typedef enum socks5_state_t {
	socks5_new,
	socks5_method_sent,
	socks5_auth_sent,
	socks5_request_sent,
	socks5_skip_domain,
	socks5_skip_address,
	socks5_MAX,
} socks5_state;

typedef struct socks5_client_t {
	int do_password; // 1 - password authentication is possible
	int to_skip;     // valid while reading last reply (after main request)
} socks5_client;

const char *socks5_strstatus[] = {
	"ok",
	"server failure",
	"connection not allowed by ruleset",
	"network unreachable",
	"host unreachable",
	"connection refused",
	"TTL expired",
	"command not supported",
	"address type not supported",
};
const size_t socks5_strstatus_len = SIZEOF_ARRAY(socks5_strstatus);

const char* socks5_status_to_str(int socks5_status)
{
	if (0 <= socks5_status && socks5_status < socks5_strstatus_len) {
		return socks5_strstatus[socks5_status];
	}
	else {
		return "";
	}
}

int socks5_is_valid_cred(const char *login, const char *password)
{
	if (!login || !password)
		return 0;
	if (strlen(login) > 255) {
		log_error(LOG_WARNING, "Socks5 login can't be more than 255 chars, <%s> is too long", login);
		return 0;
	}
	if (strlen(password) > 255) {
		log_error(LOG_WARNING, "Socks5 password can't be more than 255 chars, <%s> is too long", password);
		return 0;
	}
	return 1;
}

void socks5_client_init(redsocks_client *client)
{
	socks5_client *socks5 = (void*)(client + 1);
	const redsocks_config *config = &client->instance->config;

	client->state = socks5_new;
	socks5->do_password = socks5_is_valid_cred(config->login, config->password);
}

static struct evbuffer *socks5_mkmethods(redsocks_client *client)
{
	socks5_client *socks5 = (void*)(client + 1);
	return socks5_mkmethods_plain(socks5->do_password);
}

struct evbuffer *socks5_mkmethods_plain(int do_password)
{
	assert(do_password == 0 || do_password == 1);
	int len = sizeof(socks5_method_req) + do_password;
    socks5_method_req *req = calloc(1, len);

	req->ver = socks5_ver;
	req->num_methods = 1 + do_password;
	req->methods[0] = socks5_auth_none;
	if (do_password)
		req->methods[1] = socks5_auth_password;

	struct evbuffer *ret = mkevbuffer(req, len);
	free(req);
	return ret;
}

static struct evbuffer *socks5_mkpassword(redsocks_client *client)
{
	return socks5_mkpassword_plain(client->instance->config.login, client->instance->config.password);
}

struct evbuffer *socks5_mkpassword_plain(const char *login, const char *password)
{
	size_t ulen = strlen(login);
	size_t plen = strlen(password);
	size_t length =  1 /* version */ + 1 + ulen + 1 + plen;
	uint8_t req[length];

	req[0] = socks5_password_ver; // RFC 1929 says so
	req[1] = ulen;
	memcpy(&req[2], login, ulen);
	req[2+ulen] = plen;
	memcpy(&req[3+ulen], password, plen);
	return mkevbuffer(req, length);
}

struct evbuffer *socks5_mkcommand_plain(int socks5_cmd, const struct sockaddr_in *destaddr)
{
	struct {
		socks5_req head;
		socks5_addr_ipv4 ip;
	} PACKED req;

	assert(destaddr->sin_family == AF_INET);

	req.head.ver = socks5_ver;
	req.head.cmd = socks5_cmd;
	req.head.reserved = 0;
	req.head.addrtype = socks5_addrtype_ipv4;
	req.ip.addr = destaddr->sin_addr.s_addr;
	req.ip.port = destaddr->sin_port;
	return mkevbuffer(&req, sizeof(req));
}

static struct evbuffer *socks5_mkconnect(redsocks_client *client)
{
	return socks5_mkcommand_plain(socks5_cmd_connect, &client->destaddr);
}

static void socks5_write_cb(struct bufferevent *buffev, void *_arg)
{
	redsocks_client *client = _arg;

	redsocks_touch_client(client);

	if (client->state == socks5_new) {
		redsocks_write_helper(
			buffev, client,
			socks5_mkmethods, socks5_method_sent, sizeof(socks5_method_reply)
			);
	}
}

const char* socks5_is_known_auth_method(socks5_method_reply *reply, int do_password)
{
	if (reply->ver != socks5_ver)
		return "Socks5 server reported unexpected auth methods reply version...";
	else if (reply->method == socks5_auth_invalid)
		return "Socks5 server refused all our auth methods.";
	else if (reply->method != socks5_auth_none && !(reply->method == socks5_auth_password && do_password))
		return "Socks5 server requested unexpected auth method...";
	else
		return NULL;
}

static void socks5_read_auth_methods(struct bufferevent *buffev, redsocks_client *client, socks5_client *socks5)
{
	socks5_method_reply reply;
	const char *error = NULL;

	if (redsocks_read_expected(client, buffev->input, &reply, sizes_equal, sizeof(reply)) < 0)
		return;

	error = socks5_is_known_auth_method(&reply, socks5->do_password);
	if (error) {
		redsocks_log_error(client, LOG_NOTICE, "socks5_is_known_auth_method: %s", error);
		redsocks_drop_client(client);
	}
	else if (reply.method == socks5_auth_none) {
		redsocks_write_helper(
			buffev, client,
			socks5_mkconnect, socks5_request_sent, sizeof(socks5_reply)
			);
	}
	else if (reply.method == socks5_auth_password) {
		redsocks_write_helper(
			buffev, client,
			socks5_mkpassword, socks5_auth_sent, sizeof(socks5_auth_reply)
			);
	}
}

static void socks5_read_auth_reply(struct bufferevent *buffev, redsocks_client *client, socks5_client *socks5)
{
	socks5_auth_reply reply;

	if (redsocks_read_expected(client, buffev->input, &reply, sizes_equal, sizeof(reply)) < 0)
		return;

	if (reply.ver != socks5_password_ver) {
		redsocks_log_error(client, LOG_NOTICE, "Socks5 server reported unexpected auth reply version...");
		redsocks_drop_client(client);
	}
	else if (reply.status == socks5_password_passed)
		redsocks_write_helper(
			buffev, client,
			socks5_mkconnect, socks5_request_sent, sizeof(socks5_reply)
			);
	else
		redsocks_drop_client(client);
}

static void socks5_read_reply(struct bufferevent *buffev, redsocks_client *client, socks5_client *socks5)
{
	socks5_reply reply;

	if (redsocks_read_expected(client, buffev->input, &reply, sizes_greater_equal, sizeof(reply)) < 0)
		return;

	if (reply.ver != socks5_ver) {
		redsocks_log_error(client, LOG_NOTICE, "Socks5 server reported unexpected reply version...");
		redsocks_drop_client(client);
	}
	else if (reply.status == socks5_status_succeeded) {
		socks5_state nextstate;
		size_t len;

		if (reply.addrtype == socks5_addrtype_ipv4) {
			len = socks5->to_skip = sizeof(socks5_addr_ipv4);
			nextstate = socks5_skip_address;
		}
		else if (reply.addrtype == socks5_addrtype_ipv6) {
			len = socks5->to_skip = sizeof(socks5_addr_ipv6);
			nextstate = socks5_skip_address;
		}
		else if (reply.addrtype == socks5_addrtype_domain) {
			socks5_addr_domain domain;
			len = sizeof(domain.size);
			nextstate = socks5_skip_domain;
		}
		else {
			redsocks_log_error(client, LOG_NOTICE, "Socks5 server reported unexpected address type...");
			redsocks_drop_client(client);
			return;
		}

		redsocks_write_helper(
			buffev, client,
			NULL, nextstate, len
			);
	}
	else {
		redsocks_log_error(client, LOG_NOTICE, "Socks5 server status: %s (%i)",
				/* 0 <= reply.status && */ reply.status < SIZEOF_ARRAY(socks5_strstatus)
				? socks5_strstatus[reply.status] : "?", reply.status);
		redsocks_drop_client(client);
	}
}

static void socks5_read_cb(struct bufferevent *buffev, void *_arg)
{
	redsocks_client *client = _arg;
	socks5_client *socks5 = (void*)(client + 1);

	redsocks_touch_client(client);

	if (client->state == socks5_method_sent) {
		socks5_read_auth_methods(buffev, client, socks5);
	}
	else if (client->state == socks5_auth_sent) {
		socks5_read_auth_reply(buffev, client, socks5);
	}
	else if (client->state == socks5_request_sent) {
		socks5_read_reply(buffev, client, socks5);
	}
	else if (client->state == socks5_skip_domain) {
		socks5_addr_ipv4 ipv4; // all socks5_addr*.port are equal
		uint8_t size;
		if (redsocks_read_expected(client, buffev->input, &size, sizes_greater_equal, sizeof(size)) < 0)
			return;
		socks5->to_skip = size + sizeof(ipv4.port);
		redsocks_write_helper(
			buffev, client,
			NULL, socks5_skip_address, socks5->to_skip
			);
	}
	else if (client->state == socks5_skip_address) {
		uint8_t data[socks5->to_skip];
		if (redsocks_read_expected(client, buffev->input, data, sizes_greater_equal, socks5->to_skip) < 0)
			return;
		redsocks_start_relay(client);
	}
	else {
		redsocks_drop_client(client);
	}
}

relay_subsys socks5_subsys =
{
	.name                 = "socks5",
	.payload_len          = sizeof(socks5_client),
	.instance_payload_len = 0,
	.readcb               = socks5_read_cb,
	.writecb              = socks5_write_cb,
	.init                 = socks5_client_init,
};


/* vim:set tabstop=4 softtabstop=4 shiftwidth=4: */
/* vim:set foldmethod=marker foldlevel=32 foldmarker={,}: */
