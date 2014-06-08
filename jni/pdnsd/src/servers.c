/* servers.c - manage a set of dns servers

   Copyright (C) 2000, 2001 Thomas Moestl
   Copyright (C) 2002, 2003, 2005, 2007, 2009, 2011 Paul A. Rombouts

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
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <string.h>
#include "thread.h"
#include "error.h"
#include "servers.h"
#include "conff.h"
#include "consts.h"
#include "icmp.h"
#include "netdev.h"
#include "helpers.h"
#include "dns_query.h"


/*
 * We may be a little over-strict with locks here. Never mind...
 * Also, there may be some code-redundancy regarding uptests. It saves some locks, though.
 */

static pthread_mutex_t servers_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t server_data_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t server_test_cond = PTHREAD_COND_INITIALIZER;
static int server_data_users = 0, server_status_ping = 0;
/* Used to notify the server status thread that it should discontinue uptests. */
volatile int signal_interrupt=0;
#define statusintsig SIGHUP

static short retest_flag=0;

static char schm[32];

static void sigint_handler(int signum);

/*
 * Execute an individual uptest. Call with locks applied
 */
static int uptest (servparm_t *serv, int j)
{
	int ret=0, count_running_ping=0;
	pdnsd_a *s_addr= PDNSD_A2_TO_A(&DA_INDEX(serv->atup_a,j).a);

	DEBUG_PDNSDA_MSG("performing uptest (type=%s) for %s\n",const_name(serv->uptest),PDNSDA2STR(s_addr));

	/* Unlock the mutex because some of the tests may take a while. */
	++server_data_users;
	if((serv->uptest==C_PING || serv->uptest==C_QUERY) && pthread_equal(pthread_self(),servstat_thrid)) {
		/* Inform other threads that a ping is in progress. */
		count_running_ping=1;
		++server_status_ping;
	}
	pthread_mutex_unlock(&servers_lock);

	switch (serv->uptest) {
	case C_NONE:
		/* Don't change */
		ret=DA_INDEX(serv->atup_a,j).is_up;
		break;
	case C_PING:
		ret=ping(is_inaddr_any(&serv->ping_a) ? s_addr : &serv->ping_a, serv->ping_timeout,PINGREPEAT)!=-1;
		break;
	case C_IF:
 	case C_DEV:
	case C_DIALD:
 		ret=if_up(serv->interface);
#if (TARGET==TARGET_LINUX)
 		if (ret!=0) {
			if(serv->uptest==C_DEV)
				ret=dev_up(serv->interface,serv->device);
			else if (serv->uptest==C_DIALD)
				ret=dev_up("diald",serv->device);
 		}
#endif
		break;
	case C_EXEC: {
	  	pid_t pid;

		if ((pid=fork())==-1) {
			DEBUG_MSG("Could not fork to perform exec uptest: %s\n",strerror(errno));
			break;
		} else if (pid==0) { /* child */
			/*
			 * If we ran as setuid or setgid, do not inherit this to the
			 * command. This is just a last guard. Running pdnsd as setuid()
			 * or setgid() is a no-no.
			 */
			if (setgid(getgid()) == -1 || setuid(getuid()) == -1) {
				log_error("Could not reset uid or gid: %s",strerror(errno));
				_exit(1);
			}
			/* Try to setuid() to a different user as specified. Good when you
			   don't want the test command to run as root */
			if (!run_as(serv->uptest_usr)) {
				_exit(1);
			}
			{
			    struct rlimit rl; int i;
			    /*
			     * Mark all open fd's FD_CLOEXEC for paranoia reasons.
			     */
			    if (getrlimit(RLIMIT_NOFILE, &rl) == -1) {
				    log_error("getrlimit() failed: %s",strerror(errno));
				    _exit(1);
			    }
			    for (i = 0; i < rl.rlim_max; i++) {
				    if (fcntl(i, F_SETFD, FD_CLOEXEC) == -1 && errno != EBADF) {
					    log_error("fcntl(F_SETFD) failed: %s",strerror(errno));
					    _exit(1);
				    }
			    }
			}
			execl("/bin/sh", "uptest_sh","-c",serv->uptest_cmd,(char *)NULL);
			_exit(1); /* failed execl */
		} else { /* parent */
			int status;
			pid_t wpid = waitpid(pid,&status,0);
			if (wpid==pid) {
				if(WIFEXITED(status)) {
					int exitstatus=WEXITSTATUS(status);
					DEBUG_MSG("uptest command \"%s\" exited with status %d\n",
						  serv->uptest_cmd, exitstatus);
					ret=(exitstatus==0);
				}
#if DEBUG>0
				else if(WIFSIGNALED(status)) {
					DEBUG_MSG("uptest command \"%s\" was terminated by signal %d\n",
						  serv->uptest_cmd, WTERMSIG(status));
				}
				else {
					DEBUG_MSG("status of uptest command \"%s\" is of unkown type (0x%x)\n",
						  serv->uptest_cmd, status);
				}
#endif
			}
#if DEBUG>0
			else if (wpid==-1) {
				DEBUG_MSG("Error while waiting for uptest command \"%s\" to terminate: "
					  "waitpid for pid %d failed: %s\n",
					  serv->uptest_cmd, pid, strerror(errno));
			}
			else {
				DEBUG_MSG("Error while waiting for uptest command \"%s\" to terminate: "
					  "waitpid returned %d, expected pid %d\n",
					  serv->uptest_cmd, wpid, pid);
			}
#endif
		}
	}
		break;
	case C_QUERY:
		ret=query_uptest(s_addr, serv->port, serv->query_test_name,
				 serv->timeout>=global.timeout?serv->timeout:global.timeout,
				 PINGREPEAT);
	} /* end of switch */

	pthread_mutex_lock(&servers_lock);
	if(count_running_ping)
		--server_status_ping;
	PDNSD_ASSERT(server_data_users>0, "server_data_users non-positive before attempt to decrement it");
	if (--server_data_users==0) pthread_cond_broadcast(&server_data_cond);

	DEBUG_PDNSDA_MSG("result of uptest for %s: %s\n",
			 PDNSDA2STR(s_addr),
			 ret?"OK":"failed");
	return ret;
}

