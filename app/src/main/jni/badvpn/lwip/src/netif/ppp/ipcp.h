/*****************************************************************************
* ipcp.h -  PPP IP NCP: Internet Protocol Network Control Protocol header file.
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
* 97-12-04 Guy Lancaster <glanca@gesn.com>, Global Election Systems Inc.
*   Original derived from BSD codes.
*****************************************************************************/
/*
 * ipcp.h - IP Control Protocol definitions.
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
 * $Id: ipcp.h,v 1.4 2010/01/18 20:49:43 goldsimon Exp $
 */

#ifndef IPCP_H
#define IPCP_H

/*
 * Options.
 */
#define CI_ADDRS            1      /* IP Addresses */
#define CI_COMPRESSTYPE     2      /* Compression Type */
#define CI_ADDR             3

#define CI_MS_DNS1          129    /* Primary DNS value */
#define CI_MS_WINS1         128    /* Primary WINS value */
#define CI_MS_DNS2          131    /* Secondary DNS value */
#define CI_MS_WINS2         130    /* Secondary WINS value */

#define IPCP_VJMODE_OLD     1      /* "old" mode (option # = 0x0037) */
#define IPCP_VJMODE_RFC1172 2      /* "old-rfc"mode (option # = 0x002d) */
#define IPCP_VJMODE_RFC1332 3      /* "new-rfc"mode (option # = 0x002d, */
                                   /*  maxslot and slot number compression) */

#define IPCP_VJ_COMP        0x002d /* current value for VJ compression option */
#define IPCP_VJ_COMP_OLD    0x0037 /* "old" (i.e, broken) value for VJ */
                                   /* compression option */ 

typedef struct ipcp_options {
  u_int   neg_addr      : 1; /* Negotiate IP Address? */
  u_int   old_addrs     : 1; /* Use old (IP-Addresses) option? */
  u_int   req_addr      : 1; /* Ask peer to send IP address? */
  u_int   default_route : 1; /* Assign default route through interface? */
  u_int   proxy_arp     : 1; /* Make proxy ARP entry for peer? */
  u_int   neg_vj        : 1; /* Van Jacobson Compression? */
  u_int   old_vj        : 1; /* use old (short) form of VJ option? */
  u_int   accept_local  : 1; /* accept peer's value for ouraddr */
  u_int   accept_remote : 1; /* accept peer's value for hisaddr */
  u_int   req_dns1      : 1; /* Ask peer to send primary DNS address? */
  u_int   req_dns2      : 1; /* Ask peer to send secondary DNS address? */
  u_short vj_protocol;       /* protocol value to use in VJ option */
  u_char  maxslotindex;      /* VJ slots - 1. */
  u_char  cflag;             /* VJ slot compression flag. */
  u32_t   ouraddr, hisaddr;  /* Addresses in NETWORK BYTE ORDER */
  u32_t   dnsaddr[2];        /* Primary and secondary MS DNS entries */
  u32_t   winsaddr[2];       /* Primary and secondary MS WINS entries */
} ipcp_options;

extern fsm ipcp_fsm[];
extern ipcp_options ipcp_wantoptions[];
extern ipcp_options ipcp_gotoptions[];
extern ipcp_options ipcp_allowoptions[];
extern ipcp_options ipcp_hisoptions[];

extern struct protent ipcp_protent;

#endif /* IPCP_H */
