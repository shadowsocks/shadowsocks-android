/* Shared library add-on to iptables for DSCP
 *
 * (C) 2002 by Harald Welte <laforge@gnumonks.org>
 *
 * This program is distributed under the terms of GNU GPL v2, 1991
 *
 * libipt_dscp.c borrowed heavily from libipt_tos.c
 *
 * --class support added by Iain Barnes
 * 
 * For a list of DSCP codepoints see 
 * http://www.iana.org/assignments/dscp-registry
 *
 */
#include <stdio.h>
#include <string.h>
#include <xtables.h>
#include <linux/netfilter/xt_dscp.h>

/* This is evil, but it's my code - HW*/
#include "dscp_helper.c"

enum {
	O_DSCP = 0,
	O_DSCP_CLASS,
	F_DSCP       = 1 << O_DSCP,
	F_DSCP_CLASS = 1 << O_DSCP_CLASS,
};

static void dscp_help(void)
{
	printf(
"dscp match options\n"
"[!] --dscp value		Match DSCP codepoint with numerical value\n"
"  		                This value can be in decimal (ex: 32)\n"
"               		or in hex (ex: 0x20)\n"
"[!] --dscp-class name		Match the DiffServ class. This value may\n"
"				be any of the BE,EF, AFxx or CSx classes\n"
"\n"
"				These two options are mutually exclusive !\n");
}

static const struct xt_option_entry dscp_opts[] = {
	{.name = "dscp", .id = O_DSCP, .excl = F_DSCP_CLASS,
	 .type = XTTYPE_UINT8, .min = 0, .max = XT_DSCP_MAX,
	 .flags = XTOPT_PUT, XTOPT_POINTER(struct xt_dscp_info, dscp)},
	{.name = "dscp-class", .id = O_DSCP_CLASS, .excl = F_DSCP,
	 .type = XTTYPE_STRING},
	XTOPT_TABLEEND,
};

static void dscp_parse(struct xt_option_call *cb)
{
	struct xt_dscp_info *dinfo = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_DSCP:
		if (cb->invert)
			dinfo->invert = 1;
		break;
	case O_DSCP_CLASS:
		dinfo->dscp = class_to_dscp(cb->arg);
		if (cb->invert)
			dinfo->invert = 1;
		break;
	}
}

static void dscp_check(struct xt_fcheck_call *cb)
{
	if (cb->xflags == 0)
		xtables_error(PARAMETER_PROBLEM,
		           "DSCP match: Parameter --dscp is required");
}

static void
dscp_print(const void *ip, const struct xt_entry_match *match, int numeric)
{
	const struct xt_dscp_info *dinfo =
		(const struct xt_dscp_info *)match->data;
	printf(" DSCP match %s0x%02x", dinfo->invert ? "!" : "", dinfo->dscp);
}

static void dscp_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_dscp_info *dinfo =
		(const struct xt_dscp_info *)match->data;

	printf("%s --dscp 0x%02x", dinfo->invert ? " !" : "", dinfo->dscp);
}

static struct xtables_match dscp_match = {
	.family		= NFPROTO_UNSPEC,
	.name 		= "dscp",
	.version 	= XTABLES_VERSION,
	.size 		= XT_ALIGN(sizeof(struct xt_dscp_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_dscp_info)),
	.help		= dscp_help,
	.print		= dscp_print,
	.save		= dscp_save,
	.x6_parse	= dscp_parse,
	.x6_fcheck	= dscp_check,
	.x6_options	= dscp_opts,
};

void _init(void)
{
	xtables_register_match(&dscp_match);
}
