/* ip6tables match extension for limiting packets per destination
 *
 * (C) 2003-2004 by Harald Welte <laforge@netfilter.org>
 *
 * Development of this code was funded by Astaro AG, http://www.astaro.com/
 *
 * Based on ipt_limit.c by
 * Jérôme de Vivie   <devivie@info.enserb.u-bordeaux.fr>
 * Hervé Eychenne    <rv@wallfire.org>
 * 
 * Error corections by nmalykh@bilim.com (22.01.2005)
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <xtables.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_hashlimit.h>

#define XT_HASHLIMIT_BURST	5

/* miliseconds */
#define XT_HASHLIMIT_GCINTERVAL	1000
#define XT_HASHLIMIT_EXPIRE	10000

static void hashlimit_help(void)
{
	printf(
"hashlimit match options:\n"
"--hashlimit <avg>		max average match rate\n"
"                                [Packets per second unless followed by \n"
"                                /sec /minute /hour /day postfixes]\n"
"--hashlimit-mode <mode>		mode is a comma-separated list of\n"
"					dstip,srcip,dstport,srcport\n"
"--hashlimit-name <name>		name for /proc/net/ipt_hashlimit/\n"
"[--hashlimit-burst <num>]	number to match in a burst, default %u\n"
"[--hashlimit-htable-size <num>]	number of hashtable buckets\n"
"[--hashlimit-htable-max <num>]	number of hashtable entries\n"
"[--hashlimit-htable-gcinterval]	interval between garbage collection runs\n"
"[--hashlimit-htable-expire]	after which time are idle entries expired?\n",
XT_HASHLIMIT_BURST);
}

enum {
	O_UPTO = 0,
	O_ABOVE,
	O_LIMIT,
	O_MODE,
	O_SRCMASK,
	O_DSTMASK,
	O_NAME,
	O_BURST,
	O_HTABLE_SIZE,
	O_HTABLE_MAX,
	O_HTABLE_GCINT,
	O_HTABLE_EXPIRE,
	F_UPTO  = 1 << O_UPTO,
	F_ABOVE = 1 << O_ABOVE,
};

static void hashlimit_mt_help(void)
{
	printf(
"hashlimit match options:\n"
"  --hashlimit-upto <avg>           max average match rate\n"
"                                   [Packets per second unless followed by \n"
"                                   /sec /minute /hour /day postfixes]\n"
"  --hashlimit-above <avg>          min average match rate\n"
"  --hashlimit-mode <mode>          mode is a comma-separated list of\n"
"                                   dstip,srcip,dstport,srcport (or none)\n"
"  --hashlimit-srcmask <length>     source address grouping prefix length\n"
"  --hashlimit-dstmask <length>     destination address grouping prefix length\n"
"  --hashlimit-name <name>          name for /proc/net/ipt_hashlimit\n"
"  --hashlimit-burst <num>	    number to match in a burst, default %u\n"
"  --hashlimit-htable-size <num>    number of hashtable buckets\n"
"  --hashlimit-htable-max <num>     number of hashtable entries\n"
"  --hashlimit-htable-gcinterval    interval between garbage collection runs\n"
"  --hashlimit-htable-expire        after which time are idle entries expired?\n"
"\n", XT_HASHLIMIT_BURST);
}

#define s struct xt_hashlimit_info
static const struct xt_option_entry hashlimit_opts[] = {
	{.name = "hashlimit", .id = O_UPTO, .excl = F_ABOVE,
	 .type = XTTYPE_STRING, .flags = XTOPT_INVERT},
	{.name = "hashlimit-burst", .id = O_BURST, .type = XTTYPE_UINT32,
	 .min = 1, .max = 10000, .flags = XTOPT_PUT,
	 XTOPT_POINTER(s, cfg.burst)},
	{.name = "hashlimit-htable-size", .id = O_HTABLE_SIZE,
	 .type = XTTYPE_UINT32, .flags = XTOPT_PUT,
	 XTOPT_POINTER(s, cfg.size)},
	{.name = "hashlimit-htable-max", .id = O_HTABLE_MAX,
	 .type = XTTYPE_UINT32, .flags = XTOPT_PUT,
	 XTOPT_POINTER(s, cfg.max)},
	{.name = "hashlimit-htable-gcinterval", .id = O_HTABLE_GCINT,
	 .type = XTTYPE_UINT32, .flags = XTOPT_PUT,
	 XTOPT_POINTER(s, cfg.gc_interval)},
	{.name = "hashlimit-htable-expire", .id = O_HTABLE_EXPIRE,
	 .type = XTTYPE_UINT32, .flags = XTOPT_PUT,
	 XTOPT_POINTER(s, cfg.expire)},
	{.name = "hashlimit-mode", .id = O_MODE, .type = XTTYPE_STRING,
	 .flags = XTOPT_MAND},
	{.name = "hashlimit-name", .id = O_NAME, .type = XTTYPE_STRING,
	 .flags = XTOPT_MAND | XTOPT_PUT, XTOPT_POINTER(s, name), .min = 1},
	XTOPT_TABLEEND,
};
#undef s

