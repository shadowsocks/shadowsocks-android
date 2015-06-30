/*
 * Shared library add-on to iptables to add IPVS matching.
 *
 * Detailed doc is in the kernel module source net/netfilter/xt_ipvs.c
 *
 * Author: Hannes Eder <heder@google.com>
 */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <xtables.h>
#include <linux/ip_vs.h>
#include <linux/netfilter/xt_ipvs.h>

enum {
	/* For xt_ipvs: make sure this matches up with %XT_IPVS_*'s order */
	O_IPVS = 0,
	O_VPROTO,
	O_VADDR,
	O_VPORT,
	O_VDIR,
	O_VMETHOD,
	O_VPORTCTL,
};

#define s struct xt_ipvs_mtinfo
static const struct xt_option_entry ipvs_mt_opts[] = {
	{.name = "ipvs", .id = O_IPVS, .type = XTTYPE_NONE,
	 .flags = XTOPT_INVERT},
	{.name = "vproto", .id = O_VPROTO, .type = XTTYPE_STRING,
	 .flags = XTOPT_INVERT | XTOPT_PUT, XTOPT_POINTER(s, l4proto)},
	{.name = "vaddr", .id = O_VADDR, .type = XTTYPE_HOSTMASK,
	 .flags = XTOPT_INVERT},
	{.name = "vport", .id = O_VPORT, .type = XTTYPE_PORT,
	 .flags = XTOPT_NBO | XTOPT_INVERT | XTOPT_PUT,
	 XTOPT_POINTER(s, vport)},
	{.name = "vdir", .id = O_VDIR, .type = XTTYPE_STRING},
	{.name = "vmethod", .id = O_VMETHOD, .type = XTTYPE_STRING,
	 .flags = XTOPT_INVERT},
	{.name = "vportctl", .id = O_VPORTCTL, .type = XTTYPE_PORT,
	 .flags = XTOPT_NBO | XTOPT_INVERT | XTOPT_PUT,
	 XTOPT_POINTER(s, vportctl)},
	XTOPT_TABLEEND,
};
#undef s

static void ipvs_mt_help(void)
{
	printf(
"IPVS match options:\n"
"[!] --ipvs                      packet belongs to an IPVS connection\n"
"\n"
"Any of the following options implies --ipvs (even negated)\n"
"[!] --vproto protocol           VIP protocol to match; by number or name,\n"
"                                e.g. \"tcp\"\n"
"[!] --vaddr address[/mask]      VIP address to match\n"
"[!] --vport port                VIP port to match; by number or name,\n"
"                                e.g. \"http\"\n"
"    --vdir {ORIGINAL|REPLY}     flow direction of packet\n"
"[!] --vmethod {GATE|IPIP|MASQ}  IPVS forwarding method used\n"
"[!] --vportctl port             VIP port of the controlling connection to\n"
"                                match, e.g. 21 for FTP\n"
		);
}

static void ipvs_mt_parse(struct xt_option_call *cb)
{
	struct xt_ipvs_mtinfo *data = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_VPROTO:
		data->l4proto = cb->val.protocol;
		break;
	case O_VADDR:
		memcpy(&data->vaddr, &cb->val.haddr, sizeof(cb->val.haddr));
		memcpy(&data->vmask, &cb->val.hmask, sizeof(cb->val.hmask));
		break;
	case O_VDIR:
		if (strcasecmp(cb->arg, "ORIGINAL") == 0) {
			data->bitmask |= XT_IPVS_DIR;
			data->invert   &= ~XT_IPVS_DIR;
		} else if (strcasecmp(cb->arg, "REPLY") == 0) {
			data->bitmask |= XT_IPVS_DIR;
			data->invert  |= XT_IPVS_DIR;
		} else {
			xtables_param_act(XTF_BAD_VALUE,
					  "ipvs", "--vdir", cb->arg);
		}
		break;
	case O_VMETHOD:
		if (strcasecmp(cb->arg, "GATE") == 0)
			data->fwd_method = IP_VS_CONN_F_DROUTE;
		else if (strcasecmp(cb->arg, "IPIP") == 0)
			data->fwd_method = IP_VS_CONN_F_TUNNEL;
		else if (strcasecmp(cb->arg, "MASQ") == 0)
			data->fwd_method = IP_VS_CONN_F_MASQ;
		else
			xtables_param_act(XTF_BAD_VALUE,
					  "ipvs", "--vmethod", cb->arg);
		break;
	}
	data->bitmask |= 1 << cb->entry->id;
	if (cb->invert)
		data->invert |= 1 << cb->entry->id;
}

static void ipvs_mt_check(struct xt_fcheck_call *cb)
{
	struct xt_ipvs_mtinfo *info = cb->data;

	if (cb->xflags == 0)
		xtables_error(PARAMETER_PROBLEM,
			      "IPVS: At least one option is required");
	if (info->bitmask & XT_IPVS_ONCE_MASK) {
		if (info->invert & XT_IPVS_IPVS_PROPERTY)
			xtables_error(PARAMETER_PROBLEM,
				      "! --ipvs cannot be together with"
				      " other options");
		info->bitmask |= XT_IPVS_IPVS_PROPERTY;
	}
}