static int scheme_ok(servparm_t *serv)
{
	if (serv->scheme[0]) {
		if (!schm[0]) {
		  	ssize_t nschm;
			int sc = open(global.scheme_file, O_RDONLY);
			char *s;
			if (sc<0)
				return 0;
			nschm = read(sc, schm, sizeof(schm)-1);
			close(sc);
			if (nschm < 0)
				return 0;
			schm[nschm] = '\0';
			s = strchr(schm, '\n');
			if (s)
				*s='\0';
		}
		if (fnmatch(serv->scheme, schm, 0))
		  	return 0;
	}
	return 1;
}

/* Internal server test. Call with locks applied.
   May test a single server ip or several collectively.
 */
static void retest(int i, int j)
{
  time_t s_ts;
  servparm_t *srv=&DA_INDEX(servers,i);
  int nsrvs=DA_NEL(srv->atup_a);

  if(!nsrvs) return;
  if(j>=0) {
    if(j<nsrvs) nsrvs=j+1;  /* test just one */
  }
  else {
    j=0;        /* test a range of servers */
  }

  if(!scheme_ok(srv)) {
    s_ts=time(NULL);

    for(;j<nsrvs;++j) {
      atup_t *at=&DA_INDEX(srv->atup_a,j);
      at->is_up=0;
      at->i_ts=s_ts;
    }
  }
  else if(srv->uptest==C_NONE) {
    s_ts=time(NULL);

    for(;j<nsrvs;++j) {
	DA_INDEX(srv->atup_a,j).i_ts=s_ts;
    }
  }
  else if(srv->uptest==C_QUERY || (srv->uptest==C_PING && is_inaddr_any(&srv->ping_a))) {  /* test each ip address separately */
    for(;j<nsrvs;++j) {
	atup_t *at=&DA_INDEX(srv->atup_a,j);
	s_ts=time(NULL);
	at->is_up=uptest(srv,j);
	if(signal_interrupt)
	  break;
	at->i_ts=s_ts;
    }
  }
  else {  /* test ip addresses collectively */
    int res;

    s_ts=time(NULL);
    res=uptest(srv,j);
    for(;j<nsrvs;++j) {
      atup_t *at=&DA_INDEX(srv->atup_a,j);
      at->is_up=res;
      if(signal_interrupt && srv->uptest==C_PING)
	continue;
      at->i_ts=s_ts;
    }
  }
}


