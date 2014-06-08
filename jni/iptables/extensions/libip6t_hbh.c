#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <xtables.h>
#include <linux/netfilter_ipv6/ip6t_opts.h>

#define DEBUG		0

enum {
	O_HBH_LEN = 0,
	O_HBH_OPTS,
};

static void hbh_help(void)
{
	printf(
"hbh match options:\n"
"[!] --hbh-len length            total length of this header\n"
"  --hbh-opts TYPE[:LEN][,TYPE[:LEN]...] \n"
"                                Options and its length (list, max: %d)\n",
IP6T_OPTS_OPTSNR);
}

static const struct xt_option_entry hbh_opts[] = {
	{.name = "hbh-len", .id = O_HBH_LEN, .type = XTTYPE_UINT32,
	 .flags = XTOPT_INVERT | XTOPT_PUT,
	 XTOPT_POINTER(struct ip6t_opts, hdrlen)},
	{.name = "hbh-opts", .id = O_HBH_OPTS, .type = XTTYPE_STRING},
	XTOPT_TABLEEND,
};

static uint32_t
parse_opts_num(const char *idstr, const char *typestr)
{
	unsigned long int id;
	char* ep;

	id =  strtoul(idstr,&ep,0) ;

	if ( idstr == ep ) {
		xtables_error(PARAMETER_PROBLEM,
			   "hbh: no valid digits in %s `%s'", typestr, idstr);
	}
	if ( id == ULONG_MAX  && errno == ERANGE ) {
		xtables_error(PARAMETER_PROBLEM,
			   "%s `%s' specified too big: would overflow",
			   typestr, idstr);
	}	
	if ( *idstr != '\0'  && *ep != '\0' ) {
		xtables_error(PARAMETER_PROBLEM,
			   "hbh: error parsing %s `%s'", typestr, idstr);
	}
	return id;
}

static int
parse_options(const char *optsstr, uint16_t *opts)
{
        char *buffer, *cp, *next, *range;
        unsigned int i;
	
	buffer = strdup(optsstr);
	if (!buffer) xtables_error(OTHER_PROBLEM, "strdup failed");
			
        for (cp=buffer, i=0; cp && i<IP6T_OPTS_OPTSNR; cp=next,i++)
        {
                next=strchr(cp, ',');
                if (next) *next++='\0';
                range = strchr(cp, ':');
                if (range) {
                        if (i == IP6T_OPTS_OPTSNR-1)
				xtables_error(PARAMETER_PROBLEM,
                                           "too many ports specified");
                        *range++ = '\0';
                }
		opts[i] = (parse_opts_num(cp, "opt") & 0xFF) << 8;
                if (range) {
			if (opts[i] == 0)
				xtables_error(PARAMETER_PROBLEM, "PAD0 has not got length");
			opts[i] |= parse_opts_num(range, "length") & 0xFF;
                } else {
                        opts[i] |= (0x00FF);
		}

#if DEBUG
		printf("opts str: %s %s\n", cp, range);
		printf("opts opt: %04X\n", opts[i]);
#endif
	}
	if (cp) xtables_error(PARAMETER_PROBLEM, "too many addresses specified");

	free(buffer);

#if DEBUG
	printf("addr nr: %d\n", i);
#endif

	return i;
}

static void hbh_parse(struct xt_option_call *cb)
{
	struct ip6t_opts *optinfo = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_HBH_LEN:
		if (cb->invert)
			optinfo->invflags |= IP6T_OPTS_INV_LEN;
		break;
	case O_HBH_OPTS:
		optinfo->optsnr = parse_options(cb->arg, optinfo->opts);
		optinfo->flags |= IP6T_OPTS_OPTS;
		break;
	}
}

static void
print_options(unsigned int optsnr, uint16_t *optsp)
{
	unsigned int i;

	for(i=0; i<optsnr; i++){
		printf("%c", (i==0)?' ':',');
		printf("%d", (optsp[i] & 0xFF00)>>8);
		if ((optsp[i] & 0x00FF) != 0x00FF){
			printf(":%d", (optsp[i] & 0x00FF));
		} 
	}
}

static void hbh_print(const void *ip, const struct xt_entry_match *match,
                      int numeric)
{
	const struct ip6t_opts *optinfo = (struct ip6t_opts *)match->data;

	printf(" hbh");
	if (optinfo->flags & IP6T_OPTS_LEN) {
		printf(" length");
		printf(":%s", optinfo->invflags & IP6T_OPTS_INV_LEN ? "!" : "");
		printf("%u", optinfo->hdrlen);
	}
	if (optinfo->flags & IP6T_OPTS_OPTS) printf(" opts");
	print_options(optinfo->optsnr, (uint16_t *)optinfo->opts);
	if (optinfo->invflags & ~IP6T_OPTS_INV_MASK)
		printf(" Unknown invflags: 0x%X",
		       optinfo->invflags & ~IP6T_OPTS_INV_MASK);
}

static void hbh_save(const void *ip, const struct xt_entry_match *match)
{
	const struct ip6t_opts *optinfo = (struct ip6t_opts *)match->data;

	if (optinfo->flags & IP6T_OPTS_LEN) {
		printf("%s --hbh-len %u",
			(optinfo->invflags & IP6T_OPTS_INV_LEN) ? " !" : "",
			optinfo->hdrlen);
	}

	if (optinfo->flags & IP6T_OPTS_OPTS)
		printf(" --hbh-opts");
	print_options(optinfo->optsnr, (uint16_t *)optinfo->opts);
}

static struct xtables_match hbh_mt6_reg = {
	.name 		= "hbh",
	.version	= XTABLES_VERSION,
	.family		= NFPROTO_IPV6,
	.size		= XT_ALIGN(sizeof(struct ip6t_opts)),
	.userspacesize	= XT_ALIGN(sizeof(struct ip6t_opts)),
	.help		= hbh_help,
	.print		= hbh_print,
	.save		= hbh_save,
	.x6_parse	= hbh_parse,
	.x6_options	= hbh_opts,
};

void
_init(void)
{
	xtables_register_match(&hbh_mt6_reg);
}
