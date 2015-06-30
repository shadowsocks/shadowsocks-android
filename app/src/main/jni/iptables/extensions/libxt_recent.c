#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <xtables.h>
#include <linux/netfilter/xt_recent.h>

enum {
	O_SET = 0,
	O_RCHECK,
	O_UPDATE,
	O_REMOVE,
	O_SECONDS,
	O_HITCOUNT,
	O_RTTL,
	O_NAME,
	O_RSOURCE,
	O_RDEST,
	F_SET    = 1 << O_SET,
	F_RCHECK = 1 << O_RCHECK,
	F_UPDATE = 1 << O_UPDATE,
	F_REMOVE = 1 << O_REMOVE,
	F_ANY_OP = F_SET | F_RCHECK | F_UPDATE | F_REMOVE,
};

#define s struct xt_recent_mtinfo
static const struct xt_option_entry recent_opts[] = {
	{.name = "set", .id = O_SET, .type = XTTYPE_NONE,
	 .excl = F_ANY_OP, .flags = XTOPT_INVERT},
	{.name = "rcheck", .id = O_RCHECK, .type = XTTYPE_NONE,
	 .excl = F_ANY_OP, .flags = XTOPT_INVERT},
	{.name = "update", .id = O_UPDATE, .type = XTTYPE_NONE,
	 .excl = F_ANY_OP, .flags = XTOPT_INVERT},
	{.name = "remove", .id = O_REMOVE, .type = XTTYPE_NONE,
	 .excl = F_ANY_OP, .flags = XTOPT_INVERT},
	{.name = "seconds", .id = O_SECONDS, .type = XTTYPE_UINT32,
	 .flags = XTOPT_PUT, XTOPT_POINTER(s, seconds)},
	{.name = "hitcount", .id = O_HITCOUNT, .type = XTTYPE_UINT32,
	 .flags = XTOPT_PUT, XTOPT_POINTER(s, hit_count)},
	{.name = "rttl", .id = O_RTTL, .type = XTTYPE_NONE,
	 .excl = F_SET | F_REMOVE},
	{.name = "name", .id = O_NAME, .type = XTTYPE_STRING,
	 .flags = XTOPT_PUT, XTOPT_POINTER(s, name)},
	{.name = "rsource", .id = O_RSOURCE, .type = XTTYPE_NONE},
	{.name = "rdest", .id = O_RDEST, .type = XTTYPE_NONE},
	XTOPT_TABLEEND,
};
#undef s

static void recent_help(void)
{
	printf(
"recent match options:\n"
"[!] --set                       Add source address to list, always matches.\n"
"[!] --rcheck                    Match if source address in list.\n"
"[!] --update                    Match if source address in list, also update last-seen time.\n"
"[!] --remove                    Match if source address in list, also removes that address from list.\n"
"    --seconds seconds           For check and update commands above.\n"
"                                Specifies that the match will only occur if source address last seen within\n"
"                                the last 'seconds' seconds.\n"
"    --hitcount hits             For check and update commands above.\n"
"                                Specifies that the match will only occur if source address seen hits times.\n"
"                                May be used in conjunction with the seconds option.\n"
"    --rttl                      For check and update commands above.\n"
"                                Specifies that the match will only occur if the source address and the TTL\n"
"                                match between this packet and the one which was set.\n"
"                                Useful if you have problems with people spoofing their source address in order\n"
"                                to DoS you via this module.\n"
"    --name name                 Name of the recent list to be used.  DEFAULT used if none given.\n"
"    --rsource                   Match/Save the source address of each packet in the recent list table (default).\n"
"    --rdest                     Match/Save the destination address of each packet in the recent list table.\n"
"xt_recent by: Stephen Frost <sfrost@snowman.net>.  http://snowman.net/projects/ipt_recent/\n");
}

