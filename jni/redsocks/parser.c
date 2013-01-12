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
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include "utils.h"
#include "parser.h"
#include "log.h"

#define FREE(ptr) do { free(ptr); ptr = NULL; } while (0)

typedef int (*value_parser)(parser_context *context, void *addr, const char *token);
struct parser_context_t {
	FILE *fd;
	parser_section *sections;
	int line;
	int error;
	parser_errhandler errhandler;
	struct {
		size_t size;
		size_t filled;
		char *data;
	} buffer;
};


void parser_error(parser_context *context, const char *msg)
{
	context->error = 1;
	if (context->errhandler)
		context->errhandler(msg, context->line);
	else
		fprintf(stderr, "file parsing error at line %u: %s\n", context->line, msg);
}

parser_context* parser_start(FILE *fd, parser_errhandler errhandler)
{
	parser_context *ret = calloc(1, sizeof(parser_context));
	if (!ret)
		return NULL;
	ret->fd = fd;
	ret->errhandler = errhandler;
	ret->buffer.size = 128; // should be big enough to fetch whole ``line``
	ret->buffer.data = malloc(ret->buffer.size);
	if (!ret->buffer.data) {
		free(ret);
		return NULL;
	}
	return ret;
}

void parser_add_section(parser_context *context, parser_section *section)
{
	section->next = context->sections;
	context->sections = section;
	section->context = context;
}

void parser_stop(parser_context *context)
{
	free(context->buffer.data);
	free(context);
}

static char unescape(int c)
{
	switch (c) {
		case 'n': return '\n';
		case 't': return '\t';
		case 'r': return '\r';

		case '\\': return '\\';
		case '\'': return '\'';
		case '\"': return '\"';

		default: return 0;
	}
}

/** returns NULL on invalid OR incomplete token
 */
static char *gettoken(parser_context *context, char **iter)
{
	char *ret = NULL;
	size_t len = 0;
	enum {
		gt_cstr,
		gt_plainstr
	} copytype;

	// skip spaces
	while (**iter && isspace(**iter))
		(*iter)++;
	if ( !**iter )
		return NULL;

	// count strlen() of output buffer
	if ( **iter == '\"' ) { // string with escapes and spaces
		char *p = *iter + 1;

		copytype = gt_cstr;
		while ( 1 ) {
			if (*p == '\0')
				return NULL;
			if (*p == '\"')
				break;
			if (*p == '\\') {
				if ( p[1] != '\0') {
					if ( unescape(p[1]) )
						p++;
					else {
						parser_error(context, "unknown escaped char after \\");
						return NULL;
					}
				}
				else {
					return NULL;
				}
			}
			len++;
			p++;
		}
	}
	else if ( isdigit(**iter) ) { // integer OR IP/NETMASK
		char *p = *iter;
		copytype = gt_plainstr;
		while ( 1 ) {
			if ( *p == '\0' )
				return NULL;
			else if ( isdigit(*p) || *p == '.' )
				p++;
			else if ( *p == '/' ) {
				if (isdigit(p[1]))
					p++;
				else if (p[1] == '/' || p[1] == '*') // comment token is coming!
					break;
				else
					return NULL;
			}
			else
				break;
		}
		len = p - *iter;
	}
	else if ( isalpha(**iter) ) { // simple-string
		char *p = *iter;
		copytype = gt_plainstr;
		while ( 1 ) {
			if ( *p == '\0' )
				return NULL;
			else if (isalnum(*p) || *p == '_' || *p == '.' || *p == '-') // for domain-names
				p++;
			else
				break;
		}
		len = p - *iter;
	}
	else if ( **iter == '{' || **iter == '}' || **iter == '=' || **iter == ';' ) { // config punctuation
		copytype = gt_plainstr;
		len = 1;
	}
	else if ( **iter == '/' && ( (*iter)[1] == '/' || (*iter)[1] == '*' ) ) { // comment-start
		copytype = gt_plainstr;
		len = 2;
	}
	else {
		parser_error(context, "unexpected char");
		return NULL;
	}

	ret = malloc(len + 1);
	if (!ret) {
		parser_error(context, "malloc failed");
		return NULL;
	}

	if (copytype == gt_cstr) {
		char *p = ret;
		(*iter)++;
		while ( 1 ) {
			if (**iter == '\"') {
				(*iter)++;
				break;
			}
			if (**iter == '\\') {
				*p = unescape(*(*iter + 1));
				(*iter)++;
			}
			else {
				*p = **iter;
			}
			(*iter)++;
			p++;
		}
		*p = 0;
	}
	else if (copytype == gt_plainstr) {
		memcpy(ret, *iter, len);
		*iter += len;
		ret[len] = 0;
	}

	return ret;
}

static void context_filled_add(parser_context *context, int shift)
{
	context->buffer.filled += shift;
	context->buffer.data[context->buffer.filled] = 0;
}

static void context_filled_set(parser_context *context, size_t val)
{
	context->buffer.filled = val;
	context->buffer.data[context->buffer.filled] = 0;
}

