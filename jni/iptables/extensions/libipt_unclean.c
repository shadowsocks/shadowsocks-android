/* Shared library add-on to iptables for unclean. */
#include <xtables.h>

static struct xtables_match unclean_mt_reg = {
	.name		= "unclean",
	.version	= XTABLES_VERSION,
	.family		= NFPROTO_IPV4,
	.size		= XT_ALIGN(0),
	.userspacesize	= XT_ALIGN(0),
};

void _init(void)
{
	xtables_register_match(&unclean_mt_reg);
}
