#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <getopt.h>

#include <xtables.h>
#include <linux/netfilter/xt_rateest.h>

/* Ugly hack to pass info to final_check function. We should fix the API */
static struct xt_rateest_match_info *rateest_info;

static void rateest_help(void)
{
	printf(
"rateest match options:\n"
" --rateest1 name		Rate estimator name\n"
" --rateest2 name		Rate estimator name\n"
" --rateest-delta		Compare difference(s) to given rate(s)\n"
" --rateest-bps1 [bps]		Compare bps\n"
" --rateest-pps1 [pps]		Compare pps\n"
" --rateest-bps2 [bps]		Compare bps\n"
" --rateest-pps2 [pps]		Compare pps\n"
" [!] --rateest-lt		Match if rate is less than given rate/estimator\n"
" [!] --rateest-gt		Match if rate is greater than given rate/estimator\n"
" [!] --rateest-eq		Match if rate is equal to given rate/estimator\n");
}

enum rateest_options {
	OPT_RATEEST1,
	OPT_RATEEST2,
	OPT_RATEEST_BPS1,
	OPT_RATEEST_PPS1,
	OPT_RATEEST_BPS2,
	OPT_RATEEST_PPS2,
	OPT_RATEEST_DELTA,
	OPT_RATEEST_LT,
	OPT_RATEEST_GT,
	OPT_RATEEST_EQ,
};

static const struct option rateest_opts[] = {
	{.name = "rateest1",      .has_arg = true,  .val = OPT_RATEEST1},
	{.name = "rateest",       .has_arg = true,  .val = OPT_RATEEST1}, /* alias for absolute mode */
	{.name = "rateest2",      .has_arg = true,  .val = OPT_RATEEST2},
	{.name = "rateest-bps1",  .has_arg = false, .val = OPT_RATEEST_BPS1},
	{.name = "rateest-pps1",  .has_arg = false, .val = OPT_RATEEST_PPS1},
	{.name = "rateest-bps2",  .has_arg = false, .val = OPT_RATEEST_BPS2},
	{.name = "rateest-pps2",  .has_arg = false, .val = OPT_RATEEST_PPS2},
	{.name = "rateest-bps",   .has_arg = false, .val = OPT_RATEEST_BPS2}, /* alias for absolute mode */
	{.name = "rateest-pps",   .has_arg = false, .val = OPT_RATEEST_PPS2}, /* alias for absolute mode */
	{.name = "rateest-delta", .has_arg = false, .val = OPT_RATEEST_DELTA},
	{.name = "rateest-lt",    .has_arg = false, .val = OPT_RATEEST_LT},
	{.name = "rateest-gt",    .has_arg = false, .val = OPT_RATEEST_GT},
	{.name = "rateest-eq",    .has_arg = false, .val = OPT_RATEEST_EQ},
	XT_GETOPT_TABLEEND,
};

/* Copied from iproute. See http://physics.nist.gov/cuu/Units/binary.html */
static const struct rate_suffix {
	const char *name;
	double scale;
} suffixes[] = {
	{ "bit",	1. },
	{ "Kibit",	1024. },
	{ "kbit",	1000. },
	{ "Mibit",	1024.*1024. },
	{ "mbit",	1000000. },
	{ "Gibit",	1024.*1024.*1024. },
	{ "gbit",	1000000000. },
	{ "Tibit",	1024.*1024.*1024.*1024. },
	{ "tbit",	1000000000000. },
	{ "Bps",	8. },
	{ "KiBps",	8.*1024. },
	{ "KBps",	8000. },
	{ "MiBps",	8.*1024*1024. },
	{ "MBps",	8000000. },
	{ "GiBps",	8.*1024.*1024.*1024. },
	{ "GBps",	8000000000. },
	{ "TiBps",	8.*1024.*1024.*1024.*1024. },
	{ "TBps",	8000000000000. },
	{NULL},
};

