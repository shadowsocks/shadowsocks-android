/* Shared library add-on to iptables to add TCP support. */
#include <stdbool.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <netinet/in.h>
#include <xtables.h>
#include <linux/netfilter/xt_tcpudp.h>

static void tcp_help(void)
{
	printf(
"tcp match options:\n"
"[!] --tcp-flags mask comp	match when TCP flags & mask == comp\n"
"				(Flags: SYN ACK FIN RST URG PSH ALL NONE)\n"
"[!] --syn			match when only SYN flag set\n"
"				(equivalent to --tcp-flags SYN,RST,ACK,FIN SYN)\n"
"[!] --source-port port[:port]\n"
" --sport ...\n"
"				match source port(s)\n"
"[!] --destination-port port[:port]\n"
" --dport ...\n"
"				match destination port(s)\n"
"[!] --tcp-option number        match if TCP option set\n");
}

static const struct option tcp_opts[] = {
	{.name = "source-port",      .has_arg = true,  .val = '1'},
	{.name = "sport",            .has_arg = true,  .val = '1'}, /* synonym */
	{.name = "destination-port", .has_arg = true,  .val = '2'},
	{.name = "dport",            .has_arg = true,  .val = '2'}, /* synonym */
	{.name = "syn",              .has_arg = false, .val = '3'},
	{.name = "tcp-flags",        .has_arg = true,  .val = '4'},
	{.name = "tcp-option",       .has_arg = true,  .val = '5'},
	XT_GETOPT_TABLEEND,
};

static void
parse_tcp_ports(const char *portstring, uint16_t *ports)
{
	char *buffer;
	char *cp;

	buffer = strdup(portstring);
	if ((cp = strchr(buffer, ':')) == NULL)
		ports[0] = ports[1] = xtables_parse_port(buffer, "tcp");
	else {
		*cp = '\0';
		cp++;

		ports[0] = buffer[0] ? xtables_parse_port(buffer, "tcp") : 0;
		ports[1] = cp[0] ? xtables_parse_port(cp, "tcp") : 0xFFFF;

		if (ports[0] > ports[1])
			xtables_error(PARAMETER_PROBLEM,
				   "invalid portrange (min > max)");
	}
	free(buffer);
}

struct tcp_flag_names {
	const char *name;
	unsigned int flag;
};

static const struct tcp_flag_names tcp_flag_names[]
= { { "FIN", 0x01 },
    { "SYN", 0x02 },
    { "RST", 0x04 },
    { "PSH", 0x08 },
    { "ACK", 0x10 },
    { "URG", 0x20 },
    { "ALL", 0x3F },
    { "NONE", 0 },
};

static unsigned int
parse_tcp_flag(const char *flags)
{
	unsigned int ret = 0;
	char *ptr;
	char *buffer;

	buffer = strdup(flags);

	for (ptr = strtok(buffer, ","); ptr; ptr = strtok(NULL, ",")) {
		unsigned int i;
		for (i = 0; i < ARRAY_SIZE(tcp_flag_names); ++i)
			if (strcasecmp(tcp_flag_names[i].name, ptr) == 0) {
				ret |= tcp_flag_names[i].flag;
				break;
			}
		if (i == ARRAY_SIZE(tcp_flag_names))
			xtables_error(PARAMETER_PROBLEM,
				   "Unknown TCP flag `%s'", ptr);
	}

	free(buffer);
	return ret;
}

static void
parse_tcp_flags(struct xt_tcp *tcpinfo,
		const char *mask,
		const char *cmp,
		int invert)
{
	tcpinfo->flg_mask = parse_tcp_flag(mask);
	tcpinfo->flg_cmp = parse_tcp_flag(cmp);

	if (invert)
		tcpinfo->invflags |= XT_TCP_INV_FLAGS;
}

static void
parse_tcp_option(const char *option, uint8_t *result)
{
	unsigned int ret;

	if (!xtables_strtoui(option, NULL, &ret, 1, UINT8_MAX))
		xtables_error(PARAMETER_PROBLEM, "Bad TCP option \"%s\"", option);

	*result = ret;
}

static void tcp_init(struct xt_entry_match *m)
{
	struct xt_tcp *tcpinfo = (struct xt_tcp *)m->data;

	tcpinfo->spts[1] = tcpinfo->dpts[1] = 0xFFFF;
}

#define TCP_SRC_PORTS 0x01
#define TCP_DST_PORTS 0x02
#define TCP_FLAGS 0x04
#define TCP_OPTION	0x08

