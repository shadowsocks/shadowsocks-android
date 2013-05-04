#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <xtables.h>
#include <linux/netfilter_ipv6/ip6t_opts.h>

enum {
	O_DSTLEN = 0,
	O_DSTOPTS,
};

static void dst_help(void)
{
	printf(
"dst match options:\n"
"[!] --dst-len length            total length of this header\n"
"  --dst-opts TYPE[:LEN][,TYPE[:LEN]...]\n"
"                                Options and its length (list, max: %d)\n",
IP6T_OPTS_OPTSNR);
}

static const struct xt_option_entry dst_opts[] = {
	{.name = "dst-len", .id = O_DSTLEN, .type = XTTYPE_UINT32,
	 .flags = XTOPT_INVERT | XTOPT_PUT,
	 XTOPT_POINTER(struct ip6t_opts, hdrlen)},
	{.name = "dst-opts", .id = O_DSTOPTS, .type = XTTYPE_STRING},
	XTOPT_TABLEEND,
};

static uint32_t
parse_opts_num(const char *idstr, const char *typestr)
{
	unsigned long int id;
	char* ep;

	id = strtoul(idstr, &ep, 0);

	if ( idstr == ep ) {
		xtables_error(PARAMETER_PROBLEM,
		           "dst: no valid digits in %s `%s'", typestr, idstr);
	}
	if ( id == ULONG_MAX  && errno == ERANGE ) {
		xtables_error(PARAMETER_PROBLEM,
			   "%s `%s' specified too big: would overflow",
			   typestr, idstr);
	}
	if ( *idstr != '\0'  && *ep != '\0' ) {
		xtables_error(PARAMETER_PROBLEM,
		           "dst: error parsing %s `%s'", typestr, idstr);
	}
	return id;
}

static int
parse_options(const char *optsstr, uint16_t *opts)
{
        char *buffer, *cp, *next, *range;
        unsigned int i;
	
	buffer = strdup(optsstr);
        if (!buffer)
		xtables_error(OTHER_PROBLEM, "strdup failed");
			
        for (cp = buffer, i = 0; cp && i < IP6T_OPTS_OPTSNR; cp = next, i++)
        {
                next = strchr(cp, ',');

                if (next)
			*next++='\0';

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
				xtables_error(PARAMETER_PROBLEM,
					"PAD0 hasn't got length");
			opts[i] |= parse_opts_num(range, "length") & 0xFF;
                } else
                        opts[i] |= (0x00FF);

#ifdef DEBUG
		printf("opts str: %s %s\n", cp, range);
		printf("opts opt: %04X\n", opts[i]);
#endif
	}

        if (cp)
		xtables_error(PARAMETER_PROBLEM, "too many addresses specified");

	free(buffer);

#ifdef DEBUG
	printf("addr nr: %d\n", i);
#endif

	return i;
}

static void dst_parse(struct xt_option_call *cb)
{
	struct ip6t_opts *optinfo = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_DSTOPTS:
		optinfo->optsnr = parse_options(cb->arg, optinfo->opts);
		optinfo->flags |= IP6T_OPTS_OPTS;
		break;
	}
}

static void
print_options(unsigned int optsnr, uint16_t *optsp)
{
	unsigned int i;

	printf(" ");
	for(i = 0; i < optsnr; i++) {
		printf("%d", (optsp[i] & 0xFF00) >> 8);

		if ((optsp[i] & 0x00FF) != 0x00FF)
			printf(":%d", (optsp[i] & 0x00FF));

		printf("%c", (i != optsnr - 1) ? ',' : ' ');
	}
}

static void dst_print(const void *ip, const struct xt_entry_match *match,
                      int numeric)
{
	const struct ip6t_opts *optinfo = (struct ip6t_opts *)match->data;

	printf(" dst");
	if (optinfo->flags & IP6T_OPTS_LEN)
		printf(" length:%s%u",
			optinfo->invflags & IP6T_OPTS_INV_LEN ? "!" : "",
			optinfo->hdrlen);

	if (optinfo->flags & IP6T_OPTS_OPTS)
		printf(" opts");

	print_options(optinfo->optsnr, (uint16_t *)optinfo->opts);

	if (optinfo->invflags & ~IP6T_OPTS_INV_MASK)
		printf(" Unknown invflags: 0x%X",
		       optinfo->invflags & ~IP6T_OPTS_INV_MASK);
}

static void dst_save(const void *ip, const struct xt_entry_match *match)
{
	const struct ip6t_opts *optinfo = (struct ip6t_opts *)match->data;

	if (optinfo->flags & IP6T_OPTS_LEN) {
		printf("%s --dst-len %u",
			(optinfo->invflags & IP6T_OPTS_INV_LEN) ? " !" : "",
			optinfo->hdrlen);
	}

	if (optinfo->flags & IP6T_OPTS_OPTS)
		printf(" --dst-opts");

	print_options(optinfo->optsnr, (uint16_t *)optinfo->opts);
}

static struct xtables_match dst_mt6_reg = {
	.name          = "dst",
	.version       = XTABLES_VERSION,
	.family        = NFPROTO_IPV6,
	.size          = XT_ALIGN(sizeof(struct ip6t_opts)),
	.userspacesize = XT_ALIGN(sizeof(struct ip6t_opts)),
	.help          = dst_help,
	.print         = dst_print,
	.save          = dst_save,
	.x6_parse      = dst_parse,
	.x6_options    = dst_opts,
};

void
_init(void)
{
	xtables_register_match(&dst_mt6_reg);
}
