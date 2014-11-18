/*
 *	libxt_conntrack
 *	Shared library add-on to iptables for conntrack matching support.
 *
 *	GPL (C) 2001  Marc Boucher (marc@mbsi.ca).
 *	Copyright © CC Computer Consultants GmbH, 2007 - 2008
 *	Jan Engelhardt <jengelh@computergmbh.de>
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xtables.h>
#include <linux/netfilter/xt_conntrack.h>
#include <linux/netfilter/nf_conntrack_common.h>

struct ip_conntrack_old_tuple {
	struct {
		__be32 ip;
		union {
			__u16 all;
		} u;
	} src;

	struct {
		__be32 ip;
		union {
			__u16 all;
		} u;

		/* The protocol. */
		__u16 protonum;
	} dst;
};

struct xt_conntrack_info {
	unsigned int statemask, statusmask;

	struct ip_conntrack_old_tuple tuple[IP_CT_DIR_MAX];
	struct in_addr sipmsk[IP_CT_DIR_MAX], dipmsk[IP_CT_DIR_MAX];

	unsigned long expires_min, expires_max;

	/* Flags word */
	uint8_t flags;
	/* Inverse flags */
	uint8_t invflags;
};

enum {
	O_CTSTATE = 0,
	O_CTPROTO,
	O_CTORIGSRC,
	O_CTORIGDST,
	O_CTREPLSRC,
	O_CTREPLDST,
	O_CTORIGSRCPORT,
	O_CTORIGDSTPORT,
	O_CTREPLSRCPORT,
	O_CTREPLDSTPORT,
	O_CTSTATUS,
	O_CTEXPIRE,
	O_CTDIR,
};

static void conntrack_mt_help(void)
{
	printf(
"conntrack match options:\n"
"[!] --ctstate {INVALID|ESTABLISHED|NEW|RELATED|UNTRACKED|SNAT|DNAT}[,...]\n"
"                               State(s) to match\n"
"[!] --ctproto proto            Protocol to match; by number or name, e.g. \"tcp\"\n"
"[!] --ctorigsrc address[/mask]\n"
"[!] --ctorigdst address[/mask]\n"
"[!] --ctreplsrc address[/mask]\n"
"[!] --ctrepldst address[/mask]\n"
"                               Original/Reply source/destination address\n"
"[!] --ctorigsrcport port\n"
"[!] --ctorigdstport port\n"
"[!] --ctreplsrcport port\n"
"[!] --ctrepldstport port\n"
"                               TCP/UDP/SCTP orig./reply source/destination port\n"
"[!] --ctstatus {NONE|EXPECTED|SEEN_REPLY|ASSURED|CONFIRMED}[,...]\n"
"                               Status(es) to match\n"
"[!] --ctexpire time[:time]     Match remaining lifetime in seconds against\n"
"                               value or range of values (inclusive)\n"
"    --ctdir {ORIGINAL|REPLY}   Flow direction of packet\n");
}

#define s struct xt_conntrack_info /* for v0 */
static const struct xt_option_entry conntrack_mt_opts_v0[] = {
	{.name = "ctstate", .id = O_CTSTATE, .type = XTTYPE_STRING,
	 .flags = XTOPT_INVERT},
	{.name = "ctproto", .id = O_CTPROTO, .type = XTTYPE_PROTOCOL,
	 .flags = XTOPT_INVERT},
	{.name = "ctorigsrc", .id = O_CTORIGSRC, .type = XTTYPE_HOST,
	 .flags = XTOPT_INVERT},
	{.name = "ctorigdst", .id = O_CTORIGDST, .type = XTTYPE_HOST,
	 .flags = XTOPT_INVERT},
	{.name = "ctreplsrc", .id = O_CTREPLSRC, .type = XTTYPE_HOST,
	 .flags = XTOPT_INVERT},
	{.name = "ctrepldst", .id = O_CTREPLDST, .type = XTTYPE_HOST,
	 .flags = XTOPT_INVERT},
	{.name = "ctstatus", .id = O_CTSTATUS, .type = XTTYPE_STRING,
	 .flags = XTOPT_INVERT},
	{.name = "ctexpire", .id = O_CTEXPIRE, .type = XTTYPE_UINT32RC,
	 .flags = XTOPT_INVERT},
	XTOPT_TABLEEND,
};
#undef s

#define s struct xt_conntrack_mtinfo3 /* for v1-v3 */
/* We exploit the fact that v1-v3 share the same layout */
static const struct xt_option_entry conntrack_mt_opts[] = {
	{.name = "ctstate", .id = O_CTSTATE, .type = XTTYPE_STRING,
	 .flags = XTOPT_INVERT},
	{.name = "ctproto", .id = O_CTPROTO, .type = XTTYPE_PROTOCOL,
	 .flags = XTOPT_INVERT},
	{.name = "ctorigsrc", .id = O_CTORIGSRC, .type = XTTYPE_HOSTMASK,
	 .flags = XTOPT_INVERT},
	{.name = "ctorigdst", .id = O_CTORIGDST, .type = XTTYPE_HOSTMASK,
	 .flags = XTOPT_INVERT},
	{.name = "ctreplsrc", .id = O_CTREPLSRC, .type = XTTYPE_HOSTMASK,
	 .flags = XTOPT_INVERT},
	{.name = "ctrepldst", .id = O_CTREPLDST, .type = XTTYPE_HOSTMASK,
	 .flags = XTOPT_INVERT},
	{.name = "ctstatus", .id = O_CTSTATUS, .type = XTTYPE_STRING,
	 .flags = XTOPT_INVERT},
	{.name = "ctexpire", .id = O_CTEXPIRE, .type = XTTYPE_UINT32RC,
	 .flags = XTOPT_INVERT},
	{.name = "ctorigsrcport", .id = O_CTORIGSRCPORT, .type = XTTYPE_PORTRC,
	 .flags = XTOPT_INVERT},
	{.name = "ctorigdstport", .id = O_CTORIGDSTPORT, .type = XTTYPE_PORTRC,
	 .flags = XTOPT_INVERT},
	{.name = "ctreplsrcport", .id = O_CTREPLSRCPORT, .type = XTTYPE_PORTRC,
	 .flags = XTOPT_INVERT},
	{.name = "ctrepldstport", .id = O_CTREPLDSTPORT, .type = XTTYPE_PORTRC,
	 .flags = XTOPT_INVERT},
	{.name = "ctdir", .id = O_CTDIR, .type = XTTYPE_STRING},
	XTOPT_TABLEEND,
};
#undef s

