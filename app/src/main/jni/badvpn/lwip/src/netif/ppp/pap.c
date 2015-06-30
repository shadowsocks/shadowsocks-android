/*****************************************************************************
* pap.c - Network Password Authentication Protocol program file.
*
* Copyright (c) 2003 by Marc Boucher, Services Informatiques (MBSI) inc.
* portions Copyright (c) 1997 by Global Election Systems Inc.
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
* 97-12-12 Guy Lancaster <lancasterg@acm.org>, Global Election Systems Inc.
*   Original.
*****************************************************************************/
/*
 * upap.c - User/Password Authentication Protocol.
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

#if PAP_SUPPORT /* don't build if not configured for use in lwipopts.h */

#include "ppp_impl.h"
#include "pppdebug.h"

#include "auth.h"
#include "pap.h"

#include <string.h>

#if 0 /* UNUSED */
static bool hide_password = 1;

/*
 * Command-line options.
 */
static option_t pap_option_list[] = {
    { "hide-password", o_bool, &hide_password,
      "Don't output passwords to log", 1 },
    { "show-password", o_bool, &hide_password,
      "Show password string in debug log messages", 0 },
    { "pap-restart", o_int, &upap[0].us_timeouttime,
      "Set retransmit timeout for PAP" },
    { "pap-max-authreq", o_int, &upap[0].us_maxtransmits,
      "Set max number of transmissions for auth-reqs" },
    { "pap-timeout", o_int, &upap[0].us_reqtimeout,
      "Set time limit for peer PAP authentication" },
    { NULL }
};
#endif

/*
 * Protocol entry points.
 */
static void upap_init      (int);
static void upap_lowerup   (int);
static void upap_lowerdown (int);
static void upap_input     (int, u_char *, int);
static void upap_protrej   (int);
#if PPP_ADDITIONAL_CALLBACKS
static int  upap_printpkt (u_char *, int, void (*)(void *, char *, ...), void *);
#endif /* PPP_ADDITIONAL_CALLBACKS */

struct protent pap_protent = {
  PPP_PAP,
  upap_init,
  upap_input,
  upap_protrej,
  upap_lowerup,
  upap_lowerdown,
  NULL,
  NULL,
#if PPP_ADDITIONAL_CALLBACKS
  upap_printpkt,
  NULL,
#endif /* PPP_ADDITIONAL_CALLBACKS */
  1,
  "PAP",
#if PPP_ADDITIONAL_CALLBACKS
  NULL,
  NULL,
  NULL
#endif /* PPP_ADDITIONAL_CALLBACKS */
};

upap_state upap[NUM_PPP]; /* UPAP state; one for each unit */

static void upap_timeout   (void *);
static void upap_reqtimeout(void *);
static void upap_rauthreq  (upap_state *, u_char *, u_char, int);
static void upap_rauthack  (upap_state *, u_char *, int, int);
static void upap_rauthnak  (upap_state *, u_char *, int, int);
static void upap_sauthreq  (upap_state *);
static void upap_sresp     (upap_state *, u_char, u_char, char *, int);


/*
 * upap_init - Initialize a UPAP unit.
 */
static void
upap_init(int unit)
{
  upap_state *u = &upap[unit];

  UPAPDEBUG(LOG_INFO, ("upap_init: %d\n", unit));
  u->us_unit         = unit;
  u->us_user         = NULL;
  u->us_userlen      = 0;
  u->us_passwd       = NULL;
  u->us_passwdlen    = 0;
  u->us_clientstate  = UPAPCS_INITIAL;
  u->us_serverstate  = UPAPSS_INITIAL;
  u->us_id           = 0;
  u->us_timeouttime  = UPAP_DEFTIMEOUT;
  u->us_maxtransmits = 10;
  u->us_reqtimeout   = UPAP_DEFREQTIME;
}

/*
 * upap_authwithpeer - Authenticate us with our peer (start client).
 *
 * Set new state and send authenticate's.
 */
void
upap_authwithpeer(int unit, char *user, char *password)
{
  upap_state *u = &upap[unit];

  UPAPDEBUG(LOG_INFO, ("upap_authwithpeer: %d user=%s password=%s s=%d\n",
             unit, user, password, u->us_clientstate));

  /* Save the username and password we're given */
  u->us_user = user;
  u->us_userlen = (int)strlen(user);
  u->us_passwd = password;
  u->us_passwdlen = (int)strlen(password);

  u->us_transmits = 0;

  /* Lower layer up yet? */
  if (u->us_clientstate == UPAPCS_INITIAL ||
      u->us_clientstate == UPAPCS_PENDING) {
    u->us_clientstate = UPAPCS_PENDING;
    return;
  }

  upap_sauthreq(u);      /* Start protocol */
}


