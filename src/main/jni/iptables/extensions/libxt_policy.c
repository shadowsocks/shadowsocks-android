#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <xtables.h>
#include <linux/netfilter/xt_policy.h>

enum {
	O_DIRECTION = 0,
	O_POLICY,
	O_STRICT,
	O_REQID,
	O_SPI,
	O_PROTO,
	O_MODE,
	O_TUNNELSRC,
	O_TUNNELDST,
	O_NEXT,
	F_STRICT = 1 << O_STRICT,
};

static void policy_help(void)
{
	printf(
"policy match options:\n"
"  --dir in|out			match policy applied during decapsulation/\n"
"				policy to be applied during encapsulation\n"
"  --pol none|ipsec		match policy\n"
"  --strict 			match entire policy instead of single element\n"
"				at any position\n"
"These options may be used repeatedly, to describe policy elements:\n"
"[!] --reqid reqid		match reqid\n"
"[!] --spi spi			match SPI\n"
"[!] --proto proto		match protocol (ah/esp/ipcomp)\n"
"[!] --mode mode 		match mode (transport/tunnel)\n"
"[!] --tunnel-src addr/mask	match tunnel source\n"
"[!] --tunnel-dst addr/mask	match tunnel destination\n"
"  --next 			begin next element in policy\n");
}

static const struct xt_option_entry policy_opts[] = {
	{.name = "dir", .id = O_DIRECTION, .type = XTTYPE_STRING,
	 .flags = XTOPT_INVERT},
	{.name = "pol", .id = O_POLICY, .type = XTTYPE_STRING},
	{.name = "strict", .id = O_STRICT, .type = XTTYPE_NONE},
	{.name = "reqid", .id = O_REQID, .type = XTTYPE_UINT32,
	 .flags = XTOPT_MULTI | XTOPT_INVERT},
	{.name = "spi", .id = O_SPI, .type = XTTYPE_UINT32,
	 .flags = XTOPT_MULTI | XTOPT_INVERT},
	{.name = "tunnel-src", .id = O_TUNNELSRC, .type = XTTYPE_HOSTMASK,
	 .flags = XTOPT_MULTI | XTOPT_INVERT},
	{.name = "tunnel-dst", .id = O_TUNNELDST, .type = XTTYPE_HOSTMASK,
	 .flags = XTOPT_MULTI | XTOPT_INVERT},
	{.name = "proto", .id = O_PROTO, .type = XTTYPE_PROTOCOL,
	 .flags = XTOPT_MULTI | XTOPT_INVERT},
	{.name = "mode", .id = O_MODE, .type = XTTYPE_STRING,
	 .flags = XTOPT_MULTI | XTOPT_INVERT},
	{.name = "next", .id = O_NEXT, .type = XTTYPE_NONE,
	 .flags = XTOPT_MULTI, .also = F_STRICT},
	XTOPT_TABLEEND,
};

static int parse_direction(const char *s)
{
	if (strcmp(s, "in") == 0)
		return XT_POLICY_MATCH_IN;
	if (strcmp(s, "out") == 0)
		return XT_POLICY_MATCH_OUT;
	xtables_error(PARAMETER_PROBLEM, "policy_match: invalid dir \"%s\"", s);
}

static int parse_policy(const char *s)
{
	if (strcmp(s, "none") == 0)
		return XT_POLICY_MATCH_NONE;
	if (strcmp(s, "ipsec") == 0)
		return 0;
	xtables_error(PARAMETER_PROBLEM, "policy match: invalid policy \"%s\"", s);
}

static int parse_mode(const char *s)
{
	if (strcmp(s, "transport") == 0)
		return XT_POLICY_MODE_TRANSPORT;
	if (strcmp(s, "tunnel") == 0)
		return XT_POLICY_MODE_TUNNEL;
	xtables_error(PARAMETER_PROBLEM, "policy match: invalid mode \"%s\"", s);
}