#define s struct xt_hashlimit_mtinfo1
static const struct xt_option_entry hashlimit_mt_opts[] = {
	{.name = "hashlimit-upto", .id = O_UPTO, .excl = F_ABOVE,
	 .type = XTTYPE_STRING, .flags = XTOPT_INVERT},
	{.name = "hashlimit-above", .id = O_ABOVE, .excl = F_UPTO,
	 .type = XTTYPE_STRING, .flags = XTOPT_INVERT},
	{.name = "hashlimit", .id = O_UPTO, .excl = F_ABOVE,
	 .type = XTTYPE_STRING, .flags = XTOPT_INVERT}, /* old name */
	{.name = "hashlimit-srcmask", .id = O_SRCMASK, .type = XTTYPE_PLEN},
	{.name = "hashlimit-dstmask", .id = O_DSTMASK, .type = XTTYPE_PLEN},
	{.name = "hashlimit-burst", .id = O_BURST, .type = XTTYPE_UINT32,
	 .min = 1, .max = 10000, .flags = XTOPT_PUT,
	 XTOPT_POINTER(s, cfg.burst)},
	{.name = "hashlimit-htable-size", .id = O_HTABLE_SIZE,
	 .type = XTTYPE_UINT32, .flags = XTOPT_PUT,
	 XTOPT_POINTER(s, cfg.size)},
	{.name = "hashlimit-htable-max", .id = O_HTABLE_MAX,
	 .type = XTTYPE_UINT32, .flags = XTOPT_PUT,
	 XTOPT_POINTER(s, cfg.max)},
	{.name = "hashlimit-htable-gcinterval", .id = O_HTABLE_GCINT,
	 .type = XTTYPE_UINT32, .flags = XTOPT_PUT,
	 XTOPT_POINTER(s, cfg.gc_interval)},
	{.name = "hashlimit-htable-expire", .id = O_HTABLE_EXPIRE,
	 .type = XTTYPE_UINT32, .flags = XTOPT_PUT,
	 XTOPT_POINTER(s, cfg.expire)},
	{.name = "hashlimit-mode", .id = O_MODE, .type = XTTYPE_STRING},
	{.name = "hashlimit-name", .id = O_NAME, .type = XTTYPE_STRING,
	 .flags = XTOPT_MAND | XTOPT_PUT, XTOPT_POINTER(s, name), .min = 1},
	XTOPT_TABLEEND,
};
#undef s

static
int parse_rate(const char *rate, uint32_t *val)
{
	const char *delim;
	uint32_t r;
	uint32_t mult = 1;  /* Seconds by default. */

	delim = strchr(rate, '/');
	if (delim) {
		if (strlen(delim+1) == 0)
			return 0;

		if (strncasecmp(delim+1, "second", strlen(delim+1)) == 0)
			mult = 1;
		else if (strncasecmp(delim+1, "minute", strlen(delim+1)) == 0)
			mult = 60;
		else if (strncasecmp(delim+1, "hour", strlen(delim+1)) == 0)
			mult = 60*60;
		else if (strncasecmp(delim+1, "day", strlen(delim+1)) == 0)
			mult = 24*60*60;
		else
			return 0;
	}
	r = atoi(rate);
	if (!r)
		return 0;

	/* This would get mapped to infinite (1/day is minimum they
           can specify, so we're ok at that end). */
	if (r / mult > XT_HASHLIMIT_SCALE)
		xtables_error(PARAMETER_PROBLEM, "Rate too fast \"%s\"\n", rate);

	*val = XT_HASHLIMIT_SCALE * mult / r;
	return 1;
}

static void hashlimit_init(struct xt_entry_match *m)
{
	struct xt_hashlimit_info *r = (struct xt_hashlimit_info *)m->data;

	r->cfg.burst = XT_HASHLIMIT_BURST;
	r->cfg.gc_interval = XT_HASHLIMIT_GCINTERVAL;
	r->cfg.expire = XT_HASHLIMIT_EXPIRE;

}