static int
parse_state(const char *state, size_t len, struct xt_conntrack_info *sinfo)
{
	if (strncasecmp(state, "INVALID", len) == 0)
		sinfo->statemask |= XT_CONNTRACK_STATE_INVALID;
	else if (strncasecmp(state, "NEW", len) == 0)
		sinfo->statemask |= XT_CONNTRACK_STATE_BIT(IP_CT_NEW);
	else if (strncasecmp(state, "ESTABLISHED", len) == 0)
		sinfo->statemask |= XT_CONNTRACK_STATE_BIT(IP_CT_ESTABLISHED);
	else if (strncasecmp(state, "RELATED", len) == 0)
		sinfo->statemask |= XT_CONNTRACK_STATE_BIT(IP_CT_RELATED);
	else if (strncasecmp(state, "UNTRACKED", len) == 0)
		sinfo->statemask |= XT_CONNTRACK_STATE_UNTRACKED;
	else if (strncasecmp(state, "SNAT", len) == 0)
		sinfo->statemask |= XT_CONNTRACK_STATE_SNAT;
	else if (strncasecmp(state, "DNAT", len) == 0)
		sinfo->statemask |= XT_CONNTRACK_STATE_DNAT;
	else
		return 0;
	return 1;
}

static void
parse_states(const char *arg, struct xt_conntrack_info *sinfo)
{
	const char *comma;

	while ((comma = strchr(arg, ',')) != NULL) {
		if (comma == arg || !parse_state(arg, comma-arg, sinfo))
			xtables_error(PARAMETER_PROBLEM, "Bad ctstate \"%s\"", arg);
		arg = comma+1;
	}
	if (!*arg)
		xtables_error(PARAMETER_PROBLEM, "\"--ctstate\" requires a list of "
					      "states with no spaces, e.g. "
					      "ESTABLISHED,RELATED");
	if (strlen(arg) == 0 || !parse_state(arg, strlen(arg), sinfo))
		xtables_error(PARAMETER_PROBLEM, "Bad ctstate \"%s\"", arg);
}

static bool
conntrack_ps_state(struct xt_conntrack_mtinfo3 *info, const char *state,
                   size_t z)
{
	if (strncasecmp(state, "INVALID", z) == 0)
		info->state_mask |= XT_CONNTRACK_STATE_INVALID;
	else if (strncasecmp(state, "NEW", z) == 0)
		info->state_mask |= XT_CONNTRACK_STATE_BIT(IP_CT_NEW);
	else if (strncasecmp(state, "ESTABLISHED", z) == 0)
		info->state_mask |= XT_CONNTRACK_STATE_BIT(IP_CT_ESTABLISHED);
	else if (strncasecmp(state, "RELATED", z) == 0)
		info->state_mask |= XT_CONNTRACK_STATE_BIT(IP_CT_RELATED);
	else if (strncasecmp(state, "UNTRACKED", z) == 0)
		info->state_mask |= XT_CONNTRACK_STATE_UNTRACKED;
	else if (strncasecmp(state, "SNAT", z) == 0)
		info->state_mask |= XT_CONNTRACK_STATE_SNAT;
	else if (strncasecmp(state, "DNAT", z) == 0)
		info->state_mask |= XT_CONNTRACK_STATE_DNAT;
	else
		return false;
	return true;
}

static void
conntrack_ps_states(struct xt_conntrack_mtinfo3 *info, const char *arg)
{
	const char *comma;

	while ((comma = strchr(arg, ',')) != NULL) {
		if (comma == arg || !conntrack_ps_state(info, arg, comma - arg))
			xtables_error(PARAMETER_PROBLEM,
			           "Bad ctstate \"%s\"", arg);
		arg = comma + 1;
	}

	if (strlen(arg) == 0 || !conntrack_ps_state(info, arg, strlen(arg)))
		xtables_error(PARAMETER_PROBLEM, "Bad ctstate \"%s\"", arg);
}

static int
parse_status(const char *status, size_t len, struct xt_conntrack_info *sinfo)
{
	if (strncasecmp(status, "NONE", len) == 0)
		sinfo->statusmask |= 0;
	else if (strncasecmp(status, "EXPECTED", len) == 0)
		sinfo->statusmask |= IPS_EXPECTED;
	else if (strncasecmp(status, "SEEN_REPLY", len) == 0)
		sinfo->statusmask |= IPS_SEEN_REPLY;
	else if (strncasecmp(status, "ASSURED", len) == 0)
		sinfo->statusmask |= IPS_ASSURED;
#ifdef IPS_CONFIRMED
	else if (strncasecmp(status, "CONFIRMED", len) == 0)
		sinfo->statusmask |= IPS_CONFIRMED;
#endif
	else
		return 0;
	return 1;
}