/* This is called by the server status thread to discover the addresses of root servers.
   Call with server_lock applied.
*/
static addr2_array resolv_rootserver_addrs(atup_array a, int port, char edns_query, time_t timeout)
{
	addr2_array retval=NULL;

	/* Unlock the mutex because this may take a while. */
	++server_data_users;
	pthread_mutex_unlock(&servers_lock);

	retval= dns_rootserver_resolv(a,port,edns_query,timeout);

	pthread_mutex_lock(&servers_lock);
	PDNSD_ASSERT(server_data_users>0, "server_data_users non-positive before attempt to decrement it");
	if (--server_data_users==0) pthread_cond_broadcast(&server_data_cond);

	return retval;
}

/*
 * Refresh the server status by pinging or testing the interface in the given interval.
 * Note that you may get inaccuracies in the dimension of the ping timeout or the runtime
 * of your uptest command if you have uptest=ping or uptest=exec for at least one server.
 * This happens when all the uptests for the first n servers take more time than the inteval
 * of n+1 (or 0 when n+1>servnum). I do not think that these delays are critical, so I did
 * not to anything about that (because that may also be costly).
 */
void *servstat_thread(void *p)
{
	struct sigaction action;
	int keep_testing;

	/* (void)p; */  /* To inhibit "unused variable" warning */

	THREAD_SIGINIT;

	pthread_mutex_lock(&servers_lock);
	/* servstat_thrid=pthread_self(); */

	signal_interrupt=0;
	action.sa_handler = sigint_handler;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	if(sigaction(statusintsig, &action, NULL) == 0) {
		sigset_t smask;
		sigemptyset(&smask);
		sigaddset(&smask, statusintsig);
		pthread_sigmask(SIG_UNBLOCK,&smask,NULL);
	}
	else {
		log_warn("Cannot install signal handler for server status thread: %s\n",strerror(errno));
	}

	for(;;) {
		do {
			int i,n;
			keep_testing=0;
			retest_flag=0;
			schm[0] = '\0';
			n=DA_NEL(servers);
			for (i=0;i<n;++i) {
				servparm_t *sp=&DA_INDEX(servers,i);
				int j,m;
				if(sp->rootserver==2) {
					/* First get addresses of root servers. */
					addr2_array adrs;
					int l, one_up=0;

					if(!scheme_ok(sp)) {
						time_t now=time(NULL);
						m=DA_NEL(sp->atup_a);
						for(j=0;j<m;++j)
							DA_INDEX(sp->atup_a,j).i_ts=now;
					} else if(sp->uptest==C_PING || sp->uptest==C_QUERY) {
						/* Skip ping or query tests until after discovery. */
						if(sp->interval>0)
							one_up= DA_NEL(sp->atup_a);
						else {
							time_t now=time(NULL);
							m=DA_NEL(sp->atup_a);
							for(j=0;j<m;++j) {
								atup_t *at=&DA_INDEX(sp->atup_a,j);
								if(at->is_up || at->i_ts==0)
									one_up=1;
								at->i_ts=now;
							}
						}
					}
					else {
						retest(i,-1);

						m=DA_NEL(sp->atup_a);
						for(j=0;j<m;++j) {
							if(DA_INDEX(sp->atup_a,j).is_up) {
								one_up=1;
								break;
							}
						}
					}

					if(!one_up) {
						if (needs_intermittent_testing(sp)) keep_testing=1;
						continue;
					}

					DEBUG_MSG("Attempting to discover root servers for server section #%d.\n",i);
					adrs=resolv_rootserver_addrs(sp->atup_a,sp->port,sp->edns_query,sp->timeout);
					l= DA_NEL(adrs);
					if(l>0) {
						struct timeval now;
						struct timespec timeout;
						atup_array ata;
						DEBUG_MSG("Filling server section #%d with %d root server addresses.\n",i,l);
						gettimeofday(&now,NULL);
						timeout.tv_sec = now.tv_sec + 60;     /* time out after 60 seconds */
						timeout.tv_nsec = now.tv_usec * 1000;
						while (server_data_users>0) {
							if(pthread_cond_timedwait(&server_data_cond, &servers_lock, &timeout) == ETIMEDOUT) {
								DEBUG_MSG("Timed out while waiting for exclusive access to server data"
									  " to set root server addresses of server section #%d\n",i);
								da_free(adrs);
								keep_testing=1;
								continue;
							}
						}
						ata = DA_CREATE(atup_array, l);
						if(!ata) {
							log_warn("Out of memory in servstat_thread() while discovering root servers.");
							da_free(adrs);
							keep_testing=1;
							continue;
						}
						for(j=0; j<l; ++j) {
							atup_t *at = &DA_INDEX(ata,j);
							at->a = DA_INDEX(adrs,j);
							at->is_up=sp->preset;
							at->i_ts= sp->interval<0 ? time(NULL): 0;
						}
						da_free(sp->atup_a);
						sp->atup_a=ata;
						da_free(adrs);
						/* Successfully set IP addresses for this server section. */
						sp->rootserver=1;
					}
					else {
						DEBUG_MSG("Failed to discover root servers in servstat_thread() (server section #%d).\n",i);
						if(adrs) da_free(adrs);
						if(DA_NEL(sp->atup_a)) keep_testing=1;
						continue;
					}
				}

				if (needs_testing(sp)) keep_testing=1;
				m=DA_NEL(sp->atup_a);
				for(j=0;j<m;++j)
					if(DA_INDEX(sp->atup_a,j).i_ts)
						goto individual_tests;
				/* Test collectively */
				if(!signal_interrupt) retest(i,-1);
				continue;

			individual_tests:
				for(j=0; !signal_interrupt && j<m; ++j) {
					time_t ts=DA_INDEX(sp->atup_a,j).i_ts, now;

					if (ts==0 /* Always test servers with timestamp 0 */ ||
					    (needs_intermittent_testing(sp) &&
					     ((now=time(NULL))-ts>sp->interval ||
					      ts>now /* kluge for clock skew */)))
					{
						retest(i,j);
					}
				}
			}
		} while(!signal_interrupt && retest_flag);

		signal_interrupt=0;

		/* Break the loop and exit the thread if it is no longer needed. */
		if(!keep_testing) break;

		{
			struct timeval now;
			struct timespec timeout;
			time_t minwait;
			int i,n,retval;

			gettimeofday(&now,NULL);
			minwait=3600; /* Check at least once every hour. */
			n=DA_NEL(servers);
			for (i=0;i<n;++i) {
				servparm_t *sp=&DA_INDEX(servers,i);
				int j,m=DA_NEL(sp->atup_a);
				for(j=0;j<m;++j) {
					time_t ts= DA_INDEX(sp->atup_a,j).i_ts;
					if(ts==0) {
						/* Test servers with timestamp 0 without delay */
						if(minwait > 0) minwait=0;
					}
					else if(needs_intermittent_testing(sp)) {
						time_t wait= ts + sp->interval - now.tv_sec;
						if(wait < minwait) minwait=wait;
					}
				}
			}
			timeout.tv_sec = now.tv_sec;
			if(minwait>0)
				timeout.tv_sec += minwait;
			timeout.tv_nsec = now.tv_usec * 1000 + 500000000;  /* wait at least half a second. */
			if(timeout.tv_nsec>=1000000000) {
				timeout.tv_nsec -= 1000000000;
				++timeout.tv_sec;
			}
			/* While we wait for a server_test_cond condition or a timeout
			   the servers_lock mutex is unlocked, so other threads can access
			   server data
			*/
			retval=pthread_cond_timedwait(&server_test_cond, &servers_lock, &timeout);
			DEBUG_MSG("Server status thread woke up (%s signal).\n",
				  retval==0?"test condition":retval==ETIMEDOUT?"timer":retval==EINTR?"interrupt":"error");
		}
	}

	/* server status thread no longer needed. */
	servstat_thrid=main_thrid;
	pthread_mutex_unlock(&servers_lock);
	DEBUG_MSG("Server status thread exiting.\n");
	return NULL;
}

