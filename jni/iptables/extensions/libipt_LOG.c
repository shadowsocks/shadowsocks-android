#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <xtables.h>
#include <linux/netfilter_ipv4/ipt_LOG.h>

#define LOG_DEFAULT_LEVEL LOG_WARNING

#ifndef IPT_LOG_UID /* Old kernel */
#define IPT_LOG_UID	0x08	/* Log UID owning local socket */
#undef  IPT_LOG_MASK
#define IPT_LOG_MASK	0x0f
#endif

enum {
	O_LOG_LEVEL = 0,
	O_LOG_PREFIX,
	O_LOG_TCPSEQ,
	O_LOG_TCPOPTS,
	O_LOG_IPOPTS,
	O_LOG_UID,
	O_LOG_MAC,
};

static void LOG_help(void)
{
	printf(
"LOG target options:\n"
" --log-level level		Level of logging (numeric or see syslog.conf)\n"
" --log-prefix prefix		Prefix log messages with this prefix.\n\n"
" --log-tcp-sequence		Log TCP sequence numbers.\n\n"
" --log-tcp-options		Log TCP options.\n\n"
" --log-ip-options		Log IP options.\n\n"
" --log-uid			Log UID owning the local socket.\n\n"
" --log-macdecode		Decode MAC addresses and protocol.\n\n");
}

#define s struct ipt_log_info
static const struct xt_option_entry LOG_opts[] = {
	{.name = "log-level", .id = O_LOG_LEVEL, .type = XTTYPE_SYSLOGLEVEL,
	 .flags = XTOPT_PUT, XTOPT_POINTER(s, level)},
	{.name = "log-prefix", .id = O_LOG_PREFIX, .type = XTTYPE_STRING,
	 .flags = XTOPT_PUT, XTOPT_POINTER(s, prefix), .min = 1},
	{.name = "log-tcp-sequence", .id = O_LOG_TCPSEQ, .type = XTTYPE_NONE},
	{.name = "log-tcp-options", .id = O_LOG_TCPOPTS, .type = XTTYPE_NONE},
	{.name = "log-ip-options", .id = O_LOG_IPOPTS, .type = XTTYPE_NONE},
	{.name = "log-uid", .id = O_LOG_UID, .type = XTTYPE_NONE},
	{.name = "log-macdecode", .id = O_LOG_MAC, .type = XTTYPE_NONE},
	XTOPT_TABLEEND,
};
#undef s

static void LOG_init(struct xt_entry_target *t)
{
	struct ipt_log_info *loginfo = (struct ipt_log_info *)t->data;

	loginfo->level = LOG_DEFAULT_LEVEL;

}

struct ipt_log_names {
	const char *name;
	unsigned int level;
};

static const struct ipt_log_names ipt_log_names[]
= { { .name = "alert",   .level = LOG_ALERT },
    { .name = "crit",    .level = LOG_CRIT },
    { .name = "debug",   .level = LOG_DEBUG },
    { .name = "emerg",   .level = LOG_EMERG },
    { .name = "error",   .level = LOG_ERR },		/* DEPRECATED */
    { .name = "info",    .level = LOG_INFO },
    { .name = "notice",  .level = LOG_NOTICE },
    { .name = "panic",   .level = LOG_EMERG },		/* DEPRECATED */
    { .name = "warning", .level = LOG_WARNING }
};

static void LOG_parse(struct xt_option_call *cb)
{
	struct ipt_log_info *info = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_LOG_PREFIX:
		if (strchr(cb->arg, '\n') != NULL)
			xtables_error(PARAMETER_PROBLEM,
				   "Newlines not allowed in --log-prefix");
		break;
	case O_LOG_TCPSEQ:
		info->logflags = IPT_LOG_TCPSEQ;
		break;
	case O_LOG_TCPOPTS:
		info->logflags = IPT_LOG_TCPOPT;
		break;
	case O_LOG_IPOPTS:
		info->logflags = IPT_LOG_IPOPT;
		break;
	case O_LOG_UID:
		info->logflags = IPT_LOG_UID;
		break;
	case O_LOG_MAC:
		info->logflags = IPT_LOG_MACDECODE;
		break;
	}
}

static void LOG_print(const void *ip, const struct xt_entry_target *target,
                      int numeric)
{
	const struct ipt_log_info *loginfo
		= (const struct ipt_log_info *)target->data;
	unsigned int i = 0;

	printf(" LOG");
	if (numeric)
		printf(" flags %u level %u",
		       loginfo->logflags, loginfo->level);
	else {
		for (i = 0; i < ARRAY_SIZE(ipt_log_names); ++i)
			if (loginfo->level == ipt_log_names[i].level) {
				printf(" level %s", ipt_log_names[i].name);
				break;
			}
		if (i == ARRAY_SIZE(ipt_log_names))
			printf(" UNKNOWN level %u", loginfo->level);
		if (loginfo->logflags & IPT_LOG_TCPSEQ)
			printf(" tcp-sequence");
		if (loginfo->logflags & IPT_LOG_TCPOPT)
			printf(" tcp-options");
		if (loginfo->logflags & IPT_LOG_IPOPT)
			printf(" ip-options");
		if (loginfo->logflags & IPT_LOG_UID)
			printf(" uid");
		if (loginfo->logflags & IPT_LOG_MACDECODE)
			printf(" macdecode");
		if (loginfo->logflags & ~(IPT_LOG_MASK))
			printf(" unknown-flags");
	}

	if (strcmp(loginfo->prefix, "") != 0)
		printf(" prefix \"%s\"", loginfo->prefix);
}

static void LOG_save(const void *ip, const struct xt_entry_target *target)
{
	const struct ipt_log_info *loginfo
		= (const struct ipt_log_info *)target->data;

	if (strcmp(loginfo->prefix, "") != 0) {
		printf(" --log-prefix");
		xtables_save_string(loginfo->prefix);
	}

	if (loginfo->level != LOG_DEFAULT_LEVEL)
		printf(" --log-level %d", loginfo->level);

	if (loginfo->logflags & IPT_LOG_TCPSEQ)
		printf(" --log-tcp-sequence");
	if (loginfo->logflags & IPT_LOG_TCPOPT)
		printf(" --log-tcp-options");
	if (loginfo->logflags & IPT_LOG_IPOPT)
		printf(" --log-ip-options");
	if (loginfo->logflags & IPT_LOG_UID)
		printf(" --log-uid");
	if (loginfo->logflags & IPT_LOG_MACDECODE)
		printf(" --log-macdecode");
}

static struct xtables_target log_tg_reg = {
	.name          = "LOG",
	.version       = XTABLES_VERSION,
	.family        = NFPROTO_IPV4,
	.size          = XT_ALIGN(sizeof(struct ipt_log_info)),
	.userspacesize = XT_ALIGN(sizeof(struct ipt_log_info)),
	.help          = LOG_help,
	.init          = LOG_init,
	.print         = LOG_print,
	.save          = LOG_save,
	.x6_parse      = LOG_parse,
	.x6_options    = LOG_opts,
};

void _init(void)
{
	xtables_register_target(&log_tg_reg);
}
