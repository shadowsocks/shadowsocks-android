/* Copyright (C) 2000-2002 Joakim Axelsson <gozem@linux.nu>
 *                         Patrick Schaaf <bof@bof.de>
 *                         Martin Josefsson <gandalf@wlug.westbo.se>
 * Copyright (C) 2003-2010 Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.  
 */

/* Shared library add-on to iptables to add IP set matching. */
#include <stdbool.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <ctype.h>
#include <errno.h>

#include <xtables.h>
#include <linux/netfilter/xt_set.h>
#include "libxt_set.h"

/* Revision 0 */

static void
set_help_v0(void)
{
	printf("set match options:\n"
	       " [!] --match-set name flags\n"
	       "		 'name' is the set name from to match,\n" 
	       "		 'flags' are the comma separated list of\n"
	       "		 'src' and 'dst' specifications.\n");
}

static const struct option set_opts_v0[] = {
	{.name = "match-set", .has_arg = true, .val = '1'},
	{.name = "set",       .has_arg = true, .val = '2'},
	XT_GETOPT_TABLEEND,
};

static void
set_check_v0(unsigned int flags)
{
	if (!flags)
		xtables_error(PARAMETER_PROBLEM,
			"You must specify `--match-set' with proper arguments");
}

static int
set_parse_v0(int c, char **argv, int invert, unsigned int *flags,
	     const void *entry, struct xt_entry_match **match)
{
	struct xt_set_info_match_v0 *myinfo = 
		(struct xt_set_info_match_v0 *) (*match)->data;
	struct xt_set_info_v0 *info = &myinfo->match_set;

	switch (c) {
	case '2':
		fprintf(stderr,
			"--set option deprecated, please use --match-set\n");
	case '1':		/* --match-set <set> <flag>[,<flag> */
		if (info->u.flags[0])
			xtables_error(PARAMETER_PROBLEM,
				      "--match-set can be specified only once");

		xtables_check_inverse(optarg, &invert, &optind, 0, argv);
		if (invert)
			info->u.flags[0] |= IPSET_MATCH_INV;

		if (!argv[optind]
		    || argv[optind][0] == '-'
		    || argv[optind][0] == '!')
			xtables_error(PARAMETER_PROBLEM,
				      "--match-set requires two args.");

		if (strlen(optarg) > IPSET_MAXNAMELEN - 1)
			xtables_error(PARAMETER_PROBLEM,
				      "setname `%s' too long, max %d characters.",
				      optarg, IPSET_MAXNAMELEN - 1);

		get_set_byname(optarg, (struct xt_set_info *)info);
		parse_dirs_v0(argv[optind], info);
		DEBUGP("parse: set index %u\n", info->index);
		optind++;
		
		*flags = 1;
		break;
	}

	return 1;
}

static void
print_match_v0(const char *prefix, const struct xt_set_info_v0 *info)
{
	int i;
	char setname[IPSET_MAXNAMELEN];

	get_set_byid(setname, info->index);
	printf("%s %s %s",
	       (info->u.flags[0] & IPSET_MATCH_INV) ? " !" : "",
	       prefix,
	       setname); 
	for (i = 0; i < IPSET_DIM_MAX; i++) {
		if (!info->u.flags[i])
			break;		
		printf("%s%s",
		       i == 0 ? " " : ",",
		       info->u.flags[i] & IPSET_SRC ? "src" : "dst");
	}
}

/* Prints out the matchinfo. */
static void
set_print_v0(const void *ip, const struct xt_entry_match *match, int numeric)
{
	const struct xt_set_info_match_v0 *info = (const void *)match->data;

	print_match_v0("match-set", &info->match_set);
}

static void
set_save_v0(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_set_info_match_v0 *info = (const void *)match->data;

	print_match_v0("--match-set", &info->match_set);
}

/* Revision 1 */

#define set_help_v1	set_help_v0
#define set_opts_v1	set_opts_v0
#define set_check_v1	set_check_v0

static int
set_parse_v1(int c, char **argv, int invert, unsigned int *flags,
	     const void *entry, struct xt_entry_match **match)
{
	struct xt_set_info_match_v1 *myinfo = 
		(struct xt_set_info_match_v1 *) (*match)->data;
	struct xt_set_info *info = &myinfo->match_set;

	switch (c) {
	case '2':
		fprintf(stderr,
			"--set option deprecated, please use --match-set\n");
	case '1':		/* --match-set <set> <flag>[,<flag> */
		if (info->dim)
			xtables_error(PARAMETER_PROBLEM,
				      "--match-set can be specified only once");

		xtables_check_inverse(optarg, &invert, &optind, 0, argv);
		if (invert)
			info->flags |= IPSET_INV_MATCH;

		if (!argv[optind]
		    || argv[optind][0] == '-'
		    || argv[optind][0] == '!')
			xtables_error(PARAMETER_PROBLEM,
				      "--match-set requires two args.");

		if (strlen(optarg) > IPSET_MAXNAMELEN - 1)
			xtables_error(PARAMETER_PROBLEM,
				      "setname `%s' too long, max %d characters.",
				      optarg, IPSET_MAXNAMELEN - 1);

		get_set_byname(optarg, info);
		parse_dirs(argv[optind], info);
		DEBUGP("parse: set index %u\n", info->index);
		optind++;
		
		*flags = 1;
		break;
	}

	return 1;
}

static void
print_match(const char *prefix, const struct xt_set_info *info)
{
	int i;
	char setname[IPSET_MAXNAMELEN];

	get_set_byid(setname, info->index);
	printf("%s %s %s",
	       (info->flags & IPSET_INV_MATCH) ? " !" : "",
	       prefix,
	       setname); 
	for (i = 1; i <= info->dim; i++) {		
		printf("%s%s",
		       i == 1 ? " " : ",",
		       info->flags & (1 << i) ? "src" : "dst");
	}
}

/* Prints out the matchinfo. */
static void
set_print_v1(const void *ip, const struct xt_entry_match *match, int numeric)
{
	const struct xt_set_info_match_v1 *info = (const void *)match->data;

	print_match("match-set", &info->match_set);
}

static void
set_save_v1(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_set_info_match_v1 *info = (const void *)match->data;

	print_match("--match-set", &info->match_set);
}

static struct xtables_match set_mt_reg[] = {
	{
		.name		= "set",
		.revision	= 0,
		.version	= XTABLES_VERSION,
		.family		= NFPROTO_IPV4,
		.size		= XT_ALIGN(sizeof(struct xt_set_info_match_v0)),
		.userspacesize	= XT_ALIGN(sizeof(struct xt_set_info_match_v0)),
		.help		= set_help_v0,
		.parse		= set_parse_v0,
		.final_check	= set_check_v0,
		.print		= set_print_v0,
		.save		= set_save_v0,
		.extra_opts	= set_opts_v0,
	},
	{
		.name		= "set",
		.revision	= 1,
		.version	= XTABLES_VERSION,
		.family		= NFPROTO_UNSPEC,
		.size		= XT_ALIGN(sizeof(struct xt_set_info_match_v1)),
		.userspacesize	= XT_ALIGN(sizeof(struct xt_set_info_match_v1)),
		.help		= set_help_v1,
		.parse		= set_parse_v1,
		.final_check	= set_check_v1,
		.print		= set_print_v1,
		.save		= set_save_v1,
		.extra_opts	= set_opts_v1,
	},
};

void _init(void)
{
	xtables_register_matches(set_mt_reg, ARRAY_SIZE(set_mt_reg));
}
