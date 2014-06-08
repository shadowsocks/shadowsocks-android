#include <stdio.h>
#include <xtables.h>
#include <linux/netfilter/xt_physdev.h>

enum {
	O_PHYSDEV_IN = 0,
	O_PHYSDEV_OUT,
	O_PHYSDEV_IS_IN,
	O_PHYSDEV_IS_OUT,
	O_PHYSDEV_IS_BRIDGED,
};

static void physdev_help(void)
{
	printf(
"physdev match options:\n"
" [!] --physdev-in inputname[+]		bridge port name ([+] for wildcard)\n"
" [!] --physdev-out outputname[+]	bridge port name ([+] for wildcard)\n"
" [!] --physdev-is-in			arrived on a bridge device\n"
" [!] --physdev-is-out			will leave on a bridge device\n"
" [!] --physdev-is-bridged		it's a bridged packet\n");
}

#define s struct xt_physdev_info
static const struct xt_option_entry physdev_opts[] = {
	{.name = "physdev-in", .id = O_PHYSDEV_IN, .type = XTTYPE_STRING,
	 .flags = XTOPT_INVERT | XTOPT_PUT, XTOPT_POINTER(s, physindev)},
	{.name = "physdev-out", .id = O_PHYSDEV_OUT, .type = XTTYPE_STRING,
	 .flags = XTOPT_INVERT | XTOPT_PUT, XTOPT_POINTER(s, physoutdev)},
	{.name = "physdev-is-in", .id = O_PHYSDEV_IS_IN, .type = XTTYPE_NONE},
	{.name = "physdev-is-out", .id = O_PHYSDEV_IS_OUT,
	 .type = XTTYPE_NONE},
	{.name = "physdev-is-bridged", .id = O_PHYSDEV_IS_BRIDGED,
	 .type = XTTYPE_NONE},
	XTOPT_TABLEEND,
};
#undef s

static void physdev_parse(struct xt_option_call *cb)
{
	struct xt_physdev_info *info = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_PHYSDEV_IN:
		xtables_parse_interface(cb->arg, info->physindev,
				(unsigned char *)info->in_mask);
		if (cb->invert)
			info->invert |= XT_PHYSDEV_OP_IN;
		info->bitmask |= XT_PHYSDEV_OP_IN;
		break;
	case O_PHYSDEV_OUT:
		xtables_parse_interface(cb->arg, info->physoutdev,
				(unsigned char *)info->out_mask);
		if (cb->invert)
			info->invert |= XT_PHYSDEV_OP_OUT;
		info->bitmask |= XT_PHYSDEV_OP_OUT;
		break;
	case O_PHYSDEV_IS_IN:
		info->bitmask |= XT_PHYSDEV_OP_ISIN;
		if (cb->invert)
			info->invert |= XT_PHYSDEV_OP_ISIN;
		break;
	case O_PHYSDEV_IS_OUT:
		info->bitmask |= XT_PHYSDEV_OP_ISOUT;
		if (cb->invert)
			info->invert |= XT_PHYSDEV_OP_ISOUT;
		break;
	case O_PHYSDEV_IS_BRIDGED:
		if (cb->invert)
			info->invert |= XT_PHYSDEV_OP_BRIDGED;
		info->bitmask |= XT_PHYSDEV_OP_BRIDGED;
		break;
	}
}

static void physdev_check(struct xt_fcheck_call *cb)
{
	if (cb->xflags == 0)
		xtables_error(PARAMETER_PROBLEM, "PHYSDEV: no physdev option specified");
}

static void
physdev_print(const void *ip, const struct xt_entry_match *match, int numeric)
{
	const struct xt_physdev_info *info = (const void *)match->data;

	printf(" PHYSDEV match");
	if (info->bitmask & XT_PHYSDEV_OP_ISIN)
		printf("%s --physdev-is-in",
		       info->invert & XT_PHYSDEV_OP_ISIN ? " !":"");
	if (info->bitmask & XT_PHYSDEV_OP_IN)
		printf("%s --physdev-in %s",
		(info->invert & XT_PHYSDEV_OP_IN) ? " !":"", info->physindev);

	if (info->bitmask & XT_PHYSDEV_OP_ISOUT)
		printf("%s --physdev-is-out",
		       info->invert & XT_PHYSDEV_OP_ISOUT ? " !":"");
	if (info->bitmask & XT_PHYSDEV_OP_OUT)
		printf("%s --physdev-out %s",
		(info->invert & XT_PHYSDEV_OP_OUT) ? " !":"", info->physoutdev);
	if (info->bitmask & XT_PHYSDEV_OP_BRIDGED)
		printf("%s --physdev-is-bridged",
		       info->invert & XT_PHYSDEV_OP_BRIDGED ? " !":"");
}

static void physdev_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_physdev_info *info = (const void *)match->data;

	if (info->bitmask & XT_PHYSDEV_OP_ISIN)
		printf("%s --physdev-is-in",
		       (info->invert & XT_PHYSDEV_OP_ISIN) ? " !" : "");
	if (info->bitmask & XT_PHYSDEV_OP_IN)
		printf("%s --physdev-in %s",
		       (info->invert & XT_PHYSDEV_OP_IN) ? " !" : "",
		       info->physindev);

	if (info->bitmask & XT_PHYSDEV_OP_ISOUT)
		printf("%s --physdev-is-out",
		       (info->invert & XT_PHYSDEV_OP_ISOUT) ? " !" : "");
	if (info->bitmask & XT_PHYSDEV_OP_OUT)
		printf("%s --physdev-out %s",
		       (info->invert & XT_PHYSDEV_OP_OUT) ? " !" : "",
		       info->physoutdev);
	if (info->bitmask & XT_PHYSDEV_OP_BRIDGED)
		printf("%s --physdev-is-bridged",
		       (info->invert & XT_PHYSDEV_OP_BRIDGED) ? " !" : "");
}

static struct xtables_match physdev_match = {
	.family		= NFPROTO_UNSPEC,
	.name		= "physdev",
	.version	= XTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_physdev_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_physdev_info)),
	.help		= physdev_help,
	.print		= physdev_print,
	.save		= physdev_save,
	.x6_parse	= physdev_parse,
	.x6_fcheck	= physdev_check,
	.x6_options	= physdev_opts,
};

void _init(void)
{
	xtables_register_match(&physdev_match);
}