static int vp_pbool(parser_context *context, void *addr, const char *token)
{
	char *strtrue[] = { "ok", "on", "yes", "true" };
	char *strfalse[] = { "off", "no", "false" };
	char **tpl;

	FOREACH(tpl, strtrue)
		if (strcmp(token, *tpl) == 0) {
			*(bool*)addr = true;
			return 0;
		}

	FOREACH(tpl, strfalse)
		if (strcmp(token, *tpl) == 0) {
			*(bool*)addr = false;
			return 0;
		}

	parser_error(context, "boolean is not parsed");
	return -1;
}

static int vp_pchar(parser_context *context, void *addr, const char *token)
{
	char *p = strdup(token);
	if (!p) {
		parser_error(context, "strdup failed");
		return -1;
	}
	*(char**)addr = p;
	return 0;
}

static int vp_uint16(parser_context *context, void *addr, const char *token)
{
	char *end;
	unsigned long int uli = strtoul(token, &end, 0);
	if (uli > 0xFFFF) {
		parser_error(context, "integer out of 16bit range");
		return -1;
	}
	if (*end != '\0') {
		parser_error(context, "integer is not parsed");
		return -1;
	}
	*(uint16_t*)addr = (uint16_t)uli;
	return 0;
}

static int vp_in_addr(parser_context *context, void *addr, const char *token)
{
	struct in_addr ia;

	if (inet_aton(token, &ia)) {
		memcpy(addr, &ia, sizeof(ia));
	}
	else {
		struct addrinfo *ainfo, hints;
		int err;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET; /* IPv4-only */
		hints.ai_socktype = SOCK_STREAM; /* I want to have one address once and ONLY once, that's why I specify socktype and protocol */
		hints.ai_protocol = IPPROTO_TCP;
		hints.ai_flags = AI_ADDRCONFIG; /* I don't need IPv4 addrs without IPv4 connectivity */
		err = getaddrinfo(token, NULL, &hints, &ainfo);
		if (err == 0) {
			int count, taken;
			struct addrinfo *iter;
			struct sockaddr_in *resolved_addr;
			for (iter = ainfo, count = 0; iter; iter = iter->ai_next, ++count)
				;
			taken = rand() % count;
			for (iter = ainfo; taken > 0; iter = iter->ai_next, --taken)
				;
			resolved_addr = (struct sockaddr_in*)iter->ai_addr;
			assert(resolved_addr->sin_family == iter->ai_family && iter->ai_family == AF_INET);
			if (count != 1)
				log_error(LOG_WARNING, "%s resolves to %d addresses, using %s",
				          token, count, inet_ntoa(resolved_addr->sin_addr));
			memcpy(addr, &resolved_addr->sin_addr, sizeof(ia));
			freeaddrinfo(ainfo);
		}
		else {
			if (err == EAI_SYSTEM)
				parser_error(context, strerror(errno));
			else
				parser_error(context, gai_strerror(err));
			return -1;
		}
	}
	return 0;
}

static int vp_in_addr2(parser_context *context, void *addr, const char *token)
{
	char *host = NULL, *mask = NULL;
	struct in_addr ia;
	int retval = 0;

	host = strdup(token);
	if (!host) {
		parser_error(context, "strdup failed");
		return -1;
	}
	mask = strchr(host, '/');
	if (mask) {
		*mask = '\0';
		mask++;
	}

	if (inet_aton(host, &ia)) {
		memcpy(addr, &ia, sizeof(ia));
	}
	else {
		parser_error(context, "invalid IP address");
		retval = -1;
	}

	if (mask) {
		struct in_addr *pinmask = ((struct in_addr*)addr) + 1;
		char *end;
		unsigned long int uli = strtoul(mask, &end, 0);;
		if (*end == '.') {
			if (inet_aton(mask, &ia)) {
				memcpy(pinmask , &ia, sizeof(ia));
			}
			else {
				parser_error(context, "invalid IP address");
				retval = -1;
			}
		}
		else if (0 < uli && uli < 32) {
			pinmask->s_addr = htonl((INADDR_BROADCAST << (32 - uli)));
		}
		else {
			parser_error(context, "number of netmask bits out of range");
			retval = -1;
		}
	}

	free(host);
	return retval;
}

static value_parser value_parser_by_type[] =
{
	[pt_bool] = vp_pbool,
	[pt_pchar] = vp_pchar,
	[pt_uint16] = vp_uint16,
	[pt_in_addr] = vp_in_addr,
	[pt_in_addr2] = vp_in_addr2,
};

