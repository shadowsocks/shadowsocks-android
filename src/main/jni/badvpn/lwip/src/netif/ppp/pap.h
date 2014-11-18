/*****************************************************************************
* pap.h -  PPP Password Authentication Protocol header file.
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
 * upap.h - User/Password Authentication Protocol definitions.
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

#ifndef PAP_H
#define PAP_H

#if PAP_SUPPORT /* don't build if not configured for use in lwipopts.h */

/*
 * Packet header = Code, id, length.
 */
#define UPAP_HEADERLEN (sizeof (u_char) + sizeof (u_char) + sizeof (u_short))


/*
 * UPAP codes.
 */
#define UPAP_AUTHREQ 1 /* Authenticate-Request */
#define UPAP_AUTHACK 2 /* Authenticate-Ack */
#define UPAP_AUTHNAK 3 /* Authenticate-Nak */

/*
 * Each interface is described by upap structure.
 */
typedef struct upap_state {
  int us_unit;           /* Interface unit number */
  const char *us_user;   /* User */
  int us_userlen;        /* User length */
  const char *us_passwd; /* Password */
  int us_passwdlen;      /* Password length */
  int us_clientstate;    /* Client state */
  int us_serverstate;    /* Server state */
  u_char us_id;          /* Current id */
  int us_timeouttime;    /* Timeout (seconds) for auth-req retrans. */
  int us_transmits;      /* Number of auth-reqs sent */
  int us_maxtransmits;   /* Maximum number of auth-reqs to send */
  int us_reqtimeout;     /* Time to wait for auth-req from peer */
} upap_state;

/*
 * Client states.
 */
#define UPAPCS_INITIAL 0 /* Connection down */
#define UPAPCS_CLOSED  1 /* Connection up, haven't requested auth */
#define UPAPCS_PENDING 2 /* Connection down, have requested auth */
#define UPAPCS_AUTHREQ 3 /* We've sent an Authenticate-Request */
#define UPAPCS_OPEN    4 /* We've received an Ack */
#define UPAPCS_BADAUTH 5 /* We've received a Nak */

/*
 * Server states.
 */
#define UPAPSS_INITIAL 0 /* Connection down */
#define UPAPSS_CLOSED  1 /* Connection up, haven't requested auth */
#define UPAPSS_PENDING 2 /* Connection down, have requested auth */
#define UPAPSS_LISTEN  3 /* Listening for an Authenticate */
#define UPAPSS_OPEN    4 /* We've sent an Ack */
#define UPAPSS_BADAUTH 5 /* We've sent a Nak */


extern upap_state upap[];

void upap_authwithpeer  (int, char *, char *);
void upap_authpeer      (int);

extern struct protent pap_protent;

#endif /* PAP_SUPPORT */

#endif /* PAP_H */