static void hashlimit_mt4_init(struct xt_entry_match *match)
{
	struct xt_hashlimit_mtinfo1 *info = (void *)match->data;

	info->cfg.mode        = 0;
	info->cfg.burst       = XT_HASHLIMIT_BURST;
	info->cfg.gc_interval = XT_HASHLIMIT_GCINTERVAL;
	info->cfg.expire      = XT_HASHLIMIT_EXPIRE;
	info->cfg.srcmask     = 32;
	info->cfg.dstmask     = 32;
}

static void hashlimit_mt6_init(struct xt_entry_match *match)
{
	struct xt_hashlimit_mtinfo1 *info = (void *)match->data;

	info->cfg.mode        = 0;
	info->cfg.burst       = XT_HASHLIMIT_BURST;
	info->cfg.gc_interval = XT_HASHLIMIT_GCINTERVAL;
	info->cfg.expire      = XT_HASHLIMIT_EXPIRE;
	info->cfg.srcmask     = 128;
	info->cfg.dstmask     = 128;
}

/* Parse a 'mode' parameter into the required bitmask */
static int parse_mode(uint32_t *mode, const char *option_arg)
{
	char *tok;
	char *arg = strdup(option_arg);

	if (!arg)
		return -1;

	for (tok = strtok(arg, ",|");
	     tok;
	     tok = strtok(NULL, ",|")) {
		if (!strcmp(tok, "dstip"))
			*mode |= XT_HASHLIMIT_HASH_DIP;
		else if (!strcmp(tok, "srcip"))
			*mode |= XT_HASHLIMIT_HASH_SIP;
		else if (!strcmp(tok, "srcport"))
			*mode |= XT_HASHLIMIT_HASH_SPT;
		else if (!strcmp(tok, "dstport"))
			*mode |= XT_HASHLIMIT_HASH_DPT;
		else {
			free(arg);
			return -1;
		}
	}
	free(arg);
	return 0;
}

static void hashlimit_parse(struct xt_option_call *cb)
{
	struct xt_hashlimit_info *info = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_UPTO:
		if (cb->invert)
			info->cfg.mode |= XT_HASHLIMIT_INVERT;
		if (!parse_rate(cb->arg, &info->cfg.avg))
			xtables_param_act(XTF_BAD_VALUE, "hashlimit",
			          "--hashlimit-upto", cb->arg);
		break;
	case O_ABOVE:
		if (!cb->invert)
			info->cfg.mode |= XT_HASHLIMIT_INVERT;
		if (!parse_rate(cb->arg, &info->cfg.avg))
			xtables_param_act(XTF_BAD_VALUE, "hashlimit",
			          "--hashlimit-above", cb->arg);
		break;
	case O_MODE:
		if (parse_mode(&info->cfg.mode, cb->arg) < 0)
			xtables_param_act(XTF_BAD_VALUE, "hashlimit",
			          "--hashlimit-mode", cb->arg);
		break;
	}
}

static void hashlimit_mt_parse(struct xt_option_call *cb)
{
	struct xt_hashlimit_mtinfo1 *info = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_UPTO:
		if (cb->invert)
			info->cfg.mode |= XT_HASHLIMIT_INVERT;
		if (!parse_rate(cb->arg, &info->cfg.avg))
			xtables_param_act(XTF_BAD_VALUE, "hashlimit",
			          "--hashlimit-upto", cb->arg);
		break;
	case O_ABOVE:
		if (!cb->invert)
			info->cfg.mode |= XT_HASHLIMIT_INVERT;
		if (!parse_rate(cb->arg, &info->cfg.avg))
			xtables_param_act(XTF_BAD_VALUE, "hashlimit",
			          "--hashlimit-above", cb->arg);
		break;
	case O_MODE:
		if (parse_mode(&info->cfg.mode, cb->arg) < 0)
			xtables_param_act(XTF_BAD_VALUE, "hashlimit",
			          "--hashlimit-mode", cb->arg);
		break;
	case O_SRCMASK:
		info->cfg.srcmask = cb->val.hlen;
		break;
	case O_DSTMASK:
		info->cfg.dstmask = cb->val.hlen;
		break;
	}
}

static void hashlimit_check(struct xt_fcheck_call *cb)
{
	if (!(cb->xflags & (F_UPTO | F_ABOVE)))
		xtables_error(PARAMETER_PROBLEM,
				"You have to specify --hashlimit");
}

static const struct rates
{
	const char *name;
	uint32_t mult;
} rates[] = { { "day", XT_HASHLIMIT_SCALE*24*60*60 },
	      { "hour", XT_HASHLIMIT_SCALE*60*60 },
	      { "min", XT_HASHLIMIT_SCALE*60 },
	      { "sec", XT_HASHLIMIT_SCALE } };