static int
rateest_get_rate(uint32_t *rate, const char *str)
{
	char *p;
	double bps = strtod(str, &p);
	const struct rate_suffix *s;

	if (p == str)
		return -1;

	if (*p == '\0') {
		*rate = bps / 8.;	/* assume bytes/sec */
		return 0;
	}

	for (s = suffixes; s->name; ++s) {
		if (strcasecmp(s->name, p) == 0) {
			*rate = (bps * s->scale) / 8.;
			return 0;
		}
	}

	return -1;
}

static int
rateest_parse(int c, char **argv, int invert, unsigned int *flags,
	      const void *entry, struct xt_entry_match **match)
{
	struct xt_rateest_match_info *info = (void *)(*match)->data;
	unsigned int val;

	rateest_info = info;

	switch (c) {
	case OPT_RATEEST1:
		xtables_check_inverse(optarg, &invert, &optind, 0, argv);
		if (invert)
			xtables_error(PARAMETER_PROBLEM,
				   "rateest: rateest can't be inverted");

		if (*flags & (1 << c))
			xtables_error(PARAMETER_PROBLEM,
				   "rateest: can't specify --rateest1 twice");
		*flags |= 1 << c;

		strncpy(info->name1, optarg, sizeof(info->name1) - 1);
		break;

	case OPT_RATEEST2:
		xtables_check_inverse(optarg, &invert, &optind, 0, argv);
		if (invert)
			xtables_error(PARAMETER_PROBLEM,
				   "rateest: rateest can't be inverted");

		if (*flags & (1 << c))
			xtables_error(PARAMETER_PROBLEM,
				   "rateest: can't specify --rateest2 twice");
		*flags |= 1 << c;

		strncpy(info->name2, optarg, sizeof(info->name2) - 1);
		info->flags |= XT_RATEEST_MATCH_REL;
		break;

	case OPT_RATEEST_BPS1:
		xtables_check_inverse(optarg, &invert, &optind, 0, argv);
		if (invert)
			xtables_error(PARAMETER_PROBLEM,
				   "rateest: rateest-bps can't be inverted");

		if (*flags & (1 << c))
			xtables_error(PARAMETER_PROBLEM,
				   "rateest: can't specify --rateest-bps1 twice");
		*flags |= 1 << c;

		info->flags |= XT_RATEEST_MATCH_BPS;

		/* The rate is optional and only required in absolute mode */
		if (!argv[optind] || *argv[optind] == '-' || *argv[optind] == '!')
			break;

		if (rateest_get_rate(&info->bps1, argv[optind]) < 0)
			xtables_error(PARAMETER_PROBLEM,
				   "rateest: could not parse rate `%s'",
				   argv[optind]);
		optind++;
		break;

	case OPT_RATEEST_PPS1:
		xtables_check_inverse(optarg, &invert, &optind, 0, argv);
		if (invert)
			xtables_error(PARAMETER_PROBLEM,
				   "rateest: rateest-pps can't be inverted");

		if (*flags & (1 << c))
			xtables_error(PARAMETER_PROBLEM,
				   "rateest: can't specify --rateest-pps1 twice");
		*flags |= 1 << c;

		info->flags |= XT_RATEEST_MATCH_PPS;

		/* The rate is optional and only required in absolute mode */
		if (!argv[optind] || *argv[optind] == '-' || *argv[optind] == '!')
			break;

		if (!xtables_strtoui(argv[optind], NULL, &val, 0, UINT32_MAX))
			xtables_error(PARAMETER_PROBLEM,
				   "rateest: could not parse pps `%s'",
				   argv[optind]);
		info->pps1 = val;
		optind++;
		break;

	case OPT_RATEEST_BPS2:
		xtables_check_inverse(optarg, &invert, &optind, 0, argv);
		if (invert)
			xtables_error(PARAMETER_PROBLEM,
				   "rateest: rateest-bps can't be inverted");

		if (*flags & (1 << c))
			xtables_error(PARAMETER_PROBLEM,
				   "rateest: can't specify --rateest-bps2 twice");
		*flags |= 1 << c;

		info->flags |= XT_RATEEST_MATCH_BPS;

		/* The rate is optional and only required in absolute mode */
		if (!argv[optind] || *argv[optind] == '-' || *argv[optind] == '!')
			break;

		if (rateest_get_rate(&info->bps2, argv[optind]) < 0)
			xtables_error(PARAMETER_PROBLEM,
				   "rateest: could not parse rate `%s'",
				   argv[optind]);
		optind++;
		break;

	case OPT_RATEEST_PPS2:
		xtables_check_inverse(optarg, &invert, &optind, 0, argv);
		if (invert)
			xtables_error(PARAMETER_PROBLEM,
				   "rateest: rateest-pps can't be inverted");

		if (*flags & (1 << c))
			xtables_error(PARAMETER_PROBLEM,
				   "rateest: can't specify --rateest-pps2 twice");
		*flags |= 1 << c;

		info->flags |= XT_RATEEST_MATCH_PPS;

		/* The rate is optional and only required in absolute mode */
		if (!argv[optind] || *argv[optind] == '-' || *argv[optind] == '!')
			break;

		if (!xtables_strtoui(argv[optind], NULL, &val, 0, UINT32_MAX))
			xtables_error(PARAMETER_PROBLEM,
				   "rateest: could not parse pps `%s'",
				   argv[optind]);
		info->pps2 = val;
		optind++;
		break;

	case OPT_RATEEST_DELTA:
		xtables_check_inverse(optarg, &invert, &optind, 0, argv);
		if (invert)
			xtables_error(PARAMETER_PROBLEM,
				   "rateest: rateest-delta can't be inverted");

		if (*flags & (1 << c))
			xtables_error(PARAMETER_PROBLEM,
				   "rateest: can't specify --rateest-delta twice");
		*flags |= 1 << c;

		info->flags |= XT_RATEEST_MATCH_DELTA;
		break;

	case OPT_RATEEST_EQ:
		xtables_check_inverse(optarg, &invert, &optind, 0, argv);

		if (*flags & (1 << c))
			xtables_error(PARAMETER_PROBLEM,
				   "rateest: can't specify lt/gt/eq twice");
		*flags |= 1 << c;

		info->mode = XT_RATEEST_MATCH_EQ;
		if (invert)
			info->flags |= XT_RATEEST_MATCH_INVERT;
		break;

	case OPT_RATEEST_LT:
		xtables_check_inverse(optarg, &invert, &optind, 0, argv);

		if (*flags & (1 << c))
			xtables_error(PARAMETER_PROBLEM,
				   "rateest: can't specify lt/gt/eq twice");
		*flags |= 1 << c;

		info->mode = XT_RATEEST_MATCH_LT;
		if (invert)
			info->flags |= XT_RATEEST_MATCH_INVERT;
		break;

	case OPT_RATEEST_GT:
		xtables_check_inverse(optarg, &invert, &optind, 0, argv);

		if (*flags & (1 << c))
			xtables_error(PARAMETER_PROBLEM,
				   "rateest: can't specify lt/gt/eq twice");
		*flags |= 1 << c;

		info->mode = XT_RATEEST_MATCH_GT;
		if (invert)
			info->flags |= XT_RATEEST_MATCH_INVERT;
		break;
	}

	return 1;
}

