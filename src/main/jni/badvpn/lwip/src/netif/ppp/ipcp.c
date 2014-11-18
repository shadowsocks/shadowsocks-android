/** In contrast to pppd 2.3.1, DNS support has been added, proxy-ARP and
    dial-on-demand has been stripped. */
/*****************************************************************************
* ipcp.c - Network PPP IP Control Protocol program file.
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
* 97-12-08 Guy Lancaster <lancasterg@acm.org>, Global Election Systems Inc.
*   Original.
*****************************************************************************/
/*
 * ipcp.c - PPP IP Control Protocol.
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

#include "auth.h"
#include "fsm.h"
#include "vj.h"
#include "ipcp.h"

#include "lwip/inet.h"

#include <string.h>

/* #define OLD_CI_ADDRS 1 */ /* Support deprecated address negotiation. */

/* global vars */
ipcp_options ipcp_wantoptions[NUM_PPP];  /* Options that we want to request */
ipcp_options ipcp_gotoptions[NUM_PPP];   /* Options that peer ack'd */
ipcp_options ipcp_allowoptions[NUM_PPP]; /* Options we allow peer to request */
ipcp_options ipcp_hisoptions[NUM_PPP];   /* Options that we ack'd */

/* local vars */
static int default_route_set[NUM_PPP]; /* Have set up a default route */
static int cis_received[NUM_PPP];      /* # Conf-Reqs received */


/*
 * Callbacks for fsm code.  (CI = Configuration Information)
 */
static void ipcp_resetci (fsm *);                     /* Reset our CI */
static int  ipcp_cilen (fsm *);                       /* Return length of our CI */
static void ipcp_addci (fsm *, u_char *, int *);      /* Add our CI */
static int  ipcp_ackci (fsm *, u_char *, int);        /* Peer ack'd our CI */
static int  ipcp_nakci (fsm *, u_char *, int);        /* Peer nak'd our CI */
static int  ipcp_rejci (fsm *, u_char *, int);        /* Peer rej'd our CI */
static int  ipcp_reqci (fsm *, u_char *, int *, int); /* Rcv CI */
static void ipcp_up (fsm *);                          /* We're UP */
static void ipcp_down (fsm *);                        /* We're DOWN */
#if PPP_ADDITIONAL_CALLBACKS
static void ipcp_script (fsm *, char *); /* Run an up/down script */
#endif
static void ipcp_finished (fsm *);                    /* Don't need lower layer */


fsm ipcp_fsm[NUM_PPP]; /* IPCP fsm structure */


static fsm_callbacks ipcp_callbacks = { /* IPCP callback routines */
  ipcp_resetci,  /* Reset our Configuration Information */
  ipcp_cilen,    /* Length of our Configuration Information */
  ipcp_addci,    /* Add our Configuration Information */
  ipcp_ackci,    /* ACK our Configuration Information */
  ipcp_nakci,    /* NAK our Configuration Information */
  ipcp_rejci,    /* Reject our Configuration Information */
  ipcp_reqci,    /* Request peer's Configuration Information */
  ipcp_up,       /* Called when fsm reaches LS_OPENED state */
  ipcp_down,     /* Called when fsm leaves LS_OPENED state */
  NULL,          /* Called when we want the lower layer up */
  ipcp_finished, /* Called when we want the lower layer down */
  NULL,          /* Called when Protocol-Reject received */
  NULL,          /* Retransmission is necessary */
  NULL,          /* Called to handle protocol-specific codes */
  "IPCP"         /* String name of protocol */
};

/*
 * Protocol entry points from main code.
 */
static void ipcp_init (int);
static void ipcp_open (int);
static void ipcp_close (int, char *);
static void ipcp_lowerup (int);
static void ipcp_lowerdown (int);
static void ipcp_input (int, u_char *, int);
static void ipcp_protrej (int);


struct protent ipcp_protent = {
  PPP_IPCP,
  ipcp_init,
  ipcp_input,
  ipcp_protrej,
  ipcp_lowerup,
  ipcp_lowerdown,
  ipcp_open,
  ipcp_close,
#if PPP_ADDITIONAL_CALLBACKS
  ipcp_printpkt,
  NULL,
#endif /* PPP_ADDITIONAL_CALLBACKS */
  1,
  "IPCP",
#if PPP_ADDITIONAL_CALLBACKS
  ip_check_options,
  NULL,
  ip_active_pkt
#endif /* PPP_ADDITIONAL_CALLBACKS */
};

static void ipcp_clear_addrs (int);

/*
 * Lengths of configuration options.
 */
#define CILEN_VOID     2
#define CILEN_COMPRESS 4  /* min length for compression protocol opt. */
#define CILEN_VJ       6  /* length for RFC1332 Van-Jacobson opt. */
#define CILEN_ADDR     6  /* new-style single address option */
#define CILEN_ADDRS    10 /* old-style dual address option */


#define CODENAME(x) ((x) == CONFACK ? "ACK" : \
                     (x) == CONFNAK ? "NAK" : "REJ")


/*
 * ipcp_init - Initialize IPCP.
 */
static void
ipcp_init(int unit)
{
  fsm           *f = &ipcp_fsm[unit];
  ipcp_options *wo = &ipcp_wantoptions[unit];
  ipcp_options *ao = &ipcp_allowoptions[unit];

  f->unit      = unit;
  f->protocol  = PPP_IPCP;
  f->callbacks = &ipcp_callbacks;
  fsm_init(&ipcp_fsm[unit]);

  memset(wo, 0, sizeof(*wo));
  memset(ao, 0, sizeof(*ao));

  wo->neg_addr      = 1;
  wo->ouraddr       = 0;
#if VJ_SUPPORT
  wo->neg_vj        = 1;
#else  /* VJ_SUPPORT */
  wo->neg_vj        = 0;
#endif /* VJ_SUPPORT */
  wo->vj_protocol   = IPCP_VJ_COMP;
  wo->maxslotindex  = MAX_SLOTS - 1;
  wo->cflag         = 0;
  wo->default_route = 1;

  ao->neg_addr      = 1;
#if VJ_SUPPORT
  ao->neg_vj        = 1;
#else  /* VJ_SUPPORT */
  ao->neg_vj        = 0;
#endif /* VJ_SUPPORT */
  ao->maxslotindex  = MAX_SLOTS - 1;
  ao->cflag         = 1;
  ao->default_route = 1;
}