/*
 * Start the server status thread.
 */
int start_servstat_thread()
{
	pthread_t stt;

	int rv=pthread_create(&stt,&attr_detached,servstat_thread,NULL);
	if (rv)
		log_warn("Failed to start server status thread: %s",strerror(rv));
	else {
		servstat_thrid=stt;
		log_info(2,"Server status thread started.");
	}
	return rv;
}

/*
 * This can be used to mark a server (or a list of nadr servers) up (up=1) or down (up=0),
 * or to schedule an immediate retest (up=-1).
 * We can't always use indices to identify a server, because we allow run-time
 * configuration of server addresses, so the servers are identified by their IP addresses.
 */
void sched_server_test(pdnsd_a *sa, int nadr, int up)
{
	int k,signal_test;

	pthread_mutex_lock(&servers_lock);

	signal_test=0;
	/* This obviously isn't very efficient, but nadr should be small
	   and anything else would introduce considerable overhead */
	for(k=0;k<nadr;++k) {
		pdnsd_a *sak= &sa[k];
		int i,n=DA_NEL(servers);
		for(i=0;i<n;++i) {
			servparm_t *sp=&DA_INDEX(servers,i);
			int j,m=DA_NEL(sp->atup_a);
			for(j=0;j<m;++j) {
				atup_t *at=&DA_INDEX(sp->atup_a,j);
				if(equiv_inaddr2(sak,&at->a)) {
					if(up>=0) {
						at->is_up=up;
						at->i_ts=time(NULL);
						DEBUG_PDNSDA_MSG("Marked server %s %s.\n",PDNSDA2STR(sak),up?"up":"down");
					}
					else if(at->i_ts) {
						/* A test may take a while, and we don't want to hold
						   up the calling thread.
						   Instead we set the timestamp to zero and signal
						   a condition which should wake up the server test thread.
						*/
						at->i_ts=0;
						signal_test=1;
					}
				}
			}
		}
	}
	if(signal_test) pthread_cond_signal(&server_test_cond);

	pthread_mutex_unlock(&servers_lock);
}

