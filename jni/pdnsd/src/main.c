/* main.c - Command line parsing, intialisation and server start

   Copyright (C) 2000, 2001 Thomas Moestl
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2010 Paul A. Rombouts

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

/* in order to use O_NOFOLLOW on Linux: */
/* #define _GNU_SOURCE */

#include <config.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <syslog.h>
#include <pwd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include "consts.h"
#include "cache.h"
#include "status.h"
#include "servers.h"
#include "dns_answer.h"
#include "dns_query.h"
#include "error.h"
#include "helpers.h"
#include "icmp.h"
#include "hash.h"


#if DEBUG>0
short int debug_p=0;
#endif
short int stat_pipe=0;

/* int sigr=0; */
#if defined(ENABLE_IPV4) && defined(ENABLE_IPV6)
short int run_ipv4=DEFAULT_IPV4;
short int cmdlineipv=0;
#endif
cmdlineflags_t cmdline={0};
pthread_t main_thrid,servstat_thrid,statsock_thrid,tcps_thrid,udps_thrid;
uid_t init_uid;
#if DEBUG>0
FILE *dbg_file=NULL;
#endif
volatile int tcp_socket=-1;
volatile int udp_socket=-1;
sigset_t sigs_msk;
char *conf_file=CONFDIR"/pdnsd.conf";


/* version and licensing information */
static const char info_message[] =

	"pdnsd - dns proxy daemon, version " VERSION "\n\n"
	"Copyright (C) 2000, 2001 Thomas Moestl\n"
	"Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2010 Paul A. Rombouts\n\n"
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


/* the help page */
static const char help_message[] =

	"\n\nUsage: pdnsd [-h] [-V] [-s] [-d] [-g] [-t] [-p file] [-vn] [-mxx] [-c file]"
#ifdef ENABLE_IPV4
	" [-4]"
#endif
#ifdef ENABLE_IPV6
	" [-6] [-i prefix]"
#endif
#if defined(ENABLE_IPV4) && defined(ENABLE_IPV6)
	" [-a]"
#endif
	"\n\n"
	"Options:\n"
	"-h\t\t--or--\n"
	"--help\t\tprint this help page and exit.\n"
	"-V\t\t--or--\n"
	"--version\tprint version and license information and exit.\n"
	"--pdnsd-user\tprint the user pdnsd will run as and exit.\n"
	"-s\t\t--or--\n"
	"--status\tEnable status control socket in the cache directory.\n"
	"-d\t\t--or--\n"
	"--daemon\tStart pdnsd in daemon mode (as background process.)\n"
	"-g\t\t--or--\n"
	"--debug\t\tPrint some debug messages on the console or to the\n"
	"\t\tfile pdnsd.debug in your cache directory (in daemon mode).\n"
	"-t\t\t--or--\n"
	"--tcp\t\tEnables the TCP server thread. pdnsd will then serve\n"
	"\t\tTCP and UDP queries.\n"
	"-p\t\tWrites the pid the server runs as to a specified filename.\n"
	"\t\tWorks only in daemon mode.\n"
	"-vn\t\tsets the verbosity of pdnsd. n is a numeric argument from 0\n"
	"\t\t(normal operation) to 9 (many messages for debugging).\n"
	"\t\tUse like -v2\n"
	"-mxx\t\tsets the query method pdnsd uses. Possible values for xx are:\n"
	"\t\tuo (UDP only), to (TCP only), tu (TCP or, if the server\n"
	"\t\tdoes not support this, UDP) and ut (UDP and, if the reply was\n"
	"\t\ttruncated, TCP). Use like -muo. Preset: "
#if M_PRESET==UDP_ONLY
	"-muo"
#elif M_PRESET==TCP_ONLY
	"-mto"
#elif M_PRESET==TCP_UDP
	"-mtu"
#else
	"-mut"
#endif
	"\n"
	"-c\t\t--or--\n"
	"--config-file\tspecifies the file the configuration is read from.\n"
	"\t\tDefault is " CONFDIR "/pdnsd.conf\n"
