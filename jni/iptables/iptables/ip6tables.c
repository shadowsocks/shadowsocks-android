/* Code to take an ip6tables-style command line and do it. */

/*
 * Author: Paul.Russell@rustcorp.com.au and mneuling@radlogic.com.au
 *
 * (C) 2000-2002 by the netfilter coreteam <coreteam@netfilter.org>:
 * 		    Paul 'Rusty' Russell <rusty@rustcorp.com.au>
 * 		    Marc Boucher <marc+nf@mbsi.ca>
 * 		    James Morris <jmorris@intercode.com.au>
 * 		    Harald Welte <laforge@gnumonks.org>
 * 		    Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <getopt.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <limits.h>
#include <ip6tables.h>
#include <xtables.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "ip6tables-multi.h"
#include "xshared.h"

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define FMT_NUMERIC	0x0001
#define FMT_NOCOUNTS	0x0002
#define FMT_KILOMEGAGIGA 0x0004
#define FMT_OPTIONS	0x0008
#define FMT_NOTABLE	0x0010
#define FMT_NOTARGET	0x0020
#define FMT_VIA		0x0040
#define FMT_NONEWLINE	0x0080
#define FMT_LINENUMBERS 0x0100

#define FMT_PRINT_RULE (FMT_NOCOUNTS | FMT_OPTIONS | FMT_VIA \
			| FMT_NUMERIC | FMT_NOTABLE)
#define FMT(tab,notab) ((format) & FMT_NOTABLE ? (notab) : (tab))


#define CMD_NONE		0x0000U
#define CMD_INSERT		0x0001U
#define CMD_DELETE		0x0002U
#define CMD_DELETE_NUM		0x0004U
#define CMD_REPLACE		0x0008U
#define CMD_APPEND		0x0010U
#define CMD_LIST		0x0020U
#define CMD_FLUSH		0x0040U
#define CMD_ZERO		0x0080U
#define CMD_NEW_CHAIN		0x0100U
#define CMD_DELETE_CHAIN	0x0200U
#define CMD_SET_POLICY		0x0400U
#define CMD_RENAME_CHAIN	0x0800U
#define CMD_LIST_RULES		0x1000U
#define CMD_ZERO_NUM		0x2000U
#define CMD_CHECK		0x4000U
#define NUMBER_OF_CMD	16
static const char cmdflags[] = { 'I', 'D', 'D', 'R', 'A', 'L', 'F', 'Z',
				 'Z', 'N', 'X', 'P', 'E', 'S', 'C' };

#define NUMBER_OF_OPT	ARRAY_SIZE(optflags)
static const char optflags[]
= { 'n', 's', 'd', 'p', 'j', 'v', 'x', 'i', 'o', '0', 'c'};

static struct option original_opts[] = {
	{.name = "append",        .has_arg = 1, .val = 'A'},
	{.name = "delete",        .has_arg = 1, .val = 'D'},
	{.name = "check" ,        .has_arg = 1, .val = 'C'},
	{.name = "insert",        .has_arg = 1, .val = 'I'},
	{.name = "replace",       .has_arg = 1, .val = 'R'},
	{.name = "list",          .has_arg = 2, .val = 'L'},
	{.name = "list-rules",    .has_arg = 2, .val = 'S'},
	{.name = "flush",         .has_arg = 2, .val = 'F'},
	{.name = "zero",          .has_arg = 2, .val = 'Z'},
	{.name = "new-chain",     .has_arg = 1, .val = 'N'},
	{.name = "delete-chain",  .has_arg = 2, .val = 'X'},
	{.name = "rename-chain",  .has_arg = 1, .val = 'E'},
	{.name = "policy",        .has_arg = 1, .val = 'P'},
	{.name = "source",        .has_arg = 1, .val = 's'},
	{.name = "destination",   .has_arg = 1, .val = 'd'},
	{.name = "src",           .has_arg = 1, .val = 's'}, /* synonym */
	{.name = "dst",           .has_arg = 1, .val = 'd'}, /* synonym */
	{.name = "protocol",      .has_arg = 1, .val = 'p'},
	{.name = "in-interface",  .has_arg = 1, .val = 'i'},
	{.name = "jump",          .has_arg = 1, .val = 'j'},
	{.name = "table",         .has_arg = 1, .val = 't'},
	{.name = "match",         .has_arg = 1, .val = 'm'},
	{.name = "numeric",       .has_arg = 0, .val = 'n'},
	{.name = "out-interface", .has_arg = 1, .val = 'o'},
	{.name = "verbose",       .has_arg = 0, .val = 'v'},
	{.name = "exact",         .has_arg = 0, .val = 'x'},
	{.name = "version",       .has_arg = 0, .val = 'V'},
	{.name = "help",          .has_arg = 2, .val = 'h'},
	{.name = "line-numbers",  .has_arg = 0, .val = '0'},
	{.name = "modprobe",      .has_arg = 1, .val = 'M'},
	{.name = "set-counters",  .has_arg = 1, .val = 'c'},
	{.name = "goto",          .has_arg = 1, .val = 'g'},
	{.name = "ipv4",          .has_arg = 0, .val = '4'},
	{.name = "ipv6",          .has_arg = 0, .val = '6'},
	{NULL},
};

void ip6tables_exit_error(enum xtables_exittype status, const char *msg, ...) __attribute__((noreturn, format(printf,2,3)));
struct xtables_globals ip6tables_globals = {
	.option_offset = 0,
	.program_version = IPTABLES_VERSION,
	.orig_opts = original_opts,
	.exit_err = ip6tables_exit_error,
};

/* Table of legal combinations of commands and options.  If any of the
 * given commands make an option legal, that option is legal (applies to
 * CMD_LIST and CMD_ZERO only).
 * Key:
 *  +  compulsory
 *  x  illegal
 *     optional
 */

static const char commands_v_options[NUMBER_OF_CMD][NUMBER_OF_OPT] =
/* Well, it's better than "Re: Linux vs FreeBSD" */
{
	/*     -n  -s  -d  -p  -j  -v  -x  -i  -o --line -c */
/*INSERT*/    {'x',' ',' ',' ',' ',' ','x',' ',' ','x',' '},
/*DELETE*/    {'x',' ',' ',' ',' ',' ','x',' ',' ','x','x'},
/*DELETE_NUM*/{'x','x','x','x','x',' ','x','x','x','x','x'},
/*REPLACE*/   {'x',' ',' ',' ',' ',' ','x',' ',' ','x',' '},
/*APPEND*/    {'x',' ',' ',' ',' ',' ','x',' ',' ','x',' '},
/*LIST*/      {' ','x','x','x','x',' ',' ','x','x',' ','x'},
/*FLUSH*/     {'x','x','x','x','x',' ','x','x','x','x','x'},
/*ZERO*/      {'x','x','x','x','x',' ','x','x','x','x','x'},
/*ZERO_NUM*/  {'x','x','x','x','x',' ','x','x','x','x','x'},
/*NEW_CHAIN*/ {'x','x','x','x','x',' ','x','x','x','x','x'},
/*DEL_CHAIN*/ {'x','x','x','x','x',' ','x','x','x','x','x'},
/*SET_POLICY*/{'x','x','x','x','x',' ','x','x','x','x',' '},
/*RENAME*/    {'x','x','x','x','x',' ','x','x','x','x','x'},
/*LIST_RULES*/{'x','x','x','x','x',' ','x','x','x','x','x'},
/*CHECK*/     {'x',' ',' ',' ',' ',' ','x',' ',' ','x','x'},
};

static const unsigned int inverse_for_options[NUMBER_OF_OPT] =
{
/* -n */ 0,
/* -s */ IP6T_INV_SRCIP,
/* -d */ IP6T_INV_DSTIP,
/* -p */ IP6T_INV_PROTO,
/* -j */ 0,
/* -v */ 0,
/* -x */ 0,
/* -i */ IP6T_INV_VIA_IN,
/* -o */ IP6T_INV_VIA_OUT,
/*--line*/ 0,
/* -c */ 0,
};

#define opts ip6tables_globals.opts
#define prog_name ip6tables_globals.program_name
#define prog_vers ip6tables_globals.program_version
/* A few hardcoded protocols for 'all' and in case the user has no
   /etc/protocols */
struct pprot {
	const char *name;
	uint8_t num;
};

static void __attribute__((noreturn))
exit_tryhelp(int status)
{
	if (line != -1)
		fprintf(stderr, "Error occurred at line: %d\n", line);
	fprintf(stderr, "Try `%s -h' or '%s --help' for more information.\n",
			prog_name, prog_name);
	xtables_free_opts(1);
	exit(status);
}