/* Mark a set of servers up or down or schedule uptests.
 * If i>=0 only the server section with index i is scanned,
 * if i<0 all sections are scanned.
 * Only sections matching label are actually set. A NULL label matches
 * any section.
 * up=1 or up=0 means mark server up or down, up=-1 means retest.
 *
 * A non-zero return value indicates an error.
 */
int mark_servers(int i, char *label, int up)
{
	int retval=0,n,signal_test;

	pthread_mutex_lock(&servers_lock);

	signal_test=0;
	n=DA_NEL(servers);
	if(i>=0) {
		/* just one section */
		if(i<n) n=i+1;
	}
	else {
		i=0; /* scan all sections */
	}
	for(;i<n;++i) {
		servparm_t *sp=&DA_INDEX(servers,i);
		if(!label || (sp->label && !strcmp(sp->label,label))) {
			int j,m=DA_NEL(sp->atup_a);

			/* If a section with undiscovered root servers is marked up, signal a test. */
			if(m && sp->rootserver>1 && up>0) signal_test=1;

			for(j=0;j<m;++j) {
				atup_t *at=&DA_INDEX(sp->atup_a,j);
				if(up>=0) {
					at->is_up=up;
					at->i_ts=time(NULL);
				}
				else if(at->i_ts) {
					/* A test may take a while, and we don't want to hold
					   up the calling thread.
					   Instead we set the timestamp to zero and signal
					   a condition which should wake up the server test thread.
					*/
					at->i_ts=0;
					signal_test=1;
				}
			}
		}
	}
	if(signal_test) {
		if(pthread_equal(servstat_thrid,main_thrid))
			retval=start_servstat_thread();
		else {
			retest_flag=1;
			retval=pthread_cond_signal(&server_test_cond);
		}
	}

	pthread_mutex_unlock(&servers_lock);
	return retval;
}

