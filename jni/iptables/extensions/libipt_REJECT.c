/* Shared library add-on to iptables to add customized REJECT support.
 *
 * (C) 2000 Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>
 */
#include <stdio.h>
#include <string.h>
#include <xtables.h>
#include <linux/netfilter_ipv4/ipt_REJECT.h>
#include <linux/version.h>

/* If we are compiling against a kernel that does not support
 * IPT_ICMP_ADMIN_PROHIBITED, we are emulating it.
 * The result will be a plain DROP of the packet instead of
 * reject. -- Maciej Soltysiak <solt@dns.toxicfilms.tv>
 */
#ifndef IPT_ICMP_ADMIN_PROHIBITED
#define IPT_ICMP_ADMIN_PROHIBITED	IPT_TCP_RESET + 1
#endif

struct reject_names {
	const char *name;
	const char *alias;
	enum ipt_reject_with with;
	const char *desc;
};

enum {
	O_REJECT_WITH = 0,
};

static const struct reject_names reject_table[] = {
	{"icmp-net-unreachable", "net-unreach",
		IPT_ICMP_NET_UNREACHABLE, "ICMP network unreachable"},
	{"icmp-host-unreachable", "host-unreach",
		IPT_ICMP_HOST_UNREACHABLE, "ICMP host unreachable"},
	{"icmp-proto-unreachable", "proto-unreach",
		IPT_ICMP_PROT_UNREACHABLE, "ICMP protocol unreachable"},
	{"icmp-port-unreachable", "port-unreach",
		IPT_ICMP_PORT_UNREACHABLE, "ICMP port unreachable (default)"},
#if 0
	{"echo-reply", "echoreply",
	 IPT_ICMP_ECHOREPLY, "for ICMP echo only: faked ICMP echo reply"},
#endif
	{"icmp-net-prohibited", "net-prohib",
	 IPT_ICMP_NET_PROHIBITED, "ICMP network prohibited"},
	{"icmp-host-prohibited", "host-prohib",
	 IPT_ICMP_HOST_PROHIBITED, "ICMP host prohibited"},
	{"tcp-reset", "tcp-rst",
	 IPT_TCP_RESET, "TCP RST packet"},
	{"icmp-admin-prohibited", "admin-prohib",
	 IPT_ICMP_ADMIN_PROHIBITED, "ICMP administratively prohibited (*)"}
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

	printf("(*) See man page or read the INCOMPATIBILITES file for compatibility issues.\n");
}

static const struct xt_option_entry REJECT_opts[] = {
	{.name = "reject-with", .id = O_REJECT_WITH, .type = XTTYPE_STRING},
	XTOPT_TABLEEND,
};

static void REJECT_init(struct xt_entry_target *t)
{
	struct ipt_reject_info *reject = (struct ipt_reject_info *)t->data;

	/* default */
	reject->with = IPT_ICMP_PORT_UNREACHABLE;

}

static void REJECT_parse(struct xt_option_call *cb)
{
	struct ipt_reject_info *reject = cb->data;
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
	/* This due to be dropped late in 2.4 pre-release cycle --RR */
	if (strncasecmp("echo-reply", cb->arg, strlen(cb->arg)) == 0 ||
	    strncasecmp("echoreply", cb->arg, strlen(cb->arg)) == 0)
		fprintf(stderr, "--reject-with echo-reply no longer"
			" supported\n");
	xtables_error(PARAMETER_PROBLEM,
		"unknown reject type \"%s\"", cb->arg);
}

static void REJECT_print(const void *ip, const struct xt_entry_target *target,
                         int numeric)
{
	const struct ipt_reject_info *reject
		= (const struct ipt_reject_info *)target->data;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(reject_table); ++i)
		if (reject_table[i].with == reject->with)
			break;
	printf(" reject-with %s", reject_table[i].name);
}

static void REJECT_save(const void *ip, const struct xt_entry_target *target)
{
	const struct ipt_reject_info *reject
		= (const struct ipt_reject_info *)target->data;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(reject_table); ++i)
		if (reject_table[i].with == reject->with)
			break;

	printf(" --reject-with %s", reject_table[i].name);
}

static struct xtables_target reject_tg_reg = {
	.name		= "REJECT",
	.version	= XTABLES_VERSION,
	.family		= NFPROTO_IPV4,
	.size		= XT_ALIGN(sizeof(struct ipt_reject_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct ipt_reject_info)),
	.help		= REJECT_help,
	.init		= REJECT_init,
	.print		= REJECT_print,
	.save		= REJECT_save,
	.x6_parse	= REJECT_parse,
	.x6_options	= REJECT_opts,
};

void _init(void)
{
	xtables_register_target(&reject_tg_reg);
}
