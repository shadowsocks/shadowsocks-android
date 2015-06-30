#ifndef _IP6TABLES_USER_H
#define _IP6TABLES_USER_H

#include <netinet/ip.h>
#include <xtables.h>
#include <libiptc/libip6tc.h>
#include <iptables/internal.h>

/* Your shared library should call one of these. */
extern int do_command6(int argc, char *argv[], char **table,
		       struct ip6tc_handle **handle);

extern int for_each_chain6(int (*fn)(const ip6t_chainlabel, int, struct ip6tc_handle *), int verbose, int builtinstoo, struct ip6tc_handle *handle);
extern int flush_entries6(const ip6t_chainlabel chain, int verbose, struct ip6tc_handle *handle);
extern int delete_chain6(const ip6t_chainlabel chain, int verbose, struct ip6tc_handle *handle);
void print_rule6(const struct ip6t_entry *e, struct ip6tc_handle *h, const char *chain, int counters);

extern struct xtables_globals ip6tables_globals;

#endif /*_IP6TABLES_USER_H*/
