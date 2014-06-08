/* pdnsd-ctl.c - Control pdnsd through a pipe

   Copyright (C) 2000, 2001 Thomas Moestl
   Copyright (C) 2002, 2003, 2004, 2005, 2007, 2008, 2009, 2010, 2011 Paul A. Rombouts

  This file is part of the pdnsd package.

  pdnsd is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  pdnsd is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with pdnsd; see the file COPYING. If not, see
  <http://www.gnu.org/licenses/>.
*/

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <ctype.h>
#include <stddef.h>	/* for offsetof */
#include "../helpers.h"
#include "../status.h"
#include "../conff.h"
#include "../list.h"
#include "../dns.h"
#include "../rr_types.h"
#include "../cache.h"

#if !defined(HAVE_ALLOCA) && !defined(alloca)
#define alloca malloc
#endif


#if defined(HAVE_STRUCT_IN6_ADDR) && defined(HAVE_INET_PTON)
# define ALLOW_AAAA IS_CACHED_AAAA
#else
# define ALLOW_AAAA 0
#endif

static short int verbose=1;

typedef struct {
	char *name;
	int  val;
} cmd_s;

#define CMD_LIST_RRTYPES (CTL_MAX+1)
#define CMD_HELP         (CTL_MAX+2)
#define CMD_VERSION      (CTL_MAX+3)

static const cmd_s top_cmds[]={
	{"help",CMD_HELP},{"version",CMD_VERSION},{"list-rrtypes",CMD_LIST_RRTYPES},
	{"status",CTL_STATS},{"server",CTL_SERVER},{"record",CTL_RECORD},
	{"source",CTL_SOURCE},{"add",CTL_ADD},{"neg",CTL_NEG},
	{"config",CTL_CONFIG},{"include",CTL_INCLUDE},{"eval",CTL_EVAL},
	{"empty-cache",CTL_EMPTY}, {"dump",CTL_DUMP},
	{NULL,0}
};
static const cmd_s server_cmds[]= {{"up",CTL_S_UP},{"down",CTL_S_DOWN},{"retest",CTL_S_RETEST},{NULL,0}};
static const cmd_s record_cmds[]= {{"delete",CTL_R_DELETE},{"invalidate",CTL_R_INVAL},{NULL,0}};
static const cmd_s onoff_cmds[]= {{"off",0},{"on",1},{NULL,0}};
static const cmd_s rectype_cmds[]= {{"a",T_A},
#if ALLOW_AAAA
				    {"aaaa",T_AAAA},
#endif
				    {"ptr",T_PTR},{"cname",T_CNAME},{"mx",T_MX},{"ns",T_NS},{NULL,0}};

static const char version_message[] =
	"pdnsd-ctl, version pdnsd-" VERSION "\n\n";

static const char license_statement[] =
	"Copyright (C) 2000, 2001 Thomas Moestl\n"
	"Copyright (C) 2002, 2003, 2004, 2005, 2007, 2008, 2009, 2010, 2011 Paul A. Rombouts\n\n"
	"This program is part of the pdnsd package.\n\n"
	"pdnsd is free software; you can redistribute it and/or modify\n"
	"it under the terms of the GNU General Public License as published by\n"
	"the Free Software Foundation; either version 3 of the License, or\n"
	"(at your option) any later version.\n\n"
	"pdnsd is distributed in the hope that it will be useful,\n"
	"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
	"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
	"GNU General Public License for more details.\n\n"
	"You should have received a copy of the GNU General Public License\n"
	"along with pdsnd; see the file COPYING.  If not, see\n"
	"<http://www.gnu.org/licenses/>.\n";

