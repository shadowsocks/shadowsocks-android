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
 *
 *
 * http-relay upstream module for redsocks
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "log.h"
#include "redsocks.h"
#include "http-auth.h"
#include "utils.h"

#define HTTP_HEAD_WM_HIGH (4096)

typedef enum httpr_state_t {
	httpr_new,
	httpr_recv_request_headers,
	httpr_request_sent,
	httpr_reply_came,
	httpr_headers_skipped,
	httpr_MAX,
} httpr_state;

typedef struct httpr_buffer_t {
	char *buff;
	int len;
	int max_len;
} httpr_buffer;

typedef struct httpr_client_t {
	char *firstline;
	char *host;
	int has_host;
	httpr_buffer client_buffer;
	httpr_buffer relay_buffer;
} httpr_client;

extern const char *auth_request_header;
extern const char *auth_response_header;

static void httpr_connect_relay(redsocks_client *client);

static int httpr_buffer_init(httpr_buffer *buff)
{
	buff->max_len = 4096;
	buff->len = 0;
	buff->buff = calloc(buff->max_len, 1);
	if (!buff->buff)
		return -1;
	return 0;
}

static void httpr_buffer_fini(httpr_buffer *buff)
{
	free(buff->buff);
	buff->buff = NULL;
}

static int httpr_buffer_append(httpr_buffer *buff, const char *data, int len)
{
	while (buff->len + len + 1 > buff->max_len) {
		/* double the buffer size */
		buff->max_len *= 2;
	}
	char *new_buff = calloc(buff->max_len, 1);
	if (!new_buff) {
		return -1;
	}
	memcpy(new_buff, buff->buff, buff->len);
	memcpy(new_buff + buff->len, data, len);
	buff->len += len;
	new_buff[buff->len] = '\0';
	free(buff->buff);
	buff->buff = new_buff;
	return 0;
}

static void httpr_client_init(redsocks_client *client)
{
	httpr_client *httpr = (void*)(client + 1);

	client->state = httpr_new;
	memset(httpr, 0, sizeof(*httpr));
	httpr_buffer_init(&httpr->client_buffer);
	httpr_buffer_init(&httpr->relay_buffer);
}

static void httpr_client_fini(redsocks_client *client)
{
	httpr_client *httpr = (void*)(client + 1);

	free(httpr->firstline);
	httpr->firstline = NULL;
	free(httpr->host);
	httpr->host = NULL;
	httpr_buffer_fini(&httpr->client_buffer);
	httpr_buffer_fini(&httpr->relay_buffer);
}

static void httpr_instance_fini(redsocks_instance *instance)
{
	http_auth *auth = (void*)(instance + 1);
	free(auth->last_auth_query);
	auth->last_auth_query = NULL;
}

static char *get_auth_request_header(struct evbuffer *buf)
{
	char *line;
	for (;;) {
		line = redsocks_evbuffer_readline(buf);
		if (line == NULL || *line == '\0' || strchr(line, ':') == NULL) {
			free(line);
			return NULL;
		}
		if (strncasecmp(line, auth_request_header, strlen(auth_request_header)) == 0)
			return line;
		free(line);
	}
}