static void
exit_printhelp(const struct xtables_rule_match *matches)
{
	printf("%s v%s\n\n"
"Usage: %s -[ACD] chain rule-specification [options]\n"
"       %s -I chain [rulenum] rule-specification [options]\n"
"       %s -R chain rulenum rule-specification [options]\n"
"       %s -D chain rulenum [options]\n"
"       %s -[LS] [chain [rulenum]] [options]\n"
"       %s -[FZ] [chain] [options]\n"
"       %s -[NX] chain\n"
"       %s -E old-chain-name new-chain-name\n"
"       %s -P chain target [options]\n"
"       %s -h (print this help information)\n\n",
	       prog_name, prog_vers, prog_name, prog_name,
	       prog_name, prog_name, prog_name, prog_name,
	       prog_name, prog_name, prog_name, prog_name);

	printf(
"Commands:\n"
"Either long or short options are allowed.\n"
"  --append  -A chain		Append to chain\n"
"  --check   -C chain		Check for the existence of a rule\n"
"  --delete  -D chain		Delete matching rule from chain\n"
"  --delete  -D chain rulenum\n"
"				Delete rule rulenum (1 = first) from chain\n"
"  --insert  -I chain [rulenum]\n"
"				Insert in chain as rulenum (default 1=first)\n"
"  --replace -R chain rulenum\n"
"				Replace rule rulenum (1 = first) in chain\n"
"  --list    -L [chain [rulenum]]\n"
"				List the rules in a chain or all chains\n"
"  --list-rules -S [chain [rulenum]]\n"
"				Print the rules in a chain or all chains\n"
"  --flush   -F [chain]		Delete all rules in  chain or all chains\n"
"  --zero    -Z [chain [rulenum]]\n"
"				Zero counters in chain or all chains\n"
"  --new     -N chain		Create a new user-defined chain\n"
"  --delete-chain\n"
"            -X [chain]		Delete a user-defined chain\n"
"  --policy  -P chain target\n"
"				Change policy on chain to target\n"
"  --rename-chain\n"
"            -E old-chain new-chain\n"
"				Change chain name, (moving any references)\n"

"Options:\n"
"    --ipv4	-4		Error (line is ignored by ip6tables-restore)\n"
"    --ipv6	-6		Nothing (line is ignored by iptables-restore)\n"
"[!] --proto	-p proto	protocol: by number or name, eg. `tcp'\n"
"[!] --source	-s address[/mask][,...]\n"
"				source specification\n"
"[!] --destination -d address[/mask][,...]\n"
"				destination specification\n"
"[!] --in-interface -i input name[+]\n"
"				network interface name ([+] for wildcard)\n"
"  --jump	-j target\n"
"				target for rule (may load target extension)\n"
#ifdef IP6T_F_GOTO
"  --goto	-g chain\n"
"				jump to chain with no return\n"
#endif
"  --match	-m match\n"
"				extended match (may load extension)\n"
"  --numeric	-n		numeric output of addresses and ports\n"
"[!] --out-interface -o output name[+]\n"
"				network interface name ([+] for wildcard)\n"
"  --table	-t table	table to manipulate (default: `filter')\n"
"  --verbose	-v		verbose mode\n"
"  --line-numbers		print line numbers when listing\n"
"  --exact	-x		expand numbers (display exact values)\n"
/*"[!] --fragment	-f		match second or further fragments only\n"*/
"  --modprobe=<command>		try to insert modules using this command\n"
"  --set-counters PKTS BYTES	set the counter during insert/append\n"
"[!] --version	-V		print package version.\n");

	print_extension_helps(xtables_targets, matches);
	exit(0);
}

void
ip6tables_exit_error(enum xtables_exittype status, const char *msg, ...)
{
	va_list args;

	va_start(args, msg);
	fprintf(stderr, "%s v%s: ", prog_name, prog_vers);
	vfprintf(stderr, msg, args);
	va_end(args);
	fprintf(stderr, "\n");
	if (status == PARAMETER_PROBLEM)
		exit_tryhelp(status);
	if (status == VERSION_PROBLEM)
		fprintf(stderr,
			"Perhaps ip6tables or your kernel needs to be upgraded.\n");
	/* On error paths, make sure that we don't leak memory */
	xtables_free_opts(1);
	exit(status);
}

static void
generic_opt_check(int command, int options)
{
	int i, j, legal = 0;

	/* Check that commands are valid with options.  Complicated by the
	 * fact that if an option is legal with *any* command given, it is
	 * legal overall (ie. -z and -l).
	 */
	for (i = 0; i < NUMBER_OF_OPT; i++) {
		legal = 0; /* -1 => illegal, 1 => legal, 0 => undecided. */

		for (j = 0; j < NUMBER_OF_CMD; j++) {
			if (!(command & (1<<j)))
				continue;

			if (!(options & (1<<i))) {
				if (commands_v_options[j][i] == '+')
					xtables_error(PARAMETER_PROBLEM,
						   "You need to supply the `-%c' "
						   "option for this command\n",
						   optflags[i]);
			} else {
				if (commands_v_options[j][i] != 'x')
					legal = 1;
				else if (legal == 0)
					legal = -1;
			}
		}
		if (legal == -1)
			xtables_error(PARAMETER_PROBLEM,
				   "Illegal option `-%c' with this command\n",
				   optflags[i]);
	}
}

static char
opt2char(int option)
{
	const char *ptr;
	for (ptr = optflags; option > 1; option >>= 1, ptr++);

	return *ptr;
}

static char
cmd2char(int option)
{
	const char *ptr;
	for (ptr = cmdflags; option > 1; option >>= 1, ptr++);

	return *ptr;
}

static void
add_command(unsigned int *cmd, const int newcmd, const int othercmds,
	    int invert)
{
	if (invert)
		xtables_error(PARAMETER_PROBLEM, "unexpected '!' flag");
	if (*cmd & (~othercmds))
		xtables_error(PARAMETER_PROBLEM, "Cannot use -%c with -%c\n",
			   cmd2char(newcmd), cmd2char(*cmd & (~othercmds)));
	*cmd |= newcmd;
}

/*
 *	All functions starting with "parse" should succeed, otherwise
 *	the program fails.
 *	Most routines return pointers to static data that may change
 *	between calls to the same or other routines with a few exceptions:
 *	"host_to_addr", "parse_hostnetwork", and "parse_hostnetworkmask"
 *	return global static data.
*/

/* These are invalid numbers as upper layer protocol */
static int is_exthdr(uint16_t proto)
{
	return (proto == IPPROTO_ROUTING ||
		proto == IPPROTO_FRAGMENT ||
		proto == IPPROTO_AH ||
		proto == IPPROTO_DSTOPTS);
}

/* Can't be zero. */
static int
parse_rulenumber(const char *rule)
{
	unsigned int rulenum;

	if (!xtables_strtoui(rule, NULL, &rulenum, 1, INT_MAX))
		xtables_error(PARAMETER_PROBLEM,
			   "Invalid rule number `%s'", rule);

	return rulenum;
}

static const char *
parse_target(const char *targetname)
{
	const char *ptr;

	if (strlen(targetname) < 1)
		xtables_error(PARAMETER_PROBLEM,
			   "Invalid target name (too short)");

	if (strlen(targetname) >= XT_EXTENSION_MAXNAMELEN)
		xtables_error(PARAMETER_PROBLEM,
			   "Invalid target name `%s' (%u chars max)",
			   targetname, XT_EXTENSION_MAXNAMELEN - 1);

	for (ptr = targetname; *ptr; ptr++)
		if (isspace(*ptr))
			xtables_error(PARAMETER_PROBLEM,
				   "Invalid target name `%s'", targetname);
	return targetname;
}

static void
set_option(unsigned int *options, unsigned int option, uint8_t *invflg,
	   int invert)
{
	if (*options & option)
		xtables_error(PARAMETER_PROBLEM, "multiple -%c flags not allowed",
			   opt2char(option));
	*options |= option;

	if (invert) {
		unsigned int i;
		for (i = 0; 1 << i != option; i++);

		if (!inverse_for_options[i])
			xtables_error(PARAMETER_PROBLEM,
				   "cannot have ! before -%c",
				   opt2char(option));
		*invflg |= inverse_for_options[i];
	}
}