static const char *const help_messages[] =
{
	"Usage: pdnsd-ctl [-c cachedir] [-q] <command> [arguments]\n\n"

	"Command-line options:\n"

	"-c\tcachedir\n\tSet the cache directory to cachedir (must match pdnsd setting).\n"
	"\tThe default is '" CACHEDIR "'.\n"
	"-q\n\tBe quiet unless output is specified by command or something goes wrong.\n\n"

	"Commands and needed arguments are:\n"

	"help\t[no arguments]\n\tPrint this help.\n"
	"version\t[no arguments]\n\tPrint version and license info.\n",

	"status\t[no arguments]\n\tPrint pdnsd's status.\n",

	"server\t(index|label)\t(up|down|retest)\t[dns1[,dns2[,...]]]\n"
	"\tSet the status of the server with the given index to up or down, or\n"
	"\tforce a retest. The index is assigned in the order of definition in\n"
	"\tpdnsd.conf starting with 0. Use the status command to see the indexes.\n"
	"\tYou can specify the label of a server (that matches the label option)\n"
	"\tinstead of an index to make this easier.\n"

	"\tYou can specify all instead of an index to perform the action for all\n"
	"\tservers registered with pdnsd.\n"

	"\tAn optional third argument can be given consisting of a list of IP\n"
	"\taddresses separated by commas or spaces. This list will replace the\n"
	"\taddresses of name servers used by pdnsd for the given server section.\n"
	"\tThis feature is useful for run-time configuration of pdnsd with dynamic\n"
	"\tDNS data in scripts called by ppp or DHCP clients. The last argument\n"
	"\tmay also be an empty string, which causes existing IP addresses to be\n"
	"\tremoved and the corresponding server section to become inactive.\n",

	"record\tname\t(delete|invalidate)\n"
	"\tDelete or invalidate the record of the given domain if it is in the\n"
	"\tcache.\n",

	"source\tfn\towner\t[ttl]\t[(on|off)]\t[noauth]\n"
	"\tLoad a hosts-style file. Works like using the pdnsd source\n"
	"\tconfiguration section.\n"
	"\tOwner and ttl are used as in the source section. ttl has a default\n"
	"\tof 900 (it does not need to be specified). The next to last argument\n"
	"\tcorresponds to the serve_aliases option, and is off by default.\n"
	"\tnoauth is used to make the domains non-authoritative (please\n"
	"\tconsult the pdnsd manual for what that means).\n"
	"\tfn is the name of the file, which must be readable by pdnsd.\n",

	"add\ta\taddr\tname\t[ttl]\t[noauth]\n"
#if ALLOW_AAAA
	"add\taaaa\taddr\tname\t[ttl]\t[noauth]\n"
#endif
	"add\tptr\thost\tname\t[ttl]\t[noauth]\n"
	"add\tcname\thost\tname\t[ttl]\t[noauth]\n"
	"add\tmx\thost\tname\tpref\t[ttl]\t[noauth]\n"
	"add\tns\thost\tname\t[ttl]\t[noauth]\n"
	"\tAdd a record of the given type to the pdnsd cache, replacing existing\n"
	"\trecords for the same name and type. The 2nd argument corresponds\n"
 	"\tto the value of the option in the rr section that is named like\n"
 	"\tthe first argument. The addr argument may be a list of IP addresses,\n"
	"\tseparated by commas or white space. The ttl is optional, the default is\n"
	"\t900 seconds. noauth is used to make the domains non-authoritative.\n"
 	"\tIf you want no other record than the newly added in the cache, do\n"
 	"\tpdnsdctl record <name> delete\n"
 	"\tbefore adding records.\n",

	"neg\tname\t[type]\t[ttl]\n"
 	"\tAdd a negatively cached record to pdnsd's cache, replacing existing\n"
	"\trecords for the same name and type. If no type is given, the whole\n"
	"\tdomain is cached negatively. For negatively cached records, errors are\n"
	"\timmediately returned on a query, without querying other servers first.\n"
	"\tThe ttl is optional, the default is 900 seconds.\n",

	"config\t[filename]\n"
	"\tReload pdnsd's configuration file.\n"
	"\tThe config file must be owned by the uid that pdnsd had when it was\n"
	"\tstarted, and be readable by pdnsd's run_as uid. If no file name is\n"
	"\tspecified, the config file used at start up is reloaded.\n",

	"include\tfilename\n"
	"\tParse the given file as an include file, which may contain the same\n"
	"\ttype of sections as a config file, expect for global and server\n"
	"\tsections, which are not allowed. This command can be used to add data\n"
	"\tto the cache without reconfiguring pdnsd.\n",

	"eval\tstring\n"
	"\tParse string as if it were part of pdnsd's configuration file.\n"
	"\tThe string should hold one or more complete configuration sections,\n"
	"\tbut no global and server sections, which are not allowed.\n"
	"\tIf multiple strings are given, they will be joined using newline chars\n"
	"\tand parsed together.\n",

	"empty-cache\t[[+|-]name ...]\n"
	"\tDelete all entries in the cache matching include/exclude rules.\n"
	"\tIf no arguments are provided, the cache is completely emptied,\n"
	"\tfreeing all existing entries. This also removes \"local\" records,\n"
	"\tas defined by the config file. To restore local records, run\n"
	"\t\"pdnsd-ctl config\" or \"pdnsd-ctl include filename\"  immediately\n"
	"\tafterwards.\n"
	"\tIf one or more arguments are provided, these are interpreted as \n"
	"\tinclude/exclude names. If an argument starts with a '+' the name is to\n"
	"\tbe included. If an argument starts with a '-' it is to be excluded.\n"
	"\tIf an argument does not begin with '+' or '-', a '+' is assumed.\n"
	"\tIf the domain name of a cache entry ends in one of the names in the\n"
	"\tlist, the first match will determine what happens. If the matching name\n"
	"\tis to be included, the cache entry is deleted, otherwise it remains.\n"
	"\tIf there are no matches, the default action is not to delete.\n",

	"dump\t[name]\n"
	"\tPrint information stored in the cache about name.\n"
	"\tIf name begins with a dot and is not the root domain, information\n"
	"\tabout the names in the cache ending in name (including name without\n"
	"\tthe leading dot) will be printed. If name is missing, information about\n"
	"\tall the names in the cache will be printed.\n",

	"list-rrtypes\t[no arguments]\n"
	"\tList available rr types for the neg command. Note that those are only\n"
	"\tused for the neg command, not for add!\n"
};