static void
rateest_final_check(unsigned int flags)
{
	struct xt_rateest_match_info *info = rateest_info;

	if (info == NULL)
		xtables_error(PARAMETER_PROBLEM, "rateest match: "
		           "you need to specify some flags");
	if (!(info->flags & XT_RATEEST_MATCH_REL))
		info->flags |= XT_RATEEST_MATCH_ABS;
}

static void
rateest_print_rate(uint32_t rate, int numeric)
{
	double tmp = (double)rate*8;

	if (numeric)
		printf(" %u", rate);
	else if (tmp >= 1000.0*1000000.0)
		printf(" %.0fMbit", tmp/1000000.0);
	else if (tmp >= 1000.0 * 1000.0)
		printf(" %.0fKbit", tmp/1000.0);
	else
		printf(" %.0fbit", tmp);
}

static void
rateest_print_mode(const struct xt_rateest_match_info *info,
                   const char *prefix)
{
	if (info->flags & XT_RATEEST_MATCH_INVERT)
		printf(" !");

	switch (info->mode) {
	case XT_RATEEST_MATCH_EQ:
		printf(" %seq", prefix);
		break;
	case XT_RATEEST_MATCH_LT:
		printf(" %slt", prefix);
		break;
	case XT_RATEEST_MATCH_GT:
		printf(" %sgt", prefix);
		break;
	default:
		exit(1);
	}
}

