#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#if defined(__GLIBC__) && __GLIBC__ == 2
#include <net/ethernet.h>
#else
#include <linux/if_ether.h>
#endif
#include <xtables.h>
#include <linux/netfilter_ipv4/ipt_realm.h>

enum {
	O_REALM = 0,
};

static void realm_help(void)
{
	printf(
"realm match options:\n"
"[!] --realm value[/mask]\n"
"				Match realm\n");
}

static const struct xt_option_entry realm_opts[] = {
	{.name = "realm", .id = O_REALM, .type = XTTYPE_STRING,
	 .flags = XTOPT_MAND | XTOPT_INVERT},
	XTOPT_TABLEEND,
};

/* array of realms from /etc/iproute2/rt_realms */
static struct xtables_lmap *realms;

static void realm_init(struct xt_entry_match *m)
{
	const char file[] = "/etc/iproute2/rt_realms";
	realms = xtables_lmap_init(file);
	if (realms == NULL && errno != ENOENT)
		fprintf(stderr, "Warning: %s: %s\n", file, strerror(errno));
}

static void realm_parse(struct xt_option_call *cb)
{
	struct ipt_realm_info *realminfo = cb->data;
	int id;
	char *end;

	xtables_option_parse(cb);
	realminfo->id = strtoul(cb->arg, &end, 0);
	if (end != cb->arg && (*end == '/' || *end == '\0')) {
		if (*end == '/')
			realminfo->mask = strtoul(end+1, &end, 0);
		else
			realminfo->mask = 0xffffffff;
		if (*end != '\0' || end == cb->arg)
			xtables_error(PARAMETER_PROBLEM,
				   "Bad realm value \"%s\"", cb->arg);
	} else {
		id = xtables_lmap_name2id(realms, cb->arg);
		if (id == -1)
			xtables_error(PARAMETER_PROBLEM,
				   "Realm \"%s\" not found", cb->arg);
		realminfo->id = id;
		realminfo->mask = 0xffffffff;
	}
	if (cb->invert)
		realminfo->invert = 1;
}

static void
print_realm(unsigned long id, unsigned long mask, int numeric)
{
	const char* name = NULL;

	if (mask != 0xffffffff)
		printf(" 0x%lx/0x%lx", id, mask);
	else {
		if (numeric == 0)
			name = xtables_lmap_id2name(realms, id);
		if (name)
			printf(" %s", name);
		else
			printf(" 0x%lx", id);
	}
}

static void realm_print(const void *ip, const struct xt_entry_match *match,
                        int numeric)
{
	const struct ipt_realm_info *ri = (const void *)match->data;

	if (ri->invert)
		printf(" !");

	printf(" realm");
	print_realm(ri->id, ri->mask, numeric);
}

static void realm_save(const void *ip, const struct xt_entry_match *match)
{
	const struct ipt_realm_info *ri = (const void *)match->data;

	if (ri->invert)
		printf(" !");

	printf(" --realm");
	print_realm(ri->id, ri->mask, 0);
}

static struct xtables_match realm_mt_reg = {
	.name		= "realm",
	.version	= XTABLES_VERSION,
	.family		= NFPROTO_IPV4,
	.size		= XT_ALIGN(sizeof(struct ipt_realm_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct ipt_realm_info)),
	.help		= realm_help,
	.init		= realm_init,
	.print		= realm_print,
	.save		= realm_save,
	.x6_parse	= realm_parse,
	.x6_options	= realm_opts,
};

void _init(void)
{
	xtables_register_match(&realm_mt_reg);
}
