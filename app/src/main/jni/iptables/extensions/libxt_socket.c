/*
 * Shared library add-on to iptables to add early socket matching support.
 *
 * Copyright (C) 2007 BalaBit IT Ltd.
 */
#include <stdio.h>
#include <xtables.h>
#include <linux/netfilter/xt_socket.h>

enum {
	O_TRANSPARENT = 0,
};

static const struct xt_option_entry socket_mt_opts[] = {
	{.name = "transparent", .id = O_TRANSPARENT, .type = XTTYPE_NONE},
	XTOPT_TABLEEND,
};

static void socket_mt_help(void)
{
	printf(
		"socket match options:\n"
		"  --transparent    Ignore non-transparent sockets\n\n");
}

static void socket_mt_parse(struct xt_option_call *cb)
{
	struct xt_socket_mtinfo1 *info = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_TRANSPARENT:
		info->flags |= XT_SOCKET_TRANSPARENT;
		break;
	}
}

static void
socket_mt_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_socket_mtinfo1 *info = (const void *)match->data;

	if (info->flags & XT_SOCKET_TRANSPARENT)
		printf(" --transparent");
}

static void
socket_mt_print(const void *ip, const struct xt_entry_match *match,
		int numeric)
{
	printf(" socket");
	socket_mt_save(ip, match);
}

static struct xtables_match socket_mt_reg[] = {
	{
		.name          = "socket",
		.revision      = 0,
		.family        = NFPROTO_IPV4,
		.version       = XTABLES_VERSION,
		.size          = XT_ALIGN(0),
		.userspacesize = XT_ALIGN(0),
	},
	{
		.name          = "socket",
		.revision      = 1,
		.family        = NFPROTO_UNSPEC,
		.version       = XTABLES_VERSION,
		.size          = XT_ALIGN(sizeof(struct xt_socket_mtinfo1)),
		.userspacesize = XT_ALIGN(sizeof(struct xt_socket_mtinfo1)),
		.help          = socket_mt_help,
		.print         = socket_mt_print,
		.save          = socket_mt_save,
		.x6_parse      = socket_mt_parse,
		.x6_options    = socket_mt_opts,
	},
};

void _init(void)
{
	xtables_register_matches(socket_mt_reg, ARRAY_SIZE(socket_mt_reg));
}
