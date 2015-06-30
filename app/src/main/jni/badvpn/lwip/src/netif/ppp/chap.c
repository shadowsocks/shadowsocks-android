/*** WARNING - THIS HAS NEVER BEEN FINISHED ***/
/*****************************************************************************
* chap.c - Network Challenge Handshake Authentication Protocol program file.
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
* 97-12-04 Guy Lancaster <lancasterg@acm.org>, Global Election Systems Inc.
*   Original based on BSD chap.c.
*****************************************************************************/
/*
 * chap.c - Challenge Handshake Authentication Protocol.
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
 * Copyright (c) 1991 Gregory M. Christy.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Gregory M. Christy.  The name of the author may not be used to
 * endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "lwip/opt.h"

#if PPP_SUPPORT  /* don't build if not configured for use in lwipopts.h */

#if CHAP_SUPPORT /* don't build if not configured for use in lwipopts.h */

#include "ppp_impl.h"
#include "pppdebug.h"

#include "magic.h"
#include "randm.h"
#include "auth.h"
#include "md5.h"
#include "chap.h"
#include "chpms.h"

#include <string.h>

#if 0 /* UNUSED */
/*
 * Command-line options.
 */
static option_t chap_option_list[] = {
    { "chap-restart", o_int, &chap[0].timeouttime,
      "Set timeout for CHAP" },
    { "chap-max-challenge", o_int, &chap[0].max_transmits,
      "Set max #xmits for challenge" },
    { "chap-interval", o_int, &chap[0].chal_interval,
      "Set interval for rechallenge" },
#ifdef MSLANMAN
    { "ms-lanman", o_bool, &ms_lanman,
      "Use LanMan passwd when using MS-CHAP", 1 },
#endif
    { NULL }
};
#endif /* UNUSED */

/*
 * Protocol entry points.
 */
static void ChapInit (int);
static void ChapLowerUp (int);
static void ChapLowerDown (int);
static void ChapInput (int, u_char *, int);
static void ChapProtocolReject (int);
#if PPP_ADDITIONAL_CALLBACKS
static int  ChapPrintPkt (u_char *, int, void (*) (void *, char *, ...), void *);
#endif

struct protent chap_protent = {
  PPP_CHAP,
  ChapInit,
  ChapInput,
  ChapProtocolReject,
  ChapLowerUp,
  ChapLowerDown,
  NULL,
  NULL,
#if PPP_ADDITIONAL_CALLBACKS
  ChapPrintPkt,
  NULL,
#endif /* PPP_ADDITIONAL_CALLBACKS */
  1,
  "CHAP",
#if PPP_ADDITIONAL_CALLBACKS
  NULL,
  NULL,
  NULL
#endif /* PPP_ADDITIONAL_CALLBACKS */
};

chap_state chap[NUM_PPP]; /* CHAP state; one for each unit */

static void ChapChallengeTimeout (void *);
static void ChapResponseTimeout (void *);
static void ChapReceiveChallenge (chap_state *, u_char *, u_char, int);
static void ChapRechallenge (void *);
static void ChapReceiveResponse (chap_state *, u_char *, int, int);
static void ChapReceiveSuccess(chap_state *cstate, u_char *inp, u_char id, int len);
static void ChapReceiveFailure(chap_state *cstate, u_char *inp, u_char id, int len);
static void ChapSendStatus (chap_state *, int);
static void ChapSendChallenge (chap_state *);
static void ChapSendResponse (chap_state *);
static void ChapGenChallenge (chap_state *);

/*
 * ChapInit - Initialize a CHAP unit.
 */
static void
ChapInit(int unit)
{
  chap_state *cstate = &chap[unit];

  BZERO(cstate, sizeof(*cstate));
  cstate->unit = unit;
  cstate->clientstate = CHAPCS_INITIAL;
  cstate->serverstate = CHAPSS_INITIAL;
  cstate->timeouttime = CHAP_DEFTIMEOUT;
  cstate->max_transmits = CHAP_DEFTRANSMITS;
  /* random number generator is initialized in magic_init */
}


/*
 * ChapAuthWithPeer - Authenticate us with our peer (start client).
 *
 */
