/* Shared library add-on to iptables to add ULOG support.
 * 
 * (C) 2000 by Harald Welte <laforge@gnumonks.org>
 *
 * multipart netlink support based on ideas by Sebastian Zander 
 * 						<zander@fokus.gmd.de>
 *
 * This software is released under the terms of GNU GPL
 * 
 * libipt_ULOG.c,v 1.7 2001/01/30 11:55:02 laforge Exp
 */
#include <stdio.h>
#include <string.h>
#include <xtables.h>
/* For 64bit kernel / 32bit userspace */
#include <linux/netfilter_ipv4/ipt_ULOG.h>

enum {
	O_ULOG_NLGROUP = 0,
	O_ULOG_PREFIX,
	O_ULOG_CPRANGE,
	O_ULOG_QTHR,
};

static void ULOG_help(void)
{
	printf("ULOG target options:\n"
	       " --ulog-nlgroup nlgroup		NETLINK group used for logging\n"
	       " --ulog-cprange size		Bytes of each packet to be passed\n"
	       " --ulog-qthreshold		Threshold of in-kernel queue\n"
	       " --ulog-prefix prefix		Prefix log messages with this prefix.\n");
}

static const struct xt_option_entry ULOG_opts[] = {
	{.name = "ulog-nlgroup", .id = O_ULOG_NLGROUP, .type = XTTYPE_UINT8,
	 .min = 1, .max = 32},
	{.name = "ulog-prefix", .id = O_ULOG_PREFIX, .type = XTTYPE_STRING,
	 .flags = XTOPT_PUT, XTOPT_POINTER(struct ipt_ulog_info, prefix),
	 .min = 1},
	{.name = "ulog-cprange", .id = O_ULOG_CPRANGE, .type = XTTYPE_UINT64,
	 .min = 1, .max = ULOG_MAX_QLEN},
	{.name = "ulog-qthreshold", .id = O_ULOG_QTHR, .type = XTTYPE_UINT64},
	XTOPT_TABLEEND,
};

static void ULOG_init(struct xt_entry_target *t)
{
	struct ipt_ulog_info *loginfo = (struct ipt_ulog_info *) t->data;

	loginfo->nl_group = ULOG_DEFAULT_NLGROUP;
	loginfo->qthreshold = ULOG_DEFAULT_QTHRESHOLD;

}

static void ULOG_parse(struct xt_option_call *cb)
{
	struct ipt_ulog_info *loginfo = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_ULOG_NLGROUP:
		loginfo->nl_group = 1 << (cb->val.u8 - 1);
		break;
	case O_ULOG_PREFIX:
		if (strchr(cb->arg, '\n') != NULL)
			xtables_error(PARAMETER_PROBLEM,
				   "Newlines not allowed in --ulog-prefix");
		break;
	case O_ULOG_CPRANGE:
		loginfo->copy_range = cb->val.u64;
		break;
	case O_ULOG_QTHR:
		loginfo->qthreshold = cb->val.u64;
		break;
	}
}

static void ULOG_save(const void *ip, const struct xt_entry_target *target)
{
	const struct ipt_ulog_info *loginfo
	    = (const struct ipt_ulog_info *) target->data;

	if (strcmp(loginfo->prefix, "") != 0) {
		fputs(" --ulog-prefix", stdout);
		xtables_save_string(loginfo->prefix);
	}

	if (loginfo->nl_group != ULOG_DEFAULT_NLGROUP)
		printf(" --ulog-nlgroup %d", ffs(loginfo->nl_group));
	if (loginfo->copy_range)
		printf(" --ulog-cprange %u", (unsigned int)loginfo->copy_range);

	if (loginfo->qthreshold != ULOG_DEFAULT_QTHRESHOLD)
		printf(" --ulog-qthreshold %u", (unsigned int)loginfo->qthreshold);
}

static void ULOG_print(const void *ip, const struct xt_entry_target *target,
                       int numeric)
{
	const struct ipt_ulog_info *loginfo
	    = (const struct ipt_ulog_info *) target->data;

	printf(" ULOG ");
	printf("copy_range %u nlgroup %d", (unsigned int)loginfo->copy_range,
	       ffs(loginfo->nl_group));
	if (strcmp(loginfo->prefix, "") != 0)
		printf(" prefix \"%s\"", loginfo->prefix);
	printf(" queue_threshold %u", (unsigned int)loginfo->qthreshold);
}

static struct xtables_target ulog_tg_reg = {
	.name		= "ULOG",
	.version	= XTABLES_VERSION,
	.family		= NFPROTO_IPV4,
	.size		= XT_ALIGN(sizeof(struct ipt_ulog_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct ipt_ulog_info)),
	.help		= ULOG_help,
	.init		= ULOG_init,
	.print		= ULOG_print,
	.save		= ULOG_save,
	.x6_parse	= ULOG_parse,
	.x6_options	= ULOG_opts,
};

void _init(void)
{
	xtables_register_target(&ulog_tg_reg);
}