static void
print_num(uint64_t number, unsigned int format)
{
	if (format & FMT_KILOMEGAGIGA) {
		if (number > 99999) {
			number = (number + 500) / 1000;
			if (number > 9999) {
				number = (number + 500) / 1000;
				if (number > 9999) {
					number = (number + 500) / 1000;
					if (number > 9999) {
						number = (number + 500) / 1000;
						printf(FMT("%4lluT ","%lluT "), (unsigned long long)number);
					}
					else printf(FMT("%4lluG ","%lluG "), (unsigned long long)number);
				}
				else printf(FMT("%4lluM ","%lluM "), (unsigned long long)number);
			} else
				printf(FMT("%4lluK ","%lluK "), (unsigned long long)number);
		} else
			printf(FMT("%5llu ","%llu "), (unsigned long long)number);
	} else
		printf(FMT("%8llu ","%llu "), (unsigned long long)number);
}


static void
print_header(unsigned int format, const char *chain, struct ip6tc_handle *handle)
{
	struct ip6t_counters counters;
	const char *pol = ip6tc_get_policy(chain, &counters, handle);
	printf("Chain %s", chain);
	if (pol) {
		printf(" (policy %s", pol);
		if (!(format & FMT_NOCOUNTS)) {
			fputc(' ', stdout);
			print_num(counters.pcnt, (format|FMT_NOTABLE));
			fputs("packets, ", stdout);
			print_num(counters.bcnt, (format|FMT_NOTABLE));
			fputs("bytes", stdout);
		}
		printf(")\n");
	} else {
		unsigned int refs;
		if (!ip6tc_get_references(&refs, chain, handle))
			printf(" (ERROR obtaining refs)\n");
		else
			printf(" (%u references)\n", refs);
	}

	if (format & FMT_LINENUMBERS)
		printf(FMT("%-4s ", "%s "), "num");
	if (!(format & FMT_NOCOUNTS)) {
		if (format & FMT_KILOMEGAGIGA) {
			printf(FMT("%5s ","%s "), "pkts");
			printf(FMT("%5s ","%s "), "bytes");
		} else {
			printf(FMT("%8s ","%s "), "pkts");
			printf(FMT("%10s ","%s "), "bytes");
		}
	}
	if (!(format & FMT_NOTARGET))
		printf(FMT("%-9s ","%s "), "target");
	fputs(" prot ", stdout);
	if (format & FMT_OPTIONS)
		fputs("opt", stdout);
	if (format & FMT_VIA) {
		printf(FMT(" %-6s ","%s "), "in");
		printf(FMT("%-6s ","%s "), "out");
	}
	printf(FMT(" %-19s ","%s "), "source");
	printf(FMT(" %-19s "," %s "), "destination");
	printf("\n");
}


static int
print_match(const struct ip6t_entry_match *m,
	    const struct ip6t_ip6 *ip,
	    int numeric)
{
	const struct xtables_match *match =
		xtables_find_match(m->u.user.name, XTF_TRY_LOAD, NULL);

	if (match) {
		if (match->print)
			match->print(ip, m, numeric);
		else
			printf("%s ", match->name);
	} else {
		if (m->u.user.name[0])
			printf("UNKNOWN match `%s' ", m->u.user.name);
	}
	/* Don't stop iterating. */
	return 0;
}

/* e is called `fw' here for historical reasons */
static void
print_firewall(const struct ip6t_entry *fw,
	       const char *targname,
	       unsigned int num,
	       unsigned int format,
	       struct ip6tc_handle *const handle)
{
	const struct xtables_target *target = NULL;
	const struct ip6t_entry_target *t;
	char buf[BUFSIZ];

	if (!ip6tc_is_chain(targname, handle))
		target = xtables_find_target(targname, XTF_TRY_LOAD);
	else
		target = xtables_find_target(IP6T_STANDARD_TARGET,
		         XTF_LOAD_MUST_SUCCEED);

	t = ip6t_get_target((struct ip6t_entry *)fw);

	if (format & FMT_LINENUMBERS)
		printf(FMT("%-4u ", "%u "), num);

	if (!(format & FMT_NOCOUNTS)) {
		print_num(fw->counters.pcnt, format);
		print_num(fw->counters.bcnt, format);
	}

	if (!(format & FMT_NOTARGET))
		printf(FMT("%-9s ", "%s "), targname);

	fputc(fw->ipv6.invflags & IP6T_INV_PROTO ? '!' : ' ', stdout);
	{
		const char *pname = proto_to_name(fw->ipv6.proto, format&FMT_NUMERIC);
		if (pname)
			printf(FMT("%-5s", "%s "), pname);
		else
			printf(FMT("%-5hu", "%hu "), fw->ipv6.proto);
	}

	if (format & FMT_OPTIONS) {
		if (format & FMT_NOTABLE)
			fputs("opt ", stdout);
		fputc(' ', stdout); /* Invert flag of FRAG */
		fputc(' ', stdout); /* -f */
		fputc(' ', stdout);
	}

	if (format & FMT_VIA) {
		char iface[IFNAMSIZ+2];

		if (fw->ipv6.invflags & IP6T_INV_VIA_IN) {
			iface[0] = '!';
			iface[1] = '\0';
		}
		else iface[0] = '\0';

		if (fw->ipv6.iniface[0] != '\0') {
			strcat(iface, fw->ipv6.iniface);
		}
		else if (format & FMT_NUMERIC) strcat(iface, "*");
		else strcat(iface, "any");
		printf(FMT(" %-6s ","in %s "), iface);

		if (fw->ipv6.invflags & IP6T_INV_VIA_OUT) {
			iface[0] = '!';
			iface[1] = '\0';
		}
		else iface[0] = '\0';

		if (fw->ipv6.outiface[0] != '\0') {
			strcat(iface, fw->ipv6.outiface);
		}
		else if (format & FMT_NUMERIC) strcat(iface, "*");
		else strcat(iface, "any");
		printf(FMT("%-6s ","out %s "), iface);
	}

	fputc(fw->ipv6.invflags & IP6T_INV_SRCIP ? '!' : ' ', stdout);
	if (!memcmp(&fw->ipv6.smsk, &in6addr_any, sizeof in6addr_any)
	    && !(format & FMT_NUMERIC))
		printf(FMT("%-19s ","%s "), "anywhere");
	else {
		if (format & FMT_NUMERIC)
			strcpy(buf, xtables_ip6addr_to_numeric(&fw->ipv6.src));
		else
			strcpy(buf, xtables_ip6addr_to_anyname(&fw->ipv6.src));
		strcat(buf, xtables_ip6mask_to_numeric(&fw->ipv6.smsk));
		printf(FMT("%-19s ","%s "), buf);
	}

	fputc(fw->ipv6.invflags & IP6T_INV_DSTIP ? '!' : ' ', stdout);
	if (!memcmp(&fw->ipv6.dmsk, &in6addr_any, sizeof in6addr_any)
	    && !(format & FMT_NUMERIC))
		printf(FMT("%-19s ","-> %s"), "anywhere");
	else {
		if (format & FMT_NUMERIC)
			strcpy(buf, xtables_ip6addr_to_numeric(&fw->ipv6.dst));
		else
			strcpy(buf, xtables_ip6addr_to_anyname(&fw->ipv6.dst));
		strcat(buf, xtables_ip6mask_to_numeric(&fw->ipv6.dmsk));
		printf(FMT("%-19s ","-> %s"), buf);
	}

	if (format & FMT_NOTABLE)
		fputs("  ", stdout);

#ifdef IP6T_F_GOTO
	if(fw->ipv6.flags & IP6T_F_GOTO)
		printf("[goto] ");
#endif

	IP6T_MATCH_ITERATE(fw, print_match, &fw->ipv6, format & FMT_NUMERIC);

	if (target) {
		if (target->print)
			/* Print the target information. */
			target->print(&fw->ipv6, t, format & FMT_NUMERIC);
	} else if (t->u.target_size != sizeof(*t))
		printf("[%u bytes of unknown target data] ",
		       (unsigned int)(t->u.target_size - sizeof(*t)));

	if (!(format & FMT_NONEWLINE))
		fputc('\n', stdout);
}

static void
print_firewall_line(const struct ip6t_entry *fw,
		    struct ip6tc_handle *const h)
{
	struct ip6t_entry_target *t;

	t = ip6t_get_target((struct ip6t_entry *)fw);
	print_firewall(fw, t->u.user.name, 0, FMT_PRINT_RULE, h);
}