static void
parse_statuses(const char *arg, struct xt_conntrack_info *sinfo)
{
	const char *comma;

	while ((comma = strchr(arg, ',')) != NULL) {
		if (comma == arg || !parse_status(arg, comma-arg, sinfo))
			xtables_error(PARAMETER_PROBLEM, "Bad ctstatus \"%s\"", arg);
		arg = comma+1;
	}

	if (strlen(arg) == 0 || !parse_status(arg, strlen(arg), sinfo))
		xtables_error(PARAMETER_PROBLEM, "Bad ctstatus \"%s\"", arg);
}

static bool
conntrack_ps_status(struct xt_conntrack_mtinfo3 *info, const char *status,
                    size_t z)
{
	if (strncasecmp(status, "NONE", z) == 0)
		info->status_mask |= 0;
	else if (strncasecmp(status, "EXPECTED", z) == 0)
		info->status_mask |= IPS_EXPECTED;
	else if (strncasecmp(status, "SEEN_REPLY", z) == 0)
		info->status_mask |= IPS_SEEN_REPLY;
	else if (strncasecmp(status, "ASSURED", z) == 0)
		info->status_mask |= IPS_ASSURED;
	else if (strncasecmp(status, "CONFIRMED", z) == 0)
		info->status_mask |= IPS_CONFIRMED;
	else
		return false;
	return true;
}

static void
conntrack_ps_statuses(struct xt_conntrack_mtinfo3 *info, const char *arg)
{
	const char *comma;

	while ((comma = strchr(arg, ',')) != NULL) {
		if (comma == arg || !conntrack_ps_status(info, arg, comma - arg))
			xtables_error(PARAMETER_PROBLEM,
			           "Bad ctstatus \"%s\"", arg);
		arg = comma + 1;
	}

	if (strlen(arg) == 0 || !conntrack_ps_status(info, arg, strlen(arg)))
		xtables_error(PARAMETER_PROBLEM, "Bad ctstatus \"%s\"", arg);
}

static void conntrack_parse(struct xt_option_call *cb)
{
	struct xt_conntrack_info *sinfo = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_CTSTATE:
		parse_states(cb->arg, sinfo);
		if (cb->invert)
			sinfo->invflags |= XT_CONNTRACK_STATE;
		break;
	case O_CTPROTO:
		if (cb->invert)
			sinfo->invflags |= XT_CONNTRACK_PROTO;
		sinfo->tuple[IP_CT_DIR_ORIGINAL].dst.protonum = cb->val.protocol;

		if (sinfo->tuple[IP_CT_DIR_ORIGINAL].dst.protonum == 0
		    && (sinfo->invflags & XT_INV_PROTO))
			xtables_error(PARAMETER_PROBLEM,
				   "rule would never match protocol");

		sinfo->flags |= XT_CONNTRACK_PROTO;
		break;
	case O_CTORIGSRC:
		if (cb->invert)
			sinfo->invflags |= XT_CONNTRACK_ORIGSRC;
		sinfo->tuple[IP_CT_DIR_ORIGINAL].src.ip = cb->val.haddr.ip;
		sinfo->flags |= XT_CONNTRACK_ORIGSRC;
		break;
	case O_CTORIGDST:
		if (cb->invert)
			sinfo->invflags |= XT_CONNTRACK_ORIGDST;
		sinfo->tuple[IP_CT_DIR_ORIGINAL].dst.ip = cb->val.haddr.ip;
		sinfo->flags |= XT_CONNTRACK_ORIGDST;
		break;
	case O_CTREPLSRC:
		if (cb->invert)
			sinfo->invflags |= XT_CONNTRACK_REPLSRC;
		sinfo->tuple[IP_CT_DIR_REPLY].src.ip = cb->val.haddr.ip;
		sinfo->flags |= XT_CONNTRACK_REPLSRC;
		break;
	case O_CTREPLDST:
		if (cb->invert)
			sinfo->invflags |= XT_CONNTRACK_REPLDST;
		sinfo->tuple[IP_CT_DIR_REPLY].dst.ip = cb->val.haddr.ip;
		sinfo->flags |= XT_CONNTRACK_REPLDST;
		break;
	case O_CTSTATUS:
		parse_statuses(cb->arg, sinfo);
		if (cb->invert)
			sinfo->invflags |= XT_CONNTRACK_STATUS;
		sinfo->flags |= XT_CONNTRACK_STATUS;
		break;
	case O_CTEXPIRE:
		sinfo->expires_min = cb->val.u32_range[0];
		sinfo->expires_max = cb->val.u32_range[0];
		if (cb->nvals >= 2)
			sinfo->expires_max = cb->val.u32_range[1];
		if (cb->invert)
			sinfo->invflags |= XT_CONNTRACK_EXPIRES;
		sinfo->flags |= XT_CONNTRACK_EXPIRES;
		break;
	}
}

