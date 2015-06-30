/* Shared library add-on to iptables to add limit support.
 *
 * Jérôme de Vivie   <devivie@info.enserb.u-bordeaux.fr>
 * Hervé Eychenne    <rv@wallfire.org>
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <xtables.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_limit.h>

#define XT_LIMIT_AVG	"3/hour"
#define XT_LIMIT_BURST	5

enum {
	O_LIMIT = 0,
	O_BURST,
};

static void limit_help(void)
{
	printf(
"limit match options:\n"
"--limit avg			max average match rate: default "XT_LIMIT_AVG"\n"
"                                [Packets per second unless followed by \n"
"                                /sec /minute /hour /day postfixes]\n"
"--limit-burst number		number to match in a burst, default %u\n",
XT_LIMIT_BURST);
}

static const struct xt_option_entry limit_opts[] = {
	{.name = "limit", .id = O_LIMIT, .type = XTTYPE_STRING},
	{.name = "limit-burst", .id = O_BURST, .type = XTTYPE_UINT32,
	 .flags = XTOPT_PUT, XTOPT_POINTER(struct xt_rateinfo, burst),
	 .min = 0, .max = 10000},
	XTOPT_TABLEEND,
};

static
int parse_rate(const char *rate, uint32_t *val)
{
	const char *delim;
	uint32_t r;
	uint32_t mult = 1;  /* Seconds by default. */

	delim = strchr(rate, '/');
	if (delim) {
		if (strlen(delim+1) == 0)
			return 0;

		if (strncasecmp(delim+1, "second", strlen(delim+1)) == 0)
			mult = 1;
		else if (strncasecmp(delim+1, "minute", strlen(delim+1)) == 0)
			mult = 60;
		else if (strncasecmp(delim+1, "hour", strlen(delim+1)) == 0)
			mult = 60*60;
		else if (strncasecmp(delim+1, "day", strlen(delim+1)) == 0)
			mult = 24*60*60;
		else
			return 0;
	}
	r = atoi(rate);
	if (!r)
		return 0;

	/* This would get mapped to infinite (1/day is minimum they
           can specify, so we're ok at that end). */
	if (r / mult > XT_LIMIT_SCALE)
		xtables_error(PARAMETER_PROBLEM, "Rate too fast \"%s\"\n", rate);

	*val = XT_LIMIT_SCALE * mult / r;
	return 1;
}

static void limit_init(struct xt_entry_match *m)
{
	struct xt_rateinfo *r = (struct xt_rateinfo *)m->data;

	parse_rate(XT_LIMIT_AVG, &r->avg);
	r->burst = XT_LIMIT_BURST;

}

/* FIXME: handle overflow:
	if (r->avg*r->burst/r->burst != r->avg)
		xtables_error(PARAMETER_PROBLEM,
			   "Sorry: burst too large for that avg rate.\n");
*/

static void limit_parse(struct xt_option_call *cb)
{
	struct xt_rateinfo *r = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_LIMIT:
		if (!parse_rate(cb->arg, &r->avg))
			xtables_error(PARAMETER_PROBLEM,
				   "bad rate \"%s\"'", cb->arg);
		break;
	}
	if (cb->invert)
		xtables_error(PARAMETER_PROBLEM,
			   "limit does not support invert");
}

static const struct rates
{
	const char *name;
	uint32_t mult;
} rates[] = { { "day", XT_LIMIT_SCALE*24*60*60 },
	      { "hour", XT_LIMIT_SCALE*60*60 },
	      { "min", XT_LIMIT_SCALE*60 },
	      { "sec", XT_LIMIT_SCALE } };

static void print_rate(uint32_t period)
{
	unsigned int i;

	for (i = 1; i < ARRAY_SIZE(rates); ++i)
		if (period > rates[i].mult
            || rates[i].mult/period < rates[i].mult%period)
			break;

	printf(" %u/%s", rates[i-1].mult / period, rates[i-1].name);
}

static void
limit_print(const void *ip, const struct xt_entry_match *match, int numeric)
{
	const struct xt_rateinfo *r = (const void *)match->data;
	printf(" limit: avg"); print_rate(r->avg);
	printf(" burst %u", r->burst);
}

static void limit_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_rateinfo *r = (const void *)match->data;

	printf(" --limit"); print_rate(r->avg);
	if (r->burst != XT_LIMIT_BURST)
		printf(" --limit-burst %u", r->burst);
}

static struct xtables_match limit_match = {
	.family		= NFPROTO_UNSPEC,
	.name		= "limit",
	.version	= XTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_rateinfo)),
	.userspacesize	= offsetof(struct xt_rateinfo, prev),
	.help		= limit_help,
	.init		= limit_init,
	.x6_parse	= limit_parse,
	.print		= limit_print,
	.save		= limit_save,
	.x6_options	= limit_opts,
};

void _init(void)
{
	xtables_register_match(&limit_match);
}
