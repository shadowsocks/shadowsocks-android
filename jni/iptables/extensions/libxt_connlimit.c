#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <xtables.h>
#include <linux/netfilter/xt_connlimit.h>

enum {
	O_UPTO = 0,
	O_ABOVE,
	O_MASK,
	O_SADDR,
	O_DADDR,
	F_UPTO  = 1 << O_UPTO,
	F_ABOVE = 1 << O_ABOVE,
	F_MASK  = 1 << O_MASK,
	F_SADDR = 1 << O_SADDR,
	F_DADDR = 1 << O_DADDR,
};

static void connlimit_help(void)
{
	printf(
"connlimit match options:\n"
"  --connlimit-upto n     match if the number of existing connections is 0..n\n"
"  --connlimit-above n    match if the number of existing connections is >n\n"
"  --connlimit-mask n     group hosts using prefix length (default: max len)\n"
"  --connlimit-saddr      select source address for grouping\n"
"  --connlimit-daddr      select destination addresses for grouping\n");
}

#define s struct xt_connlimit_info
static const struct xt_option_entry connlimit_opts[] = {
	{.name = "connlimit-upto", .id = O_UPTO, .excl = F_ABOVE,
	 .type = XTTYPE_UINT32, .flags = XTOPT_INVERT | XTOPT_PUT,
	 XTOPT_POINTER(s, limit)},
	{.name = "connlimit-above", .id = O_ABOVE, .excl = F_UPTO,
	 .type = XTTYPE_UINT32, .flags = XTOPT_INVERT | XTOPT_PUT,
	 XTOPT_POINTER(s, limit)},
	{.name = "connlimit-mask", .id = O_MASK, .type = XTTYPE_PLENMASK,
	 .flags = XTOPT_PUT, XTOPT_POINTER(s, mask)},
	{.name = "connlimit-saddr", .id = O_SADDR, .excl = F_DADDR,
	 .type = XTTYPE_NONE},
	{.name = "connlimit-daddr", .id = O_DADDR, .excl = F_SADDR,
	 .type = XTTYPE_NONE},
	XTOPT_TABLEEND,
};
#undef s

static void connlimit_init(struct xt_entry_match *match)
{
	struct xt_connlimit_info *info = (void *)match->data;

	/* This will also initialize the v4 mask correctly */
	memset(info->v6_mask, 0xFF, sizeof(info->v6_mask));
}

static void connlimit_parse(struct xt_option_call *cb, uint8_t family)
{
	struct xt_connlimit_info *info = cb->data;
	const unsigned int revision = (*cb->match)->u.user.revision;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_ABOVE:
		if (cb->invert)
			info->flags |= XT_CONNLIMIT_INVERT;
		break;
	case O_UPTO:
		if (!cb->invert)
			info->flags |= XT_CONNLIMIT_INVERT;
		break;
	case O_SADDR:
		if (revision < 1)
			xtables_error(PARAMETER_PROBLEM,
				"xt_connlimit.0 does not support "
				"--connlimit-daddr");
		info->flags &= ~XT_CONNLIMIT_DADDR;
		break;
	case O_DADDR:
		if (revision < 1)
			xtables_error(PARAMETER_PROBLEM,
				"xt_connlimit.0 does not support "
				"--connlimit-daddr");
		info->flags |= XT_CONNLIMIT_DADDR;
		break;
	}
}

static void connlimit_parse4(struct xt_option_call *cb)
{
	return connlimit_parse(cb, NFPROTO_IPV4);
}

static void connlimit_parse6(struct xt_option_call *cb)
{
	return connlimit_parse(cb, NFPROTO_IPV6);
}

static void connlimit_check(struct xt_fcheck_call *cb)
{
	if ((cb->xflags & (F_UPTO | F_ABOVE)) == 0)
		xtables_error(PARAMETER_PROBLEM,
			"You must specify \"--connlimit-above\" or "
			"\"--connlimit-upto\".");
}

static unsigned int count_bits4(uint32_t mask)
{
	unsigned int bits = 0;

	for (mask = ~ntohl(mask); mask != 0; mask >>= 1)
		++bits;

	return 32 - bits;
}

static unsigned int count_bits6(const uint32_t *mask)
{
	unsigned int bits = 0, i;
	uint32_t tmp[4];

	for (i = 0; i < 4; ++i)
		for (tmp[i] = ~ntohl(mask[i]); tmp[i] != 0; tmp[i] >>= 1)
			++bits;
	return 128 - bits;
}