static void conntrack_mt_parse(struct xt_option_call *cb, uint8_t rev)
{
	struct xt_conntrack_mtinfo3 *info = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_CTSTATE:
		conntrack_ps_states(info, cb->arg);
		info->match_flags |= XT_CONNTRACK_STATE;
		if (cb->invert)
			info->invert_flags |= XT_CONNTRACK_STATE;
		break;
	case O_CTPROTO:
		info->l4proto = cb->val.protocol;
		if (info->l4proto == 0 && (info->invert_flags & XT_INV_PROTO))
			xtables_error(PARAMETER_PROBLEM, "conntrack: rule would "
			           "never match protocol");

		info->match_flags |= XT_CONNTRACK_PROTO;
		if (cb->invert)
			info->invert_flags |= XT_CONNTRACK_PROTO;
		break;
	case O_CTORIGSRC:
		info->origsrc_addr = cb->val.haddr;
		info->origsrc_mask = cb->val.hmask;
		info->match_flags |= XT_CONNTRACK_ORIGSRC;
		if (cb->invert)
			info->invert_flags |= XT_CONNTRACK_ORIGSRC;
		break;
	case O_CTORIGDST:
		info->origdst_addr = cb->val.haddr;
		info->origdst_mask = cb->val.hmask;
		info->match_flags |= XT_CONNTRACK_ORIGDST;
		if (cb->invert)
			info->invert_flags |= XT_CONNTRACK_ORIGDST;
		break;
	case O_CTREPLSRC:
		info->replsrc_addr = cb->val.haddr;
		info->replsrc_mask = cb->val.hmask;
		info->match_flags |= XT_CONNTRACK_REPLSRC;
		if (cb->invert)
			info->invert_flags |= XT_CONNTRACK_REPLSRC;
		break;
	case O_CTREPLDST:
		info->repldst_addr = cb->val.haddr;
		info->repldst_mask = cb->val.hmask;
		info->match_flags |= XT_CONNTRACK_REPLDST;
		if (cb->invert)
			info->invert_flags |= XT_CONNTRACK_REPLDST;
		break;
	case O_CTSTATUS:
		conntrack_ps_statuses(info, cb->arg);
		info->match_flags |= XT_CONNTRACK_STATUS;
		if (cb->invert)
			info->invert_flags |= XT_CONNTRACK_STATUS;
		break;
	case O_CTEXPIRE:
		info->expires_min = cb->val.u32_range[0];
		info->expires_max = cb->val.u32_range[0];
		if (cb->nvals >= 2)
			info->expires_max = cb->val.u32_range[1];
		info->match_flags |= XT_CONNTRACK_EXPIRES;
		if (cb->invert)
			info->invert_flags |= XT_CONNTRACK_EXPIRES;
		break;
	case O_CTORIGSRCPORT:
		info->origsrc_port = cb->val.port_range[0];
		info->origsrc_port_high = cb->val.port_range[cb->nvals >= 2];
		info->match_flags |= XT_CONNTRACK_ORIGSRC_PORT;
		if (cb->invert)
			info->invert_flags |= XT_CONNTRACK_ORIGSRC_PORT;
		break;
	case O_CTORIGDSTPORT:
		info->origdst_port = cb->val.port_range[0];
		info->origdst_port_high = cb->val.port_range[cb->nvals >= 2];
		info->match_flags |= XT_CONNTRACK_ORIGDST_PORT;
		if (cb->invert)
			info->invert_flags |= XT_CONNTRACK_ORIGDST_PORT;
		break;
	case O_CTREPLSRCPORT:
		info->replsrc_port = cb->val.port_range[0];
		info->replsrc_port_high = cb->val.port_range[cb->nvals >= 2];
		info->match_flags |= XT_CONNTRACK_REPLSRC_PORT;
		if (cb->invert)
			info->invert_flags |= XT_CONNTRACK_REPLSRC_PORT;
		break;
	case O_CTREPLDSTPORT:
		info->repldst_port = cb->val.port_range[0];
		info->repldst_port_high = cb->val.port_range[cb->nvals >= 2];
		info->match_flags |= XT_CONNTRACK_REPLDST_PORT;
		if (cb->invert)
			info->invert_flags |= XT_CONNTRACK_REPLDST_PORT;
		break;
	case O_CTDIR:
		if (strcasecmp(cb->arg, "ORIGINAL") == 0) {
			info->match_flags  |= XT_CONNTRACK_DIRECTION;
			info->invert_flags &= ~XT_CONNTRACK_DIRECTION;
		} else if (strcasecmp(cb->arg, "REPLY") == 0) {
			info->match_flags  |= XT_CONNTRACK_DIRECTION;
			info->invert_flags |= XT_CONNTRACK_DIRECTION;
		} else {
			xtables_param_act(XTF_BAD_VALUE, "conntrack", "--ctdir", cb->arg);
		}
		break;
	}
}

#define cinfo_transform(r, l) \
	do { \
		memcpy((r), (l), offsetof(typeof(*(l)), state_mask)); \
		(r)->state_mask  = (l)->state_mask; \
		(r)->status_mask = (l)->status_mask; \
	} while (false);

static void conntrack1_mt_parse(struct xt_option_call *cb)
{
	struct xt_conntrack_mtinfo1 *info = cb->data;
	struct xt_conntrack_mtinfo3 up;

	memset(&up, 0, sizeof(up));
	cinfo_transform(&up, info);
	up.origsrc_port_high = up.origsrc_port;
	up.origdst_port_high = up.origdst_port;
	up.replsrc_port_high = up.replsrc_port;
	up.repldst_port_high = up.repldst_port;
	cb->data = &up;
	conntrack_mt_parse(cb, 3);
	if (up.origsrc_port != up.origsrc_port_high ||
	    up.origdst_port != up.origdst_port_high ||
	    up.replsrc_port != up.replsrc_port_high ||
	    up.repldst_port != up.repldst_port_high)
		xtables_error(PARAMETER_PROBLEM,
			"conntrack rev 1 does not support port ranges");
	cinfo_transform(info, &up);
	cb->data = info;
}