int parser_run(parser_context *context)
{
	char *section_token = NULL, *key_token = NULL, *value_token = NULL;
	parser_section *section = NULL;
	bool in_comment = false;
	bool need_more_space = false;
	bool need_more_data  = true;
	while ( !context->error && !feof(context->fd) ) {
		assert(context->buffer.filled < context->buffer.size); // ``<`` and not ``<=``

		if (need_more_space) {
			char *new = realloc(context->buffer.data, context->buffer.size * 2);
			if (!new) {
				parser_error(context, "realloc failure");
				return -1;
			}
			context->buffer.data = new;
			context->buffer.size *= 2;
			need_more_space = false;
		}

		if (need_more_data) { // read one line per call
			char *sbegin = context->buffer.data + context->buffer.filled;
			int len;
			if (fgets(sbegin, context->buffer.size - context->buffer.filled, context->fd) == NULL) {
				if (ferror(context->fd)) {
					parser_error(context, "file read failure");
					return -1;
				}
				else
					continue;
			}
			len = strlen(sbegin);
			context_filled_add(context, +len);
			if (len > 0 && sbegin[len - 1] == '\n') {
				context->line++;
				need_more_data = false;
			}
			else {
				need_more_space = true;
				continue;
			}
		}

		if ( in_comment ) {
			char *endc = strstr(context->buffer.data, "*/");
			if (endc) {
				endc += 2;
				int comment_len = endc - context->buffer.data;
				memmove(context->buffer.data, endc, context->buffer.filled - comment_len);
				context_filled_add(context, -comment_len);
				in_comment = false;
			}
			else {
				context_filled_set(context, 0);
				need_more_data = true;
				continue;
			}
		}

		char *token = NULL, *iter = context->buffer.data;
		while ( !context->error && !in_comment && (token = gettoken(context, &iter)) ) {
			if (strcmp(token, "//") == 0) {
				char *endc = strchr(iter, '\n');
				iter -= 2;
				endc += 1;
				// |*data          |*iter        |*endc       -->|<--.filled
				int moved_len = context->buffer.filled - (endc - context->buffer.data);
				memmove(iter, endc, moved_len);
				context_filled_add(context, -(endc - iter));
			}
			else if (strcmp(token, "/*") == 0) {
				int moved_len = iter - context->buffer.data;
				memmove(context->buffer.data, iter, context->buffer.filled - moved_len);
				context_filled_add(context, -moved_len);
				iter = context->buffer.data;
				in_comment = true;
			}
			else if (strcmp(token, "{") == 0) { // } - I love folding
				if (section) {
					parser_error(context, "section-in-section is invalid");
				}
				else if (!section_token) {
					parser_error(context, "expected token before ``{''"); // } - I love folding
				}
				else {
					for (parser_section *p = context->sections; p; p = p->next) {
						if (strcmp(p->name, section_token) == 0) {
							section = p;
							break;
						}
					}
					if (section) {
						if (section->onenter)
							if ( section->onenter(section) == -1 )
								parser_error(context, "section->onenter failed");
					}
					else {
						parser_error(context, "unknown section");
					}
					FREE(section_token);
				}
			}
			else if (strcmp(token, "}") == 0) { // { - I love folding
				if (section) {
					if (section->onexit)
						if ( section->onexit(section) == -1 )
							parser_error(context, "section->onexit failed");
					section = NULL;
				}
				else {
					parser_error(context, "can't close non-opened section");
				}
			}
			else if (strcmp(token, "=") == 0) {
				if (!section) {
					parser_error(context, "assignment used outside of any section");
				}
				else if (!key_token) {
					parser_error(context, "assignment used without key");
				}
				else {
					; // What can I do? :)
				}
			}
			else if (strcmp(token, ";") == 0) {
				if (!section) {
					parser_error(context, "assignment termination outside of any section");
				}
				else if (key_token && !value_token) {
					parser_error(context, "assignment has only key but no value");
				}
				else if (key_token && value_token) {
					parser_entry *e;
					for (e = section->entries; e->key; e++)
						if (strcmp(e->key, key_token) == 0)
							break;
					if (e) {
						if ( (value_parser_by_type[e->type])(context, e->addr, value_token) == -1 )
							parser_error(context, "value can't be parsed");
					}
					else {
						parser_error(context, "assignment with unknown key");
					}
				}
				else {
					assert(false);
				}
				FREE(key_token);
				FREE(value_token);
			}
			else {
				if (!section && !section_token) {
					section_token = token;
					token = 0;
				}
				else if (section && !key_token) {
					key_token = token;
					token = 0;
				}
				else if (section && key_token && !value_token) {
					value_token = token;
					token = 0;
				}
				else {
					parser_error(context, "invalid token order");
				}
			}
			free(token);
		}
		int moved_len = iter - context->buffer.data;
		memmove(context->buffer.data, iter, context->buffer.filled - moved_len);
		context_filled_add(context, -moved_len);

		if (!token)
			need_more_data = true;
	}
	// file-error goes is thrown here
	if (section)
		parser_error(context, "unclosed section");
	if (section_token) {
		parser_error(context, "stale section_token");
		free(section_token);
	}
	if (key_token) {
		parser_error(context, "stale key_token");
		free(key_token);
	}
	if (value_token) {
		parser_error(context, "stale value_token");
		free(value_token);
	}
	return context->error ? -1 : 0;
}


/* vim:set tabstop=4 softtabstop=4 shiftwidth=4: */
/* vim:set foldmethod=marker foldlevel=32 foldmarker={,}: */
