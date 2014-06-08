#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <xtables.h>
#include <limits.h> /* INT_MAX in ip6_tables.h */
#include <linux/netfilter_ipv4/ip_tables.h>

/* special hack for icmp-type 'any': 
 * Up to kernel <=2.4.20 the problem was:
 * '-p icmp ' matches all icmp packets
 * '-p icmp -m icmp' matches _only_ ICMP type 0 :(
 * This is now fixed by initializing the field * to icmp type 0xFF
 * See: https://bugzilla.netfilter.org/cgi-bin/bugzilla/show_bug.cgi?id=37
 */

enum {
	O_ICMP_TYPE = 0,
};

struct icmp_names {
	const char *name;
	uint8_t type;
	uint8_t code_min, code_max;
};

static const struct icmp_names icmp_codes[] = {
	{ "any", 0xFF, 0, 0xFF },
	{ "echo-reply", 0, 0, 0xFF },
	/* Alias */ { "pong", 0, 0, 0xFF },

	{ "destination-unreachable", 3, 0, 0xFF },
	{   "network-unreachable", 3, 0, 0 },
	{   "host-unreachable", 3, 1, 1 },
	{   "protocol-unreachable", 3, 2, 2 },
	{   "port-unreachable", 3, 3, 3 },
	{   "fragmentation-needed", 3, 4, 4 },
	{   "source-route-failed", 3, 5, 5 },
	{   "network-unknown", 3, 6, 6 },
	{   "host-unknown", 3, 7, 7 },
	{   "network-prohibited", 3, 9, 9 },
	{   "host-prohibited", 3, 10, 10 },
	{   "TOS-network-unreachable", 3, 11, 11 },
	{   "TOS-host-unreachable", 3, 12, 12 },
	{   "communication-prohibited", 3, 13, 13 },
	{   "host-precedence-violation", 3, 14, 14 },
	{   "precedence-cutoff", 3, 15, 15 },

	{ "source-quench", 4, 0, 0xFF },

	{ "redirect", 5, 0, 0xFF },
	{   "network-redirect", 5, 0, 0 },
	{   "host-redirect", 5, 1, 1 },
	{   "TOS-network-redirect", 5, 2, 2 },
	{   "TOS-host-redirect", 5, 3, 3 },

	{ "echo-request", 8, 0, 0xFF },
	/* Alias */ { "ping", 8, 0, 0xFF },

	{ "router-advertisement", 9, 0, 0xFF },

	{ "router-solicitation", 10, 0, 0xFF },

	{ "time-exceeded", 11, 0, 0xFF },
	/* Alias */ { "ttl-exceeded", 11, 0, 0xFF },
	{   "ttl-zero-during-transit", 11, 0, 0 },
	{   "ttl-zero-during-reassembly", 11, 1, 1 },

	{ "parameter-problem", 12, 0, 0xFF },
	{   "ip-header-bad", 12, 0, 0 },
	{   "required-option-missing", 12, 1, 1 },

	{ "timestamp-request", 13, 0, 0xFF },

	{ "timestamp-reply", 14, 0, 0xFF },

	{ "address-mask-request", 17, 0, 0xFF },

	{ "address-mask-reply", 18, 0, 0xFF }
};

static void
print_icmptypes(void)
{
	unsigned int i;
	printf("Valid ICMP Types:");

	for (i = 0; i < ARRAY_SIZE(icmp_codes); ++i) {
		if (i && icmp_codes[i].type == icmp_codes[i-1].type) {
			if (icmp_codes[i].code_min == icmp_codes[i-1].code_min
			    && (icmp_codes[i].code_max
				== icmp_codes[i-1].code_max))
				printf(" (%s)", icmp_codes[i].name);
			else
				printf("\n   %s", icmp_codes[i].name);
		}
		else
			printf("\n%s", icmp_codes[i].name);
	}
	printf("\n");
}

static void icmp_help(void)
{
	printf(
"icmp match options:\n"
"[!] --icmp-type typename	match icmp type\n"
"[!] --icmp-type type[/code]	(or numeric type or type/code)\n");
	print_icmptypes();
}

static const struct xt_option_entry icmp_opts[] = {
	{.name = "icmp-type", .id = O_ICMP_TYPE, .type = XTTYPE_STRING,
	 .flags = XTOPT_MAND | XTOPT_INVERT},
	XTOPT_TABLEEND,
};

