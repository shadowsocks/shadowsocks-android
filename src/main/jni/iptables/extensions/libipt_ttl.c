/* Shared library add-on to iptables to add TTL matching support 
 * (C) 2000 by Harald Welte <laforge@gnumonks.org>
 *
 * This program is released under the terms of GNU GPL */
#include <stdio.h>
#include <xtables.h>
#include <linux/netfilter_ipv4/ipt_ttl.h>

enum {
	O_TTL_EQ = 0,
	O_TTL_LT,
	O_TTL_GT,
	F_TTL_EQ = 1 << O_TTL_EQ,
	F_TTL_LT = 1 << O_TTL_LT,
	F_TTL_GT = 1 << O_TTL_GT,
	F_ANY    = F_TTL_EQ | F_TTL_LT | F_TTL_GT,
};

static void ttl_help(void)
{
	printf(
"ttl match options:\n"
"  --ttl-eq value	Match time to live value\n"
"  --ttl-lt value	Match TTL < value\n"
"  --ttl-gt value	Match TTL > value\n");
}

static void ttl_parse(struct xt_option_call *cb)
{
	struct ipt_ttl_info *info = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_TTL_EQ:
		info->mode = cb->invert ? IPT_TTL_NE : IPT_TTL_EQ;
		break;
	case O_TTL_LT:
		info->mode = IPT_TTL_LT;
		break;
	case O_TTL_GT:
		info->mode = IPT_TTL_GT;
		break;
	}
}

static void ttl_check(struct xt_fcheck_call *cb)
{
	if (!(cb->xflags & F_ANY))
		xtables_error(PARAMETER_PROBLEM,
			"TTL match: You must specify one of "
			"`--ttl-eq', `--ttl-lt', `--ttl-gt");
}

static void ttl_print(const void *ip, const struct xt_entry_match *match,
                      int numeric)
{
	const struct ipt_ttl_info *info = 
		(struct ipt_ttl_info *) match->data;

	printf(" TTL match ");
	switch (info->mode) {
		case IPT_TTL_EQ:
			printf("TTL ==");
			break;
		case IPT_TTL_NE:
			printf("TTL !=");
			break;
		case IPT_TTL_LT:
			printf("TTL <");
			break;
		case IPT_TTL_GT:
			printf("TTL >");
			break;
	}
	printf(" %u", info->ttl);
}

static void ttl_save(const void *ip, const struct xt_entry_match *match)
{
	const struct ipt_ttl_info *info =
		(struct ipt_ttl_info *) match->data;

	switch (info->mode) {
		case IPT_TTL_EQ:
			printf(" --ttl-eq");
			break;
		case IPT_TTL_NE:
			printf(" ! --ttl-eq");
			break;
		case IPT_TTL_LT:
			printf(" --ttl-lt");
			break;
		case IPT_TTL_GT:
			printf(" --ttl-gt");
			break;
		default:
			/* error */
			break;
	}
	printf(" %u", info->ttl);
}

#define s struct ipt_ttl_info
static const struct xt_option_entry ttl_opts[] = {
	{.name = "ttl-lt", .id = O_TTL_LT, .excl = F_ANY, .type = XTTYPE_UINT8,
	 .flags = XTOPT_PUT, XTOPT_POINTER(s, ttl)},
	{.name = "ttl-gt", .id = O_TTL_GT, .excl = F_ANY, .type = XTTYPE_UINT8,
	 .flags = XTOPT_PUT, XTOPT_POINTER(s, ttl)},
	{.name = "ttl-eq", .id = O_TTL_EQ, .excl = F_ANY, .type = XTTYPE_UINT8,
	 .flags = XTOPT_INVERT | XTOPT_PUT, XTOPT_POINTER(s, ttl)},
	{.name = "ttl", .id = O_TTL_EQ, .excl = F_ANY, .type = XTTYPE_UINT8,
	 .flags = XTOPT_PUT, XTOPT_POINTER(s, ttl)},
	XTOPT_TABLEEND,
};
#undef s

static struct xtables_match ttl_mt_reg = {
	.name		= "ttl",
	.version	= XTABLES_VERSION,
	.family		= NFPROTO_IPV4,
	.size		= XT_ALIGN(sizeof(struct ipt_ttl_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct ipt_ttl_info)),
	.help		= ttl_help,
	.print		= ttl_print,
	.save		= ttl_save,
	.x6_parse	= ttl_parse,
	.x6_fcheck	= ttl_check,
	.x6_options	= ttl_opts,
};


void _init(void) 
{
	xtables_register_match(&ttl_mt_reg);
}