void
ChapAuthWithPeer(int unit, char *our_name, u_char digest)
{
  chap_state *cstate = &chap[unit];

  cstate->resp_name = our_name;
  cstate->resp_type = digest;

  if (cstate->clientstate == CHAPCS_INITIAL ||
      cstate->clientstate == CHAPCS_PENDING) {
    /* lower layer isn't up - wait until later */
    cstate->clientstate = CHAPCS_PENDING;
    return;
  }

  /*
   * We get here as a result of LCP coming up.
   * So even if CHAP was open before, we will 
   * have to re-authenticate ourselves.
   */
  cstate->clientstate = CHAPCS_LISTEN;
}


/*
 * ChapAuthPeer - Authenticate our peer (start server).
 */
void
ChapAuthPeer(int unit, char *our_name, u_char digest)
{
  chap_state *cstate = &chap[unit];

  cstate->chal_name = our_name;
  cstate->chal_type = digest;
  
  if (cstate->serverstate == CHAPSS_INITIAL ||
      cstate->serverstate == CHAPSS_PENDING) {
    /* lower layer isn't up - wait until later */
    cstate->serverstate = CHAPSS_PENDING;
    return;
  }

  ChapGenChallenge(cstate);
  ChapSendChallenge(cstate);    /* crank it up dude! */
  cstate->serverstate = CHAPSS_INITIAL_CHAL;
}


/*
 * ChapChallengeTimeout - Timeout expired on sending challenge.
 */
static void
ChapChallengeTimeout(void *arg)
{
  chap_state *cstate = (chap_state *) arg;

  /* if we aren't sending challenges, don't worry.  then again we */
  /* probably shouldn't be here either */
  if (cstate->serverstate != CHAPSS_INITIAL_CHAL &&
      cstate->serverstate != CHAPSS_RECHALLENGE) {
    return;
  }

  if (cstate->chal_transmits >= cstate->max_transmits) {
    /* give up on peer */
    CHAPDEBUG(LOG_ERR, ("Peer failed to respond to CHAP challenge\n"));
    cstate->serverstate = CHAPSS_BADAUTH;
    auth_peer_fail(cstate->unit, PPP_CHAP);
    return;
  }

  ChapSendChallenge(cstate); /* Re-send challenge */
}


/*
 * ChapResponseTimeout - Timeout expired on sending response.
 */
static void
ChapResponseTimeout(void *arg)
{
  chap_state *cstate = (chap_state *) arg;

  /* if we aren't sending a response, don't worry. */
  if (cstate->clientstate != CHAPCS_RESPONSE) {
    return;
  }

  ChapSendResponse(cstate);    /* re-send response */
}


/*
 * ChapRechallenge - Time to challenge the peer again.
 */
static void
ChapRechallenge(void *arg)
{
  chap_state *cstate = (chap_state *) arg;
  
  /* if we aren't sending a response, don't worry. */
  if (cstate->serverstate != CHAPSS_OPEN) {
    return;
  }

  ChapGenChallenge(cstate);
  ChapSendChallenge(cstate);
  cstate->serverstate = CHAPSS_RECHALLENGE;
}


/*
 * ChapLowerUp - The lower layer is up.
 *
 * Start up if we have pending requests.
 */
static void
ChapLowerUp(int unit)
{
  chap_state *cstate = &chap[unit];

  if (cstate->clientstate == CHAPCS_INITIAL) {
    cstate->clientstate = CHAPCS_CLOSED;
  } else if (cstate->clientstate == CHAPCS_PENDING) {
    cstate->clientstate = CHAPCS_LISTEN;
  }

  if (cstate->serverstate == CHAPSS_INITIAL) {
    cstate->serverstate = CHAPSS_CLOSED;
  } else if (cstate->serverstate == CHAPSS_PENDING) {
    ChapGenChallenge(cstate);
    ChapSendChallenge(cstate);
    cstate->serverstate = CHAPSS_INITIAL_CHAL;
  }
}


/*
 * ChapLowerDown - The lower layer is down.
 *
 * Cancel all timeouts.
 */