static void conntrack2_mt_parse(struct xt_option_call *cb)
{
#define cinfo2_transform(r, l) \
		memcpy((r), (l), offsetof(typeof(*(l)), sizeof(*info));

	struct xt_conntrack_mtinfo2 *info = cb->data;
	struct xt_conntrack_mtinfo3 up;

	memset(&up, 0, sizeof(up));
	memcpy(&up, info, sizeof(*info));
	up.origsrc_port_high = up.origsrc_port;
	up.origdst_port_high = up.origdst_port;
	up.replsrc_port_high = up.replsrc_port;
	up.repldst_port_high = up.repldst_port;
	cb->data = &up;
	conntrack_mt_parse(cb, 3);
	if (up.origsrc_port != up.origsrc_port_high ||
	    up.origdst_port != up.origdst_port_high ||
	    up.replsrc_port != up.replsrc_port_high ||
	    up.repldst_port != up.repldst_port_high)
		xtables_error(PARAMETER_PROBLEM,
			"conntrack rev 2 does not support port ranges");
	memcpy(info, &up, sizeof(*info));
	cb->data = info;
#undef cinfo2_transform
}

static void conntrack3_mt_parse(struct xt_option_call *cb)
{
	conntrack_mt_parse(cb, 3);
}

static void conntrack_mt_check(struct xt_fcheck_call *cb)
{
	if (cb->xflags == 0)
		xtables_error(PARAMETER_PROBLEM, "conntrack: At least one option "
		           "is required");
}

static void
print_state(unsigned int statemask)
{
	const char *sep = " ";

	if (statemask & XT_CONNTRACK_STATE_INVALID) {
		printf("%sINVALID", sep);
		sep = ",";
	}
	if (statemask & XT_CONNTRACK_STATE_BIT(IP_CT_NEW)) {
		printf("%sNEW", sep);
		sep = ",";
	}
	if (statemask & XT_CONNTRACK_STATE_BIT(IP_CT_RELATED)) {
		printf("%sRELATED", sep);
		sep = ",";
	}
	if (statemask & XT_CONNTRACK_STATE_BIT(IP_CT_ESTABLISHED)) {
		printf("%sESTABLISHED", sep);
		sep = ",";
	}
	if (statemask & XT_CONNTRACK_STATE_UNTRACKED) {
		printf("%sUNTRACKED", sep);
		sep = ",";
	}
	if (statemask & XT_CONNTRACK_STATE_SNAT) {
		printf("%sSNAT", sep);
		sep = ",";
	}
	if (statemask & XT_CONNTRACK_STATE_DNAT) {
		printf("%sDNAT", sep);
		sep = ",";
	}
}

static void
print_status(unsigned int statusmask)
{
	const char *sep = " ";

	if (statusmask & IPS_EXPECTED) {
		printf("%sEXPECTED", sep);
		sep = ",";
	}
	if (statusmask & IPS_SEEN_REPLY) {
		printf("%sSEEN_REPLY", sep);
		sep = ",";
	}
	if (statusmask & IPS_ASSURED) {
		printf("%sASSURED", sep);
		sep = ",";
	}
	if (statusmask & IPS_CONFIRMED) {
		printf("%sCONFIRMED", sep);
		sep = ",";
	}
	if (statusmask == 0)
		printf("%sNONE", sep);
}

static void
conntrack_dump_addr(const union nf_inet_addr *addr,
                    const union nf_inet_addr *mask,
                    unsigned int family, bool numeric)
{
	if (family == NFPROTO_IPV4) {
		if (!numeric && addr->ip == 0) {
			printf(" anywhere");
			return;
		}
		if (numeric)
			printf(" %s%s",
			       xtables_ipaddr_to_numeric(&addr->in),
			       xtables_ipmask_to_numeric(&mask->in));
		else
			printf(" %s%s",
			       xtables_ipaddr_to_anyname(&addr->in),
			       xtables_ipmask_to_numeric(&mask->in));
	} else if (family == NFPROTO_IPV6) {
		if (!numeric && addr->ip6[0] == 0 && addr->ip6[1] == 0 &&
		    addr->ip6[2] == 0 && addr->ip6[3] == 0) {
			printf(" anywhere");
			return;
		}
		if (numeric)
			printf(" %s%s",
			       xtables_ip6addr_to_numeric(&addr->in6),
			       xtables_ip6mask_to_numeric(&mask->in6));
		else
			printf(" %s%s",
			       xtables_ip6addr_to_anyname(&addr->in6),
			       xtables_ip6mask_to_numeric(&mask->in6));
	}
}

static void
print_addr(const struct in_addr *addr, const struct in_addr *mask,
           int inv, int numeric)
{
	char buf[BUFSIZ];

	if (inv)
		printf(" !");

	if (mask->s_addr == 0L && !numeric)
		printf(" %s", "anywhere");
	else {
		if (numeric)
			strcpy(buf, xtables_ipaddr_to_numeric(addr));
		else
			strcpy(buf, xtables_ipaddr_to_anyname(addr));
		strcat(buf, xtables_ipmask_to_numeric(mask));
		printf(" %s", buf);
	}
}

static void
matchinfo_print(const void *ip, const struct xt_entry_match *match, int numeric, const char *optpfx)
{
	const struct xt_conntrack_info *sinfo = (const void *)match->data;

	if(sinfo->flags & XT_CONNTRACK_STATE) {
        	if (sinfo->invflags & XT_CONNTRACK_STATE)
			printf(" !");
		printf(" %sctstate", optpfx);
		print_state(sinfo->statemask);
	}

	if(sinfo->flags & XT_CONNTRACK_PROTO) {
        	if (sinfo->invflags & XT_CONNTRACK_PROTO)
			printf(" !");
		printf(" %sctproto", optpfx);
		printf(" %u", sinfo->tuple[IP_CT_DIR_ORIGINAL].dst.protonum);
	}

	if(sinfo->flags & XT_CONNTRACK_ORIGSRC) {
		if (sinfo->invflags & XT_CONNTRACK_ORIGSRC)
			printf(" !");
		printf(" %sctorigsrc", optpfx);

		print_addr(
		    (struct in_addr *)&sinfo->tuple[IP_CT_DIR_ORIGINAL].src.ip,
		    &sinfo->sipmsk[IP_CT_DIR_ORIGINAL],
		    false,
		    numeric);
	}

	if(sinfo->flags & XT_CONNTRACK_ORIGDST) {
		if (sinfo->invflags & XT_CONNTRACK_ORIGDST)
			printf(" !");
		printf(" %sctorigdst", optpfx);

		print_addr(
		    (struct in_addr *)&sinfo->tuple[IP_CT_DIR_ORIGINAL].dst.ip,
		    &sinfo->dipmsk[IP_CT_DIR_ORIGINAL],
		    false,
		    numeric);
	}

	if(sinfo->flags & XT_CONNTRACK_REPLSRC) {
		if (sinfo->invflags & XT_CONNTRACK_REPLSRC)
			printf(" !");
		printf(" %sctreplsrc", optpfx);

		print_addr(
		    (struct in_addr *)&sinfo->tuple[IP_CT_DIR_REPLY].src.ip,
		    &sinfo->sipmsk[IP_CT_DIR_REPLY],
		    false,
		    numeric);
	}

	if(sinfo->flags & XT_CONNTRACK_REPLDST) {
		if (sinfo->invflags & XT_CONNTRACK_REPLDST)
			printf(" !");
		printf(" %sctrepldst", optpfx);

		print_addr(
		    (struct in_addr *)&sinfo->tuple[IP_CT_DIR_REPLY].dst.ip,
		    &sinfo->dipmsk[IP_CT_DIR_REPLY],
		    false,
		    numeric);
	}

	if(sinfo->flags & XT_CONNTRACK_STATUS) {
        	if (sinfo->invflags & XT_CONNTRACK_STATUS)
			printf(" !");
		printf(" %sctstatus", optpfx);
		print_status(sinfo->statusmask);
	}

	if(sinfo->flags & XT_CONNTRACK_EXPIRES) {
        	if (sinfo->invflags & XT_CONNTRACK_EXPIRES)
			printf(" !");
		printf(" %sctexpire ", optpfx);

        	if (sinfo->expires_max == sinfo->expires_min)
			printf("%lu", sinfo->expires_min);
        	else
			printf("%lu:%lu", sinfo->expires_min, sinfo->expires_max);
	}

	if (sinfo->flags & XT_CONNTRACK_DIRECTION) {
		if (sinfo->invflags & XT_CONNTRACK_DIRECTION)
			printf(" %sctdir REPLY", optpfx);
		else
			printf(" %sctdir ORIGINAL", optpfx);
	}

}

static void
conntrack_dump_ports(const char *prefix, const char *opt,
		     u_int16_t port_low, u_int16_t port_high)
{
	if (port_high == 0 || port_low == port_high)
		printf(" %s%s %u", prefix, opt, port_low);
	else
		printf(" %s%s %u:%u", prefix, opt, port_low, port_high);
}

static void
conntrack_dump(const struct xt_conntrack_mtinfo3 *info, const char *prefix,
               unsigned int family, bool numeric, bool v3)
{
	if (info->match_flags & XT_CONNTRACK_STATE) {
		if (info->invert_flags & XT_CONNTRACK_STATE)
			printf(" !");
		printf(" %sctstate", prefix);
		print_state(info->state_mask);
	}

	if (info->match_flags & XT_CONNTRACK_PROTO) {
		if (info->invert_flags & XT_CONNTRACK_PROTO)
			printf(" !");
		printf(" %sctproto %u", prefix, info->l4proto);
	}

	if (info->match_flags & XT_CONNTRACK_ORIGSRC) {
		if (info->invert_flags & XT_CONNTRACK_ORIGSRC)
			printf(" !");
		printf(" %sctorigsrc", prefix);
		conntrack_dump_addr(&info->origsrc_addr, &info->origsrc_mask,
		                    family, numeric);
	}

	if (info->match_flags & XT_CONNTRACK_ORIGDST) {
		if (info->invert_flags & XT_CONNTRACK_ORIGDST)
			printf(" !");
		printf(" %sctorigdst", prefix);
		conntrack_dump_addr(&info->origdst_addr, &info->origdst_mask,
		                    family, numeric);
	}

	if (info->match_flags & XT_CONNTRACK_REPLSRC) {
		if (info->invert_flags & XT_CONNTRACK_REPLSRC)
			printf(" !");
		printf(" %sctreplsrc", prefix);
		conntrack_dump_addr(&info->replsrc_addr, &info->replsrc_mask,
		                    family, numeric);
	}

	if (info->match_flags & XT_CONNTRACK_REPLDST) {
		if (info->invert_flags & XT_CONNTRACK_REPLDST)
			printf(" !");
		printf(" %sctrepldst", prefix);
		conntrack_dump_addr(&info->repldst_addr, &info->repldst_mask,
		                    family, numeric);
	}

	if (info->match_flags & XT_CONNTRACK_ORIGSRC_PORT) {
		if (info->invert_flags & XT_CONNTRACK_ORIGSRC_PORT)
			printf(" !");
		conntrack_dump_ports(prefix, "ctorigsrcport",
				     v3 ? info->origsrc_port : ntohs(info->origsrc_port),
				     v3 ? info->origsrc_port_high : 0);
	}

	if (info->match_flags & XT_CONNTRACK_ORIGDST_PORT) {
		if (info->invert_flags & XT_CONNTRACK_ORIGDST_PORT)
			printf(" !");
		conntrack_dump_ports(prefix, "ctorigdstport",
				     v3 ? info->origdst_port : ntohs(info->origdst_port),
				     v3 ? info->origdst_port_high : 0);
	}

	if (info->match_flags & XT_CONNTRACK_REPLSRC_PORT) {
		if (info->invert_flags & XT_CONNTRACK_REPLSRC_PORT)
			printf(" !");
		conntrack_dump_ports(prefix, "ctreplsrcport",
				     v3 ? info->replsrc_port : ntohs(info->replsrc_port),
				     v3 ? info->replsrc_port_high : 0);
	}

	if (info->match_flags & XT_CONNTRACK_REPLDST_PORT) {
		if (info->invert_flags & XT_CONNTRACK_REPLDST_PORT)
			printf(" !");
		conntrack_dump_ports(prefix, "ctrepldstport",
				     v3 ? info->repldst_port : ntohs(info->repldst_port),
				     v3 ? info->repldst_port_high : 0);
	}

	if (info->match_flags & XT_CONNTRACK_STATUS) {
		if (info->invert_flags & XT_CONNTRACK_STATUS)
			printf(" !");
		printf(" %sctstatus", prefix);
		print_status(info->status_mask);
	}

	if (info->match_flags & XT_CONNTRACK_EXPIRES) {
		if (info->invert_flags & XT_CONNTRACK_EXPIRES)
			printf(" !");
		printf(" %sctexpire ", prefix);

		if (info->expires_max == info->expires_min)
			printf("%u", (unsigned int)info->expires_min);
		else
			printf("%u:%u", (unsigned int)info->expires_min,
			       (unsigned int)info->expires_max);
	}

	if (info->match_flags & XT_CONNTRACK_DIRECTION) {
		if (info->invert_flags & XT_CONNTRACK_DIRECTION)
			printf(" %sctdir REPLY", prefix);
		else
			printf(" %sctdir ORIGINAL", prefix);
	}
}

static void conntrack_print(const void *ip, const struct xt_entry_match *match,
                            int numeric)
{
	matchinfo_print(ip, match, numeric, "");
}

static void
conntrack1_mt4_print(const void *ip, const struct xt_entry_match *match,
                     int numeric)
{
	const struct xt_conntrack_mtinfo1 *info = (void *)match->data;
	struct xt_conntrack_mtinfo3 up;

	cinfo_transform(&up, info);
	conntrack_dump(&up, "", NFPROTO_IPV4, numeric, false);
}

static void
conntrack1_mt6_print(const void *ip, const struct xt_entry_match *match,
                     int numeric)
{
	const struct xt_conntrack_mtinfo1 *info = (void *)match->data;
	struct xt_conntrack_mtinfo3 up;

	cinfo_transform(&up, info);
	conntrack_dump(&up, "", NFPROTO_IPV6, numeric, false);
}

static void
conntrack2_mt_print(const void *ip, const struct xt_entry_match *match,
                    int numeric)
{
	conntrack_dump((const void *)match->data, "", NFPROTO_IPV4, numeric, false);
}

static void
conntrack2_mt6_print(const void *ip, const struct xt_entry_match *match,
                     int numeric)
{
	conntrack_dump((const void *)match->data, "", NFPROTO_IPV6, numeric, false);
}

static void
conntrack3_mt_print(const void *ip, const struct xt_entry_match *match,
                    int numeric)
{
	conntrack_dump((const void *)match->data, "", NFPROTO_IPV4, numeric, true);
}

static void
conntrack3_mt6_print(const void *ip, const struct xt_entry_match *match,
                     int numeric)
{
	conntrack_dump((const void *)match->data, "", NFPROTO_IPV6, numeric, true);
}

static void conntrack_save(const void *ip, const struct xt_entry_match *match)
{
	matchinfo_print(ip, match, 1, "--");
}

static void conntrack3_mt_save(const void *ip,
                               const struct xt_entry_match *match)
{
	conntrack_dump((const void *)match->data, "--", NFPROTO_IPV4, true, true);
}

static void conntrack3_mt6_save(const void *ip,
                                const struct xt_entry_match *match)
{
	conntrack_dump((const void *)match->data, "--", NFPROTO_IPV6, true, true);
}

static void conntrack2_mt_save(const void *ip,
                               const struct xt_entry_match *match)
{
	conntrack_dump((const void *)match->data, "--", NFPROTO_IPV4, true, false);
}

static void conntrack2_mt6_save(const void *ip,
                                const struct xt_entry_match *match)
{
	conntrack_dump((const void *)match->data, "--", NFPROTO_IPV6, true, false);
}

static void
conntrack1_mt4_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_conntrack_mtinfo1 *info = (void *)match->data;
	struct xt_conntrack_mtinfo3 up;

	cinfo_transform(&up, info);
	conntrack_dump(&up, "--", NFPROTO_IPV4, true, false);
}

