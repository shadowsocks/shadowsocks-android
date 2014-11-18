#ifndef _IPTABLES_USER_H
#define _IPTABLES_USER_H

#include <netinet/ip.h>
#include <xtables.h>
#include <libiptc/libiptc.h>
#include <iptables/internal.h>

/* Your shared library should call one of these. */
extern int do_command4(int argc, char *argv[], char **table,
		      struct iptc_handle **handle);
extern int delete_chain4(const ipt_chainlabel chain, int verbose,
			struct iptc_handle *handle);
extern int flush_entries4(const ipt_chainlabel chain, int verbose, 
			struct iptc_handle *handle);
extern int for_each_chain4(int (*fn)(const ipt_chainlabel, int, struct iptc_handle *),
		int verbose, int builtinstoo, struct iptc_handle *handle);
extern void print_rule4(const struct ipt_entry *e,
		struct iptc_handle *handle, const char *chain, int counters);

/* kernel revision handling */
extern int kernel_version;
extern void get_kernel_version(void);
#define LINUX_VERSION(x,y,z)	(0x10000*(x) + 0x100*(y) + z)
#define LINUX_VERSION_MAJOR(x)	(((x)>>16) & 0xFF)
#define LINUX_VERSION_MINOR(x)	(((x)>> 8) & 0xFF)
#define LINUX_VERSION_PATCH(x)	( (x)      & 0xFF)

extern struct xtables_globals iptables_globals;

#endif /*_IPTABLES_USER_H*/
