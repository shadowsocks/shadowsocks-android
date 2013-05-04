#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <xtables.h>
#include <linux/netfilter_ipv6/ip6t_rt.h>
#include <arpa/inet.h>

enum {
	O_RT_TYPE = 0,
	O_RT_SEGSLEFT,
	O_RT_LEN,
	O_RT0RES,
	O_RT0ADDRS,
	O_RT0NSTRICT,
	F_RT_TYPE  = 1 << O_RT_TYPE,
	F_RT0ADDRS = 1 << O_RT0ADDRS,
};

static void rt_help(void)
{
	printf(
"rt match options:\n"
"[!] --rt-type type             match the type\n"
"[!] --rt-segsleft num[:num]    match the Segments Left field (range)\n"
"[!] --rt-len length            total length of this header\n"
" --rt-0-res                    check the reserved field too (type 0)\n"
" --rt-0-addrs ADDR[,ADDR...]   Type=0 addresses (list, max: %d)\n"
" --rt-0-not-strict             List of Type=0 addresses not a strict list\n",
IP6T_RT_HOPS);
}

#define s struct ip6t_rt
static const struct xt_option_entry rt_opts[] = {
	{.name = "rt-type", .id = O_RT_TYPE, .type = XTTYPE_UINT32,
	 .flags = XTOPT_INVERT | XTOPT_PUT, XTOPT_POINTER(s, rt_type)},
	{.name = "rt-segsleft", .id = O_RT_SEGSLEFT, .type = XTTYPE_UINT32RC,
	 .flags = XTOPT_INVERT | XTOPT_PUT, XTOPT_POINTER(s, segsleft)},
	{.name = "rt-len", .id = O_RT_LEN, .type = XTTYPE_UINT32,
	 .flags = XTOPT_INVERT | XTOPT_PUT, XTOPT_POINTER(s, hdrlen)},
	{.name = "rt-0-res", .id = O_RT0RES, .type = XTTYPE_NONE},
	{.name = "rt-0-addrs", .id = O_RT0ADDRS, .type = XTTYPE_STRING},
	{.name = "rt-0-not-strict", .id = O_RT0NSTRICT, .type = XTTYPE_NONE},
	XTOPT_TABLEEND,
};
#undef s

static const char *
addr_to_numeric(const struct in6_addr *addrp)
{
	static char buf[50+1];
	return inet_ntop(AF_INET6, addrp, buf, sizeof(buf));
}

static struct in6_addr *
numeric_to_addr(const char *num)
{
	static struct in6_addr ap;
	int err;

	if ((err=inet_pton(AF_INET6, num, &ap)) == 1)
		return &ap;
#ifdef DEBUG
	fprintf(stderr, "\nnumeric2addr: %d\n", err);
#endif
	xtables_error(PARAMETER_PROBLEM, "bad address: %s", num);

	return (struct in6_addr *)NULL;
}


static int
parse_addresses(const char *addrstr, struct in6_addr *addrp)
{
        char *buffer, *cp, *next;
        unsigned int i;
	
	buffer = strdup(addrstr);
	if (!buffer) xtables_error(OTHER_PROBLEM, "strdup failed");
			
        for (cp=buffer, i=0; cp && i<IP6T_RT_HOPS; cp=next,i++)
        {
                next=strchr(cp, ',');
                if (next) *next++='\0';
		memcpy(&(addrp[i]), numeric_to_addr(cp), sizeof(struct in6_addr));
#if DEBUG
		printf("addr str: %s\n", cp);
		printf("addr ip6: %s\n", addr_to_numeric((numeric_to_addr(cp))));
		printf("addr [%d]: %s\n", i, addr_to_numeric(&(addrp[i])));
#endif
	}
	if (cp) xtables_error(PARAMETER_PROBLEM, "too many addresses specified");

	free(buffer);

#if DEBUG
	printf("addr nr: %d\n", i);
#endif

	return i;
}

static void rt_parse(struct xt_option_call *cb)
{
	struct ip6t_rt *rtinfo = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_RT_TYPE:
		if (cb->invert)
			rtinfo->invflags |= IP6T_RT_INV_TYP;
		rtinfo->flags |= IP6T_RT_TYP;
		break;
	case O_RT_SEGSLEFT:
		if (cb->nvals == 1)
			rtinfo->segsleft[1] = rtinfo->segsleft[0];
		if (cb->invert)
			rtinfo->invflags |= IP6T_RT_INV_SGS;
		rtinfo->flags |= IP6T_RT_SGS;
		break;
	case O_RT_LEN:
		if (cb->invert)
			rtinfo->invflags |= IP6T_RT_INV_LEN;
		rtinfo->flags |= IP6T_RT_LEN;
		break;
	case O_RT0RES:
		if (!(cb->xflags & F_RT_TYPE) || rtinfo->rt_type != 0 ||
		    rtinfo->invflags & IP6T_RT_INV_TYP)
			xtables_error(PARAMETER_PROBLEM,
				   "`--rt-type 0' required before `--rt-0-res'");
		rtinfo->flags |= IP6T_RT_RES;
		break;
	case O_RT0ADDRS:
		if (!(cb->xflags & F_RT_TYPE) || rtinfo->rt_type != 0 ||
		    rtinfo->invflags & IP6T_RT_INV_TYP)
			xtables_error(PARAMETER_PROBLEM,
				   "`--rt-type 0' required before `--rt-0-addrs'");
		rtinfo->addrnr = parse_addresses(cb->arg, rtinfo->addrs);
		rtinfo->flags |= IP6T_RT_FST;
		break;
	case O_RT0NSTRICT:
		if (!(cb->xflags & F_RT0ADDRS))
			xtables_error(PARAMETER_PROBLEM,
				   "`--rt-0-addr ...' required before `--rt-0-not-strict'");
		rtinfo->flags |= IP6T_RT_FST_NSTRICT;
		break;
	}
}