static void
ChapLowerDown(int unit)
{
  chap_state *cstate = &chap[unit];

  /* Timeout(s) pending?  Cancel if so. */
  if (cstate->serverstate == CHAPSS_INITIAL_CHAL ||
      cstate->serverstate == CHAPSS_RECHALLENGE) {
    UNTIMEOUT(ChapChallengeTimeout, cstate);
  } else if (cstate->serverstate == CHAPSS_OPEN
      && cstate->chal_interval != 0) {
    UNTIMEOUT(ChapRechallenge, cstate);
  }
  if (cstate->clientstate == CHAPCS_RESPONSE) {
    UNTIMEOUT(ChapResponseTimeout, cstate);
  }
  cstate->clientstate = CHAPCS_INITIAL;
  cstate->serverstate = CHAPSS_INITIAL;
}


/*
 * ChapProtocolReject - Peer doesn't grok CHAP.
 */
static void
ChapProtocolReject(int unit)
{
  chap_state *cstate = &chap[unit];
  
  if (cstate->serverstate != CHAPSS_INITIAL &&
      cstate->serverstate != CHAPSS_CLOSED) {
    auth_peer_fail(unit, PPP_CHAP);
  }
  if (cstate->clientstate != CHAPCS_INITIAL &&
      cstate->clientstate != CHAPCS_CLOSED) {
    auth_withpeer_fail(unit, PPP_CHAP); /* lwip: just sets the PPP error code on this unit to PPPERR_AUTHFAIL */
  }
  ChapLowerDown(unit); /* shutdown chap */
}


/*
 * ChapInput - Input CHAP packet.
 */
static void
ChapInput(int unit, u_char *inpacket, int packet_len)
{
  chap_state *cstate = &chap[unit];
  u_char *inp;
  u_char code, id;
  int len;
  
  /*
   * Parse header (code, id and length).
   * If packet too short, drop it.
   */
  inp = inpacket;
  if (packet_len < CHAP_HEADERLEN) {
    CHAPDEBUG(LOG_INFO, ("ChapInput: rcvd short header.\n"));
    return;
  }
  GETCHAR(code, inp);
  GETCHAR(id, inp);
  GETSHORT(len, inp);
  if (len < CHAP_HEADERLEN) {
    CHAPDEBUG(LOG_INFO, ("ChapInput: rcvd illegal length.\n"));
    return;
  }
  if (len > packet_len) {
    CHAPDEBUG(LOG_INFO, ("ChapInput: rcvd short packet.\n"));
    return;
  }
  len -= CHAP_HEADERLEN;
  
  /*
   * Action depends on code (as in fact it usually does :-).
   */
  switch (code) {
    case CHAP_CHALLENGE:
      ChapReceiveChallenge(cstate, inp, id, len);
      break;
    
    case CHAP_RESPONSE:
      ChapReceiveResponse(cstate, inp, id, len);
      break;
    
    case CHAP_FAILURE:
      ChapReceiveFailure(cstate, inp, id, len);
      break;
    
    case CHAP_SUCCESS:
      ChapReceiveSuccess(cstate, inp, id, len);
      break;
    
    default:        /* Need code reject? */
      CHAPDEBUG(LOG_WARNING, ("Unknown CHAP code (%d) received.\n", code));
      break;
  }
}


/*
 * ChapReceiveChallenge - Receive Challenge and send Response.
 */