#ifdef ENABLE_IPV4
	"-4\t\tswitches to IPv4 mode.\n"
	"\t\t"
#  if DEFAULT_IPV4
	"On"
#  else
	"Off"
#  endif
	" by default.\n"
#endif
#ifdef ENABLE_IPV6
	"-6\t\tswitches to IPv6 mode.\n"
	"\t\t"
#  if DEFAULT_IPV4
	"Off"
#  else
	"On"
#  endif
	" by default.\n"
	"-i\t\t--or--\n"
	"--ipv4_6_prefix\tspecifies the prefix pdnsd uses to map IPv4 to IPv6\n"
	"\t\taddresses. Must be a valid IPv6 address.\n"
	"\t\tDefault is " DEFAULT_IPV4_6_PREFIX "\n"
#endif
#if defined(ENABLE_IPV4) && defined(ENABLE_IPV6)
	"-a\t\tWith this option, pdnsd will try to detect automatically if\n"
	"\t\tthe system supports IPv6, and revert to IPv4 otherwise.\n"
#endif
	"\n\n\"no\" can be prepended to the --status, --daemon, --debug and --tcp\n"
	"options (e.g. --notcp) to reverse their effect.\n";


/* These are some init steps we have to call before we get daemon on linux, but need
 * to call after daemonizing on other OSes.
 * Theay are also the last steps before we drop privileges. */
int final_init()
{
#ifndef NO_TCP_SERVER
	if (!global.notcp)
		tcp_socket=init_tcp_socket();
#endif
	udp_socket=init_udp_socket();
	if (tcp_socket==-1 && udp_socket==-1) {
		log_error("tcp and udp initialization failed. Exiting.");
		return 0;
	}
	if (global.strict_suid) {
		if (!run_as(global.run_as)) {
			return 0;
		}
	}
	return 1;
}

#if defined(ENABLE_IPV4) && defined(ENABLE_IPV6)
/* Check if IPv6 is available.
 * With thanks to Juliusz Chroboczek.
 */
static int check_ipv6()
{
    int fd;
    fd = socket(PF_INET6, SOCK_STREAM, 0);
    if(fd < 0) {
        if(errno == EPROTONOSUPPORT || errno == EAFNOSUPPORT || errno == EINVAL)
            return 0;
        return -1;
    }
    close(fd);
    return 1;
}
#endif


/*
 * Argument parsing, init, server startup
 */