static int
append_entry(const ip6t_chainlabel chain,
	     struct ip6t_entry *fw,
	     unsigned int nsaddrs,
	     const struct in6_addr saddrs[],
	     const struct in6_addr smasks[],
	     unsigned int ndaddrs,
	     const struct in6_addr daddrs[],
	     const struct in6_addr dmasks[],
	     int verbose,
	     struct ip6tc_handle *handle)
{
	unsigned int i, j;
	int ret = 1;

	for (i = 0; i < nsaddrs; i++) {
		fw->ipv6.src = saddrs[i];
		fw->ipv6.smsk = smasks[i];
		for (j = 0; j < ndaddrs; j++) {
			fw->ipv6.dst = daddrs[j];
			fw->ipv6.dmsk = dmasks[j];
			if (verbose)
				print_firewall_line(fw, handle);
			ret &= ip6tc_append_entry(chain, fw, handle);
		}
	}

	return ret;
}

static int
replace_entry(const ip6t_chainlabel chain,
	      struct ip6t_entry *fw,
	      unsigned int rulenum,
	      const struct in6_addr *saddr, const struct in6_addr *smask,
	      const struct in6_addr *daddr, const struct in6_addr *dmask,
	      int verbose,
	      struct ip6tc_handle *handle)
{
	fw->ipv6.src = *saddr;
	fw->ipv6.dst = *daddr;
	fw->ipv6.smsk = *smask;
	fw->ipv6.dmsk = *dmask;

	if (verbose)
		print_firewall_line(fw, handle);
	return ip6tc_replace_entry(chain, fw, rulenum, handle);
}

static int
insert_entry(const ip6t_chainlabel chain,
	     struct ip6t_entry *fw,
	     unsigned int rulenum,
	     unsigned int nsaddrs,
	     const struct in6_addr saddrs[],
	     const struct in6_addr smasks[],
	     unsigned int ndaddrs,
	     const struct in6_addr daddrs[],
	     const struct in6_addr dmasks[],
	     int verbose,
	     struct ip6tc_handle *handle)
{
	unsigned int i, j;
	int ret = 1;

	for (i = 0; i < nsaddrs; i++) {
		fw->ipv6.src = saddrs[i];
		fw->ipv6.smsk = smasks[i];
		for (j = 0; j < ndaddrs; j++) {
			fw->ipv6.dst = daddrs[j];
			fw->ipv6.dmsk = dmasks[j];
			if (verbose)
				print_firewall_line(fw, handle);
			ret &= ip6tc_insert_entry(chain, fw, rulenum, handle);
		}
	}

	return ret;
}

static unsigned char *
make_delete_mask(const struct xtables_rule_match *matches,
		 const struct xtables_target *target)
{
	/* Establish mask for comparison */
	unsigned int size;
	const struct xtables_rule_match *matchp;
	unsigned char *mask, *mptr;

	size = sizeof(struct ip6t_entry);
	for (matchp = matches; matchp; matchp = matchp->next)
		size += XT_ALIGN(sizeof(struct ip6t_entry_match)) + matchp->match->size;

	mask = xtables_calloc(1, size
			 + XT_ALIGN(sizeof(struct ip6t_entry_target))
			 + target->size);

	memset(mask, 0xFF, sizeof(struct ip6t_entry));
	mptr = mask + sizeof(struct ip6t_entry);

	for (matchp = matches; matchp; matchp = matchp->next) {
		memset(mptr, 0xFF,
		       XT_ALIGN(sizeof(struct ip6t_entry_match))
		       + matchp->match->userspacesize);
		mptr += XT_ALIGN(sizeof(struct ip6t_entry_match)) + matchp->match->size;
	}

	memset(mptr, 0xFF,
	       XT_ALIGN(sizeof(struct ip6t_entry_target))
	       + target->userspacesize);

	return mask;
}

static int
delete_entry(const ip6t_chainlabel chain,
	     struct ip6t_entry *fw,
	     unsigned int nsaddrs,
	     const struct in6_addr saddrs[],
	     const struct in6_addr smasks[],
	     unsigned int ndaddrs,
	     const struct in6_addr daddrs[],
	     const struct in6_addr dmasks[],
	     int verbose,
	     struct ip6tc_handle *handle,
	     struct xtables_rule_match *matches,
	     const struct xtables_target *target)
{
	unsigned int i, j;
	int ret = 1;
	unsigned char *mask;

	mask = make_delete_mask(matches, target);
	for (i = 0; i < nsaddrs; i++) {
		fw->ipv6.src = saddrs[i];
		fw->ipv6.smsk = smasks[i];
		for (j = 0; j < ndaddrs; j++) {
			fw->ipv6.dst = daddrs[j];
			fw->ipv6.dmsk = dmasks[j];
			if (verbose)
				print_firewall_line(fw, handle);
			ret &= ip6tc_delete_entry(chain, fw, mask, handle);
		}
	}
	free(mask);

	return ret;
}

static int
check_entry(const ip6t_chainlabel chain, struct ip6t_entry *fw,
	    unsigned int nsaddrs, const struct in6_addr *saddrs,
	    const struct in6_addr *smasks, unsigned int ndaddrs,
	    const struct in6_addr *daddrs, const struct in6_addr *dmasks,
	    bool verbose, struct ip6tc_handle *handle,
	    struct xtables_rule_match *matches,
	    const struct xtables_target *target)
{
	unsigned int i, j;
	int ret = 1;
	unsigned char *mask;

	mask = make_delete_mask(matches, target);
	for (i = 0; i < nsaddrs; i++) {
		fw->ipv6.src = saddrs[i];
		fw->ipv6.smsk = smasks[i];
		for (j = 0; j < ndaddrs; j++) {
			fw->ipv6.dst = daddrs[j];
			fw->ipv6.dmsk = dmasks[j];
			if (verbose)
				print_firewall_line(fw, handle);
			ret &= ip6tc_check_entry(chain, fw, mask, handle);
		}
	}

	free(mask);
	return ret;
}

int
for_each_chain6(int (*fn)(const ip6t_chainlabel, int, struct ip6tc_handle *),
	       int verbose, int builtinstoo, struct ip6tc_handle *handle)
{
	int ret = 1;
	const char *chain;
	char *chains;
	unsigned int i, chaincount = 0;

	chain = ip6tc_first_chain(handle);
	while (chain) {
		chaincount++;
		chain = ip6tc_next_chain(handle);
	}

	chains = xtables_malloc(sizeof(ip6t_chainlabel) * chaincount);
	i = 0;
	chain = ip6tc_first_chain(handle);
	while (chain) {
		strcpy(chains + i*sizeof(ip6t_chainlabel), chain);
		i++;
		chain = ip6tc_next_chain(handle);
	}

	for (i = 0; i < chaincount; i++) {
		if (!builtinstoo
		    && ip6tc_builtin(chains + i*sizeof(ip6t_chainlabel),
				    handle) == 1)
			continue;
		ret &= fn(chains + i*sizeof(ip6t_chainlabel), verbose, handle);
	}

	free(chains);
	return ret;
}

int
flush_entries6(const ip6t_chainlabel chain, int verbose,
	      struct ip6tc_handle *handle)
{
	if (!chain)
		return for_each_chain6(flush_entries6, verbose, 1, handle);

	if (verbose)
		fprintf(stdout, "Flushing chain `%s'\n", chain);
	return ip6tc_flush_entries(chain, handle);
}

static int
zero_entries(const ip6t_chainlabel chain, int verbose,
	     struct ip6tc_handle *handle)
{
	if (!chain)
		return for_each_chain6(zero_entries, verbose, 1, handle);

	if (verbose)
		fprintf(stdout, "Zeroing chain `%s'\n", chain);
	return ip6tc_zero_entries(chain, handle);
}

int
delete_chain6(const ip6t_chainlabel chain, int verbose,
	     struct ip6tc_handle *handle)
{
	if (!chain)
		return for_each_chain6(delete_chain6, verbose, 0, handle);

	if (verbose)
		fprintf(stdout, "Deleting chain `%s'\n", chain);
	return ip6tc_delete_chain(chain, handle);
}