static void
ChapReceiveChallenge(chap_state *cstate, u_char *inp, u_char id, int len)
{
  int rchallenge_len;
  u_char *rchallenge;
  int secret_len;
  char secret[MAXSECRETLEN];
  char rhostname[256];
  MD5_CTX mdContext;
  u_char hash[MD5_SIGNATURE_SIZE];

  CHAPDEBUG(LOG_INFO, ("ChapReceiveChallenge: Rcvd id %d.\n", id));
  if (cstate->clientstate == CHAPCS_CLOSED ||
    cstate->clientstate == CHAPCS_PENDING) {
    CHAPDEBUG(LOG_INFO, ("ChapReceiveChallenge: in state %d\n",
         cstate->clientstate));
    return;
  }

  if (len < 2) {
    CHAPDEBUG(LOG_INFO, ("ChapReceiveChallenge: rcvd short packet.\n"));
    return;
  }

  GETCHAR(rchallenge_len, inp);
  len -= sizeof (u_char) + rchallenge_len;  /* now name field length */
  if (len < 0) {
    CHAPDEBUG(LOG_INFO, ("ChapReceiveChallenge: rcvd short packet.\n"));
    return;
  }
  rchallenge = inp;
  INCPTR(rchallenge_len, inp);

  if (len >= (int)sizeof(rhostname)) {
    len = sizeof(rhostname) - 1;
  }
  BCOPY(inp, rhostname, len);
  rhostname[len] = '\000';

  CHAPDEBUG(LOG_INFO, ("ChapReceiveChallenge: received name field '%s'\n",
             rhostname));

  /* Microsoft doesn't send their name back in the PPP packet */
  if (ppp_settings.remote_name[0] != 0 && (ppp_settings.explicit_remote || rhostname[0] == 0)) {
    strncpy(rhostname, ppp_settings.remote_name, sizeof(rhostname));
    rhostname[sizeof(rhostname) - 1] = 0;
    CHAPDEBUG(LOG_INFO, ("ChapReceiveChallenge: using '%s' as remote name\n",
               rhostname));
  }

  /* get secret for authenticating ourselves with the specified host */
  if (!get_secret(cstate->unit, cstate->resp_name, rhostname,
                  secret, &secret_len, 0)) {
    secret_len = 0;    /* assume null secret if can't find one */
    CHAPDEBUG(LOG_WARNING, ("No CHAP secret found for authenticating us to %s\n",
               rhostname));
  }

  /* cancel response send timeout if necessary */
  if (cstate->clientstate == CHAPCS_RESPONSE) {
    UNTIMEOUT(ChapResponseTimeout, cstate);
  }

  cstate->resp_id = id;
  cstate->resp_transmits = 0;

  /*  generate MD based on negotiated type */
  switch (cstate->resp_type) { 

  case CHAP_DIGEST_MD5:
    MD5Init(&mdContext);
    MD5Update(&mdContext, &cstate->resp_id, 1);
    MD5Update(&mdContext, (u_char*)secret, secret_len);
    MD5Update(&mdContext, rchallenge, rchallenge_len);
    MD5Final(hash, &mdContext);
    BCOPY(hash, cstate->response, MD5_SIGNATURE_SIZE);
    cstate->resp_length = MD5_SIGNATURE_SIZE;
    break;
  
#if MSCHAP_SUPPORT
  case CHAP_MICROSOFT:
    ChapMS(cstate, rchallenge, rchallenge_len, secret, secret_len);
    break;
#endif

  default:
    CHAPDEBUG(LOG_INFO, ("unknown digest type %d\n", cstate->resp_type));
    return;
  }

  BZERO(secret, sizeof(secret));
  ChapSendResponse(cstate);
}


/*
 * ChapReceiveResponse - Receive and process response.
 */
