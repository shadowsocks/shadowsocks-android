/*****************************************************************************
* auth.c - Network Authentication and Phase Control program file.
*
* Copyright (c) 2003 by Marc Boucher, Services Informatiques (MBSI) inc.
* Copyright (c) 1997 by Global Election Systems Inc.  All rights reserved.
*
* The authors hereby grant permission to use, copy, modify, distribute,
* and license this software and its documentation for any purpose, provided
* that existing copyright notices are retained in all copies and that this
* notice and the following disclaimer are included verbatim in any 
* distributions. No written agreement, license, or royalty fee is required
* for any of the authorized uses.
*
* THIS SOFTWARE IS PROVIDED BY THE CONTRIBUTORS *AS IS* AND ANY EXPRESS OR
* IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
* IN NO EVENT SHALL THE CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
* NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
* THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
******************************************************************************
* REVISION HISTORY
*
* 03-01-01 Marc Boucher <marc@mbsi.ca>
*   Ported to lwIP.
* 97-12-08 Guy Lancaster <lancasterg@acm.org>, Global Election Systems Inc.
*   Ported from public pppd code.
*****************************************************************************/
/*
 * auth.c - PPP authentication and phase control.
 *
 * Copyright (c) 1993 The Australian National University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the Australian National University.  The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Copyright (c) 1989 Carnegie Mellon University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Carnegie Mellon University.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "lwip/opt.h"

#if PPP_SUPPORT /* don't build if not configured for use in lwipopts.h */

#include "ppp_impl.h"
#include "pppdebug.h"

#include "fsm.h"
#include "lcp.h"
#include "pap.h"
#include "chap.h"
#include "auth.h"
#include "ipcp.h"

#if CBCP_SUPPORT
#include "cbcp.h"
#endif /* CBCP_SUPPORT */

#include "lwip/inet.h"

#include <string.h>

#if 0 /* UNUSED */
/* Bits in scan_authfile return value */
#define NONWILD_SERVER  1
#define NONWILD_CLIENT  2

#define ISWILD(word)  (word[0] == '*' && word[1] == 0)
#endif /* UNUSED */

#if PAP_SUPPORT || CHAP_SUPPORT
/* The name by which the peer authenticated itself to us. */
static char peer_authname[MAXNAMELEN];
#endif /* PAP_SUPPORT || CHAP_SUPPORT */

/* Records which authentication operations haven't completed yet. */
static int auth_pending[NUM_PPP];

/* Set if we have successfully called plogin() */
static int logged_in;

/* Set if we have run the /etc/ppp/auth-up script. */
static int did_authup; /* @todo, we don't need this in lwip*/

/* List of addresses which the peer may use. */
static struct wordlist *addresses[NUM_PPP];

#if 0 /* UNUSED */
/* Wordlist giving addresses which the peer may use
   without authenticating itself. */
static struct wordlist *noauth_addrs;

/* Extra options to apply, from the secrets file entry for the peer. */
static struct wordlist *extra_options;
#endif /* UNUSED */

/* Number of network protocols which we have opened. */
static int num_np_open;

/* Number of network protocols which have come up. */
static int num_np_up;

#if PAP_SUPPORT || CHAP_SUPPORT
/* Set if we got the contents of passwd[] from the pap-secrets file. */
static int passwd_from_file;
#endif /* PAP_SUPPORT || CHAP_SUPPORT */

#if 0 /* UNUSED */
/* Set if we require authentication only because we have a default route. */
static bool default_auth;

/* Hook to enable a plugin to control the idle time limit */
int (*idle_time_hook) __P((struct ppp_idle *)) = NULL;

/* Hook for a plugin to say whether we can possibly authenticate any peer */
int (*pap_check_hook) __P((void)) = NULL;

/* Hook for a plugin to check the PAP user and password */
int (*pap_auth_hook) __P((char *user, char *passwd, char **msgp,
        struct wordlist **paddrs,
        struct wordlist **popts)) = NULL;

/* Hook for a plugin to know about the PAP user logout */
void (*pap_logout_hook) __P((void)) = NULL;

/* Hook for a plugin to get the PAP password for authenticating us */
int (*pap_passwd_hook) __P((char *user, char *passwd)) = NULL;

/*
 * This is used to ensure that we don't start an auth-up/down
 * script while one is already running.
 */
enum script_state {
    s_down,
    s_up
};

static enum script_state auth_state = s_down;
static enum script_state auth_script_state = s_down;
static pid_t auth_script_pid = 0;

/*
 * Option variables.
 * lwip: some of these are present in the ppp_settings structure
 */
bool uselogin = 0;            /* Use /etc/passwd for checking PAP */
bool cryptpap = 0;            /* Passwords in pap-secrets are encrypted */
bool refuse_pap = 0;          /* Don't wanna auth. ourselves with PAP */
bool refuse_chap = 0;         /* Don't wanna auth. ourselves with CHAP */
bool usehostname = 0;         /* Use hostname for our_name */
bool auth_required = 0;       /* Always require authentication from peer */
bool allow_any_ip = 0;        /* Allow peer to use any IP address */
bool explicit_remote = 0;     /* User specified explicit remote name */
char remote_name[MAXNAMELEN]; /* Peer's name for authentication */

