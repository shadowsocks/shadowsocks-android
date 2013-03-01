/*
 * IPv6 Hop Limit matching module
 * Maciej Soltysiak <solt@dns.toxicfilms.tv>
 * Based on HW's ttl match
 * This program is released under the terms of GNU GPL
 * Cleanups by Stephane Ouellette <ouellettes@videotron.ca>
 */
#include <stdio.h>
#include <xtables.h>
#include <linux/netfilter_ipv6/ip6t_hl.h>

enum {
	O_HL_EQ = 0,
	O_HL_LT,
	O_HL_GT,
	F_HL_EQ = 1 << O_HL_EQ,
	F_HL_LT = 1 << O_HL_LT,
	F_HL_GT = 1 << O_HL_GT,
	F_ANY  = F_HL_EQ | F_HL_LT | F_HL_GT,
};

static void hl_help(void)
{
	printf(
"hl match options:\n"
"[!] --hl-eq value	Match hop limit value\n"
"  --hl-lt value	Match HL < value\n"
"  --hl-gt value	Match HL > value\n");
}

static void hl_parse(struct xt_option_call *cb)
{
	struct ip6t_hl_info *info = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_HL_EQ:
		info->mode = cb->invert ? IP6T_HL_NE : IP6T_HL_EQ;
		break;
	case O_HL_LT:
		info->mode = IP6T_HL_LT;
		break;
	case O_HL_GT:
		info->mode = IP6T_HL_GT;
		break;
	}
}

static void hl_check(struct xt_fcheck_call *cb)
{
	if (!(cb->xflags & F_ANY))
		xtables_error(PARAMETER_PROBLEM,
			"HL match: You must specify one of "
			"`--hl-eq', `--hl-lt', `--hl-gt'");
}

static void hl_print(const void *ip, const struct xt_entry_match *match,
                     int numeric)
{
	static const char *const op[] = {
		[IP6T_HL_EQ] = "==",
		[IP6T_HL_NE] = "!=",
		[IP6T_HL_LT] = "<",
		[IP6T_HL_GT] = ">" };

	const struct ip6t_hl_info *info = 
		(struct ip6t_hl_info *) match->data;

	printf(" HL match HL %s %u", op[info->mode], info->hop_limit);
}

static void hl_save(const void *ip, const struct xt_entry_match *match)
{
	static const char *const op[] = {
		[IP6T_HL_EQ] = "--hl-eq",
		[IP6T_HL_NE] = "! --hl-eq",
		[IP6T_HL_LT] = "--hl-lt",
		[IP6T_HL_GT] = "--hl-gt" };

	const struct ip6t_hl_info *info =
		(struct ip6t_hl_info *) match->data;

	printf(" %s %u", op[info->mode], info->hop_limit);
}

#define s struct ip6t_hl_info
static const struct xt_option_entry hl_opts[] = {
	{.name = "hl-lt", .id = O_HL_LT, .excl = F_ANY, .type = XTTYPE_UINT8,
	 .flags = XTOPT_PUT, XTOPT_POINTER(s, hop_limit)},
	{.name = "hl-gt", .id = O_HL_GT, .excl = F_ANY, .type = XTTYPE_UINT8,
	 .flags = XTOPT_PUT, XTOPT_POINTER(s, hop_limit)},
	{.name = "hl-eq", .id = O_HL_EQ, .excl = F_ANY, .type = XTTYPE_UINT8,
	 .flags = XTOPT_INVERT | XTOPT_PUT, XTOPT_POINTER(s, hop_limit)},
	{.name = "hl", .id = O_HL_EQ, .excl = F_ANY, .type = XTTYPE_UINT8,
	 .flags = XTOPT_PUT, XTOPT_POINTER(s, hop_limit)},
	XTOPT_TABLEEND,
};
#undef s

static struct xtables_match hl_mt6_reg = {
	.name          = "hl",
	.version       = XTABLES_VERSION,
	.family        = NFPROTO_IPV6,
	.size          = XT_ALIGN(sizeof(struct ip6t_hl_info)),
	.userspacesize = XT_ALIGN(sizeof(struct ip6t_hl_info)),
	.help          = hl_help,
	.print         = hl_print,
	.save          = hl_save,
	.x6_parse      = hl_parse,
	.x6_fcheck     = hl_check,
	.x6_options    = hl_opts,
};


void _init(void) 
{
	xtables_register_match(&hl_mt6_reg);
}
