/*****************************************************************************
* fsm.h - Network Control Protocol Finite State Machine header file.
*
* Copyright (c) 2003 by Marc Boucher, Services Informatiques (MBSI) inc.
* Copyright (c) 1997 Global Election Systems Inc.
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
*   Original based on BSD code.
*****************************************************************************/
/*
 * fsm.h - {Link, IP} Control Protocol Finite State Machine definitions.
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
 * $Id: fsm.h,v 1.5 2009/12/31 17:08:08 goldsimon Exp $
 */

#ifndef FSM_H
#define FSM_H

/*
 * LCP Packet header = Code, id, length.
 */
#define HEADERLEN (sizeof (u_char) + sizeof (u_char) + sizeof (u_short))


/*
 *  CP (LCP, IPCP, etc.) codes.
 */
#define CONFREQ     1 /* Configuration Request */
#define CONFACK     2 /* Configuration Ack */
#define CONFNAK     3 /* Configuration Nak */
#define CONFREJ     4 /* Configuration Reject */
#define TERMREQ     5 /* Termination Request */
#define TERMACK     6 /* Termination Ack */
#define CODEREJ     7 /* Code Reject */


/*
 * Each FSM is described by an fsm structure and fsm callbacks.
 */
typedef struct fsm {
  int unit;                        /* Interface unit number */
  u_short protocol;                /* Data Link Layer Protocol field value */
  int state;                       /* State */
  int flags;                       /* Contains option bits */
  u_char id;                       /* Current id */
  u_char reqid;                    /* Current request id */
  u_char seen_ack;                 /* Have received valid Ack/Nak/Rej to Req */
  int timeouttime;                 /* Timeout time in milliseconds */
  int maxconfreqtransmits;         /* Maximum Configure-Request transmissions */
  int retransmits;                 /* Number of retransmissions left */
  int maxtermtransmits;            /* Maximum Terminate-Request transmissions */
  int nakloops;                    /* Number of nak loops since last ack */
  int maxnakloops;                 /* Maximum number of nak loops tolerated */
  struct fsm_callbacks* callbacks; /* Callback routines */
  char* term_reason;               /* Reason for closing protocol */
  int term_reason_len;             /* Length of term_reason */
} fsm;


typedef struct fsm_callbacks {
  void (*resetci)(fsm*);                            /* Reset our Configuration Information */
  int  (*cilen)(fsm*);                              /* Length of our Configuration Information */
  void (*addci)(fsm*, u_char*, int*);               /* Add our Configuration Information */
  int  (*ackci)(fsm*, u_char*, int);                /* ACK our Configuration Information */
  int  (*nakci)(fsm*, u_char*, int);                /* NAK our Configuration Information */
  int  (*rejci)(fsm*, u_char*, int);                /* Reject our Configuration Information */
  int  (*reqci)(fsm*, u_char*, int*, int);          /* Request peer's Configuration Information */
  void (*up)(fsm*);                                 /* Called when fsm reaches LS_OPENED state */
  void (*down)(fsm*);                               /* Called when fsm leaves LS_OPENED state */
  void (*starting)(fsm*);                           /* Called when we want the lower layer */
  void (*finished)(fsm*);                           /* Called when we don't want the lower layer */
  void (*protreject)(int);                          /* Called when Protocol-Reject received */
  void (*retransmit)(fsm*);                         /* Retransmission is necessary */
  int  (*extcode)(fsm*, int, u_char, u_char*, int); /* Called when unknown code received */
  char *proto_name;                                 /* String name for protocol (for messages) */
} fsm_callbacks;


/*
 * Link states.
 */
#define LS_INITIAL  0 /* Down, hasn't been opened */
#define LS_STARTING 1 /* Down, been opened */
#define LS_CLOSED   2 /* Up, hasn't been opened */
#define LS_STOPPED  3 /* Open, waiting for down event */
#define LS_CLOSING  4 /* Terminating the connection, not open */
#define LS_STOPPING 5 /* Terminating, but open */
#define LS_REQSENT  6 /* We've sent a Config Request */
#define LS_ACKRCVD  7 /* We've received a Config Ack */
#define LS_ACKSENT  8 /* We've sent a Config Ack */
#define LS_OPENED   9 /* Connection available */

/*
 * Flags - indicate options controlling FSM operation
 */
#define OPT_PASSIVE 1 /* Don't die if we don't get a response */
#define OPT_RESTART 2 /* Treat 2nd OPEN as DOWN, UP */
#define OPT_SILENT  4 /* Wait for peer to speak first */


/*
 * Prototypes
 */
void fsm_init (fsm*);
void fsm_lowerup (fsm*);
void fsm_lowerdown (fsm*);
void fsm_open (fsm*);
void fsm_close (fsm*, char*);
void fsm_input (fsm*, u_char*, int);
void fsm_protreject (fsm*);
void fsm_sdata (fsm*, u_char, u_char, u_char*, int);


/*
 * Variables
 */
extern int peer_mru[]; /* currently negotiated peer MRU (per unit) */

#endif /* FSM_H */
