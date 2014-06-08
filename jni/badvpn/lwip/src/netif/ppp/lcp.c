/*****************************************************************************
* lcp.c - Network Link Control Protocol program file.
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
* 97-12-01 Guy Lancaster <lancasterg@acm.org>, Global Election Systems Inc.
*   Original.
*****************************************************************************/

/*
 * lcp.c - PPP Link Control Protocol.
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
#include "chap.h"
#include "magic.h"
#include "auth.h"
#include "lcp.h"

#include <string.h>

#if PPPOE_SUPPORT
#include "netif/ppp_oe.h"
#else
#define PPPOE_MAXMTU PPP_MAXMRU
#endif

#if 0 /* UNUSED */
/*
 * LCP-related command-line options.
 */
int lcp_echo_interval = 0;  /* Interval between LCP echo-requests */
int lcp_echo_fails = 0;     /* Tolerance to unanswered echo-requests */
bool  lax_recv = 0;         /* accept control chars in asyncmap */

static int setescape (char **);

static option_t lcp_option_list[] = {
    /* LCP options */
    /* list stripped for simplicity */
    {NULL}
};
#endif /* UNUSED */

/* options */
LinkPhase lcp_phase[NUM_PPP];          /* Phase of link session (RFC 1661) */
static u_int lcp_echo_interval      = LCP_ECHOINTERVAL; /* Interval between LCP echo-requests */
static u_int lcp_echo_fails         = LCP_MAXECHOFAILS; /* Tolerance to unanswered echo-requests */

/* global vars */
static fsm lcp_fsm[NUM_PPP];                            /* LCP fsm structure (global)*/
lcp_options lcp_wantoptions[NUM_PPP];  /* Options that we want to request */
lcp_options lcp_gotoptions[NUM_PPP];   /* Options that peer ack'd */
lcp_options lcp_allowoptions[NUM_PPP]; /* Options we allow peer to request */
lcp_options lcp_hisoptions[NUM_PPP];   /* Options that we ack'd */
ext_accm xmit_accm[NUM_PPP];           /* extended transmit ACCM */

static u32_t lcp_echos_pending      = 0;                /* Number of outstanding echo msgs */
static u32_t lcp_echo_number        = 0;                /* ID number of next echo frame */
static u32_t lcp_echo_timer_running = 0;                /* TRUE if a timer is running */

/* @todo: do we really need such a large buffer? The typical 1500 bytes seem too much. */
static u_char nak_buffer[PPP_MRU]; /* where we construct a nak packet */ 

/*
 * Callbacks for fsm code.  (CI = Configuration Information)
 */
static void lcp_resetci (fsm*);                   /* Reset our CI */
static int  lcp_cilen (fsm*);                     /* Return length of our CI */
static void lcp_addci (fsm*, u_char*, int*);      /* Add our CI to pkt */
static int  lcp_ackci (fsm*, u_char*, int);       /* Peer ack'd our CI */
static int  lcp_nakci (fsm*, u_char*, int);       /* Peer nak'd our CI */
static int  lcp_rejci (fsm*, u_char*, int);       /* Peer rej'd our CI */
static int  lcp_reqci (fsm*, u_char*, int*, int); /* Rcv peer CI */
static void lcp_up (fsm*);                        /* We're UP */
static void lcp_down (fsm*);                      /* We're DOWN */
static void lcp_starting (fsm*);                  /* We need lower layer up */
static void lcp_finished (fsm*);                  /* We need lower layer down */
static int  lcp_extcode (fsm*, int, u_char, u_char*, int);
static void lcp_rprotrej (fsm*, u_char*, int);

/*
 * routines to send LCP echos to peer
 */

static void lcp_echo_lowerup (int);
static void lcp_echo_lowerdown (int);
static void LcpEchoTimeout (void*);
static void lcp_received_echo_reply (fsm*, int, u_char*, int);
static void LcpSendEchoRequest (fsm*);
static void LcpLinkFailure (fsm*);
static void LcpEchoCheck (fsm*);

static fsm_callbacks lcp_callbacks = { /* LCP callback routines */
  lcp_resetci,  /* Reset our Configuration Information */
  lcp_cilen,    /* Length of our Configuration Information */
  lcp_addci,    /* Add our Configuration Information */
  lcp_ackci,    /* ACK our Configuration Information */
  lcp_nakci,    /* NAK our Configuration Information */
  lcp_rejci,    /* Reject our Configuration Information */
  lcp_reqci,    /* Request peer's Configuration Information */
  lcp_up,       /* Called when fsm reaches LS_OPENED state */
  lcp_down,     /* Called when fsm leaves LS_OPENED state */
  lcp_starting, /* Called when we want the lower layer up */
  lcp_finished, /* Called when we want the lower layer down */
  NULL,         /* Called when Protocol-Reject received */
  NULL,         /* Retransmission is necessary */
  lcp_extcode,  /* Called to handle LCP-specific codes */
  "LCP"         /* String name of protocol */
};

/*
 * Protocol entry points.
 * Some of these are called directly.
 */

static void lcp_input (int, u_char *, int);
static void lcp_protrej (int);

struct protent lcp_protent = {
    PPP_LCP,
    lcp_init,
    lcp_input,
    lcp_protrej,
    lcp_lowerup,
    lcp_lowerdown,
    lcp_open,
    lcp_close,
#if PPP_ADDITIONAL_CALLBACKS
    lcp_printpkt,
    NULL,
#endif /* PPP_ADDITIONAL_CALLBACKS */
    1,
    "LCP",
#if PPP_ADDITIONAL_CALLBACKS
    NULL,
    NULL,
    NULL
#endif /* PPP_ADDITIONAL_CALLBACKS */
};

int lcp_loopbackfail = DEFLOOPBACKFAIL;

/*
 * Length of each type of configuration option (in octets)
 */
#define CILEN_VOID  2
#define CILEN_CHAR  3
#define CILEN_SHORT 4 /* CILEN_VOID + sizeof(short) */
#define CILEN_CHAP  5 /* CILEN_VOID + sizeof(short) + 1 */
#define CILEN_LONG  6 /* CILEN_VOID + sizeof(long) */
#define CILEN_LQR   8 /* CILEN_VOID + sizeof(short) + sizeof(long) */
#define CILEN_CBCP  3

#define CODENAME(x)  ((x) == CONFACK ? "ACK" : (x) == CONFNAK ? "NAK" : "REJ")

#if 0 /* UNUSED */
/*
 * setescape - add chars to the set we escape on transmission.
 */
static int
setescape(argv)
    char **argv;
{
    int n, ret;
    char *p, *endp;

    p = *argv;
    ret = 1;
    while (*p) {
      n = strtol(p, &endp, 16);
      if (p == endp) {
        option_error("escape parameter contains invalid hex number '%s'", p);
        return 0;
      }
      p = endp;
      if (n < 0 || n == 0x5E || n > 0xFF) {
        option_error("can't escape character 0x%x", n);
        ret = 0;
      } else
        xmit_accm[0][n >> 5] |= 1 << (n & 0x1F);
      while (*p == ',' || *p == ' ')
        ++p;
    }
    return ret;
}
#endif /* UNUSED */

/*
 * lcp_init - Initialize LCP.
 */
