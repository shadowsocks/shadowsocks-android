#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <xtables.h>
#include <limits.h> /* INT_MAX in ip_tables.h/ip6_tables.h */
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv6/ip6_tables.h>
#include <linux/netfilter/xt_multiport.h>

enum {
	O_SOURCE_PORTS = 0,
	O_DEST_PORTS,
	O_SD_PORTS,
	F_SOURCE_PORTS = 1 << O_SOURCE_PORTS,
	F_DEST_PORTS   = 1 << O_DEST_PORTS,
	F_SD_PORTS     = 1 << O_SD_PORTS,
	F_ANY          = F_SOURCE_PORTS | F_DEST_PORTS | F_SD_PORTS,
};

/* Function which prints out usage message. */
static void multiport_help(void)
{
	printf(
"multiport match options:\n"
" --source-ports port[,port,port...]\n"
" --sports ...\n"
"				match source port(s)\n"
" --destination-ports port[,port,port...]\n"
" --dports ...\n"
"				match destination port(s)\n"
" --ports port[,port,port]\n"
"				match both source and destination port(s)\n"
" NOTE: this kernel does not support port ranges in multiport.\n");
}

static void multiport_help_v1(void)
{
	printf(
"multiport match options:\n"
"[!] --source-ports port[,port:port,port...]\n"
" --sports ...\n"
"				match source port(s)\n"
"[!] --destination-ports port[,port:port,port...]\n"
" --dports ...\n"
"				match destination port(s)\n"
"[!] --ports port[,port:port,port]\n"
"				match both source and destination port(s)\n");
}

static const struct xt_option_entry multiport_opts[] = {
	{.name = "source-ports", .id = O_SOURCE_PORTS, .type = XTTYPE_STRING,
	 .excl = F_ANY, .flags = XTOPT_INVERT},
	{.name = "sports", .id = O_SOURCE_PORTS, .type = XTTYPE_STRING,
	 .excl = F_ANY, .flags = XTOPT_INVERT},
	{.name = "destination-ports", .id = O_DEST_PORTS,
	 .type = XTTYPE_STRING, .excl = F_ANY, .flags = XTOPT_INVERT},
	{.name = "dports", .id = O_DEST_PORTS, .type = XTTYPE_STRING,
	 .excl = F_ANY, .flags = XTOPT_INVERT},
	{.name = "ports", .id = O_SD_PORTS, .type = XTTYPE_STRING,
	 .excl = F_ANY, .flags = XTOPT_INVERT},
	XTOPT_TABLEEND,
};

static const char *
proto_to_name(uint8_t proto)
{
	switch (proto) {
	case IPPROTO_TCP:
		return "tcp";
	case IPPROTO_UDP:
		return "udp";
	case IPPROTO_UDPLITE:
		return "udplite";
	case IPPROTO_SCTP:
		return "sctp";
	case IPPROTO_DCCP:
		return "dccp";
	default:
		return NULL;
	}
}

static unsigned int
parse_multi_ports(const char *portstring, uint16_t *ports, const char *proto)
{
	char *buffer, *cp, *next;
	unsigned int i;

	buffer = strdup(portstring);
	if (!buffer) xtables_error(OTHER_PROBLEM, "strdup failed");

	for (cp=buffer, i=0; cp && i<XT_MULTI_PORTS; cp=next,i++)
	{
		next=strchr(cp, ',');
		if (next) *next++='\0';
		ports[i] = xtables_parse_port(cp, proto);
	}
	if (cp) xtables_error(PARAMETER_PROBLEM, "too many ports specified");
	free(buffer);
	return i;
}