static void
rateest_print(const void *ip, const struct xt_entry_match *match, int numeric)
{
	const struct xt_rateest_match_info *info = (const void *)match->data;

	printf(" rateest match ");

	printf("%s", info->name1);
	if (info->flags & XT_RATEEST_MATCH_DELTA)
		printf(" delta");

	if (info->flags & XT_RATEEST_MATCH_BPS) {
		printf(" bps");
		if (info->flags & XT_RATEEST_MATCH_DELTA)
			rateest_print_rate(info->bps1, numeric);
		if (info->flags & XT_RATEEST_MATCH_ABS) {
			rateest_print_mode(info, "");
			rateest_print_rate(info->bps2, numeric);
		}
	}
	if (info->flags & XT_RATEEST_MATCH_PPS) {
		printf(" pps");
		if (info->flags & XT_RATEEST_MATCH_DELTA)
			printf(" %u", info->pps1);
		if (info->flags & XT_RATEEST_MATCH_ABS) {
			rateest_print_mode(info, "");
			printf(" %u", info->pps2);
		}
	}

	if (info->flags & XT_RATEEST_MATCH_REL) {
		rateest_print_mode(info, "");

		printf(" %s", info->name2);
		if (info->flags & XT_RATEEST_MATCH_DELTA)
			printf(" delta");

		if (info->flags & XT_RATEEST_MATCH_BPS) {
			printf(" bps");
			if (info->flags & XT_RATEEST_MATCH_DELTA)
				rateest_print_rate(info->bps2, numeric);
		}
		if (info->flags & XT_RATEEST_MATCH_PPS) {
			printf(" pps");
			if (info->flags & XT_RATEEST_MATCH_DELTA)
				printf(" %u", info->pps2);
		}
	}
}

static void
rateest_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_rateest_match_info *info = (const void *)match->data;

	if (info->flags & XT_RATEEST_MATCH_REL) {
		printf(" --rateest1 %s", info->name1);
		if (info->flags & XT_RATEEST_MATCH_BPS)
			printf(" --rateest-bps");
		if (info->flags & XT_RATEEST_MATCH_PPS)
			printf(" --rateest-pps");
		rateest_print_mode(info, " --rateest-");
		printf(" --rateest2 %s", info->name2);
	} else {
		printf(" --rateest %s", info->name1);
		if (info->flags & XT_RATEEST_MATCH_BPS) {
			printf(" --rateest-bps1");
			rateest_print_rate(info->bps1, 0);
			printf(" --rateest-bps2");
			rateest_print_rate(info->bps2, 0);
			rateest_print_mode(info, "--rateest-");
		}
		if (info->flags & XT_RATEEST_MATCH_PPS) {
			printf(" --rateest-pps");
			rateest_print_mode(info, "--rateest-");
			printf(" %u", info->pps2);
		}
	}
}

static struct xtables_match rateest_mt_reg = {
	.family		= NFPROTO_UNSPEC,
	.name		= "rateest",
	.version	= XTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_rateest_match_info)),
	.userspacesize	= XT_ALIGN(offsetof(struct xt_rateest_match_info, est1)),
	.help		= rateest_help,
	.parse		= rateest_parse,
	.final_check	= rateest_final_check,
	.print		= rateest_print,
	.save		= rateest_save,
	.extra_opts	= rateest_opts,
};

void _init(void)
{
	xtables_register_match(&rateest_mt_reg);
}