void
lcp_init(int unit)
{
  fsm         *f  = &lcp_fsm[unit];
  lcp_options *wo = &lcp_wantoptions[unit];
  lcp_options *ao = &lcp_allowoptions[unit];

  f->unit      = unit;
  f->protocol  = PPP_LCP;
  f->callbacks = &lcp_callbacks;

  fsm_init(f);

  wo->passive           = 0;
  wo->silent            = 0;
  wo->restart           = 0;               /* Set to 1 in kernels or multi-line implementations */
  wo->neg_mru           = 1;
  wo->mru               = PPP_DEFMRU;
  wo->neg_asyncmap      = 1;
  wo->asyncmap          = 0x00000000l;     /* Assume don't need to escape any ctl chars. */
  wo->neg_chap          = 0;               /* Set to 1 on server */
  wo->neg_upap          = 0;               /* Set to 1 on server */
  wo->chap_mdtype       = CHAP_DIGEST_MD5;
  wo->neg_magicnumber   = 1;
  wo->neg_pcompression  = 1;
  wo->neg_accompression = 1;
  wo->neg_lqr           = 0;               /* no LQR implementation yet */
  wo->neg_cbcp          = 0;

  ao->neg_mru           = 1;
  ao->mru               = PPP_MAXMRU;
  ao->neg_asyncmap      = 1;
  ao->asyncmap          = 0x00000000l;     /* Assume don't need to escape any ctl chars. */
  ao->neg_chap          = (CHAP_SUPPORT != 0);
  ao->chap_mdtype       = CHAP_DIGEST_MD5;
  ao->neg_upap          = (PAP_SUPPORT != 0);
  ao->neg_magicnumber   = 1;
  ao->neg_pcompression  = 1;
  ao->neg_accompression = 1;
  ao->neg_lqr           = 0;               /* no LQR implementation yet */
  ao->neg_cbcp          = (CBCP_SUPPORT != 0);

  /* 
   * Set transmit escape for the flag and escape characters plus anything
   * set for the allowable options.
   */
  memset(xmit_accm[unit], 0, sizeof(xmit_accm[0]));
  xmit_accm[unit][15] = 0x60;
  xmit_accm[unit][0]  = (u_char)((ao->asyncmap        & 0xFF));
  xmit_accm[unit][1]  = (u_char)((ao->asyncmap >> 8)  & 0xFF);
  xmit_accm[unit][2]  = (u_char)((ao->asyncmap >> 16) & 0xFF);
  xmit_accm[unit][3]  = (u_char)((ao->asyncmap >> 24) & 0xFF);
  LCPDEBUG(LOG_INFO, ("lcp_init: xmit_accm=%X %X %X %X\n",
        xmit_accm[unit][0],
        xmit_accm[unit][1],
        xmit_accm[unit][2],
        xmit_accm[unit][3]));
  
  lcp_phase[unit] = PHASE_INITIALIZE;
}


/*
 * lcp_open - LCP is allowed to come up.
 */
void
lcp_open(int unit)
{
  fsm         *f  = &lcp_fsm[unit];
  lcp_options *wo = &lcp_wantoptions[unit];

  f->flags = 0;
  if (wo->passive) {
    f->flags |= OPT_PASSIVE;
  }
  if (wo->silent) {
    f->flags |= OPT_SILENT;
  }
  fsm_open(f);

  lcp_phase[unit] = PHASE_ESTABLISH;
}


/*
 * lcp_close - Take LCP down.
 */
void
lcp_close(int unit, char *reason)
{
  fsm *f = &lcp_fsm[unit];

  if (lcp_phase[unit] != PHASE_DEAD) {
    lcp_phase[unit] = PHASE_TERMINATE;
  }
  if (f->state == LS_STOPPED && f->flags & (OPT_PASSIVE|OPT_SILENT)) {
    /*
     * This action is not strictly according to the FSM in RFC1548,
     * but it does mean that the program terminates if you do an
     * lcp_close() in passive/silent mode when a connection hasn't
     * been established.
     */
    f->state = LS_CLOSED;
    lcp_finished(f);
  } else {
    fsm_close(f, reason);
  }
}


/*
 * lcp_lowerup - The lower layer is up.
 */
void
lcp_lowerup(int unit)
{
  lcp_options *wo = &lcp_wantoptions[unit];

  /*
   * Don't use A/C or protocol compression on transmission,
   * but accept A/C and protocol compressed packets
   * if we are going to ask for A/C and protocol compression.
   */
  ppp_set_xaccm(unit, &xmit_accm[unit]);
  ppp_send_config(unit, PPP_MRU, 0xffffffffl, 0, 0);
  ppp_recv_config(unit, PPP_MRU, 0x00000000l,
  wo->neg_pcompression, wo->neg_accompression);
  peer_mru[unit] = PPP_MRU;
  lcp_allowoptions[unit].asyncmap = (u_long)xmit_accm[unit][0]
                                 | ((u_long)xmit_accm[unit][1] << 8)
                                 | ((u_long)xmit_accm[unit][2] << 16)
                                 | ((u_long)xmit_accm[unit][3] << 24);
  LCPDEBUG(LOG_INFO, ("lcp_lowerup: asyncmap=%X %X %X %X\n",
            xmit_accm[unit][3],
            xmit_accm[unit][2],
            xmit_accm[unit][1],
            xmit_accm[unit][0]));

  fsm_lowerup(&lcp_fsm[unit]);
}


/*
 * lcp_lowerdown - The lower layer is down.
 */
void
lcp_lowerdown(int unit)
{
  fsm_lowerdown(&lcp_fsm[unit]);
}


/*
 * lcp_input - Input LCP packet.
 */
static void
lcp_input(int unit, u_char *p, int len)
{
  fsm *f = &lcp_fsm[unit];

  fsm_input(f, p, len);
}


/*
 * lcp_extcode - Handle a LCP-specific code.
 */
static int
lcp_extcode(fsm *f, int code, u_char id, u_char *inp, int len)
{
  u_char *magp;

  switch( code ){
    case PROTREJ:
      lcp_rprotrej(f, inp, len);
      break;
  
    case ECHOREQ:
      if (f->state != LS_OPENED) {
        break;
      }
      LCPDEBUG(LOG_INFO, ("lcp: Echo-Request, Rcvd id %d\n", id));
      magp = inp;
      PUTLONG(lcp_gotoptions[f->unit].magicnumber, magp);
      fsm_sdata(f, ECHOREP, id, inp, len);
      break;

    case ECHOREP:
      lcp_received_echo_reply(f, id, inp, len);
      break;

    case DISCREQ:
      break;

    default:
      return 0;
  }
  return 1;
}


/*
 * lcp_rprotrej - Receive an Protocol-Reject.
 *
 * Figure out which protocol is rejected and inform it.
 */
static void
lcp_rprotrej(fsm *f, u_char *inp, int len)
{
  int i;
  struct protent *protp;
  u_short prot;

  if (len < (int)sizeof (u_short)) {
    LCPDEBUG(LOG_INFO, ("lcp_rprotrej: Rcvd short Protocol-Reject packet!\n"));
    return;
  }

  GETSHORT(prot, inp);

  LCPDEBUG(LOG_INFO, ("lcp_rprotrej: Rcvd Protocol-Reject packet for %x!\n", prot));

  /*
   * Protocol-Reject packets received in any state other than the LCP
   * LS_OPENED state SHOULD be silently discarded.
   */
  if( f->state != LS_OPENED ) {
    LCPDEBUG(LOG_INFO, ("Protocol-Reject discarded: LCP in state %d\n", f->state));
    return;
  }

  /*
   * Upcall the proper Protocol-Reject routine.
   */
  for (i = 0; (protp = ppp_protocols[i]) != NULL; ++i) {
    if (protp->protocol == prot && protp->enabled_flag) {
      (*protp->protrej)(f->unit);
      return;
    }
  }

  LCPDEBUG(LOG_WARNING, ("Protocol-Reject for unsupported protocol 0x%x\n", prot));
}


/*
 * lcp_protrej - A Protocol-Reject was received.
 */
static void
lcp_protrej(int unit)
{
  LWIP_UNUSED_ARG(unit);
  /*
   * Can't reject LCP!
   */
  LCPDEBUG(LOG_WARNING, ("lcp_protrej: Received Protocol-Reject for LCP!\n"));
  fsm_protreject(&lcp_fsm[unit]);
}


/*
 * lcp_sprotrej - Send a Protocol-Reject for some protocol.
 */
void
lcp_sprotrej(int unit, u_char *p, int len)
{
  /*
   * Send back the protocol and the information field of the
   * rejected packet.  We only get here if LCP is in the LS_OPENED state.
   */

  fsm_sdata(&lcp_fsm[unit], PROTREJ, ++lcp_fsm[unit].id, p, len);
}


/*
 * lcp_resetci - Reset our CI.
 */
static void
lcp_resetci(fsm *f)
{
  lcp_wantoptions[f->unit].magicnumber = magic();
  lcp_wantoptions[f->unit].numloops = 0;
  lcp_gotoptions[f->unit] = lcp_wantoptions[f->unit];
  peer_mru[f->unit] = PPP_MRU;
  auth_reset(f->unit);
}


/*
 * lcp_cilen - Return length of our CI.
 */
static int
lcp_cilen(fsm *f)
{
  lcp_options *go = &lcp_gotoptions[f->unit];

#define LENCIVOID(neg)  ((neg) ? CILEN_VOID : 0)
#define LENCICHAP(neg)  ((neg) ? CILEN_CHAP : 0)
#define LENCISHORT(neg) ((neg) ? CILEN_SHORT : 0)
#define LENCILONG(neg)  ((neg) ? CILEN_LONG : 0)
#define LENCILQR(neg)   ((neg) ? CILEN_LQR: 0)
#define LENCICBCP(neg)  ((neg) ? CILEN_CBCP: 0)
  /*
   * NB: we only ask for one of CHAP and UPAP, even if we will
   * accept either.
   */
  return (LENCISHORT(go->neg_mru && go->mru != PPP_DEFMRU) +
          LENCILONG(go->neg_asyncmap && go->asyncmap != 0xFFFFFFFFl) +
          LENCICHAP(go->neg_chap) +
          LENCISHORT(!go->neg_chap && go->neg_upap) +
          LENCILQR(go->neg_lqr) +
          LENCICBCP(go->neg_cbcp) +
          LENCILONG(go->neg_magicnumber) +
          LENCIVOID(go->neg_pcompression) +
          LENCIVOID(go->neg_accompression));
}


