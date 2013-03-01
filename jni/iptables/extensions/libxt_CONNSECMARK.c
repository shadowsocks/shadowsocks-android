/*
 * Shared library add-on to iptables to add CONNSECMARK target support.
 *
 * Based on the MARK and CONNMARK targets.
 *
 * Copyright (C) 2006 Red Hat, Inc., James Morris <jmorris@redhat.com>
 */
#include <stdio.h>
#include <xtables.h>
#include <linux/netfilter/xt_CONNSECMARK.h>

#define PFX "CONNSECMARK target: "

enum {
	O_SAVE = 0,
	O_RESTORE,
	F_SAVE    = 1 << O_SAVE,
	F_RESTORE = 1 << O_RESTORE,
};

static void CONNSECMARK_help(void)
{
	printf(
"CONNSECMARK target options:\n"
"  --save                   Copy security mark from packet to conntrack\n"
"  --restore                Copy security mark from connection to packet\n");
}

static const struct xt_option_entry CONNSECMARK_opts[] = {
	{.name = "save", .id = O_SAVE, .excl = F_RESTORE, .type = XTTYPE_NONE},
	{.name = "restore", .id = O_RESTORE, .excl = F_SAVE,
	 .type = XTTYPE_NONE},
	XTOPT_TABLEEND,
};

static void CONNSECMARK_parse(struct xt_option_call *cb)
{
	struct xt_connsecmark_target_info *info = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_SAVE:
		info->mode = CONNSECMARK_SAVE;
		break;
	case O_RESTORE:
		info->mode = CONNSECMARK_RESTORE;
		break;
	}
}

static void CONNSECMARK_check(struct xt_fcheck_call *cb)
{
	if (cb->xflags == 0)
		xtables_error(PARAMETER_PROBLEM, PFX "parameter required");
}

static void print_connsecmark(const struct xt_connsecmark_target_info *info)
{
	switch (info->mode) {
	case CONNSECMARK_SAVE:
		printf("save");
		break;
		
	case CONNSECMARK_RESTORE:
		printf("restore");
		break;
		
	default:
		xtables_error(OTHER_PROBLEM, PFX "invalid mode %hhu\n", info->mode);
	}
}

static void
CONNSECMARK_print(const void *ip, const struct xt_entry_target *target,
                  int numeric)
{
	const struct xt_connsecmark_target_info *info =
		(struct xt_connsecmark_target_info*)(target)->data;

	printf(" CONNSECMARK ");
	print_connsecmark(info);
}

static void
CONNSECMARK_save(const void *ip, const struct xt_entry_target *target)
{
	const struct xt_connsecmark_target_info *info =
		(struct xt_connsecmark_target_info*)target->data;

	printf("--");
	print_connsecmark(info);
}

static struct xtables_target connsecmark_target = {
	.family		= NFPROTO_UNSPEC,
	.name		= "CONNSECMARK",
	.version	= XTABLES_VERSION,
	.revision	= 0,
	.size		= XT_ALIGN(sizeof(struct xt_connsecmark_target_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_connsecmark_target_info)),
	.help		= CONNSECMARK_help,
	.print		= CONNSECMARK_print,
	.save		= CONNSECMARK_save,
	.x6_parse	= CONNSECMARK_parse,
	.x6_fcheck	= CONNSECMARK_check,
	.x6_options	= CONNSECMARK_opts,
};

void _init(void)
{
	xtables_register_target(&connsecmark_target);
}
