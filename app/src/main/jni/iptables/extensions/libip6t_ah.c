#include <stdio.h>
#include <xtables.h>
#include <linux/netfilter_ipv6/ip6t_ah.h>

enum {
	O_AHSPI = 0,
	O_AHLEN,
	O_AHRES,
};

static void ah_help(void)
{
	printf(
"ah match options:\n"
"[!] --ahspi spi[:spi]          match spi (range)\n"
"[!] --ahlen length             total length of this header\n"
" --ahres                       check the reserved field too\n");
}

#define s struct ip6t_ah
static const struct xt_option_entry ah_opts[] = {
	{.name = "ahspi", .id = O_AHSPI, .type = XTTYPE_UINT32RC,
	 .flags = XTOPT_INVERT | XTOPT_PUT, XTOPT_POINTER(s, spis)},
	{.name = "ahlen", .id = O_AHLEN, .type = XTTYPE_UINT32,
	 .flags = XTOPT_INVERT | XTOPT_PUT, XTOPT_POINTER(s, hdrlen)},
	{.name = "ahres", .id = O_AHRES, .type = XTTYPE_NONE},
	XTOPT_TABLEEND,
};
#undef s

static void ah_parse(struct xt_option_call *cb)
{
	struct ip6t_ah *ahinfo = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_AHSPI:
		if (cb->nvals == 1)
			ahinfo->spis[1] = ahinfo->spis[0];
		if (cb->invert)
			ahinfo->invflags |= IP6T_AH_INV_SPI;
		break;
	case O_AHLEN:
		if (cb->invert)
			ahinfo->invflags |= IP6T_AH_INV_LEN;
		break;
	case O_AHRES:
		ahinfo->hdrres = 1;
		break;
	}
}

static void
print_spis(const char *name, uint32_t min, uint32_t max,
	    int invert)
{
	const char *inv = invert ? "!" : "";

	if (min != 0 || max != 0xFFFFFFFF || invert) {
		if (min == max)
			printf("%s:%s%u", name, inv, min);
		else
			printf("%ss:%s%u:%u", name, inv, min, max);
	}
}

static void
print_len(const char *name, uint32_t len, int invert)
{
	const char *inv = invert ? "!" : "";

	if (len != 0 || invert)
		printf("%s:%s%u", name, inv, len);
}

static void ah_print(const void *ip, const struct xt_entry_match *match,
                     int numeric)
{
	const struct ip6t_ah *ah = (struct ip6t_ah *)match->data;

	printf(" ah ");
	print_spis("spi", ah->spis[0], ah->spis[1],
		    ah->invflags & IP6T_AH_INV_SPI);
	print_len("length", ah->hdrlen, 
		    ah->invflags & IP6T_AH_INV_LEN);

	if (ah->hdrres)
		printf(" reserved");

	if (ah->invflags & ~IP6T_AH_INV_MASK)
		printf(" Unknown invflags: 0x%X",
		       ah->invflags & ~IP6T_AH_INV_MASK);
}

static void ah_save(const void *ip, const struct xt_entry_match *match)
{
	const struct ip6t_ah *ahinfo = (struct ip6t_ah *)match->data;

	if (!(ahinfo->spis[0] == 0
	    && ahinfo->spis[1] == 0xFFFFFFFF)) {
		printf("%s --ahspi ",
			(ahinfo->invflags & IP6T_AH_INV_SPI) ? " !" : "");
		if (ahinfo->spis[0]
		    != ahinfo->spis[1])
			printf("%u:%u",
			       ahinfo->spis[0],
			       ahinfo->spis[1]);
		else
			printf("%u",
			       ahinfo->spis[0]);
	}

	if (ahinfo->hdrlen != 0 || (ahinfo->invflags & IP6T_AH_INV_LEN) ) {
		printf("%s --ahlen %u",
			(ahinfo->invflags & IP6T_AH_INV_LEN) ? " !" : "",
			ahinfo->hdrlen);
	}

	if (ahinfo->hdrres != 0 )
		printf(" --ahres");
}

static struct xtables_match ah_mt6_reg = {
	.name          = "ah",
	.version       = XTABLES_VERSION,
	.family        = NFPROTO_IPV6,
	.size          = XT_ALIGN(sizeof(struct ip6t_ah)),
	.userspacesize = XT_ALIGN(sizeof(struct ip6t_ah)),
	.help          = ah_help,
	.print         = ah_print,
	.save          = ah_save,
	.x6_parse      = ah_parse,
	.x6_options    = ah_opts,
};

void
_init(void)
{
	xtables_register_match(&ah_mt6_reg);
}
