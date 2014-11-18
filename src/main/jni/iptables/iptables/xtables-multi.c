#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xshared.h"

#include "xtables-multi.h"

#ifdef ENABLE_IPV4
#include "iptables-multi.h"
#endif

#ifdef ENABLE_IPV6
#include "ip6tables-multi.h"
#endif

static const struct subcommand multi_subcommands[] = {
#ifdef ENABLE_IPV4
	{"iptables",            iptables_main},
	{"main4",               iptables_main},
	{"iptables-save",       iptables_save_main},
	{"save4",               iptables_save_main},
	{"iptables-restore",    iptables_restore_main},
	{"restore4",            iptables_restore_main},
#endif
	{"iptables-xml",        iptables_xml_main},
	{"xml",                 iptables_xml_main},
#ifdef ENABLE_IPV6
	{"ip6tables",           ip6tables_main},
	{"main6",               ip6tables_main},
	{"ip6tables-save",      ip6tables_save_main},
	{"save6",               ip6tables_save_main},
	{"ip6tables-restore",   ip6tables_restore_main},
	{"restore6",            ip6tables_restore_main},
#endif
	{NULL},
};

int main(int argc, char **argv)
{
	return subcmd_main(argc, argv, multi_subcommands);
}
