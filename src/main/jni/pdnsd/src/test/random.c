#include <config.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include "../helpers.h"
#include "../conff.h"

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


int main(void)
{
	init_rng();
	printf("%i\n",(int)get_rand16());
	free_rng();
	return 0;
}
