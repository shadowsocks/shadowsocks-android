#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <xtables.h>
#include <limits.h> /* INT_MAX in ip_tables.h */
#include <linux/netfilter_ipv4/ip_tables.h>
#include <net/netfilter/nf_nat.h>

enum {
	O_TO_PORTS = 0,
	O_RANDOM,
	F_TO_PORTS = 1 << O_TO_PORTS,
	F_RANDOM   = 1 << O_RANDOM,
};

static void REDIRECT_help(void)
{
	printf(
"REDIRECT target options:\n"
" --to-ports <port>[-<port>]\n"
"				Port (range) to map to.\n"
" [--random]\n");
}

static const struct xt_option_entry REDIRECT_opts[] = {
	{.name = "to-ports", .id = O_TO_PORTS, .type = XTTYPE_STRING},
	{.name = "random", .id = O_RANDOM, .type = XTTYPE_NONE},
	XTOPT_TABLEEND,
};

static void REDIRECT_init(struct xt_entry_target *t)
{
	struct nf_nat_multi_range *mr = (struct nf_nat_multi_range *)t->data;

	/* Actually, it's 0, but it's ignored at the moment. */
	mr->rangesize = 1;
}

/* Parses ports */
static void
parse_ports(const char *arg, struct nf_nat_multi_range *mr)
{
	char *end = "";
	unsigned int port, maxport;

	mr->range[0].flags |= IP_NAT_RANGE_PROTO_SPECIFIED;

	if (!xtables_strtoui(arg, &end, &port, 0, UINT16_MAX) &&
	    (port = xtables_service_to_port(arg, NULL)) == (unsigned)-1)
		xtables_param_act(XTF_BAD_VALUE, "REDIRECT", "--to-ports", arg);

	switch (*end) {
	case '\0':
		mr->range[0].min.tcp.port
			= mr->range[0].max.tcp.port
			= htons(port);
		return;
	case '-':
		if (!xtables_strtoui(end + 1, NULL, &maxport, 0, UINT16_MAX) &&
		    (maxport = xtables_service_to_port(end + 1, NULL)) == (unsigned)-1)
			break;

		if (maxport < port)
			break;

		mr->range[0].min.tcp.port = htons(port);
		mr->range[0].max.tcp.port = htons(maxport);
		return;
	default:
		break;
	}
	xtables_param_act(XTF_BAD_VALUE, "REDIRECT", "--to-ports", arg);
}

static void REDIRECT_parse(struct xt_option_call *cb)
{
	const struct ipt_entry *entry = cb->xt_entry;
	struct nf_nat_multi_range *mr = (void *)(*cb->target)->data;
	int portok;

	if (entry->ip.proto == IPPROTO_TCP
	    || entry->ip.proto == IPPROTO_UDP
	    || entry->ip.proto == IPPROTO_SCTP
	    || entry->ip.proto == IPPROTO_DCCP
	    || entry->ip.proto == IPPROTO_ICMP)
		portok = 1;
	else
		portok = 0;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_TO_PORTS:
		if (!portok)
			xtables_error(PARAMETER_PROBLEM,
				   "Need TCP, UDP, SCTP or DCCP with port specification");
		parse_ports(cb->arg, mr);
		if (cb->xflags & F_RANDOM)
			mr->range[0].flags |= IP_NAT_RANGE_PROTO_RANDOM;
		break;
	case O_RANDOM:
		if (cb->xflags & F_TO_PORTS)
			mr->range[0].flags |= IP_NAT_RANGE_PROTO_RANDOM;
		break;
	}
}

static void REDIRECT_print(const void *ip, const struct xt_entry_target *target,
                           int numeric)
{
	const struct nf_nat_multi_range *mr = (const void *)target->data;
	const struct nf_nat_range *r = &mr->range[0];

	if (r->flags & IP_NAT_RANGE_PROTO_SPECIFIED) {
		printf(" redir ports ");
		printf("%hu", ntohs(r->min.tcp.port));
		if (r->max.tcp.port != r->min.tcp.port)
			printf("-%hu", ntohs(r->max.tcp.port));
		if (mr->range[0].flags & IP_NAT_RANGE_PROTO_RANDOM)
			printf(" random");
	}
}

static void REDIRECT_save(const void *ip, const struct xt_entry_target *target)
{
	const struct nf_nat_multi_range *mr = (const void *)target->data;
	const struct nf_nat_range *r = &mr->range[0];

	if (r->flags & IP_NAT_RANGE_PROTO_SPECIFIED) {
		printf(" --to-ports ");
		printf("%hu", ntohs(r->min.tcp.port));
		if (r->max.tcp.port != r->min.tcp.port)
			printf("-%hu", ntohs(r->max.tcp.port));
		if (mr->range[0].flags & IP_NAT_RANGE_PROTO_RANDOM)
			printf(" --random");
	}
}

static struct xtables_target redirect_tg_reg = {
	.name		= "REDIRECT",
	.version	= XTABLES_VERSION,
	.family		= NFPROTO_IPV4,
	.size		= XT_ALIGN(sizeof(struct nf_nat_multi_range)),
	.userspacesize	= XT_ALIGN(sizeof(struct nf_nat_multi_range)),
	.help		= REDIRECT_help,
	.init		= REDIRECT_init,
 	.x6_parse	= REDIRECT_parse,
	.print		= REDIRECT_print,
	.save		= REDIRECT_save,
	.x6_options	= REDIRECT_opts,
};

void _init(void)
{
	xtables_register_target(&redirect_tg_reg);
}