static int
list_entries(const ip6t_chainlabel chain, int rulenum, int verbose, int numeric,
	     int expanded, int linenumbers, struct ip6tc_handle *handle)
{
	int found = 0;
	unsigned int format;
	const char *this;

	format = FMT_OPTIONS;
	if (!verbose)
		format |= FMT_NOCOUNTS;
	else
		format |= FMT_VIA;

	if (numeric)
		format |= FMT_NUMERIC;

	if (!expanded)
		format |= FMT_KILOMEGAGIGA;

	if (linenumbers)
		format |= FMT_LINENUMBERS;

	for (this = ip6tc_first_chain(handle);
	     this;
	     this = ip6tc_next_chain(handle)) {
		const struct ip6t_entry *i;
		unsigned int num;

		if (chain && strcmp(chain, this) != 0)
			continue;

		if (found) printf("\n");

		if (!rulenum)
		    print_header(format, this, handle);
		i = ip6tc_first_rule(this, handle);

		num = 0;
		while (i) {
			num++;
			if (!rulenum || num == rulenum)
				print_firewall(i,
					       ip6tc_get_target(i, handle),
					       num,
					       format,
					       handle);
			i = ip6tc_next_rule(i, handle);
		}
		found = 1;
	}

	errno = ENOENT;
	return found;
}

/* This assumes that mask is contiguous, and byte-bounded. */
static void
print_iface(char letter, const char *iface, const unsigned char *mask,
	    int invert)
{
	unsigned int i;

	if (mask[0] == 0)
		return;

	printf("%s -%c ", invert ? " !" : "", letter);

	for (i = 0; i < IFNAMSIZ; i++) {
		if (mask[i] != 0) {
			if (iface[i] != '\0')
				printf("%c", iface[i]);
		} else {
			/* we can access iface[i-1] here, because
			 * a few lines above we make sure that mask[0] != 0 */
			if (iface[i-1] != '\0')
				printf("+");
			break;
		}
	}
}

/* The ip6tables looks up the /etc/protocols. */
static void print_proto(uint16_t proto, int invert)
{
	if (proto) {
		unsigned int i;
		const char *invertstr = invert ? " !" : "";

		const struct protoent *pent = getprotobynumber(proto);
		if (pent) {
			printf("%s -p %s",
			       invertstr, pent->p_name);
			return;
		}

		for (i = 0; xtables_chain_protos[i].name != NULL; ++i)
			if (xtables_chain_protos[i].num == proto) {
				printf("%s -p %s",
				       invertstr, xtables_chain_protos[i].name);
				return;
			}

		printf("%s -p %u", invertstr, proto);
	}
}

static int print_match_save(const struct ip6t_entry_match *e,
			const struct ip6t_ip6 *ip)
{
	const struct xtables_match *match =
		xtables_find_match(e->u.user.name, XTF_TRY_LOAD, NULL);

	if (match) {
		printf(" -m %s", e->u.user.name);

		/* some matches don't provide a save function */
		if (match->save)
			match->save(ip, e);
	} else {
		if (e->u.match_size) {
			fprintf(stderr,
				"Can't find library for match `%s'\n",
				e->u.user.name);
			exit(1);
		}
	}
	return 0;
}

/* print a given ip including mask if neccessary */
static void print_ip(const char *prefix, const struct in6_addr *ip,
		     const struct in6_addr *mask, int invert)
{
	char buf[51];
	int l = ipv6_prefix_length(mask);

	if (l == 0 && !invert)
		return;

	printf("%s %s %s",
		invert ? " !" : "",
		prefix,
		inet_ntop(AF_INET6, ip, buf, sizeof buf));

	if (l == -1)
		printf("/%s", inet_ntop(AF_INET6, mask, buf, sizeof buf));
	else
		printf("/%d", l);
}

/* We want this to be readable, so only print out neccessary fields.
 * Because that's the kind of world I want to live in.  */
void print_rule6(const struct ip6t_entry *e,
		       struct ip6tc_handle *h, const char *chain, int counters)
{
	const struct ip6t_entry_target *t;
	const char *target_name;

	/* print counters for iptables-save */
	if (counters > 0)
		printf("[%llu:%llu] ", (unsigned long long)e->counters.pcnt, (unsigned long long)e->counters.bcnt);

	/* print chain name */
	printf("-A %s", chain);

	/* Print IP part. */
	print_ip("-s", &(e->ipv6.src), &(e->ipv6.smsk),
			e->ipv6.invflags & IP6T_INV_SRCIP);

	print_ip("-d", &(e->ipv6.dst), &(e->ipv6.dmsk),
			e->ipv6.invflags & IP6T_INV_DSTIP);

	print_iface('i', e->ipv6.iniface, e->ipv6.iniface_mask,
		    e->ipv6.invflags & IP6T_INV_VIA_IN);

	print_iface('o', e->ipv6.outiface, e->ipv6.outiface_mask,
		    e->ipv6.invflags & IP6T_INV_VIA_OUT);

	print_proto(e->ipv6.proto, e->ipv6.invflags & IP6T_INV_PROTO);

#if 0
	/* not definied in ipv6
	 * FIXME: linux/netfilter_ipv6/ip6_tables: IP6T_INV_FRAG why definied? */
	if (e->ipv6.flags & IPT_F_FRAG)
		printf("%s -f",
		       e->ipv6.invflags & IP6T_INV_FRAG ? " !" : "");
#endif

	if (e->ipv6.flags & IP6T_F_TOS)
		printf("%s -? %d",
		       e->ipv6.invflags & IP6T_INV_TOS ? " !" : "",
		       e->ipv6.tos);

	/* Print matchinfo part */
	if (e->target_offset) {
		IP6T_MATCH_ITERATE(e, print_match_save, &e->ipv6);
	}

	/* print counters for iptables -R */
	if (counters < 0)
		printf(" -c %llu %llu", (unsigned long long)e->counters.pcnt, (unsigned long long)e->counters.bcnt);

	/* Print target name */
	target_name = ip6tc_get_target(e, h);
	if (target_name && (*target_name != '\0'))
#ifdef IP6T_F_GOTO
		printf(" -%c %s", e->ipv6.flags & IP6T_F_GOTO ? 'g' : 'j', target_name);
#else
		printf(" -j %s", target_name);
#endif

	/* Print targinfo part */
	t = ip6t_get_target((struct ip6t_entry *)e);
	if (t->u.user.name[0]) {
		struct xtables_target *target =
			xtables_find_target(t->u.user.name, XTF_TRY_LOAD);

		if (!target) {
			fprintf(stderr, "Can't find library for target `%s'\n",
				t->u.user.name);
			exit(1);
		}

		if (target->save)
			target->save(&e->ipv6, t);
		else {
			/* If the target size is greater than ip6t_entry_target
			 * there is something to be saved, we just don't know
			 * how to print it */
			if (t->u.target_size !=
			    sizeof(struct ip6t_entry_target)) {
				fprintf(stderr, "Target `%s' is missing "
						"save function\n",
					t->u.user.name);
				exit(1);
			}
		}
	}
	printf("\n");
}

static int
list_rules(const ip6t_chainlabel chain, int rulenum, int counters,
	     struct ip6tc_handle *handle)
{
	const char *this = NULL;
	int found = 0;

	if (counters)
	    counters = -1;		/* iptables -c format */

	/* Dump out chain names first,
	 * thereby preventing dependency conflicts */
	if (!rulenum) for (this = ip6tc_first_chain(handle);
	     this;
	     this = ip6tc_next_chain(handle)) {
		if (chain && strcmp(this, chain) != 0)
			continue;

		if (ip6tc_builtin(this, handle)) {
			struct ip6t_counters count;
			printf("-P %s %s", this, ip6tc_get_policy(this, &count, handle));
			if (counters)
			    printf(" -c %llu %llu", (unsigned long long)count.pcnt, (unsigned long long)count.bcnt);
			printf("\n");
		} else {
			printf("-N %s\n", this);
		}
	}

	for (this = ip6tc_first_chain(handle);
	     this;
	     this = ip6tc_next_chain(handle)) {
		const struct ip6t_entry *e;
		int num = 0;

		if (chain && strcmp(this, chain) != 0)
			continue;

		/* Dump out rules */
		e = ip6tc_first_rule(this, handle);
		while(e) {
			num++;
			if (!rulenum || num == rulenum)
			    print_rule6(e, handle, this, counters);
			e = ip6tc_next_rule(e, handle);
		}
		found = 1;
	}

	errno = ENOENT;
	return found;
}

static struct ip6t_entry *
generate_entry(const struct ip6t_entry *fw,
	       struct xtables_rule_match *matches,
	       struct ip6t_entry_target *target)
{
	unsigned int size;
	struct xtables_rule_match *matchp;
	struct ip6t_entry *e;

	size = sizeof(struct ip6t_entry);
	for (matchp = matches; matchp; matchp = matchp->next)
		size += matchp->match->m->u.match_size;

	e = xtables_malloc(size + target->u.target_size);
	*e = *fw;
	e->target_offset = size;
	e->next_offset = size + target->u.target_size;

