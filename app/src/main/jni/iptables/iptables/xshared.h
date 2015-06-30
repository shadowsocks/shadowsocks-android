#ifndef IPTABLES_XSHARED_H
#define IPTABLES_XSHARED_H 1

#include <limits.h>
#include <stdint.h>
#include <netinet/in.h>
#include <net/if.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv6/ip6_tables.h>

enum {
	OPT_NONE        = 0,
	OPT_NUMERIC     = 1 << 0,
	OPT_SOURCE      = 1 << 1,
	OPT_DESTINATION = 1 << 2,
	OPT_PROTOCOL    = 1 << 3,
	OPT_JUMP        = 1 << 4,
	OPT_VERBOSE     = 1 << 5,
	OPT_EXPANDED    = 1 << 6,
	OPT_VIANAMEIN   = 1 << 7,
	OPT_VIANAMEOUT  = 1 << 8,
	OPT_LINENUMBERS = 1 << 9,
	OPT_COUNTERS    = 1 << 10,
};

struct xtables_globals;
struct xtables_rule_match;
struct xtables_target;

/**
 * xtables_afinfo - protocol family dependent information
 * @kmod:		kernel module basename (e.g. "ip_tables")
 * @proc_exists:	file which exists in procfs when module already loaded
 * @libprefix:		prefix of .so library name (e.g. "libipt_")
 * @family:		nfproto family
 * @ipproto:		used by setsockopt (e.g. IPPROTO_IP)
 * @so_rev_match:	optname to check revision support of match
 * @so_rev_target:	optname to check revision support of target
 */
struct xtables_afinfo {
	const char *kmod;
	const char *proc_exists;
	const char *libprefix;
	uint8_t family;
	uint8_t ipproto;
	int so_rev_match;
	int so_rev_target;
};

struct iptables_command_state {
	union {
		struct ipt_entry fw;
		struct ip6t_entry fw6;
	};
	int invert;
	int c;
	unsigned int options;
	struct xtables_rule_match *matches;
	struct xtables_target *target;
	char *protocol;
	int proto_used;
	const char *jumpto;
	char **argv;
};

typedef int (*mainfunc_t)(int, char **);

struct subcommand {
	const char *name;
	mainfunc_t main;
};

enum {
	XT_OPTION_OFFSET_SCALE = 256,
};

extern void print_extension_helps(const struct xtables_target *,
	const struct xtables_rule_match *);
extern const char *proto_to_name(uint8_t, int);
extern int command_default(struct iptables_command_state *,
	struct xtables_globals *);
extern struct xtables_match *load_proto(struct iptables_command_state *);
extern int subcmd_main(int, char **, const struct subcommand *);

extern const struct xtables_afinfo *afinfo;

#endif /* IPTABLES_XSHARED_H */