static void print_rate(uint32_t period)
{
	unsigned int i;

	for (i = 1; i < ARRAY_SIZE(rates); ++i)
		if (period > rates[i].mult
            || rates[i].mult/period < rates[i].mult%period)
			break;

	printf(" %u/%s", rates[i-1].mult / period, rates[i-1].name);
}

static void print_mode(unsigned int mode, char separator)
{
	bool prevmode = false;

	putchar(' ');
	if (mode & XT_HASHLIMIT_HASH_SIP) {
		fputs("srcip", stdout);
		prevmode = 1;
	}
	if (mode & XT_HASHLIMIT_HASH_SPT) {
		if (prevmode)
			putchar(separator);
		fputs("srcport", stdout);
		prevmode = 1;
	}
	if (mode & XT_HASHLIMIT_HASH_DIP) {
		if (prevmode)
			putchar(separator);
		fputs("dstip", stdout);
		prevmode = 1;
	}
	if (mode & XT_HASHLIMIT_HASH_DPT) {
		if (prevmode)
			putchar(separator);
		fputs("dstport", stdout);
	}
}

static void hashlimit_print(const void *ip,
                            const struct xt_entry_match *match, int numeric)
{
	const struct xt_hashlimit_info *r = (const void *)match->data;
	fputs(" limit: avg", stdout); print_rate(r->cfg.avg);
	printf(" burst %u", r->cfg.burst);
	fputs(" mode", stdout);
	print_mode(r->cfg.mode, '-');
	if (r->cfg.size)
		printf(" htable-size %u", r->cfg.size);
	if (r->cfg.max)
		printf(" htable-max %u", r->cfg.max);
	if (r->cfg.gc_interval != XT_HASHLIMIT_GCINTERVAL)
		printf(" htable-gcinterval %u", r->cfg.gc_interval);
	if (r->cfg.expire != XT_HASHLIMIT_EXPIRE)
		printf(" htable-expire %u", r->cfg.expire);
}

static void
hashlimit_mt_print(const struct xt_hashlimit_mtinfo1 *info, unsigned int dmask)
{
	if (info->cfg.mode & XT_HASHLIMIT_INVERT)
		fputs(" limit: above", stdout);
	else
		fputs(" limit: up to", stdout);
	print_rate(info->cfg.avg);
	printf(" burst %u", info->cfg.burst);
	if (info->cfg.mode & (XT_HASHLIMIT_HASH_SIP | XT_HASHLIMIT_HASH_SPT |
	    XT_HASHLIMIT_HASH_DIP | XT_HASHLIMIT_HASH_DPT)) {
		fputs(" mode", stdout);
		print_mode(info->cfg.mode, '-');
	}
	if (info->cfg.size != 0)
		printf(" htable-size %u", info->cfg.size);
	if (info->cfg.max != 0)
		printf(" htable-max %u", info->cfg.max);
	if (info->cfg.gc_interval != XT_HASHLIMIT_GCINTERVAL)
		printf(" htable-gcinterval %u", info->cfg.gc_interval);
	if (info->cfg.expire != XT_HASHLIMIT_EXPIRE)
		printf(" htable-expire %u", info->cfg.expire);

	if (info->cfg.srcmask != dmask)
		printf(" srcmask %u", info->cfg.srcmask);
	if (info->cfg.dstmask != dmask)
		printf(" dstmask %u", info->cfg.dstmask);
}

static void
hashlimit_mt4_print(const void *ip, const struct xt_entry_match *match,
                   int numeric)
{
	const struct xt_hashlimit_mtinfo1 *info = (const void *)match->data;

	hashlimit_mt_print(info, 32);
}

static void
hashlimit_mt6_print(const void *ip, const struct xt_entry_match *match,
                   int numeric)
{
	const struct xt_hashlimit_mtinfo1 *info = (const void *)match->data;

	hashlimit_mt_print(info, 128);
}

static void hashlimit_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_hashlimit_info *r = (const void *)match->data;

	fputs(" --hashlimit", stdout); print_rate(r->cfg.avg);
	printf(" --hashlimit-burst %u", r->cfg.burst);

	fputs(" --hashlimit-mode", stdout);
	print_mode(r->cfg.mode, ',');
	
	printf(" --hashlimit-name %s", r->name);

	if (r->cfg.size)
		printf(" --hashlimit-htable-size %u", r->cfg.size);
	if (r->cfg.max)
		printf(" --hashlimit-htable-max %u", r->cfg.max);
	if (r->cfg.gc_interval != XT_HASHLIMIT_GCINTERVAL)
		printf(" --hashlimit-htable-gcinterval %u", r->cfg.gc_interval);
	if (r->cfg.expire != XT_HASHLIMIT_EXPIRE)
		printf(" --hashlimit-htable-expire %u", r->cfg.expire);
}