/*
 * upap_authpeer - Authenticate our peer (start server).
 *
 * Set new state.
 */
void
upap_authpeer(int unit)
{
  upap_state *u = &upap[unit];

  /* Lower layer up yet? */
  if (u->us_serverstate == UPAPSS_INITIAL ||
      u->us_serverstate == UPAPSS_PENDING) {
    u->us_serverstate = UPAPSS_PENDING;
    return;
  }

  u->us_serverstate = UPAPSS_LISTEN;
  if (u->us_reqtimeout > 0) {
    TIMEOUT(upap_reqtimeout, u, u->us_reqtimeout);
  }
}

/*
 * upap_timeout - Retransmission timer for sending auth-reqs expired.
 */
static void
upap_timeout(void *arg)
{
  upap_state *u = (upap_state *) arg;

  UPAPDEBUG(LOG_INFO, ("upap_timeout: %d timeout %d expired s=%d\n", 
        u->us_unit, u->us_timeouttime, u->us_clientstate));

  if (u->us_clientstate != UPAPCS_AUTHREQ) {
    UPAPDEBUG(LOG_INFO, ("upap_timeout: not in AUTHREQ state!\n"));
    return;
  }

  if (u->us_transmits >= u->us_maxtransmits) {
    /* give up in disgust */
    UPAPDEBUG(LOG_ERR, ("No response to PAP authenticate-requests\n"));
    u->us_clientstate = UPAPCS_BADAUTH;
    auth_withpeer_fail(u->us_unit, PPP_PAP);
    return;
  }

  upap_sauthreq(u);    /* Send Authenticate-Request and set upap timeout*/
}


/*
 * upap_reqtimeout - Give up waiting for the peer to send an auth-req.
 */
static void
upap_reqtimeout(void *arg)
{
  upap_state *u = (upap_state *) arg;

  if (u->us_serverstate != UPAPSS_LISTEN) {
    return; /* huh?? */
  }

  auth_peer_fail(u->us_unit, PPP_PAP);
  u->us_serverstate = UPAPSS_BADAUTH;
}


/*
 * upap_lowerup - The lower layer is up.
 *
 * Start authenticating if pending.
 */
static void
upap_lowerup(int unit)
{
  upap_state *u = &upap[unit];

  UPAPDEBUG(LOG_INFO, ("upap_lowerup: init %d clientstate s=%d\n", unit, u->us_clientstate));

  if (u->us_clientstate == UPAPCS_INITIAL) {
    u->us_clientstate = UPAPCS_CLOSED;
  } else if (u->us_clientstate == UPAPCS_PENDING) {
    upap_sauthreq(u);  /* send an auth-request */
    /* now client state is UPAPCS__AUTHREQ */
  }

  if (u->us_serverstate == UPAPSS_INITIAL) {
    u->us_serverstate = UPAPSS_CLOSED;
  } else if (u->us_serverstate == UPAPSS_PENDING) {
    u->us_serverstate = UPAPSS_LISTEN;
    if (u->us_reqtimeout > 0) {
      TIMEOUT(upap_reqtimeout, u, u->us_reqtimeout);
    }
  }
}


/*
 * upap_lowerdown - The lower layer is down.
 *
 * Cancel all timeouts.
 */
static void
upap_lowerdown(int unit)
{
  upap_state *u = &upap[unit];

  UPAPDEBUG(LOG_INFO, ("upap_lowerdown: %d s=%d\n", unit, u->us_clientstate));

  if (u->us_clientstate == UPAPCS_AUTHREQ) { /* Timeout pending? */
    UNTIMEOUT(upap_timeout, u);    /* Cancel timeout */
  }
  if (u->us_serverstate == UPAPSS_LISTEN && u->us_reqtimeout > 0) {
    UNTIMEOUT(upap_reqtimeout, u);
  }

  u->us_clientstate = UPAPCS_INITIAL;
  u->us_serverstate = UPAPSS_INITIAL;
}


/*
 * upap_protrej - Peer doesn't speak this protocol.
 *
 * This shouldn't happen.  In any case, pretend lower layer went down.
 */
static void
upap_protrej(int unit)
{
  upap_state *u = &upap[unit];

  if (u->us_clientstate == UPAPCS_AUTHREQ) {
    UPAPDEBUG(LOG_ERR, ("PAP authentication failed due to protocol-reject\n"));
    auth_withpeer_fail(unit, PPP_PAP);
  }
  if (u->us_serverstate == UPAPSS_LISTEN) {
    UPAPDEBUG(LOG_ERR, ("PAP authentication of peer failed (protocol-reject)\n"));
    auth_peer_fail(unit, PPP_PAP);
  }
  upap_lowerdown(unit);
}


/*
 * upap_input - Input UPAP packet.
 */