#endif /* UNUSED */

/* Bits in auth_pending[] */
#define PAP_WITHPEER    1
#define PAP_PEER        2
#define CHAP_WITHPEER   4
#define CHAP_PEER       8

/* @todo, move this somewhere */
/* Used for storing a sequence of words.  Usually malloced. */
struct wordlist {
  struct wordlist *next;
  char        word[1];
};


extern char *crypt (const char *, const char *);

/* Prototypes for procedures local to this file. */

static void network_phase (int);
static void check_idle (void *);
static void connect_time_expired (void *);
#if 0
static int  plogin (char *, char *, char **, int *);
#endif
static void plogout (void);
static int  null_login (int);
static int  get_pap_passwd (int, char *, char *);
static int  have_pap_secret (void);
static int  have_chap_secret (char *, char *, u32_t);
static int  ip_addr_check (u32_t, struct wordlist *);

#if 0 /* PAP_SUPPORT || CHAP_SUPPORT */
static int  scan_authfile (FILE *, char *, char *, char *,
             struct wordlist **, struct wordlist **,
             char *);
static void free_wordlist (struct wordlist *);
static void auth_script (char *);
static void auth_script_done (void *);
static void set_allowed_addrs (int unit, struct wordlist *addrs);
static int  some_ip_ok (struct wordlist *);
static int  setupapfile (char **);
static int  privgroup (char **);
static int  set_noauth_addr (char **);
static void check_access (FILE *, char *);
#endif /* 0 */ /* PAP_SUPPORT || CHAP_SUPPORT */

#if 0 /* UNUSED */
/*
 * Authentication-related options.
 */
option_t auth_options[] = {
    { "require-pap", o_bool, &lcp_wantoptions[0].neg_upap,
      "Require PAP authentication from peer", 1, &auth_required },
    { "+pap", o_bool, &lcp_wantoptions[0].neg_upap,
      "Require PAP authentication from peer", 1, &auth_required },
    { "refuse-pap", o_bool, &refuse_pap,
      "Don't agree to auth to peer with PAP", 1 },
    { "-pap", o_bool, &refuse_pap,
      "Don't allow PAP authentication with peer", 1 },
    { "require-chap", o_bool, &lcp_wantoptions[0].neg_chap,
      "Require CHAP authentication from peer", 1, &auth_required },
    { "+chap", o_bool, &lcp_wantoptions[0].neg_chap,
      "Require CHAP authentication from peer", 1, &auth_required },
    { "refuse-chap", o_bool, &refuse_chap,
      "Don't agree to auth to peer with CHAP", 1 },
    { "-chap", o_bool, &refuse_chap,
      "Don't allow CHAP authentication with peer", 1 },
    { "name", o_string, our_name,
      "Set local name for authentication",
      OPT_PRIV|OPT_STATIC, NULL, MAXNAMELEN },
    { "user", o_string, user,
      "Set name for auth with peer", OPT_STATIC, NULL, MAXNAMELEN },
    { "usehostname", o_bool, &usehostname,
      "Must use hostname for authentication", 1 },
    { "remotename", o_string, remote_name,
      "Set remote name for authentication", OPT_STATIC,
      &explicit_remote, MAXNAMELEN },
    { "auth", o_bool, &auth_required,
      "Require authentication from peer", 1 },
    { "noauth", o_bool, &auth_required,
      "Don't require peer to authenticate", OPT_PRIV, &allow_any_ip },
    {  "login", o_bool, &uselogin,
      "Use system password database for PAP", 1 },
    { "papcrypt", o_bool, &cryptpap,
      "PAP passwords are encrypted", 1 },
    { "+ua", o_special, (void *)setupapfile,
      "Get PAP user and password from file" },
    { "password", o_string, passwd,
      "Password for authenticating us to the peer", OPT_STATIC,
      NULL, MAXSECRETLEN },
    { "privgroup", o_special, (void *)privgroup,
      "Allow group members to use privileged options", OPT_PRIV },
    { "allow-ip", o_special, (void *)set_noauth_addr,
      "Set IP address(es) which can be used without authentication",
      OPT_PRIV },
    { NULL }
};
#endif /* UNUSED */
#if 0 /* UNUSED */
/*
 * setupapfile - specifies UPAP info for authenticating with peer.
 */
static int
setupapfile(char **argv)
{
    FILE * ufile;
    int l;

    lcp_allowoptions[0].neg_upap = 1;

    /* open user info file */
    seteuid(getuid());
    ufile = fopen(*argv, "r");
    seteuid(0);
    if (ufile == NULL) {
      option_error("unable to open user login data file %s", *argv);
      return 0;
    }
    check_access(ufile, *argv);

    /* get username */
    if (fgets(user, MAXNAMELEN - 1, ufile) == NULL
        || fgets(passwd, MAXSECRETLEN - 1, ufile) == NULL){
      option_error("unable to read user login data file %s", *argv);
      return 0;
    }
    fclose(ufile);

    /* get rid of newlines */
    l = strlen(user);
    if (l > 0 && user[l-1] == '\n')
      user[l-1] = 0;
    l = strlen(passwd);
    if (l > 0 && passwd[l-1] == '\n')
      passwd[l-1] = 0;

    return (1);
}
#endif /* UNUSED */