static void
print_nums(const char *name, uint32_t min, uint32_t max,
	    int invert)
{
	const char *inv = invert ? "!" : "";

	if (min != 0 || max != 0xFFFFFFFF || invert) {
		printf(" %s", name);
		if (min == max) {
			printf(":%s", inv);
			printf("%u", min);
		} else {
			printf("s:%s", inv);
			printf("%u",min);
			printf(":");
			printf("%u",max);
		}
	}
}

static void
print_addresses(unsigned int addrnr, struct in6_addr *addrp)
{
	unsigned int i;

	for(i=0; i<addrnr; i++){
		printf("%c%s", (i==0)?' ':',', addr_to_numeric(&(addrp[i])));
	}
}

static void rt_print(const void *ip, const struct xt_entry_match *match,
                     int numeric)
{
	const struct ip6t_rt *rtinfo = (struct ip6t_rt *)match->data;

	printf(" rt");
	if (rtinfo->flags & IP6T_RT_TYP)
	    printf(" type:%s%d", rtinfo->invflags & IP6T_RT_INV_TYP ? "!" : "",
		    rtinfo->rt_type);
	print_nums("segsleft", rtinfo->segsleft[0], rtinfo->segsleft[1],
		    rtinfo->invflags & IP6T_RT_INV_SGS);
	if (rtinfo->flags & IP6T_RT_LEN) {
		printf(" length");
		printf(":%s", rtinfo->invflags & IP6T_RT_INV_LEN ? "!" : "");
		printf("%u", rtinfo->hdrlen);
	}
	if (rtinfo->flags & IP6T_RT_RES) printf(" reserved");
	if (rtinfo->flags & IP6T_RT_FST) printf(" 0-addrs");
	print_addresses(rtinfo->addrnr, (struct in6_addr *)rtinfo->addrs);
	if (rtinfo->flags & IP6T_RT_FST_NSTRICT) printf(" 0-not-strict");
	if (rtinfo->invflags & ~IP6T_RT_INV_MASK)
		printf(" Unknown invflags: 0x%X",
		       rtinfo->invflags & ~IP6T_RT_INV_MASK);
}

static void rt_save(const void *ip, const struct xt_entry_match *match)
{
	const struct ip6t_rt *rtinfo = (struct ip6t_rt *)match->data;

	if (rtinfo->flags & IP6T_RT_TYP) {
		printf("%s --rt-type %u",
			(rtinfo->invflags & IP6T_RT_INV_TYP) ? " !" : "",
			rtinfo->rt_type);
	}

	if (!(rtinfo->segsleft[0] == 0
	    && rtinfo->segsleft[1] == 0xFFFFFFFF)) {
		printf("%s --rt-segsleft ",
			(rtinfo->invflags & IP6T_RT_INV_SGS) ? " !" : "");
		if (rtinfo->segsleft[0]
		    != rtinfo->segsleft[1])
			printf("%u:%u",
			       rtinfo->segsleft[0],
			       rtinfo->segsleft[1]);
		else
			printf("%u",
			       rtinfo->segsleft[0]);
	}

	if (rtinfo->flags & IP6T_RT_LEN) {
		printf("%s --rt-len %u",
			(rtinfo->invflags & IP6T_RT_INV_LEN) ? " !" : "", 
			rtinfo->hdrlen);
	}

	if (rtinfo->flags & IP6T_RT_RES) printf(" --rt-0-res");
	if (rtinfo->flags & IP6T_RT_FST) printf(" --rt-0-addrs");
	print_addresses(rtinfo->addrnr, (struct in6_addr *)rtinfo->addrs);
	if (rtinfo->flags & IP6T_RT_FST_NSTRICT) printf(" --rt-0-not-strict");

}

static struct xtables_match rt_mt6_reg = {
	.name		= "rt",
	.version	= XTABLES_VERSION,
	.family		= NFPROTO_IPV6,
	.size		= XT_ALIGN(sizeof(struct ip6t_rt)),
	.userspacesize	= XT_ALIGN(sizeof(struct ip6t_rt)),
	.help		= rt_help,
	.x6_parse	= rt_parse,
	.print		= rt_print,
	.save		= rt_save,
	.x6_options	= rt_opts,
};

void
_init(void)
{
	xtables_register_match(&rt_mt6_reg);
}
