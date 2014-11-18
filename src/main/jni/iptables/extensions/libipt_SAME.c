#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <xtables.h>
#include <net/netfilter/nf_nat.h>
#include <linux/netfilter_ipv4/ipt_SAME.h>

enum {
	O_TO_ADDR = 0,
	O_NODST,
	O_RANDOM,
	F_RANDOM = 1 << O_RANDOM,
};

static void SAME_help(void)
{
	printf(
"SAME target options:\n"
" --to <ipaddr>-<ipaddr>\n"
"				Addresses to map source to.\n"
"				 May be specified more than\n"
"				  once for multiple ranges.\n"
" --nodst\n"
"				Don't use destination-ip in\n"
"				           source selection\n"
" --random\n"
"				Randomize source port\n");
}

static const struct xt_option_entry SAME_opts[] = {
	{.name = "to", .id = O_TO_ADDR, .type = XTTYPE_STRING,
	 .flags = XTOPT_MAND},
	{.name = "nodst", .id = O_NODST, .type = XTTYPE_NONE},
	{.name = "random", .id = O_RANDOM, .type = XTTYPE_NONE},
	XTOPT_TABLEEND,
};

/* Parses range of IPs */
static void parse_to(const char *orig_arg, struct nf_nat_range *range)
{
	char *dash, *arg;
	const struct in_addr *ip;

	arg = strdup(orig_arg);
	if (arg == NULL)
		xtables_error(RESOURCE_PROBLEM, "strdup");
	range->flags |= IP_NAT_RANGE_MAP_IPS;
	dash = strchr(arg, '-');

	if (dash)
		*dash = '\0';

	ip = xtables_numeric_to_ipaddr(arg);
	if (!ip)
		xtables_error(PARAMETER_PROBLEM, "Bad IP address \"%s\"\n",
			   arg);
	range->min_ip = ip->s_addr;

	if (dash) {
		ip = xtables_numeric_to_ipaddr(dash+1);
		if (!ip)
			xtables_error(PARAMETER_PROBLEM, "Bad IP address \"%s\"\n",
				   dash+1);
	}
	range->max_ip = ip->s_addr;
	if (dash)
		if (range->min_ip > range->max_ip)
			xtables_error(PARAMETER_PROBLEM, "Bad IP range \"%s-%s\"\n",
				   arg, dash+1);
	free(arg);
}

static void SAME_parse(struct xt_option_call *cb)
{
	struct ipt_same_info *mr = cb->data;
	unsigned int count;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_TO_ADDR:
		if (mr->rangesize == IPT_SAME_MAX_RANGE)
			xtables_error(PARAMETER_PROBLEM,
				   "Too many ranges specified, maximum "
				   "is %i ranges.\n",
				   IPT_SAME_MAX_RANGE);
		parse_to(cb->arg, &mr->range[mr->rangesize]);
		/* WTF do we need this for? */
		if (cb->xflags & F_RANDOM)
			mr->range[mr->rangesize].flags 
				|= IP_NAT_RANGE_PROTO_RANDOM;
		mr->rangesize++;
		break;
	case O_NODST:
		mr->info |= IPT_SAME_NODST;
		break;
	case O_RANDOM:
		for (count=0; count < mr->rangesize; count++)
			mr->range[count].flags |= IP_NAT_RANGE_PROTO_RANDOM;
		break;
	}
}

static void SAME_print(const void *ip, const struct xt_entry_target *target,
                       int numeric)
{
	unsigned int count;
	const struct ipt_same_info *mr = (const void *)target->data;
	int random_selection = 0;
	
	printf(" same:");

	for (count = 0; count < mr->rangesize; count++) {
		const struct nf_nat_range *r = &mr->range[count];
		struct in_addr a;

		a.s_addr = r->min_ip;

		printf("%s", xtables_ipaddr_to_numeric(&a));
		a.s_addr = r->max_ip;
		
		if (r->min_ip != r->max_ip)
			printf("-%s", xtables_ipaddr_to_numeric(&a));
		if (r->flags & IP_NAT_RANGE_PROTO_RANDOM) 
			random_selection = 1;
	}
	
	if (mr->info & IPT_SAME_NODST)
		printf(" nodst");

	if (random_selection)
		printf(" random");
}

static void SAME_save(const void *ip, const struct xt_entry_target *target)
{
	unsigned int count;
	const struct ipt_same_info *mr = (const void *)target->data;
	int random_selection = 0;

	for (count = 0; count < mr->rangesize; count++) {
		const struct nf_nat_range *r = &mr->range[count];
		struct in_addr a;

		a.s_addr = r->min_ip;
		printf(" --to %s", xtables_ipaddr_to_numeric(&a));
		a.s_addr = r->max_ip;

		if (r->min_ip != r->max_ip)
			printf("-%s", xtables_ipaddr_to_numeric(&a));
		if (r->flags & IP_NAT_RANGE_PROTO_RANDOM) 
			random_selection = 1;
	}
	
	if (mr->info & IPT_SAME_NODST)
		printf(" --nodst");

	if (random_selection)
		printf(" --random");
}

static struct xtables_target same_tg_reg = {
	.name		= "SAME",
	.version	= XTABLES_VERSION,
	.family		= NFPROTO_IPV4,
	.size		= XT_ALIGN(sizeof(struct ipt_same_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct ipt_same_info)),
	.help		= SAME_help,
	.x6_parse	= SAME_parse,
	.print		= SAME_print,
	.save		= SAME_save,
	.x6_options	= SAME_opts,
};

void _init(void)
{
	xtables_register_target(&same_tg_reg);
}
