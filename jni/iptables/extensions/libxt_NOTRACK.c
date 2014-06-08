/* Shared library add-on to iptables to add NOTRACK target support. */
#include <xtables.h>

static struct xtables_target notrack_target = {
	.family		= NFPROTO_UNSPEC,
	.name		= "NOTRACK",
	.version	= XTABLES_VERSION,
	.size		= XT_ALIGN(0),
	.userspacesize	= XT_ALIGN(0),
};

void _init(void)
{
	xtables_register_target(&notrack_target);
}
