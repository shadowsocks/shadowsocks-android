#include <stdio.h>
#include <string.h>
#include <xtables.h>
#include <linux/netfilter/xt_connbytes.h>

enum {
	O_CONNBYTES = 0,
	O_CONNBYTES_DIR,
	O_CONNBYTES_MODE,
};

static void connbytes_help(void)
{
	printf(
"connbytes match options:\n"
" [!] --connbytes from:[to]\n"
"     --connbytes-dir [original, reply, both]\n"
"     --connbytes-mode [packets, bytes, avgpkt]\n");
}

static const struct xt_option_entry connbytes_opts[] = {
	{.name = "connbytes", .id = O_CONNBYTES, .type = XTTYPE_UINT64RC,
	 .flags = XTOPT_MAND | XTOPT_INVERT},
	{.name = "connbytes-dir", .id = O_CONNBYTES_DIR, .type = XTTYPE_STRING,
	 .flags = XTOPT_MAND},
	{.name = "connbytes-mode", .id = O_CONNBYTES_MODE,
	 .type = XTTYPE_STRING, .flags = XTOPT_MAND},
	XTOPT_TABLEEND,
};

static void connbytes_parse(struct xt_option_call *cb)
{
	struct xt_connbytes_info *sinfo = cb->data;
	unsigned long long i;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_CONNBYTES:
		sinfo->count.from = cb->val.u64_range[0];
		sinfo->count.to   = cb->val.u64_range[0];
		if (cb->nvals == 2)
			sinfo->count.to = cb->val.u64_range[1];
		if (cb->invert) {
			i = sinfo->count.from;
			sinfo->count.from = sinfo->count.to;
			sinfo->count.to = i;
		}
		break;
	case O_CONNBYTES_DIR:
		if (strcmp(cb->arg, "original") == 0)
			sinfo->direction = XT_CONNBYTES_DIR_ORIGINAL;
		else if (strcmp(cb->arg, "reply") == 0)
			sinfo->direction = XT_CONNBYTES_DIR_REPLY;
		else if (strcmp(cb->arg, "both") == 0)
			sinfo->direction = XT_CONNBYTES_DIR_BOTH;
		else
			xtables_error(PARAMETER_PROBLEM,
				   "Unknown --connbytes-dir `%s'", cb->arg);
		break;
	case O_CONNBYTES_MODE:
		if (strcmp(cb->arg, "packets") == 0)
			sinfo->what = XT_CONNBYTES_PKTS;
		else if (strcmp(cb->arg, "bytes") == 0)
			sinfo->what = XT_CONNBYTES_BYTES;
		else if (strcmp(cb->arg, "avgpkt") == 0)
			sinfo->what = XT_CONNBYTES_AVGPKT;
		else
			xtables_error(PARAMETER_PROBLEM,
				   "Unknown --connbytes-mode `%s'", cb->arg);
		break;
	}
}

static void print_mode(const struct xt_connbytes_info *sinfo)
{
	switch (sinfo->what) {
		case XT_CONNBYTES_PKTS:
			fputs(" packets", stdout);
			break;
		case XT_CONNBYTES_BYTES:
			fputs(" bytes", stdout);
			break;
		case XT_CONNBYTES_AVGPKT:
			fputs(" avgpkt", stdout);
			break;
		default:
			fputs(" unknown", stdout);
			break;
	}
}

static void print_direction(const struct xt_connbytes_info *sinfo)
{
	switch (sinfo->direction) {
		case XT_CONNBYTES_DIR_ORIGINAL:
			fputs(" original", stdout);
			break;
		case XT_CONNBYTES_DIR_REPLY:
			fputs(" reply", stdout);
			break;
		case XT_CONNBYTES_DIR_BOTH:
			fputs(" both", stdout);
			break;
		default:
			fputs(" unknown", stdout);
			break;
	}
}

static void
connbytes_print(const void *ip, const struct xt_entry_match *match, int numeric)
{
	const struct xt_connbytes_info *sinfo = (const void *)match->data;

	if (sinfo->count.from > sinfo->count.to) 
		printf(" connbytes ! %llu:%llu",
			(unsigned long long)sinfo->count.to,
			(unsigned long long)sinfo->count.from);
	else
		printf(" connbytes %llu:%llu",
			(unsigned long long)sinfo->count.from,
			(unsigned long long)sinfo->count.to);

	fputs(" connbytes mode", stdout);
	print_mode(sinfo);

	fputs(" connbytes direction", stdout);
	print_direction(sinfo);
}

static void connbytes_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_connbytes_info *sinfo = (const void *)match->data;

	if (sinfo->count.from > sinfo->count.to) 
		printf(" ! --connbytes %llu:%llu",
			(unsigned long long)sinfo->count.to,
			(unsigned long long)sinfo->count.from);
	else
		printf(" --connbytes %llu:%llu",
			(unsigned long long)sinfo->count.from,
			(unsigned long long)sinfo->count.to);

	fputs(" --connbytes-mode", stdout);
	print_mode(sinfo);

	fputs(" --connbytes-dir", stdout);
	print_direction(sinfo);
}

static struct xtables_match connbytes_match = {
	.family		= NFPROTO_UNSPEC,
	.name 		= "connbytes",
	.version 	= XTABLES_VERSION,
	.size 		= XT_ALIGN(sizeof(struct xt_connbytes_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_connbytes_info)),
	.help		= connbytes_help,
	.print		= connbytes_print,
	.save 		= connbytes_save,
	.x6_parse	= connbytes_parse,
	.x6_options	= connbytes_opts,
};

void _init(void)
{
	xtables_register_match(&connbytes_match);
}