/*
 * lcp_addci - Add our desired CIs to a packet.
 */
static void
lcp_addci(fsm *f, u_char *ucp, int *lenp)
{
  lcp_options *go = &lcp_gotoptions[f->unit];
  u_char *start_ucp = ucp;

#define ADDCIVOID(opt, neg) \
  if (neg) { \
    LCPDEBUG(LOG_INFO, ("lcp_addci: opt=%d\n", opt)); \
    PUTCHAR(opt, ucp); \
    PUTCHAR(CILEN_VOID, ucp); \
  }
#define ADDCISHORT(opt, neg, val) \
  if (neg) { \
    LCPDEBUG(LOG_INFO, ("lcp_addci: INT opt=%d %X\n", opt, val)); \
    PUTCHAR(opt, ucp); \
    PUTCHAR(CILEN_SHORT, ucp); \
    PUTSHORT(val, ucp); \
  }
#define ADDCICHAP(opt, neg, val, digest) \
  if (neg) { \
    LCPDEBUG(LOG_INFO, ("lcp_addci: CHAP opt=%d %X\n", opt, val)); \
    PUTCHAR(opt, ucp); \
    PUTCHAR(CILEN_CHAP, ucp); \
    PUTSHORT(val, ucp); \
    PUTCHAR(digest, ucp); \
  }
#define ADDCILONG(opt, neg, val) \
  if (neg) { \
    LCPDEBUG(LOG_INFO, ("lcp_addci: L opt=%d %lX\n", opt, val)); \
    PUTCHAR(opt, ucp); \
    PUTCHAR(CILEN_LONG, ucp); \
    PUTLONG(val, ucp); \
  }
#define ADDCILQR(opt, neg, val) \
  if (neg) { \
    LCPDEBUG(LOG_INFO, ("lcp_addci: LQR opt=%d %lX\n", opt, val)); \
    PUTCHAR(opt, ucp); \
    PUTCHAR(CILEN_LQR, ucp); \
    PUTSHORT(PPP_LQR, ucp); \
    PUTLONG(val, ucp); \
  }
#define ADDCICHAR(opt, neg, val) \
  if (neg) { \
    LCPDEBUG(LOG_INFO, ("lcp_addci: CHAR opt=%d %X '%z'\n", opt, val, val)); \
    PUTCHAR(opt, ucp); \
    PUTCHAR(CILEN_CHAR, ucp); \
    PUTCHAR(val, ucp); \
  }

  ADDCISHORT(CI_MRU, go->neg_mru && go->mru != PPP_DEFMRU, go->mru);
  ADDCILONG(CI_ASYNCMAP, go->neg_asyncmap && go->asyncmap != 0xFFFFFFFFl, go->asyncmap);
  ADDCICHAP(CI_AUTHTYPE, go->neg_chap, PPP_CHAP, go->chap_mdtype);
  ADDCISHORT(CI_AUTHTYPE, !go->neg_chap && go->neg_upap, PPP_PAP);
  ADDCILQR(CI_QUALITY, go->neg_lqr, go->lqr_period);
  ADDCICHAR(CI_CALLBACK, go->neg_cbcp, CBCP_OPT);
  ADDCILONG(CI_MAGICNUMBER, go->neg_magicnumber, go->magicnumber);
  ADDCIVOID(CI_PCOMPRESSION, go->neg_pcompression);
  ADDCIVOID(CI_ACCOMPRESSION, go->neg_accompression);

  if (ucp - start_ucp != *lenp) {
    /* this should never happen, because peer_mtu should be 1500 */
    LCPDEBUG(LOG_ERR, ("Bug in lcp_addci: wrong length\n"));
  }
}


/*
 * lcp_ackci - Ack our CIs.
 * This should not modify any state if the Ack is bad.
 *
 * Returns:
 *  0 - Ack was bad.
 *  1 - Ack was good.
 */
static int
lcp_ackci(fsm *f, u_char *p, int len)
{
  lcp_options *go = &lcp_gotoptions[f->unit];
  u_char cilen, citype, cichar;
  u_short cishort;
  u32_t cilong;

  /*
   * CIs must be in exactly the same order that we sent.
   * Check packet length and CI length at each step.
   * If we find any deviations, then this packet is bad.
   */
#define ACKCIVOID(opt, neg) \
  if (neg) { \
    if ((len -= CILEN_VOID) < 0) \
      goto bad; \
    GETCHAR(citype, p); \
    GETCHAR(cilen, p); \
    if (cilen != CILEN_VOID || citype != opt) \
      goto bad; \
  }
#define ACKCISHORT(opt, neg, val) \
  if (neg) { \
    if ((len -= CILEN_SHORT) < 0) \
      goto bad; \
    GETCHAR(citype, p); \
    GETCHAR(cilen, p); \
    if (cilen != CILEN_SHORT || citype != opt) \
      goto bad; \
    GETSHORT(cishort, p); \
    if (cishort != val) \
      goto bad; \
  }
#define ACKCICHAR(opt, neg, val) \
  if (neg) { \
    if ((len -= CILEN_CHAR) < 0) \
      goto bad; \
    GETCHAR(citype, p); \
    GETCHAR(cilen, p); \
    if (cilen != CILEN_CHAR || citype != opt) \
      goto bad; \
    GETCHAR(cichar, p); \
    if (cichar != val) \
      goto bad; \
  }
#define ACKCICHAP(opt, neg, val, digest) \
  if (neg) { \
    if ((len -= CILEN_CHAP) < 0) \
      goto bad; \
    GETCHAR(citype, p); \
    GETCHAR(cilen, p); \
    if (cilen != CILEN_CHAP || citype != opt) \
      goto bad; \
    GETSHORT(cishort, p); \
    if (cishort != val) \
      goto bad; \
    GETCHAR(cichar, p); \
    if (cichar != digest) \
      goto bad; \
  }
#define ACKCILONG(opt, neg, val) \
  if (neg) { \
    if ((len -= CILEN_LONG) < 0) \
      goto bad; \
    GETCHAR(citype, p); \
    GETCHAR(cilen, p); \
    if (cilen != CILEN_LONG ||  citype != opt) \
      goto bad; \
    GETLONG(cilong, p); \
    if (cilong != val) \
      goto bad; \
  }
#define ACKCILQR(opt, neg, val) \
  if (neg) { \
    if ((len -= CILEN_LQR) < 0) \
      goto bad; \
    GETCHAR(citype, p); \
    GETCHAR(cilen, p); \
    if (cilen != CILEN_LQR || citype != opt) \
      goto bad; \
    GETSHORT(cishort, p); \
    if (cishort != PPP_LQR) \
      goto bad; \
    GETLONG(cilong, p); \
    if (cilong != val) \
      goto bad; \
  }

  ACKCISHORT(CI_MRU, go->neg_mru && go->mru != PPP_DEFMRU, go->mru);
  ACKCILONG(CI_ASYNCMAP, go->neg_asyncmap && go->asyncmap != 0xFFFFFFFFl, go->asyncmap);
  ACKCICHAP(CI_AUTHTYPE, go->neg_chap, PPP_CHAP, go->chap_mdtype);
  ACKCISHORT(CI_AUTHTYPE, !go->neg_chap && go->neg_upap, PPP_PAP);
  ACKCILQR(CI_QUALITY, go->neg_lqr, go->lqr_period);
  ACKCICHAR(CI_CALLBACK, go->neg_cbcp, CBCP_OPT);
  ACKCILONG(CI_MAGICNUMBER, go->neg_magicnumber, go->magicnumber);
  ACKCIVOID(CI_PCOMPRESSION, go->neg_pcompression);
  ACKCIVOID(CI_ACCOMPRESSION, go->neg_accompression);

  /*
   * If there are any remaining CIs, then this packet is bad.
   */
  if (len != 0) {
    goto bad;
  }
  LCPDEBUG(LOG_INFO, ("lcp_acki: Ack\n"));
  return (1);
bad:
  LCPDEBUG(LOG_WARNING, ("lcp_acki: received bad Ack!\n"));
  return (0);
}