/*
 * ipcp_open - IPCP is allowed to come up.
 */
static void
ipcp_open(int unit)
{
  fsm_open(&ipcp_fsm[unit]);
}


/*
 * ipcp_close - Take IPCP down.
 */
static void
ipcp_close(int unit, char *reason)
{
  fsm_close(&ipcp_fsm[unit], reason);
}


/*
 * ipcp_lowerup - The lower layer is up.
 */
static void
ipcp_lowerup(int unit)
{
  fsm_lowerup(&ipcp_fsm[unit]);
}


/*
 * ipcp_lowerdown - The lower layer is down.
 */
static void
ipcp_lowerdown(int unit)
{
  fsm_lowerdown(&ipcp_fsm[unit]);
}


/*
 * ipcp_input - Input IPCP packet.
 */
static void
ipcp_input(int unit, u_char *p, int len)
{
  fsm_input(&ipcp_fsm[unit], p, len);
}


/*
 * ipcp_protrej - A Protocol-Reject was received for IPCP.
 *
 * Pretend the lower layer went down, so we shut up.
 */
static void
ipcp_protrej(int unit)
{
  fsm_lowerdown(&ipcp_fsm[unit]);
}


/*
 * ipcp_resetci - Reset our CI.
 */
static void
ipcp_resetci(fsm *f)
{
  ipcp_options *wo = &ipcp_wantoptions[f->unit];
  
  wo->req_addr = wo->neg_addr && ipcp_allowoptions[f->unit].neg_addr;
  if (wo->ouraddr == 0) {
    wo->accept_local = 1;
  }
  if (wo->hisaddr == 0) {
    wo->accept_remote = 1;
  }
  /* Request DNS addresses from the peer */
  wo->req_dns1 = ppp_settings.usepeerdns;
  wo->req_dns2 = ppp_settings.usepeerdns;
  ipcp_gotoptions[f->unit] = *wo;
  cis_received[f->unit] = 0;
}


/*
 * ipcp_cilen - Return length of our CI.
 */
static int
ipcp_cilen(fsm *f)
{
  ipcp_options *go = &ipcp_gotoptions[f->unit];
  ipcp_options *wo = &ipcp_wantoptions[f->unit];
  ipcp_options *ho = &ipcp_hisoptions[f->unit];

#define LENCIVJ(neg, old)   (neg ? (old? CILEN_COMPRESS : CILEN_VJ) : 0)
#define LENCIADDR(neg, old) (neg ? (old? CILEN_ADDRS : CILEN_ADDR) : 0)
#define LENCIDNS(neg)       (neg ? (CILEN_ADDR) : 0)

  /*
   * First see if we want to change our options to the old
   * forms because we have received old forms from the peer.
   */
  if (wo->neg_addr && !go->neg_addr && !go->old_addrs) {
    /* use the old style of address negotiation */
    go->neg_addr = 1;
    go->old_addrs = 1;
  }
  if (wo->neg_vj && !go->neg_vj && !go->old_vj) {
    /* try an older style of VJ negotiation */
    if (cis_received[f->unit] == 0) {
      /* keep trying the new style until we see some CI from the peer */
      go->neg_vj = 1;
    } else {
      /* use the old style only if the peer did */
      if (ho->neg_vj && ho->old_vj) {
        go->neg_vj = 1;
        go->old_vj = 1;
        go->vj_protocol = ho->vj_protocol;
      }
    }
  }

  return (LENCIADDR(go->neg_addr, go->old_addrs) +
          LENCIVJ(go->neg_vj, go->old_vj) +
          LENCIDNS(go->req_dns1) +
          LENCIDNS(go->req_dns2));
}


/*
 * ipcp_addci - Add our desired CIs to a packet.
 */
static void
ipcp_addci(fsm *f, u_char *ucp, int *lenp)
{
  ipcp_options *go = &ipcp_gotoptions[f->unit];
  int len = *lenp;

#define ADDCIVJ(opt, neg, val, old, maxslotindex, cflag) \
  if (neg) { \
    int vjlen = old? CILEN_COMPRESS : CILEN_VJ; \
    if (len >= vjlen) { \
      PUTCHAR(opt, ucp); \
      PUTCHAR(vjlen, ucp); \
      PUTSHORT(val, ucp); \
      if (!old) { \
        PUTCHAR(maxslotindex, ucp); \
        PUTCHAR(cflag, ucp); \
      } \
      len -= vjlen; \
    } else { \
      neg = 0; \
    } \
  }

#define ADDCIADDR(opt, neg, old, val1, val2) \
  if (neg) { \
    int addrlen = (old? CILEN_ADDRS: CILEN_ADDR); \
    if (len >= addrlen) { \
      u32_t l; \
      PUTCHAR(opt, ucp); \
      PUTCHAR(addrlen, ucp); \
      l = ntohl(val1); \
      PUTLONG(l, ucp); \
      if (old) { \
        l = ntohl(val2); \
        PUTLONG(l, ucp); \
      } \
      len -= addrlen; \
    } else { \
      neg = 0; \
    } \
  }

#define ADDCIDNS(opt, neg, addr) \
  if (neg) { \
    if (len >= CILEN_ADDR) { \
      u32_t l; \
      PUTCHAR(opt, ucp); \
      PUTCHAR(CILEN_ADDR, ucp); \
      l = ntohl(addr); \
      PUTLONG(l, ucp); \
      len -= CILEN_ADDR; \
    } else { \
      neg = 0; \
    } \
  }

  ADDCIADDR((go->old_addrs? CI_ADDRS: CI_ADDR), go->neg_addr,
      go->old_addrs, go->ouraddr, go->hisaddr);

  ADDCIVJ(CI_COMPRESSTYPE, go->neg_vj, go->vj_protocol, go->old_vj,
      go->maxslotindex, go->cflag);

  ADDCIDNS(CI_MS_DNS1, go->req_dns1, go->dnsaddr[0]);

  ADDCIDNS(CI_MS_DNS2, go->req_dns2, go->dnsaddr[1]);

  *lenp -= len;
}


