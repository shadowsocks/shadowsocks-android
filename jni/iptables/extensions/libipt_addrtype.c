/* Shared library add-on to iptables to add addrtype matching support 
 * 
 * This program is released under the terms of GNU GPL */
#include <stdio.h>
#include <string.h>
#include <xtables.h>
#include <linux/netfilter_ipv4/ipt_addrtype.h>

enum {
	O_SRC_TYPE = 0,
	O_DST_TYPE,
	O_LIMIT_IFACE_IN,
	O_LIMIT_IFACE_OUT,
	F_SRC_TYPE        = 1 << O_SRC_TYPE,
	F_DST_TYPE        = 1 << O_DST_TYPE,
	F_LIMIT_IFACE_IN  = 1 << O_LIMIT_IFACE_IN,
	F_LIMIT_IFACE_OUT = 1 << O_LIMIT_IFACE_OUT,
};

/* from linux/rtnetlink.h, must match order of enumeration */
static const char *const rtn_names[] = {
	"UNSPEC",
	"UNICAST",
	"LOCAL",
	"BROADCAST",
	"ANYCAST",
	"MULTICAST",
	"BLACKHOLE",
	"UNREACHABLE",
	"PROHIBIT",
	"THROW",
	"NAT",
	"XRESOLVE",
	NULL
};

static void addrtype_help_types(void)
{
	int i;

	for (i = 0; rtn_names[i]; i++)
		printf("                                %s\n", rtn_names[i]);
}

static void addrtype_help_v0(void)
{
	printf(
"Address type match options:\n"
" [!] --src-type type[,...]      Match source address type\n"
" [!] --dst-type type[,...]      Match destination address type\n"
"\n"
"Valid types:           \n");
	addrtype_help_types();
}

static void addrtype_help_v1(void)
{
	printf(
"Address type match options:\n"
" [!] --src-type type[,...]      Match source address type\n"
" [!] --dst-type type[,...]      Match destination address type\n"
"     --limit-iface-in           Match only on the packet's incoming device\n"
"     --limit-iface-out          Match only on the packet's incoming device\n"
"\n"
"Valid types:           \n");
	addrtype_help_types();
}

static int
parse_type(const char *name, size_t len, uint16_t *mask)
{
	int i;

	for (i = 0; rtn_names[i]; i++)
		if (strncasecmp(name, rtn_names[i], len) == 0) {
			/* build up bitmask for kernel module */
			*mask |= (1 << i);
			return 1;
		}

	return 0;
}

static void parse_types(const char *arg, uint16_t *mask)
{
	const char *comma;

	while ((comma = strchr(arg, ',')) != NULL) {
		if (comma == arg || !parse_type(arg, comma-arg, mask))
			xtables_error(PARAMETER_PROBLEM,
			           "addrtype: bad type `%s'", arg);
		arg = comma + 1;
	}

	if (strlen(arg) == 0 || !parse_type(arg, strlen(arg), mask))
		xtables_error(PARAMETER_PROBLEM, "addrtype: bad type \"%s\"", arg);
}
	
static void addrtype_parse_v0(struct xt_option_call *cb)
{
	struct ipt_addrtype_info *info = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_SRC_TYPE:
		parse_types(cb->arg, &info->source);
		if (cb->invert)
			info->invert_source = 1;
		break;
	case O_DST_TYPE:
		parse_types(cb->arg, &info->dest);
		if (cb->invert)
			info->invert_dest = 1;
		break;
	}
}

static void addrtype_parse_v1(struct xt_option_call *cb)
{
	struct ipt_addrtype_info_v1 *info = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_SRC_TYPE:
		parse_types(cb->arg, &info->source);
		if (cb->invert)
			info->flags |= IPT_ADDRTYPE_INVERT_SOURCE;
		break;
	case O_DST_TYPE:
		parse_types(cb->arg, &info->dest);
		if (cb->invert)
			info->flags |= IPT_ADDRTYPE_INVERT_DEST;
		break;
	case O_LIMIT_IFACE_IN:
		info->flags |= IPT_ADDRTYPE_LIMIT_IFACE_IN;
		break;
	case O_LIMIT_IFACE_OUT:
		info->flags |= IPT_ADDRTYPE_LIMIT_IFACE_OUT;
		break;
	}
}

static void addrtype_check(struct xt_fcheck_call *cb)
{
	if (!(cb->xflags & (F_SRC_TYPE | F_DST_TYPE)))
		xtables_error(PARAMETER_PROBLEM,
			   "addrtype: you must specify --src-type or --dst-type");
}

static void print_types(uint16_t mask)
{
	const char *sep = "";
	int i;

	for (i = 0; rtn_names[i]; i++)
		if (mask & (1 << i)) {
			printf("%s%s", sep, rtn_names[i]);
			sep = ",";
		}
}