static void
hashlimit_mt_save(const struct xt_hashlimit_mtinfo1 *info, unsigned int dmask)
{
	if (info->cfg.mode & XT_HASHLIMIT_INVERT)
		fputs(" --hashlimit-above", stdout);
	else
		fputs(" --hashlimit-upto", stdout);
	print_rate(info->cfg.avg);
	printf(" --hashlimit-burst %u", info->cfg.burst);

	if (info->cfg.mode & (XT_HASHLIMIT_HASH_SIP | XT_HASHLIMIT_HASH_SPT |
	    XT_HASHLIMIT_HASH_DIP | XT_HASHLIMIT_HASH_DPT)) {
		fputs(" --hashlimit-mode", stdout);
		print_mode(info->cfg.mode, ',');
	}

	printf(" --hashlimit-name %s", info->name);

	if (info->cfg.size != 0)
		printf(" --hashlimit-htable-size %u", info->cfg.size);
	if (info->cfg.max != 0)
		printf(" --hashlimit-htable-max %u", info->cfg.max);
	if (info->cfg.gc_interval != XT_HASHLIMIT_GCINTERVAL)
		printf(" --hashlimit-htable-gcinterval %u", info->cfg.gc_interval);
	if (info->cfg.expire != XT_HASHLIMIT_EXPIRE)
		printf(" --hashlimit-htable-expire %u", info->cfg.expire);

	if (info->cfg.srcmask != dmask)
		printf(" --hashlimit-srcmask %u", info->cfg.srcmask);
	if (info->cfg.dstmask != dmask)
		printf(" --hashlimit-dstmask %u", info->cfg.dstmask);
}

static void
hashlimit_mt4_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_hashlimit_mtinfo1 *info = (const void *)match->data;

	hashlimit_mt_save(info, 32);
}

static void
hashlimit_mt6_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_hashlimit_mtinfo1 *info = (const void *)match->data;

	hashlimit_mt_save(info, 128);
}

static struct xtables_match hashlimit_mt_reg[] = {
	{
		.family        = NFPROTO_UNSPEC,
		.name          = "hashlimit",
		.version       = XTABLES_VERSION,
		.revision      = 0,
		.size          = XT_ALIGN(sizeof(struct xt_hashlimit_info)),
		.userspacesize = offsetof(struct xt_hashlimit_info, hinfo),
		.help          = hashlimit_help,
		.init          = hashlimit_init,
		.x6_parse      = hashlimit_parse,
		.x6_fcheck     = hashlimit_check,
		.print         = hashlimit_print,
		.save          = hashlimit_save,
		.x6_options    = hashlimit_mt_opts,
	},
	{
		.version       = XTABLES_VERSION,
		.name          = "hashlimit",
		.revision      = 1,
		.family        = NFPROTO_IPV4,
		.size          = XT_ALIGN(sizeof(struct xt_hashlimit_mtinfo1)),
		.userspacesize = offsetof(struct xt_hashlimit_mtinfo1, hinfo),
		.help          = hashlimit_mt_help,
		.init          = hashlimit_mt4_init,
		.x6_parse      = hashlimit_mt_parse,
		.x6_fcheck     = hashlimit_check,
		.print         = hashlimit_mt4_print,
		.save          = hashlimit_mt4_save,
		.x6_options    = hashlimit_mt_opts,
	},
	{
		.version       = XTABLES_VERSION,
		.name          = "hashlimit",
		.revision      = 1,
		.family        = NFPROTO_IPV6,
		.size          = XT_ALIGN(sizeof(struct xt_hashlimit_mtinfo1)),
		.userspacesize = offsetof(struct xt_hashlimit_mtinfo1, hinfo),
		.help          = hashlimit_mt_help,
		.init          = hashlimit_mt6_init,
		.x6_parse      = hashlimit_mt_parse,
		.x6_fcheck     = hashlimit_check,
		.print         = hashlimit_mt6_print,
		.save          = hashlimit_mt6_save,
		.x6_options    = hashlimit_mt_opts,
	},
};

void _init(void)
{
	xtables_register_matches(hashlimit_mt_reg, ARRAY_SIZE(hashlimit_mt_reg));
}
