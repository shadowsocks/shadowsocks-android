/*
 *	"quota2" match extension for iptables
 *	Sam Johnston <samj [at] samj net>
 *	Jan Engelhardt <jengelh [at] medozas de>, 2008
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License; either
 *	version 2 of the License, or any later version, as published by the
 *	Free Software Foundation.
 */
#include <getopt.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xtables.h>
#include <linux/netfilter/xt_quota2.h>

enum {
	FL_QUOTA     = 1 << 0,
	FL_NAME      = 1 << 1,
	FL_GROW      = 1 << 2,
	FL_PACKET    = 1 << 3,
	FL_NO_CHANGE = 1 << 4,
};

enum {
	O_QUOTA     = 0,
	O_NAME,
	O_GROW,
	O_PACKET,
	O_NO_CHANGE,
};


static const struct xt_option_entry quota_mt2_opts[] = {
	{.name = "grow", .id = O_GROW, .type = XTTYPE_NONE},
	{.name = "no-change", .id = O_NO_CHANGE, .type = XTTYPE_NONE},
	{.name = "name", .id = O_NAME, .type = XTTYPE_STRING,
	 .flags = XTOPT_PUT, XTOPT_POINTER(struct xt_quota_mtinfo2, name)},
	{.name = "quota", .id = O_QUOTA, .type = XTTYPE_UINT64,
	 .flags = XTOPT_INVERT | XTOPT_PUT,
	 XTOPT_POINTER(struct xt_quota_mtinfo2, quota)},
	{.name = "packets", .id = O_PACKET, .type = XTTYPE_NONE},
	XTOPT_TABLEEND,
};

static void quota_mt2_help(void)
{
	printf(
	"quota match options:\n"
	"    --grow           provide an increasing counter\n"
	"    --no-change      never change counter/quota value for matching packets\n"
	"    --name name      name for the file in sysfs\n"
	"[!] --quota quota    initial quota (bytes or packets)\n"
	"    --packets        count packets instead of bytes\n"
	);
}

static void quota_mt2_parse(struct xt_option_call *cb)
{
	struct xt_quota_mtinfo2 *info = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_GROW:
		info->flags |= XT_QUOTA_GROW;
		break;
	case O_NO_CHANGE:
		info->flags |= XT_QUOTA_NO_CHANGE;
		break;
	case O_NAME:
		break;
	case O_PACKET:
		info->flags |= XT_QUOTA_PACKET;
		break;
	case O_QUOTA:
		if (cb->invert)
			info->flags |= XT_QUOTA_INVERT;
		break;
	}
}

static void
quota_mt2_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_quota_mtinfo2 *q = (void *)match->data;

	if (q->flags & XT_QUOTA_INVERT)
		printf(" !");
	if (q->flags & XT_QUOTA_GROW)
		printf(" --grow ");
	if (q->flags & XT_QUOTA_NO_CHANGE)
		printf(" --no-change ");
	if (q->flags & XT_QUOTA_PACKET)
		printf(" --packets ");
	if (*q->name != '\0')
		printf(" --name %s ", q->name);
	printf(" --quota %llu ", (unsigned long long)q->quota);
}

static void quota_mt2_print(const void *ip, const struct xt_entry_match *match,
                            int numeric)
{
	const struct xt_quota_mtinfo2 *q = (const void *)match->data;

	if (q->flags & XT_QUOTA_INVERT)
		printf(" !");
	if (q->flags & XT_QUOTA_GROW)
		printf(" counter");
	else
		printf(" quota");
	if (*q->name != '\0')
		printf(" %s:", q->name);
	printf(" %llu ", (unsigned long long)q->quota);
	if (q->flags & XT_QUOTA_PACKET)
		printf("packets ");
	else
		printf("bytes ");
	if (q->flags & XT_QUOTA_NO_CHANGE)
		printf("(no-change mode) ");
}

static struct xtables_match quota_mt2_reg = {
	.family        = NFPROTO_UNSPEC,
	.revision      = 3,
	.name          = "quota2",
	.version       = XTABLES_VERSION,
	.size          = XT_ALIGN(sizeof (struct xt_quota_mtinfo2)),
	.userspacesize = offsetof(struct xt_quota_mtinfo2, quota),
	.help          = quota_mt2_help,
	.x6_parse      = quota_mt2_parse,
	.print         = quota_mt2_print,
	.save          = quota_mt2_save,
	.x6_options    = quota_mt2_opts,
};

void _init(void)
{
	xtables_register_match(&quota_mt2_reg);
}
