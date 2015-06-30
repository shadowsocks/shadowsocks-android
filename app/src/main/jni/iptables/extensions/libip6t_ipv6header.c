/* ipv6header match - matches IPv6 packets based
on whether they contain certain headers */

/* Original idea: Brad Chapman 
 * Rewritten by: Andras Kis-Szabo <kisza@sch.bme.hu> */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <xtables.h>
#include <linux/netfilter_ipv6/ip6t_ipv6header.h>

enum {
	O_HEADER = 0,
	O_SOFT,
};

/* A few hardcoded protocols for 'all' and in case the user has no
 *    /etc/protocols */
struct pprot {
	char *name;
	uint8_t num;
};

struct numflag {
	uint8_t proto;
	uint8_t flag;
};

static const struct pprot chain_protos[] = {
	{ "hop-by-hop", IPPROTO_HOPOPTS },
	{ "protocol", IPPROTO_RAW },
	{ "hop", IPPROTO_HOPOPTS },
	{ "dst", IPPROTO_DSTOPTS },
	{ "route", IPPROTO_ROUTING },
	{ "frag", IPPROTO_FRAGMENT },
	{ "auth", IPPROTO_AH },
	{ "esp", IPPROTO_ESP },
	{ "none", IPPROTO_NONE },
	{ "prot", IPPROTO_RAW },
	{ "0", IPPROTO_HOPOPTS },
	{ "60", IPPROTO_DSTOPTS },
	{ "43", IPPROTO_ROUTING },
	{ "44", IPPROTO_FRAGMENT },
	{ "51", IPPROTO_AH },
	{ "50", IPPROTO_ESP },
	{ "59", IPPROTO_NONE },
	{ "255", IPPROTO_RAW },
	/* { "all", 0 }, */
};

static const struct numflag chain_flags[] = {
	{ IPPROTO_HOPOPTS, MASK_HOPOPTS },
	{ IPPROTO_DSTOPTS, MASK_DSTOPTS },
	{ IPPROTO_ROUTING, MASK_ROUTING },
	{ IPPROTO_FRAGMENT, MASK_FRAGMENT },
	{ IPPROTO_AH, MASK_AH },
	{ IPPROTO_ESP, MASK_ESP },
	{ IPPROTO_NONE, MASK_NONE },
	{ IPPROTO_RAW, MASK_PROTO },
};

static const char *
proto_to_name(uint8_t proto, int nolookup)
{
        unsigned int i;

        if (proto && !nolookup) {
		const struct protoent *pent = getprotobynumber(proto);
                if (pent)
                        return pent->p_name;
        }

        for (i = 0; i < ARRAY_SIZE(chain_protos); ++i)
                if (chain_protos[i].num == proto)
                        return chain_protos[i].name;

        return NULL;
}

static uint16_t
name_to_proto(const char *s)
{
        unsigned int proto=0;
	const struct protoent *pent;

        if ((pent = getprotobyname(s)))
        	proto = pent->p_proto;
        else {
        	unsigned int i;
        	for (i = 0; i < ARRAY_SIZE(chain_protos); ++i)
        		if (strcmp(s, chain_protos[i].name) == 0) {
        			proto = chain_protos[i].num;
        			break;
        		}

		if (i == ARRAY_SIZE(chain_protos))
			xtables_error(PARAMETER_PROBLEM,
        			"unknown header `%s' specified",
        			s);
        }

        return proto;
}

static unsigned int 
add_proto_to_mask(int proto){
	unsigned int i=0, flag=0;

	for (i = 0; i < ARRAY_SIZE(chain_flags); ++i)
			if (proto == chain_flags[i].proto){
				flag = chain_flags[i].flag;
				break;
			}

	if (i == ARRAY_SIZE(chain_flags))
		xtables_error(PARAMETER_PROBLEM,
		"unknown header `%d' specified",
		proto);
	
	return flag;
}	

static void ipv6header_help(void)
{
	printf(
"ipv6header match options:\n"
"[!] --header headers     Type of header to match, by name\n"
"                         names: hop,dst,route,frag,auth,esp,none,proto\n"
"                    long names: hop-by-hop,ipv6-opts,ipv6-route,\n"
"                                ipv6-frag,ah,esp,ipv6-nonxt,protocol\n"
"                       numbers: 0,60,43,44,51,50,59\n"
"--soft                    The header CONTAINS the specified extensions\n");
}

static const struct xt_option_entry ipv6header_opts[] = {
	{.name = "header", .id = O_HEADER, .type = XTTYPE_STRING,
	 .flags = XTOPT_MAND | XTOPT_INVERT},
	{.name = "soft", .id = O_SOFT, .type = XTTYPE_NONE},
	XTOPT_TABLEEND,
};

static unsigned int
parse_header(const char *flags) {
        unsigned int ret = 0;
        char *ptr;
        char *buffer;

        buffer = strdup(flags);

        for (ptr = strtok(buffer, ","); ptr; ptr = strtok(NULL, ",")) 
		ret |= add_proto_to_mask(name_to_proto(ptr));
                
        free(buffer);
        return ret;
}

static void ipv6header_parse(struct xt_option_call *cb)
{
	struct ip6t_ipv6header_info *info = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_HEADER:
		if (!(info->matchflags = parse_header(cb->arg)))
			xtables_error(PARAMETER_PROBLEM, "ip6t_ipv6header: cannot parse header names");
		if (cb->invert) 
			info->invflags |= 0xFF;
		break;
	case O_SOFT:
		info->modeflag |= 0xFF;
		break;
	}
}

static void
print_header(uint8_t flags){
        int have_flag = 0;

        while (flags) {
                unsigned int i;

                for (i = 0; (flags & chain_flags[i].flag) == 0; i++);

                if (have_flag)
                        printf(",");

                printf("%s", proto_to_name(chain_flags[i].proto,0));
                have_flag = 1;

                flags &= ~chain_flags[i].flag;
        }

        if (!have_flag)
                printf("NONE");
}

static void ipv6header_print(const void *ip,
                             const struct xt_entry_match *match, int numeric)
{
	const struct ip6t_ipv6header_info *info = (const struct ip6t_ipv6header_info *)match->data;
	printf(" ipv6header");

        if (info->matchflags || info->invflags) {
		printf(" flags:%s", info->invflags ? "!" : "");
                if (numeric)
			printf("0x%02X", info->matchflags);
                else {
                        print_header(info->matchflags);
                }
        }

	if (info->modeflag)
		printf(" soft");
}

static void ipv6header_save(const void *ip, const struct xt_entry_match *match)
{

	const struct ip6t_ipv6header_info *info = (const struct ip6t_ipv6header_info *)match->data;

	printf("%s --header ", info->invflags ? " !" : "");
	print_header(info->matchflags);
	if (info->modeflag)
		printf(" --soft");
}

static struct xtables_match ipv6header_mt6_reg = {
	.name		= "ipv6header",
	.version	= XTABLES_VERSION,
	.family		= NFPROTO_IPV6,
	.size		= XT_ALIGN(sizeof(struct ip6t_ipv6header_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct ip6t_ipv6header_info)),
	.help		= ipv6header_help,
	.print		= ipv6header_print,
	.save		= ipv6header_save,
	.x6_parse	= ipv6header_parse,
	.x6_options	= ipv6header_opts,
};

void _init(void)
{
	xtables_register_match(&ipv6header_mt6_reg);
}