#if 0 /* UNUSED */
/*
 * privgroup - allow members of the group to have privileged access.
 */
static int
privgroup(char **argv)
{
    struct group *g;
    int i;

    g = getgrnam(*argv);
    if (g == 0) {
      option_error("group %s is unknown", *argv);
      return 0;
    }
    for (i = 0; i < ngroups; ++i) {
      if (groups[i] == g->gr_gid) {
        privileged = 1;
        break;
      }
    }
    return 1;
}
#endif

#if 0 /* UNUSED */
/*
 * set_noauth_addr - set address(es) that can be used without authentication.
 * Equivalent to specifying an entry like `"" * "" addr' in pap-secrets.
 */
static int
set_noauth_addr(char **argv)
{
    char *addr = *argv;
    int l = strlen(addr);
    struct wordlist *wp;

    wp = (struct wordlist *) malloc(sizeof(struct wordlist) + l + 1);
    if (wp == NULL)
      novm("allow-ip argument");
    wp->word = (char *) (wp + 1);
    wp->next = noauth_addrs;
    BCOPY(addr, wp->word, l);
    noauth_addrs = wp;
    return 1;
}
#endif /* UNUSED */

/*
 * An Open on LCP has requested a change from Dead to Establish phase.
 * Do what's necessary to bring the physical layer up.
 */
void
link_required(int unit)
{
  LWIP_UNUSED_ARG(unit);

  AUTHDEBUG(LOG_INFO, ("link_required: %d\n", unit));
}

/*
 * LCP has terminated the link; go to the Dead phase and take the
 * physical layer down.
 */
void
link_terminated(int unit)
{
  AUTHDEBUG(LOG_INFO, ("link_terminated: %d\n", unit));
  if (lcp_phase[unit] == PHASE_DEAD) {
    return;
  }
  if (logged_in) {
    plogout();
  }
  lcp_phase[unit] = PHASE_DEAD;
  AUTHDEBUG(LOG_NOTICE, ("Connection terminated.\n"));
  pppLinkTerminated(unit);
}

/*
 * LCP has gone down; it will either die or try to re-establish.
 */
void
link_down(int unit)
{
  int i;
  struct protent *protp;

  AUTHDEBUG(LOG_INFO, ("link_down: %d\n", unit));

  if (did_authup) {
    /* XXX Do link down processing. */
    did_authup = 0;
  }
  for (i = 0; (protp = ppp_protocols[i]) != NULL; ++i) {
    if (!protp->enabled_flag) {
      continue;
    }
    if (protp->protocol != PPP_LCP && protp->lowerdown != NULL) {
      (*protp->lowerdown)(unit);
    }
    if (protp->protocol < 0xC000 && protp->close != NULL) {
      (*protp->close)(unit, "LCP down");
    }
  }
  num_np_open = 0;  /* number of network protocols we have opened */
  num_np_up = 0;    /* Number of network protocols which have come up */

  if (lcp_phase[unit] != PHASE_DEAD) {
    lcp_phase[unit] = PHASE_TERMINATE;
  }
  pppLinkDown(unit);
}

/*
 * The link is established.
 * Proceed to the Dead, Authenticate or Network phase as appropriate.
 */