static void
upap_input(int unit, u_char *inpacket, int l)
{
  upap_state *u = &upap[unit];
  u_char *inp;
  u_char code, id;
  int len;

  /*
   * Parse header (code, id and length).
   * If packet too short, drop it.
   */
  inp = inpacket;
  if (l < (int)UPAP_HEADERLEN) {
    UPAPDEBUG(LOG_INFO, ("pap_input: rcvd short header.\n"));
    return;
  }
  GETCHAR(code, inp);
  GETCHAR(id, inp);
  GETSHORT(len, inp);
  if (len < (int)UPAP_HEADERLEN) {
    UPAPDEBUG(LOG_INFO, ("pap_input: rcvd illegal length.\n"));
    return;
  }
  if (len > l) {
    UPAPDEBUG(LOG_INFO, ("pap_input: rcvd short packet.\n"));
    return;
  }
  len -= UPAP_HEADERLEN;

  /*
   * Action depends on code.
   */
  switch (code) {
    case UPAP_AUTHREQ:
      upap_rauthreq(u, inp, id, len);
      break;

    case UPAP_AUTHACK:
      upap_rauthack(u, inp, id, len);
      break;

    case UPAP_AUTHNAK:
      upap_rauthnak(u, inp, id, len);
      break;

    default:        /* XXX Need code reject */
      UPAPDEBUG(LOG_INFO, ("pap_input: UNHANDLED default: code: %d, id: %d, len: %d.\n", code, id, len));
      break;
  }
}


/*
 * upap_rauth - Receive Authenticate.
 */
static void
upap_rauthreq(upap_state *u, u_char *inp, u_char id, int len)
{
  u_char ruserlen, rpasswdlen;
  char *ruser, *rpasswd;
  u_char retcode;
  char *msg;
  int msglen;

  UPAPDEBUG(LOG_INFO, ("pap_rauth: Rcvd id %d.\n", id));

  if (u->us_serverstate < UPAPSS_LISTEN) {
    return;
  }

  /*
   * If we receive a duplicate authenticate-request, we are
   * supposed to return the same status as for the first request.
   */
  if (u->us_serverstate == UPAPSS_OPEN) {
    upap_sresp(u, UPAP_AUTHACK, id, "", 0);  /* return auth-ack */
    return;
  }
  if (u->us_serverstate == UPAPSS_BADAUTH) {
    upap_sresp(u, UPAP_AUTHNAK, id, "", 0);  /* return auth-nak */
    return;
  }

  /*
   * Parse user/passwd.
   */
  if (len < (int)sizeof (u_char)) {
    UPAPDEBUG(LOG_INFO, ("pap_rauth: rcvd short packet.\n"));
    return;
  }
  GETCHAR(ruserlen, inp);
  len -= sizeof (u_char) + ruserlen + sizeof (u_char);
  if (len < 0) {
    UPAPDEBUG(LOG_INFO, ("pap_rauth: rcvd short packet.\n"));
    return;
  }
  ruser = (char *) inp;
  INCPTR(ruserlen, inp);
  GETCHAR(rpasswdlen, inp);
  if (len < rpasswdlen) {
    UPAPDEBUG(LOG_INFO, ("pap_rauth: rcvd short packet.\n"));
    return;
  }
  rpasswd = (char *) inp;

  /*
   * Check the username and password given.
   */
  retcode = check_passwd(u->us_unit, ruser, ruserlen, rpasswd, rpasswdlen, &msg, &msglen);
  /* lwip: currently retcode is always UPAP_AUTHACK */
  BZERO(rpasswd, rpasswdlen);

  upap_sresp(u, retcode, id, msg, msglen);

  if (retcode == UPAP_AUTHACK) {
    u->us_serverstate = UPAPSS_OPEN;
    auth_peer_success(u->us_unit, PPP_PAP, ruser, ruserlen);
  } else {
    u->us_serverstate = UPAPSS_BADAUTH;
    auth_peer_fail(u->us_unit, PPP_PAP);
  }

  if (u->us_reqtimeout > 0) {
    UNTIMEOUT(upap_reqtimeout, u);
  }
}


/*
 * upap_rauthack - Receive Authenticate-Ack.
 */
static void
upap_rauthack(upap_state *u, u_char *inp, int id, int len)
{
  u_char msglen;
  char *msg;

  LWIP_UNUSED_ARG(id);

  UPAPDEBUG(LOG_INFO, ("pap_rauthack: Rcvd id %d s=%d\n", id, u->us_clientstate));

  if (u->us_clientstate != UPAPCS_AUTHREQ) { /* XXX */
    UPAPDEBUG(LOG_INFO, ("pap_rauthack: us_clientstate != UPAPCS_AUTHREQ\n"));
    return;
  }

  /*
   * Parse message.
   */
  if (len < (int)sizeof (u_char)) {
    UPAPDEBUG(LOG_INFO, ("pap_rauthack: ignoring missing msg-length.\n"));
  } else {
    GETCHAR(msglen, inp);
    if (msglen > 0) {
      len -= sizeof (u_char);
      if (len < msglen) {
        UPAPDEBUG(LOG_INFO, ("pap_rauthack: rcvd short packet.\n"));
        return;
      }
      msg = (char *) inp;
      PRINTMSG(msg, msglen);
    }
  }
  UNTIMEOUT(upap_timeout, u);    /* Cancel timeout */
  u->us_clientstate = UPAPCS_OPEN;

  auth_withpeer_success(u->us_unit, PPP_PAP);
}


