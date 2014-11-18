/* Shared library add-on to ip6tables to add mobility header support. */
/*
 * Copyright (C)2006 USAGI/WIDE Project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Author:
 *	Masahide NAKAMURA @USAGI <masahide.nakamura.cz@hitachi.com>
 *
 * Based on libip6t_{icmpv6,udp}.c
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <xtables.h>
#include <linux/netfilter_ipv6/ip6t_mh.h>

enum {
	O_MH_TYPE = 0,
};

struct mh_name {
	const char *name;
	uint8_t type;
};

static const struct mh_name mh_names[] = {
	{ "binding-refresh-request", 0, },
	/* Alias */ { "brr", 0, },
	{ "home-test-init", 1, },
	/* Alias */ { "hoti", 1, },
	{ "careof-test-init", 2, },
	/* Alias */ { "coti", 2, },
	{ "home-test", 3, },
	/* Alias */ { "hot", 3, },
	{ "careof-test", 4, },
	/* Alias */ { "cot", 4, },
	{ "binding-update", 5, },
	/* Alias */ { "bu", 5, },
	{ "binding-acknowledgement", 6, },
	/* Alias */ { "ba", 6, },
	{ "binding-error", 7, },
	/* Alias */ { "be", 7, },
};

static void print_types_all(void)
{
	unsigned int i;
	printf("Valid MH types:");

	for (i = 0; i < ARRAY_SIZE(mh_names); ++i) {
		if (i && mh_names[i].type == mh_names[i-1].type)
			printf(" (%s)", mh_names[i].name);
		else
			printf("\n%s", mh_names[i].name);
	}
	printf("\n");
}

static void mh_help(void)
{
	printf(
"mh match options:\n"
"[!] --mh-type type[:type]	match mh type\n");
	print_types_all();
}

static void mh_init(struct xt_entry_match *m)
{
	struct ip6t_mh *mhinfo = (struct ip6t_mh *)m->data;

	mhinfo->types[1] = 0xFF;
}

static unsigned int name_to_type(const char *name)
{
	int namelen = strlen(name);
	static const unsigned int limit = ARRAY_SIZE(mh_names);
	unsigned int match = limit;
	unsigned int i;

	for (i = 0; i < limit; i++) {
		if (strncasecmp(mh_names[i].name, name, namelen) == 0) {
			int len = strlen(mh_names[i].name);
			if (match == limit || len == namelen)
				match = i;
		}
	}

	if (match != limit) {
		return mh_names[match].type;
	} else {
		unsigned int number;

		if (!xtables_strtoui(name, NULL, &number, 0, UINT8_MAX))
			xtables_error(PARAMETER_PROBLEM,
				   "Invalid MH type `%s'\n", name);
		return number;
	}
}

static void parse_mh_types(const char *mhtype, uint8_t *types)
{
	char *buffer;
	char *cp;

	buffer = strdup(mhtype);
	if ((cp = strchr(buffer, ':')) == NULL)
		types[0] = types[1] = name_to_type(buffer);
	else {
		*cp = '\0';
		cp++;

		types[0] = buffer[0] ? name_to_type(buffer) : 0;
		types[1] = cp[0] ? name_to_type(cp) : 0xFF;

		if (types[0] > types[1])
			xtables_error(PARAMETER_PROBLEM,
				   "Invalid MH type range (min > max)");
	}
	free(buffer);
}

static void mh_parse(struct xt_option_call *cb)
{
	struct ip6t_mh *mhinfo = cb->data;

	xtables_option_parse(cb);
	parse_mh_types(cb->arg, mhinfo->types);
	if (cb->invert)
		mhinfo->invflags |= IP6T_MH_INV_TYPE;
}

static const char *type_to_name(uint8_t type)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(mh_names); ++i)
		if (mh_names[i].type == type)
			return mh_names[i].name;

	return NULL;
}

static void print_type(uint8_t type, int numeric)
{
	const char *name;
	if (numeric || !(name = type_to_name(type)))
		printf("%u", type);
	else
		printf("%s", name);
}

static void print_types(uint8_t min, uint8_t max, int invert, int numeric)
{
	const char *inv = invert ? "!" : "";

	if (min != 0 || max != 0xFF || invert) {
		printf(" ");
		if (min == max) {
			printf("%s", inv);
			print_type(min, numeric);
		} else {
			printf("%s", inv);
			print_type(min, numeric);
			printf(":");
			print_type(max, numeric);
		}
	}
}

static void mh_print(const void *ip, const struct xt_entry_match *match,
                     int numeric)
{
	const struct ip6t_mh *mhinfo = (struct ip6t_mh *)match->data;

	printf(" mh");
	print_types(mhinfo->types[0], mhinfo->types[1],
		    mhinfo->invflags & IP6T_MH_INV_TYPE,
		    numeric);
	if (mhinfo->invflags & ~IP6T_MH_INV_MASK)
		printf(" Unknown invflags: 0x%X",
		       mhinfo->invflags & ~IP6T_MH_INV_MASK);
}

static void mh_save(const void *ip, const struct xt_entry_match *match)
{
	const struct ip6t_mh *mhinfo = (struct ip6t_mh *)match->data;

	if (mhinfo->types[0] == 0 && mhinfo->types[1] == 0xFF)
		return;

	if (mhinfo->invflags & IP6T_MH_INV_TYPE)
		printf(" !");

	if (mhinfo->types[0] != mhinfo->types[1])
		printf(" --mh-type %u:%u", mhinfo->types[0], mhinfo->types[1]);
	else
		printf(" --mh-type %u", mhinfo->types[0]);
}

static const struct xt_option_entry mh_opts[] = {
	{.name = "mh-type", .id = O_MH_TYPE, .type = XTTYPE_STRING,
	 .flags = XTOPT_INVERT},
	XTOPT_TABLEEND,
};

static struct xtables_match mh_mt6_reg = {
	.name		= "mh",
	.version	= XTABLES_VERSION,
	.family		= NFPROTO_IPV6,
	.size		= XT_ALIGN(sizeof(struct ip6t_mh)),
	.userspacesize	= XT_ALIGN(sizeof(struct ip6t_mh)),
	.help		= mh_help,
	.init		= mh_init,
	.x6_parse	= mh_parse,
	.print		= mh_print,
	.save		= mh_save,
	.x6_options	= mh_opts,
};

void _init(void)
{
	xtables_register_match(&mh_mt6_reg);
}
