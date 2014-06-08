#include <config.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include "../helpers.h"
#include "../conff.h"
#include "../netdev.h"

short int daemon_p=0;
#if DEBUG>0
short int debug_p=0;
#endif
short int verbosity=VERBOSITY;
#if defined(ENABLE_IPV4) && defined(ENABLE_IPV6)
short int run_ipv4=DEFAULT_IPV4;
#endif
#ifdef ENABLE_IPV6
struct in6_addr ipv4_6_prefix;
#endif
pthread_t main_thrid,servstat_thrid;
volatile int signal_interrupt;
#if DEBUG>0
FILE *dbg_file;
#endif
globparm_t global;


int main(int argc, char *argv[])
{
	if (argc!=2) {
		printf("Usage: %s <interface>\n",argv[0]);
		exit(1);
	}
	printf("if_up: %s - %s\n",argv[1],if_up(argv[1])?"up":"down");
	return 0;
}
