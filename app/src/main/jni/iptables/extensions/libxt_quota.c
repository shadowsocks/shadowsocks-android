/*
 * Shared library add-on to iptables to add quota support
 *
 * Sam Johnston <samj@samj.net>
 */
#include <stdio.h>
#include <xtables.h>
#include <linux/netfilter/xt_quota.h>

enum {
	O_QUOTA = 0,
};

static const struct xt_option_entry quota_opts[] = {
	{.name = "quota", .id = O_QUOTA, .type = XTTYPE_UINT64,
	 .flags = XTOPT_MAND | XTOPT_INVERT | XTOPT_PUT,
	 XTOPT_POINTER(struct xt_quota_info, quota)},
	XTOPT_TABLEEND,
};

static void quota_help(void)
{
	printf("quota match options:\n"
	       "[!] --quota quota		quota (bytes)\n");
}

static void
quota_print(const void *ip, const struct xt_entry_match *match, int numeric)
{
	const struct xt_quota_info *q = (const void *)match->data;
	printf(" quota: %llu bytes", (unsigned long long)q->quota);
}

static void
quota_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_quota_info *q = (const void *)match->data;

	if (q->flags & XT_QUOTA_INVERT)
		printf("! ");
	printf(" --quota %llu", (unsigned long long) q->quota);
}

static void quota_parse(struct xt_option_call *cb)
{
	struct xt_quota_info *info = cb->data;

	xtables_option_parse(cb);
	if (cb->invert)
		info->flags |= XT_QUOTA_INVERT;
	info->quota = cb->val.u64;
}

static struct xtables_match quota_match = {
	.family		= NFPROTO_UNSPEC,
	.name		= "quota",
	.version	= XTABLES_VERSION,
	.size		= XT_ALIGN(sizeof (struct xt_quota_info)),
	.userspacesize	= offsetof(struct xt_quota_info, master),
	.help		= quota_help,
	.print		= quota_print,
	.save		= quota_save,
	.x6_parse	= quota_parse,
	.x6_options	= quota_opts,
};

void
_init(void)
{
	xtables_register_match(&quota_match);
}
