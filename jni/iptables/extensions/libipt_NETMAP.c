/* Shared library add-on to iptables to add static NAT support.
   Author: Svenning Soerensen <svenning@post5.tele.dk>
*/
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <xtables.h>
#include <net/netfilter/nf_nat.h>

#define MODULENAME "NETMAP"

enum {
	O_TO = 0,
};

static const struct xt_option_entry NETMAP_opts[] = {
	{.name = "to", .id = O_TO, .type = XTTYPE_HOSTMASK,
	 .flags = XTOPT_MAND},
	XTOPT_TABLEEND,
};

static void NETMAP_help(void)
{
	printf(MODULENAME" target options:\n"
	       "  --%s address[/mask]\n"
	       "				Network address to map to.\n\n",
	       NETMAP_opts[0].name);
}

static int
netmask2bits(uint32_t netmask)
{
	uint32_t bm;
	int bits;

	netmask = ntohl(netmask);
	for (bits = 0, bm = 0x80000000; netmask & bm; netmask <<= 1)
		bits++;
	if (netmask)
		return -1; /* holes in netmask */
	return bits;
}

static void NETMAP_init(struct xt_entry_target *t)
{
	struct nf_nat_multi_range *mr = (struct nf_nat_multi_range *)t->data;

	/* Actually, it's 0, but it's ignored at the moment. */
	mr->rangesize = 1;
}

static void NETMAP_parse(struct xt_option_call *cb)
{
	struct nf_nat_multi_range *mr = cb->data;
	struct nf_nat_range *range = &mr->range[0];

	xtables_option_parse(cb);
	range->flags |= IP_NAT_RANGE_MAP_IPS;
	range->min_ip = cb->val.haddr.ip & cb->val.hmask.ip;
	range->max_ip = range->min_ip | ~cb->val.hmask.ip;
}

static void NETMAP_print(const void *ip, const struct xt_entry_target *target,
                         int numeric)
{
	const struct nf_nat_multi_range *mr = (const void *)target->data;
	const struct nf_nat_range *r = &mr->range[0];
	struct in_addr a;
	int bits;

	a.s_addr = r->min_ip;
	printf("%s", xtables_ipaddr_to_numeric(&a));
	a.s_addr = ~(r->min_ip ^ r->max_ip);
	bits = netmask2bits(a.s_addr);
	if (bits < 0)
		printf("/%s", xtables_ipaddr_to_numeric(&a));
	else
		printf("/%d", bits);
}

static void NETMAP_save(const void *ip, const struct xt_entry_target *target)
{
	printf(" --%s ", NETMAP_opts[0].name);
	NETMAP_print(ip, target, 0);
}

static struct xtables_target netmap_tg_reg = {
	.name		= MODULENAME,
	.version	= XTABLES_VERSION,
	.family		= NFPROTO_IPV4,
	.size		= XT_ALIGN(sizeof(struct nf_nat_multi_range)),
	.userspacesize	= XT_ALIGN(sizeof(struct nf_nat_multi_range)),
	.help		= NETMAP_help,
	.init		= NETMAP_init,
	.x6_parse	= NETMAP_parse,
	.print		= NETMAP_print,
	.save		= NETMAP_save,
	.x6_options	= NETMAP_opts,
};

void _init(void)
{
	xtables_register_target(&netmap_tg_reg);
}
