#include <stdio.h>
#include <xtables.h>
#include <linux/netfilter/xt_length.h>

enum {
	O_LENGTH = 0,
};

static void length_help(void)
{
	printf(
"length match options:\n"
"[!] --length length[:length]    Match packet length against value or range\n"
"                                of values (inclusive)\n");
}
  
static const struct xt_option_entry length_opts[] = {
	{.name = "length", .id = O_LENGTH, .type = XTTYPE_UINT16RC,
	 .flags = XTOPT_MAND | XTOPT_INVERT},
	XTOPT_TABLEEND,
};

static void length_parse(struct xt_option_call *cb)
{
	struct xt_length_info *info = cb->data;

	xtables_option_parse(cb);
	info->min = cb->val.u16_range[0];
	info->max = cb->val.u16_range[0];
	if (cb->nvals >= 2)
		info->max = cb->val.u16_range[1];
	if (cb->invert)
		info->invert = 1;
}

static void
length_print(const void *ip, const struct xt_entry_match *match, int numeric)
{
	const struct xt_length_info *info = (void *)match->data;

	printf(" length %s", info->invert ? "!" : "");
	if (info->min == info->max)
		printf("%u", info->min);
	else
		printf("%u:%u", info->min, info->max);
}

static void length_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_length_info *info = (void *)match->data;

	printf("%s --length ", info->invert ? " !" : "");
	if (info->min == info->max)
		printf("%u", info->min);
	else
		printf("%u:%u", info->min, info->max);
}

static struct xtables_match length_match = {
	.family		= NFPROTO_UNSPEC,
	.name		= "length",
	.version	= XTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_length_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_length_info)),
	.help		= length_help,
	.print		= length_print,
	.save		= length_save,
	.x6_parse	= length_parse,
	.x6_options	= length_opts,
};

void _init(void)
{
	xtables_register_match(&length_match);
}
