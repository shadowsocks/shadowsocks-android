/* Shared library add-on to iptables to add MIRROR target support. */
#include <xtables.h>

static struct xtables_target mirror_tg_reg = {
	.name		= "MIRROR",
	.version	= XTABLES_VERSION,
	.family		= NFPROTO_IPV4,
	.size		= XT_ALIGN(0),
	.userspacesize	= XT_ALIGN(0),
};

void _init(void)
{
	xtables_register_target(&mirror_tg_reg);
}
