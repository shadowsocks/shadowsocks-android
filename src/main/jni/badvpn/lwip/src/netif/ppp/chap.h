/*****************************************************************************
* chap.h - Network Challenge Handshake Authentication Protocol header file.
*
* Copyright (c) 2003 by Marc Boucher, Services Informatiques (MBSI) inc.
* portions Copyright (c) 1998 Global Election Systems Inc.
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
* 97-12-03 Guy Lancaster <lancasterg@acm.org>, Global Election Systems Inc.
*   Original built from BSD network code.
******************************************************************************/
/*
 * chap.h - Challenge Handshake Authentication Protocol definitions.
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
 * Copyright (c) 1991 Gregory M. Christy
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the author.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: chap.h,v 1.6 2010/01/24 13:19:34 goldsimon Exp $
 */

#ifndef CHAP_H
#define CHAP_H

/* Code + ID + length */
#define CHAP_HEADERLEN 4

/*
 * CHAP codes.
 */

#define CHAP_DIGEST_MD5      5    /* use MD5 algorithm */
#define MD5_SIGNATURE_SIZE   16   /* 16 bytes in a MD5 message digest */
#define CHAP_MICROSOFT       0x80 /* use Microsoft-compatible alg. */
#define MS_CHAP_RESPONSE_LEN 49   /* Response length for MS-CHAP */

#define CHAP_CHALLENGE       1
#define CHAP_RESPONSE        2
#define CHAP_SUCCESS         3
#define CHAP_FAILURE         4

/*
 *  Challenge lengths (for challenges we send) and other limits.
 */
#define MIN_CHALLENGE_LENGTH 32
#define MAX_CHALLENGE_LENGTH 64
#define MAX_RESPONSE_LENGTH  64 /* sufficient for MD5 or MS-CHAP */

/*
 * Each interface is described by a chap structure.
 */

typedef struct chap_state {
  int unit;                               /* Interface unit number */
  int clientstate;                        /* Client state */
  int serverstate;                        /* Server state */
  u_char challenge[MAX_CHALLENGE_LENGTH]; /* last challenge string sent */
  u_char chal_len;                        /* challenge length */
  u_char chal_id;                         /* ID of last challenge */
  u_char chal_type;                       /* hash algorithm for challenges */
  u_char id;                              /* Current id */
  char *chal_name;                        /* Our name to use with challenge */
  int chal_interval;                      /* Time until we challenge peer again */
  int timeouttime;                        /* Timeout time in seconds */
  int max_transmits;                      /* Maximum # of challenge transmissions */
  int chal_transmits;                     /* Number of transmissions of challenge */
  int resp_transmits;                     /* Number of transmissions of response */
  u_char response[MAX_RESPONSE_LENGTH];   /* Response to send */
  u_char resp_length;                     /* length of response */
  u_char resp_id;                         /* ID for response messages */
  u_char resp_type;                       /* hash algorithm for responses */
  char *resp_name;                        /* Our name to send with response */
} chap_state;


/*
 * Client (peer) states.
 */
#define CHAPCS_INITIAL       0 /* Lower layer down, not opened */
#define CHAPCS_CLOSED        1 /* Lower layer up, not opened */
#define CHAPCS_PENDING       2 /* Auth us to peer when lower up */
#define CHAPCS_LISTEN        3 /* Listening for a challenge */
#define CHAPCS_RESPONSE      4 /* Sent response, waiting for status */
#define CHAPCS_OPEN          5 /* We've received Success */

/*
 * Server (authenticator) states.
 */
#define CHAPSS_INITIAL       0 /* Lower layer down, not opened */
#define CHAPSS_CLOSED        1 /* Lower layer up, not opened */
#define CHAPSS_PENDING       2 /* Auth peer when lower up */
#define CHAPSS_INITIAL_CHAL  3 /* We've sent the first challenge */
#define CHAPSS_OPEN          4 /* We've sent a Success msg */
#define CHAPSS_RECHALLENGE   5 /* We've sent another challenge */
#define CHAPSS_BADAUTH       6 /* We've sent a Failure msg */

extern chap_state chap[];

void ChapAuthWithPeer (int, char *, u_char);
void ChapAuthPeer (int, char *, u_char);

extern struct protent chap_protent;

#endif /* CHAP_H */
