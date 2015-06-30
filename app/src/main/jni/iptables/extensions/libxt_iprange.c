#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <xtables.h>
#include <linux/netfilter.h>
#include <linux/netfilter/xt_iprange.h>

struct ipt_iprange {
	/* Inclusive: network order. */
	__be32 min_ip, max_ip;
};

struct ipt_iprange_info {
	struct ipt_iprange src;
	struct ipt_iprange dst;

	/* Flags from above */
	uint8_t flags;
};

enum {
	O_SRC_RANGE = 0,
	O_DST_RANGE,
};

static void iprange_mt_help(void)
{
	printf(
"iprange match options:\n"
"[!] --src-range ip[-ip]    Match source IP in the specified range\n"
"[!] --dst-range ip[-ip]    Match destination IP in the specified range\n");
}

static const struct xt_option_entry iprange_mt_opts[] = {
	{.name = "src-range", .id = O_SRC_RANGE, .type = XTTYPE_STRING,
	 .flags = XTOPT_INVERT},
	{.name = "dst-range", .id = O_DST_RANGE, .type = XTTYPE_STRING,
	 .flags = XTOPT_INVERT},
	XTOPT_TABLEEND,
};

static void
iprange_parse_spec(const char *from, const char *to, union nf_inet_addr *range,
		   uint8_t family, const char *optname)
{
	const char *spec[2] = {from, to};
	struct in6_addr *ia6;
	struct in_addr *ia4;
	unsigned int i;

	memset(range, 0, sizeof(union nf_inet_addr) * 2);

	if (family == NFPROTO_IPV6) {
		for (i = 0; i < ARRAY_SIZE(spec); ++i) {
			ia6 = xtables_numeric_to_ip6addr(spec[i]);
			if (ia6 == NULL)
				xtables_param_act(XTF_BAD_VALUE, "iprange",
					optname, spec[i]);
			range[i].in6 = *ia6;
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(spec); ++i) {
			ia4 = xtables_numeric_to_ipaddr(spec[i]);
			if (ia4 == NULL)
				xtables_param_act(XTF_BAD_VALUE, "iprange",
					optname, spec[i]);
			range[i].in = *ia4;
		}
	}
}

static void iprange_parse_range(const char *oarg, union nf_inet_addr *range,
				uint8_t family, const char *optname)
{
	char *arg = strdup(oarg);
	char *dash;

	if (arg == NULL)
		xtables_error(RESOURCE_PROBLEM, "strdup");
	dash = strchr(arg, '-');
	if (dash == NULL) {
		iprange_parse_spec(arg, arg, range, family, optname);
		free(arg);
		return;
	}

	*dash = '\0';
	iprange_parse_spec(arg, dash + 1, range, family, optname);
	if (memcmp(&range[0], &range[1], sizeof(*range)) > 0)
		fprintf(stderr, "xt_iprange: range %s-%s is reversed and "
			"will never match\n", arg, dash + 1);
	free(arg);
}

static void iprange_parse(struct xt_option_call *cb)
{
	struct ipt_iprange_info *info = cb->data;
	union nf_inet_addr range[2];

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_SRC_RANGE:
		info->flags |= IPRANGE_SRC;
		if (cb->invert)
			info->flags |= IPRANGE_SRC_INV;
		iprange_parse_range(cb->arg, range, NFPROTO_IPV4, "--src-range");
		info->src.min_ip = range[0].ip;
		info->src.max_ip = range[1].ip;
		break;
	case O_DST_RANGE:
		info->flags |= IPRANGE_DST;
		if (cb->invert)
			info->flags |= IPRANGE_DST_INV;
		iprange_parse_range(cb->arg, range, NFPROTO_IPV4, "--dst-range");
		info->dst.min_ip = range[0].ip;
		info->dst.max_ip = range[1].ip;
		break;
	}
}

