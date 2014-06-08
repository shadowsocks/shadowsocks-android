/* Shared library add-on to iptables to add devgroup matching support.
 *
 * Copyright (c) 2011 Patrick McHardy <kaber@trash.net>
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <xtables.h>
#include <linux/netfilter/xt_devgroup.h>

static void devgroup_help(void)
{
	printf(
"devgroup match options:\n"
"[!] --src-group value[/mask]	Match device group of incoming device\n"
"[!] --dst-group value[/mask]	Match device group of outgoing device\n"
		);
}

enum {
	O_SRC_GROUP = 0,
	O_DST_GROUP,
};

static const struct xt_option_entry devgroup_opts[] = {
	{.name = "src-group", .id = O_SRC_GROUP, .type = XTTYPE_STRING,
	 .flags = XTOPT_INVERT},
	{.name = "dst-group", .id = O_DST_GROUP, .type = XTTYPE_STRING,
	 .flags = XTOPT_INVERT},
	XTOPT_TABLEEND,
};

/* array of devgroups from /etc/iproute2/group_map */
static struct xtables_lmap *devgroups;

static void devgroup_init(struct xt_entry_match *match)
{
	const char file[] = "/etc/iproute2/group_map";
	devgroups = xtables_lmap_init(file);
	if (devgroups == NULL && errno != ENOENT)
		fprintf(stderr, "Warning: %s: %s\n", file, strerror(errno));
}

static void devgroup_parse(struct xt_option_call *cb)
{
	struct xt_devgroup_info *info = cb->data;
	unsigned int id;
	char *end;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_SRC_GROUP:
		info->src_group = strtoul(cb->arg, &end, 0);
		if (end != cb->arg && (*end == '/' || *end == '\0')) {
			if (*end == '/')
				info->src_mask = strtoul(end+1, &end, 0);
			else
				info->src_mask = 0xffffffff;
			if (*end != '\0' || end == cb->arg)
				xtables_error(PARAMETER_PROBLEM,
					      "Bad src-group value `%s'",
					      cb->arg);
		} else {
			id = xtables_lmap_name2id(devgroups, cb->arg);
			if (id == -1)
				xtables_error(PARAMETER_PROBLEM,
					      "Device group `%s' not found",
					      cb->arg);
			info->src_group = id;
			info->src_mask  = 0xffffffff;
		}
		info->flags |= XT_DEVGROUP_MATCH_SRC;
		if (cb->invert)
			info->flags |= XT_DEVGROUP_INVERT_SRC;
		break;
	case O_DST_GROUP:
		info->dst_group = strtoul(cb->arg, &end, 0);
		if (end != cb->arg && (*end == '/' || *end == '\0')) {
			if (*end == '/')
				info->dst_mask = strtoul(end+1, &end, 0);
			else
				info->dst_mask = 0xffffffff;
			if (*end != '\0' || end == cb->arg)
				xtables_error(PARAMETER_PROBLEM,
					      "Bad dst-group value `%s'",
					      cb->arg);
		} else {
			id = xtables_lmap_name2id(devgroups, cb->arg);
			if (id == -1)
				xtables_error(PARAMETER_PROBLEM,
					      "Device group `%s' not found",
					      cb->arg);
			info->dst_group = id;
			info->dst_mask  = 0xffffffff;
		}
		info->flags |= XT_DEVGROUP_MATCH_DST;
		if (cb->invert)
			info->flags |= XT_DEVGROUP_INVERT_DST;
		break;
	}
}

static void
print_devgroup(unsigned int id, unsigned int mask, int numeric)
{
	const char *name = NULL;

	if (mask != 0xffffffff)
		printf("0x%x/0x%x", id, mask);
	else {
		if (numeric == 0)
			name = xtables_lmap_id2name(devgroups, id);
		if (name)
			printf("%s", name);
		else
			printf("0x%x", id);
	}
}

static void devgroup_show(const char *pfx, const struct xt_devgroup_info *info,
			  int numeric)
{
	if (info->flags & XT_DEVGROUP_MATCH_SRC) {
		if (info->flags & XT_DEVGROUP_INVERT_SRC)
			printf(" !");
		printf(" %ssrc-group ", pfx);
		print_devgroup(info->src_group, info->src_mask, numeric);
	}

	if (info->flags & XT_DEVGROUP_MATCH_DST) {
		if (info->flags & XT_DEVGROUP_INVERT_DST)
			printf(" !");
		printf(" %sdst-group ", pfx);
		print_devgroup(info->src_group, info->src_mask, numeric);
	}
}

static void devgroup_print(const void *ip, const struct xt_entry_match *match,
                        int numeric)
{
	const struct xt_devgroup_info *info = (const void *)match->data;

	devgroup_show("", info, numeric);
}

static void devgroup_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_devgroup_info *info = (const void *)match->data;

	devgroup_show("--", info, 0);
}

static void devgroup_check(struct xt_fcheck_call *cb)
{
	if (cb->xflags == 0)
		xtables_error(PARAMETER_PROBLEM,
			      "devgroup match: You must specify either "
			      "'--src-group' or '--dst-group'");
}

static struct xtables_match devgroup_mt_reg = {
	.name		= "devgroup",
	.version	= XTABLES_VERSION,
	.family		= NFPROTO_UNSPEC,
	.size		= XT_ALIGN(sizeof(struct xt_devgroup_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_devgroup_info)),
	.init		= devgroup_init,
	.help		= devgroup_help,
	.print		= devgroup_print,
	.save		= devgroup_save,
	.x6_parse	= devgroup_parse,
	.x6_fcheck	= devgroup_check,
	.x6_options	= devgroup_opts,
};

void _init(void)
{
	xtables_register_match(&devgroup_mt_reg);
}