static void httpr_relay_read_cb(struct bufferevent *buffev, void *_arg)
{
	redsocks_client *client = _arg;
	httpr_client *httpr = (void*)(client + 1);
	int dropped = 0;

	assert(client->state >= httpr_request_sent);

	redsocks_touch_client(client);

	httpr_buffer_fini(&httpr->relay_buffer);
	httpr_buffer_init(&httpr->relay_buffer);

	if (client->state == httpr_request_sent) {
		size_t len = EVBUFFER_LENGTH(buffev->input);
		char *line = redsocks_evbuffer_readline(buffev->input);
		if (line) {
			httpr_buffer_append(&httpr->relay_buffer, line, strlen(line));
			httpr_buffer_append(&httpr->relay_buffer, "\r\n", 2);
			unsigned int code;
			if (sscanf(line, "HTTP/%*u.%*u %u", &code) == 1) { // 1 == one _assigned_ match
				if (code == 407) { // auth failed
					http_auth *auth = (void*)(client->instance + 1);

					if (auth->last_auth_query != NULL && auth->last_auth_count == 1) {
						redsocks_log_error(client, LOG_NOTICE, "proxy auth failed");
						redsocks_drop_client(client);

						dropped = 1;
					} else if (client->instance->config.login == NULL || client->instance->config.password == NULL) {
						redsocks_log_error(client, LOG_NOTICE, "proxy auth required, but no login information provided");
						redsocks_drop_client(client);

						dropped = 1;
					} else {
						free(line);
						char *auth_request = get_auth_request_header(buffev->input);

						if (!auth_request) {
							redsocks_log_error(client, LOG_NOTICE, "403 found, but no proxy auth challenge");
							redsocks_drop_client(client);
							dropped = 1;
						} else {
							free(auth->last_auth_query);
							char *ptr = auth_request;

							ptr += strlen(auth_request_header);
							while (isspace(*ptr))
								ptr++;

							auth->last_auth_query = calloc(strlen(ptr) + 1, 1);
							strcpy(auth->last_auth_query, ptr);
							auth->last_auth_count = 0;

							free(auth_request);

							httpr_buffer_fini(&httpr->relay_buffer);

							if (bufferevent_disable(client->relay, EV_WRITE)) {
								redsocks_log_errno(client, LOG_ERR, "bufferevent_disable");
								return;
							}

							/* close relay tunnel */
							redsocks_close(EVENT_FD(&client->relay->ev_write));
							bufferevent_free(client->relay);

							/* set to initial state*/
							client->state = httpr_recv_request_headers;

							/* and reconnect */
							redsocks_connect_relay(client);
							return;
						}
					}
				} else if (100 <= code && code <= 999) {
					client->state = httpr_reply_came;
				} else {
					redsocks_log_error(client, LOG_NOTICE, "%s", line);
					redsocks_drop_client(client);
					dropped = 1;
				}
			}
			free(line);
		}
		else if (len >= HTTP_HEAD_WM_HIGH) {
			redsocks_drop_client(client);
			dropped = 1;
		}
	}

	if (dropped)
		return;

	while (client->state == httpr_reply_came) {
		char *line = redsocks_evbuffer_readline(buffev->input);
		if (line) {
			httpr_buffer_append(&httpr->relay_buffer, line, strlen(line));
			httpr_buffer_append(&httpr->relay_buffer, "\r\n", 2);
			if (strlen(line) == 0) {
				client->state = httpr_headers_skipped;
			}
			free(line);
		}
		else {
			break;
		}
	}

	if (client->state == httpr_headers_skipped) {
		if (bufferevent_write(client->client, httpr->relay_buffer.buff, httpr->relay_buffer.len) != 0) {
			redsocks_log_error(client, LOG_NOTICE, "bufferevent_write");
			redsocks_drop_client(client);
			return;
		}
		redsocks_start_relay(client);
	}

}