#define NUM_HELP_MESSAGES (sizeof(help_messages)/sizeof(char*))


/* Open connection to control socket and send command code.
   If successful, open_sock returns a file descriptor for the new socket,
   otherwise the program is aborted.
*/
static int open_sock(const char *cache_dir, uint16_t cmd)
{
	struct sockaddr_un *sa;
	unsigned int sa_len;
	int sock;
	uint16_t nc;

	if ((sock=socket(PF_UNIX,SOCK_STREAM,0))==-1) {
		perror("Error: could not open socket");
		exit(2);
	}

	sa_len = (offsetof(struct sockaddr_un, sun_path) + strlitlen("/pdnsd.status") + strlen(cache_dir));
	sa=(struct sockaddr_un *)alloca(sa_len+1);
	sa->sun_family=AF_UNIX;
	stpcpy(stpcpy(sa->sun_path,cache_dir),"/pdnsd.status");

	if (connect(sock,(struct sockaddr *)sa,sa_len)==-1) {
		fprintf(stderr,"Error: could not open socket %s: %s\n",sa->sun_path,strerror(errno));
		close(sock);
		exit(2);
	}
	if(verbose) printf("Opening socket %s\n",sa->sun_path);

	/* Send command code */

	nc=htons(cmd|CTL_CMDVERNR); /* Add magic number, convert to network byte order. */

	if (write(sock,&nc,sizeof(nc))!=sizeof(nc)) {
		perror("Error: could not write command code");
		close(sock);
		exit(2);
	}

	return sock;
}

static void send_long(int fd,uint32_t cmd)
{
	uint32_t nc=htonl(cmd);

	if (write(fd,&nc,sizeof(nc))!=sizeof(nc)) {
		perror("Error: could not write long");
		close(fd);
		exit(2);
	}
}

static void send_short(int fd,uint16_t cmd)
{
	uint16_t nc=htons(cmd);

	if (write(fd,&nc,sizeof(nc))!=sizeof(nc)) {
		perror("Error: could not write short");
		close(fd);
		exit(2);
	}
}

#define MAXSENDSTRLEN 0xfffe

static void send_string(int fd, const char *s)
{
	if(s) {
		size_t len=strlen(s);
		if(len>MAXSENDSTRLEN) {
			fprintf(stderr,"Error: send_string: string length (%lu) exceeds maximum (%u).\n",
				(unsigned long)len, MAXSENDSTRLEN);
			close(fd);
			exit(2);
		}
		send_short(fd,len);
		if (write_all(fd,s,len)!=len) {
			perror("Error: could not write string");
			close(fd);
			exit(2);
		}
	}
	else
		send_short(fd, ~0);
}

static uint16_t read_short(int fd)
{
	ssize_t err;
	uint16_t nc;

	if ((err=read(fd,&nc,sizeof(nc)))!=sizeof(nc)) {
		fprintf(stderr,"Error: could not read short: %s\n",err<0?strerror(errno):"unexpected EOF");
		close(fd);
		exit(2);
	}
	return ntohs(nc);
}

/* copy data from file descriptor fd to file stream out until EOF
   or error is encountered.
*/
static ssize_t copymsgtofile(int fd, FILE* out)
{
	ssize_t n,ntot=0;
	char buf[1024];

	while ((n=read(fd,buf,sizeof(buf)))>0)
		ntot+=fwrite(buf,1,n,out);

	if(n<0)
		return n;

	return ntot;
}