static void 
parse_icmp(const char *icmptype, uint8_t *type, uint8_t code[])
{
	static const unsigned int limit = ARRAY_SIZE(icmp_codes);
	unsigned int match = limit;
	unsigned int i;

	for (i = 0; i < limit; i++) {
		if (strncasecmp(icmp_codes[i].name, icmptype, strlen(icmptype))
		    == 0) {
			if (match != limit)
				xtables_error(PARAMETER_PROBLEM,
					   "Ambiguous ICMP type `%s':"
					   " `%s' or `%s'?",
					   icmptype,
					   icmp_codes[match].name,
					   icmp_codes[i].name);
			match = i;
		}
	}

	if (match != limit) {
		*type = icmp_codes[match].type;
		code[0] = icmp_codes[match].code_min;
		code[1] = icmp_codes[match].code_max;
	} else {
		char *slash;
		char buffer[strlen(icmptype) + 1];
		unsigned int number;

		strcpy(buffer, icmptype);
		slash = strchr(buffer, '/');

		if (slash)
			*slash = '\0';

		if (!xtables_strtoui(buffer, NULL, &number, 0, UINT8_MAX))
			xtables_error(PARAMETER_PROBLEM,
				   "Invalid ICMP type `%s'\n", buffer);
		*type = number;
		if (slash) {
			if (!xtables_strtoui(slash+1, NULL, &number, 0, UINT8_MAX))
				xtables_error(PARAMETER_PROBLEM,
					   "Invalid ICMP code `%s'\n",
					   slash+1);
			code[0] = code[1] = number;
		} else {
			code[0] = 0;
			code[1] = 0xFF;
		}
	}
}

static void icmp_init(struct xt_entry_match *m)
{
	struct ipt_icmp *icmpinfo = (struct ipt_icmp *)m->data;

	icmpinfo->type = 0xFF;
	icmpinfo->code[1] = 0xFF;
}

static void icmp_parse(struct xt_option_call *cb)
{
	struct ipt_icmp *icmpinfo = cb->data;

	xtables_option_parse(cb);
	parse_icmp(cb->arg, &icmpinfo->type, icmpinfo->code);
	if (cb->invert)
		icmpinfo->invflags |= IPT_ICMP_INV;
}

static void print_icmptype(uint8_t type,
			   uint8_t code_min, uint8_t code_max,
			   int invert,
			   int numeric)
{
	if (!numeric) {
		unsigned int i;

		for (i = 0; i < ARRAY_SIZE(icmp_codes); ++i)
			if (icmp_codes[i].type == type
			    && icmp_codes[i].code_min == code_min
			    && icmp_codes[i].code_max == code_max)
				break;

		if (i != ARRAY_SIZE(icmp_codes)) {
			printf(" %s%s",
			       invert ? "!" : "",
			       icmp_codes[i].name);
			return;
		}
	}

	if (invert)
		printf(" !");

	printf("type %u", type);
	if (code_min == code_max)
		printf(" code %u", code_min);
	else if (code_min != 0 || code_max != 0xFF)
		printf(" codes %u-%u", code_min, code_max);
}

static void icmp_print(const void *ip, const struct xt_entry_match *match,
                       int numeric)
{
	const struct ipt_icmp *icmp = (struct ipt_icmp *)match->data;

	printf(" icmp");
	print_icmptype(icmp->type, icmp->code[0], icmp->code[1],
		       icmp->invflags & IPT_ICMP_INV,
		       numeric);

	if (icmp->invflags & ~IPT_ICMP_INV)
		printf(" Unknown invflags: 0x%X",
		       icmp->invflags & ~IPT_ICMP_INV);
}

static void icmp_save(const void *ip, const struct xt_entry_match *match)
{
	const struct ipt_icmp *icmp = (struct ipt_icmp *)match->data;

	if (icmp->invflags & IPT_ICMP_INV)
		printf(" !");

	/* special hack for 'any' case */
	if (icmp->type == 0xFF) {
		printf(" --icmp-type any");
	} else {
		printf(" --icmp-type %u", icmp->type);
		if (icmp->code[0] != 0 || icmp->code[1] != 0xFF)
			printf("/%u", icmp->code[0]);
	}
}

static struct xtables_match icmp_mt_reg = {
	.name		= "icmp",
	.version	= XTABLES_VERSION,
	.family		= NFPROTO_IPV4,
	.size		= XT_ALIGN(sizeof(struct ipt_icmp)),
	.userspacesize	= XT_ALIGN(sizeof(struct ipt_icmp)),
	.help		= icmp_help,
	.init		= icmp_init,
	.print		= icmp_print,
	.save		= icmp_save,
	.x6_parse	= icmp_parse,
	.x6_options	= icmp_opts,
};

void _init(void)
{
	xtables_register_match(&icmp_mt_reg);
}