/*
 * ipcp_ackci - Ack our CIs.
 *
 * Returns:
 *  0 - Ack was bad.
 *  1 - Ack was good.
 */
static int
ipcp_ackci(fsm *f, u_char *p, int len)
{
  ipcp_options *go = &ipcp_gotoptions[f->unit];
  u_short cilen, citype, cishort;
  u32_t cilong;
  u_char cimaxslotindex, cicflag;

  /*
   * CIs must be in exactly the same order that we sent...
   * Check packet length and CI length at each step.
   * If we find any deviations, then this packet is bad.
   */

#define ACKCIVJ(opt, neg, val, old, maxslotindex, cflag) \
  if (neg) { \
    int vjlen = old? CILEN_COMPRESS : CILEN_VJ; \
    if ((len -= vjlen) < 0) { \
      goto bad; \
    } \
    GETCHAR(citype, p); \
    GETCHAR(cilen, p); \
    if (cilen != vjlen || \
        citype != opt) { \
      goto bad; \
    } \
    GETSHORT(cishort, p); \
    if (cishort != val) { \
      goto bad; \
    } \
    if (!old) { \
      GETCHAR(cimaxslotindex, p); \
      if (cimaxslotindex != maxslotindex) { \
        goto bad; \
      } \
      GETCHAR(cicflag, p); \
      if (cicflag != cflag) { \
        goto bad; \
      } \
    } \
  }
  
#define ACKCIADDR(opt, neg, old, val1, val2) \
  if (neg) { \
    int addrlen = (old? CILEN_ADDRS: CILEN_ADDR); \
    u32_t l; \
    if ((len -= addrlen) < 0) { \
      goto bad; \
    } \
    GETCHAR(citype, p); \
    GETCHAR(cilen, p); \
    if (cilen != addrlen || \
        citype != opt) { \
      goto bad; \
    } \
    GETLONG(l, p); \
    cilong = htonl(l); \
    if (val1 != cilong) { \
      goto bad; \
    } \
    if (old) { \
      GETLONG(l, p); \
      cilong = htonl(l); \
      if (val2 != cilong) { \
        goto bad; \
      } \
    } \
  }

#define ACKCIDNS(opt, neg, addr) \
  if (neg) { \
    u32_t l; \
    if ((len -= CILEN_ADDR) < 0) { \
      goto bad; \
    } \
    GETCHAR(citype, p); \
    GETCHAR(cilen, p); \
    if (cilen != CILEN_ADDR || \
        citype != opt) { \
      goto bad; \
    } \
    GETLONG(l, p); \
    cilong = htonl(l); \
    if (addr != cilong) { \
      goto bad; \
    } \
  }

  ACKCIADDR((go->old_addrs? CI_ADDRS: CI_ADDR), go->neg_addr,
        go->old_addrs, go->ouraddr, go->hisaddr);

  ACKCIVJ(CI_COMPRESSTYPE, go->neg_vj, go->vj_protocol, go->old_vj,
      go->maxslotindex, go->cflag);

  ACKCIDNS(CI_MS_DNS1, go->req_dns1, go->dnsaddr[0]);

  ACKCIDNS(CI_MS_DNS2, go->req_dns2, go->dnsaddr[1]);

  /*
   * If there are any remaining CIs, then this packet is bad.
   */
  if (len != 0) {
    goto bad;
  }
  return (1);

bad:
  IPCPDEBUG(LOG_INFO, ("ipcp_ackci: received bad Ack!\n"));
  return (0);
}

/*
 * ipcp_nakci - Peer has sent a NAK for some of our CIs.
 * This should not modify any state if the Nak is bad
 * or if IPCP is in the LS_OPENED state.
 *
 * Returns:
 *  0 - Nak was bad.
 *  1 - Nak was good.
 */