static void
ChapReceiveResponse(chap_state *cstate, u_char *inp, int id, int len)
{
  u_char *remmd, remmd_len;
  int secret_len, old_state;
  int code;
  char rhostname[256];
  MD5_CTX mdContext;
  char secret[MAXSECRETLEN];
  u_char hash[MD5_SIGNATURE_SIZE];

  CHAPDEBUG(LOG_INFO, ("ChapReceiveResponse: Rcvd id %d.\n", id));
  
  if (cstate->serverstate == CHAPSS_CLOSED ||
      cstate->serverstate == CHAPSS_PENDING) {
    CHAPDEBUG(LOG_INFO, ("ChapReceiveResponse: in state %d\n",
    cstate->serverstate));
    return;
  }

  if (id != cstate->chal_id) {
    return;      /* doesn't match ID of last challenge */
  }

  /*
  * If we have received a duplicate or bogus Response,
  * we have to send the same answer (Success/Failure)
  * as we did for the first Response we saw.
  */
  if (cstate->serverstate == CHAPSS_OPEN) {
    ChapSendStatus(cstate, CHAP_SUCCESS);
    return;
  }
  if (cstate->serverstate == CHAPSS_BADAUTH) {
    ChapSendStatus(cstate, CHAP_FAILURE);
    return;
  }
  
  if (len < 2) {
    CHAPDEBUG(LOG_INFO, ("ChapReceiveResponse: rcvd short packet.\n"));
    return;
  }
  GETCHAR(remmd_len, inp); /* get length of MD */
  remmd = inp;             /* get pointer to MD */
  INCPTR(remmd_len, inp);
  
  len -= sizeof (u_char) + remmd_len;
  if (len < 0) {
    CHAPDEBUG(LOG_INFO, ("ChapReceiveResponse: rcvd short packet.\n"));
    return;
  }

  UNTIMEOUT(ChapChallengeTimeout, cstate);
  
  if (len >= (int)sizeof(rhostname)) {
    len = sizeof(rhostname) - 1;
  }
  BCOPY(inp, rhostname, len);
  rhostname[len] = '\000';

  CHAPDEBUG(LOG_INFO, ("ChapReceiveResponse: received name field: %s\n",
             rhostname));

  /*
  * Get secret for authenticating them with us,
  * do the hash ourselves, and compare the result.
  */
  code = CHAP_FAILURE;
  if (!get_secret(cstate->unit, rhostname, cstate->chal_name,
                  secret, &secret_len, 1)) {
    CHAPDEBUG(LOG_WARNING, ("No CHAP secret found for authenticating %s\n",
               rhostname));
  } else {
    /*  generate MD based on negotiated type */
    switch (cstate->chal_type) {

      case CHAP_DIGEST_MD5:    /* only MD5 is defined for now */
        if (remmd_len != MD5_SIGNATURE_SIZE) {
          break;      /* it's not even the right length */
        }
        MD5Init(&mdContext);
        MD5Update(&mdContext, &cstate->chal_id, 1);
        MD5Update(&mdContext, (u_char*)secret, secret_len);
        MD5Update(&mdContext, cstate->challenge, cstate->chal_len);
        MD5Final(hash, &mdContext); 
        
        /* compare local and remote MDs and send the appropriate status */
        if (memcmp (hash, remmd, MD5_SIGNATURE_SIZE) == 0) {
          code = CHAP_SUCCESS;  /* they are the same! */
        }
        break;
      
      default:
        CHAPDEBUG(LOG_INFO, ("unknown digest type %d\n", cstate->chal_type));
    }
  }
  
  BZERO(secret, sizeof(secret));
  ChapSendStatus(cstate, code);

  if (code == CHAP_SUCCESS) {
    old_state = cstate->serverstate;
    cstate->serverstate = CHAPSS_OPEN;
    if (old_state == CHAPSS_INITIAL_CHAL) {
      auth_peer_success(cstate->unit, PPP_CHAP, rhostname, len);
    }
    if (cstate->chal_interval != 0) {
      TIMEOUT(ChapRechallenge, cstate, cstate->chal_interval);
    }
  } else {
    CHAPDEBUG(LOG_ERR, ("CHAP peer authentication failed\n"));
    cstate->serverstate = CHAPSS_BADAUTH;
    auth_peer_fail(cstate->unit, PPP_CHAP);
  }
}

/*
 * ChapReceiveSuccess - Receive Success
 */
static void
ChapReceiveSuccess(chap_state *cstate, u_char *inp, u_char id, int len)
{
  LWIP_UNUSED_ARG(id);
  LWIP_UNUSED_ARG(inp);

  CHAPDEBUG(LOG_INFO, ("ChapReceiveSuccess: Rcvd id %d.\n", id));

  if (cstate->clientstate == CHAPCS_OPEN) {
    /* presumably an answer to a duplicate response */
    return;
  }

  if (cstate->clientstate != CHAPCS_RESPONSE) {
    /* don't know what this is */
    CHAPDEBUG(LOG_INFO, ("ChapReceiveSuccess: in state %d\n",
               cstate->clientstate));
    return;
  }
  
  UNTIMEOUT(ChapResponseTimeout, cstate);
  
  /*
   * Print message.
   */
  if (len > 0) {
    PRINTMSG(inp, len);
  }

  cstate->clientstate = CHAPCS_OPEN;

  auth_withpeer_success(cstate->unit, PPP_CHAP);
}