/*
 * lcp_nakci - Peer has sent a NAK for some of our CIs.
 * This should not modify any state if the Nak is bad
 * or if LCP is in the LS_OPENED state.
 *
 * Returns:
 *  0 - Nak was bad.
 *  1 - Nak was good.
 */
static int
lcp_nakci(fsm *f, u_char *p, int len)
{
  lcp_options *go = &lcp_gotoptions[f->unit];
  lcp_options *wo = &lcp_wantoptions[f->unit];
  u_char citype, cichar, *next;
  u_short cishort;
  u32_t cilong;
  lcp_options no;     /* options we've seen Naks for */
  lcp_options try;    /* options to request next time */
  int looped_back = 0;
  int cilen;

  BZERO(&no, sizeof(no));
  try = *go;

  /*
   * Any Nak'd CIs must be in exactly the same order that we sent.
   * Check packet length and CI length at each step.
   * If we find any deviations, then this packet is bad.
   */
#define NAKCIVOID(opt, neg, code) \
  if (go->neg && \
      len >= CILEN_VOID && \
      p[1] == CILEN_VOID && \
      p[0] == opt) { \
    len -= CILEN_VOID; \
    INCPTR(CILEN_VOID, p); \
    no.neg = 1; \
    code \
  }
#define NAKCICHAP(opt, neg, code) \
  if (go->neg && \
      len >= CILEN_CHAP && \
      p[1] == CILEN_CHAP && \
      p[0] == opt) { \
    len -= CILEN_CHAP; \
    INCPTR(2, p); \
    GETSHORT(cishort, p); \
    GETCHAR(cichar, p); \
    no.neg = 1; \
    code \
  }
#define NAKCICHAR(opt, neg, code) \
  if (go->neg && \
      len >= CILEN_CHAR && \
      p[1] == CILEN_CHAR && \
      p[0] == opt) { \
    len -= CILEN_CHAR; \
    INCPTR(2, p); \
    GETCHAR(cichar, p); \
    no.neg = 1; \
    code \
  }
#define NAKCISHORT(opt, neg, code) \
  if (go->neg && \
      len >= CILEN_SHORT && \
      p[1] == CILEN_SHORT && \
      p[0] == opt) { \
    len -= CILEN_SHORT; \
    INCPTR(2, p); \
    GETSHORT(cishort, p); \
    no.neg = 1; \
    code \
  }
#define NAKCILONG(opt, neg, code) \
  if (go->neg && \
      len >= CILEN_LONG && \
      p[1] == CILEN_LONG && \
      p[0] == opt) { \
    len -= CILEN_LONG; \
    INCPTR(2, p); \
    GETLONG(cilong, p); \
    no.neg = 1; \
    code \
  }
#define NAKCILQR(opt, neg, code) \
  if (go->neg && \
      len >= CILEN_LQR && \
      p[1] == CILEN_LQR && \
      p[0] == opt) { \
    len -= CILEN_LQR; \
    INCPTR(2, p); \
    GETSHORT(cishort, p); \
    GETLONG(cilong, p); \
    no.neg = 1; \
    code \
  }

  /*
   * We don't care if they want to send us smaller packets than
   * we want.  Therefore, accept any MRU less than what we asked for,
   * but then ignore the new value when setting the MRU in the kernel.
   * If they send us a bigger MRU than what we asked, accept it, up to
   * the limit of the default MRU we'd get if we didn't negotiate.
   */
  if (go->neg_mru && go->mru != PPP_DEFMRU) {
    NAKCISHORT(CI_MRU, neg_mru,
      if (cishort <= wo->mru || cishort < PPP_DEFMRU) {
        try.mru = cishort;
      }
    );
  }

  /*
   * Add any characters they want to our (receive-side) asyncmap.
   */
  if (go->neg_asyncmap && go->asyncmap != 0xFFFFFFFFl) {
    NAKCILONG(CI_ASYNCMAP, neg_asyncmap,
      try.asyncmap = go->asyncmap | cilong;
    );
  }

  /*
   * If they've nak'd our authentication-protocol, check whether
   * they are proposing a different protocol, or a different
   * hash algorithm for CHAP.
   */
  if ((go->neg_chap || go->neg_upap)
      && len >= CILEN_SHORT
      && p[0] == CI_AUTHTYPE && p[1] >= CILEN_SHORT && p[1] <= len) {
    cilen = p[1];
    len -= cilen;
    no.neg_chap = go->neg_chap;
    no.neg_upap = go->neg_upap;
    INCPTR(2, p);
    GETSHORT(cishort, p);
    if (cishort == PPP_PAP && cilen == CILEN_SHORT) {
      /*
       * If we were asking for CHAP, they obviously don't want to do it.
       * If we weren't asking for CHAP, then we were asking for PAP,
       * in which case this Nak is bad.
       */
      if (!go->neg_chap) {
        goto bad;
      }
      try.neg_chap = 0;
    
    } else if (cishort == PPP_CHAP && cilen == CILEN_CHAP) {
      GETCHAR(cichar, p);
      if (go->neg_chap) {
        /*
         * We were asking for CHAP/MD5; they must want a different
         * algorithm.  If they can't do MD5, we'll have to stop
         * asking for CHAP.
         */
        if (cichar != go->chap_mdtype) {
          try.neg_chap = 0;
        }
      } else {
        /*
         * Stop asking for PAP if we were asking for it.
         */
        try.neg_upap = 0;
      }
    
    } else {
      /*
       * We don't recognize what they're suggesting.
       * Stop asking for what we were asking for.
       */
      if (go->neg_chap) {
        try.neg_chap = 0;
      } else {
        try.neg_upap = 0;
      }
      p += cilen - CILEN_SHORT;
    }
  }

  /*
   * If they can't cope with our link quality protocol, we'll have
   * to stop asking for LQR.  We haven't got any other protocol.
   * If they Nak the reporting period, take their value XXX ?
   */
  NAKCILQR(CI_QUALITY, neg_lqr,
    if (cishort != PPP_LQR) {
      try.neg_lqr = 0;
    } else {
      try.lqr_period = cilong;
    }
  );

  /*
   * Only implementing CBCP...not the rest of the callback options
   */
  NAKCICHAR(CI_CALLBACK, neg_cbcp,
    try.neg_cbcp = 0;
  );

  /*
   * Check for a looped-back line.
   */
  NAKCILONG(CI_MAGICNUMBER, neg_magicnumber,
    try.magicnumber = magic();
    looped_back = 1;
  );

  /*
   * Peer shouldn't send Nak for protocol compression or
   * address/control compression requests; they should send
   * a Reject instead.  If they send a Nak, treat it as a Reject.
   */
  NAKCIVOID(CI_PCOMPRESSION, neg_pcompression,
    try.neg_pcompression = 0;
  );
  NAKCIVOID(CI_ACCOMPRESSION, neg_accompression,
    try.neg_accompression = 0;
  );

  /*
   * There may be remaining CIs, if the peer is requesting negotiation
   * on an option that we didn't include in our request packet.
   * If we see an option that we requested, or one we've already seen
   * in this packet, then this packet is bad.
   * If we wanted to respond by starting to negotiate on the requested
   * option(s), we could, but we don't, because except for the
   * authentication type and quality protocol, if we are not negotiating
   * an option, it is because we were told not to.
   * For the authentication type, the Nak from the peer means
   * `let me authenticate myself with you' which is a bit pointless.
   * For the quality protocol, the Nak means `ask me to send you quality
   * reports', but if we didn't ask for them, we don't want them.
   * An option we don't recognize represents the peer asking to
   * negotiate some option we don't support, so ignore it.
   */
  while (len > CILEN_VOID) {
    GETCHAR(citype, p);
    GETCHAR(cilen, p);
    if (cilen < CILEN_VOID || (len -= cilen) < 0) {
      goto bad;
    }
    next = p + cilen - 2;

    switch (citype) {
      case CI_MRU:
        if ((go->neg_mru && go->mru != PPP_DEFMRU)
            || no.neg_mru || cilen != CILEN_SHORT) {
          goto bad;
        }
        GETSHORT(cishort, p);
        if (cishort < PPP_DEFMRU) {
          try.mru = cishort;
        }
        break;
      case CI_ASYNCMAP:
        if ((go->neg_asyncmap && go->asyncmap != 0xFFFFFFFFl)
            || no.neg_asyncmap || cilen != CILEN_LONG) {
          goto bad;
        }
        break;
      case CI_AUTHTYPE:
        if (go->neg_chap || no.neg_chap || go->neg_upap || no.neg_upap) {
          goto bad;
        }
        break;
      case CI_MAGICNUMBER:
        if (go->neg_magicnumber || no.neg_magicnumber ||
            cilen != CILEN_LONG) {
          goto bad;
        }
        break;
      case CI_PCOMPRESSION:
        if (go->neg_pcompression || no.neg_pcompression
            || cilen != CILEN_VOID) {
          goto bad;
        }
        break;
      case CI_ACCOMPRESSION:
        if (go->neg_accompression || no.neg_accompression
            || cilen != CILEN_VOID) {
          goto bad;
        }
        break;
      case CI_QUALITY:
        if (go->neg_lqr || no.neg_lqr || cilen != CILEN_LQR) {
          goto bad;
        }
        break;
    }
    p = next;
  }

  /* If there is still anything left, this packet is bad. */
  if (len != 0) {
    goto bad;
  }

  /*
  * OK, the Nak is good.  Now we can update state.
  */
  if (f->state != LS_OPENED) {
    if (looped_back) {
      if (++try.numloops >= lcp_loopbackfail) {
        LCPDEBUG(LOG_NOTICE, ("Serial line is looped back.\n"));
        lcp_close(f->unit, "Loopback detected");
      }
    } else {
      try.numloops = 0;
    }
    *go = try;
  }

  return 1;

bad:
  LCPDEBUG(LOG_WARNING, ("lcp_nakci: received bad Nak!\n"));
  return 0;
}