void
link_established(int unit)
{
  int auth;
  int i;
  struct protent *protp;
  lcp_options *wo = &lcp_wantoptions[unit];
  lcp_options *go = &lcp_gotoptions[unit];
#if PAP_SUPPORT || CHAP_SUPPORT
  lcp_options *ho = &lcp_hisoptions[unit];
#endif /* PAP_SUPPORT || CHAP_SUPPORT */

  AUTHDEBUG(LOG_INFO, ("link_established: unit %d; Lowering up all protocols...\n", unit));
  /*
   * Tell higher-level protocols that LCP is up.
   */
  for (i = 0; (protp = ppp_protocols[i]) != NULL; ++i) {
    if (protp->protocol != PPP_LCP && protp->enabled_flag && protp->lowerup != NULL) {
      (*protp->lowerup)(unit);
    }
  }
  if (ppp_settings.auth_required && !(go->neg_chap || go->neg_upap)) {
    /*
     * We wanted the peer to authenticate itself, and it refused:
     * treat it as though it authenticated with PAP using a username
     * of "" and a password of "".  If that's not OK, boot it out.
     */
    if (!wo->neg_upap || !null_login(unit)) {
      AUTHDEBUG(LOG_WARNING, ("peer refused to authenticate\n"));
      lcp_close(unit, "peer refused to authenticate");
      return;
    }
  }

  lcp_phase[unit] = PHASE_AUTHENTICATE;
  auth = 0;
#if CHAP_SUPPORT
  if (go->neg_chap) {
    ChapAuthPeer(unit, ppp_settings.our_name, go->chap_mdtype);
    auth |= CHAP_PEER;
  } 
#endif /* CHAP_SUPPORT */
#if PAP_SUPPORT && CHAP_SUPPORT
  else
#endif /* PAP_SUPPORT && CHAP_SUPPORT */
#if PAP_SUPPORT
  if (go->neg_upap) {
    upap_authpeer(unit);
    auth |= PAP_PEER;
  }
#endif /* PAP_SUPPORT */
#if CHAP_SUPPORT
  if (ho->neg_chap) {
    ChapAuthWithPeer(unit, ppp_settings.user, ho->chap_mdtype);
    auth |= CHAP_WITHPEER;
  }
#endif /* CHAP_SUPPORT */
#if PAP_SUPPORT && CHAP_SUPPORT
  else
#endif /* PAP_SUPPORT && CHAP_SUPPORT */
#if PAP_SUPPORT
  if (ho->neg_upap) {
    if (ppp_settings.passwd[0] == 0) {
      passwd_from_file = 1;
      if (!get_pap_passwd(unit, ppp_settings.user, ppp_settings.passwd)) {
        AUTHDEBUG(LOG_ERR, ("No secret found for PAP login\n"));
      }
    }
    upap_authwithpeer(unit, ppp_settings.user, ppp_settings.passwd);
    auth |= PAP_WITHPEER;
  }
#endif /* PAP_SUPPORT */
  auth_pending[unit] = auth;

  if (!auth) {
    network_phase(unit);
  }
}

/*
 * Proceed to the network phase.
 */
static void
network_phase(int unit)
{
  int i;
  struct protent *protp;
  lcp_options *go = &lcp_gotoptions[unit];

  /*
   * If the peer had to authenticate, run the auth-up script now.
   */
  if ((go->neg_chap || go->neg_upap) && !did_authup) {
    /* XXX Do setup for peer authentication. */
    did_authup = 1;
  }

#if CBCP_SUPPORT
  /*
   * If we negotiated callback, do it now.
   */
  if (go->neg_cbcp) {
    lcp_phase[unit] = PHASE_CALLBACK;
    (*cbcp_protent.open)(unit);
    return;
  }
#endif /* CBCP_SUPPORT */

  lcp_phase[unit] = PHASE_NETWORK;
  for (i = 0; (protp = ppp_protocols[i]) != NULL; ++i) {
    if (protp->protocol < 0xC000 && protp->enabled_flag && protp->open != NULL) {
      (*protp->open)(unit);
      if (protp->protocol != PPP_CCP) {
        ++num_np_open;
      }
    }
  }

  if (num_np_open == 0) {
    /* nothing to do */
    lcp_close(0, "No network protocols running");
  }
}
/* @todo: add void start_networks(void) here (pppd 2.3.11) */

/*
 * The peer has failed to authenticate himself using `protocol'.
 */
void
auth_peer_fail(int unit, u16_t protocol)
{
  LWIP_UNUSED_ARG(protocol);

  AUTHDEBUG(LOG_INFO, ("auth_peer_fail: %d proto=%X\n", unit, protocol));
  /*
   * Authentication failure: take the link down
   */
  lcp_close(unit, "Authentication failed");
}


#if PAP_SUPPORT || CHAP_SUPPORT
/*
 * The peer has been successfully authenticated using `protocol'.
 */
void
auth_peer_success(int unit, u16_t protocol, char *name, int namelen)
{
  int pbit;

  AUTHDEBUG(LOG_INFO, ("auth_peer_success: %d proto=%X\n", unit, protocol));
  switch (protocol) {
    case PPP_CHAP:
      pbit = CHAP_PEER;
      break;
    case PPP_PAP:
      pbit = PAP_PEER;
      break;
    default:
      AUTHDEBUG(LOG_WARNING, ("auth_peer_success: unknown protocol %x\n", protocol));
      return;
  }

  /*
   * Save the authenticated name of the peer for later.
   */
  if (namelen > (int)sizeof(peer_authname) - 1) {
    namelen = sizeof(peer_authname) - 1;
  }
  BCOPY(name, peer_authname, namelen);
  peer_authname[namelen] = 0;
  
  /*
   * If there is no more authentication still to be done,
   * proceed to the network (or callback) phase.
   */
  if ((auth_pending[unit] &= ~pbit) == 0) {
    network_phase(unit);
  }
}

/*
 * We have failed to authenticate ourselves to the peer using `protocol'.
 */
void
auth_withpeer_fail(int unit, u16_t protocol)
{
  int errCode = PPPERR_AUTHFAIL;

  LWIP_UNUSED_ARG(protocol);

  AUTHDEBUG(LOG_INFO, ("auth_withpeer_fail: %d proto=%X\n", unit, protocol));
  if (passwd_from_file) {
    BZERO(ppp_settings.passwd, MAXSECRETLEN);
  }

  /*
   * We've failed to authenticate ourselves to our peer.
   * He'll probably take the link down, and there's not much
   * we can do except wait for that.
   */
  pppIOCtl(unit, PPPCTLS_ERRCODE, &errCode);
  lcp_close(unit, "Failed to authenticate ourselves to peer");
}