static int
ipcp_nakci(fsm *f, u_char *p, int len)
{
  ipcp_options *go = &ipcp_gotoptions[f->unit];
  u_char cimaxslotindex, cicflag;
  u_char citype, cilen, *next;
  u_short cishort;
  u32_t ciaddr1, ciaddr2, l, cidnsaddr;
  ipcp_options no;    /* options we've seen Naks for */
  ipcp_options try;    /* options to request next time */

  BZERO(&no, sizeof(no));
  try = *go;

  /*
   * Any Nak'd CIs must be in exactly the same order that we sent.
   * Check packet length and CI length at each step.
   * If we find any deviations, then this packet is bad.
   */
#define NAKCIADDR(opt, neg, old, code) \
  if (go->neg && \
      len >= (cilen = (old? CILEN_ADDRS: CILEN_ADDR)) && \
      p[1] == cilen && \
      p[0] == opt) { \
    len -= cilen; \
    INCPTR(2, p); \
    GETLONG(l, p); \
    ciaddr1 = htonl(l); \
    if (old) { \
      GETLONG(l, p); \
      ciaddr2 = htonl(l); \
      no.old_addrs = 1; \
    } else { \
      ciaddr2 = 0; \
    } \
    no.neg = 1; \
    code \
  }

#define NAKCIVJ(opt, neg, code) \
  if (go->neg && \
      ((cilen = p[1]) == CILEN_COMPRESS || cilen == CILEN_VJ) && \
      len >= cilen && \
      p[0] == opt) { \
    len -= cilen; \
    INCPTR(2, p); \
    GETSHORT(cishort, p); \
    no.neg = 1; \
    code \
  }
  
#define NAKCIDNS(opt, neg, code) \
  if (go->neg && \
      ((cilen = p[1]) == CILEN_ADDR) && \
      len >= cilen && \
      p[0] == opt) { \
    len -= cilen; \
    INCPTR(2, p); \
    GETLONG(l, p); \
    cidnsaddr = htonl(l); \
    no.neg = 1; \
    code \
  }

  /*
   * Accept the peer's idea of {our,his} address, if different
   * from our idea, only if the accept_{local,remote} flag is set.
   */
  NAKCIADDR((go->old_addrs? CI_ADDRS: CI_ADDR), neg_addr, go->old_addrs,
    if (go->accept_local && ciaddr1) { /* Do we know our address? */
      try.ouraddr = ciaddr1;
      IPCPDEBUG(LOG_INFO, ("local IP address %s\n",
           inet_ntoa(ciaddr1)));
    }
    if (go->accept_remote && ciaddr2) { /* Does he know his? */
      try.hisaddr = ciaddr2;
      IPCPDEBUG(LOG_INFO, ("remote IP address %s\n",
           inet_ntoa(ciaddr2)));
    }
  );

  /*
   * Accept the peer's value of maxslotindex provided that it
   * is less than what we asked for.  Turn off slot-ID compression
   * if the peer wants.  Send old-style compress-type option if
   * the peer wants.
   */
  NAKCIVJ(CI_COMPRESSTYPE, neg_vj,
    if (cilen == CILEN_VJ) {
      GETCHAR(cimaxslotindex, p);
      GETCHAR(cicflag, p);
      if (cishort == IPCP_VJ_COMP) {
        try.old_vj = 0;
        if (cimaxslotindex < go->maxslotindex) {
          try.maxslotindex = cimaxslotindex;
        }
        if (!cicflag) {
          try.cflag = 0;
        }
      } else {
        try.neg_vj = 0;
      }
    } else {
      if (cishort == IPCP_VJ_COMP || cishort == IPCP_VJ_COMP_OLD) {
        try.old_vj = 1;
        try.vj_protocol = cishort;
      } else {
        try.neg_vj = 0;
      }
    }
  );

  NAKCIDNS(CI_MS_DNS1, req_dns1,
      try.dnsaddr[0] = cidnsaddr;
        IPCPDEBUG(LOG_INFO, ("primary DNS address %s\n", inet_ntoa(cidnsaddr)));
      );

  NAKCIDNS(CI_MS_DNS2, req_dns2,
      try.dnsaddr[1] = cidnsaddr;
        IPCPDEBUG(LOG_INFO, ("secondary DNS address %s\n", inet_ntoa(cidnsaddr)));
      );

  /*
  * There may be remaining CIs, if the peer is requesting negotiation
  * on an option that we didn't include in our request packet.
  * If they want to negotiate about IP addresses, we comply.
  * If they want us to ask for compression, we refuse.
  */
  while (len > CILEN_VOID) {
    GETCHAR(citype, p);
    GETCHAR(cilen, p);
    if( (len -= cilen) < 0 ) {
      goto bad;
    }
    next = p + cilen - 2;

    switch (citype) {
      case CI_COMPRESSTYPE:
        if (go->neg_vj || no.neg_vj ||
            (cilen != CILEN_VJ && cilen != CILEN_COMPRESS)) {
          goto bad;
        }
        no.neg_vj = 1;
        break;
      case CI_ADDRS:
        if ((go->neg_addr && go->old_addrs) || no.old_addrs
            || cilen != CILEN_ADDRS) {
          goto bad;
        }
        try.neg_addr = 1;
        try.old_addrs = 1;
        GETLONG(l, p);
        ciaddr1 = htonl(l);
        if (ciaddr1 && go->accept_local) {
          try.ouraddr = ciaddr1;
        }
        GETLONG(l, p);
        ciaddr2 = htonl(l);
        if (ciaddr2 && go->accept_remote) {
          try.hisaddr = ciaddr2;
        }
        no.old_addrs = 1;
        break;
      case CI_ADDR:
        if (go->neg_addr || no.neg_addr || cilen != CILEN_ADDR) {
          goto bad;
        }
        try.old_addrs = 0;
        GETLONG(l, p);
        ciaddr1 = htonl(l);
        if (ciaddr1 && go->accept_local) {
          try.ouraddr = ciaddr1;
        }
        if (try.ouraddr != 0) {
          try.neg_addr = 1;
        }
        no.neg_addr = 1;
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
    *go = try;
  }

  return 1;

bad:
  IPCPDEBUG(LOG_INFO, ("ipcp_nakci: received bad Nak!\n"));
  return 0;
}


/*
 * ipcp_rejci - Reject some of our CIs.
 */
static int
ipcp_rejci(fsm *f, u_char *p, int len)
{
  ipcp_options *go = &ipcp_gotoptions[f->unit];
  u_char cimaxslotindex, ciflag, cilen;
  u_short cishort;
  u32_t cilong;
  ipcp_options try;    /* options to request next time */

  try = *go;
  /*
   * Any Rejected CIs must be in exactly the same order that we sent.
   * Check packet length and CI length at each step.
   * If we find any deviations, then this packet is bad.
   */
#define REJCIADDR(opt, neg, old, val1, val2) \
  if (go->neg && \
      len >= (cilen = old? CILEN_ADDRS: CILEN_ADDR) && \
      p[1] == cilen && \
      p[0] == opt) { \
    u32_t l; \
    len -= cilen; \
    INCPTR(2, p); \
    GETLONG(l, p); \
    cilong = htonl(l); \
    /* Check rejected value. */ \
    if (cilong != val1) { \
      goto bad; \
    } \
    if (old) { \
      GETLONG(l, p); \
      cilong = htonl(l); \
      /* Check rejected value. */ \
      if (cilong != val2) { \
        goto bad; \
      } \
    } \
    try.neg = 0; \
  }

#define REJCIVJ(opt, neg, val, old, maxslot, cflag) \
  if (go->neg && \
      p[1] == (old? CILEN_COMPRESS : CILEN_VJ) && \
      len >= p[1] && \
      p[0] == opt) { \
    len -= p[1]; \
    INCPTR(2, p); \
    GETSHORT(cishort, p); \
    /* Check rejected value. */  \
    if (cishort != val) { \
      goto bad; \
    } \
    if (!old) { \
      GETCHAR(cimaxslotindex, p); \
      if (cimaxslotindex != maxslot) { \
        goto bad; \
      } \
      GETCHAR(ciflag, p); \
      if (ciflag != cflag) { \
        goto bad; \
      } \
    } \
    try.neg = 0; \
  }

#define REJCIDNS(opt, neg, dnsaddr) \
  if (go->neg && \
      ((cilen = p[1]) == CILEN_ADDR) && \
      len >= cilen && \
      p[0] == opt) { \
    u32_t l; \
    len -= cilen; \
    INCPTR(2, p); \
    GETLONG(l, p); \
    cilong = htonl(l); \
    /* Check rejected value. */ \
    if (cilong != dnsaddr) { \
      goto bad; \
    } \
    try.neg = 0; \
  }

  REJCIADDR((go->old_addrs? CI_ADDRS: CI_ADDR), neg_addr,
        go->old_addrs, go->ouraddr, go->hisaddr);

  REJCIVJ(CI_COMPRESSTYPE, neg_vj, go->vj_protocol, go->old_vj,
      go->maxslotindex, go->cflag);

  REJCIDNS(CI_MS_DNS1, req_dns1, go->dnsaddr[0]);

  REJCIDNS(CI_MS_DNS2, req_dns2, go->dnsaddr[1]);

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
  IPCPDEBUG(LOG_INFO, ("ipcp_rejci: received bad Reject!\n"));
  return 0;
}


/*
 * ipcp_reqci - Check the peer's requested CIs and send appropriate response.
 *
 * Returns: CONFACK, CONFNAK or CONFREJ and input packet modified
 * appropriately.  If reject_if_disagree is non-zero, doesn't return
 * CONFNAK; returns CONFREJ if it can't return CONFACK.
 */
static int
ipcp_reqci(fsm *f, u_char *inp/* Requested CIs */,int *len/* Length of requested CIs */,int reject_if_disagree)
{
  ipcp_options *wo = &ipcp_wantoptions[f->unit];
  ipcp_options *ho = &ipcp_hisoptions[f->unit];
  ipcp_options *ao = &ipcp_allowoptions[f->unit];
#ifdef OLD_CI_ADDRS
  ipcp_options *go = &ipcp_gotoptions[f->unit];
#endif
  u_char *cip, *next;     /* Pointer to current and next CIs */
  u_short cilen, citype;  /* Parsed len, type */
  u_short cishort;        /* Parsed short value */
  u32_t tl, ciaddr1;      /* Parsed address values */
#ifdef OLD_CI_ADDRS
  u32_t ciaddr2;          /* Parsed address values */
#endif
  int rc = CONFACK;       /* Final packet return code */
  int orc;                /* Individual option return code */
  u_char *p;              /* Pointer to next char to parse */
  u_char *ucp = inp;      /* Pointer to current output char */
  int l = *len;           /* Length left */
  u_char maxslotindex, cflag;
  int d;

  cis_received[f->unit] = 1;

  /*
   * Reset all his options.
   */
  BZERO(ho, sizeof(*ho));

  /*
   * Process all his options.
   */
  next = inp;
  while (l) {
    orc = CONFACK;       /* Assume success */
    cip = p = next;      /* Remember begining of CI */
    if (l < 2 ||         /* Not enough data for CI header or */
        p[1] < 2 ||      /*  CI length too small or */
        p[1] > l) {      /*  CI length too big? */
      IPCPDEBUG(LOG_INFO, ("ipcp_reqci: bad CI length!\n"));
      orc = CONFREJ;     /* Reject bad CI */
      cilen = (u_short)l;/* Reject till end of packet */
      l = 0;             /* Don't loop again */
      goto endswitch;
    }
    GETCHAR(citype, p);  /* Parse CI type */
    GETCHAR(cilen, p);   /* Parse CI length */
    l -= cilen;          /* Adjust remaining length */
    next += cilen;       /* Step to next CI */

    switch (citype) {      /* Check CI type */
#ifdef OLD_CI_ADDRS /* Need to save space... */
      case CI_ADDRS:
        IPCPDEBUG(LOG_INFO, ("ipcp_reqci: received ADDRS\n"));
        if (!ao->neg_addr ||
            cilen != CILEN_ADDRS) {  /* Check CI length */
          orc = CONFREJ;    /* Reject CI */
          break;
        }

        /*
         * If he has no address, or if we both have his address but
         * disagree about it, then NAK it with our idea.
         * In particular, if we don't know his address, but he does,
         * then accept it.
         */
        GETLONG(tl, p);    /* Parse source address (his) */
        ciaddr1 = htonl(tl);
        IPCPDEBUG(LOG_INFO, ("his addr %s\n", inet_ntoa(ciaddr1)));
        if (ciaddr1 != wo->hisaddr
            && (ciaddr1 == 0 || !wo->accept_remote)) {
          orc = CONFNAK;
          if (!reject_if_disagree) {
            DECPTR(sizeof(u32_t), p);
            tl = ntohl(wo->hisaddr);
            PUTLONG(tl, p);
          }
        } else if (ciaddr1 == 0 && wo->hisaddr == 0) {
          /*
           * If neither we nor he knows his address, reject the option.
           */
          orc = CONFREJ;
          wo->req_addr = 0;  /* don't NAK with 0.0.0.0 later */
          break;
        }

        /*
         * If he doesn't know our address, or if we both have our address
         * but disagree about it, then NAK it with our idea.
         */
        GETLONG(tl, p);    /* Parse desination address (ours) */
        ciaddr2 = htonl(tl);
        IPCPDEBUG(LOG_INFO, ("our addr %s\n", inet_ntoa(ciaddr2)));
        if (ciaddr2 != wo->ouraddr) {
          if (ciaddr2 == 0 || !wo->accept_local) {
            orc = CONFNAK;
            if (!reject_if_disagree) {
              DECPTR(sizeof(u32_t), p);
              tl = ntohl(wo->ouraddr);
              PUTLONG(tl, p);
            }
          } else {
            go->ouraddr = ciaddr2;  /* accept peer's idea */
          }
        }

        ho->neg_addr = 1;
        ho->old_addrs = 1;
        ho->hisaddr = ciaddr1;
        ho->ouraddr = ciaddr2;
        break;
#endif

      case CI_ADDR:
        if (!ao->neg_addr) {
          IPCPDEBUG(LOG_INFO, ("ipcp_reqci: Reject ADDR not allowed\n"));
          orc = CONFREJ;        /* Reject CI */
          break;
        } else if (cilen != CILEN_ADDR) {  /* Check CI length */
          IPCPDEBUG(LOG_INFO, ("ipcp_reqci: Reject ADDR bad len\n"));
          orc = CONFREJ;        /* Reject CI */
          break;
        }

        /*
         * If he has no address, or if we both have his address but
         * disagree about it, then NAK it with our idea.
         * In particular, if we don't know his address, but he does,
         * then accept it.
         */
        GETLONG(tl, p);  /* Parse source address (his) */
        ciaddr1 = htonl(tl);
        if (ciaddr1 != wo->hisaddr
            && (ciaddr1 == 0 || !wo->accept_remote)) {
          orc = CONFNAK;
          if (!reject_if_disagree) {
            DECPTR(sizeof(u32_t), p);
            tl = ntohl(wo->hisaddr);
            PUTLONG(tl, p);
          }
          IPCPDEBUG(LOG_INFO, ("ipcp_reqci: Nak ADDR %s\n", inet_ntoa(ciaddr1)));
        } else if (ciaddr1 == 0 && wo->hisaddr == 0) {
          /*
           * Don't ACK an address of 0.0.0.0 - reject it instead.
           */
          IPCPDEBUG(LOG_INFO, ("ipcp_reqci: Reject ADDR %s\n", inet_ntoa(ciaddr1)));
          orc = CONFREJ;
          wo->req_addr = 0;  /* don't NAK with 0.0.0.0 later */
          break;
        }

        ho->neg_addr = 1;
        ho->hisaddr = ciaddr1;
        IPCPDEBUG(LOG_INFO, ("ipcp_reqci: ADDR %s\n", inet_ntoa(ciaddr1)));
        break;

      case CI_MS_DNS1:
      case CI_MS_DNS2:
        /* Microsoft primary or secondary DNS request */
        d = citype == CI_MS_DNS2;

        /* If we do not have a DNS address then we cannot send it */
        if (ao->dnsaddr[d] == 0 ||
            cilen != CILEN_ADDR) {  /* Check CI length */
          IPCPDEBUG(LOG_INFO, ("ipcp_reqci: Rejecting DNS%d Request\n", d+1));
          orc = CONFREJ;        /* Reject CI */
          break;
        }
        GETLONG(tl, p);
        if (htonl(tl) != ao->dnsaddr[d]) {
          IPCPDEBUG(LOG_INFO, ("ipcp_reqci: Naking DNS%d Request %s\n",
                d+1, inet_ntoa(tl)));
          DECPTR(sizeof(u32_t), p);
          tl = ntohl(ao->dnsaddr[d]);
          PUTLONG(tl, p);
          orc = CONFNAK;
        }
        IPCPDEBUG(LOG_INFO, ("ipcp_reqci: received DNS%d Request\n", d+1));
        break;

      case CI_MS_WINS1:
      case CI_MS_WINS2:
        /* Microsoft primary or secondary WINS request */
        d = citype == CI_MS_WINS2;
        IPCPDEBUG(LOG_INFO, ("ipcp_reqci: received WINS%d Request\n", d+1));

        /* If we do not have a DNS address then we cannot send it */
        if (ao->winsaddr[d] == 0 ||
          cilen != CILEN_ADDR) {  /* Check CI length */
          orc = CONFREJ;      /* Reject CI */
          break;
        }
        GETLONG(tl, p);
        if (htonl(tl) != ao->winsaddr[d]) {
          DECPTR(sizeof(u32_t), p);
          tl = ntohl(ao->winsaddr[d]);
          PUTLONG(tl, p);
          orc = CONFNAK;
        }
        break;

      case CI_COMPRESSTYPE:
        if (!ao->neg_vj) {
          IPCPDEBUG(LOG_INFO, ("ipcp_reqci: Rejecting COMPRESSTYPE not allowed\n"));
          orc = CONFREJ;
          break;
        } else if (cilen != CILEN_VJ && cilen != CILEN_COMPRESS) {
          IPCPDEBUG(LOG_INFO, ("ipcp_reqci: Rejecting COMPRESSTYPE len=%d\n", cilen));
          orc = CONFREJ;
          break;
        }
        GETSHORT(cishort, p);

        if (!(cishort == IPCP_VJ_COMP ||
            (cishort == IPCP_VJ_COMP_OLD && cilen == CILEN_COMPRESS))) {
          IPCPDEBUG(LOG_INFO, ("ipcp_reqci: Rejecting COMPRESSTYPE %d\n", cishort));
          orc = CONFREJ;
          break;
        }

        ho->neg_vj = 1;
        ho->vj_protocol = cishort;
        if (cilen == CILEN_VJ) {
          GETCHAR(maxslotindex, p);
          if (maxslotindex > ao->maxslotindex) { 
            IPCPDEBUG(LOG_INFO, ("ipcp_reqci: Naking VJ max slot %d\n", maxslotindex));
            orc = CONFNAK;
            if (!reject_if_disagree) {
              DECPTR(1, p);
              PUTCHAR(ao->maxslotindex, p);
            }
          }
          GETCHAR(cflag, p);
          if (cflag && !ao->cflag) {
            IPCPDEBUG(LOG_INFO, ("ipcp_reqci: Naking VJ cflag %d\n", cflag));
            orc = CONFNAK;
            if (!reject_if_disagree) {
              DECPTR(1, p);
              PUTCHAR(wo->cflag, p);
            }
          }
          ho->maxslotindex = maxslotindex;
          ho->cflag = cflag;
        } else {
          ho->old_vj = 1;
          ho->maxslotindex = MAX_SLOTS - 1;
          ho->cflag = 1;
        }
        IPCPDEBUG(LOG_INFO, (
              "ipcp_reqci: received COMPRESSTYPE p=%d old=%d maxslot=%d cflag=%d\n",
              ho->vj_protocol, ho->old_vj, ho->maxslotindex, ho->cflag));
        break;

      default:
        IPCPDEBUG(LOG_INFO, ("ipcp_reqci: Rejecting unknown CI type %d\n", citype));
        orc = CONFREJ;
        break;
    }

endswitch:
    if (orc == CONFACK &&    /* Good CI */
        rc != CONFACK) {     /*  but prior CI wasnt? */
      continue;              /* Don't send this one */
    }

    if (orc == CONFNAK) {    /* Nak this CI? */
      if (reject_if_disagree) {  /* Getting fed up with sending NAKs? */
        IPCPDEBUG(LOG_INFO, ("ipcp_reqci: Rejecting too many naks\n"));
        orc = CONFREJ;       /* Get tough if so */
      } else {
        if (rc == CONFREJ) { /* Rejecting prior CI? */
          continue;          /* Don't send this one */
        }
        if (rc == CONFACK) { /* Ack'd all prior CIs? */
          rc = CONFNAK;      /* Not anymore... */
          ucp = inp;         /* Backup */
        }
      }
    }

    if (orc == CONFREJ &&    /* Reject this CI */
        rc != CONFREJ) {  /*  but no prior ones? */
      rc = CONFREJ;
      ucp = inp;        /* Backup */
    }
    
    /* Need to move CI? */
    if (ucp != cip) {
      BCOPY(cip, ucp, cilen);  /* Move it */
    }

    /* Update output pointer */
    INCPTR(cilen, ucp);
  }

  /*
   * If we aren't rejecting this packet, and we want to negotiate
   * their address, and they didn't send their address, then we
   * send a NAK with a CI_ADDR option appended.  We assume the
   * input buffer is long enough that we can append the extra
   * option safely.
   */
  if (rc != CONFREJ && !ho->neg_addr &&
      wo->req_addr && !reject_if_disagree) {
    IPCPDEBUG(LOG_INFO, ("ipcp_reqci: Requesting peer address\n"));
    if (rc == CONFACK) {
      rc = CONFNAK;
      ucp = inp;        /* reset pointer */
      wo->req_addr = 0;    /* don't ask again */
    }
    PUTCHAR(CI_ADDR, ucp);
    PUTCHAR(CILEN_ADDR, ucp);
    tl = ntohl(wo->hisaddr);
    PUTLONG(tl, ucp);
  }

  *len = (int)(ucp - inp);    /* Compute output length */
  IPCPDEBUG(LOG_INFO, ("ipcp_reqci: returning Configure-%s\n", CODENAME(rc)));
  return (rc);      /* Return final code */
}


#if 0
/*
 * ip_check_options - check that any IP-related options are OK,
 * and assign appropriate defaults.
 */
static void
ip_check_options(u_long localAddr)
{
  ipcp_options *wo = &ipcp_wantoptions[0];

  /*
   * Load our default IP address but allow the remote host to give us
   * a new address.
   */
  if (wo->ouraddr == 0 && !ppp_settings.disable_defaultip) {
    wo->accept_local = 1;  /* don't insist on this default value */
    wo->ouraddr = htonl(localAddr);
  }
}
#endif


/*
 * ipcp_up - IPCP has come UP.
 *
 * Configure the IP network interface appropriately and bring it up.
 */
static void
ipcp_up(fsm *f)
{
  u32_t mask;
  ipcp_options *ho = &ipcp_hisoptions[f->unit];
  ipcp_options *go = &ipcp_gotoptions[f->unit];
  ipcp_options *wo = &ipcp_wantoptions[f->unit];

  np_up(f->unit, PPP_IP);
  IPCPDEBUG(LOG_INFO, ("ipcp: up\n"));

  /*
   * We must have a non-zero IP address for both ends of the link.
   */
  if (!ho->neg_addr) {
    ho->hisaddr = wo->hisaddr;
  }

  if (ho->hisaddr == 0) {
    IPCPDEBUG(LOG_ERR, ("Could not determine remote IP address\n"));
    ipcp_close(f->unit, "Could not determine remote IP address");
    return;
  }
  if (go->ouraddr == 0) {
    IPCPDEBUG(LOG_ERR, ("Could not determine local IP address\n"));
    ipcp_close(f->unit, "Could not determine local IP address");
    return;
  }

  if (ppp_settings.usepeerdns && (go->dnsaddr[0] || go->dnsaddr[1])) {
    /*pppGotDNSAddrs(go->dnsaddr[0], go->dnsaddr[1]);*/
  }

  /*
   * Check that the peer is allowed to use the IP address it wants.
   */
  if (!auth_ip_addr(f->unit, ho->hisaddr)) {
    IPCPDEBUG(LOG_ERR, ("Peer is not authorized to use remote address %s\n",
        inet_ntoa(ho->hisaddr)));
    ipcp_close(f->unit, "Unauthorized remote IP address");
    return;
  }

  /* set tcp compression */
  sifvjcomp(f->unit, ho->neg_vj, ho->cflag, ho->maxslotindex);

  /*
   * Set IP addresses and (if specified) netmask.
   */
  mask = GetMask(go->ouraddr);

  if (!sifaddr(f->unit, go->ouraddr, ho->hisaddr, mask, go->dnsaddr[0], go->dnsaddr[1])) {
    IPCPDEBUG(LOG_WARNING, ("sifaddr failed\n"));
    ipcp_close(f->unit, "Interface configuration failed");
    return;
  }

  /* bring the interface up for IP */
  if (!sifup(f->unit)) {
    IPCPDEBUG(LOG_WARNING, ("sifup failed\n"));
    ipcp_close(f->unit, "Interface configuration failed");
    return;
  }

  sifnpmode(f->unit, PPP_IP, NPMODE_PASS);

  /* assign a default route through the interface if required */
  if (ipcp_wantoptions[f->unit].default_route) {
    if (sifdefaultroute(f->unit, go->ouraddr, ho->hisaddr)) {
      default_route_set[f->unit] = 1;
    }
  }

  IPCPDEBUG(LOG_NOTICE, ("local  IP address %s\n", inet_ntoa(go->ouraddr)));
  IPCPDEBUG(LOG_NOTICE, ("remote IP address %s\n", inet_ntoa(ho->hisaddr)));
  if (go->dnsaddr[0]) {
    IPCPDEBUG(LOG_NOTICE, ("primary   DNS address %s\n", inet_ntoa(go->dnsaddr[0])));
  }
  if (go->dnsaddr[1]) {
    IPCPDEBUG(LOG_NOTICE, ("secondary DNS address %s\n", inet_ntoa(go->dnsaddr[1])));
  }
}


/*
 * ipcp_down - IPCP has gone DOWN.
 *
 * Take the IP network interface down, clear its addresses
 * and delete routes through it.
 */
static void
ipcp_down(fsm *f)
{
  IPCPDEBUG(LOG_INFO, ("ipcp: down\n"));
  np_down(f->unit, PPP_IP);
  sifvjcomp(f->unit, 0, 0, 0);

  sifdown(f->unit);
  ipcp_clear_addrs(f->unit);
}


/*
 * ipcp_clear_addrs() - clear the interface addresses, routes, etc.
 */
static void
ipcp_clear_addrs(int unit)
{
  u32_t ouraddr, hisaddr;

  ouraddr = ipcp_gotoptions[unit].ouraddr;
  hisaddr = ipcp_hisoptions[unit].hisaddr;
  if (default_route_set[unit]) {
    cifdefaultroute(unit, ouraddr, hisaddr);
    default_route_set[unit] = 0;
  }
  cifaddr(unit, ouraddr, hisaddr);
}


/*
 * ipcp_finished - possibly shut down the lower layers.
 */
static void
ipcp_finished(fsm *f)
{
  np_finished(f->unit, PPP_IP);
}

#if PPP_ADDITIONAL_CALLBACKS
static int
ipcp_printpkt(u_char *p, int plen, void (*printer) (void *, char *, ...), void *arg)
{
  LWIP_UNUSED_ARG(p);
  LWIP_UNUSED_ARG(plen);
  LWIP_UNUSED_ARG(printer);
  LWIP_UNUSED_ARG(arg);
  return 0;
}

/*
 * ip_active_pkt - see if this IP packet is worth bringing the link up for.
 * We don't bring the link up for IP fragments or for TCP FIN packets
 * with no data.
 */
#define IP_HDRLEN   20  /* bytes */
#define IP_OFFMASK  0x1fff
#define IPPROTO_TCP 6
#define TCP_HDRLEN  20
#define TH_FIN      0x01

/*
 * We use these macros because the IP header may be at an odd address,
 * and some compilers might use word loads to get th_off or ip_hl.
 */

#define net_short(x)    (((x)[0] << 8) + (x)[1])
#define get_iphl(x)     (((unsigned char *)(x))[0] & 0xF)
#define get_ipoff(x)    net_short((unsigned char *)(x) + 6)
#define get_ipproto(x)  (((unsigned char *)(x))[9])
#define get_tcpoff(x)   (((unsigned char *)(x))[12] >> 4)
#define get_tcpflags(x) (((unsigned char *)(x))[13])

static int
ip_active_pkt(u_char *pkt, int len)
{
  u_char *tcp;
  int hlen;

  len -= PPP_HDRLEN;
  pkt += PPP_HDRLEN;
  if (len < IP_HDRLEN) {
    return 0;
  }
  if ((get_ipoff(pkt) & IP_OFFMASK) != 0) {
    return 0;
  }
  if (get_ipproto(pkt) != IPPROTO_TCP) {
    return 1;
  }
  hlen = get_iphl(pkt) * 4;
  if (len < hlen + TCP_HDRLEN) {
    return 0;
  }
  tcp = pkt + hlen;
  if ((get_tcpflags(tcp) & TH_FIN) != 0 && len == hlen + get_tcpoff(tcp) * 4) {
    return 0;
  }
  return 1;
}
#endif /* PPP_ADDITIONAL_CALLBACKS */

#endif /* PPP_SUPPORT */
