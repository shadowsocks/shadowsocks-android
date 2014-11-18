/*****************************************************************************
* ppp.c - Network Point to Point Protocol program file.
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
* 97-11-05 Guy Lancaster <lancasterg@acm.org>, Global Election Systems Inc.
*   Original.
*****************************************************************************/

/*
 * ppp_defs.h - PPP definitions.
 *
 * if_pppvar.h - private structures and declarations for PPP.
 *
 * Copyright (c) 1994 The Australian National University.
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, provided that the above copyright
 * notice appears in all copies.  This software is provided without any
 * warranty, express or implied. The Australian National University
 * makes no representations about the suitability of this software for
 * any purpose.
 *
 * IN NO EVENT SHALL THE AUSTRALIAN NATIONAL UNIVERSITY BE LIABLE TO ANY
 * PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF
 * THE AUSTRALIAN NATIONAL UNIVERSITY HAVE BEEN ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * THE AUSTRALIAN NATIONAL UNIVERSITY SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE AUSTRALIAN NATIONAL UNIVERSITY HAS NO
 * OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS,
 * OR MODIFICATIONS.
 */

/*
 * if_ppp.h - Point-to-Point Protocol definitions.
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
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "lwip/opt.h"

#if PPP_SUPPORT /* don't build if not configured for use in lwipopts.h */

#include "ppp_impl.h"
#include "lwip/ip.h" /* for ip_input() */

#include "pppdebug.h"

#include "randm.h"
#include "fsm.h"
#if PAP_SUPPORT
#include "pap.h"
#endif /* PAP_SUPPORT */
#if CHAP_SUPPORT
#include "chap.h"
#endif /* CHAP_SUPPORT */
#include "ipcp.h"
#include "lcp.h"
#include "magic.h"
#include "auth.h"
#if VJ_SUPPORT
#include "vj.h"
#endif /* VJ_SUPPORT */
#if PPPOE_SUPPORT
#include "netif/ppp_oe.h"
#endif /* PPPOE_SUPPORT */

#include "lwip/tcpip.h"
#include "lwip/api.h"
#include "lwip/snmp.h"

#include <string.h>

/*************************/
/*** LOCAL DEFINITIONS ***/
/*************************/

/** PPP_INPROC_MULTITHREADED==1 call pppInput using tcpip_callback().
 * Set this to 0 if pppInProc is called inside tcpip_thread or with NO_SYS==1.
 * Default is 1 for NO_SYS==0 (multithreaded) and 0 for NO_SYS==1 (single-threaded).
 */
#ifndef PPP_INPROC_MULTITHREADED
#define PPP_INPROC_MULTITHREADED (NO_SYS==0)
#endif

/** PPP_INPROC_OWNTHREAD==1: start a dedicated RX thread per PPP session.
 * Default is 0: call pppos_input() for received raw characters, charcater
 * reception is up to the port */
#ifndef PPP_INPROC_OWNTHREAD
#define PPP_INPROC_OWNTHREAD      PPP_INPROC_MULTITHREADED
#endif

#if PPP_INPROC_OWNTHREAD && !PPP_INPROC_MULTITHREADED
  #error "PPP_INPROC_OWNTHREAD needs PPP_INPROC_MULTITHREADED==1"
#endif

/*
 * The basic PPP frame.
 */
#define PPP_ADDRESS(p)  (((u_char *)(p))[0])
#define PPP_CONTROL(p)  (((u_char *)(p))[1])
#define PPP_PROTOCOL(p) ((((u_char *)(p))[2] << 8) + ((u_char *)(p))[3])

/* PPP packet parser states.  Current state indicates operation yet to be
 * completed. */
typedef enum {
  PDIDLE = 0,  /* Idle state - waiting. */
  PDSTART,     /* Process start flag. */
  PDADDRESS,   /* Process address field. */
  PDCONTROL,   /* Process control field. */
  PDPROTOCOL1, /* Process protocol field 1. */
  PDPROTOCOL2, /* Process protocol field 2. */
  PDDATA       /* Process data byte. */
} PPPDevStates;

#define ESCAPE_P(accm, c) ((accm)[(c) >> 3] & pppACCMMask[c & 0x07])

/************************/
/*** LOCAL DATA TYPES ***/
/************************/

/** RX buffer size: this may be configured smaller! */
#ifndef PPPOS_RX_BUFSIZE
#define PPPOS_RX_BUFSIZE    (PPP_MRU + PPP_HDRLEN)
#endif

typedef struct PPPControlRx_s {
  /** unit number / ppp descriptor */
  int pd;
  /** the rx file descriptor */
  sio_fd_t fd;
  /** receive buffer - encoded data is stored here */
#if PPP_INPROC_OWNTHREAD
  u_char rxbuf[PPPOS_RX_BUFSIZE];
#endif /* PPP_INPROC_OWNTHREAD */

  /* The input packet. */
  struct pbuf *inHead, *inTail;

#if PPPOS_SUPPORT
  u16_t inProtocol;             /* The input protocol code. */
  u16_t inFCS;                  /* Input Frame Check Sequence value. */
#endif /* PPPOS_SUPPORT */
  PPPDevStates inState;         /* The input process state. */
  char inEscaped;               /* Escape next character. */
  ext_accm inACCM;              /* Async-Ctl-Char-Map for input. */
} PPPControlRx;

/*
 * PPP interface control block.
 */
typedef struct PPPControl_s {
  PPPControlRx rx;
  char openFlag;                /* True when in use. */
#if PPPOE_SUPPORT
  struct netif *ethif;
  struct pppoe_softc *pppoe_sc;
#endif /* PPPOE_SUPPORT */
  int  if_up;                   /* True when the interface is up. */
  int  errCode;                 /* Code indicating why interface is down. */
#if PPPOS_SUPPORT
  sio_fd_t fd;                  /* File device ID of port. */
#endif /* PPPOS_SUPPORT */
  u16_t mtu;                    /* Peer's mru */
  int  pcomp;                   /* Does peer accept protocol compression? */
  int  accomp;                  /* Does peer accept addr/ctl compression? */
  u_long lastXMit;              /* Time of last transmission. */
  ext_accm outACCM;             /* Async-Ctl-Char-Map for output. */
#if PPPOS_SUPPORT && VJ_SUPPORT
  int  vjEnabled;               /* Flag indicating VJ compression enabled. */
  struct vjcompress vjComp;     /* Van Jacobson compression header. */
#endif /* PPPOS_SUPPORT && VJ_SUPPORT */

  struct netif netif;

  struct ppp_addrs addrs;

  void (*linkStatusCB)(void *ctx, int errCode, void *arg);
  void *linkStatusCtx;

} PPPControl;


/*
 * Ioctl definitions.
 */

struct npioctl {
  int         protocol; /* PPP procotol, e.g. PPP_IP */
  enum NPmode mode;
};



/***********************************/
/*** LOCAL FUNCTION DECLARATIONS ***/
/***********************************/
#if PPPOS_SUPPORT
#if PPP_INPROC_OWNTHREAD
static void pppInputThread(void *arg);
#endif /* PPP_INPROC_OWNTHREAD */
static void pppDrop(PPPControlRx *pcrx);
static void pppInProc(PPPControlRx *pcrx, u_char *s, int l);
static void pppFreeCurrentInputPacket(PPPControlRx *pcrx);
#endif /* PPPOS_SUPPORT */


/******************************/
/*** PUBLIC DATA STRUCTURES ***/
/******************************/
static PPPControl pppControl[NUM_PPP]; /* The PPP interface control blocks. */

/*
 * PPP Data Link Layer "protocol" table.
 * One entry per supported protocol.
 * The last entry must be NULL.
 */
struct protent *ppp_protocols[] = {
  &lcp_protent,
#if PAP_SUPPORT
  &pap_protent,
#endif /* PAP_SUPPORT */
#if CHAP_SUPPORT
  &chap_protent,
#endif /* CHAP_SUPPORT */
#if CBCP_SUPPORT
  &cbcp_protent,
#endif /* CBCP_SUPPORT */
  &ipcp_protent,
#if CCP_SUPPORT
  &ccp_protent,
#endif /* CCP_SUPPORT */
  NULL
};


/*
 * Buffers for outgoing packets.  This must be accessed only from the appropriate
 * PPP task so that it doesn't need to be protected to avoid collisions.
 */
u_char outpacket_buf[NUM_PPP][PPP_MRU+PPP_HDRLEN];


/*****************************/
/*** LOCAL DATA STRUCTURES ***/
/*****************************/

#if PPPOS_SUPPORT
/*
 * FCS lookup table as calculated by genfcstab.
 * @todo: smaller, slower implementation for lower memory footprint?
 */
