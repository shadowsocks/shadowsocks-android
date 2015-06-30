/* Shared library add-on to ip6tables to add customized REJECT support.
 *
 * (C) 2000 Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>
 * 
 * ported to IPv6 by Harald Welte <laforge@gnumonks.org>
 *
 */
#include <stdio.h>
#include <string.h>
#include <xtables.h>
#include <linux/netfilter_ipv6/ip6t_REJECT.h>

struct reject_names {
	const char *name;
	const char *alias;
	enum ip6t_reject_with with;
	const char *desc;
};

enum {
	O_REJECT_WITH = 0,
};

static const struct reject_names reject_table[] = {
	{"icmp6-no-route", "no-route",
		IP6T_ICMP6_NO_ROUTE, "ICMPv6 no route"},
	{"icmp6-adm-prohibited", "adm-prohibited",
		IP6T_ICMP6_ADM_PROHIBITED, "ICMPv6 administratively prohibited"},
#if 0
	{"icmp6-not-neighbor", "not-neighbor"},
		IP6T_ICMP6_NOT_NEIGHBOR, "ICMPv6 not a neighbor"},
#endif
	{"icmp6-addr-unreachable", "addr-unreach",
		IP6T_ICMP6_ADDR_UNREACH, "ICMPv6 address unreachable"},
	{"icmp6-port-unreachable", "port-unreach",
		IP6T_ICMP6_PORT_UNREACH, "ICMPv6 port unreachable"},
	{"tcp-reset", "tcp-reset",
		IP6T_TCP_RESET, "TCP RST packet"}
};

static void
print_reject_types(void)
{
	unsigned int i;

	printf("Valid reject types:\n");

	for (i = 0; i < ARRAY_SIZE(reject_table); ++i) {
		printf("    %-25s\t%s\n", reject_table[i].name, reject_table[i].desc);
		printf("    %-25s\talias\n", reject_table[i].alias);
	}
	printf("\n");
}

static void REJECT_help(void)
{
	printf(
"REJECT target options:\n"
"--reject-with type              drop input packet and send back\n"
"                                a reply packet according to type:\n");

	print_reject_types();
}

static const struct xt_option_entry REJECT_opts[] = {
	{.name = "reject-with", .id = O_REJECT_WITH, .type = XTTYPE_STRING},
	XTOPT_TABLEEND,
};

static void REJECT_init(struct xt_entry_target *t)
{
	struct ip6t_reject_info *reject = (struct ip6t_reject_info *)t->data;

	/* default */
	reject->with = IP6T_ICMP6_PORT_UNREACH;

}

static void REJECT_parse(struct xt_option_call *cb)
{
	struct ip6t_reject_info *reject = cb->data;
	unsigned int i;

	xtables_option_parse(cb);
	for (i = 0; i < ARRAY_SIZE(reject_table); ++i)
		if (strncasecmp(reject_table[i].name,
		      cb->arg, strlen(cb->arg)) == 0 ||
		    strncasecmp(reject_table[i].alias,
		      cb->arg, strlen(cb->arg)) == 0) {
			reject->with = reject_table[i].with;
			return;
		}
	xtables_error(PARAMETER_PROBLEM,
		"unknown reject type \"%s\"", cb->arg);
}

static void REJECT_print(const void *ip, const struct xt_entry_target *target,
                         int numeric)
{
	const struct ip6t_reject_info *reject
		= (const struct ip6t_reject_info *)target->data;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(reject_table); ++i)
		if (reject_table[i].with == reject->with)
			break;
	printf(" reject-with %s", reject_table[i].name);
}

static void REJECT_save(const void *ip, const struct xt_entry_target *target)
{
	const struct ip6t_reject_info *reject
		= (const struct ip6t_reject_info *)target->data;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(reject_table); ++i)
		if (reject_table[i].with == reject->with)
			break;

	printf(" --reject-with %s", reject_table[i].name);
}

static struct xtables_target reject_tg6_reg = {
	.name = "REJECT",
	.version	= XTABLES_VERSION,
	.family		= NFPROTO_IPV6,
	.size 		= XT_ALIGN(sizeof(struct ip6t_reject_info)),
	.userspacesize 	= XT_ALIGN(sizeof(struct ip6t_reject_info)),
	.help		= REJECT_help,
	.init		= REJECT_init,
	.print		= REJECT_print,
	.save		= REJECT_save,
	.x6_parse	= REJECT_parse,
	.x6_options	= REJECT_opts,
};

void _init(void)
{
	xtables_register_target(&reject_tg6_reg);
}
