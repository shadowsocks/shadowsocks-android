/* Code to restore the iptables state, from file by iptables-save.
 * (C) 2000-2002 by Harald Welte <laforge@gnumonks.org>
 * based on previous code from Rusty Russell <rusty@linuxcare.com.au>
 *
 * This code is distributed under the terms of GNU GPL v2
 */

#include <getopt.h>
#include <sys/errno.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "iptables.h"
#include "xtables.h"
#include "libiptc/libiptc.h"
#include "iptables-multi.h"

#ifdef DEBUG
#define DEBUGP(x, args...) fprintf(stderr, x, ## args)
#else
#define DEBUGP(x, args...)
#endif

static int binary = 0, counters = 0, verbose = 0, noflush = 0;

/* Keeping track of external matches and targets.  */
static const struct option options[] = {
	{.name = "binary",   .has_arg = false, .val = 'b'},
	{.name = "counters", .has_arg = false, .val = 'c'},
	{.name = "verbose",  .has_arg = false, .val = 'v'},
	{.name = "test",     .has_arg = false, .val = 't'},
	{.name = "help",     .has_arg = false, .val = 'h'},
	{.name = "noflush",  .has_arg = false, .val = 'n'},
	{.name = "modprobe", .has_arg = true,  .val = 'M'},
	{.name = "table",    .has_arg = true,  .val = 'T'},
	{NULL},
};

static void print_usage(const char *name, const char *version) __attribute__((noreturn));

#define prog_name iptables_globals.program_name

static void print_usage(const char *name, const char *version)
{
	fprintf(stderr, "Usage: %s [-b] [-c] [-v] [-t] [-h]\n"
			"	   [ --binary ]\n"
			"	   [ --counters ]\n"
			"	   [ --verbose ]\n"
			"	   [ --test ]\n"
			"	   [ --help ]\n"
			"	   [ --noflush ]\n"
			"	   [ --table=<TABLE> ]\n"
			"          [ --modprobe=<command>]\n", name);

	exit(1);
}

static struct iptc_handle *create_handle(const char *tablename)
{
	struct iptc_handle *handle;

	handle = iptc_init(tablename);

	if (!handle) {
		/* try to insmod the module if iptc_init failed */
		xtables_load_ko(xtables_modprobe_program, false);
		handle = iptc_init(tablename);
	}

	if (!handle) {
		xtables_error(PARAMETER_PROBLEM, "%s: unable to initialize "
			"table '%s'\n", prog_name, tablename);
		exit(1);
	}
	return handle;
}

static int parse_counters(char *string, struct ipt_counters *ctr)
{
	unsigned long long pcnt, bcnt;
	int ret;

	ret = sscanf(string, "[%llu:%llu]", &pcnt, &bcnt);
	ctr->pcnt = pcnt;
	ctr->bcnt = bcnt;
	return ret == 2;
}

/* global new argv and argc */
static char *newargv[255];
static int newargc;

/* function adding one argument to newargv, updating newargc 
 * returns true if argument added, false otherwise */
static int add_argv(char *what) {
	DEBUGP("add_argv: %s\n", what);
	if (what && newargc + 1 < ARRAY_SIZE(newargv)) {
		newargv[newargc] = strdup(what);
		newargc++;
		return 1;
	} else {
		xtables_error(PARAMETER_PROBLEM,
			"Parser cannot handle more arguments\n");
		return 0;
	}
}

static void free_argv(void) {
	int i;

	for (i = 0; i < newargc; i++)
		free(newargv[i]);
}

