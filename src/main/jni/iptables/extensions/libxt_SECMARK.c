/*
 * Shared library add-on to iptables to add SECMARK target support.
 *
 * Based on the MARK target.
 *
 * Copyright (C) 2006 Red Hat, Inc., James Morris <jmorris@redhat.com>
 */
#include <stdio.h>
#include <xtables.h>
#include <linux/netfilter/xt_SECMARK.h>

#define PFX "SECMARK target: "

enum {
	O_SELCTX = 0,
};

static void SECMARK_help(void)
{
	printf(
"SECMARK target options:\n"
"  --selctx value                     Set the SELinux security context\n");
}

static const struct xt_option_entry SECMARK_opts[] = {
	{.name = "selctx", .id = O_SELCTX, .type = XTTYPE_STRING,
	 .flags = XTOPT_MAND | XTOPT_PUT,
	 XTOPT_POINTER(struct xt_secmark_target_info, secctx)},
	XTOPT_TABLEEND,
};

static void SECMARK_parse(struct xt_option_call *cb)
{
	struct xt_secmark_target_info *info = cb->data;

	xtables_option_parse(cb);
	info->mode = SECMARK_MODE_SEL;
}

static void print_secmark(const struct xt_secmark_target_info *info)
{
	switch (info->mode) {
	case SECMARK_MODE_SEL:
		printf("selctx %s", info->secctx);
		break;
	
	default:
		xtables_error(OTHER_PROBLEM, PFX "invalid mode %hhu\n", info->mode);
	}
}

static void SECMARK_print(const void *ip, const struct xt_entry_target *target,
                          int numeric)
{
	const struct xt_secmark_target_info *info =
		(struct xt_secmark_target_info*)(target)->data;

	printf(" SECMARK ");
	print_secmark(info);
}

static void SECMARK_save(const void *ip, const struct xt_entry_target *target)
{
	const struct xt_secmark_target_info *info =
		(struct xt_secmark_target_info*)target->data;

	printf(" --");
	print_secmark(info);
}

static struct xtables_target secmark_target = {
	.family		= NFPROTO_UNSPEC,
	.name		= "SECMARK",
	.version	= XTABLES_VERSION,
	.revision	= 0,
	.size		= XT_ALIGN(sizeof(struct xt_secmark_target_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_secmark_target_info)),
	.help		= SECMARK_help,
	.print		= SECMARK_print,
	.save		= SECMARK_save,
	.x6_parse	= SECMARK_parse,
	.x6_options	= SECMARK_opts,
};

void _init(void)
{
	xtables_register_target(&secmark_target);
}