static void connlimit_print4(const void *ip,
                             const struct xt_entry_match *match, int numeric)
{
	const struct xt_connlimit_info *info = (const void *)match->data;

	printf(" #conn %s/%u %s %u",
	       (info->flags & XT_CONNLIMIT_DADDR) ? "dst" : "src",
	       count_bits4(info->v4_mask),
	       (info->flags & XT_CONNLIMIT_INVERT) ? "<=" : ">", info->limit);
}

static void connlimit_print6(const void *ip,
                             const struct xt_entry_match *match, int numeric)
{
	const struct xt_connlimit_info *info = (const void *)match->data;

	printf(" #conn %s/%u %s %u",
	       (info->flags & XT_CONNLIMIT_DADDR) ? "dst" : "src",
	       count_bits6(info->v6_mask),
	       (info->flags & XT_CONNLIMIT_INVERT) ? "<=" : ">", info->limit);
}

static void connlimit_save4(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_connlimit_info *info = (const void *)match->data;
	const int revision = match->u.user.revision;

	if (info->flags & XT_CONNLIMIT_INVERT)
		printf(" --connlimit-upto %u", info->limit);
	else
		printf(" --connlimit-above %u", info->limit);
	printf(" --connlimit-mask %u", count_bits4(info->v4_mask));
	if (revision >= 1) {
		if (info->flags & XT_CONNLIMIT_DADDR)
			printf(" --connlimit-daddr");
		else
			printf(" --connlimit-saddr");
	}
}

static void connlimit_save6(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_connlimit_info *info = (const void *)match->data;
	const int revision = match->u.user.revision;

	if (info->flags & XT_CONNLIMIT_INVERT)
		printf(" --connlimit-upto %u", info->limit);
	else
		printf(" --connlimit-above %u", info->limit);
	printf(" --connlimit-mask %u", count_bits6(info->v6_mask));
	if (revision >= 1) {
		if (info->flags & XT_CONNLIMIT_DADDR)
			printf(" --connlimit-daddr");
		else
			printf(" --connlimit-saddr");
	}
}

static struct xtables_match connlimit_mt_reg[] = {
	{
		.name          = "connlimit",
		.revision      = 0,
		.family        = NFPROTO_IPV4,
		.version       = XTABLES_VERSION,
		.size          = XT_ALIGN(sizeof(struct xt_connlimit_info)),
		.userspacesize = offsetof(struct xt_connlimit_info, data),
		.help          = connlimit_help,
		.init          = connlimit_init,
		.x6_parse      = connlimit_parse4,
		.x6_fcheck     = connlimit_check,
		.print         = connlimit_print4,
		.save          = connlimit_save4,
		.x6_options    = connlimit_opts,
	},
	{
		.name          = "connlimit",
		.revision      = 0,
		.family        = NFPROTO_IPV6,
		.version       = XTABLES_VERSION,
		.size          = XT_ALIGN(sizeof(struct xt_connlimit_info)),
		.userspacesize = offsetof(struct xt_connlimit_info, data),
		.help          = connlimit_help,
		.init          = connlimit_init,
		.x6_parse      = connlimit_parse6,
		.x6_fcheck     = connlimit_check,
		.print         = connlimit_print6,
		.save          = connlimit_save6,
		.x6_options    = connlimit_opts,
	},
	{
		.name          = "connlimit",
		.revision      = 1,
		.family        = NFPROTO_IPV4,
		.version       = XTABLES_VERSION,
		.size          = XT_ALIGN(sizeof(struct xt_connlimit_info)),
		.userspacesize = offsetof(struct xt_connlimit_info, data),
		.help          = connlimit_help,
		.init          = connlimit_init,
		.x6_parse      = connlimit_parse4,
		.x6_fcheck     = connlimit_check,
		.print         = connlimit_print4,
		.save          = connlimit_save4,
		.x6_options    = connlimit_opts,
	},
	{
		.name          = "connlimit",
		.revision      = 1,
		.family        = NFPROTO_IPV6,
		.version       = XTABLES_VERSION,
		.size          = XT_ALIGN(sizeof(struct xt_connlimit_info)),
		.userspacesize = offsetof(struct xt_connlimit_info, data),
		.help          = connlimit_help,
		.init          = connlimit_init,
		.x6_parse      = connlimit_parse6,
		.x6_fcheck     = connlimit_check,
		.print         = connlimit_print6,
		.save          = connlimit_save6,
		.x6_options    = connlimit_opts,
	},
};

void _init(void)
{
	xtables_register_matches(connlimit_mt_reg, ARRAY_SIZE(connlimit_mt_reg));
}
