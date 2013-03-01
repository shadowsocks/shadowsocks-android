/*
 * Shared library add-on to iptables to add TCPOPTSTRIP target support.
 * Copyright (c) 2007 Sven Schnelle <svens@bitebene.org>
 * Copyright © CC Computer Consultants GmbH, 2007
 * Jan Engelhardt <jengelh@computergmbh.de>
 */
#include <stdio.h>
#include <string.h>
#include <xtables.h>
#include <netinet/tcp.h>
#include <linux/netfilter/xt_TCPOPTSTRIP.h>
#ifndef TCPOPT_MD5SIG
#	define TCPOPT_MD5SIG 19
#endif

enum {
	O_STRIP_OPTION = 0,
};

struct tcp_optionmap {
	const char *name, *desc;
	const unsigned int option;
};

static const struct xt_option_entry tcpoptstrip_tg_opts[] = {
	{.name = "strip-options", .id = O_STRIP_OPTION, .type = XTTYPE_STRING},
	XTOPT_TABLEEND,
};

static const struct tcp_optionmap tcp_optionmap[] = {
	{"wscale",         "Window scale",         TCPOPT_WINDOW},
	{"mss",            "Maximum Segment Size", TCPOPT_MAXSEG},
	{"sack-permitted", "SACK permitted",       TCPOPT_SACK_PERMITTED},
	{"sack",           "Selective ACK",        TCPOPT_SACK},
	{"timestamp",      "Timestamp",            TCPOPT_TIMESTAMP},
	{"md5",            "MD5 signature",        TCPOPT_MD5SIG},
	{NULL},
};

static void tcpoptstrip_tg_help(void)
{
	const struct tcp_optionmap *w;

	printf(
"TCPOPTSTRIP target options:\n"
"  --strip-options value     strip specified TCP options denoted by value\n"
"                            (separated by comma) from TCP header\n"
"  Instead of the numeric value, you can also use the following names:\n"
	);

	for (w = tcp_optionmap; w->name != NULL; ++w)
		printf("    %-14s    strip \"%s\" option\n", w->name, w->desc);
}

static void
parse_list(struct xt_tcpoptstrip_target_info *info, const char *arg)
{
	unsigned int option;
	char *p;
	int i;

	while (true) {
		p = strchr(arg, ',');
		if (p != NULL)
			*p = '\0';

		option = 0;
		for (i = 0; tcp_optionmap[i].name != NULL; ++i)
			if (strcmp(tcp_optionmap[i].name, arg) == 0) {
				option = tcp_optionmap[i].option;
				break;
			}

		if (option == 0 &&
		    !xtables_strtoui(arg, NULL, &option, 0, UINT8_MAX))
			xtables_error(PARAMETER_PROBLEM,
			           "Bad TCP option value \"%s\"", arg);

		if (option < 2)
			xtables_error(PARAMETER_PROBLEM,
			           "Option value may not be 0 or 1");

		if (tcpoptstrip_test_bit(info->strip_bmap, option))
			xtables_error(PARAMETER_PROBLEM,
			           "Option \"%s\" already specified", arg);

		tcpoptstrip_set_bit(info->strip_bmap, option);
		if (p == NULL)
			break;
		arg = p + 1;
	}
}

static void tcpoptstrip_tg_parse(struct xt_option_call *cb)
{
	struct xt_tcpoptstrip_target_info *info = cb->data;

	xtables_option_parse(cb);
	parse_list(info, cb->arg);
}

static void
tcpoptstrip_print_list(const struct xt_tcpoptstrip_target_info *info,
                       bool numeric)
{
	unsigned int i, j;
	const char *name;
	bool first = true;

	for (i = 0; i < 256; ++i) {
		if (!tcpoptstrip_test_bit(info->strip_bmap, i))
			continue;
		if (!first)
			printf(",");

		first = false;
		name  = NULL;
		if (!numeric)
			for (j = 0; tcp_optionmap[j].name != NULL; ++j)
				if (tcp_optionmap[j].option == i)
					name = tcp_optionmap[j].name;

		if (name != NULL)
			printf("%s", name);
		else
			printf("%u", i);
	}
}

static void
tcpoptstrip_tg_print(const void *ip, const struct xt_entry_target *target,
                     int numeric)
{
	const struct xt_tcpoptstrip_target_info *info =
		(const void *)target->data;

	printf(" TCPOPTSTRIP options ");
	tcpoptstrip_print_list(info, numeric);
}

static void
tcpoptstrip_tg_save(const void *ip, const struct xt_entry_target *target)
{
	const struct xt_tcpoptstrip_target_info *info =
		(const void *)target->data;

	printf(" --strip-options ");
	tcpoptstrip_print_list(info, true);
}

static struct xtables_target tcpoptstrip_tg_reg = {
	.version       = XTABLES_VERSION,
	.name          = "TCPOPTSTRIP",
	.family        = NFPROTO_UNSPEC,
	.size          = XT_ALIGN(sizeof(struct xt_tcpoptstrip_target_info)),
	.userspacesize = XT_ALIGN(sizeof(struct xt_tcpoptstrip_target_info)),
	.help          = tcpoptstrip_tg_help,
	.print         = tcpoptstrip_tg_print,
	.save          = tcpoptstrip_tg_save,
	.x6_parse      = tcpoptstrip_tg_parse,
	.x6_options    = tcpoptstrip_tg_opts,
};

void _init(void)
{
	xtables_register_target(&tcpoptstrip_tg_reg);
}
