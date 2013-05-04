#include <stdio.h>
#if defined(__GLIBC__) && __GLIBC__ == 2
#include <net/ethernet.h>
#else
#include <linux/if_ether.h>
#endif
#include <xtables.h>
#include <linux/netfilter/xt_mac.h>

enum {
	O_MAC = 0,
};

static void mac_help(void)
{
	printf(
"mac match options:\n"
"[!] --mac-source XX:XX:XX:XX:XX:XX\n"
"				Match source MAC address\n");
}

#define s struct xt_mac_info
static const struct xt_option_entry mac_opts[] = {
	{.name = "mac-source", .id = O_MAC, .type = XTTYPE_ETHERMAC,
	 .flags = XTOPT_MAND | XTOPT_INVERT | XTOPT_PUT,
	 XTOPT_POINTER(s, srcaddr)},
	XTOPT_TABLEEND,
};
#undef s

static void mac_parse(struct xt_option_call *cb)
{
	struct xt_mac_info *macinfo = cb->data;

	xtables_option_parse(cb);
	if (cb->invert)
		macinfo->invert = 1;
}

static void print_mac(const unsigned char *macaddress)
{
	unsigned int i;

	printf(" %02X", macaddress[0]);
	for (i = 1; i < ETH_ALEN; ++i)
		printf(":%02X", macaddress[i]);
}

static void
mac_print(const void *ip, const struct xt_entry_match *match, int numeric)
{
	const struct xt_mac_info *info = (void *)match->data;
	printf(" MAC");

	if (info->invert)
		printf(" !");
	
	print_mac(info->srcaddr);
}

static void mac_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_mac_info *info = (void *)match->data;

	if (info->invert)
		printf(" !");

	printf(" --mac-source");
	print_mac(info->srcaddr);
}

static struct xtables_match mac_match = {
	.family		= NFPROTO_UNSPEC,
 	.name		= "mac",
	.version	= XTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_mac_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_mac_info)),
	.help		= mac_help,
	.x6_parse	= mac_parse,
	.print		= mac_print,
	.save		= mac_save,
	.x6_options	= mac_opts,
};

void _init(void)
{
	xtables_register_match(&mac_match);
}
