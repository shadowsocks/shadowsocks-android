/*
 *	"TEE" target extension for iptables
 *	Copyright © Sebastian Claßen <sebastian.classen [at] freenet.ag>, 2007
 *	Jan Engelhardt <jengelh [at] medozas de>, 2007 - 2010
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License; either
 *	version 2 of the License, or any later version, as published by the
 *	Free Software Foundation.
 */
#include <sys/socket.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>

#include <xtables.h>
#include <linux/netfilter.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_TEE.h>

enum {
	O_GATEWAY = 0,
	O_OIF,
};

#define s struct xt_tee_tginfo
static const struct xt_option_entry tee_tg_opts[] = {
	{.name = "gateway", .id = O_GATEWAY, .type = XTTYPE_HOST,
	 .flags = XTOPT_MAND | XTOPT_PUT, XTOPT_POINTER(s, gw)},
	{.name = "oif", .id = O_OIF, .type = XTTYPE_STRING,
	 .flags = XTOPT_PUT, XTOPT_POINTER(s, oif)},
	XTOPT_TABLEEND,
};
#undef s

static void tee_tg_help(void)
{
	printf(
"TEE target options:\n"
"  --gateway IPADDR    Route packet via the gateway given by address\n"
"  --oif NAME          Include oif in route calculation\n"
"\n");
}

static void tee_tg_print(const void *ip, const struct xt_entry_target *target,
                         int numeric)
{
	const struct xt_tee_tginfo *info = (const void *)target->data;

	if (numeric)
		printf(" TEE gw:%s", xtables_ipaddr_to_numeric(&info->gw.in));
	else
		printf(" TEE gw:%s", xtables_ipaddr_to_anyname(&info->gw.in));
	if (*info->oif != '\0')
		printf(" oif=%s", info->oif);
}

static void tee_tg6_print(const void *ip, const struct xt_entry_target *target,
                          int numeric)
{
	const struct xt_tee_tginfo *info = (const void *)target->data;

	if (numeric)
		printf(" TEE gw:%s", xtables_ip6addr_to_numeric(&info->gw.in6));
	else
		printf(" TEE gw:%s", xtables_ip6addr_to_anyname(&info->gw.in6));
	if (*info->oif != '\0')
		printf(" oif=%s", info->oif);
}

static void tee_tg_save(const void *ip, const struct xt_entry_target *target)
{
	const struct xt_tee_tginfo *info = (const void *)target->data;

	printf(" --gateway %s", xtables_ipaddr_to_numeric(&info->gw.in));
	if (*info->oif != '\0')
		printf(" --oif %s", info->oif);
}

static void tee_tg6_save(const void *ip, const struct xt_entry_target *target)
{
	const struct xt_tee_tginfo *info = (const void *)target->data;

	printf(" --gateway %s", xtables_ip6addr_to_numeric(&info->gw.in6));
	if (*info->oif != '\0')
		printf(" --oif %s", info->oif);
}

static struct xtables_target tee_tg_reg = {
	.name          = "TEE",
	.version       = XTABLES_VERSION,
	.revision      = 1,
	.family        = NFPROTO_IPV4,
	.size          = XT_ALIGN(sizeof(struct xt_tee_tginfo)),
	.userspacesize = XT_ALIGN(sizeof(struct xt_tee_tginfo)),
	.help          = tee_tg_help,
	.print         = tee_tg_print,
	.save          = tee_tg_save,
	.x6_parse      = xtables_option_parse,
	.x6_options    = tee_tg_opts,
};

static struct xtables_target tee_tg6_reg = {
	.name          = "TEE",
	.version       = XTABLES_VERSION,
	.revision      = 1,
	.family        = NFPROTO_IPV6,
	.size          = XT_ALIGN(sizeof(struct xt_tee_tginfo)),
	.userspacesize = XT_ALIGN(sizeof(struct xt_tee_tginfo)),
	.help          = tee_tg_help,
	.print         = tee_tg6_print,
	.save          = tee_tg6_save,
	.x6_parse      = xtables_option_parse,
	.x6_options    = tee_tg_opts,
};

void _init(void)
{
	xtables_register_target(&tee_tg_reg);
	xtables_register_target(&tee_tg6_reg);
}