/*
 * ChapReceiveFailure - Receive failure.
 */
static void
ChapReceiveFailure(chap_state *cstate, u_char *inp, u_char id, int len)
{
  LWIP_UNUSED_ARG(id);
  LWIP_UNUSED_ARG(inp);

  CHAPDEBUG(LOG_INFO, ("ChapReceiveFailure: Rcvd id %d.\n", id));

  if (cstate->clientstate != CHAPCS_RESPONSE) {
    /* don't know what this is */
    CHAPDEBUG(LOG_INFO, ("ChapReceiveFailure: in state %d\n",
               cstate->clientstate));
    return;
  }

  UNTIMEOUT(ChapResponseTimeout, cstate);

  /*
   * Print message.
   */
  if (len > 0) {
    PRINTMSG(inp, len);
  }

  CHAPDEBUG(LOG_ERR, ("CHAP authentication failed\n"));
  auth_withpeer_fail(cstate->unit, PPP_CHAP); /* lwip: just sets the PPP error code on this unit to PPPERR_AUTHFAIL */
}


/*
 * ChapSendChallenge - Send an Authenticate challenge.
 */
static void
ChapSendChallenge(chap_state *cstate)
{
  u_char *outp;
  int chal_len, name_len;
  int outlen;
  
  chal_len = cstate->chal_len;
  name_len = (int)strlen(cstate->chal_name);
  outlen = CHAP_HEADERLEN + sizeof (u_char) + chal_len + name_len;
  outp = outpacket_buf[cstate->unit];

  MAKEHEADER(outp, PPP_CHAP);    /* paste in a CHAP header */

  PUTCHAR(CHAP_CHALLENGE, outp);
  PUTCHAR(cstate->chal_id, outp);
  PUTSHORT(outlen, outp);

  PUTCHAR(chal_len, outp);    /* put length of challenge */
  BCOPY(cstate->challenge, outp, chal_len);
  INCPTR(chal_len, outp);

  BCOPY(cstate->chal_name, outp, name_len);  /* append hostname */
  
  pppWrite(cstate->unit, outpacket_buf[cstate->unit], outlen + PPP_HDRLEN);

  CHAPDEBUG(LOG_INFO, ("ChapSendChallenge: Sent id %d.\n", cstate->chal_id));
  
  TIMEOUT(ChapChallengeTimeout, cstate, cstate->timeouttime);
  ++cstate->chal_transmits;
}


/*
 * ChapSendStatus - Send a status response (ack or nak).
 */
static void
ChapSendStatus(chap_state *cstate, int code)
{
  u_char *outp;
  int outlen, msglen;
  char msg[256]; /* @todo: this can be a char*, no strcpy needed */

  if (code == CHAP_SUCCESS) {
    strcpy(msg, "Welcome!");
  } else {
    strcpy(msg, "I don't like you.  Go 'way.");
  }
  msglen = (int)strlen(msg);

  outlen = CHAP_HEADERLEN + msglen;
  outp = outpacket_buf[cstate->unit];

  MAKEHEADER(outp, PPP_CHAP);    /* paste in a header */
  
  PUTCHAR(code, outp);
  PUTCHAR(cstate->chal_id, outp);
  PUTSHORT(outlen, outp);
  BCOPY(msg, outp, msglen);
  pppWrite(cstate->unit, outpacket_buf[cstate->unit], outlen + PPP_HDRLEN);

  CHAPDEBUG(LOG_INFO, ("ChapSendStatus: Sent code %d, id %d.\n", code,
             cstate->chal_id));
}

/*
 * ChapGenChallenge is used to generate a pseudo-random challenge string of
 * a pseudo-random length between min_len and max_len.  The challenge
 * string and its length are stored in *cstate, and various other fields of
 * *cstate are initialized.
 */