static void
parse_multi_ports_v1(const char *portstring, 
		     struct xt_multiport_v1 *multiinfo,
		     const char *proto)
{
	char *buffer, *cp, *next, *range;
	unsigned int i;
	uint16_t m;

	buffer = strdup(portstring);
	if (!buffer) xtables_error(OTHER_PROBLEM, "strdup failed");

	for (i=0; i<XT_MULTI_PORTS; i++)
		multiinfo->pflags[i] = 0;
 
	for (cp=buffer, i=0; cp && i<XT_MULTI_PORTS; cp=next, i++) {
		next=strchr(cp, ',');
 		if (next) *next++='\0';
		range = strchr(cp, ':');
		if (range) {
			if (i == XT_MULTI_PORTS-1)
				xtables_error(PARAMETER_PROBLEM,
					   "too many ports specified");
			*range++ = '\0';
		}
		multiinfo->ports[i] = xtables_parse_port(cp, proto);
		if (range) {
			multiinfo->pflags[i] = 1;
			multiinfo->ports[++i] = xtables_parse_port(range, proto);
			if (multiinfo->ports[i-1] >= multiinfo->ports[i])
				xtables_error(PARAMETER_PROBLEM,
					   "invalid portrange specified");
			m <<= 1;
		}
 	}
	multiinfo->count = i;
	if (cp) xtables_error(PARAMETER_PROBLEM, "too many ports specified");
 	free(buffer);
}

static const char *
check_proto(uint16_t pnum, uint8_t invflags)
{
	const char *proto;

	if (invflags & XT_INV_PROTO)
		xtables_error(PARAMETER_PROBLEM,
			   "multiport only works with TCP, UDP, UDPLITE, SCTP and DCCP");

	if ((proto = proto_to_name(pnum)) != NULL)
		return proto;
	else if (!pnum)
		xtables_error(PARAMETER_PROBLEM,
			   "multiport needs `-p tcp', `-p udp', `-p udplite', "
			   "`-p sctp' or `-p dccp'");
	else
		xtables_error(PARAMETER_PROBLEM,
			   "multiport only works with TCP, UDP, UDPLITE, SCTP and DCCP");
}

static void __multiport_parse(struct xt_option_call *cb, uint16_t pnum,
			      uint8_t invflags)
{
	const char *proto;
	struct xt_multiport *multiinfo = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_SOURCE_PORTS:
		proto = check_proto(pnum, invflags);
		multiinfo->count = parse_multi_ports(cb->arg,
						     multiinfo->ports, proto);
		multiinfo->flags = XT_MULTIPORT_SOURCE;
		break;
	case O_DEST_PORTS:
		proto = check_proto(pnum, invflags);
		multiinfo->count = parse_multi_ports(cb->arg,
						     multiinfo->ports, proto);
		multiinfo->flags = XT_MULTIPORT_DESTINATION;
		break;
	case O_SD_PORTS:
		proto = check_proto(pnum, invflags);
		multiinfo->count = parse_multi_ports(cb->arg,
						     multiinfo->ports, proto);
		multiinfo->flags = XT_MULTIPORT_EITHER;
		break;
	}
	if (cb->invert)
		xtables_error(PARAMETER_PROBLEM,
			   "multiport.0 does not support invert");
}

static void multiport_parse(struct xt_option_call *cb)
{
	const struct ipt_entry *entry = cb->xt_entry;
	return __multiport_parse(cb,
	       entry->ip.proto, entry->ip.invflags);
}

static void multiport_parse6(struct xt_option_call *cb)
{
	const struct ip6t_entry *entry = cb->xt_entry;
	return __multiport_parse(cb,
	       entry->ipv6.proto, entry->ipv6.invflags);
}

static void __multiport_parse_v1(struct xt_option_call *cb, uint16_t pnum,
				 uint8_t invflags)
{
	const char *proto;
	struct xt_multiport_v1 *multiinfo = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_SOURCE_PORTS:
		proto = check_proto(pnum, invflags);
		parse_multi_ports_v1(cb->arg, multiinfo, proto);
		multiinfo->flags = XT_MULTIPORT_SOURCE;
		break;
	case O_DEST_PORTS:
		proto = check_proto(pnum, invflags);
		parse_multi_ports_v1(cb->arg, multiinfo, proto);
		multiinfo->flags = XT_MULTIPORT_DESTINATION;
		break;
	case O_SD_PORTS:
		proto = check_proto(pnum, invflags);
		parse_multi_ports_v1(cb->arg, multiinfo, proto);
		multiinfo->flags = XT_MULTIPORT_EITHER;
		break;
	}
	if (cb->invert)
		multiinfo->invert = 1;
}

static void multiport_parse_v1(struct xt_option_call *cb)
{
	const struct ipt_entry *entry = cb->xt_entry;
	return __multiport_parse_v1(cb,
	       entry->ip.proto, entry->ip.invflags);
}