/*
 * Test called by the dns query handlers to handle interval=onquery cases.
 */
void test_onquery()
{
	int i,n,signal_test;

	pthread_mutex_lock(&servers_lock);
	schm[0] = '\0';
	signal_test=0;
	n=DA_NEL(servers);
	for (i=0;i<n;++i) {
		servparm_t *sp=&DA_INDEX(servers,i);
		if (sp->interval==-1) {
			if(sp->rootserver<=1)
				retest(i,-1);
			else {
				/* We leave root-server discovery to the server status thread */
				int j,m=DA_NEL(sp->atup_a);
				for(j=0;j<m;++j)
					DA_INDEX(sp->atup_a,j).i_ts=0;
				signal_test=1;
			}
		}
	}

	if(signal_test) {
		int rv;
		if(pthread_equal(servstat_thrid,main_thrid))
			start_servstat_thread();
		else {
			retest_flag=1;
			if((rv=pthread_cond_signal(&server_test_cond))) {
				DEBUG_MSG("test_onquery(): couldn't signal server status thread: %s\n",strerror(rv));
			}
		}
	}

	pthread_mutex_unlock(&servers_lock);
}

/* non-exclusive lock, for read only access to server data. */
void lock_server_data()
{
	pthread_mutex_lock(&servers_lock);
	++server_data_users;
	pthread_mutex_unlock(&servers_lock);
}

void unlock_server_data()
{
	pthread_mutex_lock(&servers_lock);
	PDNSD_ASSERT(server_data_users>0, "server_data_users non-positive before attempt to decrement it");
	if (--server_data_users==0) pthread_cond_broadcast(&server_data_cond);
	pthread_mutex_unlock(&servers_lock);
}

/* Try to obtain an exclusive lock, needed for modifying server data.
   Return 1 on success, 0 on failure (time out after tm seconds).
*/
int exclusive_lock_server_data(int tm)
{
	struct timeval now;
	struct timespec timeout;

	pthread_mutex_lock(&servers_lock);
	if(server_status_ping>0 && !pthread_equal(servstat_thrid,main_thrid)) {
		int err;
		/* Try to interrupt server status thread to prevent delays. */
		DEBUG_MSG("Sending server status thread an interrupt signal.\n");
		if((err=pthread_kill(servstat_thrid,statusintsig))) {
			DEBUG_MSG("pthread_kill failed: %s\n",strerror(err));
		}
	}
	gettimeofday(&now,NULL);
	timeout.tv_sec = now.tv_sec + tm;     /* time out after tm seconds */
	timeout.tv_nsec = now.tv_usec * 1000;
	while (server_data_users>0) {
		if(pthread_cond_timedwait(&server_data_cond, &servers_lock, &timeout) == ETIMEDOUT) {
			pthread_mutex_unlock(&servers_lock);
			return 0;
		}
	}
	return 1;
}
/* Call this to free the lock obtained with exclusive_lock_server_data().
   If retest is nonzero, the server-status thread is reactivated to check
   which servers are up. This is useful in case the configuration has changed.
*/
void exclusive_unlock_server_data(int retest)
{
	if(retest) {
		if(pthread_equal(servstat_thrid,main_thrid))
			start_servstat_thread();
		else
			pthread_cond_signal(&server_test_cond);
	}
	pthread_mutex_unlock(&servers_lock);
}