/*
 * We have successfully authenticated ourselves with the peer using `protocol'.
 */
void
auth_withpeer_success(int unit, u16_t protocol)
{
  int pbit;

  AUTHDEBUG(LOG_INFO, ("auth_withpeer_success: %d proto=%X\n", unit, protocol));
  switch (protocol) {
    case PPP_CHAP:
      pbit = CHAP_WITHPEER;
      break;
    case PPP_PAP:
      if (passwd_from_file) {
        BZERO(ppp_settings.passwd, MAXSECRETLEN);
      }
      pbit = PAP_WITHPEER;
      break;
    default:
      AUTHDEBUG(LOG_WARNING, ("auth_peer_success: unknown protocol %x\n", protocol));
      pbit = 0;
  }

  /*
   * If there is no more authentication still being done,
   * proceed to the network (or callback) phase.
   */
  if ((auth_pending[unit] &= ~pbit) == 0) {
    network_phase(unit);
  }
}
#endif /* PAP_SUPPORT || CHAP_SUPPORT */


/*
 * np_up - a network protocol has come up.
 */
void
np_up(int unit, u16_t proto)
{
  LWIP_UNUSED_ARG(unit);
  LWIP_UNUSED_ARG(proto);

  AUTHDEBUG(LOG_INFO, ("np_up: %d proto=%X\n", unit, proto));
  if (num_np_up == 0) {
    AUTHDEBUG(LOG_INFO, ("np_up: maxconnect=%d idle_time_limit=%d\n",ppp_settings.maxconnect,ppp_settings.idle_time_limit));
    /*
     * At this point we consider that the link has come up successfully.
     */
    if (ppp_settings.idle_time_limit > 0) {
      TIMEOUT(check_idle, NULL, ppp_settings.idle_time_limit);
    }

    /*
     * Set a timeout to close the connection once the maximum
     * connect time has expired.
     */
    if (ppp_settings.maxconnect > 0) {
      TIMEOUT(connect_time_expired, 0, ppp_settings.maxconnect);
    }
  }
  ++num_np_up;
}

/*
 * np_down - a network protocol has gone down.
 */
void
np_down(int unit, u16_t proto)
{
  LWIP_UNUSED_ARG(unit);
  LWIP_UNUSED_ARG(proto);

  AUTHDEBUG(LOG_INFO, ("np_down: %d proto=%X\n", unit, proto));
  if (--num_np_up == 0 && ppp_settings.idle_time_limit > 0) {
    UNTIMEOUT(check_idle, NULL);
  }
}

/*
 * np_finished - a network protocol has finished using the link.
 */
void
np_finished(int unit, u16_t proto)
{
  LWIP_UNUSED_ARG(unit);
  LWIP_UNUSED_ARG(proto);

  AUTHDEBUG(LOG_INFO, ("np_finished: %d proto=%X\n", unit, proto));
  if (--num_np_open <= 0) {
    /* no further use for the link: shut up shop. */
    lcp_close(0, "No network protocols running");
  }
}

/*
 * check_idle - check whether the link has been idle for long
 * enough that we can shut it down.
 */
static void
check_idle(void *arg)
{
  struct ppp_idle idle;
  u_short itime;
  
  LWIP_UNUSED_ARG(arg);
  if (!get_idle_time(0, &idle)) {
    return;
  }
  itime = LWIP_MIN(idle.xmit_idle, idle.recv_idle);
  if (itime >= ppp_settings.idle_time_limit) {
    /* link is idle: shut it down. */
    AUTHDEBUG(LOG_INFO, ("Terminating connection due to lack of activity.\n"));
    lcp_close(0, "Link inactive");
  } else {
    TIMEOUT(check_idle, NULL, ppp_settings.idle_time_limit - itime);
  }
}

/*
 * connect_time_expired - log a message and close the connection.
 */
static void
connect_time_expired(void *arg)
{
  LWIP_UNUSED_ARG(arg);

  AUTHDEBUG(LOG_INFO, ("Connect time expired\n"));
  lcp_close(0, "Connect time expired");   /* Close connection */
}

#if 0 /* UNUSED */
/*
 * auth_check_options - called to check authentication options.
 */
void
auth_check_options(void)
{
  lcp_options *wo = &lcp_wantoptions[0];
  int can_auth;
  ipcp_options *ipwo = &ipcp_wantoptions[0];
  u32_t remote;

  /* Default our_name to hostname, and user to our_name */
  if (ppp_settings.our_name[0] == 0 || ppp_settings.usehostname) {
      strcpy(ppp_settings.our_name, ppp_settings.hostname);
  }

  if (ppp_settings.user[0] == 0) {
    strcpy(ppp_settings.user, ppp_settings.our_name);
  }

  /* If authentication is required, ask peer for CHAP or PAP. */
  if (ppp_settings.auth_required && !wo->neg_chap && !wo->neg_upap) {
    wo->neg_chap = 1;
    wo->neg_upap = 1;
  }
  
  /*
   * Check whether we have appropriate secrets to use
   * to authenticate the peer.
   */
  can_auth = wo->neg_upap && have_pap_secret();
  if (!can_auth && wo->neg_chap) {
    remote = ipwo->accept_remote? 0: ipwo->hisaddr;
    can_auth = have_chap_secret(ppp_settings.remote_name, ppp_settings.our_name, remote);
  }

  if (ppp_settings.auth_required && !can_auth) {
    ppp_panic("No auth secret");
  }
}
#endif /* UNUSED */