static void recent_init(struct xt_entry_match *match)
{
	struct xt_recent_mtinfo *info = (void *)(match)->data;

	strncpy(info->name,"DEFAULT", XT_RECENT_NAME_LEN);
	/* even though XT_RECENT_NAME_LEN is currently defined as 200,
	 * better be safe, than sorry */
	info->name[XT_RECENT_NAME_LEN-1] = '\0';
	info->side = XT_RECENT_SOURCE;
}

static void recent_parse(struct xt_option_call *cb)
{
	struct xt_recent_mtinfo *info = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_SET:
		info->check_set |= XT_RECENT_SET;
		if (cb->invert)
			info->invert = true;
		break;
	case O_RCHECK:
		info->check_set |= XT_RECENT_CHECK;
		if (cb->invert)
			info->invert = true;
		break;
	case O_UPDATE:
		info->check_set |= XT_RECENT_UPDATE;
		if (cb->invert)
			info->invert = true;
		break;
	case O_REMOVE:
		info->check_set |= XT_RECENT_REMOVE;
		if (cb->invert)
			info->invert = true;
		break;
	case O_RTTL:
		info->check_set |= XT_RECENT_TTL;
		break;
	case O_RSOURCE:
		info->side = XT_RECENT_SOURCE;
		break;
	case O_RDEST:
		info->side = XT_RECENT_DEST;
		break;
	}
}

static void recent_check(struct xt_fcheck_call *cb)
{
	if (!(cb->xflags & F_ANY_OP))
		xtables_error(PARAMETER_PROBLEM,
			"recent: you must specify one of `--set', `--rcheck' "
			"`--update' or `--remove'");
}

static void recent_print(const void *ip, const struct xt_entry_match *match,
                         int numeric)
{
	const struct xt_recent_mtinfo *info = (const void *)match->data;

	if (info->invert)
		printf(" !");

	printf(" recent:");
	if (info->check_set & XT_RECENT_SET)
		printf(" SET");
	if (info->check_set & XT_RECENT_CHECK)
		printf(" CHECK");
	if (info->check_set & XT_RECENT_UPDATE)
		printf(" UPDATE");
	if (info->check_set & XT_RECENT_REMOVE)
		printf(" REMOVE");
	if(info->seconds) printf(" seconds: %d", info->seconds);
	if(info->hit_count) printf(" hit_count: %d", info->hit_count);
	if (info->check_set & XT_RECENT_TTL)
		printf(" TTL-Match");
	if(info->name) printf(" name: %s", info->name);
	if (info->side == XT_RECENT_SOURCE)
		printf(" side: source");
	if (info->side == XT_RECENT_DEST)
		printf(" side: dest");
}

static void recent_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_recent_mtinfo *info = (const void *)match->data;

	if (info->invert)
		printf(" !");

	if (info->check_set & XT_RECENT_SET)
		printf(" --set");
	if (info->check_set & XT_RECENT_CHECK)
		printf(" --rcheck");
	if (info->check_set & XT_RECENT_UPDATE)
		printf(" --update");
	if (info->check_set & XT_RECENT_REMOVE)
		printf(" --remove");
	if(info->seconds) printf(" --seconds %d", info->seconds);
	if(info->hit_count) printf(" --hitcount %d", info->hit_count);
	if (info->check_set & XT_RECENT_TTL)
		printf(" --rttl");
	if(info->name) printf(" --name %s",info->name);
	if (info->side == XT_RECENT_SOURCE)
		printf(" --rsource");
	if (info->side == XT_RECENT_DEST)
		printf(" --rdest");
}

static struct xtables_match recent_mt_reg = {
	.name          = "recent",
	.version       = XTABLES_VERSION,
	.family        = NFPROTO_UNSPEC,
	.size          = XT_ALIGN(sizeof(struct xt_recent_mtinfo)),
	.userspacesize = XT_ALIGN(sizeof(struct xt_recent_mtinfo)),
	.help          = recent_help,
	.init          = recent_init,
	.x6_parse      = recent_parse,
	.x6_fcheck     = recent_check,
	.print         = recent_print,
	.save          = recent_save,
	.x6_options    = recent_opts,
};

void _init(void)
{
	xtables_register_match(&recent_mt_reg);
}
