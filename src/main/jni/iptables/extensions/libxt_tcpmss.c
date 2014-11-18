#include <stdio.h>
#include <xtables.h>
#include <linux/netfilter/xt_tcpmss.h>

enum {
	O_TCPMSS = 0,
};

static void tcpmss_help(void)
{
	printf(
"tcpmss match options:\n"
"[!] --mss value[:value]	Match TCP MSS range.\n"
"				(only valid for TCP SYN or SYN/ACK packets)\n");
}

static const struct xt_option_entry tcpmss_opts[] = {
	{.name = "mss", .id = O_TCPMSS, .type = XTTYPE_UINT16RC,
	 .flags = XTOPT_MAND | XTOPT_INVERT},
	XTOPT_TABLEEND,
};

static void tcpmss_parse(struct xt_option_call *cb)
{
	struct xt_tcpmss_match_info *mssinfo = cb->data;

	xtables_option_parse(cb);
	mssinfo->mss_min = cb->val.u16_range[0];
	mssinfo->mss_max = mssinfo->mss_min;
	if (cb->nvals == 2)
		mssinfo->mss_max = cb->val.u16_range[1];
	if (cb->invert)
		mssinfo->invert = 1;
}

static void
tcpmss_print(const void *ip, const struct xt_entry_match *match, int numeric)
{
	const struct xt_tcpmss_match_info *info = (void *)match->data;

	printf(" tcpmss match %s", info->invert ? "!" : "");
	if (info->mss_min == info->mss_max)
		printf("%u", info->mss_min);
	else
		printf("%u:%u", info->mss_min, info->mss_max);
}

static void tcpmss_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_tcpmss_match_info *info = (void *)match->data;

	printf("%s --mss ", info->invert ? " !" : "");
	if (info->mss_min == info->mss_max)
		printf("%u", info->mss_min);
	else
		printf("%u:%u", info->mss_min, info->mss_max);
}

static struct xtables_match tcpmss_match = {
	.family		= NFPROTO_UNSPEC,
	.name		= "tcpmss",
	.version	= XTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_tcpmss_match_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_tcpmss_match_info)),
	.help		= tcpmss_help,
	.print		= tcpmss_print,
	.save		= tcpmss_save,
	.x6_parse	= tcpmss_parse,
	.x6_options	= tcpmss_opts,
};

void _init(void)
{
	xtables_register_match(&tcpmss_match);
}