/*
 * lcp_rejci - Peer has Rejected some of our CIs.
 * This should not modify any state if the Reject is bad
 * or if LCP is in the LS_OPENED state.
 *
 * Returns:
 *  0 - Reject was bad.
 *  1 - Reject was good.
 */
static int
lcp_rejci(fsm *f, u_char *p, int len)
{
  lcp_options *go = &lcp_gotoptions[f->unit];
  u_char cichar;
  u_short cishort;
  u32_t cilong;
  lcp_options try; /* options to request next time */

  try = *go;

  /*
   * Any Rejected CIs must be in exactly the same order that we sent.
   * Check packet length and CI length at each step.
   * If we find any deviations, then this packet is bad.
   */
#define REJCIVOID(opt, neg) \
  if (go->neg && \
      len >= CILEN_VOID && \
      p[1] == CILEN_VOID && \
      p[0] == opt) { \
    len -= CILEN_VOID; \
    INCPTR(CILEN_VOID, p); \
    try.neg = 0; \
    LCPDEBUG(LOG_INFO, ("lcp_rejci: void opt %d rejected\n", opt)); \
  }
#define REJCISHORT(opt, neg, val) \
  if (go->neg && \
      len >= CILEN_SHORT && \
      p[1] == CILEN_SHORT && \
      p[0] == opt) { \
    len -= CILEN_SHORT; \
    INCPTR(2, p); \
    GETSHORT(cishort, p); \
    /* Check rejected value. */ \
    if (cishort != val) { \
      goto bad; \
    } \
    try.neg = 0; \
    LCPDEBUG(LOG_INFO, ("lcp_rejci: short opt %d rejected\n", opt)); \
  }
#define REJCICHAP(opt, neg, val, digest) \
  if (go->neg && \
      len >= CILEN_CHAP && \
      p[1] == CILEN_CHAP && \
      p[0] == opt) { \
    len -= CILEN_CHAP; \
    INCPTR(2, p); \
    GETSHORT(cishort, p); \
    GETCHAR(cichar, p); \
    /* Check rejected value. */ \
    if (cishort != val || cichar != digest) { \
      goto bad; \
    } \
    try.neg = 0; \
    try.neg_upap = 0; \
    LCPDEBUG(LOG_INFO, ("lcp_rejci: chap opt %d rejected\n", opt)); \
  }
#define REJCILONG(opt, neg, val) \
  if (go->neg && \
      len >= CILEN_LONG && \
      p[1] == CILEN_LONG && \
      p[0] == opt) { \
    len -= CILEN_LONG; \
    INCPTR(2, p); \
    GETLONG(cilong, p); \
    /* Check rejected value. */ \
    if (cilong != val) { \
      goto bad; \
    } \
    try.neg = 0; \
    LCPDEBUG(LOG_INFO, ("lcp_rejci: long opt %d rejected\n", opt)); \
  }
#define REJCILQR(opt, neg, val) \
  if (go->neg && \
      len >= CILEN_LQR && \
      p[1] == CILEN_LQR && \
      p[0] == opt) { \
    len -= CILEN_LQR; \
    INCPTR(2, p); \
    GETSHORT(cishort, p); \
    GETLONG(cilong, p); \
    /* Check rejected value. */ \
    if (cishort != PPP_LQR || cilong != val) { \
      goto bad; \
    } \
    try.neg = 0; \
    LCPDEBUG(LOG_INFO, ("lcp_rejci: LQR opt %d rejected\n", opt)); \
  }
#define REJCICBCP(opt, neg, val) \
  if (go->neg && \
      len >= CILEN_CBCP && \
      p[1] == CILEN_CBCP && \
      p[0] == opt) { \
    len -= CILEN_CBCP; \
    INCPTR(2, p); \
    GETCHAR(cichar, p); \
    /* Check rejected value. */ \
    if (cichar != val) { \
      goto bad; \
    } \
    try.neg = 0; \
    LCPDEBUG(LOG_INFO, ("lcp_rejci: Callback opt %d rejected\n", opt)); \
  }
  
  REJCISHORT(CI_MRU, neg_mru, go->mru);
  REJCILONG(CI_ASYNCMAP, neg_asyncmap, go->asyncmap);
  REJCICHAP(CI_AUTHTYPE, neg_chap, PPP_CHAP, go->chap_mdtype);
  if (!go->neg_chap) {
    REJCISHORT(CI_AUTHTYPE, neg_upap, PPP_PAP);
  }
  REJCILQR(CI_QUALITY, neg_lqr, go->lqr_period);
  REJCICBCP(CI_CALLBACK, neg_cbcp, CBCP_OPT);
  REJCILONG(CI_MAGICNUMBER, neg_magicnumber, go->magicnumber);
  REJCIVOID(CI_PCOMPRESSION, neg_pcompression);
  REJCIVOID(CI_ACCOMPRESSION, neg_accompression);
  
  /*
   * If there are any remaining CIs, then this packet is bad.
   */
  if (len != 0) {
    goto bad;
  }
  /*
   * Now we can update state.
   */
  if (f->state != LS_OPENED) {
    *go = try;
  }
  return 1;
  
bad:
  LCPDEBUG(LOG_WARNING, ("lcp_rejci: received bad Reject!\n"));
  return 0;
}


/*
 * lcp_reqci - Check the peer's requested CIs and send appropriate response.
 *
 * Returns: CONFACK, CONFNAK or CONFREJ and input packet modified
 * appropriately.  If reject_if_disagree is non-zero, doesn't return
 * CONFNAK; returns CONFREJ if it can't return CONFACK.
 */
