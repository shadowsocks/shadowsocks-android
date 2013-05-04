#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <xtables.h>

#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_NFLOG.h>

enum {
	O_GROUP = 0,
	O_PREFIX,
	O_RANGE,
	O_THRESHOLD,
};

#define s struct xt_nflog_info
static const struct xt_option_entry NFLOG_opts[] = {
	{.name = "nflog-group", .id = O_GROUP, .type = XTTYPE_UINT16,
	 .flags = XTOPT_PUT, XTOPT_POINTER(s, group)},
	{.name = "nflog-prefix", .id = O_PREFIX, .type = XTTYPE_STRING,
	 .min = 1, .flags = XTOPT_PUT, XTOPT_POINTER(s, prefix)},
	{.name = "nflog-range", .id = O_RANGE, .type = XTTYPE_UINT32,
	 .flags = XTOPT_PUT, XTOPT_POINTER(s, len)},
	{.name = "nflog-threshold", .id = O_THRESHOLD, .type = XTTYPE_UINT16,
	 .flags = XTOPT_PUT, XTOPT_POINTER(s, threshold)},
	XTOPT_TABLEEND,
};
#undef s

static void NFLOG_help(void)
{
	printf("NFLOG target options:\n"
	       " --nflog-group NUM		NETLINK group used for logging\n"
	       " --nflog-range NUM		Number of byte to copy\n"
	       " --nflog-threshold NUM		Message threshold of in-kernel queue\n"
	       " --nflog-prefix STRING		Prefix string for log messages\n");
}

static void NFLOG_init(struct xt_entry_target *t)
{
	struct xt_nflog_info *info = (struct xt_nflog_info *)t->data;

	info->threshold	= XT_NFLOG_DEFAULT_THRESHOLD;
}

static void NFLOG_parse(struct xt_option_call *cb)
{
	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_PREFIX:
		if (strchr(cb->arg, '\n') != NULL)
			xtables_error(PARAMETER_PROBLEM,
				   "Newlines not allowed in --log-prefix");
		break;
	}
}

static void nflog_print(const struct xt_nflog_info *info, char *prefix)
{
	if (info->prefix[0] != '\0') {
		printf(" %snflog-prefix ", prefix);
		xtables_save_string(info->prefix);
	}
	if (info->group)
		printf(" %snflog-group %u", prefix, info->group);
	if (info->len)
		printf(" %snflog-range %u", prefix, info->len);
	if (info->threshold != XT_NFLOG_DEFAULT_THRESHOLD)
		printf(" %snflog-threshold %u", prefix, info->threshold);
}

static void NFLOG_print(const void *ip, const struct xt_entry_target *target,
                        int numeric)
{
	const struct xt_nflog_info *info = (struct xt_nflog_info *)target->data;

	nflog_print(info, "");
}

static void NFLOG_save(const void *ip, const struct xt_entry_target *target)
{
	const struct xt_nflog_info *info = (struct xt_nflog_info *)target->data;

	nflog_print(info, "--");
}

static struct xtables_target nflog_target = {
	.family		= NFPROTO_UNSPEC,
	.name		= "NFLOG",
	.version	= XTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_nflog_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_nflog_info)),
	.help		= NFLOG_help,
	.init		= NFLOG_init,
	.x6_parse	= NFLOG_parse,
	.print		= NFLOG_print,
	.save		= NFLOG_save,
	.x6_options	= NFLOG_opts,
};

void _init(void)
{
	xtables_register_target(&nflog_target);
}