int main(int argc,char *argv[])
{
	int i,sig,pfd=-1;  /* Initialized to inhibit compiler warning */

	main_thrid=pthread_self();
	servstat_thrid=main_thrid;
	statsock_thrid=main_thrid;
	tcps_thrid=main_thrid;
	udps_thrid=main_thrid;
	init_uid=getuid();
#ifdef ENABLE_IPV6
	{
		int err;
		if((err=inet_pton(AF_INET6,DEFAULT_IPV4_6_PREFIX,&global.ipv4_6_prefix))<=0) {
			fprintf(stderr,"Error: inet_pton() wont accept default prefix %s in %s, line %d\n",
				DEFAULT_IPV4_6_PREFIX,__FILE__,__LINE__);
			if(err)
				perror("inet_pton");
			exit(1);
		}
	}
#endif

	/* Parse the command line.
	   Remember which options were specified here, because the command-line options
	   shall override the ones given in the config file */
	for (i=1;i<argc;i++) {
		char *arg=argv[i];
		if (strcmp(arg,"-h")==0 || strcmp(arg,"--help")==0) {
			fputs(info_message,stdout);
			fputs(help_message,stdout);
			exit(1);
		} else if (strcmp(arg,"-V")==0 || strcmp(arg,"--version")==0) {
			fputs(info_message,stdout);
			exit(1);
		} else if (strcmp(arg,"-c")==0 || strcmp(arg,"--config-file")==0) {
			if (++i<argc) {
				conf_file=argv[i];
			} else {
				fprintf(stderr,"Error: file name expected after %s option.\n",arg);
				exit(1);
			}
		} else if (strcmp(arg,"-4")==0) {
#ifdef ENABLE_IPV4
# ifdef ENABLE_IPV6
			run_ipv4=1; cmdlineipv=1;
# endif
#else
			fprintf(stderr,"Error: -4: pdnsd was compiled without IPv4 support.\n");
			exit(1);
#endif
		} else if (strcmp(arg,"-6")==0) {
#ifdef ENABLE_IPV6
# ifdef ENABLE_IPV4
			run_ipv4=0; cmdlineipv=1;
# endif
#else
			fprintf(stderr,"Error: -6: pdnsd was compiled without IPv6 support.\n");
			exit(1);
#endif
		} else if (strcmp(arg,"-a")==0) {
#if defined(ENABLE_IPV4) && defined(ENABLE_IPV6)
			int rv=check_ipv6();
			if(rv<0) {
				fprintf(stderr,"Error: -a: can't check availability of IPv6: %s\n"
					"Try using -4 or -6 option instead.\n",strerror(errno));
				exit(1);
			}
			if((run_ipv4= !rv))
				fprintf(stderr,"Switching to IPv4 mode.\n");
			cmdlineipv=1;
#else
			fprintf(stderr,"Warning: -a option does nothing unless pdnsd is compiled with both IPv4 AND IPv6 support.\n");
#endif
		} else if(strcmp(arg,"-i")==0 || strcmp(arg,"--ipv4_6_prefix")==0) {
			if (++i<argc) {
#ifdef ENABLE_IPV6
				if(inet_pton(AF_INET6,argv[i],&global.ipv4_6_prefix)<=0) {
					fprintf(stderr,"Error: %s: argument not a valid IPv6 address.\n",arg);
					exit(1);
				}
				cmdline.prefix=1;
#else
				fprintf(stderr,"pdnsd was compiled without IPv6 support. %s will be ignored.\n",arg);
#endif
			} else {
				fprintf(stderr,"Error: IPv6 address expected after %s option.\n",arg);
				exit(1);
			}
		} else if (strcmp(arg,"-s")==0 || strcmp(arg,"--status")==0) {
			global.stat_pipe=1; cmdline.stat_pipe=1;
		} else if (strcmp(arg,"--nostatus")==0) {
			global.stat_pipe=0; cmdline.stat_pipe=1;
		} else if (strcmp(arg,"-d")==0 || strcmp(arg,"--daemon")==0) {
			global.daemon=1; cmdline.daemon=1;
		} else if (strcmp(arg,"--nodaemon")==0) {
			global.daemon=0; cmdline.daemon=1;
		} else if (strcmp(arg,"-t")==0 || strcmp(arg,"--tcp")==0) {
			global.notcp=0; cmdline.notcp=1;
#ifdef NO_TCP_SERVER
			fprintf(stderr,"pdnsd was compiled without tcp server support. -t has no effect.\n");
#endif
		} else if (strcmp(arg,"--notcp")==0) {
			global.notcp=1; cmdline.notcp=1;
		} else if (strcmp(arg,"-p")==0) {
			if (++i<argc) {
				global.pidfile=argv[i]; cmdline.pidfile=1;
			} else {
				fprintf(stderr,"Error: file name expected after -p option.\n");
				exit(1);
			}
		} else if (strncmp(arg,"-v",2)==0) {
			if (strlen(arg)!=3 || !isdigit(arg[2])) {
				fprintf(stderr,"Error: one digit expected after -v option (like -v2).\n");
				exit(1);
			}
			global.verbosity=arg[2]-'0'; cmdline.verbosity=1;
		} else if (strncmp(arg,"-m",2)==0) {
			if (strlen(arg)!=4) {
				fprintf(stderr,"Error: uo, to or tu expected after the  -m option (like -muo).\n");
				exit(1);
			}
			if (strcmp(&arg[2],"uo")==0) {
#ifdef NO_UDP_QUERIES
				fprintf(stderr,"Error: pdnsd was compiled without UDP support.\n");
				exit(1);
#else
				global.query_method=UDP_ONLY;
#endif
			} else if (strcmp(&arg[2],"to")==0) {
#ifdef NO_TCP_QUERIES
				fprintf(stderr,"Error: pdnsd was compiled without TCP support.\n");
				exit(1);
#else
				global.query_method=TCP_ONLY;
#endif
			} else if (strcmp(&arg[2],"tu")==0) {
#if defined(NO_UDP_QUERIES) || defined(NO_TCP_QUERIES)
				fprintf(stderr,"Error: pdnsd was not compiled with UDP and TCP support.\n");
				exit(1);
#else
				global.query_method=TCP_UDP;
#endif
			} else if (strcmp(&arg[2],"ut")==0) {
#if defined(NO_UDP_QUERIES) || defined(NO_TCP_QUERIES)
				fprintf(stderr,"Error: pdnsd was not compiled with UDP and TCP support.\n");
				exit(1);
#else
				global.query_method=UDP_TCP;
#endif
			} else {
				fprintf(stderr,"Error: uo, to, tu or ut expected after the  -m option (like -muo).\n");
				exit(1);
			}
			cmdline.query_method=1;
		} else if (strcmp(arg,"-g")==0 || strcmp(arg,"--debug")==0) {
			global.debug=1; cmdline.debug=1;
#if !DEBUG
			fprintf(stderr,"pdnsd was compiled without debugging support. -g has no effect.\n");
#endif
		} else if (strcmp(arg,"--nodebug")==0) {
			global.debug=0; cmdline.debug=1;
		} else if (strcmp(arg,"--pdnsd-user")==0) {
			cmdline.pdnsduser=1;
		} else {
			char *equ=strchr(arg,'=');
			if(equ) {
				int plen=equ-arg;
				char *valstr=equ+1;
#       			define arg_isparam(strlit) (!strncmp(arg,strlit,strlitlen(strlit)) && plen==strlitlen(strlit))

				if(arg_isparam("--config-file")) {
					conf_file=valstr;
				}
				else if(arg_isparam("--ipv4_6_prefix")) {
#ifdef ENABLE_IPV6
					if(inet_pton(AF_INET6,valstr,&global.ipv4_6_prefix)<=0) {
						fprintf(stderr,"Error: --ipv4_6_prefix: argument not a valid IPv6 address.\n");
						exit(1);
					}
					cmdline.prefix=1;
#else
					fprintf(stderr,"pdnsd was compiled without IPv6 support. --ipv4_6_prefix will be ignored.\n");
#endif
				}
				else {
					fprintf(stderr,"Error: unknown option: %.*s\n",plen,arg);
					exit(1);
				}
			} else {
				fprintf(stderr,"Error: unknown option: %s\n",arg);
				exit(1);
			}
		}
	}

	init_cache();
	{
		char *errmsg;
		if(!read_config_file(conf_file,&global,&servers,0,&errmsg)) {
			fputs(errmsg?:"Out of memory.",stderr);
			fputc('\n',stderr);
			exit(3);
		}
	}

	if(cmdline.pdnsduser) {
		if (global.run_as[0]) {
			printf("%s\n",global.run_as);
		} else {
			uid_t uid=getuid();
			struct passwd *pws=getpwuid(uid);
			if (pws)
				printf("%s\n",pws->pw_name);
			else
				printf("%i\n",uid);
		}
		exit(0);
	}

	if(!global.cache_dir)   global.cache_dir = CACHEDIR;
	if(!global.scheme_file) global.scheme_file = "/var/lib/pcmcia/scheme";
	stat_pipe=global.stat_pipe;

	if (!(global.run_as[0] && global.strict_suid)) {
		for (i=0; i<DA_NEL(servers); i++) {
			servparm_t *sp=&DA_INDEX(servers,i);
			if (sp->uptest==C_EXEC && sp->uptest_usr[0]=='\0') {
				uid_t uid=getuid();
				struct passwd *pws=getpwuid(uid);

				/* No explicit uptest user given. If we run_as and strict_suid, we assume that
				 * this is safe. If not - warn. */
				fprintf(stderr,"Warning: uptest command \"%s\" will implicitly be executed as user ", sp->uptest_cmd);
				if (pws)
					fprintf(stderr,"%s\n",pws->pw_name);
				else
					fprintf(stderr,"%i\n",uid);

			}
		}
	}

	if (global.daemon && global.pidfile) {
		if (unlink(global.pidfile)!=0 && errno!=ENOENT) {
			log_error("Error: could not unlink pid file %s: %s",global.pidfile, strerror(errno));
			exit(1);
		}
		if ((pfd=open(global.pidfile,O_WRONLY|O_CREAT|O_EXCL
#ifdef O_NOFOLLOW
			      |O_NOFOLLOW
#else
		/*
		 * No O_NOFOLLOW. Nevertheless, this not a hole, since the
		 * directory for pidfiles should not be world writeable.
		 * OS's that do not support O_NOFOLLOW are currently not
		 * supported, this is just-in-case code.
		 */
#endif
			      , 0600))==-1)
		{
			log_error("Error: could not open pid file %s: %s",global.pidfile, strerror(errno));
			exit(1);
		}
	}
	for (i=0;i<DA_NEL(servers);i++) {
		if (DA_INDEX(servers,i).uptest==C_PING) {
			init_ping_socket();
			break;
		}
	}

	if (!init_rng())
		exit(1);
#if (TARGET==TARGET_LINUX)
	if (!final_init())
		exit(1);
#endif

	{
		struct sigaction action;
		action.sa_handler = SIG_IGN;
		sigemptyset(&action.sa_mask);
		action.sa_flags = 0;
		if(sigaction(SIGPIPE, &action, NULL) != 0)
			log_error("Could not call sigaction to ignore SIGPIPE: %s",strerror(errno));
	}

	umask(0077); /* for security reasons */
	if (global.daemon) {
		pid_t pid;
		int fd;

		/* become a daemon */
		pid=fork();
		if (pid==-1) {
			log_error("Could not become a daemon: fork #1 failed: %s",strerror(errno));
			exit(1);
		}
		if (pid!=0) {
			/* This is the parent.
			   The child is going to do another fork() and will exit quickly.
			   Perhaps we should wait for the child and return
			   its exit status? */
			exit(0); /* exit parent */
		}
		/* dissociate from controlling terminal */
		if (setsid()==-1) {
			log_error("Could not become a daemon: setsid failed: %s",strerror(errno));
			_exit(1);
		}
		pid=fork();
		if (pid==-1) {
			log_error("Could not become a daemon: fork #2 failed: %s",strerror(errno));
			_exit(1);
		}
		if (pid!=0) {
			int exitval=0;
			if (global.pidfile) {
				if(fsprintf(pfd,"%i\n",(int)pid)<0) {
					log_error("Error: could not write to pid file %s: %s",
						  global.pidfile, strerror(errno));
					exitval=1;
				}
				if(close(pfd)<0) {
					log_error("Error: could not close pid file %s: %s",
						  global.pidfile, strerror(errno));
					exitval=1;
				}
			}
			_exit(exitval); /* exit parent, so we are no session group leader */
		}

		if (global.pidfile) close(pfd);
		if(chdir("/"))
			log_warn("Cannot chdir to root directory: %s",strerror(errno));
		if ((fd=open("/dev/null",O_RDONLY))==-1) {
			log_error("Could not become a daemon: open for /dev/null failed: %s",strerror(errno));
			_exit(1);
		}
		dup2(fd,0);
		close(fd);
		if ((fd=open("/dev/null",O_WRONLY))==-1) {
			log_error("Could not become a daemon: open for /dev/null failed: %s",strerror(errno));
			_exit(1);
		}
		dup2(fd,1);
		dup2(fd,2);
		close(fd);
#if DEBUG>0
		if (global.debug) {
			char dbgpath[strlen(global.cache_dir)+sizeof("/pdnsd.debug")];
			stpcpy(stpcpy(dbgpath,global.cache_dir),"/pdnsd.debug");
			if (!(dbg_file=fopen(dbgpath,"w")))
				log_warn("Warning: could not open debug file %s: %s",dbgpath, strerror(errno));
		}
#endif
	} else {
#if DEBUG>0
		dbg_file=stdout;
#endif
	}

#if DEBUG>0
	debug_p= (global.debug && dbg_file);
#endif
	log_info(0,"pdnsd-%s starting.\n",VERSION);
	DEBUG_MSG("Debug messages activated\n");

#if (TARGET!=TARGET_LINUX)
	if (!final_init())
		_exit(1);
#endif
	DEBUG_MSG(SEL_IPVER("Using IPv4.\n", "Using IPv6.\n"));

	/* initialize attribute for creating detached threads */
	pthread_attr_init(&attr_detached);
	pthread_attr_setdetachstate(&attr_detached,PTHREAD_CREATE_DETACHED);

	read_disk_cache();

	/* This must be done before any other thread is started to avoid races. */
	if (stat_pipe)
		init_stat_sock();


	/* Before this point, logging and cache accesses are not locked because we are single-threaded. */
	init_log_lock();
	init_cache_lock();

	sigemptyset(&sigs_msk);
	sigaddset(&sigs_msk,SIGHUP);
	sigaddset(&sigs_msk,SIGINT);
#ifndef THREADLIB_NPTL
	sigaddset(&sigs_msk,SIGILL);
#endif
	sigaddset(&sigs_msk,SIGABRT);
	sigaddset(&sigs_msk,SIGFPE);
#ifndef THREADLIB_NPTL
	sigaddset(&sigs_msk,SIGSEGV);
#endif
	sigaddset(&sigs_msk,SIGTERM);
	/* if (!daemon_p) {
		sigaddset(&sigs_msk,SIGQUIT);
	} */
#if (TARGET==TARGET_LINUX)
	pthread_sigmask(SIG_BLOCK,&sigs_msk,NULL);
#endif

#if DEBUG>0
	{
		int err;
		/* Generate a key for storing our thread id's */
		if ((err=pthread_key_create(&thrid_key, NULL)) != 0) {
			log_error("pthread_key_create failed: %s",strerror(err));
			_exit(1);
		}
	}
#endif

	{
#if DEBUG>0
		int thrdsucc=1;
# define thrdfail (thrdsucc=0)
#else
# define thrdfail
#endif

		if(start_servstat_thread()) thrdfail;

#if (TARGET==TARGET_LINUX)
		if (!global.strict_suid) {
			if (!run_as(global.run_as)) {
				_exit(1);
			}
		}
#endif

		if (stat_pipe)
			if(start_stat_sock()) thrdfail;

		start_dns_servers();

#if DEBUG>0
		if(thrdsucc) {
			DEBUG_MSG("All threads started successfully.\n");
		}
#endif
#undef thrdfail
	}

#if (TARGET==TARGET_LINUX) && !defined(THREADLIB_NPTL)
	pthread_sigmask(SIG_BLOCK,&sigs_msk,NULL);
	waiting=1;
#endif
	{
		int err;
		while ((err=sigwait(&sigs_msk,&sig))) {
			if (err!=EINTR) {
				log_error("sigwait failed: %s",strerror(err));
				sig=0;
				break;
			}
		}
	}
	if(sig) DEBUG_MSG("Signal %i caught.\n",sig);
	write_disk_cache();
	destroy_cache();
	if(sig) log_warn("Caught signal %i. Exiting.",sig);
	if (sig==SIGSEGV || sig==SIGILL || sig==SIGBUS)
		crash_msg("This is a fatal signal probably triggered by a bug.");
	if (ping_isocket!=-1)
		close(ping_isocket);
#ifdef ENABLE_IPV6
	if (ping6_isocket!=-1)
		close(ping6_isocket);
#endif
	/* Close and delete the status socket */
	if(stat_pipe) close(stat_sock);
	if (sock_path && unlink(sock_path))
		log_warn("Failed to unlink %s: %s",sock_path, strerror(errno));

	free_rng();
#if DEBUG>0
	if (debug_p && global.daemon)
		if(fclose(dbg_file)<0) {
			log_warn("Could not close debug file: %s", strerror(errno));
		}
#endif
	_exit(0);
}