static void policy_parse(struct xt_option_call *cb)
{
	struct xt_policy_info *info = cb->data;
	struct xt_policy_elem *e = &info->pol[info->len];

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_DIRECTION:
		info->flags |= parse_direction(cb->arg);
		break;
	case O_POLICY:
		info->flags |= parse_policy(cb->arg);
		break;
	case O_STRICT:
		info->flags |= XT_POLICY_MATCH_STRICT;
		break;
	case O_REQID:
		if (e->match.reqid)
			xtables_error(PARAMETER_PROBLEM,
			           "policy match: double --reqid option");
		e->match.reqid = 1;
		e->invert.reqid = cb->invert;
		e->reqid = cb->val.u32;
		break;
	case O_SPI:
		if (e->match.spi)
			xtables_error(PARAMETER_PROBLEM,
			           "policy match: double --spi option");
		e->match.spi = 1;
		e->invert.spi = cb->invert;
		e->spi = cb->val.u32;
		break;
	case O_TUNNELSRC:
		if (e->match.saddr)
			xtables_error(PARAMETER_PROBLEM,
			           "policy match: double --tunnel-src option");

		e->match.saddr = 1;
		e->invert.saddr = cb->invert;
		memcpy(&e->saddr, &cb->val.haddr, sizeof(cb->val.haddr));
		memcpy(&e->smask, &cb->val.hmask, sizeof(cb->val.hmask));
                break;
	case O_TUNNELDST:
		if (e->match.daddr)
			xtables_error(PARAMETER_PROBLEM,
			           "policy match: double --tunnel-dst option");
		e->match.daddr = 1;
		e->invert.daddr = cb->invert;
		memcpy(&e->daddr, &cb->val.haddr, sizeof(cb->val.haddr));
		memcpy(&e->dmask, &cb->val.hmask, sizeof(cb->val.hmask));
		break;
	case O_PROTO:
		if (e->match.proto)
			xtables_error(PARAMETER_PROBLEM,
			           "policy match: double --proto option");
		e->proto = cb->val.protocol;
		if (e->proto != IPPROTO_AH && e->proto != IPPROTO_ESP &&
		    e->proto != IPPROTO_COMP)
			xtables_error(PARAMETER_PROBLEM,
			           "policy match: protocol must be ah/esp/ipcomp");
		e->match.proto = 1;
		e->invert.proto = cb->invert;
		break;
	case O_MODE:
		if (e->match.mode)
			xtables_error(PARAMETER_PROBLEM,
			           "policy match: double --mode option");
		e->match.mode = 1;
		e->invert.mode = cb->invert;
		e->mode = parse_mode(cb->arg);
		break;
	case O_NEXT:
		if (++info->len == XT_POLICY_MAX_ELEM)
			xtables_error(PARAMETER_PROBLEM,
			           "policy match: maximum policy depth reached");
		break;
	}
}

static void policy_check(struct xt_fcheck_call *cb)
{
	struct xt_policy_info *info = cb->data;
	const struct xt_policy_elem *e;
	int i;

	/*
	 * The old "no parameters given" check is carried out
	 * by testing for --dir.
	 */
	if (!(info->flags & (XT_POLICY_MATCH_IN | XT_POLICY_MATCH_OUT)))
		xtables_error(PARAMETER_PROBLEM,
		           "policy match: neither --dir in nor --dir out specified");

	if (info->flags & XT_POLICY_MATCH_NONE) {
		if (info->flags & XT_POLICY_MATCH_STRICT)
			xtables_error(PARAMETER_PROBLEM,
			           "policy match: policy none but --strict given");

		if (info->len != 0)
			xtables_error(PARAMETER_PROBLEM,
			           "policy match: policy none but policy given");
	} else
		info->len++;	/* increase len by 1, no --next after last element */

	/*
	 * This is already represented with O_NEXT requiring F_STRICT in the
	 * options table, but will keep this code as a comment for reference.
	 *
	if (!(info->flags & XT_POLICY_MATCH_STRICT) && info->len > 1)
		xtables_error(PARAMETER_PROBLEM,
		           "policy match: multiple elements but no --strict");
	 */

	for (i = 0; i < info->len; i++) {
		e = &info->pol[i];

		if (info->flags & XT_POLICY_MATCH_STRICT &&
		    !(e->match.reqid || e->match.spi || e->match.saddr ||
		      e->match.daddr || e->match.proto || e->match.mode))
			xtables_error(PARAMETER_PROBLEM,
				"policy match: empty policy element %u. "
				"--strict is in effect, but at least one of "
				"reqid, spi, tunnel-src, tunnel-dst, proto or "
				"mode is required.", i);

		if ((e->match.saddr || e->match.daddr)
		    && ((e->mode == XT_POLICY_MODE_TUNNEL && e->invert.mode) ||
		        (e->mode == XT_POLICY_MODE_TRANSPORT && !e->invert.mode)))
			xtables_error(PARAMETER_PROBLEM,
			           "policy match: --tunnel-src/--tunnel-dst "
			           "is only valid in tunnel mode");
	}
}

static void print_mode(const char *prefix, uint8_t mode, int numeric)
{
	printf(" %smode ", prefix);

	switch (mode) {
	case XT_POLICY_MODE_TRANSPORT:
		printf("transport");
		break;
	case XT_POLICY_MODE_TUNNEL:
		printf("tunnel");
		break;
	default:
		printf("???");
		break;
	}
}

static void print_proto(const char *prefix, uint8_t proto, int numeric)
{
	const struct protoent *p = NULL;

	printf(" %sproto ", prefix);
	if (!numeric)
		p = getprotobynumber(proto);
	if (p != NULL)
		printf("%s", p->p_name);
	else
		printf("%u", proto);
}

#define PRINT_INVERT(x)		\
do {				\
	if (x)			\
		printf(" !");	\
} while(0)