static void httpr_relay_write_cb(struct bufferevent *buffev, void *_arg)
{
	redsocks_client *client = _arg;
	httpr_client *httpr = (void*)(client + 1);
	int len = 0;

	assert(client->state >= httpr_recv_request_headers);

	redsocks_touch_client(client);

	if (client->state == httpr_recv_request_headers) {
		if (httpr->firstline) {
			len = bufferevent_write(client->relay, httpr->firstline, strlen(httpr->firstline));
			if (len < 0) {
				redsocks_log_errno(client, LOG_ERR, "bufferevent_write");
				redsocks_drop_client(client);
				return;
			}
		}


		http_auth *auth = (void*)(client->instance + 1);
		++auth->last_auth_count;

		const char *auth_scheme = NULL;
		char *auth_string = NULL;

		if (auth->last_auth_query != NULL) {
			/* find previous auth challange */

			if (strncasecmp(auth->last_auth_query, "Basic", 5) == 0) {
				auth_string = basic_authentication_encode(client->instance->config.login, client->instance->config.password);
				auth_scheme = "Basic";
			} else if (strncasecmp(auth->last_auth_query, "Digest", 6) == 0 && httpr->firstline) {
				/* calculate method & uri */
				char *ptr = strchr(httpr->firstline, ' '), *ptr2;
				char *method = calloc(ptr - httpr->firstline + 1, 1);
				memcpy(method, httpr->firstline, ptr - httpr->firstline);
				method[ptr - httpr->firstline] = 0;

				ptr = strchr(httpr->firstline, '/');
				if (!ptr || *++ptr != '/') {
					free(method);
					redsocks_log_error(client, LOG_NOTICE, "malformed request came");
					redsocks_drop_client(client);
					return;
				}
				if (!(ptr = strchr(++ptr, '/')) || !(ptr2 = strchr(ptr, ' '))) {
					free(method);
					redsocks_log_error(client, LOG_NOTICE, "malformed request came");
					redsocks_drop_client(client);
					return;
				}
				char *uri = calloc(ptr2 - ptr + 1, 1);
				memcpy(uri, ptr, ptr2 - ptr);
				uri[ptr2 - ptr] = 0;

				/* prepare an random string for cnounce */
				char cnounce[17];
				snprintf(cnounce, sizeof(cnounce), "%04x%04x%04x%04x",
				         rand() & 0xffff, rand() & 0xffff, rand() & 0xffff, rand() & 0xffff);

				auth_string = digest_authentication_encode(auth->last_auth_query + 7, //line
						client->instance->config.login, client->instance->config.password, //user, pass
						method, uri, auth->last_auth_count, cnounce); // method, path, nc, cnounce

				free(method);
				free(uri);
				auth_scheme = "Digest";
			}
		}

		if (auth_string != NULL) {
			len = 0;
			len |= bufferevent_write(client->relay, auth_response_header, strlen(auth_response_header));
			len |= bufferevent_write(client->relay, " ", 1);
			len |= bufferevent_write(client->relay, auth_scheme, strlen(auth_scheme));
			len |= bufferevent_write(client->relay, " ", 1);
			len |= bufferevent_write(client->relay, auth_string, strlen(auth_string));
			len |= bufferevent_write(client->relay, "\r\n", 2);
			if (len) {
				redsocks_log_errno(client, LOG_ERR, "bufferevent_write");
				redsocks_drop_client(client);
				return;
			}
		}

		free(auth_string);

		len = bufferevent_write(client->relay, httpr->client_buffer.buff, httpr->client_buffer.len);
		if (len < 0) {
			redsocks_log_errno(client, LOG_ERR, "bufferevent_write");
			redsocks_drop_client(client);
			return;
		}

		client->state = httpr_request_sent;

		buffev->wm_read.low = 1;
		buffev->wm_read.high = HTTP_HEAD_WM_HIGH;
		bufferevent_enable(buffev, EV_READ);
	}
}

// drops client on failure
static int httpr_append_header(redsocks_client *client, char *line)
{
	httpr_client *httpr = (void*)(client + 1);

	if (httpr_buffer_append(&httpr->client_buffer, line, strlen(line)) != 0)
		return -1;
	if (httpr_buffer_append(&httpr->client_buffer, "\x0d\x0a", 2) != 0)
		return -1;
	return 0;
}

// This function is not reenterable
static char *fmt_http_host(struct sockaddr_in addr)
{
	static char host[] = "123.123.123.123:12345";
	if (ntohs(addr.sin_port) == 80)
		return inet_ntoa(addr.sin_addr);
	else {
		snprintf(host, sizeof(host),
				"%s:%u",
				inet_ntoa(addr.sin_addr),
				ntohs(addr.sin_port)
				);
		return host;
	}
}

static int httpr_toss_http_firstline(redsocks_client *client)
{
	httpr_client *httpr = (void*)(client + 1);
	char *uri = NULL;
	char *host = httpr->has_host ? httpr->host : fmt_http_host(client->destaddr);

	assert(httpr->firstline);

	uri = strchr(httpr->firstline, ' ');
	if (uri)
		uri += 1; // one char further
	else {
		redsocks_log_error(client, LOG_NOTICE, "malformed request came");
		goto fail;
	}

	httpr_buffer nbuff;
	if (httpr_buffer_init(&nbuff) != 0) {
		redsocks_log_error(client, LOG_ERR, "httpr_buffer_init");
		goto fail;
	}

	if (httpr_buffer_append(&nbuff, httpr->firstline, uri - httpr->firstline) != 0)
		goto addition_fail;
	if (httpr_buffer_append(&nbuff, "http://", 7) != 0)
		goto addition_fail;
	if (httpr_buffer_append(&nbuff, host, strlen(host)) != 0)
		goto addition_fail;
	if (httpr_buffer_append(&nbuff, uri, strlen(uri)) != 0)
		goto addition_fail;
	if (httpr_buffer_append(&nbuff, "\x0d\x0a", 2) != 0)
		goto addition_fail;

	free(httpr->firstline);

	httpr->firstline = calloc(nbuff.len + 1, 1);
	strcpy(httpr->firstline, nbuff.buff);
	httpr_buffer_fini(&nbuff);
	return 0;

addition_fail:
	httpr_buffer_fini(&nbuff);
fail:
	redsocks_log_error(client, LOG_ERR, "httpr_toss_http_firstline");
	return -1;
}