/*
  Change addresses of servers during runtime.
  i is the number of the server section to change.
  ar should point to an array of IP addresses (may be NULL).
  up=1 or up=0 means mark server up or down afterwards,
  up=-1 means retest.

  A non-zero return value indicates an error.
*/
int change_servers(int i, addr_array ar, int up)
{
	int retval=0,j,change,signal_test;
	int n;
	servparm_t *sp;

	pthread_mutex_lock(&servers_lock);

	signal_test=0;
	change=0;
	n=DA_NEL(ar);
	sp=&DA_INDEX(servers,i);
	if(n != DA_NEL(sp->atup_a) || sp->rootserver>1)
		change=1;
	else {
		int j;
		for(j=0;j<n;++j)
			if(!same_inaddr2(&DA_INDEX(ar,j),&DA_INDEX(sp->atup_a,j).a)) {
				change=1;
				break;
			}
	}
	if(change) {
		/* we need exclusive access to the server data to make the changes */
		struct timeval now;
		struct timespec timeout;
		atup_array ata;

		if(server_status_ping>0 && !pthread_equal(servstat_thrid,main_thrid)) {
			int err;
			/* Try to interrupt server status thread to prevent delays. */
			DEBUG_MSG("Sending server status thread an interrupt signal.\n");
			if((err=pthread_kill(servstat_thrid,statusintsig))) {
				DEBUG_MSG("pthread_kill failed: %s\n",strerror(err));
			}
		}

		DEBUG_MSG("Changing IPs of server section #%d\n",i);
		gettimeofday(&now,NULL);
		timeout.tv_sec = now.tv_sec + 60;     /* time out after 60 seconds */
		timeout.tv_nsec = now.tv_usec * 1000;
		while (server_data_users>0) {
			if(pthread_cond_timedwait(&server_data_cond, &servers_lock, &timeout) == ETIMEDOUT) {
				retval=ETIMEDOUT;
				goto unlock_mutex;
			}
		}

		ata= DA_CREATE(atup_array, n);
		if(!ata) {
			log_warn("Out of memory in change_servers().");
			retval=ENOMEM;
			goto unlock_mutex;
		}
		da_free(sp->atup_a);
		sp->atup_a=ata;
		/* Stop trying to discover rootservers
		   if we set the addresses using this routine. */
		if(sp->rootserver>1) sp->rootserver=1;
	}

	for(j=0; j<n; ++j) {
		atup_t *at = &DA_INDEX(sp->atup_a,j);
		if(change) {
			SET_PDNSD_A2(&at->a, &DA_INDEX(ar,j));
			at->is_up=sp->preset;
		}
		if(up>=0) {
			at->is_up=up;
			at->i_ts=time(NULL);
		}
		else if(change || at->i_ts) {
			/* A test may take a while, and we don't want to hold
			   up the calling thread.
			   Instead we set the timestamp to zero and signal
			   a condition which should wake up the server test thread.
			*/
			at->i_ts=0;
			signal_test=1;
		}
	}

	if(signal_test) {
		if(pthread_equal(servstat_thrid,main_thrid))
			retval=start_servstat_thread();
		else {
			retest_flag=1;
			retval=pthread_cond_signal(&server_test_cond);
		}
	}

 unlock_mutex:
	pthread_mutex_unlock(&servers_lock);
	return retval;
}


/*
  The signal handler for the signal to tell the server status thread to discontinue testing.
*/
static void sigint_handler(int signum)
{
	signal_interrupt=1;
}