static void multiport_parse6_v1(struct xt_option_call *cb)
{
	const struct ip6t_entry *entry = cb->xt_entry;
	return __multiport_parse_v1(cb,
	       entry->ipv6.proto, entry->ipv6.invflags);
}

static void multiport_check(struct xt_fcheck_call *cb)
{
	if (cb->xflags == 0)
		xtables_error(PARAMETER_PROBLEM, "multiport expection an option");
}

static const char *
port_to_service(int port, uint8_t proto)
{
	const struct servent *service;

	if ((service = getservbyport(htons(port), proto_to_name(proto))))
		return service->s_name;

	return NULL;
}

static void
print_port(uint16_t port, uint8_t protocol, int numeric)
{
	const char *service;

	if (numeric || (service = port_to_service(port, protocol)) == NULL)
		printf("%u", port);
	else
		printf("%s", service);
}

static void
__multiport_print(const struct xt_entry_match *match, int numeric,
                  uint16_t proto)
{
	const struct xt_multiport *multiinfo
		= (const struct xt_multiport *)match->data;
	unsigned int i;

	printf(" multiport ");

	switch (multiinfo->flags) {
	case XT_MULTIPORT_SOURCE:
		printf("sports ");
		break;

	case XT_MULTIPORT_DESTINATION:
		printf("dports ");
		break;

	case XT_MULTIPORT_EITHER:
		printf("ports ");
		break;

	default:
		printf("ERROR ");
		break;
	}

	for (i=0; i < multiinfo->count; i++) {
		printf("%s", i ? "," : "");
		print_port(multiinfo->ports[i], proto, numeric);
	}
}

static void multiport_print(const void *ip_void,
                            const struct xt_entry_match *match, int numeric)
{
	const struct ipt_ip *ip = ip_void;
	__multiport_print(match, numeric, ip->proto);
}

static void multiport_print6(const void *ip_void,
                             const struct xt_entry_match *match, int numeric)
{
	const struct ip6t_ip6 *ip = ip_void;
	__multiport_print(match, numeric, ip->proto);
}

static void __multiport_print_v1(const struct xt_entry_match *match,
                                 int numeric, uint16_t proto)
{
	const struct xt_multiport_v1 *multiinfo
		= (const struct xt_multiport_v1 *)match->data;
	unsigned int i;

	printf(" multiport ");

	switch (multiinfo->flags) {
	case XT_MULTIPORT_SOURCE:
		printf("sports ");
		break;

	case XT_MULTIPORT_DESTINATION:
		printf("dports ");
		break;

	case XT_MULTIPORT_EITHER:
		printf("ports ");
		break;

	default:
		printf("ERROR ");
		break;
	}

	if (multiinfo->invert)
		printf(" !");

	for (i=0; i < multiinfo->count; i++) {
		printf("%s", i ? "," : "");
		print_port(multiinfo->ports[i], proto, numeric);
		if (multiinfo->pflags[i]) {
			printf(":");
			print_port(multiinfo->ports[++i], proto, numeric);
		}
	}
}

static void multiport_print_v1(const void *ip_void,
                               const struct xt_entry_match *match, int numeric)
{
	const struct ipt_ip *ip = ip_void;
	__multiport_print_v1(match, numeric, ip->proto);
}

static void multiport_print6_v1(const void *ip_void,
                                const struct xt_entry_match *match, int numeric)
{
	const struct ip6t_ip6 *ip = ip_void;
	__multiport_print_v1(match, numeric, ip->proto);
}

static void __multiport_save(const struct xt_entry_match *match,
                             uint16_t proto)
{
	const struct xt_multiport *multiinfo
		= (const struct xt_multiport *)match->data;
	unsigned int i;

	switch (multiinfo->flags) {
	case XT_MULTIPORT_SOURCE:
		printf(" --sports ");
		break;

	case XT_MULTIPORT_DESTINATION:
		printf(" --dports ");
		break;

	case XT_MULTIPORT_EITHER:
		printf(" --ports ");
		break;
	}

	for (i=0; i < multiinfo->count; i++) {
		printf("%s", i ? "," : "");
		print_port(multiinfo->ports[i], proto, 1);
	}
}