static void httpr_client_read_content(struct bufferevent *buffev, redsocks_client *client)
{
	httpr_client *httpr = (void*)(client + 1);

	static int post_buffer_len = 64 * 1024;
	char *post_buffer = calloc(post_buffer_len, 1);
	if (!post_buffer) {
		redsocks_log_error(client, LOG_ERR, "run out of memory");
		redsocks_drop_client(client);
		return;
	}
	int error;
	while (true) {
		error = evbuffer_remove(buffev->input, post_buffer, post_buffer_len);
		if (error < 0) {
			free(post_buffer);
			redsocks_log_error(client, LOG_ERR, "evbuffer_remove");
			redsocks_drop_client(client);
			return;
		}
		if (error == 0)
			break;
		httpr_buffer_append(&httpr->client_buffer, post_buffer, error);
		if (client->relay && client->state >= httpr_request_sent) {
			if (bufferevent_write(client->relay, post_buffer, error) != 0) {
				free(post_buffer);
				redsocks_log_error(client, LOG_ERR, "bufferevent_write");
				redsocks_drop_client(client);
				return;
			}
		}

	}
	free(post_buffer);
}

static void httpr_client_read_cb(struct bufferevent *buffev, void *_arg)
{
	redsocks_client *client = _arg;
	httpr_client *httpr = (void*)(client + 1);

	redsocks_touch_client(client);

	if (client->state >= httpr_recv_request_headers) {
		httpr_client_read_content(buffev, client);
		return;
	}

	char *line = NULL;
	int connect_relay = 0;

	while (!connect_relay && (line = redsocks_evbuffer_readline(buffev->input))) {
		int skip_line = 0;
		int do_drop = 0;

		if (strlen(line) > 0) {
			if (!httpr->firstline) {
				httpr->firstline = line;
				line = 0;
			}
			else if (strncasecmp(line, "Host", 4) == 0) {
				httpr->has_host = 1;
				char *ptr = line + 5;
				while (*ptr && isspace(*ptr))
					ptr ++;
				httpr->host = calloc(strlen(ptr) + 1, 1);
				strcpy(httpr->host, ptr);
			} else if (strncasecmp(line, "Proxy-Connection", 16) == 0)
				skip_line = 1;
			else if (strncasecmp(line, "Connection", 10) == 0)
				skip_line = 1;

		}
		else { // last line of request
			if (!httpr->firstline || httpr_toss_http_firstline(client) < 0)
				do_drop = 1;

			if (!httpr->has_host) {
				char host[32]; // "Host: 123.456.789.012:34567"
				int written_wo_null = snprintf(host, sizeof(host), "Host: %s",
				                               fmt_http_host(client->destaddr));
				UNUSED(written_wo_null);
				assert(0 < written_wo_null && written_wo_null < sizeof(host));
				if (httpr_append_header(client, host) < 0)
					do_drop = 1;
			}

			if (httpr_append_header(client, "Proxy-Connection: close") < 0)
				do_drop = 1;

			if (httpr_append_header(client, "Connection: close") < 0)
				do_drop = 1;

			connect_relay = 1;
		}

		if (line && !skip_line)
			if (httpr_append_header(client, line) < 0)
				do_drop = 1;

		free(line);

		if (do_drop) {
			redsocks_drop_client(client);
			return;
		}
	}

	if (connect_relay) {
		client->state = httpr_recv_request_headers;
		httpr_client_read_content(buffev, client);
		redsocks_connect_relay(client);
	}
}

static void httpr_connect_relay(redsocks_client *client)
{
	int error;

	client->client->readcb = httpr_client_read_cb;
	error = bufferevent_enable(client->client, EV_READ);
	if (error) {
		redsocks_log_errno(client, LOG_ERR, "bufferevent_enable");
		redsocks_drop_client(client);
	}
}

relay_subsys http_relay_subsys =
{
	.name                 = "http-relay",
	.payload_len          = sizeof(httpr_client),
	.instance_payload_len = sizeof(http_auth),
	.init                 = httpr_client_init,
	.fini                 = httpr_client_fini,
	.connect_relay        = httpr_connect_relay,
	.readcb               = httpr_relay_read_cb,
	.writecb              = httpr_relay_write_cb,
	.instance_fini        = httpr_instance_fini,
};

/* vim:set tabstop=4 softtabstop=4 shiftwidth=4: */
/* vim:set foldmethod=marker foldlevel=32 foldmarker={,}: */