/*
 * auth_reset - called when LCP is starting negotiations to recheck
 * authentication options, i.e. whether we have appropriate secrets
 * to use for authenticating ourselves and/or the peer.
 */
void
auth_reset(int unit)
{
  lcp_options *go = &lcp_gotoptions[unit];
  lcp_options *ao = &lcp_allowoptions[0];
  ipcp_options *ipwo = &ipcp_wantoptions[0];
  u32_t remote;

  AUTHDEBUG(LOG_INFO, ("auth_reset: %d\n", unit));
  ao->neg_upap = !ppp_settings.refuse_pap && (ppp_settings.passwd[0] != 0 || get_pap_passwd(unit, NULL, NULL));
  ao->neg_chap = !ppp_settings.refuse_chap && ppp_settings.passwd[0] != 0 /*have_chap_secret(ppp_settings.user, ppp_settings.remote_name, (u32_t)0)*/;

  if (go->neg_upap && !have_pap_secret()) {
    go->neg_upap = 0;
  }
  if (go->neg_chap) {
    remote = ipwo->accept_remote? 0: ipwo->hisaddr;
    if (!have_chap_secret(ppp_settings.remote_name, ppp_settings.our_name, remote)) {
      go->neg_chap = 0;
    }
  }
}

#if PAP_SUPPORT
/*
 * check_passwd - Check the user name and passwd against the PAP secrets
 * file.  If requested, also check against the system password database,
 * and login the user if OK.
 *
 * returns:
 *  UPAP_AUTHNAK: Authentication failed.
 *  UPAP_AUTHACK: Authentication succeeded.
 * In either case, msg points to an appropriate message.
 */
u_char
check_passwd( int unit, char *auser, int userlen, char *apasswd, int passwdlen, char **msg, int *msglen)
{
#if 1 /* XXX Assume all entries OK. */
  LWIP_UNUSED_ARG(unit);
  LWIP_UNUSED_ARG(auser);
  LWIP_UNUSED_ARG(userlen);
  LWIP_UNUSED_ARG(apasswd);
  LWIP_UNUSED_ARG(passwdlen);
  LWIP_UNUSED_ARG(msglen);
  *msg = (char *) 0;
  return UPAP_AUTHACK;     /* XXX Assume all entries OK. */
#else
  u_char ret = 0;
  struct wordlist *addrs = NULL;
  char passwd[256], user[256];
  char secret[MAXWORDLEN];
  static u_short attempts = 0;
  
  /*
   * Make copies of apasswd and auser, then null-terminate them.
   */
  BCOPY(apasswd, passwd, passwdlen);
  passwd[passwdlen] = '\0';
  BCOPY(auser, user, userlen);
  user[userlen] = '\0';
  *msg = (char *) 0;

  /* XXX Validate user name and password. */
  ret = UPAP_AUTHACK;     /* XXX Assume all entries OK. */
      
  if (ret == UPAP_AUTHNAK) {
    if (*msg == (char *) 0) {
      *msg = "Login incorrect";
    }
    *msglen = strlen(*msg);
    /*
     * Frustrate passwd stealer programs.
     * Allow 10 tries, but start backing off after 3 (stolen from login).
     * On 10'th, drop the connection.
     */
    if (attempts++ >= 10) {
      AUTHDEBUG(LOG_WARNING, ("%d LOGIN FAILURES BY %s\n", attempts, user));
      /*ppp_panic("Excess Bad Logins");*/
    }
    if (attempts > 3) {
      /* @todo: this was sleep(), i.e. seconds, not milliseconds
       * I don't think we really need this in lwIP - we would block tcpip_thread!
       */
      /*sys_msleep((attempts - 3) * 5);*/
    }
    if (addrs != NULL) {
      free_wordlist(addrs);
    }
  } else {
    attempts = 0; /* Reset count */
    if (*msg == (char *) 0) {
      *msg = "Login ok";
    }
    *msglen = strlen(*msg);
    set_allowed_addrs(unit, addrs);
  }

  BZERO(passwd, sizeof(passwd));
  BZERO(secret, sizeof(secret));

  return ret;
#endif
}
#endif /* PAP_SUPPORT */

#if 0 /* UNUSED */
/*
 * This function is needed for PAM.
 */

#ifdef USE_PAM

/* lwip does not support PAM*/

#endif  /* USE_PAM */

#endif /* UNUSED */