static int
lcp_reqci(fsm *f, 
          u_char *inp,    /* Requested CIs */
          int *lenp,      /* Length of requested CIs */
          int reject_if_disagree)
{
  lcp_options *go = &lcp_gotoptions[f->unit];
  lcp_options *ho = &lcp_hisoptions[f->unit];
  lcp_options *ao = &lcp_allowoptions[f->unit];
  u_char *cip, *next;         /* Pointer to current and next CIs */
  int cilen, citype;          /* Parsed len, type */
  u_char cichar;              /* Parsed char value */
  u_short cishort;            /* Parsed short value */
  u32_t cilong;               /* Parse long value */
  int rc = CONFACK;           /* Final packet return code */
  int orc;                    /* Individual option return code */
  u_char *p;                  /* Pointer to next char to parse */
  u_char *rejp;               /* Pointer to next char in reject frame */
  u_char *nakp;               /* Pointer to next char in Nak frame */
  int l = *lenp;              /* Length left */
#if TRACELCP > 0
  char traceBuf[80];
  size_t traceNdx = 0;
#endif

  /*
   * Reset all his options.
   */
  BZERO(ho, sizeof(*ho));

  /*
   * Process all his options.
   */
  next = inp;
  nakp = nak_buffer;
  rejp = inp;
  while (l) {
    orc = CONFACK;      /* Assume success */
    cip = p = next;     /* Remember begining of CI */
    if (l < 2 ||        /* Not enough data for CI header or */
        p[1] < 2 ||     /*  CI length too small or */
        p[1] > l) {     /*  CI length too big? */
      LCPDEBUG(LOG_WARNING, ("lcp_reqci: bad CI length!\n"));
      orc = CONFREJ;    /* Reject bad CI */
      cilen = l;        /* Reject till end of packet */
      l = 0;            /* Don't loop again */
      citype = 0;
      goto endswitch;
    }
    GETCHAR(citype, p); /* Parse CI type */
    GETCHAR(cilen, p);  /* Parse CI length */
    l -= cilen;         /* Adjust remaining length */
    next += cilen;      /* Step to next CI */

    switch (citype) {   /* Check CI type */
      case CI_MRU:
        if (!ao->neg_mru) {    /* Allow option? */
          LCPDEBUG(LOG_INFO, ("lcp_reqci: Reject MRU - not allowed\n"));
          orc = CONFREJ;    /* Reject CI */
          break;
        } else if (cilen != CILEN_SHORT) {  /* Check CI length */
          LCPDEBUG(LOG_INFO, ("lcp_reqci: Reject MRU - bad length\n"));
          orc = CONFREJ;    /* Reject CI */
          break;
        }
        GETSHORT(cishort, p);  /* Parse MRU */

        /*
         * He must be able to receive at least our minimum.
         * No need to check a maximum.  If he sends a large number,
         * we'll just ignore it.
         */
        if (cishort < PPP_MINMRU) {
          LCPDEBUG(LOG_INFO, ("lcp_reqci: Nak - MRU too small\n"));
          orc = CONFNAK;    /* Nak CI */
          PUTCHAR(CI_MRU, nakp);
          PUTCHAR(CILEN_SHORT, nakp);
          PUTSHORT(PPP_MINMRU, nakp);  /* Give him a hint */
          break;
        }
        ho->neg_mru = 1;    /* Remember he sent MRU */
        ho->mru = cishort;    /* And remember value */
#if TRACELCP > 0
        snprintf(&traceBuf[traceNdx], sizeof(traceBuf), " MRU %d", cishort);
        traceNdx = strlen(traceBuf);
#endif
        break;

      case CI_ASYNCMAP:
        if (!ao->neg_asyncmap) {
          LCPDEBUG(LOG_INFO, ("lcp_reqci: Reject ASYNCMAP not allowed\n"));
          orc = CONFREJ;
          break;
        } else if (cilen != CILEN_LONG) {
          LCPDEBUG(LOG_INFO, ("lcp_reqci: Reject ASYNCMAP bad length\n"));
          orc = CONFREJ;
          break;
        }
        GETLONG(cilong, p);
        
        /*
         * Asyncmap must have set at least the bits
         * which are set in lcp_allowoptions[unit].asyncmap.
         */
        if ((ao->asyncmap & ~cilong) != 0) {
          LCPDEBUG(LOG_INFO, ("lcp_reqci: Nak ASYNCMAP %lX missing %lX\n", 
                    cilong, ao->asyncmap));
          orc = CONFNAK;
          PUTCHAR(CI_ASYNCMAP, nakp);
          PUTCHAR(CILEN_LONG, nakp);
          PUTLONG(ao->asyncmap | cilong, nakp);
          break;
        }
        ho->neg_asyncmap = 1;
        ho->asyncmap = cilong;
#if TRACELCP > 0
        snprintf(&traceBuf[traceNdx], sizeof(traceBuf), " ASYNCMAP=%lX", cilong);
        traceNdx = strlen(traceBuf);
#endif
        break;

      case CI_AUTHTYPE:
        if (cilen < CILEN_SHORT) {
          LCPDEBUG(LOG_INFO, ("lcp_reqci: Reject AUTHTYPE missing arg\n"));
          orc = CONFREJ;
          break;
        } else if (!(ao->neg_upap || ao->neg_chap)) {
          /*
           * Reject the option if we're not willing to authenticate.
           */
          LCPDEBUG(LOG_INFO, ("lcp_reqci: Reject AUTHTYPE not allowed\n"));
          orc = CONFREJ;
          break;
        }
        GETSHORT(cishort, p);
        
        /*
         * Authtype must be UPAP or CHAP.
         *
         * Note: if both ao->neg_upap and ao->neg_chap are set,
         * and the peer sends a Configure-Request with two
         * authenticate-protocol requests, one for CHAP and one
         * for UPAP, then we will reject the second request.
         * Whether we end up doing CHAP or UPAP depends then on
         * the ordering of the CIs in the peer's Configure-Request.
         */
        
        if (cishort == PPP_PAP) {
          if (ho->neg_chap) {  /* we've already accepted CHAP */
            LCPDEBUG(LOG_WARNING, ("lcp_reqci: Reject AUTHTYPE PAP already accepted\n"));
            orc = CONFREJ;
            break;
          } else if (cilen != CILEN_SHORT) {
            LCPDEBUG(LOG_WARNING, ("lcp_reqci: Reject AUTHTYPE PAP bad len\n"));
            orc = CONFREJ;
            break;
          }
          if (!ao->neg_upap) {  /* we don't want to do PAP */
            LCPDEBUG(LOG_WARNING, ("lcp_reqci: Nak AUTHTYPE PAP not allowed\n"));
            orc = CONFNAK;  /* NAK it and suggest CHAP */
            PUTCHAR(CI_AUTHTYPE, nakp);
            PUTCHAR(CILEN_CHAP, nakp);
            PUTSHORT(PPP_CHAP, nakp);
            PUTCHAR(ao->chap_mdtype, nakp);
            break;
          }
          ho->neg_upap = 1;
#if TRACELCP > 0
          snprintf(&traceBuf[traceNdx], sizeof(traceBuf), " PAP (%X)", cishort);
          traceNdx = strlen(traceBuf);
#endif
          break;
        }
        if (cishort == PPP_CHAP) {
          if (ho->neg_upap) {  /* we've already accepted PAP */
            LCPDEBUG(LOG_WARNING, ("lcp_reqci: Reject AUTHTYPE CHAP accepted PAP\n"));
            orc = CONFREJ;
            break;
          } else if (cilen != CILEN_CHAP) {
            LCPDEBUG(LOG_WARNING, ("lcp_reqci: Reject AUTHTYPE CHAP bad len\n"));
            orc = CONFREJ;
            break;
          }
          if (!ao->neg_chap) {  /* we don't want to do CHAP */
            LCPDEBUG(LOG_WARNING, ("lcp_reqci: Nak AUTHTYPE CHAP not allowed\n"));
            orc = CONFNAK;  /* NAK it and suggest PAP */
            PUTCHAR(CI_AUTHTYPE, nakp);
            PUTCHAR(CILEN_SHORT, nakp);
            PUTSHORT(PPP_PAP, nakp);
            break;
          }
          GETCHAR(cichar, p);  /* get digest type*/
          if (cichar != CHAP_DIGEST_MD5
#if MSCHAP_SUPPORT
              && cichar != CHAP_MICROSOFT
#endif
          ) {
            LCPDEBUG(LOG_WARNING, ("lcp_reqci: Nak AUTHTYPE CHAP digest=%d\n", (int)cichar));
            orc = CONFNAK;
            PUTCHAR(CI_AUTHTYPE, nakp);
            PUTCHAR(CILEN_CHAP, nakp);
            PUTSHORT(PPP_CHAP, nakp);
            PUTCHAR(ao->chap_mdtype, nakp);
            break;
          }
#if TRACELCP > 0
          snprintf(&traceBuf[traceNdx], sizeof(traceBuf), " CHAP %X,%d", cishort, (int)cichar);
          traceNdx = strlen(traceBuf);
#endif
          ho->chap_mdtype = cichar; /* save md type */
          ho->neg_chap = 1;
          break;
        }
        
        /*
         * We don't recognize the protocol they're asking for.
         * Nak it with something we're willing to do.
         * (At this point we know ao->neg_upap || ao->neg_chap.)
         */
        orc = CONFNAK;
        PUTCHAR(CI_AUTHTYPE, nakp);
        if (ao->neg_chap) {
          LCPDEBUG(LOG_WARNING, ("lcp_reqci: Nak AUTHTYPE %d req CHAP\n", cishort));
          PUTCHAR(CILEN_CHAP, nakp);
          PUTSHORT(PPP_CHAP, nakp);
          PUTCHAR(ao->chap_mdtype, nakp);
        } else {
          LCPDEBUG(LOG_WARNING, ("lcp_reqci: Nak AUTHTYPE %d req PAP\n", cishort));
          PUTCHAR(CILEN_SHORT, nakp);
          PUTSHORT(PPP_PAP, nakp);
        }
        break;
      
      case CI_QUALITY:
        GETSHORT(cishort, p);
        GETLONG(cilong, p);
#if TRACELCP > 0
        snprintf(&traceBuf[traceNdx], sizeof(traceBuf), " QUALITY (%x %x)", cishort, (unsigned int) cilong);
        traceNdx = strlen(traceBuf);
#endif

        if (!ao->neg_lqr ||
            cilen != CILEN_LQR) {
          orc = CONFREJ;
          break;
        }
        
        /*
         * Check the protocol and the reporting period.
         * XXX When should we Nak this, and what with?
         */
        if (cishort != PPP_LQR) {
          orc = CONFNAK;
          PUTCHAR(CI_QUALITY, nakp);
          PUTCHAR(CILEN_LQR, nakp);
          PUTSHORT(PPP_LQR, nakp);
          PUTLONG(ao->lqr_period, nakp);
          break;
        }
        break;
      
      case CI_MAGICNUMBER:
        if (!(ao->neg_magicnumber || go->neg_magicnumber) ||
            cilen != CILEN_LONG) {
          orc = CONFREJ;
          break;
        }
        GETLONG(cilong, p);
#if TRACELCP > 0
        snprintf(&traceBuf[traceNdx], sizeof(traceBuf), " MAGICNUMBER (%lX)", cilong);
        traceNdx = strlen(traceBuf);
#endif

        /*
         * He must have a different magic number.
         */
        if (go->neg_magicnumber &&
            cilong == go->magicnumber) {
          cilong = magic();  /* Don't put magic() inside macro! */
          orc = CONFNAK;
          PUTCHAR(CI_MAGICNUMBER, nakp);
          PUTCHAR(CILEN_LONG, nakp);
          PUTLONG(cilong, nakp);
          break;
        }
        ho->neg_magicnumber = 1;
        ho->magicnumber = cilong;
        break;
      
      
      case CI_PCOMPRESSION:
#if TRACELCP > 0
        snprintf(&traceBuf[traceNdx], sizeof(traceBuf), " PCOMPRESSION");
        traceNdx = strlen(traceBuf);
#endif
        if (!ao->neg_pcompression ||
            cilen != CILEN_VOID) {
          orc = CONFREJ;
          break;
        }
        ho->neg_pcompression = 1;
        break;
      
      case CI_ACCOMPRESSION:
#if TRACELCP > 0
        snprintf(&traceBuf[traceNdx], sizeof(traceBuf), " ACCOMPRESSION");
        traceNdx = strlen(traceBuf);
#endif
        if (!ao->neg_accompression ||
            cilen != CILEN_VOID) {
          orc = CONFREJ;
          break;
        }
        ho->neg_accompression = 1;
        break;
      
      case CI_MRRU:
#if TRACELCP > 0
        snprintf(&traceBuf[traceNdx], sizeof(traceBuf), " CI_MRRU");
        traceNdx = strlen(traceBuf);
#endif
        orc = CONFREJ;
        break;
      
      case CI_SSNHF:
#if TRACELCP > 0
        snprintf(&traceBuf[traceNdx], sizeof(traceBuf), " CI_SSNHF");
        traceNdx = strlen(traceBuf);
#endif
        orc = CONFREJ;
        break;
      
      case CI_EPDISC:
#if TRACELCP > 0
        snprintf(&traceBuf[traceNdx], sizeof(traceBuf), " CI_EPDISC");
        traceNdx = strlen(traceBuf);
#endif
        orc = CONFREJ;
        break;
      
      default:
#if TRACELCP
        snprintf(&traceBuf[traceNdx], sizeof(traceBuf), " unknown %d", citype);
        traceNdx = strlen(traceBuf);
#endif
        orc = CONFREJ;
        break;
    }

  endswitch:
#if TRACELCP
    if (traceNdx >= 80 - 32) {
      LCPDEBUG(LOG_INFO, ("lcp_reqci: rcvd%s\n", traceBuf));
      traceNdx = 0;
    }
#endif
    if (orc == CONFACK && /* Good CI */
        rc != CONFACK) {  /*  but prior CI wasnt? */
      continue;           /* Don't send this one */
    }

    if (orc == CONFNAK) {     /* Nak this CI? */
      if (reject_if_disagree  /* Getting fed up with sending NAKs? */
          && citype != CI_MAGICNUMBER) {
        orc = CONFREJ;        /* Get tough if so */
      } else {
        if (rc == CONFREJ) {  /* Rejecting prior CI? */
          continue;           /* Don't send this one */
        }
        rc = CONFNAK;
      }
    }
    if (orc == CONFREJ) {        /* Reject this CI */
      rc = CONFREJ;
      if (cip != rejp) {         /* Need to move rejected CI? */
        BCOPY(cip, rejp, cilen); /* Move it */
      }
      INCPTR(cilen, rejp);       /* Update output pointer */
    }
  }

  /*
   * If we wanted to send additional NAKs (for unsent CIs), the
   * code would go here.  The extra NAKs would go at *nakp.
   * At present there are no cases where we want to ask the
   * peer to negotiate an option.
   */

  switch (rc) {
    case CONFACK:
      *lenp = (int)(next - inp);
      break;
    case CONFNAK:
      /*
       * Copy the Nak'd options from the nak_buffer to the caller's buffer.
       */
      *lenp = (int)(nakp - nak_buffer);
      BCOPY(nak_buffer, inp, *lenp);
      break;
    case CONFREJ:
      *lenp = (int)(rejp - inp);
      break;
  }

#if TRACELCP > 0
  if (traceNdx > 0) {
    LCPDEBUG(LOG_INFO, ("lcp_reqci: %s\n", traceBuf));
  }
#endif
  LCPDEBUG(LOG_INFO, ("lcp_reqci: returning CONF%s.\n", CODENAME(rc)));
  return (rc);      /* Return final code */
}