static void
conntrack1_mt6_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_conntrack_mtinfo1 *info = (void *)match->data;
	struct xt_conntrack_mtinfo3 up;

	cinfo_transform(&up, info);
	conntrack_dump(&up, "--", NFPROTO_IPV6, true, false);
}

static struct xtables_match conntrack_mt_reg[] = {
	{
		.version       = XTABLES_VERSION,
		.name          = "conntrack",
		.revision      = 0,
		.family        = NFPROTO_IPV4,
		.size          = XT_ALIGN(sizeof(struct xt_conntrack_info)),
		.userspacesize = XT_ALIGN(sizeof(struct xt_conntrack_info)),
		.help          = conntrack_mt_help,
		.x6_parse      = conntrack_parse,
		.x6_fcheck     = conntrack_mt_check,
		.print         = conntrack_print,
		.save          = conntrack_save,
		.x6_options    = conntrack_mt_opts_v0,
	},
	{
		.version       = XTABLES_VERSION,
		.name          = "conntrack",
		.revision      = 1,
		.family        = NFPROTO_IPV4,
		.size          = XT_ALIGN(sizeof(struct xt_conntrack_mtinfo1)),
		.userspacesize = XT_ALIGN(sizeof(struct xt_conntrack_mtinfo1)),
		.help          = conntrack_mt_help,
		.x6_parse      = conntrack1_mt_parse,
		.x6_fcheck     = conntrack_mt_check,
		.print         = conntrack1_mt4_print,
		.save          = conntrack1_mt4_save,
		.x6_options    = conntrack_mt_opts,
	},
	{
		.version       = XTABLES_VERSION,
		.name          = "conntrack",
		.revision      = 1,
		.family        = NFPROTO_IPV6,
		.size          = XT_ALIGN(sizeof(struct xt_conntrack_mtinfo1)),
		.userspacesize = XT_ALIGN(sizeof(struct xt_conntrack_mtinfo1)),
		.help          = conntrack_mt_help,
		.x6_parse      = conntrack1_mt_parse,
		.x6_fcheck     = conntrack_mt_check,
		.print         = conntrack1_mt6_print,
		.save          = conntrack1_mt6_save,
		.x6_options    = conntrack_mt_opts,
	},
	{
		.version       = XTABLES_VERSION,
		.name          = "conntrack",
		.revision      = 2,
		.family        = NFPROTO_IPV4,
		.size          = XT_ALIGN(sizeof(struct xt_conntrack_mtinfo2)),
		.userspacesize = XT_ALIGN(sizeof(struct xt_conntrack_mtinfo2)),
		.help          = conntrack_mt_help,
		.x6_parse      = conntrack2_mt_parse,
		.x6_fcheck     = conntrack_mt_check,
		.print         = conntrack2_mt_print,
		.save          = conntrack2_mt_save,
		.x6_options    = conntrack_mt_opts,
	},
	{
		.version       = XTABLES_VERSION,
		.name          = "conntrack",
		.revision      = 2,
		.family        = NFPROTO_IPV6,
		.size          = XT_ALIGN(sizeof(struct xt_conntrack_mtinfo2)),
		.userspacesize = XT_ALIGN(sizeof(struct xt_conntrack_mtinfo2)),
		.help          = conntrack_mt_help,
		.x6_parse      = conntrack2_mt_parse,
		.x6_fcheck     = conntrack_mt_check,
		.print         = conntrack2_mt6_print,
		.save          = conntrack2_mt6_save,
		.x6_options    = conntrack_mt_opts,
	},
	{
		.version       = XTABLES_VERSION,
		.name          = "conntrack",
		.revision      = 3,
		.family        = NFPROTO_IPV4,
		.size          = XT_ALIGN(sizeof(struct xt_conntrack_mtinfo3)),
		.userspacesize = XT_ALIGN(sizeof(struct xt_conntrack_mtinfo3)),
		.help          = conntrack_mt_help,
		.x6_parse      = conntrack3_mt_parse,
		.x6_fcheck     = conntrack_mt_check,
		.print         = conntrack3_mt_print,
		.save          = conntrack3_mt_save,
		.x6_options    = conntrack_mt_opts,
	},
	{
		.version       = XTABLES_VERSION,
		.name          = "conntrack",
		.revision      = 3,
		.family        = NFPROTO_IPV6,
		.size          = XT_ALIGN(sizeof(struct xt_conntrack_mtinfo3)),
		.userspacesize = XT_ALIGN(sizeof(struct xt_conntrack_mtinfo3)),
		.help          = conntrack_mt_help,
		.x6_parse      = conntrack3_mt_parse,
		.x6_fcheck     = conntrack_mt_check,
		.print         = conntrack3_mt6_print,
		.save          = conntrack3_mt6_save,
		.x6_options    = conntrack_mt_opts,
	},
};

void _init(void)
{
	xtables_register_matches(conntrack_mt_reg, ARRAY_SIZE(conntrack_mt_reg));
}
