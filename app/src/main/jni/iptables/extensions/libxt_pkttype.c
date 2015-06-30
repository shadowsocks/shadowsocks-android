/* 
 * Shared library add-on to iptables to match 
 * packets by their type (BROADCAST, UNICAST, MULTICAST). 
 *
 * Michal Ludvig <michal@logix.cz>
 */
#include <stdio.h>
#include <string.h>
#include <xtables.h>
#include <linux/if_packet.h>
#include <linux/netfilter/xt_pkttype.h>

enum {
	O_PKTTYPE = 0,
};

struct pkttypes {
	const char *name;
	unsigned char pkttype;
	unsigned char printhelp;
	const char *help;
};

static const struct pkttypes supported_types[] = {
	{"unicast", PACKET_HOST, 1, "to us"},
	{"broadcast", PACKET_BROADCAST, 1, "to all"},
	{"multicast", PACKET_MULTICAST, 1, "to group"},
/*
	{"otherhost", PACKET_OTHERHOST, 1, "to someone else"},
	{"outgoing", PACKET_OUTGOING, 1, "outgoing of any type"},
*/
	/* aliases */
	{"bcast", PACKET_BROADCAST, 0, NULL},
	{"mcast", PACKET_MULTICAST, 0, NULL},
	{"host", PACKET_HOST, 0, NULL}
};

static void print_types(void)
{
	unsigned int	i;
	
	printf("Valid packet types:\n");
	for (i = 0; i < ARRAY_SIZE(supported_types); ++i)
		if(supported_types[i].printhelp == 1)
			printf("\t%-14s\t\t%s\n", supported_types[i].name, supported_types[i].help);
	printf("\n");
}

static void pkttype_help(void)
{
	printf(
"pkttype match options:\n"
"[!] --pkt-type packettype    match packet type\n");
	print_types();
}

static const struct xt_option_entry pkttype_opts[] = {
	{.name = "pkt-type", .id = O_PKTTYPE, .type = XTTYPE_STRING,
	 .flags = XTOPT_MAND | XTOPT_INVERT},
	XTOPT_TABLEEND,
};

static void parse_pkttype(const char *pkttype, struct xt_pkttype_info *info)
{
	unsigned int	i;
	
	for (i = 0; i < ARRAY_SIZE(supported_types); ++i)
		if(strcasecmp(pkttype, supported_types[i].name)==0)
		{
			info->pkttype=supported_types[i].pkttype;
			return;
		}
	
	xtables_error(PARAMETER_PROBLEM, "Bad packet type '%s'", pkttype);
}

static void pkttype_parse(struct xt_option_call *cb)
{
	struct xt_pkttype_info *info = cb->data;

	xtables_option_parse(cb);
	parse_pkttype(cb->arg, info);
	if (cb->invert)
		info->invert = 1;
}

static void print_pkttype(const struct xt_pkttype_info *info)
{
	unsigned int	i;
	
	for (i = 0; i < ARRAY_SIZE(supported_types); ++i)
		if(supported_types[i].pkttype==info->pkttype)
		{
			printf("%s", supported_types[i].name);
			return;
		}

	printf("%d", info->pkttype);	/* in case we didn't find an entry in named-packtes */
}

static void pkttype_print(const void *ip, const struct xt_entry_match *match,
                          int numeric)
{
	const struct xt_pkttype_info *info = (const void *)match->data;
	
	printf(" PKTTYPE %s= ", info->invert ? "!" : "");
	print_pkttype(info);
}

static void pkttype_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_pkttype_info *info = (const void *)match->data;
	
	printf("%s --pkt-type ", info->invert ? " !" : "");
	print_pkttype(info);
}

static struct xtables_match pkttype_match = {
	.family		= NFPROTO_UNSPEC,
	.name		= "pkttype",
	.version	= XTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_pkttype_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_pkttype_info)),
	.help		= pkttype_help,
	.print		= pkttype_print,
	.save		= pkttype_save,
	.x6_parse	= pkttype_parse,
	.x6_options	= pkttype_opts,
};

void _init(void)
{
	xtables_register_match(&pkttype_match);
}
