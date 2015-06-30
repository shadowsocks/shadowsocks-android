#include <stdio.h>
#include <string.h>
#include <xtables.h>
#include <linux/netfilter/nf_conntrack_common.h>
#include <linux/netfilter/xt_CT.h>

static void ct_help(void)
{
	printf(
"CT target options:\n"
" --notrack			Don't track connection\n"
" --helper name			Use conntrack helper 'name' for connection\n"
" --ctevents event[,event...]	Generate specified conntrack events for connection\n"
" --expevents event[,event...]	Generate specified expectation events for connection\n"
" --zone ID			Assign/Lookup connection in zone ID\n"
	);
}

enum {
	O_NOTRACK = 0,
	O_HELPER,
	O_CTEVENTS,
	O_EXPEVENTS,
	O_ZONE,
};

#define s struct xt_ct_target_info
static const struct xt_option_entry ct_opts[] = {
	{.name = "notrack", .id = O_NOTRACK, .type = XTTYPE_NONE},
	{.name = "helper", .id = O_HELPER, .type = XTTYPE_STRING,
	 .flags = XTOPT_PUT, XTOPT_POINTER(s, helper)},
	{.name = "ctevents", .id = O_CTEVENTS, .type = XTTYPE_STRING},
	{.name = "expevents", .id = O_EXPEVENTS, .type = XTTYPE_STRING},
	{.name = "zone", .id = O_ZONE, .type = XTTYPE_UINT16,
	 .flags = XTOPT_PUT, XTOPT_POINTER(s, zone)},
	XTOPT_TABLEEND,
};
#undef s

struct event_tbl {
	const char	*name;
	unsigned int	event;
};

static const struct event_tbl ct_event_tbl[] = {
	{ "new",		IPCT_NEW },
	{ "related",		IPCT_RELATED },
	{ "destroy",		IPCT_DESTROY },
	{ "reply",		IPCT_REPLY },
	{ "assured",		IPCT_ASSURED },
	{ "protoinfo",		IPCT_PROTOINFO },
	{ "helper",		IPCT_HELPER },
	{ "mark",		IPCT_MARK },
	{ "natseqinfo",		IPCT_NATSEQADJ },
	{ "secmark",		IPCT_SECMARK },
};

static const struct event_tbl exp_event_tbl[] = {
	{ "new",		IPEXP_NEW },
};

static uint32_t ct_parse_events(const struct event_tbl *tbl, unsigned int size,
				const char *events)
{
	char str[strlen(events) + 1], *e = str, *t;
	unsigned int mask = 0, i;

	strcpy(str, events);
	while ((t = strsep(&e, ","))) {
		for (i = 0; i < size; i++) {
			if (strcmp(t, tbl[i].name))
				continue;
			mask |= 1 << tbl[i].event;
			break;
		}

		if (i == size)
			xtables_error(PARAMETER_PROBLEM, "Unknown event type \"%s\"", t);
	}

	return mask;
}

static void ct_print_events(const char *pfx, const struct event_tbl *tbl,
			    unsigned int size, uint32_t mask)
{
	const char *sep = "";
	unsigned int i;

	printf(" %s ", pfx);
	for (i = 0; i < size; i++) {
		if (mask & (1 << tbl[i].event)) {
			printf("%s%s", sep, tbl[i].name);
			sep = ",";
		}
	}
}

static void ct_parse(struct xt_option_call *cb)
{
	struct xt_ct_target_info *info = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_NOTRACK:
		info->flags |= XT_CT_NOTRACK;
		break;
	case O_CTEVENTS:
		info->ct_events = ct_parse_events(ct_event_tbl, ARRAY_SIZE(ct_event_tbl), cb->arg);
		break;
	case O_EXPEVENTS:
		info->exp_events = ct_parse_events(exp_event_tbl, ARRAY_SIZE(exp_event_tbl), cb->arg);
		break;
	}
}

static void ct_print(const void *ip, const struct xt_entry_target *target, int numeric)
{
	const struct xt_ct_target_info *info =
		(const struct xt_ct_target_info *)target->data;

	printf(" CT");
	if (info->flags & XT_CT_NOTRACK)
		printf(" notrack");
	if (info->helper[0])
		printf(" helper %s", info->helper);
	if (info->ct_events)
		ct_print_events("ctevents", ct_event_tbl,
				ARRAY_SIZE(ct_event_tbl), info->ct_events);
	if (info->exp_events)
		ct_print_events("expevents", exp_event_tbl,
				ARRAY_SIZE(exp_event_tbl), info->exp_events);
	if (info->zone)
		printf("zone %u ", info->zone);
}

static void ct_save(const void *ip, const struct xt_entry_target *target)
{
	const struct xt_ct_target_info *info =
		(const struct xt_ct_target_info *)target->data;

	if (info->flags & XT_CT_NOTRACK)
		printf(" --notrack");
	if (info->helper[0])
		printf(" --helper %s", info->helper);
	if (info->ct_events)
		ct_print_events("--ctevents", ct_event_tbl,
				ARRAY_SIZE(ct_event_tbl), info->ct_events);
	if (info->exp_events)
		ct_print_events("--expevents", exp_event_tbl,
				ARRAY_SIZE(exp_event_tbl), info->exp_events);
	if (info->zone)
		printf(" --zone %u", info->zone);
}

static struct xtables_target ct_target = {
	.family		= NFPROTO_UNSPEC,
	.name		= "CT",
	.version	= XTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_ct_target_info)),
	.userspacesize	= offsetof(struct xt_ct_target_info, ct),
	.help		= ct_help,
	.print		= ct_print,
	.save		= ct_save,
	.x6_parse	= ct_parse,
	.x6_options	= ct_opts,
};

void _init(void)
{
	xtables_register_target(&ct_target);
}
