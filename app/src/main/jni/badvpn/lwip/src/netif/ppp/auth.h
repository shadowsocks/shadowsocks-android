/*****************************************************************************
* auth.h -  PPP Authentication and phase control header file.
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
* 97-12-04 Guy Lancaster <lancasterg@acm.org>, Global Election Systems Inc.
*   Original derived from BSD pppd.h.
*****************************************************************************/
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

#ifndef AUTH_H
#define AUTH_H

/***********************
*** PUBLIC FUNCTIONS ***
***********************/

/* we are starting to use the link */
void link_required (int);

/* we are finished with the link */
void link_terminated (int);

/* the LCP layer has left the Opened state */
void link_down (int);

/* the link is up; authenticate now */
void link_established (int);

/* a network protocol has come up */
void np_up (int, u16_t);

/* a network protocol has gone down */
void np_down (int, u16_t);

/* a network protocol no longer needs link */
void np_finished (int, u16_t);

/* peer failed to authenticate itself */
void auth_peer_fail (int, u16_t);

/* peer successfully authenticated itself */
void auth_peer_success (int, u16_t, char *, int);

/* we failed to authenticate ourselves */
void auth_withpeer_fail (int, u16_t);

/* we successfully authenticated ourselves */
void auth_withpeer_success (int, u16_t);

/* check authentication options supplied */
void auth_check_options (void);

/* check what secrets we have */
void auth_reset (int);

/* Check peer-supplied username/password */
u_char check_passwd (int, char *, int, char *, int, char **, int *);

/* get "secret" for chap */
int  get_secret (int, char *, char *, char *, int *, int);

/* check if IP address is authorized */
int  auth_ip_addr (int, u32_t);

/* check if IP address is unreasonable */
int  bad_ip_adrs (u32_t);

#endif /* AUTH_H */