#if 0 /* UNUSED */
/*
 * plogin - Check the user name and password against the system
 * password database, and login the user if OK.
 *
 * returns:
 *  UPAP_AUTHNAK: Login failed.
 *  UPAP_AUTHACK: Login succeeded.
 * In either case, msg points to an appropriate message.
 */
static int
plogin(char *user, char *passwd, char **msg, int *msglen)
{

  LWIP_UNUSED_ARG(user);
  LWIP_UNUSED_ARG(passwd);
  LWIP_UNUSED_ARG(msg);
  LWIP_UNUSED_ARG(msglen);


 /* The new lines are here align the file when 
  * compared against the pppd 2.3.11 code */
















  /* XXX Fail until we decide that we want to support logins. */
  return (UPAP_AUTHNAK);
}
#endif



/*
 * plogout - Logout the user.
 */
static void
plogout(void)
{
  logged_in = 0;
}

/*
 * null_login - Check if a username of "" and a password of "" are
 * acceptable, and iff so, set the list of acceptable IP addresses
 * and return 1.
 */
static int
null_login(int unit)
{
  LWIP_UNUSED_ARG(unit);
  /* XXX Fail until we decide that we want to support logins. */
  return 0;
}


/*
 * get_pap_passwd - get a password for authenticating ourselves with
 * our peer using PAP.  Returns 1 on success, 0 if no suitable password
 * could be found.
 */
static int
get_pap_passwd(int unit, char *user, char *passwd)
{
  LWIP_UNUSED_ARG(unit);
/* normally we would reject PAP if no password is provided,
   but this causes problems with some providers (like CHT in Taiwan)
   who incorrectly request PAP and expect a bogus/empty password, so
   always provide a default user/passwd of "none"/"none"

   @todo: This should be configured by the user, instead of being hardcoded here!
*/
  if(user) {
    strcpy(user, "none");
  }
  if(passwd) {
    strcpy(passwd, "none");
  }
  return 1;
}

/*
 * have_pap_secret - check whether we have a PAP file with any
 * secrets that we could possibly use for authenticating the peer.
 */
static int
have_pap_secret(void)
{
  /* XXX Fail until we set up our passwords. */
  return 0;
}

/*
 * have_chap_secret - check whether we have a CHAP file with a
 * secret that we could possibly use for authenticating `client'
 * on `server'.  Either can be the null string, meaning we don't
 * know the identity yet.
 */
static int
have_chap_secret(char *client, char *server, u32_t remote)
{
  LWIP_UNUSED_ARG(client);
  LWIP_UNUSED_ARG(server);
  LWIP_UNUSED_ARG(remote);

  /* XXX Fail until we set up our passwords. */
  return 0;
}
#if CHAP_SUPPORT

/*
 * get_secret - open the CHAP secret file and return the secret
 * for authenticating the given client on the given server.
 * (We could be either client or server).
 */
int
get_secret(int unit, char *client, char *server, char *secret, int *secret_len, int save_addrs)
{
#if 1
  int len;
  struct wordlist *addrs;

  LWIP_UNUSED_ARG(unit);
  LWIP_UNUSED_ARG(server);
  LWIP_UNUSED_ARG(save_addrs);

  addrs = NULL;

  if(!client || !client[0] || strcmp(client, ppp_settings.user)) {
    return 0;
  }

  len = (int)strlen(ppp_settings.passwd);
  if (len > MAXSECRETLEN) {
    AUTHDEBUG(LOG_ERR, ("Secret for %s on %s is too long\n", client, server));
    len = MAXSECRETLEN;
  }

  BCOPY(ppp_settings.passwd, secret, len);
  *secret_len = len;

  return 1;
#else
  int ret = 0, len;
  struct wordlist *addrs;
  char secbuf[MAXWORDLEN];
  
  addrs = NULL;
  secbuf[0] = 0;

  /* XXX Find secret. */
  if (ret < 0) {
    return 0;
  }

  if (save_addrs) {
    set_allowed_addrs(unit, addrs);
  }

  len = strlen(secbuf);
  if (len > MAXSECRETLEN) {
    AUTHDEBUG(LOG_ERR, ("Secret for %s on %s is too long\n", client, server));
    len = MAXSECRETLEN;
  }

  BCOPY(secbuf, secret, len);
  BZERO(secbuf, sizeof(secbuf));
  *secret_len = len;

  return 1;
#endif
}
#endif /* CHAP_SUPPORT */


#if 0 /* PAP_SUPPORT || CHAP_SUPPORT */
/*
 * set_allowed_addrs() - set the list of allowed addresses.
 */
static void
set_allowed_addrs(int unit, struct wordlist *addrs)
{
  if (addresses[unit] != NULL) {
    free_wordlist(addresses[unit]);
  }
  addresses[unit] = addrs;

#if 0
  /*
   * If there's only one authorized address we might as well
   * ask our peer for that one right away
   */
  if (addrs != NULL && addrs->next == NULL) {
    char *p = addrs->word;
    struct ipcp_options *wo = &ipcp_wantoptions[unit];
    u32_t a;
    struct hostent *hp;
    
    if (wo->hisaddr == 0 && *p != '!' && *p != '-' && strchr(p, '/') == NULL) {
      hp = gethostbyname(p);
      if (hp != NULL && hp->h_addrtype == AF_INET) {
        a = *(u32_t *)hp->h_addr;
      } else {
        a = inet_addr(p);
      }
      if (a != (u32_t) -1) {
        wo->hisaddr = a;
      }
    }
  }
#endif
}
#endif /* 0 */ /* PAP_SUPPORT || CHAP_SUPPORT */