/*
 * lcp_up - LCP has come UP.
 */
static void
lcp_up(fsm *f)
{
  lcp_options *wo = &lcp_wantoptions[f->unit];
  lcp_options *ho = &lcp_hisoptions[f->unit];
  lcp_options *go = &lcp_gotoptions[f->unit];
  lcp_options *ao = &lcp_allowoptions[f->unit];

  if (!go->neg_magicnumber) {
    go->magicnumber = 0;
  }
  if (!ho->neg_magicnumber) {
    ho->magicnumber = 0;
  }

  /*
   * Set our MTU to the smaller of the MTU we wanted and
   * the MRU our peer wanted.  If we negotiated an MRU,
   * set our MRU to the larger of value we wanted and
   * the value we got in the negotiation.
   */
  ppp_send_config(f->unit, LWIP_MIN(ao->mru, (ho->neg_mru? ho->mru: PPP_MRU)),
                 (ho->neg_asyncmap? ho->asyncmap: 0xffffffffl),
                  ho->neg_pcompression, ho->neg_accompression);
  /*
   * If the asyncmap hasn't been negotiated, we really should
   * set the receive asyncmap to ffffffff, but we set it to 0
   * for backwards contemptibility.
   */
  ppp_recv_config(f->unit, (go->neg_mru? LWIP_MAX(wo->mru, go->mru): PPP_MRU),
                 (go->neg_asyncmap? go->asyncmap: 0x00000000),
                  go->neg_pcompression, go->neg_accompression);

  if (ho->neg_mru) {
    peer_mru[f->unit] = ho->mru;
  }

  lcp_echo_lowerup(f->unit); /* Enable echo messages */

  link_established(f->unit); /* The link is up; authenticate now */
}


/*
 * lcp_down - LCP has gone DOWN.
 *
 * Alert other protocols.
 */
static void
lcp_down(fsm *f)
{
  lcp_options *go = &lcp_gotoptions[f->unit];

  lcp_echo_lowerdown(f->unit);

  link_down(f->unit);

  ppp_send_config(f->unit, PPP_MRU, 0xffffffffl, 0, 0);
  ppp_recv_config(f->unit, PPP_MRU,
                  (go->neg_asyncmap? go->asyncmap: 0x00000000),
                   go->neg_pcompression, go->neg_accompression);
  peer_mru[f->unit] = PPP_MRU;
}


/*
 * lcp_starting - LCP needs the lower layer up.
 */
static void
lcp_starting(fsm *f)
{
  link_required(f->unit); /* lwip: currently does nothing */
}


/*
 * lcp_finished - LCP has finished with the lower layer.
 */
static void
lcp_finished(fsm *f)
{
  link_terminated(f->unit); /* we are finished with the link */
}


#if PPP_ADDITIONAL_CALLBACKS
/*
 * print_string - print a readable representation of a string using
 * printer.
 */
