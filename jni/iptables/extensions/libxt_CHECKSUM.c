/* Shared library add-on to xtables for CHECKSUM
 *
 * (C) 2002 by Harald Welte <laforge@gnumonks.org>
 * (C) 2010 by Red Hat, Inc
 * Author: Michael S. Tsirkin <mst@redhat.com>
 *
 * This program is distributed under the terms of GNU GPL v2, 1991
 *
 * libxt_CHECKSUM.c borrowed some bits from libipt_ECN.c
 */
#include <stdio.h>
#include <xtables.h>
#include <linux/netfilter/xt_CHECKSUM.h>

enum {
	O_CHECKSUM_FILL = 0,
};

static void CHECKSUM_help(void)
{
	printf(
"CHECKSUM target options\n"
"  --checksum-fill			Fill in packet checksum.\n");
}

static const struct xt_option_entry CHECKSUM_opts[] = {
	{.name = "checksum-fill", .id = O_CHECKSUM_FILL,
	 .flags = XTOPT_MAND, .type = XTTYPE_NONE},
	XTOPT_TABLEEND,
};

static void CHECKSUM_parse(struct xt_option_call *cb)
{
	struct xt_CHECKSUM_info *einfo = cb->data;

	xtables_option_parse(cb);
	einfo->operation = XT_CHECKSUM_OP_FILL;
}

static void CHECKSUM_print(const void *ip, const struct xt_entry_target *target,
                      int numeric)
{
	const struct xt_CHECKSUM_info *einfo =
		(const struct xt_CHECKSUM_info *)target->data;

	printf(" CHECKSUM");

	if (einfo->operation & XT_CHECKSUM_OP_FILL)
		printf(" fill");
}

static void CHECKSUM_save(const void *ip, const struct xt_entry_target *target)
{
	const struct xt_CHECKSUM_info *einfo =
		(const struct xt_CHECKSUM_info *)target->data;

	if (einfo->operation & XT_CHECKSUM_OP_FILL)
		printf(" --checksum-fill");
}

static struct xtables_target checksum_tg_reg = {
	.name		= "CHECKSUM",
	.version	= XTABLES_VERSION,
	.family		= NFPROTO_UNSPEC,
	.size		= XT_ALIGN(sizeof(struct xt_CHECKSUM_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_CHECKSUM_info)),
	.help		= CHECKSUM_help,
	.print		= CHECKSUM_print,
	.save		= CHECKSUM_save,
	.x6_parse	= CHECKSUM_parse,
	.x6_options	= CHECKSUM_opts,
};

void _init(void)
{
	xtables_register_target(&checksum_tg_reg);
}