static int
tcp_parse(int c, char **argv, int invert, unsigned int *flags,
          const void *entry, struct xt_entry_match **match)
{
	struct xt_tcp *tcpinfo = (struct xt_tcp *)(*match)->data;

	switch (c) {
	case '1':
		if (*flags & TCP_SRC_PORTS)
			xtables_error(PARAMETER_PROBLEM,
				   "Only one `--source-port' allowed");
		xtables_check_inverse(optarg, &invert, &optind, 0, argv);
		parse_tcp_ports(optarg, tcpinfo->spts);
		if (invert)
			tcpinfo->invflags |= XT_TCP_INV_SRCPT;
		*flags |= TCP_SRC_PORTS;
		break;

	case '2':
		if (*flags & TCP_DST_PORTS)
			xtables_error(PARAMETER_PROBLEM,
				   "Only one `--destination-port' allowed");
		xtables_check_inverse(optarg, &invert, &optind, 0, argv);
		parse_tcp_ports(optarg, tcpinfo->dpts);
		if (invert)
			tcpinfo->invflags |= XT_TCP_INV_DSTPT;
		*flags |= TCP_DST_PORTS;
		break;

	case '3':
		if (*flags & TCP_FLAGS)
			xtables_error(PARAMETER_PROBLEM,
				   "Only one of `--syn' or `--tcp-flags' "
				   " allowed");
		parse_tcp_flags(tcpinfo, "SYN,RST,ACK,FIN", "SYN", invert);
		*flags |= TCP_FLAGS;
		break;

	case '4':
		if (*flags & TCP_FLAGS)
			xtables_error(PARAMETER_PROBLEM,
				   "Only one of `--syn' or `--tcp-flags' "
				   " allowed");
		xtables_check_inverse(optarg, &invert, &optind, 0, argv);

		if (!argv[optind]
		    || argv[optind][0] == '-' || argv[optind][0] == '!')
			xtables_error(PARAMETER_PROBLEM,
				   "--tcp-flags requires two args.");

		parse_tcp_flags(tcpinfo, optarg, argv[optind],
				invert);
		optind++;
		*flags |= TCP_FLAGS;
		break;

	case '5':
		if (*flags & TCP_OPTION)
			xtables_error(PARAMETER_PROBLEM,
				   "Only one `--tcp-option' allowed");
		xtables_check_inverse(optarg, &invert, &optind, 0, argv);
		parse_tcp_option(optarg, &tcpinfo->option);
		if (invert)
			tcpinfo->invflags |= XT_TCP_INV_OPTION;
		*flags |= TCP_OPTION;
		break;
	}

	return 1;
}

static const char *
port_to_service(int port)
{
	const struct servent *service;

	if ((service = getservbyport(htons(port), "tcp")))
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
print_option(uint8_t option, int invert, int numeric)
{
	if (option || invert)
		printf(" option=%s%u", invert ? "!" : "", option);
}

static void
print_tcpf(uint8_t flags)
{
	int have_flag = 0;

	while (flags) {
		unsigned int i;

		for (i = 0; (flags & tcp_flag_names[i].flag) == 0; i++);

		if (have_flag)
			printf(",");
		printf("%s", tcp_flag_names[i].name);
		have_flag = 1;

		flags &= ~tcp_flag_names[i].flag;
	}

	if (!have_flag)
		printf("NONE");
}

static void
print_flags(uint8_t mask, uint8_t cmp, int invert, int numeric)
{
	if (mask || invert) {
		printf("flags:%s", invert ? "!" : "");
		if (numeric)
			printf(" 0x%02X/0x%02X", mask, cmp);
		else {
			printf(" ");
			print_tcpf(mask);
			printf("/");
			print_tcpf(cmp);
		}
	}
}

static void
tcp_print(const void *ip, const struct xt_entry_match *match, int numeric)
{
	const struct xt_tcp *tcp = (struct xt_tcp *)match->data;

	printf(" tcp");
	print_ports("spt", tcp->spts[0], tcp->spts[1],
		    tcp->invflags & XT_TCP_INV_SRCPT,
		    numeric);
	print_ports("dpt", tcp->dpts[0], tcp->dpts[1],
		    tcp->invflags & XT_TCP_INV_DSTPT,
		    numeric);
	print_option(tcp->option,
		     tcp->invflags & XT_TCP_INV_OPTION,
		     numeric);
	print_flags(tcp->flg_mask, tcp->flg_cmp,
		    tcp->invflags & XT_TCP_INV_FLAGS,
		    numeric);
	if (tcp->invflags & ~XT_TCP_INV_MASK)
		printf(" Unknown invflags: 0x%X",
		       tcp->invflags & ~XT_TCP_INV_MASK);
}

static void tcp_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_tcp *tcpinfo = (struct xt_tcp *)match->data;

	if (tcpinfo->spts[0] != 0
	    || tcpinfo->spts[1] != 0xFFFF) {
		if (tcpinfo->invflags & XT_TCP_INV_SRCPT)
			printf(" !");
		if (tcpinfo->spts[0]
		    != tcpinfo->spts[1])
			printf(" --sport %u:%u",
			       tcpinfo->spts[0],
			       tcpinfo->spts[1]);
		else
			printf(" --sport %u",
			       tcpinfo->spts[0]);
	}

	if (tcpinfo->dpts[0] != 0
	    || tcpinfo->dpts[1] != 0xFFFF) {
		if (tcpinfo->invflags & XT_TCP_INV_DSTPT)
			printf(" !");
		if (tcpinfo->dpts[0]
		    != tcpinfo->dpts[1])
			printf(" --dport %u:%u",
			       tcpinfo->dpts[0],
			       tcpinfo->dpts[1]);
		else
			printf(" --dport %u",
			       tcpinfo->dpts[0]);
	}

	if (tcpinfo->option
	    || (tcpinfo->invflags & XT_TCP_INV_OPTION)) {
		if (tcpinfo->invflags & XT_TCP_INV_OPTION)
			printf(" !");
		printf(" --tcp-option %u", tcpinfo->option);
	}

	if (tcpinfo->flg_mask
	    || (tcpinfo->invflags & XT_TCP_INV_FLAGS)) {
		if (tcpinfo->invflags & XT_TCP_INV_FLAGS)
			printf(" !");
		printf(" --tcp-flags ");
		if (tcpinfo->flg_mask != 0xFF) {
			print_tcpf(tcpinfo->flg_mask);
		}
		printf(" ");
		print_tcpf(tcpinfo->flg_cmp);
	}
}

static struct xtables_match tcp_match = {
	.family		= NFPROTO_UNSPEC,
	.name		= "tcp",
	.version	= XTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_tcp)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_tcp)),
	.help		= tcp_help,
	.init		= tcp_init,
	.parse		= tcp_parse,
	.print		= tcp_print,
	.save		= tcp_save,
	.extra_opts	= tcp_opts,
};

void
_init(void)
{
	xtables_register_match(&tcp_match);
}
