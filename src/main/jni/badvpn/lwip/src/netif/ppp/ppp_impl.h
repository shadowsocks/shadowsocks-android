/*****************************************************************************
* ppp.h - Network Point to Point Protocol header file.
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

#ifndef PPP_IMPL_H
#define PPP_IMPL_H

#include "lwip/opt.h"

#if PPP_SUPPORT /* don't build if not configured for use in lwipopts.h */

#include "ppp.h"
#include "lwip/def.h"
#include "lwip/sio.h"
#include "lwip/stats.h"
#include "lwip/mem.h"
#include "lwip/netif.h"
#include "lwip/sys.h"
#include "lwip/timers.h"

/** Some defines for code we skip compared to the original pppd.
 *  These are just here to minimise the use of the ugly "#if 0". */
#define PPP_ADDITIONAL_CALLBACKS  0

/** Some error checks to test for unsupported code */
#if CBCP_SUPPORT
#error "CBCP is not supported in lwIP PPP"
#endif
#if CCP_SUPPORT
#error "CCP is not supported in lwIP PPP"
#endif

/*
 * pppd.h - PPP daemon global declarations.
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
 */
/*
 * ppp_defs.h - PPP definitions.
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

#define TIMEOUT(f, a, t)    do { sys_untimeout((f), (a)); sys_timeout((t)*1000, (f), (a)); } while(0)
#define UNTIMEOUT(f, a)     sys_untimeout((f), (a))


/*
 * Constants and structures defined by the internet system,
 * Per RFC 790, September 1981, and numerous additions.
 */

/*
 * The basic PPP frame.
 */
#define PPP_HDRLEN      4       /* octets for standard ppp header */
#define PPP_FCSLEN      2       /* octets for FCS */


/*
 * Significant octet values.
 */
#define PPP_ALLSTATIONS 0xff    /* All-Stations broadcast address */
#define PPP_UI          0x03    /* Unnumbered Information */
#define PPP_FLAG        0x7e    /* Flag Sequence */
#define PPP_ESCAPE      0x7d    /* Asynchronous Control Escape */
#define PPP_TRANS       0x20    /* Asynchronous transparency modifier */

/*
 * Protocol field values.
 */
#define PPP_IP          0x21    /* Internet Protocol */
#define PPP_AT          0x29    /* AppleTalk Protocol */
#define PPP_VJC_COMP    0x2d    /* VJ compressed TCP */
#define PPP_VJC_UNCOMP  0x2f    /* VJ uncompressed TCP */
#define PPP_COMP        0xfd    /* compressed packet */
#define PPP_IPCP        0x8021  /* IP Control Protocol */
#define PPP_ATCP        0x8029  /* AppleTalk Control Protocol */
#define PPP_CCP         0x80fd  /* Compression Control Protocol */
#define PPP_LCP         0xc021  /* Link Control Protocol */
#define PPP_PAP         0xc023  /* Password Authentication Protocol */
#define PPP_LQR         0xc025  /* Link Quality Report protocol */
#define PPP_CHAP        0xc223  /* Cryptographic Handshake Auth. Protocol */
#define PPP_CBCP        0xc029  /* Callback Control Protocol */

/*
 * Values for FCS calculations.
 */
#define PPP_INITFCS     0xffff  /* Initial FCS value */
#define PPP_GOODFCS     0xf0b8  /* Good final FCS value */
#define PPP_FCS(fcs, c) (((fcs) >> 8) ^ fcstab[((fcs) ^ (c)) & 0xff])

/*
 * Extended asyncmap - allows any character to be escaped.
 */
typedef u_char  ext_accm[32];

/*
 * What to do with network protocol (NP) packets.
 */
enum NPmode {
  NPMODE_PASS,        /* pass the packet through */
  NPMODE_DROP,        /* silently drop the packet */
  NPMODE_ERROR,       /* return an error */
  NPMODE_QUEUE        /* save it up for later. */
};

/*
 * Inline versions of get/put char/short/long.
 * Pointer is advanced; we assume that both arguments
 * are lvalues and will already be in registers.
 * cp MUST be u_char *.
 */
#define GETCHAR(c, cp) { \
    (c) = *(cp)++; \
}
#define PUTCHAR(c, cp) { \
    *(cp)++ = (u_char) (c); \
}


#define GETSHORT(s, cp) { \
    (s) = *(cp); (cp)++; (s) <<= 8; \
    (s) |= *(cp); (cp)++; \
}
#define PUTSHORT(s, cp) { \
    *(cp)++ = (u_char) ((s) >> 8); \
    *(cp)++ = (u_char) (s & 0xff); \
}

#define GETLONG(l, cp) { \
    (l) = *(cp); (cp)++; (l) <<= 8; \
    (l) |= *(cp); (cp)++; (l) <<= 8; \
    (l) |= *(cp); (cp)++; (l) <<= 8; \
    (l) |= *(cp); (cp)++; \
}
#define PUTLONG(l, cp) { \
    *(cp)++ = (u_char) ((l) >> 24); \
    *(cp)++ = (u_char) ((l) >> 16); \
    *(cp)++ = (u_char) ((l) >> 8); \
    *(cp)++ = (u_char) (l); \
}


#define INCPTR(n, cp)   ((cp) += (n))
#define DECPTR(n, cp)   ((cp) -= (n))

#define BCMP(s0, s1, l)     memcmp((u_char *)(s0), (u_char *)(s1), (l))
#define BCOPY(s, d, l)      MEMCPY((d), (s), (l))
#define BZERO(s, n)         memset(s, 0, n)