/* Shamelessly copied from libxt_conntrack.c */
static void ipvs_mt_dump_addr(const union nf_inet_addr *addr,
			      const union nf_inet_addr *mask,
			      unsigned int family, bool numeric)
{
	char buf[BUFSIZ];

	if (family == NFPROTO_IPV4) {
		if (!numeric && addr->ip == 0) {
			printf(" anywhere");
			return;
		}
		if (numeric)
			strcpy(buf, xtables_ipaddr_to_numeric(&addr->in));
		else
			strcpy(buf, xtables_ipaddr_to_anyname(&addr->in));
		strcat(buf, xtables_ipmask_to_numeric(&mask->in));
		printf(" %s", buf);
	} else if (family == NFPROTO_IPV6) {
		if (!numeric && addr->ip6[0] == 0 && addr->ip6[1] == 0 &&
		    addr->ip6[2] == 0 && addr->ip6[3] == 0) {
			printf(" anywhere");
			return;
		}
		if (numeric)
			strcpy(buf, xtables_ip6addr_to_numeric(&addr->in6));
		else
			strcpy(buf, xtables_ip6addr_to_anyname(&addr->in6));
		strcat(buf, xtables_ip6mask_to_numeric(&mask->in6));
		printf(" %s", buf);
	}
}

static void ipvs_mt_dump(const void *ip, const struct xt_ipvs_mtinfo *data,
			 unsigned int family, bool numeric, const char *prefix)
{
	if (data->bitmask == XT_IPVS_IPVS_PROPERTY) {
		if (data->invert & XT_IPVS_IPVS_PROPERTY)
			printf(" !");
		printf(" %sipvs", prefix);
	}

	if (data->bitmask & XT_IPVS_PROTO) {
		if (data->invert & XT_IPVS_PROTO)
			printf(" !");
		printf(" %sproto %u", prefix, data->l4proto);
	}

	if (data->bitmask & XT_IPVS_VADDR) {
		if (data->invert & XT_IPVS_VADDR)
			printf(" !");

		printf(" %svaddr", prefix);
		ipvs_mt_dump_addr(&data->vaddr, &data->vmask, family, numeric);
	}

	if (data->bitmask & XT_IPVS_VPORT) {
		if (data->invert & XT_IPVS_VPORT)
			printf(" !");

		printf(" %svport %u", prefix, ntohs(data->vport));
	}

	if (data->bitmask & XT_IPVS_DIR) {
		if (data->invert & XT_IPVS_DIR)
			printf(" %svdir REPLY", prefix);
		else
			printf(" %svdir ORIGINAL", prefix);
	}

	if (data->bitmask & XT_IPVS_METHOD) {
		if (data->invert & XT_IPVS_METHOD)
			printf(" !");

		printf(" %svmethod", prefix);
		switch (data->fwd_method) {
		case IP_VS_CONN_F_DROUTE:
			printf(" GATE");
			break;
		case IP_VS_CONN_F_TUNNEL:
			printf(" IPIP");
			break;
		case IP_VS_CONN_F_MASQ:
			printf(" MASQ");
			break;
		default:
			/* Hu? */
			printf(" UNKNOWN");
			break;
		}
	}

	if (data->bitmask & XT_IPVS_VPORTCTL) {
		if (data->invert & XT_IPVS_VPORTCTL)
			printf(" !");

		printf(" %svportctl %u", prefix, ntohs(data->vportctl));
	}
}

static void ipvs_mt4_print(const void *ip, const struct xt_entry_match *match,
			   int numeric)
{
	const struct xt_ipvs_mtinfo *data = (const void *)match->data;
	ipvs_mt_dump(ip, data, NFPROTO_IPV4, numeric, "");
}

static void ipvs_mt6_print(const void *ip, const struct xt_entry_match *match,
			   int numeric)
{
	const struct xt_ipvs_mtinfo *data = (const void *)match->data;
	ipvs_mt_dump(ip, data, NFPROTO_IPV6, numeric, "");
}

static void ipvs_mt4_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_ipvs_mtinfo *data = (const void *)match->data;
	ipvs_mt_dump(ip, data, NFPROTO_IPV4, true, "--");
}

static void ipvs_mt6_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_ipvs_mtinfo *data = (const void *)match->data;
	ipvs_mt_dump(ip, data, NFPROTO_IPV6, true, "--");
}

static struct xtables_match ipvs_matches_reg[] = {
	{
		.version       = XTABLES_VERSION,
		.name          = "ipvs",
		.revision      = 0,
		.family        = NFPROTO_IPV4,
		.size          = XT_ALIGN(sizeof(struct xt_ipvs_mtinfo)),
		.userspacesize = XT_ALIGN(sizeof(struct xt_ipvs_mtinfo)),
		.help          = ipvs_mt_help,
		.x6_parse      = ipvs_mt_parse,
		.x6_fcheck     = ipvs_mt_check,
		.print         = ipvs_mt4_print,
		.save          = ipvs_mt4_save,
		.x6_options    = ipvs_mt_opts,
	},
	{
		.version       = XTABLES_VERSION,
		.name          = "ipvs",
		.revision      = 0,
		.family        = NFPROTO_IPV6,
		.size          = XT_ALIGN(sizeof(struct xt_ipvs_mtinfo)),
		.userspacesize = XT_ALIGN(sizeof(struct xt_ipvs_mtinfo)),
		.help          = ipvs_mt_help,
		.x6_parse      = ipvs_mt_parse,
		.x6_fcheck     = ipvs_mt_check,
		.print         = ipvs_mt6_print,
		.save          = ipvs_mt6_save,
		.x6_options    = ipvs_mt_opts,
	},
};

void _init(void)
{
	xtables_register_matches(ipvs_matches_reg,
				 ARRAY_SIZE(ipvs_matches_reg));
}
