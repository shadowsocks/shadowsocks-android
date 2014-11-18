/*
 * Shared library add-on to iptables to add tos match support
 *
 * Copyright © CC Computer Consultants GmbH, 2007
 * Contact: Jan Engelhardt <jengelh@computergmbh.de>
 */
#include <getopt.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xtables.h>
#include <linux/netfilter/xt_dscp.h>
#include "tos_values.c"

struct ipt_tos_info {
	uint8_t tos;
	uint8_t invert;
};

enum {
	O_TOS = 1 << 0,
};

static const struct xt_option_entry tos_mt_opts_v0[] = {
	{.name = "tos", .id = O_TOS, .type = XTTYPE_TOSMASK,
	 .flags = XTOPT_INVERT | XTOPT_MAND, .max = 0xFF},
	XTOPT_TABLEEND,
};

static const struct xt_option_entry tos_mt_opts[] = {
	{.name = "tos", .id = O_TOS, .type = XTTYPE_TOSMASK,
	 .flags = XTOPT_INVERT | XTOPT_MAND, .max = 0x3F},
	XTOPT_TABLEEND,
};

static void tos_mt_help(void)
{
	const struct tos_symbol_info *symbol;

	printf(
"tos match options:\n"
"[!] --tos value[/mask]    Match Type of Service/Priority field value\n"
"[!] --tos symbol          Match TOS field (IPv4 only) by symbol\n"
"                          Accepted symbolic names for value are:\n");

	for (symbol = tos_symbol_names; symbol->name != NULL; ++symbol)
		printf("                          (0x%02x) %2u %s\n",
		       symbol->value, symbol->value, symbol->name);

	printf("\n");
}

static void tos_mt_parse_v0(struct xt_option_call *cb)
{
	struct ipt_tos_info *info = cb->data;

	xtables_option_parse(cb);
	if (cb->val.tos_mask != 0xFF)
		xtables_error(PARAMETER_PROBLEM, "tos: Your kernel is "
		           "too old to support anything besides /0xFF "
			   "as a mask.");
	info->tos = cb->val.tos_value;
	if (cb->invert)
		info->invert = true;
}

static void tos_mt_parse(struct xt_option_call *cb)
{
	struct xt_tos_match_info *info = cb->data;

	xtables_option_parse(cb);
	info->tos_value = cb->val.tos_value;
	info->tos_mask  = cb->val.tos_mask;
	if (cb->invert)
		info->invert = true;
}

static void tos_mt_print_v0(const void *ip, const struct xt_entry_match *match,
                            int numeric)
{
	const struct ipt_tos_info *info = (const void *)match->data;

	printf(" tos match ");
	if (info->invert)
		printf("!");
	if (numeric || !tos_try_print_symbolic("", info->tos, 0x3F))
		printf("0x%02x", info->tos);
}

static void tos_mt_print(const void *ip, const struct xt_entry_match *match,
                         int numeric)
{
	const struct xt_tos_match_info *info = (const void *)match->data;

	printf(" tos match");
	if (info->invert)
		printf("!");
	if (numeric ||
	    !tos_try_print_symbolic("", info->tos_value, info->tos_mask))
		printf("0x%02x/0x%02x", info->tos_value, info->tos_mask);
}

static void tos_mt_save_v0(const void *ip, const struct xt_entry_match *match)
{
	const struct ipt_tos_info *info = (const void *)match->data;

	if (info->invert)
		printf(" !");
	printf(" --tos 0x%02x", info->tos);
}

static void tos_mt_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_tos_match_info *info = (const void *)match->data;

	if (info->invert)
		printf(" !");
	printf(" --tos 0x%02x/0x%02x", info->tos_value, info->tos_mask);
}

static struct xtables_match tos_mt_reg[] = {
	{
		.version       = XTABLES_VERSION,
		.name          = "tos",
		.family        = NFPROTO_IPV4,
		.revision      = 0,
		.size          = XT_ALIGN(sizeof(struct ipt_tos_info)),
		.userspacesize = XT_ALIGN(sizeof(struct ipt_tos_info)),
		.help          = tos_mt_help,
		.print         = tos_mt_print_v0,
		.save          = tos_mt_save_v0,
		.x6_parse      = tos_mt_parse_v0,
		.x6_options    = tos_mt_opts_v0,
	},
	{
		.version       = XTABLES_VERSION,
		.name          = "tos",
		.family        = NFPROTO_UNSPEC,
		.revision      = 1,
		.size          = XT_ALIGN(sizeof(struct xt_tos_match_info)),
		.userspacesize = XT_ALIGN(sizeof(struct xt_tos_match_info)),
		.help          = tos_mt_help,
		.print         = tos_mt_print,
		.save          = tos_mt_save,
		.x6_parse      = tos_mt_parse,
		.x6_options    = tos_mt_opts,
	},
};

void _init(void)
{
	xtables_register_matches(tos_mt_reg, ARRAY_SIZE(tos_mt_reg));
}