#if PPP_DEBUG
#define PRINTMSG(m, l)  { m[l] = '\0'; LWIP_DEBUGF(LOG_INFO, ("Remote message: %s\n", m)); }
#else  /* PPP_DEBUG */
#define PRINTMSG(m, l)
#endif /* PPP_DEBUG */

/*
 * MAKEHEADER - Add PPP Header fields to a packet.
 */
#define MAKEHEADER(p, t) { \
    PUTCHAR(PPP_ALLSTATIONS, p); \
    PUTCHAR(PPP_UI, p); \
    PUTSHORT(t, p); }

/************************
*** PUBLIC DATA TYPES ***
************************/

/*
 * The following struct gives the addresses of procedures to call
 * for a particular protocol.
 */
struct protent {
    u_short protocol;       /* PPP protocol number */
    /* Initialization procedure */
    void (*init) (int unit);
    /* Process a received packet */
    void (*input) (int unit, u_char *pkt, int len);
    /* Process a received protocol-reject */
    void (*protrej) (int unit);
    /* Lower layer has come up */
    void (*lowerup) (int unit);
    /* Lower layer has gone down */
    void (*lowerdown) (int unit);
    /* Open the protocol */
    void (*open) (int unit);
    /* Close the protocol */
    void (*close) (int unit, char *reason);
#if PPP_ADDITIONAL_CALLBACKS
    /* Print a packet in readable form */
    int  (*printpkt) (u_char *pkt, int len,
              void (*printer) (void *, char *, ...),
              void *arg);
    /* Process a received data packet */
    void (*datainput) (int unit, u_char *pkt, int len);
#endif /* PPP_ADDITIONAL_CALLBACKS */
    int  enabled_flag;      /* 0 if protocol is disabled */
    char *name;         /* Text name of protocol */
#if PPP_ADDITIONAL_CALLBACKS
    /* Check requested options, assign defaults */
    void (*check_options) (u_long);
    /* Configure interface for demand-dial */
    int  (*demand_conf) (int unit);
    /* Say whether to bring up link for this pkt */
    int  (*active_pkt) (u_char *pkt, int len);
#endif /* PPP_ADDITIONAL_CALLBACKS */
};

/*
 * The following structure records the time in seconds since
 * the last NP packet was sent or received.
 */
struct ppp_idle {
  u_short xmit_idle;      /* seconds since last NP packet sent */
  u_short recv_idle;      /* seconds since last NP packet received */
};

struct ppp_settings {

  u_int  disable_defaultip : 1;       /* Don't use hostname for default IP addrs */
  u_int  auth_required     : 1;       /* Peer is required to authenticate */
  u_int  explicit_remote   : 1;       /* remote_name specified with remotename opt */
  u_int  refuse_pap        : 1;       /* Don't wanna auth. ourselves with PAP */
  u_int  refuse_chap       : 1;       /* Don't wanna auth. ourselves with CHAP */
  u_int  usehostname       : 1;       /* Use hostname for our_name */
  u_int  usepeerdns        : 1;       /* Ask peer for DNS adds */

  u_short idle_time_limit;            /* Shut down link if idle for this long */
  int  maxconnect;                    /* Maximum connect time (seconds) */

  char user       [MAXNAMELEN   + 1]; /* Username for PAP */
  char passwd     [MAXSECRETLEN + 1]; /* Password for PAP, secret for CHAP */
  char our_name   [MAXNAMELEN   + 1]; /* Our name for authentication purposes */
  char remote_name[MAXNAMELEN   + 1]; /* Peer's name for authentication */
};

/*****************************
*** PUBLIC DATA STRUCTURES ***
*****************************/

/* Buffers for outgoing packets. */
extern u_char outpacket_buf[NUM_PPP][PPP_MRU+PPP_HDRLEN];

extern struct ppp_settings ppp_settings;

extern struct protent *ppp_protocols[]; /* Table of pointers to supported protocols */


/***********************
*** PUBLIC FUNCTIONS ***
***********************/

/*
 * Write n characters to a ppp link.
 * RETURN: >= 0 Number of characters written, -1 Failed to write to device.
 */
int pppWrite(int pd, const u_char *s, int n);

void pppInProcOverEthernet(int pd, struct pbuf *pb);

struct pbuf *pppSingleBuf(struct pbuf *p);

void pppLinkTerminated(int pd);

void pppLinkDown(int pd);

/* Configure i/f transmit parameters */
void ppp_send_config (int, u16_t, u32_t, int, int);
/* Set extended transmit ACCM */
void ppp_set_xaccm (int, ext_accm *);
/* Configure i/f receive parameters */
void ppp_recv_config (int, int, u32_t, int, int);
/* Find out how long link has been idle */
int  get_idle_time (int, struct ppp_idle *);

/* Configure VJ TCP header compression */
int  sifvjcomp (int, int, u8_t, u8_t);
/* Configure i/f down (for IP) */
int  sifup (int);
/* Set mode for handling packets for proto */
int  sifnpmode (int u, int proto, enum NPmode mode);
/* Configure i/f down (for IP) */
int  sifdown (int);
/* Configure IP addresses for i/f */
int  sifaddr (int, u32_t, u32_t, u32_t, u32_t, u32_t);
/* Reset i/f IP addresses */
int  cifaddr (int, u32_t, u32_t);
/* Create default route through i/f */
int  sifdefaultroute (int, u32_t, u32_t);
/* Delete default route through i/f */
int  cifdefaultroute (int, u32_t, u32_t);

/* Get appropriate netmask for address */
u32_t GetMask (u32_t); 

#endif /* PPP_SUPPORT */

#endif /* PPP_IMPL_H */
