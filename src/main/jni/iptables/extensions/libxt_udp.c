#include <stdint.h>
#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <xtables.h>
#include <linux/netfilter/xt_tcpudp.h>

enum {
	O_SOURCE_PORT = 0,
	O_DEST_PORT,
};

static void udp_help(void)
{
	printf(
"udp match options:\n"
"[!] --source-port port[:port]\n"
" --sport ...\n"
"				match source port(s)\n"
"[!] --destination-port port[:port]\n"
" --dport ...\n"
"				match destination port(s)\n");
}

#define s struct xt_udp
static const struct xt_option_entry udp_opts[] = {
	{.name = "source-port", .id = O_SOURCE_PORT, .type = XTTYPE_PORTRC,
	 .flags = XTOPT_INVERT | XTOPT_PUT, XTOPT_POINTER(s, spts)},
	{.name = "sport", .id = O_SOURCE_PORT, .type = XTTYPE_PORTRC,
	 .flags = XTOPT_INVERT | XTOPT_PUT, XTOPT_POINTER(s, spts)},
	{.name = "destination-port", .id = O_DEST_PORT, .type = XTTYPE_PORTRC,
	 .flags = XTOPT_INVERT | XTOPT_PUT, XTOPT_POINTER(s, dpts)},
	{.name = "dport", .id = O_DEST_PORT, .type = XTTYPE_PORTRC,
	 .flags = XTOPT_INVERT | XTOPT_PUT, XTOPT_POINTER(s, dpts)},
	XTOPT_TABLEEND,
};
#undef s

static void udp_init(struct xt_entry_match *m)
{
	struct xt_udp *udpinfo = (struct xt_udp *)m->data;

	udpinfo->spts[1] = udpinfo->dpts[1] = 0xFFFF;
}

static void udp_parse(struct xt_option_call *cb)
{
	struct xt_udp *udpinfo = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_SOURCE_PORT:
		if (cb->invert)
			udpinfo->invflags |= XT_UDP_INV_SRCPT;
		break;
	case O_DEST_PORT:
		if (cb->invert)
			udpinfo->invflags |= XT_UDP_INV_DSTPT;
		break;
	}
}

static const char *
port_to_service(int port)
{
	const struct servent *service;

	if ((service = getservbyport(htons(port), "udp")))
		return service->s_name;

	return NULL;
}

static void
print_port(uint16_t port, int numeric)
{
	const char *service;

	if (numeric || (service = port_to_service(port)) == NULL)
		printf("%u", port);
	else
		printf("%s", service);
}

static void
print_ports(const char *name, uint16_t min, uint16_t max,
	    int invert, int numeric)
{
	const char *inv = invert ? "!" : "";

	if (min != 0 || max != 0xFFFF || invert) {
		printf(" %s", name);
		if (min == max) {
			printf(":%s", inv);
			print_port(min, numeric);
		} else {
			printf("s:%s", inv);
			print_port(min, numeric);
			printf(":");
			print_port(max, numeric);
		}
	}
}

static void
udp_print(const void *ip, const struct xt_entry_match *match, int numeric)
{
	const struct xt_udp *udp = (struct xt_udp *)match->data;

	printf(" udp");
	print_ports("spt", udp->spts[0], udp->spts[1],
		    udp->invflags & XT_UDP_INV_SRCPT,
		    numeric);
	print_ports("dpt", udp->dpts[0], udp->dpts[1],
		    udp->invflags & XT_UDP_INV_DSTPT,
		    numeric);
	if (udp->invflags & ~XT_UDP_INV_MASK)
		printf(" Unknown invflags: 0x%X",
		       udp->invflags & ~XT_UDP_INV_MASK);
}

static void udp_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_udp *udpinfo = (struct xt_udp *)match->data;

	if (udpinfo->spts[0] != 0
	    || udpinfo->spts[1] != 0xFFFF) {
		if (udpinfo->invflags & XT_UDP_INV_SRCPT)
			printf(" !");
		if (udpinfo->spts[0]
		    != udpinfo->spts[1])
			printf(" --sport %u:%u",
			       udpinfo->spts[0],
			       udpinfo->spts[1]);
		else
			printf(" --sport %u",
			       udpinfo->spts[0]);
	}

	if (udpinfo->dpts[0] != 0
	    || udpinfo->dpts[1] != 0xFFFF) {
		if (udpinfo->invflags & XT_UDP_INV_DSTPT)
			printf(" !");
		if (udpinfo->dpts[0]
		    != udpinfo->dpts[1])
			printf(" --dport %u:%u",
			       udpinfo->dpts[0],
			       udpinfo->dpts[1]);
		else
			printf(" --dport %u",
			       udpinfo->dpts[0]);
	}
}

static struct xtables_match udp_match = {
	.family		= NFPROTO_UNSPEC,
	.name		= "udp",
	.version	= XTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_udp)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_udp)),
	.help		= udp_help,
	.init		= udp_init,
	.print		= udp_print,
	.save		= udp_save,
	.x6_parse	= udp_parse,
	.x6_options	= udp_opts,
};

void
_init(void)
{
	xtables_register_match(&udp_match);
}