	size = 0;
	for (matchp = matches; matchp; matchp = matchp->next) {
		memcpy(e->elems + size, matchp->match->m, matchp->match->m->u.match_size);
		size += matchp->match->m->u.match_size;
	}
	memcpy(e->elems + size, target, target->u.target_size);

	return e;
}

static void clear_rule_matches(struct xtables_rule_match **matches)
{
	struct xtables_rule_match *matchp, *tmp;

	for (matchp = *matches; matchp;) {
		tmp = matchp->next;
		if (matchp->match->m) {
			free(matchp->match->m);
			matchp->match->m = NULL;
		}
		if (matchp->match == matchp->match->next) {
			free(matchp->match);
			matchp->match = NULL;
		}
		free(matchp);
		matchp = tmp;
	}

	*matches = NULL;
}

static void command_jump(struct iptables_command_state *cs)
{
	size_t size;

	set_option(&cs->options, OPT_JUMP, &cs->fw6.ipv6.invflags, cs->invert);
	cs->jumpto = parse_target(optarg);
	/* TRY_LOAD (may be chain name) */
	cs->target = xtables_find_target(cs->jumpto, XTF_TRY_LOAD);

	if (cs->target == NULL)
		return;

	size = XT_ALIGN(sizeof(struct ip6t_entry_target)) + cs->target->size;

	cs->target->t = xtables_calloc(1, size);
	cs->target->t->u.target_size = size;
	strcpy(cs->target->t->u.user.name, cs->jumpto);
	cs->target->t->u.user.revision = cs->target->revision;
	if (cs->target->init != NULL)
		cs->target->init(cs->target->t);
	if (cs->target->x6_options != NULL)
		opts = xtables_options_xfrm(ip6tables_globals.orig_opts, opts,
					    cs->target->x6_options,
					    &cs->target->option_offset);
	else
		opts = xtables_merge_options(ip6tables_globals.orig_opts, opts,
					     cs->target->extra_opts,
					     &cs->target->option_offset);
	if (opts == NULL)
		xtables_error(OTHER_PROBLEM, "can't alloc memory!");
}

static void command_match(struct iptables_command_state *cs)
{
	struct xtables_match *m;
	size_t size;

	if (cs->invert)
		xtables_error(PARAMETER_PROBLEM,
			   "unexpected ! flag before --match");

	m = xtables_find_match(optarg, XTF_LOAD_MUST_SUCCEED, &cs->matches);
	size = XT_ALIGN(sizeof(struct ip6t_entry_match)) + m->size;
	m->m = xtables_calloc(1, size);
	m->m->u.match_size = size;
	strcpy(m->m->u.user.name, m->name);
	m->m->u.user.revision = m->revision;
	if (m->init != NULL)
		m->init(m->m);
	if (m == m->next)
		return;
	/* Merge options for non-cloned matches */
	if (m->x6_options != NULL)
		opts = xtables_options_xfrm(ip6tables_globals.orig_opts, opts,
					    m->x6_options, &m->option_offset);
	else if (m->extra_opts != NULL)
		opts = xtables_merge_options(ip6tables_globals.orig_opts, opts,
					     m->extra_opts, &m->option_offset);
}