static void addrtype_print_v0(const void *ip, const struct xt_entry_match *match,
                              int numeric)
{
	const struct ipt_addrtype_info *info = 
		(struct ipt_addrtype_info *) match->data;

	printf(" ADDRTYPE match");
	if (info->source) {
		printf(" src-type ");
		if (info->invert_source)
			printf("!");
		print_types(info->source);
	}
	if (info->dest) {
		printf(" dst-type");
		if (info->invert_dest)
			printf("!");
		print_types(info->dest);
	}
}

static void addrtype_print_v1(const void *ip, const struct xt_entry_match *match,
                              int numeric)
{
	const struct ipt_addrtype_info_v1 *info = 
		(struct ipt_addrtype_info_v1 *) match->data;

	printf(" ADDRTYPE match");
	if (info->source) {
		printf(" src-type ");
		if (info->flags & IPT_ADDRTYPE_INVERT_SOURCE)
			printf("!");
		print_types(info->source);
	}
	if (info->dest) {
		printf(" dst-type ");
		if (info->flags & IPT_ADDRTYPE_INVERT_DEST)
			printf("!");
		print_types(info->dest);
	}
	if (info->flags & IPT_ADDRTYPE_LIMIT_IFACE_IN) {
		printf(" limit-in");
	}
	if (info->flags & IPT_ADDRTYPE_LIMIT_IFACE_OUT) {
		printf(" limit-out");
	}
}

static void addrtype_save_v0(const void *ip, const struct xt_entry_match *match)
{
	const struct ipt_addrtype_info *info =
		(struct ipt_addrtype_info *) match->data;

	if (info->source) {
		if (info->invert_source)
			printf(" !");
		printf(" --src-type ");
		print_types(info->source);
	}
	if (info->dest) {
		if (info->invert_dest)
			printf(" !");
		printf(" --dst-type ");
		print_types(info->dest);
	}
}

static void addrtype_save_v1(const void *ip, const struct xt_entry_match *match)
{
	const struct ipt_addrtype_info_v1 *info =
		(struct ipt_addrtype_info_v1 *) match->data;

	if (info->source) {
		if (info->flags & IPT_ADDRTYPE_INVERT_SOURCE)
			printf(" !");
		printf(" --src-type ");
		print_types(info->source);
	}
	if (info->dest) {
		if (info->flags & IPT_ADDRTYPE_INVERT_DEST)
			printf(" !");
		printf(" --dst-type ");
		print_types(info->dest);
	}
	if (info->flags & IPT_ADDRTYPE_LIMIT_IFACE_IN) {
		printf(" --limit-iface-in");
	}
	if (info->flags & IPT_ADDRTYPE_LIMIT_IFACE_OUT) {
		printf(" --limit-iface-out");
	}
}

static const struct xt_option_entry addrtype_opts_v0[] = {
	{.name = "src-type", .id = O_SRC_TYPE, .type = XTTYPE_STRING,
	 .flags = XTOPT_INVERT},
	{.name = "dst-type", .id = O_DST_TYPE, .type = XTTYPE_STRING,
	 .flags = XTOPT_INVERT},
	XTOPT_TABLEEND,
};

static const struct xt_option_entry addrtype_opts_v1[] = {
	{.name = "src-type", .id = O_SRC_TYPE, .type = XTTYPE_STRING,
	 .flags = XTOPT_INVERT},
	{.name = "dst-type", .id = O_DST_TYPE, .type = XTTYPE_STRING,
	 .flags = XTOPT_INVERT},
	{.name = "limit-iface-in", .id = O_LIMIT_IFACE_IN,
	 .type = XTTYPE_NONE, .excl = F_LIMIT_IFACE_OUT},
	{.name = "limit-iface-out", .id = O_LIMIT_IFACE_OUT,
	 .type = XTTYPE_NONE, .excl = F_LIMIT_IFACE_IN},
	XTOPT_TABLEEND,
};

static struct xtables_match addrtype_mt_reg[] = {
	{
		.name          = "addrtype",
		.version       = XTABLES_VERSION,
		.family        = NFPROTO_IPV4,
		.size          = XT_ALIGN(sizeof(struct ipt_addrtype_info)),
		.userspacesize = XT_ALIGN(sizeof(struct ipt_addrtype_info)),
		.help          = addrtype_help_v0,
		.print         = addrtype_print_v0,
		.save          = addrtype_save_v0,
		.x6_parse      = addrtype_parse_v0,
		.x6_fcheck     = addrtype_check,
		.x6_options    = addrtype_opts_v0,
	},
	{
		.name          = "addrtype",
		.revision      = 1,
		.version       = XTABLES_VERSION,
		.family        = NFPROTO_IPV4,
		.size          = XT_ALIGN(sizeof(struct ipt_addrtype_info_v1)),
		.userspacesize = XT_ALIGN(sizeof(struct ipt_addrtype_info_v1)),
		.help          = addrtype_help_v1,
		.print         = addrtype_print_v1,
		.save          = addrtype_save_v1,
		.x6_parse      = addrtype_parse_v1,
		.x6_fcheck     = addrtype_check,
		.x6_options    = addrtype_opts_v1,
	},
};


void _init(void) 
{
	xtables_register_matches(addrtype_mt_reg, ARRAY_SIZE(addrtype_mt_reg));
}