static void print_entry(const char *prefix, const struct xt_policy_elem *e,
                        bool numeric, uint8_t family)
{
	if (e->match.reqid) {
		PRINT_INVERT(e->invert.reqid);
		printf(" %sreqid %u", prefix, e->reqid);
	}
	if (e->match.spi) {
		PRINT_INVERT(e->invert.spi);
		printf(" %sspi 0x%x", prefix, e->spi);
	}
	if (e->match.proto) {
		PRINT_INVERT(e->invert.proto);
		print_proto(prefix, e->proto, numeric);
	}
	if (e->match.mode) {
		PRINT_INVERT(e->invert.mode);
		print_mode(prefix, e->mode, numeric);
	}
	if (e->match.daddr) {
		PRINT_INVERT(e->invert.daddr);
		if (family == NFPROTO_IPV6)
			printf(" %stunnel-dst %s%s", prefix,
			       xtables_ip6addr_to_numeric(&e->daddr.a6),
			       xtables_ip6mask_to_numeric(&e->dmask.a6));
		else
			printf(" %stunnel-dst %s%s", prefix,
			       xtables_ipaddr_to_numeric(&e->daddr.a4),
			       xtables_ipmask_to_numeric(&e->dmask.a4));
	}
	if (e->match.saddr) {
		PRINT_INVERT(e->invert.saddr);
		if (family == NFPROTO_IPV6)
			printf(" %stunnel-src %s%s", prefix,
			       xtables_ip6addr_to_numeric(&e->saddr.a6),
			       xtables_ip6mask_to_numeric(&e->smask.a6));
		else
			printf(" %stunnel-src %s%s", prefix,
			       xtables_ipaddr_to_numeric(&e->saddr.a4),
			       xtables_ipmask_to_numeric(&e->smask.a4));
	}
}

static void print_flags(const char *prefix, const struct xt_policy_info *info)
{
	if (info->flags & XT_POLICY_MATCH_IN)
		printf(" %sdir in", prefix);
	else
		printf(" %sdir out", prefix);

	if (info->flags & XT_POLICY_MATCH_NONE)
		printf(" %spol none", prefix);
	else
		printf(" %spol ipsec", prefix);

	if (info->flags & XT_POLICY_MATCH_STRICT)
		printf(" %sstrict", prefix);
}

static void policy4_print(const void *ip, const struct xt_entry_match *match,
                          int numeric)
{
	const struct xt_policy_info *info = (void *)match->data;
	unsigned int i;

	printf(" policy match");
	print_flags("", info);
	for (i = 0; i < info->len; i++) {
		if (info->len > 1)
			printf(" [%u]", i);
		print_entry("", &info->pol[i], numeric, NFPROTO_IPV4);
	}
}

static void policy6_print(const void *ip, const struct xt_entry_match *match,
                          int numeric)
{
	const struct xt_policy_info *info = (void *)match->data;
	unsigned int i;

	printf(" policy match");
	print_flags("", info);
	for (i = 0; i < info->len; i++) {
		if (info->len > 1)
			printf(" [%u]", i);
		print_entry("", &info->pol[i], numeric, NFPROTO_IPV6);
	}
}

static void policy4_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_policy_info *info = (void *)match->data;
	unsigned int i;

	print_flags("--", info);
	for (i = 0; i < info->len; i++) {
		print_entry("--", &info->pol[i], false, NFPROTO_IPV4);
		if (i + 1 < info->len)
			printf(" --next");
	}
}

static void policy6_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_policy_info *info = (void *)match->data;
	unsigned int i;

	print_flags("--", info);
	for (i = 0; i < info->len; i++) {
		print_entry("--", &info->pol[i], false, NFPROTO_IPV6);
		if (i + 1 < info->len)
			printf(" --next");
	}
}

static struct xtables_match policy_mt_reg[] = {
	{
		.name          = "policy",
		.version       = XTABLES_VERSION,
		.family        = NFPROTO_IPV4,
		.size          = XT_ALIGN(sizeof(struct xt_policy_info)),
		.userspacesize = XT_ALIGN(sizeof(struct xt_policy_info)),
		.help          = policy_help,
		.x6_parse      = policy_parse,
		.x6_fcheck     = policy_check,
		.print         = policy4_print,
		.save          = policy4_save,
		.x6_options    = policy_opts,
	},
	{
		.name          = "policy",
		.version       = XTABLES_VERSION,
		.family        = NFPROTO_IPV6,
		.size          = XT_ALIGN(sizeof(struct xt_policy_info)),
		.userspacesize = XT_ALIGN(sizeof(struct xt_policy_info)),
		.help          = policy_help,
		.x6_parse      = policy_parse,
		.x6_fcheck     = policy_check,
		.print         = policy6_print,
		.save          = policy6_save,
		.x6_options    = policy_opts,
	},
};

void _init(void)
{
	xtables_register_matches(policy_mt_reg, ARRAY_SIZE(policy_mt_reg));
}