static void multiport_save(const void *ip_void,
                           const struct xt_entry_match *match)
{
	const struct ipt_ip *ip = ip_void;
	__multiport_save(match, ip->proto);
}

static void multiport_save6(const void *ip_void,
                            const struct xt_entry_match *match)
{
	const struct ip6t_ip6 *ip = ip_void;
	__multiport_save(match, ip->proto);
}

static void __multiport_save_v1(const struct xt_entry_match *match,
                                uint16_t proto)
{
	const struct xt_multiport_v1 *multiinfo
		= (const struct xt_multiport_v1 *)match->data;
	unsigned int i;

	if (multiinfo->invert)
		printf(" !");

	switch (multiinfo->flags) {
	case XT_MULTIPORT_SOURCE:
		printf(" --sports ");
		break;

	case XT_MULTIPORT_DESTINATION:
		printf(" --dports ");
		break;

	case XT_MULTIPORT_EITHER:
		printf(" --ports ");
		break;
	}

	for (i=0; i < multiinfo->count; i++) {
		printf("%s", i ? "," : "");
		print_port(multiinfo->ports[i], proto, 1);
		if (multiinfo->pflags[i]) {
			printf(":");
			print_port(multiinfo->ports[++i], proto, 1);
		}
	}
}

static void multiport_save_v1(const void *ip_void,
                              const struct xt_entry_match *match)
{
	const struct ipt_ip *ip = ip_void;
	__multiport_save_v1(match, ip->proto);
}

static void multiport_save6_v1(const void *ip_void,
                               const struct xt_entry_match *match)
{
	const struct ip6t_ip6 *ip = ip_void;
	__multiport_save_v1(match, ip->proto);
}

static struct xtables_match multiport_mt_reg[] = {
	{
		.family        = NFPROTO_IPV4,
		.name          = "multiport",
		.revision      = 0,
		.version       = XTABLES_VERSION,
		.size          = XT_ALIGN(sizeof(struct xt_multiport)),
		.userspacesize = XT_ALIGN(sizeof(struct xt_multiport)),
		.help          = multiport_help,
		.x6_parse      = multiport_parse,
		.x6_fcheck     = multiport_check,
		.print         = multiport_print,
		.save          = multiport_save,
		.x6_options    = multiport_opts,
	},
	{
		.family        = NFPROTO_IPV6,
		.name          = "multiport",
		.revision      = 0,
		.version       = XTABLES_VERSION,
		.size          = XT_ALIGN(sizeof(struct xt_multiport)),
		.userspacesize = XT_ALIGN(sizeof(struct xt_multiport)),
		.help          = multiport_help,
		.x6_parse      = multiport_parse6,
		.x6_fcheck     = multiport_check,
		.print         = multiport_print6,
		.save          = multiport_save6,
		.x6_options    = multiport_opts,
	},
	{
		.family        = NFPROTO_IPV4,
		.name          = "multiport",
		.version       = XTABLES_VERSION,
		.revision      = 1,
		.size          = XT_ALIGN(sizeof(struct xt_multiport_v1)),
		.userspacesize = XT_ALIGN(sizeof(struct xt_multiport_v1)),
		.help          = multiport_help_v1,
		.x6_parse      = multiport_parse_v1,
		.x6_fcheck     = multiport_check,
		.print         = multiport_print_v1,
		.save          = multiport_save_v1,
		.x6_options    = multiport_opts,
	},
	{
		.family        = NFPROTO_IPV6,
		.name          = "multiport",
		.version       = XTABLES_VERSION,
		.revision      = 1,
		.size          = XT_ALIGN(sizeof(struct xt_multiport_v1)),
		.userspacesize = XT_ALIGN(sizeof(struct xt_multiport_v1)),
		.help          = multiport_help_v1,
		.x6_parse      = multiport_parse6_v1,
		.x6_fcheck     = multiport_check,
		.print         = multiport_print6_v1,
		.save          = multiport_save6_v1,
		.x6_options    = multiport_opts,
	},
};

void
_init(void)
{
	xtables_register_matches(multiport_mt_reg, ARRAY_SIZE(multiport_mt_reg));
}