static void iprange_mt_parse(struct xt_option_call *cb, uint8_t nfproto)
{
	struct xt_iprange_mtinfo *info = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_SRC_RANGE:
		iprange_parse_range(cb->arg, &info->src_min, nfproto,
			"--src-range");
		info->flags |= IPRANGE_SRC;
		if (cb->invert)
			info->flags |= IPRANGE_SRC_INV;
		break;
	case O_DST_RANGE:
		iprange_parse_range(cb->arg, &info->dst_min, nfproto,
			"--dst-range");
		info->flags |= IPRANGE_DST;
		if (cb->invert)
			info->flags |= IPRANGE_DST_INV;
		break;
	}
}

static void iprange_mt4_parse(struct xt_option_call *cb)
{
	iprange_mt_parse(cb, NFPROTO_IPV4);
}

static void iprange_mt6_parse(struct xt_option_call *cb)
{
	iprange_mt_parse(cb, NFPROTO_IPV6);
}

static void iprange_mt_check(struct xt_fcheck_call *cb)
{
	if (cb->xflags == 0)
		xtables_error(PARAMETER_PROBLEM,
			   "iprange match: You must specify `--src-range' or `--dst-range'");
}

static void
print_iprange(const struct ipt_iprange *range)
{
	const unsigned char *byte_min, *byte_max;

	byte_min = (const unsigned char *)&range->min_ip;
	byte_max = (const unsigned char *)&range->max_ip;
	printf(" %u.%u.%u.%u-%u.%u.%u.%u",
		byte_min[0], byte_min[1], byte_min[2], byte_min[3],
		byte_max[0], byte_max[1], byte_max[2], byte_max[3]);
}

static void iprange_print(const void *ip, const struct xt_entry_match *match,
                          int numeric)
{
	const struct ipt_iprange_info *info = (const void *)match->data;

	if (info->flags & IPRANGE_SRC) {
		printf(" source IP range");
		if (info->flags & IPRANGE_SRC_INV)
			printf(" !");
		print_iprange(&info->src);
	}
	if (info->flags & IPRANGE_DST) {
		printf(" destination IP range");
		if (info->flags & IPRANGE_DST_INV)
			printf(" !");
		print_iprange(&info->dst);
	}
}

static void
iprange_mt4_print(const void *ip, const struct xt_entry_match *match,
                  int numeric)
{
	const struct xt_iprange_mtinfo *info = (const void *)match->data;

	if (info->flags & IPRANGE_SRC) {
		printf(" source IP range");
		if (info->flags & IPRANGE_SRC_INV)
			printf(" !");
		/*
		 * ipaddr_to_numeric() uses a static buffer, so cannot
		 * combine the printf() calls.
		 */
		printf(" %s", xtables_ipaddr_to_numeric(&info->src_min.in));
		printf("-%s", xtables_ipaddr_to_numeric(&info->src_max.in));
	}
	if (info->flags & IPRANGE_DST) {
		printf(" destination IP range");
		if (info->flags & IPRANGE_DST_INV)
			printf(" !");
		printf(" %s", xtables_ipaddr_to_numeric(&info->dst_min.in));
		printf("-%s", xtables_ipaddr_to_numeric(&info->dst_max.in));
	}
}

static void
iprange_mt6_print(const void *ip, const struct xt_entry_match *match,
                  int numeric)
{
	const struct xt_iprange_mtinfo *info = (const void *)match->data;

	if (info->flags & IPRANGE_SRC) {
		printf(" source IP range");
		if (info->flags & IPRANGE_SRC_INV)
			printf(" !");
		/*
		 * ipaddr_to_numeric() uses a static buffer, so cannot
		 * combine the printf() calls.
		 */
		printf(" %s", xtables_ip6addr_to_numeric(&info->src_min.in6));
		printf("-%s", xtables_ip6addr_to_numeric(&info->src_max.in6));
	}
	if (info->flags & IPRANGE_DST) {
		printf(" destination IP range");
		if (info->flags & IPRANGE_DST_INV)
			printf(" !");
		printf(" %s", xtables_ip6addr_to_numeric(&info->dst_min.in6));
		printf("-%s", xtables_ip6addr_to_numeric(&info->dst_max.in6));
	}
}

