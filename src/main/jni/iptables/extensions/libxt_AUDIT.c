/* Shared library add-on to xtables for AUDIT
 *
 * (C) 2010-2011, Thomas Graf <tgraf@redhat.com>
 * (C) 2010-2011, Red Hat, Inc.
 *
 * This program is distributed under the terms of GNU GPL v2, 1991
 */
#include <stdio.h>
#include <string.h>
#include <xtables.h>
#include <linux/netfilter/xt_AUDIT.h>

enum {
	O_AUDIT_TYPE = 0,
};

static void audit_help(void)
{
	printf(
"AUDIT target options\n"
"  --type TYPE		Action type to be recorded.\n");
}

static const struct xt_option_entry audit_opts[] = {
	{.name = "type", .id = O_AUDIT_TYPE, .type = XTTYPE_STRING,
	 .flags = XTOPT_MAND},
	XTOPT_TABLEEND,
};

static void audit_parse(struct xt_option_call *cb)
{
	struct xt_audit_info *einfo = cb->data;

	xtables_option_parse(cb);
	if (strcasecmp(cb->arg, "accept") == 0)
		einfo->type = XT_AUDIT_TYPE_ACCEPT;
	else if (strcasecmp(cb->arg, "drop") == 0)
		einfo->type = XT_AUDIT_TYPE_DROP;
	else if (strcasecmp(cb->arg, "reject") == 0)
		einfo->type = XT_AUDIT_TYPE_REJECT;
	else
		xtables_error(PARAMETER_PROBLEM,
			   "Bad action type value \"%s\"", cb->arg);
}

static void audit_print(const void *ip, const struct xt_entry_target *target,
                      int numeric)
{
	const struct xt_audit_info *einfo =
		(const struct xt_audit_info *)target->data;

	printf(" AUDIT ");

	switch(einfo->type) {
	case XT_AUDIT_TYPE_ACCEPT:
		printf("accept");
		break;
	case XT_AUDIT_TYPE_DROP:
		printf("drop");
		break;
	case XT_AUDIT_TYPE_REJECT:
		printf("reject");
		break;
	}
}

static void audit_save(const void *ip, const struct xt_entry_target *target)
{
	const struct xt_audit_info *einfo =
		(const struct xt_audit_info *)target->data;

	switch(einfo->type) {
	case XT_AUDIT_TYPE_ACCEPT:
		printf(" --type accept");
		break;
	case XT_AUDIT_TYPE_DROP:
		printf(" --type drop");
		break;
	case XT_AUDIT_TYPE_REJECT:
		printf(" --type reject");
		break;
	}
}

static struct xtables_target audit_tg_reg = {
	.name		= "AUDIT",
	.version	= XTABLES_VERSION,
	.family		= NFPROTO_UNSPEC,
	.size		= XT_ALIGN(sizeof(struct xt_audit_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_audit_info)),
	.help		= audit_help,
	.print		= audit_print,
	.save		= audit_save,
	.x6_parse	= audit_parse,
	.x6_options	= audit_opts,
};

void _init(void)
{
	xtables_register_target(&audit_tg_reg);
}