#ifdef IPTABLES_MULTI
int
iptables_restore_main(int argc, char *argv[])
#else
int
main(int argc, char *argv[])
#endif
{
	struct iptc_handle *handle = NULL;
	char buffer[10240];
	int c;
	char curtable[IPT_TABLE_MAXNAMELEN + 1];
	FILE *in;
	int in_table = 0, testing = 0;
	const char *tablename = NULL;

	line = 0;

	iptables_globals.program_name = "iptables-restore";
	c = xtables_init_all(&iptables_globals, NFPROTO_IPV4);
	if (c < 0) {
		fprintf(stderr, "%s/%s Failed to initialize xtables\n",
				iptables_globals.program_name,
				iptables_globals.program_version);
		exit(1);
	}
#if defined(ALL_INCLUSIVE) || defined(NO_SHARED_LIBS)
	init_extensions();
	init_extensions4();
#endif

	while ((c = getopt_long(argc, argv, "bcvthnM:T:", options, NULL)) != -1) {
		switch (c) {
			case 'b':
				binary = 1;
				break;
			case 'c':
				counters = 1;
				break;
			case 'v':
				verbose = 1;
				break;
			case 't':
				testing = 1;
				break;
			case 'h':
				print_usage("iptables-restore",
					    IPTABLES_VERSION);
				break;
			case 'n':
				noflush = 1;
				break;
			case 'M':
				xtables_modprobe_program = optarg;
				break;
			case 'T':
				tablename = optarg;
				break;
		}
	}

	if (optind == argc - 1) {
		in = fopen(argv[optind], "re");
		if (!in) {
			fprintf(stderr, "Can't open %s: %s\n", argv[optind],
				strerror(errno));
			exit(1);
		}
	}
	else if (optind < argc) {
		fprintf(stderr, "Unknown arguments found on commandline\n");
		exit(1);
	}
	else in = stdin;

	/* Grab standard input. */
	while (fgets(buffer, sizeof(buffer), in)) {
		int ret = 0;

		line++;
		if (buffer[0] == '\n')
			continue;
		else if (buffer[0] == '#') {
			if (verbose)
				fputs(buffer, stdout);
			continue;
		} else if ((strcmp(buffer, "COMMIT\n") == 0) && (in_table)) {
			if (!testing) {
				DEBUGP("Calling commit\n");
				ret = iptc_commit(handle);
				iptc_free(handle);
				handle = NULL;
			} else {
				DEBUGP("Not calling commit, testing\n");
				ret = 1;
			}
			in_table = 0;
		} else if ((buffer[0] == '*') && (!in_table)) {
			/* New table */
			char *table;

			table = strtok(buffer+1, " \t\n");
			DEBUGP("line %u, table '%s'\n", line, table);
			if (!table) {
				xtables_error(PARAMETER_PROBLEM,
					"%s: line %u table name invalid\n",
					prog_name, line);
				exit(1);
			}
			strncpy(curtable, table, IPT_TABLE_MAXNAMELEN);
			curtable[IPT_TABLE_MAXNAMELEN] = '\0';

			if (tablename && (strcmp(tablename, table) != 0))
				continue;
			if (handle)
				iptc_free(handle);

			handle = create_handle(table);
			if (noflush == 0) {
				DEBUGP("Cleaning all chains of table '%s'\n",
					table);
				for_each_chain4(flush_entries4, verbose, 1,
						handle);

				DEBUGP("Deleting all user-defined chains "
				       "of table '%s'\n", table);
				for_each_chain4(delete_chain4, verbose, 0,
						handle);
			}

			ret = 1;
			in_table = 1;

		} else if ((buffer[0] == ':') && (in_table)) {
			/* New chain. */
			char *policy, *chain;

			chain = strtok(buffer+1, " \t\n");
			DEBUGP("line %u, chain '%s'\n", line, chain);
			if (!chain) {
				xtables_error(PARAMETER_PROBLEM,
					   "%s: line %u chain name invalid\n",
					   prog_name, line);
				exit(1);
			}

			if (strlen(chain) >= XT_EXTENSION_MAXNAMELEN)
				xtables_error(PARAMETER_PROBLEM,
					   "Invalid chain name `%s' "
					   "(%u chars max)",
					   chain, XT_EXTENSION_MAXNAMELEN - 1);

			if (iptc_builtin(chain, handle) <= 0) {
				if (noflush && iptc_is_chain(chain, handle)) {
					DEBUGP("Flushing existing user defined chain '%s'\n", chain);
					if (!iptc_flush_entries(chain, handle))
						xtables_error(PARAMETER_PROBLEM,
							   "error flushing chain "
							   "'%s':%s\n", chain,
							   strerror(errno));
				} else {
					DEBUGP("Creating new chain '%s'\n", chain);
					if (!iptc_create_chain(chain, handle))
						xtables_error(PARAMETER_PROBLEM,
							   "error creating chain "
							   "'%s':%s\n", chain,
							   strerror(errno));
				}
			}

			policy = strtok(NULL, " \t\n");
			DEBUGP("line %u, policy '%s'\n", line, policy);
			if (!policy) {
				xtables_error(PARAMETER_PROBLEM,
					   "%s: line %u policy invalid\n",
					   prog_name, line);
				exit(1);
			}

			if (strcmp(policy, "-") != 0) {
				struct ipt_counters count;

				if (counters) {
					char *ctrs;
					ctrs = strtok(NULL, " \t\n");

					if (!ctrs || !parse_counters(ctrs, &count))
						xtables_error(PARAMETER_PROBLEM,
							   "invalid policy counters "
							   "for chain '%s'\n", chain);

				} else {
					memset(&count, 0,
					       sizeof(struct ipt_counters));
				}

				DEBUGP("Setting policy of chain %s to %s\n",
					chain, policy);

				if (!iptc_set_policy(chain, policy, &count,
						     handle))
					xtables_error(OTHER_PROBLEM,
						"Can't set policy `%s'"
						" on `%s' line %u: %s\n",
						policy, chain, line,
						iptc_strerror(errno));
			}

			ret = 1;

		} else if (in_table) {
			int a;
			char *ptr = buffer;
			char *pcnt = NULL;
			char *bcnt = NULL;
			char *parsestart;

			/* the parser */
			char *curchar;
			int quote_open, escaped;
			size_t param_len;

			/* reset the newargv */
			newargc = 0;

			if (buffer[0] == '[') {
				/* we have counters in our input */
				ptr = strchr(buffer, ']');
				if (!ptr)
					xtables_error(PARAMETER_PROBLEM,
						   "Bad line %u: need ]\n",
						   line);

				pcnt = strtok(buffer+1, ":");
				if (!pcnt)
					xtables_error(PARAMETER_PROBLEM,
						   "Bad line %u: need :\n",
						   line);

				bcnt = strtok(NULL, "]");
				if (!bcnt)
					xtables_error(PARAMETER_PROBLEM,
						   "Bad line %u: need ]\n",
						   line);

				/* start command parsing after counter */
				parsestart = ptr + 1;
			} else {
				/* start command parsing at start of line */
				parsestart = buffer;
			}

			add_argv(argv[0]);
			add_argv("-t");
			add_argv(curtable);

			if (counters && pcnt && bcnt) {
				add_argv("--set-counters");
				add_argv((char *) pcnt);
				add_argv((char *) bcnt);
			}

			/* After fighting with strtok enough, here's now
			 * a 'real' parser. According to Rusty I'm now no
			 * longer a real hacker, but I can live with that */

			quote_open = 0;
			escaped = 0;
			param_len = 0;

			for (curchar = parsestart; *curchar; curchar++) {
				char param_buffer[1024];

				if (quote_open) {
					if (escaped) {
						param_buffer[param_len++] = *curchar;
						escaped = 0;
						continue;
					} else if (*curchar == '\\') {
						escaped = 1;
						continue;
					} else if (*curchar == '"') {
						quote_open = 0;
						*curchar = ' ';
					} else {
						param_buffer[param_len++] = *curchar;
						continue;
					}
				} else {
					if (*curchar == '"') {
						quote_open = 1;
						continue;
					}
				}

				if (*curchar == ' '
				    || *curchar == '\t'
				    || * curchar == '\n') {
					if (!param_len) {
						/* two spaces? */
						continue;
					}

					param_buffer[param_len] = '\0';

					/* check if table name specified */
					if (!strncmp(param_buffer, "-t", 2)
					    || !strncmp(param_buffer, "--table", 8)) {
						xtables_error(PARAMETER_PROBLEM,
						   "Line %u seems to have a "
						   "-t table option.\n", line);
						exit(1);
					}

					add_argv(param_buffer);
					param_len = 0;
				} else {
					/* regular character, copy to buffer */
					param_buffer[param_len++] = *curchar;

					if (param_len >= sizeof(param_buffer))
						xtables_error(PARAMETER_PROBLEM,
						   "Parameter too long!");
				}
			}

			DEBUGP("calling do_command4(%u, argv, &%s, handle):\n",
				newargc, curtable);

			for (a = 0; a < newargc; a++)
				DEBUGP("argv[%u]: %s\n", a, newargv[a]);

			ret = do_command4(newargc, newargv,
					 &newargv[2], &handle);

			free_argv();
			fflush(stdout);
		}
		if (tablename && (strcmp(tablename, curtable) != 0))
			continue;
		if (!ret) {
			fprintf(stderr, "%s: line %u failed\n",
					prog_name, line);
			exit(1);
		}
	}
	if (in_table) {
		fprintf(stderr, "%s: COMMIT expected at line %u\n",
				prog_name, line + 1);
		exit(1);
	}

	if (in != NULL)
		fclose(in);
	return 0;
}