/*
 * upap_rauthnak - Receive Authenticate-Nak.
 */
static void
upap_rauthnak(upap_state *u, u_char *inp, int id, int len)
{
  u_char msglen;
  char *msg;

  LWIP_UNUSED_ARG(id);

  UPAPDEBUG(LOG_INFO, ("pap_rauthnak: Rcvd id %d s=%d\n", id, u->us_clientstate));

  if (u->us_clientstate != UPAPCS_AUTHREQ) { /* XXX */
    return;
  }

  /*
   * Parse message.
   */
  if (len < sizeof (u_char)) {
    UPAPDEBUG(LOG_INFO, ("pap_rauthnak: ignoring missing msg-length.\n"));
  } else {
    GETCHAR(msglen, inp);
    if(msglen > 0) {
      len -= sizeof (u_char);
      if (len < msglen) {
        UPAPDEBUG(LOG_INFO, ("pap_rauthnak: rcvd short packet.\n"));
        return;
      }
      msg = (char *) inp;
      PRINTMSG(msg, msglen);
    }
  }

  u->us_clientstate = UPAPCS_BADAUTH;

  UPAPDEBUG(LOG_ERR, ("PAP authentication failed\n"));
  auth_withpeer_fail(u->us_unit, PPP_PAP);
}


/*
 * upap_sauthreq - Send an Authenticate-Request.
 */
static void
upap_sauthreq(upap_state *u)
{
  u_char *outp;
  int outlen;

  outlen = UPAP_HEADERLEN + 2 * sizeof (u_char) 
         + u->us_userlen + u->us_passwdlen;
  outp = outpacket_buf[u->us_unit];

  MAKEHEADER(outp, PPP_PAP);

  PUTCHAR(UPAP_AUTHREQ, outp);
  PUTCHAR(++u->us_id, outp);
  PUTSHORT(outlen, outp);
  PUTCHAR(u->us_userlen, outp);
  BCOPY(u->us_user, outp, u->us_userlen);
  INCPTR(u->us_userlen, outp);
  PUTCHAR(u->us_passwdlen, outp);
  BCOPY(u->us_passwd, outp, u->us_passwdlen);

  pppWrite(u->us_unit, outpacket_buf[u->us_unit], outlen + PPP_HDRLEN);

  UPAPDEBUG(LOG_INFO, ("pap_sauth: Sent id %d\n", u->us_id));

  TIMEOUT(upap_timeout, u, u->us_timeouttime);
  ++u->us_transmits;
  u->us_clientstate = UPAPCS_AUTHREQ;
}


/*
 * upap_sresp - Send a response (ack or nak).
 */
static void
upap_sresp(upap_state *u, u_char code, u_char id, char *msg, int msglen)
{
  u_char *outp;
  int outlen;

  outlen = UPAP_HEADERLEN + sizeof (u_char) + msglen;
  outp = outpacket_buf[u->us_unit];
  MAKEHEADER(outp, PPP_PAP);

  PUTCHAR(code, outp);
  PUTCHAR(id, outp);
  PUTSHORT(outlen, outp);
  PUTCHAR(msglen, outp);
  BCOPY(msg, outp, msglen);
  pppWrite(u->us_unit, outpacket_buf[u->us_unit], outlen + PPP_HDRLEN);

  UPAPDEBUG(LOG_INFO, ("pap_sresp: Sent code %d, id %d s=%d\n", code, id, u->us_clientstate));
}

#if PPP_ADDITIONAL_CALLBACKS
static char *upap_codenames[] = {
    "AuthReq", "AuthAck", "AuthNak"
};

/*
 * upap_printpkt - print the contents of a PAP packet.
 */
static int upap_printpkt(
  u_char *p,
  int plen,
  void (*printer) (void *, char *, ...),
  void *arg
)
{
  LWIP_UNUSED_ARG(p);
  LWIP_UNUSED_ARG(plen);
  LWIP_UNUSED_ARG(printer);
  LWIP_UNUSED_ARG(arg);
  return 0;
}
#endif /* PPP_ADDITIONAL_CALLBACKS */

#endif /* PAP_SUPPORT */

#endif /* PPP_SUPPORT */
