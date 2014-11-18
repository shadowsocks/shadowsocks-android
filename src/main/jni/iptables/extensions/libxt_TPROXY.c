/*
 * shared library add-on to iptables to add TPROXY target support.
 *
 * Copyright (C) 2002-2008 BalaBit IT Ltd.
 */
#include <stdio.h>
#include <limits.h>
#include <xtables.h>
#include <linux/netfilter/xt_TPROXY.h>
#include <arpa/inet.h>

enum {
	P_PORT = 0,
	P_ADDR,
	P_MARK,
	F_PORT = 1 << P_PORT,
	F_ADDR = 1 << P_ADDR,
	F_MARK = 1 << P_MARK,
};

#define s struct xt_tproxy_target_info
static const struct xt_option_entry tproxy_tg0_opts[] = {
	{.name = "on-port", .id = P_PORT, .type = XTTYPE_PORT,
	 .flags = XTOPT_MAND | XTOPT_NBO | XTOPT_PUT, XTOPT_POINTER(s, lport)},
	{.name = "on-ip", .id = P_ADDR, .type = XTTYPE_HOST},
	{.name = "tproxy-mark", .id = P_MARK, .type = XTTYPE_MARKMASK32},
	XTOPT_TABLEEND,
};
#undef s
#define s struct xt_tproxy_target_info_v1
static const struct xt_option_entry tproxy_tg1_opts[] = {
	{.name = "on-port", .id = P_PORT, .type = XTTYPE_PORT,
	 .flags = XTOPT_MAND | XTOPT_NBO | XTOPT_PUT, XTOPT_POINTER(s, lport)},
	{.name = "on-ip", .id = P_ADDR, .type = XTTYPE_HOST,
	 .flags = XTOPT_PUT, XTOPT_POINTER(s, laddr)},
	{.name = "tproxy-mark", .id = P_MARK, .type = XTTYPE_MARKMASK32},
	XTOPT_TABLEEND,
};
#undef s

static void tproxy_tg_help(void)
{
	printf(
"TPROXY target options:\n"
"  --on-port port		    Redirect connection to port, or the original port if 0\n"
"  --on-ip ip			    Optionally redirect to the given IP\n"
"  --tproxy-mark value[/mask]	    Mark packets with the given value/mask\n\n");
}

static void tproxy_tg_print(const void *ip, const struct xt_entry_target *target,
			 int numeric)
{
	const struct xt_tproxy_target_info *info = (const void *)target->data;
	printf(" TPROXY redirect %s:%u mark 0x%x/0x%x",
	       xtables_ipaddr_to_numeric((const struct in_addr *)&info->laddr),
	       ntohs(info->lport), (unsigned int)info->mark_value,
	       (unsigned int)info->mark_mask);
}

static void
tproxy_tg_print4(const void *ip, const struct xt_entry_target *target,
		 int numeric)
{
	const struct xt_tproxy_target_info_v1 *info =
		(const void *)target->data;

	printf(" TPROXY redirect %s:%u mark 0x%x/0x%x",
	       xtables_ipaddr_to_numeric(&info->laddr.in),
	       ntohs(info->lport), (unsigned int)info->mark_value,
	       (unsigned int)info->mark_mask);
}

static void
tproxy_tg_print6(const void *ip, const struct xt_entry_target *target,
		 int numeric)
{
	const struct xt_tproxy_target_info_v1 *info =
		(const void *)target->data;

	printf(" TPROXY redirect %s:%u mark 0x%x/0x%x",
	       xtables_ip6addr_to_numeric(&info->laddr.in6),
	       ntohs(info->lport), (unsigned int)info->mark_value,
	       (unsigned int)info->mark_mask);
}

static void tproxy_tg_save(const void *ip, const struct xt_entry_target *target)
{
	const struct xt_tproxy_target_info *info = (const void *)target->data;

	printf(" --on-port %u", ntohs(info->lport));
	printf(" --on-ip %s",
	       xtables_ipaddr_to_numeric((const struct in_addr *)&info->laddr));
	printf(" --tproxy-mark 0x%x/0x%x",
	       (unsigned int)info->mark_value, (unsigned int)info->mark_mask);
}

static void
tproxy_tg_save4(const void *ip, const struct xt_entry_target *target)
{
	const struct xt_tproxy_target_info_v1 *info;

	info = (const void *)target->data;
	printf(" --on-port %u", ntohs(info->lport));
	printf(" --on-ip %s", xtables_ipaddr_to_numeric(&info->laddr.in));
	printf(" --tproxy-mark 0x%x/0x%x",
	       (unsigned int)info->mark_value, (unsigned int)info->mark_mask);
}

static void
tproxy_tg_save6(const void *ip, const struct xt_entry_target *target)
{
	const struct xt_tproxy_target_info_v1 *info;

	info = (const void *)target->data;
	printf(" --on-port %u", ntohs(info->lport));
	printf(" --on-ip %s", xtables_ip6addr_to_numeric(&info->laddr.in6));
	printf(" --tproxy-mark 0x%x/0x%x",
	       (unsigned int)info->mark_value, (unsigned int)info->mark_mask);
}

static void tproxy_tg0_parse(struct xt_option_call *cb)
{
	struct xt_tproxy_target_info *info = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case P_MARK:
		info->mark_value = cb->val.mark;
		info->mark_mask  = cb->val.mask;
		break;
	case P_ADDR:
		info->laddr = cb->val.haddr.ip;
		break;
	}
}

static void tproxy_tg1_parse(struct xt_option_call *cb)
{
	struct xt_tproxy_target_info_v1 *info = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case P_MARK:
		info->mark_value = cb->val.mark;
		info->mark_mask  = cb->val.mask;
		break;
	}
}

static struct xtables_target tproxy_tg_reg[] = {
	{
		.name          = "TPROXY",
		.revision      = 0,
		.family        = NFPROTO_IPV4,
		.version       = XTABLES_VERSION,
		.size          = XT_ALIGN(sizeof(struct xt_tproxy_target_info)),
		.userspacesize = XT_ALIGN(sizeof(struct xt_tproxy_target_info)),
		.help          = tproxy_tg_help,
		.print         = tproxy_tg_print,
		.save          = tproxy_tg_save,
		.x6_options    = tproxy_tg0_opts,
		.x6_parse      = tproxy_tg0_parse,
	},
	{
		.name          = "TPROXY",
		.revision      = 1,
		.family        = NFPROTO_IPV4,
		.version       = XTABLES_VERSION,
		.size          = XT_ALIGN(sizeof(struct xt_tproxy_target_info_v1)),
		.userspacesize = XT_ALIGN(sizeof(struct xt_tproxy_target_info_v1)),
		.help          = tproxy_tg_help,
		.print         = tproxy_tg_print4,
		.save          = tproxy_tg_save4,
		.x6_options    = tproxy_tg1_opts,
		.x6_parse      = tproxy_tg1_parse,
	},
	{
		.name          = "TPROXY",
		.revision      = 1,
		.family        = NFPROTO_IPV6,
		.version       = XTABLES_VERSION,
		.size          = XT_ALIGN(sizeof(struct xt_tproxy_target_info_v1)),
		.userspacesize = XT_ALIGN(sizeof(struct xt_tproxy_target_info_v1)),
		.help          = tproxy_tg_help,
		.print         = tproxy_tg_print6,
		.save          = tproxy_tg_save6,
		.x6_options    = tproxy_tg1_opts,
		.x6_parse      = tproxy_tg1_parse,
	},
};

void _init(void)
{
	xtables_register_targets(tproxy_tg_reg, ARRAY_SIZE(tproxy_tg_reg));
}