/*
 * auth_ip_addr - check whether the peer is authorized to use
 * a given IP address.  Returns 1 if authorized, 0 otherwise.
 */
int
auth_ip_addr(int unit, u32_t addr)
{
  return ip_addr_check(addr, addresses[unit]);
}

static int /* @todo: integrate this funtion into auth_ip_addr()*/
ip_addr_check(u32_t addr, struct wordlist *addrs)
{
  /* don't allow loopback or multicast address */
  if (bad_ip_adrs(addr)) {
    return 0;
  }

  if (addrs == NULL) {
    return !ppp_settings.auth_required; /* no addresses authorized */
  }

  /* XXX All other addresses allowed. */
  return 1;
}

/*
 * bad_ip_adrs - return 1 if the IP address is one we don't want
 * to use, such as an address in the loopback net or a multicast address.
 * addr is in network byte order.
 */
int
bad_ip_adrs(u32_t addr)
{
  addr = ntohl(addr);
  return (addr >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET
      || IN_MULTICAST(addr) || IN_BADCLASS(addr);
}

#if 0 /* UNUSED */ /* PAP_SUPPORT || CHAP_SUPPORT */
/*
 * some_ip_ok - check a wordlist to see if it authorizes any
 * IP address(es).
 */
static int
some_ip_ok(struct wordlist *addrs)
{
    for (; addrs != 0; addrs = addrs->next) {
      if (addrs->word[0] == '-')
        break;
      if (addrs->word[0] != '!')
        return 1; /* some IP address is allowed */
    }
    return 0;
}

/*
 * check_access - complain if a secret file has too-liberal permissions.
 */
static void
check_access(FILE *f, char *filename)
{
    struct stat sbuf;

    if (fstat(fileno(f), &sbuf) < 0) {
      warn("cannot stat secret file %s: %m", filename);
    } else if ((sbuf.st_mode & (S_IRWXG | S_IRWXO)) != 0) {
      warn("Warning - secret file %s has world and/or group access",
            filename);
    }
}


/*
 * scan_authfile - Scan an authorization file for a secret suitable
 * for authenticating `client' on `server'.  The return value is -1
 * if no secret is found, otherwise >= 0.  The return value has
 * NONWILD_CLIENT set if the secret didn't have "*" for the client, and
 * NONWILD_SERVER set if the secret didn't have "*" for the server.
 * Any following words on the line up to a "--" (i.e. address authorization
 * info) are placed in a wordlist and returned in *addrs.  Any
 * following words (extra options) are placed in a wordlist and
 * returned in *opts.
 * We assume secret is NULL or points to MAXWORDLEN bytes of space.
 */
static int
scan_authfile(FILE *f, char *client, char *server, char *secret, struct wordlist **addrs, struct wordlist **opts, char *filename)
{
  /* We do not (currently) need this in lwip  */
  return 0; /* dummy */
}
/*
 * free_wordlist - release memory allocated for a wordlist.
 */
static void
free_wordlist(struct wordlist *wp)
{
  struct wordlist *next;

  while (wp != NULL) {
    next = wp->next;
    free(wp);
    wp = next;
  }
}

/*
 * auth_script_done - called when the auth-up or auth-down script
 * has finished.
 */
static void
auth_script_done(void *arg)
{
    auth_script_pid = 0;
    switch (auth_script_state) {
    case s_up:
      if (auth_state == s_down) {
        auth_script_state = s_down;
        auth_script(_PATH_AUTHDOWN);
      }
      break;
    case s_down:
      if (auth_state == s_up) {
        auth_script_state = s_up;
        auth_script(_PATH_AUTHUP);
      }
      break;
    }
}

/*
 * auth_script - execute a script with arguments
 * interface-name peer-name real-user tty speed
 */
static void
auth_script(char *script)
{
    char strspeed[32];
    struct passwd *pw;
    char struid[32];
    char *user_name;
    char *argv[8];

    if ((pw = getpwuid(getuid())) != NULL && pw->pw_name != NULL)
      user_name = pw->pw_name;
    else {
      slprintf(struid, sizeof(struid), "%d", getuid());
      user_name = struid;
    }
    slprintf(strspeed, sizeof(strspeed), "%d", baud_rate);

    argv[0] = script;
    argv[1] = ifname;
    argv[2] = peer_authname;
    argv[3] = user_name;
    argv[4] = devnam;
    argv[5] = strspeed;
    argv[6] = NULL;

    auth_script_pid = run_program(script, argv, 0, auth_script_done, NULL);
}
#endif  /* 0 */ /* PAP_SUPPORT || CHAP_SUPPORT */
#endif /* PPP_SUPPORT */