static void iprange_save(const void *ip, const struct xt_entry_match *match)
{
	const struct ipt_iprange_info *info = (const void *)match->data;

	if (info->flags & IPRANGE_SRC) {
		if (info->flags & IPRANGE_SRC_INV)
			printf(" !");
		printf(" --src-range");
		print_iprange(&info->src);
	}
	if (info->flags & IPRANGE_DST) {
		if (info->flags & IPRANGE_DST_INV)
			printf(" !");
		printf(" --dst-range");
		print_iprange(&info->dst);
	}
}

static void iprange_mt4_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_iprange_mtinfo *info = (const void *)match->data;

	if (info->flags & IPRANGE_SRC) {
		if (info->flags & IPRANGE_SRC_INV)
			printf(" !");
		printf(" --src-range %s", xtables_ipaddr_to_numeric(&info->src_min.in));
		printf("-%s", xtables_ipaddr_to_numeric(&info->src_max.in));
	}
	if (info->flags & IPRANGE_DST) {
		if (info->flags & IPRANGE_DST_INV)
			printf(" !");
		printf(" --dst-range %s", xtables_ipaddr_to_numeric(&info->dst_min.in));
		printf("-%s", xtables_ipaddr_to_numeric(&info->dst_max.in));
	}
}

static void iprange_mt6_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_iprange_mtinfo *info = (const void *)match->data;

	if (info->flags & IPRANGE_SRC) {
		if (info->flags & IPRANGE_SRC_INV)
			printf(" !");
		printf(" --src-range %s", xtables_ip6addr_to_numeric(&info->src_min.in6));
		printf("-%s", xtables_ip6addr_to_numeric(&info->src_max.in6));
	}
	if (info->flags & IPRANGE_DST) {
		if (info->flags & IPRANGE_DST_INV)
			printf(" !");
		printf(" --dst-range %s", xtables_ip6addr_to_numeric(&info->dst_min.in6));
		printf("-%s", xtables_ip6addr_to_numeric(&info->dst_max.in6));
	}
}

static struct xtables_match iprange_mt_reg[] = {
	{
		.version       = XTABLES_VERSION,
		.name          = "iprange",
		.revision      = 0,
		.family        = NFPROTO_IPV4,
		.size          = XT_ALIGN(sizeof(struct ipt_iprange_info)),
		.userspacesize = XT_ALIGN(sizeof(struct ipt_iprange_info)),
		.help          = iprange_mt_help,
		.x6_parse      = iprange_parse,
		.x6_fcheck     = iprange_mt_check,
		.print         = iprange_print,
		.save          = iprange_save,
		.x6_options    = iprange_mt_opts,
	},
	{
		.version       = XTABLES_VERSION,
		.name          = "iprange",
		.revision      = 1,
		.family        = NFPROTO_IPV4,
		.size          = XT_ALIGN(sizeof(struct xt_iprange_mtinfo)),
		.userspacesize = XT_ALIGN(sizeof(struct xt_iprange_mtinfo)),
		.help          = iprange_mt_help,
		.x6_parse      = iprange_mt4_parse,
		.x6_fcheck     = iprange_mt_check,
		.print         = iprange_mt4_print,
		.save          = iprange_mt4_save,
		.x6_options    = iprange_mt_opts,
	},
	{
		.version       = XTABLES_VERSION,
		.name          = "iprange",
		.revision      = 1,
		.family        = NFPROTO_IPV6,
		.size          = XT_ALIGN(sizeof(struct xt_iprange_mtinfo)),
		.userspacesize = XT_ALIGN(sizeof(struct xt_iprange_mtinfo)),
		.help          = iprange_mt_help,
		.x6_parse      = iprange_mt6_parse,
		.x6_fcheck     = iprange_mt_check,
		.print         = iprange_mt6_print,
		.save          = iprange_mt6_save,
		.x6_options    = iprange_mt_opts,
	},
};

void _init(void)
{
	xtables_register_matches(iprange_mt_reg, ARRAY_SIZE(iprange_mt_reg));
}
