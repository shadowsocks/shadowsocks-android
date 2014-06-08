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
 * http-auth library, provide basic and digest scheme
 *	 see RFC 2617 for details 
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "md5.h"
#include "base64.h"

#include "http-auth.h"

char* basic_authentication_encode(const char *user, const char *passwd)
{
	/* prepare the user:pass key pair */
	int pair_len = strlen(user) + 1 + strlen(passwd);
	char *pair_ptr = calloc(pair_len + 1, 1);

	sprintf(pair_ptr, "%s:%s", user, passwd);

	/* calculate the final string length */
	int basic_len = BASE64_SIZE(pair_len);
	char *basic_ptr = calloc(basic_len + 1, 1);

	if (!base64_encode(basic_ptr, basic_len, (const uint8_t*)pair_ptr, pair_len))
		return NULL;

	return basic_ptr;
}

#define MD5_HASHLEN (16)

static void dump_hash(char *buf, const unsigned char *hash) 
{
	int i;

	for (i = 0; i < MD5_HASHLEN; i++) {
		buf += sprintf(buf, "%02x", hash[i]);
	}

	*buf = 0;
}

typedef struct {
	const char *b, *e;
} param_token;

/* Extract a parameter from the string (typically an HTTP header) at
   **SOURCE and advance SOURCE to the next parameter.  Return false
   when there are no more parameters to extract.  The name of the
   parameter is returned in NAME, and the value in VALUE.  If the
   parameter has no value, the token's b==e.*/

static int extract_param(const char **source, param_token *name, param_token *value, char separator)
{
	const char *p = *source;

	while (isspace (*p)) 
		++p;

	if (!*p)
	{
		*source = p;
		return 0;             /* no error; nothing more to extract */
	}

	/* Extract name. */
	name->b = p;
	while (*p && !isspace (*p) && *p != '=' && *p != separator) 
		++p;
	name->e = p;
	if (name->b == name->e)
		return 0;               /* empty name: error */

	while (isspace (*p)) 
		++p;
	if (*p == separator || !*p)           /* no value */
	{
		value->b = value->e = "";
		if (*p == separator) ++p;
		*source = p;
		return 1;
	}
	if (*p != '=')
		return 0;               /* error */

	/* *p is '=', extract value */
	++p;
	while (isspace (*p)) ++p;
	if (*p == '"')                /* quoted */
	{
		value->b = ++p;
		while (*p && *p != '"') ++p;
		if (!*p)
			return 0;
		value->e = p++;
		/* Currently at closing quote; find the end of param. */
		while (isspace (*p)) ++p;
		while (*p && *p != separator) ++p;
		if (*p == separator)
			++p;
		else if (*p)
			/* garbage after closed quote, e.g. foo="bar"baz */
			return 0;
	}
	else                          /* unquoted */
	{
		value->b = p;
		while (*p && *p != separator) ++p;
		value->e = p;
		while (value->e != value->b && isspace (value->e[-1]))
			--value->e;
		if (*p == separator) ++p;
	}
	*source = p;
	return 1;

}