static int match_cmd(const char *cmd, const cmd_s cmds[])
{
	int i;
	for(i=0; cmds[i].name; ++i) {
		if (strcasecmp(cmd,cmds[i].name)==0)
			return cmds[i].val;
	}
	return -1;
}

int main(int argc, char *argv[])
{
	char *cache_dir= CACHEDIR;
	int rv=0;
	{
		int i;
		char *arg;
		for(i=1; i<argc && (arg=argv[i]) && *arg=='-'; ++i) {
			if(!strcmp(arg,"-c")) {
				if(++i<argc) {
					cache_dir= argv[i];
				}
				else {
					fprintf(stderr,"file name expected after -c option.\n");
					goto print_try_pdnsd_ctl_help;
				}
			}
			else if(!strcmp(arg,"-q")) {
				verbose=0;
			}
			else {
				fprintf(stderr,"Unknown option: %s\n",arg);
				goto print_try_pdnsd_ctl_help;
			}
		}
		argc -= i;
		argv += i;
	}

	if (argc<1) {
		fprintf(stderr,"No command specified.\n");
	print_try_pdnsd_ctl_help:
		fprintf(stderr,"Try 'pdnsd-ctl help' for available commands and options.\n");
		exit(2);
	} else {
		int pf,acnt=0,cmd;

		cmd=match_cmd(argv[0],top_cmds);
		if(cmd==-1) {
			fprintf(stderr,"Command not recognized: %s\n",argv[0]);
			goto print_try_pdnsd_ctl_help;
		}
		switch (cmd) {
		case CMD_HELP: {
			int i;
			fputs(version_message,stdout);
			for(i=0; i<NUM_HELP_MESSAGES; ++i)
				fputs(help_messages[i],stdout);
		}
			break;

		case CMD_VERSION:
			fputs(version_message,stdout);
			fputs(license_statement,stdout);
			break;

		case CMD_LIST_RRTYPES: {
			int i;
			if (argc!=1)
				goto wrong_args;
			printf("Available RR types for the neg command:\n");
			for (i=0; i<NRRTOT; ++i)
				printf("%s\n",rrnames[rrcachiterlist[i]-T_MIN]);
		}
			break;

		case CTL_STATS:
			if (argc!=1)
				goto wrong_args;
			pf=open_sock(cache_dir, cmd);
			goto copy_pf;

		case CTL_SERVER: {
			int server_cmd;
			if (argc<3 || argc>4)
				goto wrong_args;
			acnt=2;
			server_cmd=match_cmd(argv[2],server_cmds);
			if(server_cmd==-1) goto bad_arg;
			pf=open_sock(cache_dir, cmd);
			send_string(pf,argv[1]);
			send_short(pf,server_cmd);
			send_string(pf,argc<4?NULL:argv[3]);
		}
			goto read_retval;

		case CTL_RECORD: {
			int record_cmd;
			if (argc!=3)
				goto wrong_args;
			acnt=2;
			record_cmd=match_cmd(argv[2],record_cmds);
			if(record_cmd==-1) goto bad_arg;
			pf=open_sock(cache_dir, cmd);
			send_short(pf,record_cmd);
			send_string(pf,argv[1]);
		}
			goto read_retval;

		case CTL_SOURCE: {
			long ttl;
			int servaliases,flags;
			if (argc<3 || argc>6)
				goto wrong_args;
			ttl=900;
			flags=DF_LOCAL;
			acnt=3;
			if (argc==6 || (argc>=4 && isdigit(argv[3][0]))) {
				char *endptr;
				ttl=strtol(argv[3],&endptr,0);
				if (*endptr)
					goto bad_arg;
				acnt++;
			}
			servaliases=0;
			if (acnt<argc && (argc==6 || strcasecmp(argv[acnt], "noauth"))) {
				servaliases=match_cmd(argv[acnt],onoff_cmds);
				if(servaliases==-1) goto bad_arg;
				acnt++;
			}
			if (acnt<argc) {
				if (!strcasecmp(argv[acnt], "noauth"))
					flags=0;
				else
					goto bad_arg;
			}
			pf=open_sock(cache_dir, cmd);
			send_string(pf,argv[1]);
			send_string(pf,argv[2]);
			send_long(pf,ttl);
			send_short(pf,servaliases);
			send_short(pf,flags);
		}
			goto read_retval;

		case CTL_ADD: {
			long ttl;
			int tp,flags,pref;
			unsigned int nadr;
			size_t adrsz, adrbufsz;
			char *q;

			if (argc<2) goto wrong_args;
			acnt=1;
			tp=match_cmd(argv[1],rectype_cmds);
			if(tp==-1) goto bad_arg;
			acnt=((tp==T_MX)?5:4);
			if (argc<acnt || argc>acnt+2)
				goto wrong_args;

			ttl=900;
			flags=DF_LOCAL;
			pref=0;
			if(tp==T_MX) {
				char *endptr;
				pref=strtol(argv[4],&endptr,0);
				if (*endptr) {
					acnt=4;
					goto bad_arg;
				}
			}

			if (acnt<argc && strcasecmp(argv[acnt],"noauth")) {
				char *endptr;
				ttl=strtol(argv[acnt],&endptr,0);
				if (*endptr)
					goto bad_arg;
				acnt++;
			}
			if (acnt<argc && !strcasecmp(argv[acnt],"noauth")) {
				flags=0;
				acnt++;
			}
			if (acnt<argc)
				goto bad_arg;

			nadr=0; adrsz=0;
			switch (tp) {
			case T_A:
				adrsz= sizeof(struct in_addr);
#if ALLOW_AAAA
				goto count_addresses;
			case T_AAAA:
				adrsz= sizeof(struct in6_addr);
			count_addresses:
#endif
				/* first count the number of comma- or space-delimited address strings,
				   ignoring blank strings. */
				for(q=argv[2];;) {
					for(;;++q) {
						if(!*q) goto finished_counting_addresses;
						if(*q!=',' && !isspace(*q)) break;
					}
					do {
						++q;
					} while(*q && *q!=',' && !isspace(*q));
					++nadr;
				}
			finished_counting_addresses:
				if (!nadr) {
					fprintf(stderr,"Empty IP list for 'add %s' command.\n", argv[1]);
					exit(2);
				}
				break;
			}

			adrbufsz = nadr*adrsz;
			{
				/* Variable-size array for storing IP addresses. */
				unsigned char adrbuf[adrbufsz] __attribute__((aligned));

				switch (tp) {
				case T_A:
#if ALLOW_AAAA
				case T_AAAA:
#endif
				{
					/* Convert the address strings into binary addresses and
					   store them in adrbuf. */
					unsigned char *adrp = adrbuf;
					for(q=argv[2];;) {
						char *p; size_t len;
						for(;;++q) {
							if(!*q) goto finished_converting_addresses;
							if(*q!=',' && !isspace(*q)) break;
						}
						p=q;
						do {
							++q;
						} while(*q && *q!=',' && !isspace(*q));
						len = q-p;
						{
							char tmpbuf[len+1];
							memcpy(tmpbuf,p,len);
							tmpbuf[len]=0;

							if(
#if ALLOW_AAAA
								tp==T_AAAA? inet_pton(AF_INET6,tmpbuf,adrp)<=0:
#endif
								!inet_aton(tmpbuf,(struct in_addr *)adrp))
							{
								fprintf(stderr,"Bad IP for 'add %s' command: %s\n",
									argv[1],tmpbuf);
								exit(2);
							}
						}
						adrp += adrsz;
					}
				}
				finished_converting_addresses:
					break;
				}

				pf=open_sock(cache_dir, cmd);
				send_short(pf,tp);
				send_string(pf,argv[3]);
				send_long(pf,ttl);
				send_short(pf,flags);

				switch (tp) {
				case T_A:
#if ALLOW_AAAA
				case T_AAAA:
#endif
					send_short(pf,nadr);
					if(write_all(pf,adrbuf,adrbufsz)!=adrbufsz) {
						perror("Error: could not send IP address(es)");
						close(pf);
						exit(2);
					}
					break;
				case T_MX:
					send_short(pf,pref);
					/* fall through */
				case T_PTR:
				case T_CNAME:
				case T_NS:
					send_string(pf,argv[2]);
					break;
				}
			}
		}
			goto read_retval;

		case CTL_NEG: {
			long ttl;
			int tp;

			if (argc<2 || argc>4)
				goto wrong_args;
			tp=255;
			ttl=900;
			acnt=2;
			if (argc==3) {
				if (isdigit(argv[2][0])) {
					char *endptr;
					ttl=strtol(argv[2],&endptr,0);
					if (*endptr)
						goto bad_arg;
				} else if ((tp=rr_tp_byname(argv[2]))==-1) {
					goto bad_type;
				}
			} else if (argc==4) {
				char *endptr;
				if ((tp=rr_tp_byname(argv[2]))==-1)
					goto bad_type;
				ttl=strtol(argv[3],&endptr,0);
				if (*endptr) {
					acnt=3;
					goto bad_arg;
				}
			}
			pf=open_sock(cache_dir, cmd);
			send_string(pf,argv[1]);
			send_short(pf,tp);
			send_long(pf,ttl);
		}
			goto read_retval;

		case CTL_CONFIG:
			if (argc>2)
				goto wrong_args;
			pf=open_sock(cache_dir, cmd);
			send_string(pf,argc<2?NULL:argv[1]);
			goto read_retval;

		case CTL_INCLUDE:
			if (argc!=2)
				goto wrong_args;
			pf=open_sock(cache_dir, cmd);
			send_string(pf,argv[1]);
			goto read_retval;

		case CTL_EVAL: {
			int i; size_t bufsz;

			if (argc<2)
				goto wrong_args;
			bufsz=0;
			for(i=1; i<argc; ++i)
				bufsz += strlen(argv[i])+1;

			if(bufsz>MAXSENDSTRLEN) {
				fprintf(stderr,"Cannot send 'eval' command: "
					"string length (%lu) exceeds maximum (%u).\n",
				(unsigned long)bufsz, MAXSENDSTRLEN);
				exit(2);
			}
			pf=open_sock(cache_dir, cmd);
			send_short(pf,bufsz);
			{
				/* Variable-size array for storing the joined strings. */
				char buf[bufsz];
				char *p=buf;
				for(i=1; i<argc; ++i) {
					p=stpcpy(p,argv[i]);
					*p++ = '\n';
				}
				if(write_all(pf,buf,bufsz)!=bufsz) {
					perror("Error: could not write string");
					close(pf);
					exit(2);
				}
			}
		}
			goto read_retval;

		case CTL_EMPTY: {
			int i; size_t totsz=0;
			for(i=1; i<argc; ++i)
				totsz += strlen(argv[i])+1;

			if(totsz>MAXSENDSTRLEN) {
				fprintf(stderr,"Cannot send 'empty' command: "
					"string length (%lu) exceeds maximum (%u).\n",
					(unsigned long)totsz, MAXSENDSTRLEN);
				exit(2);
			}
			pf=open_sock(cache_dir, cmd);
			if(argc>1) {
				send_short(pf,totsz);
				for(i=1; i<argc; ++i) {
					size_t sz=strlen(argv[i])+1;
					if(write_all(pf,argv[i],sz)!=sz) {
						perror("Error: could not write string");
						close(pf);
						exit(2);
					}
				}
			}
			else
				send_short(pf,~0);
		}
			goto read_retval;

		case CTL_DUMP:
			if (argc>2)
				goto wrong_args;
			pf=open_sock(cache_dir, cmd);
			send_string(pf,argc<2?NULL:argv[1]);
		copy_pf:
			if((rv=read_short(pf)))
				goto retval_failed;
			if(copymsgtofile(pf,stdout)<0) {
				perror("Error while reading from socket");
				close(pf);
				exit(2);
			}
			goto close_pf;

		read_retval:
			if((rv=read_short(pf))) {
			retval_failed:
				fprintf(stderr,"Failed: ");
				if(copymsgtofile(pf,stderr)<0)
					fprintf(stderr,"(could not read error message from socket: %s)",strerror(errno));

				fputc('\n',stderr);
			}
		close_pf:
			if(close(pf)==-1)
				perror("Couldn't close socket");
			else if (rv==0 && verbose)
				printf("Succeeded\n");
			break;
		wrong_args:
			fprintf(stderr,"Wrong number of arguments for '%s' command.\n",argv[0]);
			goto print_cmd_usage;
		bad_arg:
			fprintf(stderr,"Bad argument for '%s' command: %s\n",argv[0],argv[acnt]);
		print_cmd_usage:
			fprintf(stderr,"Usage:\n\n%s\n"
				"Try 'pdnsd-ctl help' for a description of all available commands and options.\n",
				help_messages[cmd]);
			exit(2);
		bad_type:
			fprintf(stderr,"Bad (type) argument for '%s' command: %s\n"
				"Run 'pdnsd-ctl list-rrtypes' for a list of available rr types.\n",
				argv[0],argv[acnt]);
			exit(2);
		}
	}

	return rv;
}

