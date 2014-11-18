/*****************************************************************************
* lcp.h - Network Link Control Protocol header file.
*
* Copyright (c) 2003 by Marc Boucher, Services Informatiques (MBSI) inc.
* portions Copyright (c) 1997 Global Election Systems Inc.
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
* 97-11-05 Guy Lancaster <glanca@gesn.com>, Global Election Systems Inc.
*   Original derived from BSD codes.
*****************************************************************************/
/*
 * lcp.h - Link Control Protocol definitions.
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
 *
 * $Id: lcp.h,v 1.4 2010/01/18 20:49:43 goldsimon Exp $
 */

#ifndef LCP_H
#define LCP_H
/*
 * Options.
 */
#define CI_MRU           1  /* Maximum Receive Unit */
#define CI_ASYNCMAP      2  /* Async Control Character Map */
#define CI_AUTHTYPE      3  /* Authentication Type */
#define CI_QUALITY       4  /* Quality Protocol */
#define CI_MAGICNUMBER   5  /* Magic Number */
#define CI_PCOMPRESSION  7  /* Protocol Field Compression */
#define CI_ACCOMPRESSION 8  /* Address/Control Field Compression */
#define CI_CALLBACK      13 /* callback */
#define CI_MRRU          17 /* max reconstructed receive unit; multilink */
#define CI_SSNHF         18 /* short sequence numbers for multilink */
#define CI_EPDISC        19 /* endpoint discriminator */

/*
 * LCP-specific packet types (code numbers).
 */
#define PROTREJ          8  /* Protocol Reject */
#define ECHOREQ          9  /* Echo Request */
#define ECHOREP          10 /* Echo Reply */
#define DISCREQ          11 /* Discard Request */
#define CBCP_OPT         6  /* Use callback control protocol */

/*
 * The state of options is described by an lcp_options structure.
 */
typedef struct lcp_options {
    u_int passive           : 1; /* Don't die if we don't get a response */
    u_int silent            : 1; /* Wait for the other end to start first */
    u_int restart           : 1; /* Restart vs. exit after close */
    u_int neg_mru           : 1; /* Negotiate the MRU? */
    u_int neg_asyncmap      : 1; /* Negotiate the async map? */
    u_int neg_upap          : 1; /* Ask for UPAP authentication? */
    u_int neg_chap          : 1; /* Ask for CHAP authentication? */
    u_int neg_magicnumber   : 1; /* Ask for magic number? */
    u_int neg_pcompression  : 1; /* HDLC Protocol Field Compression? */
    u_int neg_accompression : 1; /* HDLC Address/Control Field Compression? */
    u_int neg_lqr           : 1; /* Negotiate use of Link Quality Reports */
    u_int neg_cbcp          : 1; /* Negotiate use of CBCP */
#ifdef PPP_MULTILINK
    u_int neg_mrru          : 1; /* Negotiate multilink MRRU */
    u_int neg_ssnhf         : 1; /* Negotiate short sequence numbers */
    u_int neg_endpoint      : 1; /* Negotiate endpoint discriminator */
#endif
    u_short mru;                 /* Value of MRU */
#ifdef PPP_MULTILINK
    u_short mrru;                /* Value of MRRU, and multilink enable */
#endif
    u_char chap_mdtype;          /* which MD type (hashing algorithm) */
    u32_t asyncmap;              /* Value of async map */
    u32_t magicnumber;
    int numloops;                /* Number of loops during magic number neg. */
    u32_t lqr_period;            /* Reporting period for LQR 1/100ths second */
#ifdef PPP_MULTILINK
    struct epdisc endpoint;      /* endpoint discriminator */
#endif
} lcp_options;

/*
 * Values for phase from BSD pppd.h based on RFC 1661.
 */
typedef enum {
  PHASE_DEAD = 0,
  PHASE_INITIALIZE,
  PHASE_ESTABLISH,
  PHASE_AUTHENTICATE,
  PHASE_CALLBACK,
  PHASE_NETWORK,
  PHASE_TERMINATE
} LinkPhase;



extern LinkPhase lcp_phase[NUM_PPP]; /* Phase of link session (RFC 1661) */
extern lcp_options lcp_wantoptions[];
extern lcp_options lcp_gotoptions[];
extern lcp_options lcp_allowoptions[];
extern lcp_options lcp_hisoptions[];
extern ext_accm xmit_accm[];


void lcp_init     (int);
void lcp_open     (int);
void lcp_close    (int, char *);
void lcp_lowerup  (int);
void lcp_lowerdown(int);
void lcp_sprotrej (int, u_char *, int); /* send protocol reject */

extern struct protent lcp_protent;

/* Default number of times we receive our magic number from the peer
   before deciding the link is looped-back. */
#define DEFLOOPBACKFAIL 10

#endif /* LCP_H */