static void
ChapGenChallenge(chap_state *cstate)
{
  int chal_len;
  u_char *ptr = cstate->challenge;
  int i;

  /* pick a random challenge length between MIN_CHALLENGE_LENGTH and 
     MAX_CHALLENGE_LENGTH */  
  chal_len = (unsigned)
        ((((magic() >> 16) *
              (MAX_CHALLENGE_LENGTH - MIN_CHALLENGE_LENGTH)) >> 16)
           + MIN_CHALLENGE_LENGTH);
  LWIP_ASSERT("chal_len <= 0xff", chal_len <= 0xffff);
  cstate->chal_len = (u_char)chal_len;
  cstate->chal_id = ++cstate->id;
  cstate->chal_transmits = 0;

  /* generate a random string */
  for (i = 0; i < chal_len; i++ ) {
    *ptr++ = (char) (magic() & 0xff);
  }
}

/*
 * ChapSendResponse - send a response packet with values as specified
 * in *cstate.
 */
/* ARGSUSED */
static void
ChapSendResponse(chap_state *cstate)
{
  u_char *outp;
  int outlen, md_len, name_len;

  md_len = cstate->resp_length;
  name_len = (int)strlen(cstate->resp_name);
  outlen = CHAP_HEADERLEN + sizeof (u_char) + md_len + name_len;
  outp = outpacket_buf[cstate->unit];

  MAKEHEADER(outp, PPP_CHAP);
  
  PUTCHAR(CHAP_RESPONSE, outp);  /* we are a response */
  PUTCHAR(cstate->resp_id, outp);  /* copy id from challenge packet */
  PUTSHORT(outlen, outp);      /* packet length */
  
  PUTCHAR(md_len, outp);      /* length of MD */
  BCOPY(cstate->response, outp, md_len);    /* copy MD to buffer */
  INCPTR(md_len, outp);

  BCOPY(cstate->resp_name, outp, name_len);  /* append our name */

  /* send the packet */
  pppWrite(cstate->unit, outpacket_buf[cstate->unit], outlen + PPP_HDRLEN);

  cstate->clientstate = CHAPCS_RESPONSE;
  TIMEOUT(ChapResponseTimeout, cstate, cstate->timeouttime);
  ++cstate->resp_transmits;
}

#if PPP_ADDITIONAL_CALLBACKS
static char *ChapCodenames[] = {
  "Challenge", "Response", "Success", "Failure"
};
/*
 * ChapPrintPkt - print the contents of a CHAP packet.
 */
static int
ChapPrintPkt( u_char *p, int plen, void (*printer) (void *, char *, ...), void *arg)
{
  int code, id, len;
  int clen, nlen;
  u_char x;

  if (plen < CHAP_HEADERLEN) {
    return 0;
  }
  GETCHAR(code, p);
  GETCHAR(id, p);
  GETSHORT(len, p);
  if (len < CHAP_HEADERLEN || len > plen) {
    return 0;
  }

  if (code >= 1 && code <= sizeof(ChapCodenames) / sizeof(char *)) {
    printer(arg, " %s", ChapCodenames[code-1]);
  } else {
    printer(arg, " code=0x%x", code);
  }
  printer(arg, " id=0x%x", id);
  len -= CHAP_HEADERLEN;
  switch (code) {
    case CHAP_CHALLENGE:
    case CHAP_RESPONSE:
      if (len < 1) {
        break;
      }
      clen = p[0];
      if (len < clen + 1) {
        break;
      }
      ++p;
      nlen = len - clen - 1;
      printer(arg, " <");
      for (; clen > 0; --clen) {
        GETCHAR(x, p);
        printer(arg, "%.2x", x);
      }
      printer(arg, ">, name = %.*Z", nlen, p);
      break;
    case CHAP_FAILURE:
    case CHAP_SUCCESS:
      printer(arg, " %.*Z", len, p);
      break;
    default:
      for (clen = len; clen > 0; --clen) {
        GETCHAR(x, p);
        printer(arg, " %.2x", x);
      }
  }

  return len + CHAP_HEADERLEN;
}
#endif /* PPP_ADDITIONAL_CALLBACKS */

#endif /* CHAP_SUPPORT */

#endif /* PPP_SUPPORT */
