#include <math.h>
#include <stdio.h>
#include <string.h>
#include <xtables.h>
#include <linux/netfilter/xt_statistic.h>

enum {
	O_MODE = 0,
	O_PROBABILITY,
	O_EVERY,
	O_PACKET,
	F_PROBABILITY = 1 << O_PROBABILITY,
	F_EVERY       = 1 << O_EVERY,
	F_PACKET      = 1 << O_PACKET,
};

static void statistic_help(void)
{
	printf(
"statistic match options:\n"
" --mode mode                    Match mode (random, nth)\n"
" random mode:\n"
"[!] --probability p		 Probability\n"
" nth mode:\n"
"[!] --every n			 Match every nth packet\n"
" --packet p			 Initial counter value (0 <= p <= n-1, default 0)\n");
}

#define s struct xt_statistic_info
static const struct xt_option_entry statistic_opts[] = {
	{.name = "mode", .id = O_MODE, .type = XTTYPE_STRING,
	 .flags = XTOPT_MAND},
	{.name = "probability", .id = O_PROBABILITY, .type = XTTYPE_DOUBLE,
	 .flags = XTOPT_INVERT, .min = 0, .max = 1,
	 .excl = F_EVERY | F_PACKET},
	{.name = "every", .id = O_EVERY, .type = XTTYPE_UINT32, .min = 1,
	 .flags = XTOPT_INVERT | XTOPT_PUT, XTOPT_POINTER(s, u.nth.every),
	 .excl = F_PROBABILITY, .also = F_PACKET},
	{.name = "packet", .id = O_PACKET, .type = XTTYPE_UINT32,
	 .flags = XTOPT_PUT, XTOPT_POINTER(s, u.nth.packet),
	 .excl = F_PROBABILITY, .also = F_EVERY},
	XTOPT_TABLEEND,
};
#undef s

static void statistic_parse(struct xt_option_call *cb)
{
	struct xt_statistic_info *info = cb->data;

	if (cb->invert)
		info->flags |= XT_STATISTIC_INVERT;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_MODE:
		if (strcmp(cb->arg, "random") == 0)
			info->mode = XT_STATISTIC_MODE_RANDOM;
		else if (strcmp(cb->arg, "nth") == 0)
			info->mode = XT_STATISTIC_MODE_NTH;
		else
			xtables_error(PARAMETER_PROBLEM, "Bad mode \"%s\"",
				cb->arg);
		break;
	case O_PROBABILITY:
		info->u.random.probability = lround(0x80000000 * cb->val.dbl);
		break;
	case O_EVERY:
		--info->u.nth.every;
		break;
	}
}

static void statistic_check(struct xt_fcheck_call *cb)
{
	struct xt_statistic_info *info = cb->data;

	if (info->mode == XT_STATISTIC_MODE_RANDOM &&
	    !(cb->xflags & F_PROBABILITY))
		xtables_error(PARAMETER_PROBLEM,
			"--probability must be specified when using "
			"random mode");
	if (info->mode == XT_STATISTIC_MODE_NTH &&
	    !(cb->xflags & (F_EVERY | F_PACKET)))
		xtables_error(PARAMETER_PROBLEM,
			"--every and --packet must be specified when "
			"using nth mode");

	/* at this point, info->u.nth.every have been decreased. */
	if (info->u.nth.packet > info->u.nth.every)
		xtables_error(PARAMETER_PROBLEM,
			  "the --packet p must be 0 <= p <= n-1");

	info->u.nth.count = info->u.nth.every - info->u.nth.packet;
}

static void print_match(const struct xt_statistic_info *info, char *prefix)
{
	switch (info->mode) {
	case XT_STATISTIC_MODE_RANDOM:
		printf(" %smode random%s %sprobability %.11f", prefix,
		       (info->flags & XT_STATISTIC_INVERT) ? " !" : "",
		       prefix,
		       1.0 * info->u.random.probability / 0x80000000);
		break;
	case XT_STATISTIC_MODE_NTH:
		printf(" %smode nth%s %severy %u", prefix,
		       (info->flags & XT_STATISTIC_INVERT) ? " !" : "",
		       prefix,
		       info->u.nth.every + 1);
		if (info->u.nth.packet)
			printf(" %spacket %u", prefix, info->u.nth.packet);
		break;
	}
}

static void
statistic_print(const void *ip, const struct xt_entry_match *match, int numeric)
{
	const struct xt_statistic_info *info = (const void *)match->data;

	printf(" statistic");
	print_match(info, "");
}

static void statistic_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_statistic_info *info = (const void *)match->data;

	print_match(info, "--");
}

static struct xtables_match statistic_match = {
	.family		= NFPROTO_UNSPEC,
	.name		= "statistic",
	.version	= XTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_statistic_info)),
	.userspacesize	= offsetof(struct xt_statistic_info, u.nth.count),
	.help		= statistic_help,
	.x6_parse	= statistic_parse,
	.x6_fcheck	= statistic_check,
	.print		= statistic_print,
	.save		= statistic_save,
	.x6_options	= statistic_opts,
};

void _init(void)
{
	xtables_register_match(&statistic_match);
}