static const u_short fcstab[256] = {
  0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
  0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
  0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
  0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
  0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
  0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
  0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
  0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
  0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
  0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
  0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
  0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
  0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
  0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
  0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
  0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
  0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
  0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
  0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
  0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
  0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
  0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
  0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
  0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
  0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
  0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
  0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
  0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
  0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
  0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
  0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
  0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

/* PPP's Asynchronous-Control-Character-Map.  The mask array is used
 * to select the specific bit for a character. */
static u_char pppACCMMask[] = {
  0x01,
  0x02,
  0x04,
  0x08,
  0x10,
  0x20,
  0x40,
  0x80
};

#if PPP_INPROC_OWNTHREAD
/** Wake up the task blocked in reading from serial line (if any) */
static void
pppRecvWakeup(int pd)
{
  PPPDEBUG(LOG_DEBUG, ("pppRecvWakeup: unit %d\n", pd));
  if (pppControl[pd].openFlag != 0) {
    sio_read_abort(pppControl[pd].fd);
  }
}
#endif /* PPP_INPROC_OWNTHREAD */
#endif /* PPPOS_SUPPORT */

void
pppLinkTerminated(int pd)
{
  PPPDEBUG(LOG_DEBUG, ("pppLinkTerminated: unit %d\n", pd));

#if PPPOE_SUPPORT
  if (pppControl[pd].ethif) {
    pppoe_disconnect(pppControl[pd].pppoe_sc);
  } else
#endif /* PPPOE_SUPPORT */
  {
#if PPPOS_SUPPORT
    PPPControl* pc;
#if PPP_INPROC_OWNTHREAD
    pppRecvWakeup(pd);
#endif /* PPP_INPROC_OWNTHREAD */
    pc = &pppControl[pd];

    PPPDEBUG(LOG_DEBUG, ("pppLinkTerminated: unit %d: linkStatusCB=%p errCode=%d\n", pd, pc->linkStatusCB, pc->errCode));
    if (pc->linkStatusCB) {
      pc->linkStatusCB(pc->linkStatusCtx, pc->errCode ? pc->errCode : PPPERR_PROTOCOL, NULL);
    }

    pc->openFlag = 0;/**/
#endif /* PPPOS_SUPPORT */
  }
  PPPDEBUG(LOG_DEBUG, ("pppLinkTerminated: finished.\n"));
}

void
pppLinkDown(int pd)
{
  PPPDEBUG(LOG_DEBUG, ("pppLinkDown: unit %d\n", pd));

#if PPPOE_SUPPORT
  if (pppControl[pd].ethif) {
    pppoe_disconnect(pppControl[pd].pppoe_sc);
  } else
#endif /* PPPOE_SUPPORT */
  {
#if PPPOS_SUPPORT && PPP_INPROC_OWNTHREAD
    pppRecvWakeup(pd);
#endif /* PPPOS_SUPPORT && PPP_INPROC_OWNTHREAD*/
  }
}

/** Initiate LCP open request */
static void
pppStart(int pd)
{
  PPPDEBUG(LOG_DEBUG, ("pppStart: unit %d\n", pd));
  lcp_lowerup(pd);
  lcp_open(pd); /* Start protocol */
  PPPDEBUG(LOG_DEBUG, ("pppStart: finished\n"));
}

/** LCP close request */
static void
pppStop(int pd)
{
  PPPDEBUG(LOG_DEBUG, ("pppStop: unit %d\n", pd));
  lcp_close(pd, "User request");
}

/** Called when carrier/link is lost */
static void
pppHup(int pd)
{
  PPPDEBUG(LOG_DEBUG, ("pppHupCB: unit %d\n", pd));
  lcp_lowerdown(pd);
  link_terminated(pd);
}

/***********************************/
/*** PUBLIC FUNCTION DEFINITIONS ***/
/***********************************/
/* Initialize the PPP subsystem. */

struct ppp_settings ppp_settings;

void
pppInit(void)
{
  struct protent *protp;
  int i, j;

  memset(&ppp_settings, 0, sizeof(ppp_settings));
  ppp_settings.usepeerdns = 1;
  pppSetAuth(PPPAUTHTYPE_NONE, NULL, NULL);

  magicInit();

  for (i = 0; i < NUM_PPP; i++) {
    /* Initialize each protocol to the standard option set. */
    for (j = 0; (protp = ppp_protocols[j]) != NULL; ++j) {
      (*protp->init)(i);
    }
  }
}

void
pppSetAuth(enum pppAuthType authType, const char *user, const char *passwd)
{
  switch(authType) {
    case PPPAUTHTYPE_NONE:
    default:
#ifdef LWIP_PPP_STRICT_PAP_REJECT
      ppp_settings.refuse_pap = 1;
#else  /* LWIP_PPP_STRICT_PAP_REJECT */
      /* some providers request pap and accept an empty login/pw */
      ppp_settings.refuse_pap = 0;
#endif /* LWIP_PPP_STRICT_PAP_REJECT */
      ppp_settings.refuse_chap = 1;
      break;

    case PPPAUTHTYPE_ANY:
      /* Warning: Using PPPAUTHTYPE_ANY might have security consequences.
       * RFC 1994 says:
       *
       * In practice, within or associated with each PPP server, there is a
       * database which associates "user" names with authentication
       * information ("secrets").  It is not anticipated that a particular
       * named user would be authenticated by multiple methods.  This would
       * make the user vulnerable to attacks which negotiate the least secure
       * method from among a set (such as PAP rather than CHAP).  If the same
       * secret was used, PAP would reveal the secret to be used later with
       * CHAP.
       *
       * Instead, for each user name there should be an indication of exactly
       * one method used to authenticate that user name.  If a user needs to
       * make use of different authentication methods under different
       * circumstances, then distinct user names SHOULD be employed, each of
       * which identifies exactly one authentication method.
       *
       */
      ppp_settings.refuse_pap = 0;
      ppp_settings.refuse_chap = 0;
      break;

    case PPPAUTHTYPE_PAP:
      ppp_settings.refuse_pap = 0;
      ppp_settings.refuse_chap = 1;
      break;

    case PPPAUTHTYPE_CHAP:
      ppp_settings.refuse_pap = 1;
      ppp_settings.refuse_chap = 0;
      break;
  }

  if(user) {
    strncpy(ppp_settings.user, user, sizeof(ppp_settings.user)-1);
    ppp_settings.user[sizeof(ppp_settings.user)-1] = '\0';
  } else {
    ppp_settings.user[0] = '\0';
  }

  if(passwd) {
    strncpy(ppp_settings.passwd, passwd, sizeof(ppp_settings.passwd)-1);
    ppp_settings.passwd[sizeof(ppp_settings.passwd)-1] = '\0';
  } else {
    ppp_settings.passwd[0] = '\0';
  }
}

#if PPPOS_SUPPORT
/** Open a new PPP connection using the given I/O device.
 * This initializes the PPP control block but does not
 * attempt to negotiate the LCP session.  If this port
 * connects to a modem, the modem connection must be
 * established before calling this.
 * Return a new PPP connection descriptor on success or
 * an error code (negative) on failure.
 *
 * pppOpen() is directly defined to this function.
 */
int
pppOverSerialOpen(sio_fd_t fd, pppLinkStatusCB_fn linkStatusCB, void *linkStatusCtx)
{
  PPPControl *pc;
  int pd;

  if (linkStatusCB == NULL) {
    /* PPP is single-threaded: without a callback,
     * there is no way to know when the link is up. */
    return PPPERR_PARAM;
  }

  /* Find a free PPP session descriptor. */
  for (pd = 0; pd < NUM_PPP && pppControl[pd].openFlag != 0; pd++);

  if (pd >= NUM_PPP) {
    pd = PPPERR_OPEN;
  } else {
    pc = &pppControl[pd];
    /* input pbuf left over from last session? */
    pppFreeCurrentInputPacket(&pc->rx);
    /* @todo: is this correct or do I overwrite something? */
    memset(pc, 0, sizeof(PPPControl));
    pc->rx.pd = pd;
    pc->rx.fd = fd;

    pc->openFlag = 1;
    pc->fd = fd;

#if VJ_SUPPORT
    vj_compress_init(&pc->vjComp);
#endif /* VJ_SUPPORT */

    /* 
     * Default the in and out accm so that escape and flag characters
     * are always escaped. 
     */
    pc->rx.inACCM[15] = 0x60; /* no need to protect since RX is not running */
    pc->outACCM[15] = 0x60;

    pc->linkStatusCB = linkStatusCB;
    pc->linkStatusCtx = linkStatusCtx;

    /*
     * Start the connection and handle incoming events (packet or timeout).
     */
    PPPDEBUG(LOG_INFO, ("pppOverSerialOpen: unit %d: Connecting\n", pd));
    pppStart(pd);
#if PPP_INPROC_OWNTHREAD
    sys_thread_new(PPP_THREAD_NAME, pppInputThread, (void*)&pc->rx, PPP_THREAD_STACKSIZE, PPP_THREAD_PRIO);
#endif /* PPP_INPROC_OWNTHREAD */
  }

  return pd;
}
#endif /* PPPOS_SUPPORT */

#if PPPOE_SUPPORT
static void pppOverEthernetLinkStatusCB(int pd, int up);

void
pppOverEthernetClose(int pd)
{
  PPPControl* pc = &pppControl[pd];

  /* *TJL* There's no lcp_deinit */
  lcp_close(pd, NULL);

  pppoe_destroy(&pc->netif);
}

int pppOverEthernetOpen(struct netif *ethif, const char *service_name, const char *concentrator_name,
                        pppLinkStatusCB_fn linkStatusCB, void *linkStatusCtx)
{
  PPPControl *pc;
  int pd;

  LWIP_UNUSED_ARG(service_name);
  LWIP_UNUSED_ARG(concentrator_name);

  if (linkStatusCB == NULL) {
    /* PPP is single-threaded: without a callback,
     * there is no way to know when the link is up. */
    return PPPERR_PARAM;
  }

  /* Find a free PPP session descriptor. Critical region? */
  for (pd = 0; pd < NUM_PPP && pppControl[pd].openFlag != 0; pd++);
  if (pd >= NUM_PPP) {
    pd = PPPERR_OPEN;
  } else {
    pc = &pppControl[pd];
    memset(pc, 0, sizeof(PPPControl));
    pc->openFlag = 1;
    pc->ethif = ethif;

    pc->linkStatusCB  = linkStatusCB;
    pc->linkStatusCtx = linkStatusCtx;

    lcp_wantoptions[pd].mru = PPPOE_MAXMTU;
    lcp_wantoptions[pd].neg_asyncmap = 0;
    lcp_wantoptions[pd].neg_pcompression = 0;
    lcp_wantoptions[pd].neg_accompression = 0;

    lcp_allowoptions[pd].mru = PPPOE_MAXMTU;
    lcp_allowoptions[pd].neg_asyncmap = 0;
    lcp_allowoptions[pd].neg_pcompression = 0;
    lcp_allowoptions[pd].neg_accompression = 0;

    if(pppoe_create(ethif, pd, pppOverEthernetLinkStatusCB, &pc->pppoe_sc) != ERR_OK) {
      pc->openFlag = 0;
      return PPPERR_OPEN;
    }

    pppoe_connect(pc->pppoe_sc);
  }

  return pd;
}
#endif /* PPPOE_SUPPORT */


/* Close a PPP connection and release the descriptor. 
 * Any outstanding packets in the queues are dropped.
 * Return 0 on success, an error code on failure. */
int
pppClose(int pd)
{
  PPPControl *pc = &pppControl[pd];
  int st = 0;

  PPPDEBUG(LOG_DEBUG, ("pppClose() called\n"));

  /* Disconnect */
#if PPPOE_SUPPORT
  if(pc->ethif) {
    PPPDEBUG(LOG_DEBUG, ("pppClose: unit %d kill_link -> pppStop\n", pd));
    pc->errCode = PPPERR_USER;
    /* This will leave us at PHASE_DEAD. */
    pppStop(pd);
  } else
#endif /* PPPOE_SUPPORT */
  {
#if PPPOS_SUPPORT
    PPPDEBUG(LOG_DEBUG, ("pppClose: unit %d kill_link -> pppStop\n", pd));
    pc->errCode = PPPERR_USER;
    /* This will leave us at PHASE_DEAD. */
    pppStop(pd);
#if PPP_INPROC_OWNTHREAD
    pppRecvWakeup(pd);
#endif /* PPP_INPROC_OWNTHREAD */
#endif /* PPPOS_SUPPORT */
  }

  return st;
}

/* This function is called when carrier is lost on the PPP channel. */
void
pppSigHUP(int pd)
{
  PPPDEBUG(LOG_DEBUG, ("pppSigHUP: unit %d sig_hup -> pppHupCB\n", pd));
  pppHup(pd);
}

#if PPPOS_SUPPORT
static void
nPut(PPPControl *pc, struct pbuf *nb)
{
  struct pbuf *b;
  int c;

  for(b = nb; b != NULL; b = b->next) {
    if((c = sio_write(pc->fd, b->payload, b->len)) != b->len) {
      PPPDEBUG(LOG_WARNING,
               ("PPP nPut: incomplete sio_write(fd:%"SZT_F", len:%d, c: 0x%"X8_F") c = %d\n", (size_t)pc->fd, b->len, c, c));
      LINK_STATS_INC(link.err);
      pc->lastXMit = 0; /* prepend PPP_FLAG to next packet */
      snmp_inc_ifoutdiscards(&pc->netif);
      pbuf_free(nb);
      return;
    }
  }

  snmp_add_ifoutoctets(&pc->netif, nb->tot_len);
  snmp_inc_ifoutucastpkts(&pc->netif);
  pbuf_free(nb);
  LINK_STATS_INC(link.xmit);
}

/* 
 * pppAppend - append given character to end of given pbuf.  If outACCM
 * is not NULL and the character needs to be escaped, do so.
 * If pbuf is full, append another.
 * Return the current pbuf.
 */
static struct pbuf *
pppAppend(u_char c, struct pbuf *nb, ext_accm *outACCM)
{
  struct pbuf *tb = nb;
  
  /* Make sure there is room for the character and an escape code.
   * Sure we don't quite fill the buffer if the character doesn't
   * get escaped but is one character worth complicating this? */
  /* Note: We assume no packet header. */
  if (nb && (PBUF_POOL_BUFSIZE - nb->len) < 2) {
    tb = pbuf_alloc(PBUF_RAW, 0, PBUF_POOL);
    if (tb) {
      nb->next = tb;
    } else {
      LINK_STATS_INC(link.memerr);
    }
    nb = tb;
  }

  if (nb) {
    if (outACCM && ESCAPE_P(*outACCM, c)) {
      *((u_char*)nb->payload + nb->len++) = PPP_ESCAPE;
      *((u_char*)nb->payload + nb->len++) = c ^ PPP_TRANS;
    } else {
      *((u_char*)nb->payload + nb->len++) = c;
    }
  }

  return tb;
}
#endif /* PPPOS_SUPPORT */

#if PPPOE_SUPPORT
static err_t
pppifOutputOverEthernet(int pd, struct pbuf *p)
{
  PPPControl *pc = &pppControl[pd];
  struct pbuf *pb;
  u_short protocol = PPP_IP;
  int i=0;
  u16_t tot_len;

  /* @todo: try to use pbuf_header() here! */
  pb = pbuf_alloc(PBUF_LINK, PPPOE_HDRLEN + sizeof(protocol), PBUF_RAM);
  if(!pb) {
    LINK_STATS_INC(link.memerr);
    LINK_STATS_INC(link.proterr);
    snmp_inc_ifoutdiscards(&pc->netif);
    return ERR_MEM;
  }

  pbuf_header(pb, -(s16_t)PPPOE_HDRLEN);

  pc->lastXMit = sys_jiffies();

  if (!pc->pcomp || protocol > 0xFF) {
    *((u_char*)pb->payload + i++) = (protocol >> 8) & 0xFF;
  }
  *((u_char*)pb->payload + i) = protocol & 0xFF;

  pbuf_chain(pb, p);
  tot_len = pb->tot_len;

  if(pppoe_xmit(pc->pppoe_sc, pb) != ERR_OK) {
    LINK_STATS_INC(link.err);
    snmp_inc_ifoutdiscards(&pc->netif);
    return PPPERR_DEVICE;
  }

  snmp_add_ifoutoctets(&pc->netif, tot_len);
  snmp_inc_ifoutucastpkts(&pc->netif);
  LINK_STATS_INC(link.xmit);
  return ERR_OK;
}
#endif /* PPPOE_SUPPORT */

/* Send a packet on the given connection. */
static err_t
pppifOutput(struct netif *netif, struct pbuf *pb, ip_addr_t *ipaddr)
{
  int pd = (int)(size_t)netif->state;
  PPPControl *pc = &pppControl[pd];
#if PPPOS_SUPPORT
  u_short protocol = PPP_IP;
  u_int fcsOut = PPP_INITFCS;
  struct pbuf *headMB = NULL, *tailMB = NULL, *p;
  u_char c;
#endif /* PPPOS_SUPPORT */

  LWIP_UNUSED_ARG(ipaddr);

  /* Validate parameters. */
  /* We let any protocol value go through - it can't hurt us
   * and the peer will just drop it if it's not accepting it. */
  if (pd < 0 || pd >= NUM_PPP || !pc->openFlag || !pb) {
    PPPDEBUG(LOG_WARNING, ("pppifOutput[%d]: bad parms prot=%d pb=%p\n",
              pd, PPP_IP, pb));
    LINK_STATS_INC(link.opterr);
    LINK_STATS_INC(link.drop);
    snmp_inc_ifoutdiscards(netif);
    return ERR_ARG;
  }

  /* Check that the link is up. */
  if (lcp_phase[pd] == PHASE_DEAD) {
    PPPDEBUG(LOG_ERR, ("pppifOutput[%d]: link not up\n", pd));
    LINK_STATS_INC(link.rterr);
    LINK_STATS_INC(link.drop);
    snmp_inc_ifoutdiscards(netif);
    return ERR_RTE;
  }

#if PPPOE_SUPPORT
  if(pc->ethif) {
    return pppifOutputOverEthernet(pd, pb);
  }
#endif /* PPPOE_SUPPORT */

#if PPPOS_SUPPORT
  /* Grab an output buffer. */
  headMB = pbuf_alloc(PBUF_RAW, 0, PBUF_POOL);
  if (headMB == NULL) {
    PPPDEBUG(LOG_WARNING, ("pppifOutput[%d]: first alloc fail\n", pd));
    LINK_STATS_INC(link.memerr);
    LINK_STATS_INC(link.drop);
    snmp_inc_ifoutdiscards(netif);
    return ERR_MEM;
  }

#if VJ_SUPPORT
  /* 
   * Attempt Van Jacobson header compression if VJ is configured and
   * this is an IP packet. 
   */
  if (protocol == PPP_IP && pc->vjEnabled) {
    switch (vj_compress_tcp(&pc->vjComp, pb)) {
      case TYPE_IP:
        /* No change...
           protocol = PPP_IP_PROTOCOL; */
        break;
      case TYPE_COMPRESSED_TCP:
        protocol = PPP_VJC_COMP;
        break;
      case TYPE_UNCOMPRESSED_TCP:
        protocol = PPP_VJC_UNCOMP;
        break;
      default:
        PPPDEBUG(LOG_WARNING, ("pppifOutput[%d]: bad IP packet\n", pd));
        LINK_STATS_INC(link.proterr);
        LINK_STATS_INC(link.drop);
        snmp_inc_ifoutdiscards(netif);
        pbuf_free(headMB);
        return ERR_VAL;
    }
  }
#endif /* VJ_SUPPORT */

  tailMB = headMB;

  /* Build the PPP header. */
  if ((sys_jiffies() - pc->lastXMit) >= PPP_MAXIDLEFLAG) {
    tailMB = pppAppend(PPP_FLAG, tailMB, NULL);
  }

  pc->lastXMit = sys_jiffies();
  if (!pc->accomp) {
    fcsOut = PPP_FCS(fcsOut, PPP_ALLSTATIONS);
    tailMB = pppAppend(PPP_ALLSTATIONS, tailMB, &pc->outACCM);
    fcsOut = PPP_FCS(fcsOut, PPP_UI);
    tailMB = pppAppend(PPP_UI, tailMB, &pc->outACCM);
  }
  if (!pc->pcomp || protocol > 0xFF) {
    c = (protocol >> 8) & 0xFF;
    fcsOut = PPP_FCS(fcsOut, c);
    tailMB = pppAppend(c, tailMB, &pc->outACCM);
  }
  c = protocol & 0xFF;
  fcsOut = PPP_FCS(fcsOut, c);
  tailMB = pppAppend(c, tailMB, &pc->outACCM);

  /* Load packet. */
  for(p = pb; p; p = p->next) {
    int n;
    u_char *sPtr;

    sPtr = (u_char*)p->payload;
    n = p->len;
    while (n-- > 0) {
      c = *sPtr++;

      /* Update FCS before checking for special characters. */
      fcsOut = PPP_FCS(fcsOut, c);
      
      /* Copy to output buffer escaping special characters. */
      tailMB = pppAppend(c, tailMB, &pc->outACCM);
    }
  }

  /* Add FCS and trailing flag. */
  c = ~fcsOut & 0xFF;
  tailMB = pppAppend(c, tailMB, &pc->outACCM);
  c = (~fcsOut >> 8) & 0xFF;
  tailMB = pppAppend(c, tailMB, &pc->outACCM);
  tailMB = pppAppend(PPP_FLAG, tailMB, NULL);

  /* If we failed to complete the packet, throw it away. */
  if (!tailMB) {
    PPPDEBUG(LOG_WARNING,
             ("pppifOutput[%d]: Alloc err - dropping proto=%d\n", 
              pd, protocol));
    pbuf_free(headMB);
    LINK_STATS_INC(link.memerr);
    LINK_STATS_INC(link.drop);
    snmp_inc_ifoutdiscards(netif);
    return ERR_MEM;
  }

  /* Send it. */
  PPPDEBUG(LOG_INFO, ("pppifOutput[%d]: proto=0x%"X16_F"\n", pd, protocol));

  nPut(pc, headMB);
#endif /* PPPOS_SUPPORT */

  return ERR_OK;
}

/* Get and set parameters for the given connection.
 * Return 0 on success, an error code on failure. */
int
pppIOCtl(int pd, int cmd, void *arg)
{
  PPPControl *pc = &pppControl[pd];
  int st = 0;

  if (pd < 0 || pd >= NUM_PPP) {
    st = PPPERR_PARAM;
  } else {
    switch(cmd) {
    case PPPCTLG_UPSTATUS:      /* Get the PPP up status. */
      if (arg) {
        *(int *)arg = (int)(pc->if_up);
      } else {
        st = PPPERR_PARAM;
      }
      break;
    case PPPCTLS_ERRCODE:       /* Set the PPP error code. */
      if (arg) {
        pc->errCode = *(int *)arg;
      } else {
        st = PPPERR_PARAM;
      }
      break;
    case PPPCTLG_ERRCODE:       /* Get the PPP error code. */
      if (arg) {
        *(int *)arg = (int)(pc->errCode);
      } else {
        st = PPPERR_PARAM;
      }
      break;
#if PPPOS_SUPPORT
    case PPPCTLG_FD:            /* Get the fd associated with the ppp */
      if (arg) {
        *(sio_fd_t *)arg = pc->fd;
      } else {
        st = PPPERR_PARAM;
      }
      break;
#endif /* PPPOS_SUPPORT */
    default:
      st = PPPERR_PARAM;
      break;
    }
  }

  return st;
}

/*
 * Return the Maximum Transmission Unit for the given PPP connection.
 */
u_short
pppMTU(int pd)
{
  PPPControl *pc = &pppControl[pd];
  u_short st;

  /* Validate parameters. */
  if (pd < 0 || pd >= NUM_PPP || !pc->openFlag) {
    st = 0;
  } else {
    st = pc->mtu;
  }

  return st;
}

#if PPPOE_SUPPORT
int
pppWriteOverEthernet(int pd, const u_char *s, int n)
{
  PPPControl *pc = &pppControl[pd];
  struct pbuf *pb;

  /* skip address & flags */
  s += 2;
  n -= 2;

  LWIP_ASSERT("PPPOE_HDRLEN + n <= 0xffff", PPPOE_HDRLEN + n <= 0xffff);
  pb = pbuf_alloc(PBUF_LINK, (u16_t)(PPPOE_HDRLEN + n), PBUF_RAM);
  if(!pb) {
    LINK_STATS_INC(link.memerr);
    LINK_STATS_INC(link.proterr);
    snmp_inc_ifoutdiscards(&pc->netif);
    return PPPERR_ALLOC;
  }

  pbuf_header(pb, -(s16_t)PPPOE_HDRLEN);

  pc->lastXMit = sys_jiffies();

  MEMCPY(pb->payload, s, n);

  if(pppoe_xmit(pc->pppoe_sc, pb) != ERR_OK) {
    LINK_STATS_INC(link.err);
    snmp_inc_ifoutdiscards(&pc->netif);
    return PPPERR_DEVICE;
  }

  snmp_add_ifoutoctets(&pc->netif, (u16_t)n);
  snmp_inc_ifoutucastpkts(&pc->netif);
  LINK_STATS_INC(link.xmit);
  return PPPERR_NONE;
}
#endif /* PPPOE_SUPPORT */

/*
 * Write n characters to a ppp link.
 *  RETURN: >= 0 Number of characters written
 *           -1 Failed to write to device
 */
int
pppWrite(int pd, const u_char *s, int n)
{
  PPPControl *pc = &pppControl[pd];
#if PPPOS_SUPPORT
  u_char c;
  u_int fcsOut;
  struct pbuf *headMB, *tailMB;
#endif /* PPPOS_SUPPORT */

#if PPPOE_SUPPORT
  if(pc->ethif) {
    return pppWriteOverEthernet(pd, s, n);
  }
#endif /* PPPOE_SUPPORT */

#if PPPOS_SUPPORT
  headMB = pbuf_alloc(PBUF_RAW, 0, PBUF_POOL);
  if (headMB == NULL) {
    LINK_STATS_INC(link.memerr);
    LINK_STATS_INC(link.proterr);
    snmp_inc_ifoutdiscards(&pc->netif);
    return PPPERR_ALLOC;
  }

  tailMB = headMB;

  /* If the link has been idle, we'll send a fresh flag character to
   * flush any noise. */
  if ((sys_jiffies() - pc->lastXMit) >= PPP_MAXIDLEFLAG) {
    tailMB = pppAppend(PPP_FLAG, tailMB, NULL);
  }
  pc->lastXMit = sys_jiffies();

  fcsOut = PPP_INITFCS;
  /* Load output buffer. */
  while (n-- > 0) {
    c = *s++;

    /* Update FCS before checking for special characters. */
    fcsOut = PPP_FCS(fcsOut, c);

    /* Copy to output buffer escaping special characters. */
    tailMB = pppAppend(c, tailMB, &pc->outACCM);
  }
    
  /* Add FCS and trailing flag. */
  c = ~fcsOut & 0xFF;
  tailMB = pppAppend(c, tailMB, &pc->outACCM);
  c = (~fcsOut >> 8) & 0xFF;
  tailMB = pppAppend(c, tailMB, &pc->outACCM);
  tailMB = pppAppend(PPP_FLAG, tailMB, NULL);

  /* If we failed to complete the packet, throw it away.
   * Otherwise send it. */
  if (!tailMB) {
    PPPDEBUG(LOG_WARNING,
             ("pppWrite[%d]: Alloc err - dropping pbuf len=%d\n", pd, headMB->len));
           /*"pppWrite[%d]: Alloc err - dropping %d:%.*H", pd, headMB->len, LWIP_MIN(headMB->len * 2, 40), headMB->payload)); */
    pbuf_free(headMB);
    LINK_STATS_INC(link.memerr);
    LINK_STATS_INC(link.proterr);
    snmp_inc_ifoutdiscards(&pc->netif);
    return PPPERR_ALLOC;
  }

  PPPDEBUG(LOG_INFO, ("pppWrite[%d]: len=%d\n", pd, headMB->len));
                   /* "pppWrite[%d]: %d:%.*H", pd, headMB->len, LWIP_MIN(headMB->len * 2, 40), headMB->payload)); */
  nPut(pc, headMB);
#endif /* PPPOS_SUPPORT */

  return PPPERR_NONE;
}

/*
 * ppp_send_config - configure the transmit characteristics of
 * the ppp interface.
 */
void
ppp_send_config( int unit, u16_t mtu, u32_t asyncmap, int pcomp, int accomp)
{
  PPPControl *pc = &pppControl[unit];
  int i;
  
  pc->mtu = mtu;
  pc->pcomp = pcomp;
  pc->accomp = accomp;
  
  /* Load the ACCM bits for the 32 control codes. */
  for (i = 0; i < 32/8; i++) {
    pc->outACCM[i] = (u_char)((asyncmap >> (8 * i)) & 0xFF);
  }
  PPPDEBUG(LOG_INFO, ("ppp_send_config[%d]: outACCM=%X %X %X %X\n",
            unit,
            pc->outACCM[0], pc->outACCM[1], pc->outACCM[2], pc->outACCM[3]));
}


/*
 * ppp_set_xaccm - set the extended transmit ACCM for the interface.
 */
void
ppp_set_xaccm(int unit, ext_accm *accm)
{
  SMEMCPY(pppControl[unit].outACCM, accm, sizeof(ext_accm));
  PPPDEBUG(LOG_INFO, ("ppp_set_xaccm[%d]: outACCM=%X %X %X %X\n",
            unit,
            pppControl[unit].outACCM[0],
            pppControl[unit].outACCM[1],
            pppControl[unit].outACCM[2],
            pppControl[unit].outACCM[3]));
}


/*
 * ppp_recv_config - configure the receive-side characteristics of
 * the ppp interface.
 */
void
ppp_recv_config( int unit, int mru, u32_t asyncmap, int pcomp, int accomp)
{
  PPPControl *pc = &pppControl[unit];
  int i;
  SYS_ARCH_DECL_PROTECT(lev);

  LWIP_UNUSED_ARG(accomp);
  LWIP_UNUSED_ARG(pcomp);
  LWIP_UNUSED_ARG(mru);

  /* Load the ACCM bits for the 32 control codes. */
  SYS_ARCH_PROTECT(lev);
  for (i = 0; i < 32 / 8; i++) {
    /* @todo: does this work? ext_accm has been modified from pppd! */
    pc->rx.inACCM[i] = (u_char)(asyncmap >> (i * 8));
  }
  SYS_ARCH_UNPROTECT(lev);
  PPPDEBUG(LOG_INFO, ("ppp_recv_config[%d]: inACCM=%X %X %X %X\n",
            unit,
            pc->rx.inACCM[0], pc->rx.inACCM[1], pc->rx.inACCM[2], pc->rx.inACCM[3]));
}

#if 0
/*
 * ccp_test - ask kernel whether a given compression method
 * is acceptable for use.  Returns 1 if the method and parameters
 * are OK, 0 if the method is known but the parameters are not OK
 * (e.g. code size should be reduced), or -1 if the method is unknown.
 */
int
ccp_test( int unit, int opt_len,  int for_transmit, u_char *opt_ptr)
{
  return 0; /* XXX Currently no compression. */
}

/*
 * ccp_flags_set - inform kernel about the current state of CCP.
 */
void
ccp_flags_set(int unit, int isopen, int isup)
{
  /* XXX */
}

/*
 * ccp_fatal_error - returns 1 if decompression was disabled as a
 * result of an error detected after decompression of a packet,
 * 0 otherwise.  This is necessary because of patent nonsense.
 */
int
ccp_fatal_error(int unit)
{
  /* XXX */
  return 0;
}
#endif

/*
 * get_idle_time - return how long the link has been idle.
 */
int
get_idle_time(int u, struct ppp_idle *ip)
{
  /* XXX */
  LWIP_UNUSED_ARG(u);
  LWIP_UNUSED_ARG(ip);

  return 0;
}


/*
 * Return user specified netmask, modified by any mask we might determine
 * for address `addr' (in network byte order).
 * Here we scan through the system's list of interfaces, looking for
 * any non-point-to-point interfaces which might appear to be on the same
 * network as `addr'.  If we find any, we OR in their netmask to the
 * user-specified netmask.
 */
u32_t
GetMask(u32_t addr)
{
  u32_t mask, nmask;

  addr = htonl(addr);
  if (IP_CLASSA(addr)) { /* determine network mask for address class */
    nmask = IP_CLASSA_NET;
  } else if (IP_CLASSB(addr)) {
    nmask = IP_CLASSB_NET;
  } else { 
    nmask = IP_CLASSC_NET;
  }

  /* class D nets are disallowed by bad_ip_adrs */
  mask = PP_HTONL(0xffffff00UL) | htonl(nmask);
  
  /* XXX
   * Scan through the system's network interfaces.
   * Get each netmask and OR them into our mask.
   */

  return mask;
}

/*
 * sifvjcomp - config tcp header compression
 */
int
sifvjcomp(int pd, int vjcomp, u8_t cidcomp, u8_t maxcid)
{
#if PPPOS_SUPPORT && VJ_SUPPORT
  PPPControl *pc = &pppControl[pd];
  
  pc->vjEnabled = vjcomp;
  pc->vjComp.compressSlot = cidcomp;
  pc->vjComp.maxSlotIndex = maxcid;
  PPPDEBUG(LOG_INFO, ("sifvjcomp: VJ compress enable=%d slot=%d max slot=%d\n",
            vjcomp, cidcomp, maxcid));
#else /* PPPOS_SUPPORT && VJ_SUPPORT */
  LWIP_UNUSED_ARG(pd);
  LWIP_UNUSED_ARG(vjcomp);
  LWIP_UNUSED_ARG(cidcomp);
  LWIP_UNUSED_ARG(maxcid);
#endif /* PPPOS_SUPPORT && VJ_SUPPORT */

  return 0;
}

/*
 * pppifNetifInit - netif init callback
 */
static err_t
pppifNetifInit(struct netif *netif)
{
  netif->name[0] = 'p';
  netif->name[1] = 'p';
  netif->output = pppifOutput;
  netif->mtu = pppMTU((int)(size_t)netif->state);
  netif->flags = NETIF_FLAG_POINTTOPOINT | NETIF_FLAG_LINK_UP;
#if LWIP_NETIF_HOSTNAME
  /* @todo: Initialize interface hostname */
  /* netif_set_hostname(netif, "lwip"); */
#endif /* LWIP_NETIF_HOSTNAME */
  return ERR_OK;
}


/*
 * sifup - Config the interface up and enable IP packets to pass.
 */
int
sifup(int pd)
{
  PPPControl *pc = &pppControl[pd];
  int st = 1;
  
  if (pd < 0 || pd >= NUM_PPP || !pc->openFlag) {
    st = 0;
    PPPDEBUG(LOG_WARNING, ("sifup[%d]: bad parms\n", pd));
  } else {
    netif_remove(&pc->netif);
    if (netif_add(&pc->netif, &pc->addrs.our_ipaddr, &pc->addrs.netmask,
                  &pc->addrs.his_ipaddr, (void *)(size_t)pd, pppifNetifInit, ip_input)) {
      netif_set_up(&pc->netif);
      pc->if_up = 1;
      pc->errCode = PPPERR_NONE;

      PPPDEBUG(LOG_DEBUG, ("sifup: unit %d: linkStatusCB=%p errCode=%d\n", pd, pc->linkStatusCB, pc->errCode));
      if (pc->linkStatusCB) {
        pc->linkStatusCB(pc->linkStatusCtx, pc->errCode, &pc->addrs);
      }
    } else {
      st = 0;
      PPPDEBUG(LOG_ERR, ("sifup[%d]: netif_add failed\n", pd));
    }
  }

  return st;
}

/*
 * sifnpmode - Set the mode for handling packets for a given NP.
 */
int
sifnpmode(int u, int proto, enum NPmode mode)
{
  LWIP_UNUSED_ARG(u);
  LWIP_UNUSED_ARG(proto);
  LWIP_UNUSED_ARG(mode);
  return 0;
}

/*
 * sifdown - Config the interface down and disable IP.
 */
int
sifdown(int pd)
{
  PPPControl *pc = &pppControl[pd];
  int st = 1;
  
  if (pd < 0 || pd >= NUM_PPP || !pc->openFlag) {
    st = 0;
    PPPDEBUG(LOG_WARNING, ("sifdown[%d]: bad parms\n", pd));
  } else {
    pc->if_up = 0;
    /* make sure the netif status callback is called */
    netif_set_down(&pc->netif);
    netif_remove(&pc->netif);
    PPPDEBUG(LOG_DEBUG, ("sifdown: unit %d: linkStatusCB=%p errCode=%d\n", pd, pc->linkStatusCB, pc->errCode));
    if (pc->linkStatusCB) {
      pc->linkStatusCB(pc->linkStatusCtx, PPPERR_CONNECT, NULL);
    }
  }
  return st;
}

/**
 * sifaddr - Config the interface IP addresses and netmask.
 * @param pd Interface unit ???
 * @param o Our IP address ???
 * @param h His IP address ???
 * @param m IP subnet mask ???
 * @param ns1 Primary DNS
 * @param ns2 Secondary DNS
 */
int
sifaddr( int pd, u32_t o, u32_t h, u32_t m, u32_t ns1, u32_t ns2)
{
  PPPControl *pc = &pppControl[pd];
  int st = 1;
  
  if (pd < 0 || pd >= NUM_PPP || !pc->openFlag) {
    st = 0;
    PPPDEBUG(LOG_WARNING, ("sifup[%d]: bad parms\n", pd));
  } else {
    SMEMCPY(&pc->addrs.our_ipaddr, &o, sizeof(o));
    SMEMCPY(&pc->addrs.his_ipaddr, &h, sizeof(h));
    SMEMCPY(&pc->addrs.netmask, &m, sizeof(m));
    SMEMCPY(&pc->addrs.dns1, &ns1, sizeof(ns1));
    SMEMCPY(&pc->addrs.dns2, &ns2, sizeof(ns2));
  }
  return st;
}

/**
 * cifaddr - Clear the interface IP addresses, and delete routes
 * through the interface if possible.
 * @param pd Interface unit ???
 * @param o Our IP address ???
 * @param h IP broadcast address ???
 */
int
cifaddr( int pd, u32_t o, u32_t h)
{
  PPPControl *pc = &pppControl[pd];
  int st = 1;
  
  LWIP_UNUSED_ARG(o);
  LWIP_UNUSED_ARG(h);
  if (pd < 0 || pd >= NUM_PPP || !pc->openFlag) {
    st = 0;
    PPPDEBUG(LOG_WARNING, ("sifup[%d]: bad parms\n", pd));
  } else {
    IP4_ADDR(&pc->addrs.our_ipaddr, 0,0,0,0);
    IP4_ADDR(&pc->addrs.his_ipaddr, 0,0,0,0);
    IP4_ADDR(&pc->addrs.netmask, 255,255,255,0);
    IP4_ADDR(&pc->addrs.dns1, 0,0,0,0);
    IP4_ADDR(&pc->addrs.dns2, 0,0,0,0);
  }
  return st;
}

/*
 * sifdefaultroute - assign a default route through the address given.
 */
int
sifdefaultroute(int pd, u32_t l, u32_t g)
{
  PPPControl *pc = &pppControl[pd];
  int st = 1;

  LWIP_UNUSED_ARG(l);
  LWIP_UNUSED_ARG(g);

  if (pd < 0 || pd >= NUM_PPP || !pc->openFlag) {
    st = 0;
    PPPDEBUG(LOG_WARNING, ("sifup[%d]: bad parms\n", pd));
  } else {
    netif_set_default(&pc->netif);
  }

  /* TODO: check how PPP handled the netMask, previously not set by ipSetDefault */

  return st;
}

/*
 * cifdefaultroute - delete a default route through the address given.
 */
int
cifdefaultroute(int pd, u32_t l, u32_t g)
{
  PPPControl *pc = &pppControl[pd];
  int st = 1;

  LWIP_UNUSED_ARG(l);
  LWIP_UNUSED_ARG(g);

  if (pd < 0 || pd >= NUM_PPP || !pc->openFlag) {
    st = 0;
    PPPDEBUG(LOG_WARNING, ("sifup[%d]: bad parms\n", pd));
  } else {
    netif_set_default(NULL);
  }

  return st;
}

/**********************************/
/*** LOCAL FUNCTION DEFINITIONS ***/
/**********************************/

#if PPPOS_SUPPORT && PPP_INPROC_OWNTHREAD
/* The main PPP process function.  This implements the state machine according
 * to section 4 of RFC 1661: The Point-To-Point Protocol. */
static void
pppInputThread(void *arg)
{
  int count;
  PPPControlRx *pcrx = arg;

  while (lcp_phase[pcrx->pd] != PHASE_DEAD) {
    count = sio_read(pcrx->fd, pcrx->rxbuf, PPPOS_RX_BUFSIZE);
    if(count > 0) {
      pppInProc(pcrx, pcrx->rxbuf, count);
    } else {
      /* nothing received, give other tasks a chance to run */
      sys_msleep(1);
    }
  }
}
#endif /* PPPOS_SUPPORT && PPP_INPROC_OWNTHREAD */

#if PPPOE_SUPPORT

void
pppOverEthernetInitFailed(int pd)
{
  PPPControl* pc;

  pppHup(pd);
  pppStop(pd);

  pc = &pppControl[pd];
  pppoe_destroy(&pc->netif);
  pc->openFlag = 0;

  if(pc->linkStatusCB) {
    pc->linkStatusCB(pc->linkStatusCtx, pc->errCode ? pc->errCode : PPPERR_PROTOCOL, NULL);
  }
}

static void
pppOverEthernetLinkStatusCB(int pd, int up)
{
  if(up) {
    PPPDEBUG(LOG_INFO, ("pppOverEthernetLinkStatusCB: unit %d: Connecting\n", pd));
    pppStart(pd);
  } else {
    pppOverEthernetInitFailed(pd);
  }
}
#endif /* PPPOE_SUPPORT */

struct pbuf *
pppSingleBuf(struct pbuf *p)
{
  struct pbuf *q, *b;
  u_char *pl;

  if(p->tot_len == p->len) {
    return p;
  }

  q = pbuf_alloc(PBUF_RAW, p->tot_len, PBUF_RAM);
  if(!q) {
    PPPDEBUG(LOG_ERR,
             ("pppSingleBuf: unable to alloc new buf (%d)\n", p->tot_len));
    return p; /* live dangerously */
  }

  for(b = p, pl = q->payload; b != NULL; b = b->next) {
    MEMCPY(pl, b->payload, b->len);
    pl += b->len;
  }

  pbuf_free(p);

  return q;
}

/** Input helper struct, must be packed since it is stored to pbuf->payload,
 * which might be unaligned.
 */
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/bpstruct.h"
#endif
PACK_STRUCT_BEGIN
struct pppInputHeader {
  PACK_STRUCT_FIELD(int unit);
  PACK_STRUCT_FIELD(u16_t proto);
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/epstruct.h"
#endif

/*
 * Pass the processed input packet to the appropriate handler.
 * This function and all handlers run in the context of the tcpip_thread
 */
static void
pppInput(void *arg)
{
  struct pbuf *nb = (struct pbuf *)arg;
  u16_t protocol;
  int pd;

  pd = ((struct pppInputHeader *)nb->payload)->unit;
  protocol = ((struct pppInputHeader *)nb->payload)->proto;
    
  if(pbuf_header(nb, -(int)sizeof(struct pppInputHeader))) {
    LWIP_ASSERT("pbuf_header failed\n", 0);
    goto drop;
  }

  LINK_STATS_INC(link.recv);
  snmp_inc_ifinucastpkts(&pppControl[pd].netif);
  snmp_add_ifinoctets(&pppControl[pd].netif, nb->tot_len);

  /*
   * Toss all non-LCP packets unless LCP is OPEN.
   * Until we get past the authentication phase, toss all packets
   * except LCP, LQR and authentication packets.
   */
  if((lcp_phase[pd] <= PHASE_AUTHENTICATE) && (protocol != PPP_LCP)) {
    if(!((protocol == PPP_LQR) || (protocol == PPP_PAP) || (protocol == PPP_CHAP)) ||
        (lcp_phase[pd] != PHASE_AUTHENTICATE)) {
      PPPDEBUG(LOG_INFO, ("pppInput: discarding proto 0x%"X16_F" in phase %d\n", protocol, lcp_phase[pd]));
      goto drop;
    }
  }

  switch(protocol) {
    case PPP_VJC_COMP:      /* VJ compressed TCP */
#if PPPOS_SUPPORT && VJ_SUPPORT
      PPPDEBUG(LOG_INFO, ("pppInput[%d]: vj_comp in pbuf len=%d\n", pd, nb->len));
      /*
       * Clip off the VJ header and prepend the rebuilt TCP/IP header and
       * pass the result to IP.
       */
      if ((vj_uncompress_tcp(&nb, &pppControl[pd].vjComp) >= 0) && (pppControl[pd].netif.input)) {
        pppControl[pd].netif.input(nb, &pppControl[pd].netif);
        return;
      }
      /* Something's wrong so drop it. */
      PPPDEBUG(LOG_WARNING, ("pppInput[%d]: Dropping VJ compressed\n", pd));
#else  /* PPPOS_SUPPORT && VJ_SUPPORT */
      /* No handler for this protocol so drop the packet. */
      PPPDEBUG(LOG_INFO, ("pppInput[%d]: drop VJ Comp in %d:%s\n", pd, nb->len, nb->payload));
#endif /* PPPOS_SUPPORT && VJ_SUPPORT */
      break;

    case PPP_VJC_UNCOMP:    /* VJ uncompressed TCP */
#if PPPOS_SUPPORT && VJ_SUPPORT
      PPPDEBUG(LOG_INFO, ("pppInput[%d]: vj_un in pbuf len=%d\n", pd, nb->len));
      /*
       * Process the TCP/IP header for VJ header compression and then pass
       * the packet to IP.
       */
      if ((vj_uncompress_uncomp(nb, &pppControl[pd].vjComp) >= 0) && pppControl[pd].netif.input) {
        pppControl[pd].netif.input(nb, &pppControl[pd].netif);
        return;
      }
      /* Something's wrong so drop it. */
      PPPDEBUG(LOG_WARNING, ("pppInput[%d]: Dropping VJ uncompressed\n", pd));
#else  /* PPPOS_SUPPORT && VJ_SUPPORT */
      /* No handler for this protocol so drop the packet. */
      PPPDEBUG(LOG_INFO,
               ("pppInput[%d]: drop VJ UnComp in %d:.*H\n", 
                pd, nb->len, LWIP_MIN(nb->len * 2, 40), nb->payload));
#endif /* PPPOS_SUPPORT && VJ_SUPPORT */
      break;

    case PPP_IP:            /* Internet Protocol */
      PPPDEBUG(LOG_INFO, ("pppInput[%d]: ip in pbuf len=%d\n", pd, nb->len));
      if (pppControl[pd].netif.input) {
        pppControl[pd].netif.input(nb, &pppControl[pd].netif);
        return;
      }
      break;

    default: {
      struct protent *protp;
      int i;

      /*
       * Upcall the proper protocol input routine.
       */
      for (i = 0; (protp = ppp_protocols[i]) != NULL; ++i) {
        if (protp->protocol == protocol && protp->enabled_flag) {
          PPPDEBUG(LOG_INFO, ("pppInput[%d]: %s len=%d\n", pd, protp->name, nb->len));
          nb = pppSingleBuf(nb);
          (*protp->input)(pd, nb->payload, nb->len);
          PPPDEBUG(LOG_DETAIL, ("pppInput[%d]: packet processed\n", pd));
          goto out;
        }
      }

      /* No handler for this protocol so reject the packet. */
      PPPDEBUG(LOG_INFO, ("pppInput[%d]: rejecting unsupported proto 0x%"X16_F" len=%d\n", pd, protocol, nb->len));
      if (pbuf_header(nb, sizeof(protocol))) {
        LWIP_ASSERT("pbuf_header failed\n", 0);
        goto drop;
      }
#if BYTE_ORDER == LITTLE_ENDIAN
      protocol = htons(protocol);
#endif /* BYTE_ORDER == LITTLE_ENDIAN */
      SMEMCPY(nb->payload, &protocol, sizeof(protocol));
      lcp_sprotrej(pd, nb->payload, nb->len);
    }
    break;
  }

drop:
  LINK_STATS_INC(link.drop);
  snmp_inc_ifindiscards(&pppControl[pd].netif);

out:
  pbuf_free(nb);
  return;
}

#if PPPOS_SUPPORT
/*
 * Drop the input packet.
 */
static void
pppFreeCurrentInputPacket(PPPControlRx *pcrx)
{
  if (pcrx->inHead != NULL) {
    if (pcrx->inTail && (pcrx->inTail != pcrx->inHead)) {
      pbuf_free(pcrx->inTail);
    }
    pbuf_free(pcrx->inHead);
    pcrx->inHead = NULL;
  }
  pcrx->inTail = NULL;
}

/*
 * Drop the input packet and increase error counters.
 */
static void
pppDrop(PPPControlRx *pcrx)
{
  if (pcrx->inHead != NULL) {
#if 0
    PPPDEBUG(LOG_INFO, ("pppDrop: %d:%.*H\n", pcrx->inHead->len, min(60, pcrx->inHead->len * 2), pcrx->inHead->payload));
#endif
    PPPDEBUG(LOG_INFO, ("pppDrop: pbuf len=%d, addr %p\n", pcrx->inHead->len, (void*)pcrx->inHead));
  }
  pppFreeCurrentInputPacket(pcrx);
#if VJ_SUPPORT
  vj_uncompress_err(&pppControl[pcrx->pd].vjComp);
#endif /* VJ_SUPPORT */

  LINK_STATS_INC(link.drop);
  snmp_inc_ifindiscards(&pppControl[pcrx->pd].netif);
}

#if !PPP_INPROC_OWNTHREAD
/** Pass received raw characters to PPPoS to be decoded. This function is
 * thread-safe and can be called from a dedicated RX-thread or from a main-loop.
 *
 * @param pd PPP descriptor index, returned by pppOpen()
 * @param data received data
 * @param len length of received data
 */
void
pppos_input(int pd, u_char* data, int len)
{
  pppInProc(&pppControl[pd].rx, data, len);
}
#endif

/**
 * Process a received octet string.
 */
static void
pppInProc(PPPControlRx *pcrx, u_char *s, int l)
{
  struct pbuf *nextNBuf;
  u_char curChar;
  u_char escaped;
  SYS_ARCH_DECL_PROTECT(lev);

  PPPDEBUG(LOG_DEBUG, ("pppInProc[%d]: got %d bytes\n", pcrx->pd, l));
  while (l-- > 0) {
    curChar = *s++;

    SYS_ARCH_PROTECT(lev);
    escaped = ESCAPE_P(pcrx->inACCM, curChar);
    SYS_ARCH_UNPROTECT(lev);
    /* Handle special characters. */
    if (escaped) {
      /* Check for escape sequences. */
      /* XXX Note that this does not handle an escaped 0x5d character which
       * would appear as an escape character.  Since this is an ASCII ']'
       * and there is no reason that I know of to escape it, I won't complicate
       * the code to handle this case. GLL */
      if (curChar == PPP_ESCAPE) {
        pcrx->inEscaped = 1;
      /* Check for the flag character. */
      } else if (curChar == PPP_FLAG) {
        /* If this is just an extra flag character, ignore it. */
        if (pcrx->inState <= PDADDRESS) {
          /* ignore it */;
        /* If we haven't received the packet header, drop what has come in. */
        } else if (pcrx->inState < PDDATA) {
          PPPDEBUG(LOG_WARNING,
                   ("pppInProc[%d]: Dropping incomplete packet %d\n", 
                    pcrx->pd, pcrx->inState));
          LINK_STATS_INC(link.lenerr);
          pppDrop(pcrx);
        /* If the fcs is invalid, drop the packet. */
        } else if (pcrx->inFCS != PPP_GOODFCS) {
          PPPDEBUG(LOG_INFO,
                   ("pppInProc[%d]: Dropping bad fcs 0x%"X16_F" proto=0x%"X16_F"\n", 
                    pcrx->pd, pcrx->inFCS, pcrx->inProtocol));
          /* Note: If you get lots of these, check for UART frame errors or try different baud rate */
          LINK_STATS_INC(link.chkerr);
          pppDrop(pcrx);
        /* Otherwise it's a good packet so pass it on. */
        } else {
          struct pbuf *inp;
          /* Trim off the checksum. */
          if(pcrx->inTail->len > 2) {
            pcrx->inTail->len -= 2;

            pcrx->inTail->tot_len = pcrx->inTail->len;
            if (pcrx->inTail != pcrx->inHead) {
              pbuf_cat(pcrx->inHead, pcrx->inTail);
            }
          } else {
            pcrx->inTail->tot_len = pcrx->inTail->len;
            if (pcrx->inTail != pcrx->inHead) {
              pbuf_cat(pcrx->inHead, pcrx->inTail);
            }

            pbuf_realloc(pcrx->inHead, pcrx->inHead->tot_len - 2);
          }

          /* Dispatch the packet thereby consuming it. */
          inp = pcrx->inHead;
          /* Packet consumed, release our references. */
          pcrx->inHead = NULL;
          pcrx->inTail = NULL;
#if PPP_INPROC_MULTITHREADED
          if(tcpip_callback_with_block(pppInput, inp, 0) != ERR_OK) {
            PPPDEBUG(LOG_ERR, ("pppInProc[%d]: tcpip_callback() failed, dropping packet\n", pcrx->pd));
            pbuf_free(inp);
            LINK_STATS_INC(link.drop);
            snmp_inc_ifindiscards(&pppControl[pcrx->pd].netif);
          }
#else /* PPP_INPROC_MULTITHREADED */
          pppInput(inp);
#endif /* PPP_INPROC_MULTITHREADED */
        }

        /* Prepare for a new packet. */
        pcrx->inFCS = PPP_INITFCS;
        pcrx->inState = PDADDRESS;
        pcrx->inEscaped = 0;
      /* Other characters are usually control characters that may have
       * been inserted by the physical layer so here we just drop them. */
      } else {
        PPPDEBUG(LOG_WARNING,
                 ("pppInProc[%d]: Dropping ACCM char <%d>\n", pcrx->pd, curChar));
      }
    /* Process other characters. */
    } else {
      /* Unencode escaped characters. */
      if (pcrx->inEscaped) {
        pcrx->inEscaped = 0;
        curChar ^= PPP_TRANS;
      }

      /* Process character relative to current state. */
      switch(pcrx->inState) {
        case PDIDLE:                    /* Idle state - waiting. */
          /* Drop the character if it's not 0xff
           * we would have processed a flag character above. */
          if (curChar != PPP_ALLSTATIONS) {
            break;
          }

        /* Fall through */
        case PDSTART:                   /* Process start flag. */
          /* Prepare for a new packet. */
          pcrx->inFCS = PPP_INITFCS;

        /* Fall through */
        case PDADDRESS:                 /* Process address field. */
          if (curChar == PPP_ALLSTATIONS) {
            pcrx->inState = PDCONTROL;
            break;
          }
          /* Else assume compressed address and control fields so
           * fall through to get the protocol... */
        case PDCONTROL:                 /* Process control field. */
          /* If we don't get a valid control code, restart. */
          if (curChar == PPP_UI) {
            pcrx->inState = PDPROTOCOL1;
            break;
          }
#if 0
          else {
            PPPDEBUG(LOG_WARNING,
                     ("pppInProc[%d]: Invalid control <%d>\n", pcrx->pd, curChar));
            pcrx->inState = PDSTART;
          }
#endif
        case PDPROTOCOL1:               /* Process protocol field 1. */
          /* If the lower bit is set, this is the end of the protocol
           * field. */
          if (curChar & 1) {
            pcrx->inProtocol = curChar;
            pcrx->inState = PDDATA;
          } else {
            pcrx->inProtocol = (u_int)curChar << 8;
            pcrx->inState = PDPROTOCOL2;
          }
          break;
        case PDPROTOCOL2:               /* Process protocol field 2. */
          pcrx->inProtocol |= curChar;
          pcrx->inState = PDDATA;
          break;
        case PDDATA:                    /* Process data byte. */
          /* Make space to receive processed data. */
          if (pcrx->inTail == NULL || pcrx->inTail->len == PBUF_POOL_BUFSIZE) {
            if (pcrx->inTail != NULL) {
              pcrx->inTail->tot_len = pcrx->inTail->len;
              if (pcrx->inTail != pcrx->inHead) {
                pbuf_cat(pcrx->inHead, pcrx->inTail);
                /* give up the inTail reference now */
                pcrx->inTail = NULL;
              }
            }
            /* If we haven't started a packet, we need a packet header. */
            nextNBuf = pbuf_alloc(PBUF_RAW, 0, PBUF_POOL);
            if (nextNBuf == NULL) {
              /* No free buffers.  Drop the input packet and let the
               * higher layers deal with it.  Continue processing
               * the received pbuf chain in case a new packet starts. */
              PPPDEBUG(LOG_ERR, ("pppInProc[%d]: NO FREE MBUFS!\n", pcrx->pd));
              LINK_STATS_INC(link.memerr);
              pppDrop(pcrx);
              pcrx->inState = PDSTART;  /* Wait for flag sequence. */
              break;
            }
            if (pcrx->inHead == NULL) {
              struct pppInputHeader *pih = nextNBuf->payload;

              pih->unit = pcrx->pd;
              pih->proto = pcrx->inProtocol;

              nextNBuf->len += sizeof(*pih);

              pcrx->inHead = nextNBuf;
            }
            pcrx->inTail = nextNBuf;
          }
          /* Load character into buffer. */
          ((u_char*)pcrx->inTail->payload)[pcrx->inTail->len++] = curChar;
          break;
      }

      /* update the frame check sequence number. */
      pcrx->inFCS = PPP_FCS(pcrx->inFCS, curChar);
    }
  } /* while (l-- > 0), all bytes processed */

  avRandomize();
}
#endif /* PPPOS_SUPPORT */

#if PPPOE_SUPPORT
void
pppInProcOverEthernet(int pd, struct pbuf *pb)
{
  struct pppInputHeader *pih;
  u16_t inProtocol;

  if(pb->len < sizeof(inProtocol)) {
    PPPDEBUG(LOG_ERR, ("pppInProcOverEthernet: too small for protocol field\n"));
    goto drop;
  }

  inProtocol = (((u8_t *)pb->payload)[0] << 8) | ((u8_t*)pb->payload)[1];

  /* make room for pppInputHeader - should not fail */
  if (pbuf_header(pb, sizeof(*pih) - sizeof(inProtocol)) != 0) {
    PPPDEBUG(LOG_ERR, ("pppInProcOverEthernet: could not allocate room for header\n"));
    goto drop;
  }

  pih = pb->payload;

  pih->unit = pd;
  pih->proto = inProtocol;

  /* Dispatch the packet thereby consuming it. */
  pppInput(pb);
  return;

drop:
  LINK_STATS_INC(link.drop);
  snmp_inc_ifindiscards(&pppControl[pd].netif);
  pbuf_free(pb);
  return;
}
#endif /* PPPOE_SUPPORT */

#if LWIP_NETIF_STATUS_CALLBACK
/** Set the status callback of a PPP's netif
 *
 * @param pd The PPP descriptor returned by pppOpen()
 * @param status_callback pointer to the status callback function
 *
 * @see netif_set_status_callback
 */
void
ppp_set_netif_statuscallback(int pd, netif_status_callback_fn status_callback)
{
  netif_set_status_callback(&pppControl[pd].netif, status_callback); 
}
#endif /* LWIP_NETIF_STATUS_CALLBACK */

#if LWIP_NETIF_LINK_CALLBACK
/** Set the link callback of a PPP's netif
 *
 * @param pd The PPP descriptor returned by pppOpen()
 * @param link_callback pointer to the link callback function
 *
 * @see netif_set_link_callback
 */
void
ppp_set_netif_linkcallback(int pd, netif_status_callback_fn link_callback)
{
  netif_set_link_callback(&pppControl[pd].netif, link_callback); 
}
#endif /* LWIP_NETIF_LINK_CALLBACK */

#endif /* PPP_SUPPORT */