static void
print_string( char *p, int len, void (*printer) (void *, char *, ...), void *arg)
{
  int c;
  
  printer(arg, "\"");
  for (; len > 0; --len) {
    c = *p++;
    if (' ' <= c && c <= '~') {
        if (c == '\\' || c == '"') {
          printer(arg, "\\");
        }
        printer(arg, "%c", c);
    } else {
      switch (c) {
        case '\n':
          printer(arg, "\\n");
          break;
        case '\r':
          printer(arg, "\\r");
          break;
        case '\t':
          printer(arg, "\\t");
          break;
        default:
          printer(arg, "\\%.3o", c);
        }
    }
  }
  printer(arg, "\"");
}


/*
 * lcp_printpkt - print the contents of an LCP packet.
 */
static char *lcp_codenames[] = {
  "ConfReq", "ConfAck", "ConfNak", "ConfRej",
  "TermReq", "TermAck", "CodeRej", "ProtRej",
  "EchoReq", "EchoRep", "DiscReq"
};

static int
lcp_printpkt( u_char *p, int plen, void (*printer) (void *, char *, ...), void *arg)
{
  int code, id, len, olen;
  u_char *pstart, *optend;
  u_short cishort;
  u32_t cilong;

  if (plen < HEADERLEN) {
    return 0;
  }
  pstart = p;
  GETCHAR(code, p);
  GETCHAR(id, p);
  GETSHORT(len, p);
  if (len < HEADERLEN || len > plen) {
    return 0;
  }

  if (code >= 1 && code <= sizeof(lcp_codenames) / sizeof(char *)) {
    printer(arg, " %s", lcp_codenames[code-1]);
  } else {
    printer(arg, " code=0x%x", code);
  }
  printer(arg, " id=0x%x", id);
  len -= HEADERLEN;
  switch (code) {
    case CONFREQ:
    case CONFACK:
    case CONFNAK:
    case CONFREJ:
      /* print option list */
      while (len >= 2) {
        GETCHAR(code, p);
        GETCHAR(olen, p);
        p -= 2;
        if (olen < 2 || olen > len) {
          break;
        }
        printer(arg, " <");
        len -= olen;
        optend = p + olen;
        switch (code) {
          case CI_MRU:
            if (olen == CILEN_SHORT) {
              p += 2;
              GETSHORT(cishort, p);
              printer(arg, "mru %d", cishort);
            }
            break;
          case CI_ASYNCMAP:
            if (olen == CILEN_LONG) {
              p += 2;
              GETLONG(cilong, p);
              printer(arg, "asyncmap 0x%lx", cilong);
            }
            break;
          case CI_AUTHTYPE:
            if (olen >= CILEN_SHORT) {
              p += 2;
              printer(arg, "auth ");
              GETSHORT(cishort, p);
              switch (cishort) {
                case PPP_PAP:
                  printer(arg, "pap");
                  break;
                case PPP_CHAP:
                  printer(arg, "chap");
                  break;
                default:
                  printer(arg, "0x%x", cishort);
              }
            }
            break;
          case CI_QUALITY:
            if (olen >= CILEN_SHORT) {
              p += 2;
              printer(arg, "quality ");
              GETSHORT(cishort, p);
              switch (cishort) {
                case PPP_LQR:
                  printer(arg, "lqr");
                  break;
                default:
                  printer(arg, "0x%x", cishort);
              }
            }
            break;
          case CI_CALLBACK:
            if (olen >= CILEN_CHAR) {
              p += 2;
              printer(arg, "callback ");
              GETSHORT(cishort, p);
              switch (cishort) {
                case CBCP_OPT:
                  printer(arg, "CBCP");
                  break;
                default:
                  printer(arg, "0x%x", cishort);
              }
            }
            break;
          case CI_MAGICNUMBER:
            if (olen == CILEN_LONG) {
              p += 2;
              GETLONG(cilong, p);
              printer(arg, "magic 0x%x", cilong);
            }
            break;
          case CI_PCOMPRESSION:
            if (olen == CILEN_VOID) {
              p += 2;
              printer(arg, "pcomp");
            }
            break;
          case CI_ACCOMPRESSION:
            if (olen == CILEN_VOID) {
              p += 2;
              printer(arg, "accomp");
            }
            break;
        }
        while (p < optend) {
          GETCHAR(code, p);
          printer(arg, " %.2x", code);
        }
        printer(arg, ">");
      }
      break;
    
    case TERMACK:
    case TERMREQ:
      if (len > 0 && *p >= ' ' && *p < 0x7f) {
        printer(arg, " ");
        print_string((char*)p, len, printer, arg);
        p += len;
        len = 0;
      }
      break;
    
    case ECHOREQ:
    case ECHOREP:
    case DISCREQ:
      if (len >= 4) {
        GETLONG(cilong, p);
        printer(arg, " magic=0x%x", cilong);
        p += 4;
        len -= 4;
      }
      break;
  }

  /* print the rest of the bytes in the packet */
  for (; len > 0; --len) {
    GETCHAR(code, p);
    printer(arg, " %.2x", code);
  }

  return (int)(p - pstart);
}
#endif /* PPP_ADDITIONAL_CALLBACKS */

/*
 * Time to shut down the link because there is nothing out there.
 */
static void
LcpLinkFailure (fsm *f)
{
  if (f->state == LS_OPENED) {
    LCPDEBUG(LOG_INFO, ("No response to %d echo-requests\n", lcp_echos_pending));
    LCPDEBUG(LOG_NOTICE, ("Serial link appears to be disconnected.\n"));
    lcp_close(f->unit, "Peer not responding");
  }
}

/*
 * Timer expired for the LCP echo requests from this process.
 */
static void
LcpEchoCheck (fsm *f)
{
  LcpSendEchoRequest (f);

  /*
   * Start the timer for the next interval.
   */
  LWIP_ASSERT("lcp_echo_timer_running == 0", lcp_echo_timer_running == 0);

  TIMEOUT (LcpEchoTimeout, f, lcp_echo_interval);
  lcp_echo_timer_running = 1;
}

/*
 * LcpEchoTimeout - Timer expired on the LCP echo
 */
static void
LcpEchoTimeout (void *arg)
{
  if (lcp_echo_timer_running != 0) {
    lcp_echo_timer_running = 0;
    LcpEchoCheck ((fsm *) arg);
  }
}

/*
 * LcpEchoReply - LCP has received a reply to the echo
 */
static void
lcp_received_echo_reply (fsm *f, int id, u_char *inp, int len)
{
  u32_t magic;

  LWIP_UNUSED_ARG(id);

  /* Check the magic number - don't count replies from ourselves. */
  if (len < 4) {
    LCPDEBUG(LOG_WARNING, ("lcp: received short Echo-Reply, length %d\n", len));
    return;
  }
  GETLONG(magic, inp);
  if (lcp_gotoptions[f->unit].neg_magicnumber && magic == lcp_gotoptions[f->unit].magicnumber) {
    LCPDEBUG(LOG_WARNING, ("appear to have received our own echo-reply!\n"));
    return;
  }

  /* Reset the number of outstanding echo frames */
  lcp_echos_pending = 0;
}

/*
 * LcpSendEchoRequest - Send an echo request frame to the peer
 */
static void
LcpSendEchoRequest (fsm *f)
{
  u32_t lcp_magic;
  u_char pkt[4], *pktp;

  /*
   * Detect the failure of the peer at this point.
   */
  if (lcp_echo_fails != 0) {
    if (lcp_echos_pending >= lcp_echo_fails) {
      LcpLinkFailure(f);
      lcp_echos_pending = 0;
    }
  }

  /*
   * Make and send the echo request frame.
   */
  if (f->state == LS_OPENED) {
    lcp_magic = lcp_gotoptions[f->unit].magicnumber;
    pktp = pkt;
    PUTLONG(lcp_magic, pktp);
    fsm_sdata(f, ECHOREQ, (u_char)(lcp_echo_number++ & 0xFF), pkt, (int)(pktp - pkt));
    ++lcp_echos_pending;
  }
}

/*
 * lcp_echo_lowerup - Start the timer for the LCP frame
 */

static void
lcp_echo_lowerup (int unit)
{
  fsm *f = &lcp_fsm[unit];

  /* Clear the parameters for generating echo frames */
  lcp_echos_pending      = 0;
  lcp_echo_number        = 0;
  lcp_echo_timer_running = 0;

  /* If a timeout interval is specified then start the timer */
  if (lcp_echo_interval != 0) {
    LcpEchoCheck (f);
  }
}

/*
 * lcp_echo_lowerdown - Stop the timer for the LCP frame
 */

static void
lcp_echo_lowerdown (int unit)
{
  fsm *f = &lcp_fsm[unit];

  if (lcp_echo_timer_running != 0) {
    UNTIMEOUT (LcpEchoTimeout, f);
    lcp_echo_timer_running = 0;
  }
}

#endif /* PPP_SUPPORT */
