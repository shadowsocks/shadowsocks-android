/*
 * Copyright (c) 2003+ Evgeniy Polyakov <zbr@ioremap.net>
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

/*
 * xtables interface for OS fingerprint matching module.
 */
#include <stdio.h>
#include <string.h>
#include <xtables.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <linux/netfilter/xt_osf.h>

enum {
	O_GENRE = 0,
	O_TTL,
	O_LOGLEVEL,
};

static void osf_help(void)
{
	printf("OS fingerprint match options:\n"
		"[!] --genre string     Match a OS genre by passive fingerprinting.\n"
		"--ttl level            Use some TTL check extensions to determine OS:\n"
		"       0                       true ip and fingerprint TTL comparison. Works for LAN.\n"
		"       1                       check if ip TTL is less than fingerprint one. Works for global addresses.\n"
		"       2                       do not compare TTL at all. Allows to detect NMAP, but can produce false results.\n"
		"--log level            Log determined genres into dmesg even if they do not match desired one:\n"
		"       0                       log all matched or unknown signatures.\n"
		"       1                       log only first one.\n"
		"       2                       log all known matched signatures.\n"
		);
}

#define s struct xt_osf_info
static const struct xt_option_entry osf_opts[] = {
	{.name = "genre", .id = O_GENRE, .type = XTTYPE_STRING,
	 .flags = XTOPT_MAND | XTOPT_INVERT | XTOPT_PUT,
	 XTOPT_POINTER(s, genre)},
	{.name = "ttl", .id = O_TTL, .type = XTTYPE_UINT32,
	 .flags = XTOPT_PUT, XTOPT_POINTER(s, ttl), .min = 0, .max = 2},
	{.name = "log", .id = O_LOGLEVEL, .type = XTTYPE_UINT32,
	 .flags = XTOPT_PUT, XTOPT_POINTER(s, loglevel), .min = 0, .max = 2},
	XTOPT_TABLEEND,
};
#undef s

static void osf_parse(struct xt_option_call *cb)
{
	struct xt_osf_info *info = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
		case O_GENRE:
			if (cb->invert)
				info->flags |= XT_OSF_INVERT;
			info->len = strlen(info->genre);
			break;
		case O_TTL:
			info->flags |= XT_OSF_TTL;
			break;
		case O_LOGLEVEL:
			info->flags |= XT_OSF_LOG;
			break;
	}
}

static void osf_print(const void *ip, const struct xt_entry_match *match, int numeric)
{
	const struct xt_osf_info *info = (const struct xt_osf_info*) match->data;

	printf(" OS fingerprint match %s%s", (info->flags & XT_OSF_INVERT) ? "! " : "", info->genre);
}

static void osf_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_osf_info *info = (const struct xt_osf_info*) match->data;

	printf(" --genre %s%s", (info->flags & XT_OSF_INVERT) ? "! ": "", info->genre);
}

static struct xtables_match osf_match = {
	.name		= "osf",
	.version	= XTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_osf_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_osf_info)),
	.help		= osf_help,
	.x6_parse	= osf_parse,
	.print		= osf_print,
	.save		= osf_save,
	.x6_options	= osf_opts,
	.family		= NFPROTO_IPV4,
};

void _init(void)
{
	xtables_register_match(&osf_match);
}
