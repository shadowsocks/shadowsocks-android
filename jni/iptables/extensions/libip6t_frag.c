#include <stdio.h>
#include <xtables.h>
#include <linux/netfilter_ipv6/ip6t_frag.h>

enum {
	O_FRAGID = 0,
	O_FRAGLEN,
	O_FRAGRES,
	O_FRAGFIRST,
	O_FRAGMORE,
	O_FRAGLAST,
	F_FRAGMORE = 1 << O_FRAGMORE,
	F_FRAGLAST = 1 << O_FRAGLAST,
};

static void frag_help(void)
{
	printf(
"frag match options:\n"
"[!] --fragid id[:id]           match the id (range)\n"
"[!] --fraglen length           total length of this header\n"
" --fragres                     check the reserved field too\n"
" --fragfirst                   matches on the first fragment\n"
" [--fragmore|--fraglast]       there are more fragments or this\n"
"                               is the last one\n");
}

#define s struct ip6t_frag
static const struct xt_option_entry frag_opts[] = {
	{.name = "fragid", .id = O_FRAGID, .type = XTTYPE_UINT32RC,
	 .flags = XTOPT_INVERT | XTOPT_PUT, XTOPT_POINTER(s, ids)},
	{.name = "fraglen", .id = O_FRAGLEN, .type = XTTYPE_UINT32,
	 .flags = XTOPT_INVERT | XTOPT_PUT, XTOPT_POINTER(s, hdrlen)},
	{.name = "fragres", .id = O_FRAGRES, .type = XTTYPE_NONE},
	{.name = "fragfirst", .id = O_FRAGFIRST, .type = XTTYPE_NONE},
	{.name = "fragmore", .id = O_FRAGMORE, .type = XTTYPE_NONE,
	 .excl = F_FRAGLAST},
	{.name = "fraglast", .id = O_FRAGLAST, .type = XTTYPE_NONE,
	 .excl = F_FRAGMORE},
	XTOPT_TABLEEND,
};
#undef s

static void frag_parse(struct xt_option_call *cb)
{
	struct ip6t_frag *fraginfo = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_FRAGID:
		if (cb->nvals == 1)
			fraginfo->ids[1] = fraginfo->ids[0];
		break;
	case O_FRAGRES:
		fraginfo->flags |= IP6T_FRAG_RES;
		break;
	case O_FRAGFIRST:
		fraginfo->flags |= IP6T_FRAG_FST;
		break;
	case O_FRAGMORE:
		fraginfo->flags |= IP6T_FRAG_MF;
		break;
	case O_FRAGLAST:
		fraginfo->flags |= IP6T_FRAG_NMF;
		break;
	}
}

static void
print_ids(const char *name, uint32_t min, uint32_t max,
	    int invert)
{
	const char *inv = invert ? "!" : "";

	if (min != 0 || max != 0xFFFFFFFF || invert) {
		printf("%s", name);
		if (min == max)
			printf(":%s%u", inv, min);
		else
			printf("s:%s%u:%u", inv, min, max);
	}
}

static void frag_print(const void *ip, const struct xt_entry_match *match,
                       int numeric)
{
	const struct ip6t_frag *frag = (struct ip6t_frag *)match->data;

	printf(" frag ");
	print_ids("id", frag->ids[0], frag->ids[1],
		    frag->invflags & IP6T_FRAG_INV_IDS);

	if (frag->flags & IP6T_FRAG_LEN) {
		printf(" length:%s%u",
			frag->invflags & IP6T_FRAG_INV_LEN ? "!" : "",
			frag->hdrlen);
	}

	if (frag->flags & IP6T_FRAG_RES)
		printf(" reserved");

	if (frag->flags & IP6T_FRAG_FST)
		printf(" first");

	if (frag->flags & IP6T_FRAG_MF)
		printf(" more");

	if (frag->flags & IP6T_FRAG_NMF)
		printf(" last");

	if (frag->invflags & ~IP6T_FRAG_INV_MASK)
		printf(" Unknown invflags: 0x%X",
		       frag->invflags & ~IP6T_FRAG_INV_MASK);
}

static void frag_save(const void *ip, const struct xt_entry_match *match)
{
	const struct ip6t_frag *fraginfo = (struct ip6t_frag *)match->data;

	if (!(fraginfo->ids[0] == 0
	    && fraginfo->ids[1] == 0xFFFFFFFF)) {
		printf("%s --fragid ",
			(fraginfo->invflags & IP6T_FRAG_INV_IDS) ? " !" : "");
		if (fraginfo->ids[0]
		    != fraginfo->ids[1])
			printf("%u:%u",
			       fraginfo->ids[0],
			       fraginfo->ids[1]);
		else
			printf("%u",
			       fraginfo->ids[0]);
	}

	if (fraginfo->flags & IP6T_FRAG_LEN) {
		printf("%s --fraglen %u",
			(fraginfo->invflags & IP6T_FRAG_INV_LEN) ? " !" : "",
			fraginfo->hdrlen);
	}

	if (fraginfo->flags & IP6T_FRAG_RES)
		printf(" --fragres");

	if (fraginfo->flags & IP6T_FRAG_FST)
		printf(" --fragfirst");

	if (fraginfo->flags & IP6T_FRAG_MF)
		printf(" --fragmore");

	if (fraginfo->flags & IP6T_FRAG_NMF)
		printf(" --fraglast");
}

static struct xtables_match frag_mt6_reg = {
	.name          = "frag",
	.version       = XTABLES_VERSION,
	.family        = NFPROTO_IPV6,
	.size          = XT_ALIGN(sizeof(struct ip6t_frag)),
	.userspacesize = XT_ALIGN(sizeof(struct ip6t_frag)),
	.help          = frag_help,
	.print         = frag_print,
	.save          = frag_save,
	.x6_parse      = frag_parse,
	.x6_options    = frag_opts,
};

void
_init(void)
{
	xtables_register_match(&frag_mt6_reg);
}