char* digest_authentication_encode(const char *line, const char *user, const char *passwd, 
		const char *method, const char *path, int count, const char *cnonce)
{
	char *realm = NULL, *opaque = NULL, *nonce = NULL, *qop = NULL;
	char nc[9];
	sprintf(nc, "%08x", count);

	const char *ptr = line;
	param_token name, value;

	while (extract_param(&ptr, &name, &value, ',')) {
		int namelen = name.e - name.b;
		int valuelen = value.e - value.b;
		
		if (strncasecmp(name.b, "realm" , namelen) == 0) {
			strncpy(realm  = calloc(valuelen + 1, 1), value.b, valuelen);
			realm[valuelen] = '\0';
		}
		else if (strncasecmp(name.b, "opaque", namelen) == 0) {
			strncpy(opaque = calloc(valuelen + 1, 1), value.b, valuelen);
			opaque[valuelen] = '\0';
		}
		else if (strncasecmp(name.b, "nonce" , namelen) == 0) {
			strncpy(nonce  = calloc(valuelen + 1, 1), value.b, valuelen);
			nonce[valuelen] = '\0';
		}
		else if (strncasecmp(name.b, "qop"   , namelen) == 0) {
			strncpy(qop    = calloc(valuelen + 1, 1), value.b, valuelen);
			qop[valuelen] = '\0';
		}
	}

	if (!realm || !nonce || !user || !passwd || !path || !method) {
		free(realm);
		free(opaque);
		free(nonce);
		free(qop);
		return NULL;
	}

	if (qop && strncasecmp(qop, "auth", 5) != 0) {
		/* FIXME: currently don't support auth-int, only "auth" is supported */
		free(realm);
		free(opaque);
		free(nonce);
		free(qop);
		return NULL;
	}

	/* calculate the digest value */
	md5_state_t ctx;
	md5_byte_t hash[MD5_HASHLEN];
	char a1buf[MD5_HASHLEN * 2 + 1], a2buf[MD5_HASHLEN * 2 + 1];
	char response[MD5_HASHLEN * 2 + 1];

	/* A1 = username-value ":" realm-value ":" passwd */
	md5_init(&ctx);
	md5_append(&ctx, (md5_byte_t*)user, strlen(user));
	md5_append(&ctx, (md5_byte_t*)":", 1);
	md5_append(&ctx, (md5_byte_t*)realm, strlen(realm));
	md5_append(&ctx, (md5_byte_t*)":", 1);
	md5_append(&ctx, (md5_byte_t*)passwd, strlen(passwd));
	md5_finish(&ctx, hash);
	dump_hash(a1buf, hash);

	/* A2 = Method ":" digest-uri-value */
	md5_init(&ctx);
	md5_append(&ctx, (md5_byte_t*)method, strlen(method));
	md5_append(&ctx, (md5_byte_t*)":", 1);
	md5_append(&ctx, (md5_byte_t*)path, strlen(path));
	md5_finish(&ctx, hash);
	dump_hash(a2buf, hash);

	/* qop set: request-digest = H(A1) ":" nonce-value ":" nc-value ":" cnonce-value ":" qop-value ":" H(A2) */
	/* not set: request-digest = H(A1) ":" nonce-value ":" H(A2) */
	md5_init(&ctx);
	md5_append(&ctx, (md5_byte_t*)a1buf, strlen(a1buf));
	md5_append(&ctx, (md5_byte_t*)":", 1);
	md5_append(&ctx, (md5_byte_t*)nonce, strlen(nonce));
	md5_append(&ctx, (md5_byte_t*)":", 1);
	if (qop) {
		md5_append(&ctx, (md5_byte_t*)nc, strlen(nc));
		md5_append(&ctx, (md5_byte_t*)":", 1);
		md5_append(&ctx, (md5_byte_t*)cnonce, strlen(cnonce));
		md5_append(&ctx, (md5_byte_t*)":", 1);
		md5_append(&ctx, (md5_byte_t*)qop, strlen(qop));
		md5_append(&ctx, (md5_byte_t*)":", 1);
	}
	md5_append(&ctx, (md5_byte_t*)a2buf, strlen(a2buf));
	md5_finish(&ctx, hash);
	dump_hash(response, hash);

	/* prepare the final string */
	int len = 256;
	len += strlen(user);
	len += strlen(realm);
	len += strlen(nonce);
	len += strlen(path);
	len += strlen(response);

	if (qop) {
		len += strlen(qop);
		len += strlen(nc);
		len += strlen(cnonce);
	}

	if (opaque) {
		len += strlen(opaque);
	}

	char *res = (char*)malloc(len);
	if (!qop) {
		sprintf(res, "username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", response=\"%s\"", 
				user, realm, nonce, path, response);
	} else {
		sprintf(res, "username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", response=\"%s\", qop=%s, nc=%s, cnonce=\"%s\"",
				user, realm, nonce, path, response, qop, nc, cnonce);
	}

	if (opaque) {
		char *p = res + strlen(res);
		strcat (p, ", opaque=\"");
		strcat (p, opaque);
		strcat (p, "\"");
	}

	free(realm);
	free(opaque);
	free(nonce);
	free(qop);
	return res;
}

const char *auth_request_header = "Proxy-Authenticate:";
const char *auth_response_header = "Proxy-Authorization:";
