#include <stdio.h>
#include <string.h>
#include <xtables.h>
#include <linux/netfilter/nf_conntrack_common.h>
#include <linux/netfilter/xt_state.h>

#ifndef XT_STATE_UNTRACKED
#define XT_STATE_UNTRACKED (1 << (IP_CT_NUMBER + 1))
#endif

enum {
	O_STATE = 0,
};

static void
state_help(void)
{
	printf(
"state match options:\n"
" [!] --state [INVALID|ESTABLISHED|NEW|RELATED|UNTRACKED][,...]\n"
"				State(s) to match\n");
}

static const struct xt_option_entry state_opts[] = {
	{.name = "state", .id = O_STATE, .type = XTTYPE_STRING,
	 .flags = XTOPT_MAND},
	XTOPT_TABLEEND,
};

static int
state_parse_state(const char *state, size_t len, struct xt_state_info *sinfo)
{
	if (strncasecmp(state, "INVALID", len) == 0)
		sinfo->statemask |= XT_STATE_INVALID;
	else if (strncasecmp(state, "NEW", len) == 0)
		sinfo->statemask |= XT_STATE_BIT(IP_CT_NEW);
	else if (strncasecmp(state, "ESTABLISHED", len) == 0)
		sinfo->statemask |= XT_STATE_BIT(IP_CT_ESTABLISHED);
	else if (strncasecmp(state, "RELATED", len) == 0)
		sinfo->statemask |= XT_STATE_BIT(IP_CT_RELATED);
	else if (strncasecmp(state, "UNTRACKED", len) == 0)
		sinfo->statemask |= XT_STATE_UNTRACKED;
	else
		return 0;
	return 1;
}

static void
state_parse_states(const char *arg, struct xt_state_info *sinfo)
{
	const char *comma;

	while ((comma = strchr(arg, ',')) != NULL) {
		if (comma == arg || !state_parse_state(arg, comma-arg, sinfo))
			xtables_error(PARAMETER_PROBLEM, "Bad state \"%s\"", arg);
		arg = comma+1;
	}
	if (!*arg)
		xtables_error(PARAMETER_PROBLEM, "\"--state\" requires a list of "
					      "states with no spaces, e.g. "
					      "ESTABLISHED,RELATED");
	if (strlen(arg) == 0 || !state_parse_state(arg, strlen(arg), sinfo))
		xtables_error(PARAMETER_PROBLEM, "Bad state \"%s\"", arg);
}

static void state_parse(struct xt_option_call *cb)
{
	struct xt_state_info *sinfo = cb->data;

	xtables_option_parse(cb);
	state_parse_states(cb->arg, sinfo);
	if (cb->invert)
		sinfo->statemask = ~sinfo->statemask;
}

static void state_print_state(unsigned int statemask)
{
	const char *sep = "";

	if (statemask & XT_STATE_INVALID) {
		printf("%sINVALID", sep);
		sep = ",";
	}
	if (statemask & XT_STATE_BIT(IP_CT_NEW)) {
		printf("%sNEW", sep);
		sep = ",";
	}
	if (statemask & XT_STATE_BIT(IP_CT_RELATED)) {
		printf("%sRELATED", sep);
		sep = ",";
	}
	if (statemask & XT_STATE_BIT(IP_CT_ESTABLISHED)) {
		printf("%sESTABLISHED", sep);
		sep = ",";
	}
	if (statemask & XT_STATE_UNTRACKED) {
		printf("%sUNTRACKED", sep);
		sep = ",";
	}
}

static void
state_print(const void *ip,
      const struct xt_entry_match *match,
      int numeric)
{
	const struct xt_state_info *sinfo = (const void *)match->data;

	printf(" state ");
	state_print_state(sinfo->statemask);
}

static void state_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_state_info *sinfo = (const void *)match->data;

	printf(" --state ");
	state_print_state(sinfo->statemask);
}

static struct xtables_match state_match = { 
	.family		= NFPROTO_UNSPEC,
	.name		= "state",
	.version	= XTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_state_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_state_info)),
	.help		= state_help,
	.print		= state_print,
	.save		= state_save,
	.x6_parse	= state_parse,
	.x6_options	= state_opts,
};

void _init(void)
{
	xtables_register_match(&state_match);
}