int do_command6(int argc, char *argv[], char **table, struct ip6tc_handle **handle)
{
	struct iptables_command_state cs;
	struct ip6t_entry *e = NULL;
	unsigned int nsaddrs = 0, ndaddrs = 0;
	struct in6_addr *saddrs = NULL, *daddrs = NULL;
	struct in6_addr *smasks = NULL, *dmasks = NULL;

	int verbose = 0;
	const char *chain = NULL;
	const char *shostnetworkmask = NULL, *dhostnetworkmask = NULL;
	const char *policy = NULL, *newname = NULL;
	unsigned int rulenum = 0, command = 0;
	const char *pcnt = NULL, *bcnt = NULL;
	int ret = 1;
	struct xtables_match *m;
	struct xtables_rule_match *matchp;
	struct xtables_target *t;
	unsigned long long cnt;

	memset(&cs, 0, sizeof(cs));
	cs.jumpto = "";
	cs.argv = argv;

	/* re-set optind to 0 in case do_command6 gets called
	 * a second time */
	optind = 0;

	/* clear mflags in case do_command6 gets called a second time
	 * (we clear the global list of all matches for security)*/
	for (m = xtables_matches; m; m = m->next)
		m->mflags = 0;

	for (t = xtables_targets; t; t = t->next) {
		t->tflags = 0;
		t->used = 0;
	}

	/* Suppress error messages: we may add new options if we
           demand-load a protocol. */
	opterr = 0;

	opts = xt_params->orig_opts;
	while ((cs.c = getopt_long(argc, argv,
	   "-:A:C:D:R:I:L::S::M:F::Z::N:X::E:P:Vh::o:p:s:d:j:i:bvnt:m:xc:g:46",
					   opts, NULL)) != -1) {
		switch (cs.c) {
			/*
			 * Command selection
			 */
		case 'A':
			add_command(&command, CMD_APPEND, CMD_NONE,
				    cs.invert);
			chain = optarg;
			break;

		case 'C':
			add_command(&command, CMD_CHECK, CMD_NONE,
			            cs.invert);
			chain = optarg;
			break;

		case 'D':
			add_command(&command, CMD_DELETE, CMD_NONE,
				    cs.invert);
			chain = optarg;
			if (optind < argc && argv[optind][0] != '-'
			    && argv[optind][0] != '!') {
				rulenum = parse_rulenumber(argv[optind++]);
				command = CMD_DELETE_NUM;
			}
			break;

		case 'R':
			add_command(&command, CMD_REPLACE, CMD_NONE,
				    cs.invert);
			chain = optarg;
			if (optind < argc && argv[optind][0] != '-'
			    && argv[optind][0] != '!')
				rulenum = parse_rulenumber(argv[optind++]);
			else
				xtables_error(PARAMETER_PROBLEM,
					   "-%c requires a rule number",
					   cmd2char(CMD_REPLACE));
			break;

		case 'I':
			add_command(&command, CMD_INSERT, CMD_NONE,
				    cs.invert);
			chain = optarg;
			if (optind < argc && argv[optind][0] != '-'
			    && argv[optind][0] != '!')
				rulenum = parse_rulenumber(argv[optind++]);
			else rulenum = 1;
			break;

		case 'L':
			add_command(&command, CMD_LIST,
				    CMD_ZERO | CMD_ZERO_NUM, cs.invert);
			if (optarg) chain = optarg;
			else if (optind < argc && argv[optind][0] != '-'
				 && argv[optind][0] != '!')
				chain = argv[optind++];
			if (optind < argc && argv[optind][0] != '-'
			    && argv[optind][0] != '!')
				rulenum = parse_rulenumber(argv[optind++]);
			break;

		case 'S':
			add_command(&command, CMD_LIST_RULES,
				    CMD_ZERO | CMD_ZERO_NUM, cs.invert);
			if (optarg) chain = optarg;
			else if (optind < argc && argv[optind][0] != '-'
				 && argv[optind][0] != '!')
				chain = argv[optind++];
			if (optind < argc && argv[optind][0] != '-'
			    && argv[optind][0] != '!')
				rulenum = parse_rulenumber(argv[optind++]);
			break;

		case 'F':
			add_command(&command, CMD_FLUSH, CMD_NONE,
				    cs.invert);
			if (optarg) chain = optarg;
			else if (optind < argc && argv[optind][0] != '-'
				 && argv[optind][0] != '!')
				chain = argv[optind++];
			break;

		case 'Z':
			add_command(&command, CMD_ZERO, CMD_LIST|CMD_LIST_RULES,
				    cs.invert);
			if (optarg) chain = optarg;
			else if (optind < argc && argv[optind][0] != '-'
				&& argv[optind][0] != '!')
				chain = argv[optind++];
			if (optind < argc && argv[optind][0] != '-'
				&& argv[optind][0] != '!') {
				rulenum = parse_rulenumber(argv[optind++]);
				command = CMD_ZERO_NUM;
			}
			break;

		case 'N':
			if (optarg && (*optarg == '-' || *optarg == '!'))
				xtables_error(PARAMETER_PROBLEM,
					   "chain name not allowed to start "
					   "with `%c'\n", *optarg);
			if (xtables_find_target(optarg, XTF_TRY_LOAD))
				xtables_error(PARAMETER_PROBLEM,
					   "chain name may not clash "
					   "with target name\n");
			add_command(&command, CMD_NEW_CHAIN, CMD_NONE,
				    cs.invert);
			chain = optarg;
			break;

		case 'X':
			add_command(&command, CMD_DELETE_CHAIN, CMD_NONE,
				    cs.invert);
			if (optarg) chain = optarg;
			else if (optind < argc && argv[optind][0] != '-'
				 && argv[optind][0] != '!')
				chain = argv[optind++];
			break;

		case 'E':
			add_command(&command, CMD_RENAME_CHAIN, CMD_NONE,
				    cs.invert);
			chain = optarg;
			if (optind < argc && argv[optind][0] != '-'
			    && argv[optind][0] != '!')
				newname = argv[optind++];
			else
				xtables_error(PARAMETER_PROBLEM,
					   "-%c requires old-chain-name and "
					   "new-chain-name",
					    cmd2char(CMD_RENAME_CHAIN));
			break;

		case 'P':
			add_command(&command, CMD_SET_POLICY, CMD_NONE,
				    cs.invert);
			chain = optarg;
			if (optind < argc && argv[optind][0] != '-'
			    && argv[optind][0] != '!')
				policy = argv[optind++];
			else
				xtables_error(PARAMETER_PROBLEM,
					   "-%c requires a chain and a policy",
					   cmd2char(CMD_SET_POLICY));
			break;

		case 'h':
			if (!optarg)
				optarg = argv[optind];

			/* ip6tables -p icmp -h */
			if (!cs.matches && cs.protocol)
				xtables_find_match(cs.protocol, XTF_TRY_LOAD,
					&cs.matches);

			exit_printhelp(cs.matches);

			/*
			 * Option selection
			 */
		case 'p':
			xtables_check_inverse(optarg, &cs.invert, &optind, argc, argv);
			set_option(&cs.options, OPT_PROTOCOL, &cs.fw6.ipv6.invflags,
				   cs.invert);

			/* Canonicalize into lower case */
			for (cs.protocol = optarg; *cs.protocol; cs.protocol++)
				*cs.protocol = tolower(*cs.protocol);

			cs.protocol = optarg;
			cs.fw6.ipv6.proto = xtables_parse_protocol(cs.protocol);
			cs.fw6.ipv6.flags |= IP6T_F_PROTO;

			if (cs.fw6.ipv6.proto == 0
			    && (cs.fw6.ipv6.invflags & IP6T_INV_PROTO))
				xtables_error(PARAMETER_PROBLEM,
					   "rule would never match protocol");

			if (is_exthdr(cs.fw6.ipv6.proto)
			    && (cs.fw6.ipv6.invflags & IP6T_INV_PROTO) == 0)
				fprintf(stderr,
					"Warning: never matched protocol: %s. "
					"use extension match instead.\n",
					cs.protocol);
			break;

		case 's':
			xtables_check_inverse(optarg, &cs.invert, &optind, argc, argv);
			set_option(&cs.options, OPT_SOURCE, &cs.fw6.ipv6.invflags,
				   cs.invert);
			shostnetworkmask = optarg;
			break;

		case 'd':
			xtables_check_inverse(optarg, &cs.invert, &optind, argc, argv);
			set_option(&cs.options, OPT_DESTINATION, &cs.fw6.ipv6.invflags,
				   cs.invert);
			dhostnetworkmask = optarg;
			break;

#ifdef IP6T_F_GOTO
		case 'g':
			set_option(&cs.options, OPT_JUMP, &cs.fw6.ipv6.invflags,
					cs.invert);
			cs.fw6.ipv6.flags |= IP6T_F_GOTO;
			cs.jumpto = parse_target(optarg);
			break;
#endif

		case 'j':
			command_jump(&cs);
			break;


		case 'i':
			if (*optarg == '\0')
				xtables_error(PARAMETER_PROBLEM,
					"Empty interface is likely to be "
					"undesired");
			xtables_check_inverse(optarg, &cs.invert, &optind, argc, argv);
			set_option(&cs.options, OPT_VIANAMEIN, &cs.fw6.ipv6.invflags,
				   cs.invert);
			xtables_parse_interface(optarg,
					cs.fw6.ipv6.iniface,
					cs.fw6.ipv6.iniface_mask);
			break;

		case 'o':
			if (*optarg == '\0')
				xtables_error(PARAMETER_PROBLEM,
					"Empty interface is likely to be "
					"undesired");
			xtables_check_inverse(optarg, &cs.invert, &optind, argc, argv);
			set_option(&cs.options, OPT_VIANAMEOUT, &cs.fw6.ipv6.invflags,
				   cs.invert);
			xtables_parse_interface(optarg,
					cs.fw6.ipv6.outiface,
					cs.fw6.ipv6.outiface_mask);
			break;

		case 'v':
			if (!verbose)
				set_option(&cs.options, OPT_VERBOSE,
					   &cs.fw6.ipv6.invflags, cs.invert);
			verbose++;
			break;

		case 'm':
			command_match(&cs);
			break;

		case 'n':
			set_option(&cs.options, OPT_NUMERIC, &cs.fw6.ipv6.invflags,
				   cs.invert);
			break;

		case 't':
			if (cs.invert)
				xtables_error(PARAMETER_PROBLEM,
					   "unexpected ! flag before --table");
			*table = optarg;
			break;

		case 'x':
			set_option(&cs.options, OPT_EXPANDED, &cs.fw6.ipv6.invflags,
				   cs.invert);
			break;

		case 'V':
			if (cs.invert)
				printf("Not %s ;-)\n", prog_vers);
			else
				printf("%s v%s\n",
				       prog_name, prog_vers);
			exit(0);

		case '0':
			set_option(&cs.options, OPT_LINENUMBERS, &cs.fw6.ipv6.invflags,
				   cs.invert);
			break;

		case 'M':
			xtables_modprobe_program = optarg;
			break;

		case 'c':

			set_option(&cs.options, OPT_COUNTERS, &cs.fw6.ipv6.invflags,
				   cs.invert);
			pcnt = optarg;
			bcnt = strchr(pcnt + 1, ',');
			if (bcnt)
			    bcnt++;
			if (!bcnt && optind < argc && argv[optind][0] != '-'
			    && argv[optind][0] != '!')
				bcnt = argv[optind++];
			if (!bcnt)
				xtables_error(PARAMETER_PROBLEM,
					"-%c requires packet and byte counter",
					opt2char(OPT_COUNTERS));

			if (sscanf(pcnt, "%llu", &cnt) != 1)
				xtables_error(PARAMETER_PROBLEM,
					"-%c packet counter not numeric",
					opt2char(OPT_COUNTERS));
			cs.fw6.counters.pcnt = cnt;

			if (sscanf(bcnt, "%llu", &cnt) != 1)
				xtables_error(PARAMETER_PROBLEM,
					"-%c byte counter not numeric",
					opt2char(OPT_COUNTERS));
			cs.fw6.counters.bcnt = cnt;
			break;

		case '4':
			/* This is not the IPv4 iptables */
			if (line != -1)
				return 1; /* success: line ignored */
			fprintf(stderr, "This is the IPv6 version of ip6tables.\n");
			exit_tryhelp(2);

		case '6':
			/* This is indeed the IPv6 ip6tables */
			break;

		case 1: /* non option */
			if (optarg[0] == '!' && optarg[1] == '\0') {
				if (cs.invert)
					xtables_error(PARAMETER_PROBLEM,
						   "multiple consecutive ! not"
						   " allowed");
				cs.invert = TRUE;
				optarg[0] = '\0';
				continue;
			}
			fprintf(stderr, "Bad argument `%s'\n", optarg);
			exit_tryhelp(2);

		default:
			if (command_default(&cs, &ip6tables_globals) == 1)
				/*
				 * If new options were loaded, we must retry
				 * getopt immediately and not allow
				 * cs.invert=FALSE to be executed.
				 */
				continue;
			break;
		}
		cs.invert = FALSE;
	}

	for (matchp = cs.matches; matchp; matchp = matchp->next)
		xtables_option_mfcall(matchp->match);
	if (cs.target != NULL)
		xtables_option_tfcall(cs.target);

	/* Fix me: must put inverse options checking here --MN */

	if (optind < argc)
		xtables_error(PARAMETER_PROBLEM,
			   "unknown arguments found on commandline");
	if (!command)
		xtables_error(PARAMETER_PROBLEM, "no command specified");
	if (cs.invert)
		xtables_error(PARAMETER_PROBLEM,
			   "nothing appropriate following !");

	if (command & (CMD_REPLACE | CMD_INSERT | CMD_DELETE | CMD_APPEND | CMD_CHECK)) {
		if (!(cs.options & OPT_DESTINATION))
			dhostnetworkmask = "::0/0";
		if (!(cs.options & OPT_SOURCE))
			shostnetworkmask = "::0/0";
	}

	if (shostnetworkmask)
		xtables_ip6parse_multiple(shostnetworkmask, &saddrs,
					  &smasks, &nsaddrs);

	if (dhostnetworkmask)
		xtables_ip6parse_multiple(dhostnetworkmask, &daddrs,
					  &dmasks, &ndaddrs);

	if ((nsaddrs > 1 || ndaddrs > 1) &&
	    (cs.fw6.ipv6.invflags & (IP6T_INV_SRCIP | IP6T_INV_DSTIP)))
		xtables_error(PARAMETER_PROBLEM, "! not allowed with multiple"
			   " source or destination IP addresses");

	if (command == CMD_REPLACE && (nsaddrs != 1 || ndaddrs != 1))
		xtables_error(PARAMETER_PROBLEM, "Replacement rule does not "
			   "specify a unique address");

	generic_opt_check(command, cs.options);

	if (chain != NULL && strlen(chain) >= XT_EXTENSION_MAXNAMELEN)
		xtables_error(PARAMETER_PROBLEM,
			   "chain name `%s' too long (must be under %u chars)",
			   chain, XT_EXTENSION_MAXNAMELEN);

	/* only allocate handle if we weren't called with a handle */
	if (!*handle)
		*handle = ip6tc_init(*table);

	/* try to insmod the module if iptc_init failed */
	if (!*handle && xtables_load_ko(xtables_modprobe_program, false) != -1)
		*handle = ip6tc_init(*table);

	if (!*handle)
		xtables_error(VERSION_PROBLEM,
			"can't initialize ip6tables table `%s': %s",
			*table, ip6tc_strerror(errno));

	if (command == CMD_APPEND
	    || command == CMD_DELETE
	    || command == CMD_CHECK
	    || command == CMD_INSERT
	    || command == CMD_REPLACE) {
		if (strcmp(chain, "PREROUTING") == 0
		    || strcmp(chain, "INPUT") == 0) {
			/* -o not valid with incoming packets. */
			if (cs.options & OPT_VIANAMEOUT)
				xtables_error(PARAMETER_PROBLEM,
					   "Can't use -%c with %s\n",
					   opt2char(OPT_VIANAMEOUT),
					   chain);
		}

		if (strcmp(chain, "POSTROUTING") == 0
		    || strcmp(chain, "OUTPUT") == 0) {
			/* -i not valid with outgoing packets */
			if (cs.options & OPT_VIANAMEIN)
				xtables_error(PARAMETER_PROBLEM,
					   "Can't use -%c with %s\n",
					   opt2char(OPT_VIANAMEIN),
					   chain);
		}

		if (cs.target && ip6tc_is_chain(cs.jumpto, *handle)) {
			fprintf(stderr,
				"Warning: using chain %s, not extension\n",
				cs.jumpto);

			if (cs.target->t)
				free(cs.target->t);

			cs.target = NULL;
		}

		/* If they didn't specify a target, or it's a chain
		   name, use standard. */
		if (!cs.target
		    && (strlen(cs.jumpto) == 0
			|| ip6tc_is_chain(cs.jumpto, *handle))) {
			size_t size;

			cs.target = xtables_find_target(IP6T_STANDARD_TARGET,
					XTF_LOAD_MUST_SUCCEED);

			size = sizeof(struct ip6t_entry_target)
				+ cs.target->size;
			cs.target->t = xtables_calloc(1, size);
			cs.target->t->u.target_size = size;
			strcpy(cs.target->t->u.user.name, cs.jumpto);
			if (cs.target->init != NULL)
				cs.target->init(cs.target->t);
		}

		if (!cs.target) {
			/* it is no chain, and we can't load a plugin.
			 * We cannot know if the plugin is corrupt, non
			 * existant OR if the user just misspelled a
			 * chain. */
#ifdef IP6T_F_GOTO
			if (cs.fw6.ipv6.flags & IP6T_F_GOTO)
				xtables_error(PARAMETER_PROBLEM,
						"goto '%s' is not a chain\n",
						cs.jumpto);
#endif
			xtables_find_target(cs.jumpto, XTF_LOAD_MUST_SUCCEED);
		} else {
			e = generate_entry(&cs.fw6, cs.matches, cs.target->t);
			free(cs.target->t);
		}
	}

	switch (command) {
	case CMD_APPEND:
		ret = append_entry(chain, e,
				   nsaddrs, saddrs, smasks,
				   ndaddrs, daddrs, dmasks,
				   cs.options&OPT_VERBOSE,
				   *handle);
		break;
	case CMD_DELETE:
		ret = delete_entry(chain, e,
				   nsaddrs, saddrs, smasks,
				   ndaddrs, daddrs, dmasks,
				   cs.options&OPT_VERBOSE,
				   *handle, cs.matches, cs.target);
		break;
	case CMD_DELETE_NUM:
		ret = ip6tc_delete_num_entry(chain, rulenum - 1, *handle);
		break;
	case CMD_CHECK:
		ret = check_entry(chain, e,
				   nsaddrs, saddrs, smasks,
				   ndaddrs, daddrs, dmasks,
				   cs.options&OPT_VERBOSE,
				   *handle, cs.matches, cs.target);
		break;
	case CMD_REPLACE:
		ret = replace_entry(chain, e, rulenum - 1,
				    saddrs, smasks, daddrs, dmasks,
				    cs.options&OPT_VERBOSE, *handle);
		break;
	case CMD_INSERT:
		ret = insert_entry(chain, e, rulenum - 1,
				   nsaddrs, saddrs, smasks,
				   ndaddrs, daddrs, dmasks,
				   cs.options&OPT_VERBOSE,
				   *handle);
		break;
	case CMD_FLUSH:
		ret = flush_entries6(chain, cs.options&OPT_VERBOSE, *handle);
		break;
	case CMD_ZERO:
		ret = zero_entries(chain, cs.options&OPT_VERBOSE, *handle);
		break;
	case CMD_ZERO_NUM:
		ret = ip6tc_zero_counter(chain, rulenum, *handle);
		break;
	case CMD_LIST:
	case CMD_LIST|CMD_ZERO:
	case CMD_LIST|CMD_ZERO_NUM:
		ret = list_entries(chain,
				   rulenum,
				   cs.options&OPT_VERBOSE,
				   cs.options&OPT_NUMERIC,
				   cs.options&OPT_EXPANDED,
				   cs.options&OPT_LINENUMBERS,
				   *handle);
		if (ret && (command & CMD_ZERO))
			ret = zero_entries(chain,
					   cs.options&OPT_VERBOSE, *handle);
		if (ret && (command & CMD_ZERO_NUM))
			ret = ip6tc_zero_counter(chain, rulenum, *handle);
		break;
	case CMD_LIST_RULES:
	case CMD_LIST_RULES|CMD_ZERO:
	case CMD_LIST_RULES|CMD_ZERO_NUM:
		ret = list_rules(chain,
				   rulenum,
				   cs.options&OPT_VERBOSE,
				   *handle);
		if (ret && (command & CMD_ZERO))
			ret = zero_entries(chain,
					   cs.options&OPT_VERBOSE, *handle);
		if (ret && (command & CMD_ZERO_NUM))
			ret = ip6tc_zero_counter(chain, rulenum, *handle);
		break;
	case CMD_NEW_CHAIN:
		ret = ip6tc_create_chain(chain, *handle);
		break;
	case CMD_DELETE_CHAIN:
		ret = delete_chain6(chain, cs.options&OPT_VERBOSE, *handle);
		break;
	case CMD_RENAME_CHAIN:
		ret = ip6tc_rename_chain(chain, newname,	*handle);
		break;
	case CMD_SET_POLICY:
		ret = ip6tc_set_policy(chain, policy, cs.options&OPT_COUNTERS ? &cs.fw6.counters : NULL, *handle);
		break;
	default:
		/* We should never reach this... */
		exit_tryhelp(2);
	}

	if (verbose > 1)
		dump_entries6(*handle);

	clear_rule_matches(&cs.matches);

	if (e != NULL) {
		free(e);
		e = NULL;
	}

	free(saddrs);
	free(smasks);
	free(daddrs);
	free(dmasks);
	xtables_free_opts(1);

	return ret;
}
