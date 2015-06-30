/*****************************************************************************
* ppp_oe.c - PPP Over Ethernet implementation for lwIP.
*
* Copyright (c) 2006 by Marc Boucher, Services Informatiques (MBSI) inc.
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
* 06-01-01 Marc Boucher <marc@mbsi.ca>
*   Ported to lwIP.
*****************************************************************************/



/* based on NetBSD: if_pppoe.c,v 1.64 2006/01/31 23:50:15 martin Exp */

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Martin Husemann <martin@NetBSD.org>.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "lwip/opt.h"

#if PPPOE_SUPPORT /* don't build if not configured for use in lwipopts.h */

#include "netif/ppp_oe.h"

#include "ppp_impl.h"
#include "pppdebug.h"

#include "lwip/timers.h"
#include "lwip/memp.h"

#include <string.h>
#include <stdio.h>


/* Add a 16 bit unsigned value to a buffer pointed to by PTR */
#define PPPOE_ADD_16(PTR, VAL) \
    *(PTR)++ = (u8_t)((VAL) / 256);    \
    *(PTR)++ = (u8_t)((VAL) % 256)

/* Add a complete PPPoE header to the buffer pointed to by PTR */
#define PPPOE_ADD_HEADER(PTR, CODE, SESS, LEN)  \
    *(PTR)++ = PPPOE_VERTYPE;  \
    *(PTR)++ = (CODE);         \
    PPPOE_ADD_16(PTR, SESS);   \
    PPPOE_ADD_16(PTR, LEN)

#define PPPOE_DISC_TIMEOUT (5*1000)  /* base for quick timeout calculation */
#define PPPOE_SLOW_RETRY   (60*1000) /* persistent retry interval */
#define PPPOE_DISC_MAXPADI  4        /* retry PADI four times (quickly) */
#define PPPOE_DISC_MAXPADR  2        /* retry PADR twice */

#ifdef PPPOE_SERVER
#error "PPPOE_SERVER is not yet supported under lwIP!"
/* from if_spppsubr.c */
#define IFF_PASSIVE IFF_LINK0 /* wait passively for connection */
#endif

#ifndef PPPOE_ERRORSTRING_LEN
#define PPPOE_ERRORSTRING_LEN     64
#endif
static char pppoe_error_tmp[PPPOE_ERRORSTRING_LEN];


/* input routines */
static void pppoe_dispatch_disc_pkt(struct netif *, struct pbuf *);

/* management routines */
static int pppoe_do_disconnect(struct pppoe_softc *);
static void pppoe_abort_connect(struct pppoe_softc *);
static void pppoe_clear_softc(struct pppoe_softc *, const char *);

/* internal timeout handling */
static void pppoe_timeout(void *);

/* sending actual protocol controll packets */
static err_t pppoe_send_padi(struct pppoe_softc *);
static err_t pppoe_send_padr(struct pppoe_softc *);
#ifdef PPPOE_SERVER
static err_t pppoe_send_pado(struct pppoe_softc *);
static err_t pppoe_send_pads(struct pppoe_softc *);
#endif
static err_t pppoe_send_padt(struct netif *, u_int, const u8_t *);

/* internal helper functions */
static struct pppoe_softc * pppoe_find_softc_by_session(u_int, struct netif *);
static struct pppoe_softc * pppoe_find_softc_by_hunique(u8_t *, size_t, struct netif *);

/** linked list of created pppoe interfaces */
static struct pppoe_softc *pppoe_softc_list;

err_t
pppoe_create(struct netif *ethif, int pd, void (*linkStatusCB)(int pd, int up), struct pppoe_softc **scptr)
{
  struct pppoe_softc *sc;

  sc = (struct pppoe_softc *)memp_malloc(MEMP_PPPOE_IF);
  if (sc == NULL) {
    *scptr = NULL;
    return ERR_MEM;
  }
  memset(sc, 0, sizeof(struct pppoe_softc));

  /* changed to real address later */
  MEMCPY(&sc->sc_dest, ethbroadcast.addr, sizeof(sc->sc_dest));

  sc->sc_pd = pd;
  sc->sc_linkStatusCB = linkStatusCB;
  sc->sc_ethif = ethif;

  /* put the new interface at the head of the list */
  sc->next = pppoe_softc_list;
  pppoe_softc_list = sc;

  *scptr = sc;

  return ERR_OK;
}

err_t
pppoe_destroy(struct netif *ifp)
{
  struct pppoe_softc *sc, *prev = NULL;

  for (sc = pppoe_softc_list; sc != NULL; prev = sc, sc = sc->next) {
    if (sc->sc_ethif == ifp) {
      break;
    }
  }

  if(!(sc && (sc->sc_ethif == ifp))) {
    return ERR_IF;
  }

  sys_untimeout(pppoe_timeout, sc);
  if (prev == NULL) {
    /* remove sc from the head of the list */
    pppoe_softc_list = sc->next;
  } else {
    /* remove sc from the list */
    prev->next = sc->next;
  }

#ifdef PPPOE_TODO
  if (sc->sc_concentrator_name) {
    mem_free(sc->sc_concentrator_name);
  }
  if (sc->sc_service_name) {
    mem_free(sc->sc_service_name);
  }
#endif /* PPPOE_TODO */
  memp_free(MEMP_PPPOE_IF, sc);

  return ERR_OK;
}

/*
 * Find the interface handling the specified session.
 * Note: O(number of sessions open), this is a client-side only, mean
 * and lean implementation, so number of open sessions typically should
 * be 1.
 */
static struct pppoe_softc *
pppoe_find_softc_by_session(u_int session, struct netif *rcvif)
{
  struct pppoe_softc *sc;

  if (session == 0) {
    return NULL;
  }

  for (sc = pppoe_softc_list; sc != NULL; sc = sc->next) {
    if (sc->sc_state == PPPOE_STATE_SESSION
        && sc->sc_session == session) {
      if (sc->sc_ethif == rcvif) {
        return sc;
      } else {
        return NULL;
      }
    }
  }
  return NULL;
}

/* Check host unique token passed and return appropriate softc pointer,
 * or NULL if token is bogus. */
static struct pppoe_softc *
pppoe_find_softc_by_hunique(u8_t *token, size_t len, struct netif *rcvif)
{
  struct pppoe_softc *sc, *t;

  if (pppoe_softc_list == NULL) {
    return NULL;
  }

  if (len != sizeof sc) {
    return NULL;
  }
  MEMCPY(&t, token, len);

  for (sc = pppoe_softc_list; sc != NULL; sc = sc->next) {
    if (sc == t) {
      break;
    }
  }

  if (sc == NULL) {
    PPPDEBUG(LOG_DEBUG, ("pppoe: alien host unique tag, no session found\n"));
    return NULL;
  }

  /* should be safe to access *sc now */
  if (sc->sc_state < PPPOE_STATE_PADI_SENT || sc->sc_state >= PPPOE_STATE_SESSION) {
    printf("%c%c%"U16_F": host unique tag found, but it belongs to a connection in state %d\n",
      sc->sc_ethif->name[0], sc->sc_ethif->name[1], sc->sc_ethif->num, sc->sc_state);
    return NULL;
  }
  if (sc->sc_ethif != rcvif) {
    printf("%c%c%"U16_F": wrong interface, not accepting host unique\n",
      sc->sc_ethif->name[0], sc->sc_ethif->name[1], sc->sc_ethif->num);
    return NULL;
  }
  return sc;
}

static void
pppoe_linkstatus_up(struct pppoe_softc *sc)
{
  sc->sc_linkStatusCB(sc->sc_pd, 1);
}

/* analyze and handle a single received packet while not in session state */
static void
pppoe_dispatch_disc_pkt(struct netif *netif, struct pbuf *pb)
{
  u16_t tag, len;
  u16_t session, plen;
  struct pppoe_softc *sc;
  const char *err_msg;
  char devname[6];
  u8_t *ac_cookie;
  u16_t ac_cookie_len;
#ifdef PPPOE_SERVER
  u8_t *hunique;
  size_t hunique_len;
#endif
  struct pppoehdr *ph;
  struct pppoetag pt;
  int off, err, errortag;
  struct eth_hdr *ethhdr;

  pb = pppSingleBuf(pb);

  strcpy(devname, "pppoe");  /* as long as we don't know which instance */
  err_msg = NULL;
  errortag = 0;
  if (pb->len < sizeof(*ethhdr)) {
    goto done;
  }
  ethhdr = (struct eth_hdr *)pb->payload;
  off = sizeof(*ethhdr);

  ac_cookie = NULL;
  ac_cookie_len = 0;
#ifdef PPPOE_SERVER
  hunique = NULL;
  hunique_len = 0;
#endif
  session = 0;
  if (pb->len - off < PPPOE_HEADERLEN) {
    printf("pppoe: packet too short: %d\n", pb->len);
    goto done;
  }

  ph = (struct pppoehdr *) (ethhdr + 1);
  if (ph->vertype != PPPOE_VERTYPE) {
    printf("pppoe: unknown version/type packet: 0x%x\n", ph->vertype);
    goto done;
  }
  session = ntohs(ph->session);
  plen = ntohs(ph->plen);
  off += sizeof(*ph);

  if (plen + off > pb->len) {
    printf("pppoe: packet content does not fit: data available = %d, packet size = %u\n",
        pb->len - off, plen);
    goto done;
  }
  if(pb->tot_len == pb->len) {
    pb->tot_len = pb->len = (u16_t)off + plen; /* ignore trailing garbage */
  }
  tag = 0;
  len = 0;
  sc = NULL;
  while (off + sizeof(pt) <= pb->len) {
    MEMCPY(&pt, (u8_t*)pb->payload + off, sizeof(pt));
    tag = ntohs(pt.tag);
    len = ntohs(pt.len);
    if (off + sizeof(pt) + len > pb->len) {
      printf("pppoe: tag 0x%x len 0x%x is too long\n", tag, len);
      goto done;
    }
    switch (tag) {
      case PPPOE_TAG_EOL:
        goto breakbreak;
      case PPPOE_TAG_SNAME:
        break;  /* ignored */
      case PPPOE_TAG_ACNAME:
        break;  /* ignored */
      case PPPOE_TAG_HUNIQUE:
        if (sc != NULL) {
          break;
        }
#ifdef PPPOE_SERVER
        hunique = (u8_t*)pb->payload + off + sizeof(pt);
        hunique_len = len;
#endif
        sc = pppoe_find_softc_by_hunique((u8_t*)pb->payload + off + sizeof(pt), len, netif);
        if (sc != NULL) {
          snprintf(devname, sizeof(devname), "%c%c%"U16_F, sc->sc_ethif->name[0], sc->sc_ethif->name[1], sc->sc_ethif->num);
        }
        break;
      case PPPOE_TAG_ACCOOKIE:
        if (ac_cookie == NULL) {
          ac_cookie = (u8_t*)pb->payload + off + sizeof(pt);
          ac_cookie_len = len;
        }
        break;
      case PPPOE_TAG_SNAME_ERR:
        err_msg = "SERVICE NAME ERROR";
        errortag = 1;
        break;
      case PPPOE_TAG_ACSYS_ERR:
        err_msg = "AC SYSTEM ERROR";
        errortag = 1;
        break;
      case PPPOE_TAG_GENERIC_ERR:
        err_msg = "GENERIC ERROR";
        errortag = 1;
        break;
    }
    if (err_msg) {
      if (errortag && len) {
        u16_t error_len = LWIP_MIN(len, sizeof(pppoe_error_tmp)-1);
        strncpy(pppoe_error_tmp, (char*)pb->payload + off + sizeof(pt), error_len);
        pppoe_error_tmp[error_len-1] = '\0';
        printf("%s: %s: %s\n", devname, err_msg, pppoe_error_tmp);
      } else {
        printf("%s: %s\n", devname, err_msg);
      }
      if (errortag) {
        goto done;
      }
    }
    off += sizeof(pt) + len;
  }

breakbreak:;
  switch (ph->code) {
    case PPPOE_CODE_PADI:
#ifdef PPPOE_SERVER
      /*
       * got service name, concentrator name, and/or host unique.
       * ignore if we have no interfaces with IFF_PASSIVE|IFF_UP.
       */
      if (LIST_EMPTY(&pppoe_softc_list)) {
        goto done;
      }
      LIST_FOREACH(sc, &pppoe_softc_list, sc_list) {
        if (!(sc->sc_sppp.pp_if.if_flags & IFF_UP)) {
          continue;
        }
        if (!(sc->sc_sppp.pp_if.if_flags & IFF_PASSIVE)) {
          continue;
        }
        if (sc->sc_state == PPPOE_STATE_INITIAL) {
          break;
        }
      }
      if (sc == NULL) {
        /* printf("pppoe: free passive interface is not found\n"); */
        goto done;
      }
      if (hunique) {
        if (sc->sc_hunique) {
          mem_free(sc->sc_hunique);
        }
        sc->sc_hunique = mem_malloc(hunique_len);
        if (sc->sc_hunique == NULL) {
          goto done;
        }
        sc->sc_hunique_len = hunique_len;
        MEMCPY(sc->sc_hunique, hunique, hunique_len);
      }
      MEMCPY(&sc->sc_dest, eh->ether_shost, sizeof sc->sc_dest);
      sc->sc_state = PPPOE_STATE_PADO_SENT;
      pppoe_send_pado(sc);
      break;
#endif /* PPPOE_SERVER */
    case PPPOE_CODE_PADR:
#ifdef PPPOE_SERVER
      /*
       * get sc from ac_cookie if IFF_PASSIVE
       */
      if (ac_cookie == NULL) {
        /* be quiet if there is not a single pppoe instance */
        printf("pppoe: received PADR but not includes ac_cookie\n");
        goto done;
      }
      sc = pppoe_find_softc_by_hunique(ac_cookie, ac_cookie_len, netif);
      if (sc == NULL) {
        /* be quiet if there is not a single pppoe instance */
        if (!LIST_EMPTY(&pppoe_softc_list)) {
          printf("pppoe: received PADR but could not find request for it\n");
        }
        goto done;
      }
      if (sc->sc_state != PPPOE_STATE_PADO_SENT) {
        printf("%c%c%"U16_F": received unexpected PADR\n", sc->sc_ethif->name[0], sc->sc_ethif->name[1], sc->sc_ethif->num);
        goto done;
      }
      if (hunique) {
        if (sc->sc_hunique) {
          mem_free(sc->sc_hunique);
        }
        sc->sc_hunique = mem_malloc(hunique_len);
        if (sc->sc_hunique == NULL) {
          goto done;
        }
        sc->sc_hunique_len = hunique_len;
        MEMCPY(sc->sc_hunique, hunique, hunique_len);
      }
      pppoe_send_pads(sc);
      sc->sc_state = PPPOE_STATE_SESSION;
      pppoe_linkstatus_up(sc); /* notify upper layers */
      break;
#else
      /* ignore, we are no access concentrator */
      goto done;
#endif /* PPPOE_SERVER */
    case PPPOE_CODE_PADO:
      if (sc == NULL) {
        /* be quiet if there is not a single pppoe instance */
        if (pppoe_softc_list != NULL) {
          printf("pppoe: received PADO but could not find request for it\n");
        }
        goto done;
      }
      if (sc->sc_state != PPPOE_STATE_PADI_SENT) {
        printf("%c%c%"U16_F": received unexpected PADO\n", sc->sc_ethif->name[0], sc->sc_ethif->name[1], sc->sc_ethif->num);
        goto done;
      }
      if (ac_cookie) {
        sc->sc_ac_cookie_len = ac_cookie_len;
        MEMCPY(sc->sc_ac_cookie, ac_cookie, ac_cookie_len);
      }
      MEMCPY(&sc->sc_dest, ethhdr->src.addr, sizeof(sc->sc_dest.addr));
      sys_untimeout(pppoe_timeout, sc);
      sc->sc_padr_retried = 0;
      sc->sc_state = PPPOE_STATE_PADR_SENT;
      if ((err = pppoe_send_padr(sc)) != 0) {
        PPPDEBUG(LOG_DEBUG, ("pppoe: %c%c%"U16_F": failed to send PADR, error=%d\n", sc->sc_ethif->name[0], sc->sc_ethif->name[1], sc->sc_ethif->num, err));
      }
      sys_timeout(PPPOE_DISC_TIMEOUT * (1 + sc->sc_padr_retried), pppoe_timeout, sc);
      break;
    case PPPOE_CODE_PADS:
      if (sc == NULL) {
        goto done;
      }
      sc->sc_session = session;
      sys_untimeout(pppoe_timeout, sc);
      PPPDEBUG(LOG_DEBUG, ("pppoe: %c%c%"U16_F": session 0x%x connected\n", sc->sc_ethif->name[0], sc->sc_ethif->name[1], sc->sc_ethif->num, session));
      sc->sc_state = PPPOE_STATE_SESSION;
      pppoe_linkstatus_up(sc); /* notify upper layers */
      break;
    case PPPOE_CODE_PADT:
      if (sc == NULL) {
        goto done;
      }
      pppoe_clear_softc(sc, "received PADT");
      break;
    default:
      if(sc) {
        printf("%c%c%"U16_F": unknown code (0x%"X16_F") session = 0x%"X16_F"\n",
            sc->sc_ethif->name[0], sc->sc_ethif->name[1], sc->sc_ethif->num,
            (u16_t)ph->code, session);
      } else {
        printf("pppoe: unknown code (0x%"X16_F") session = 0x%"X16_F"\n", (u16_t)ph->code, session);
      }
      break;
  }

done:
  pbuf_free(pb);
  return;
}

void
pppoe_disc_input(struct netif *netif, struct pbuf *p)
{
  /* avoid error messages if there is not a single pppoe instance */
  if (pppoe_softc_list != NULL) {
    pppoe_dispatch_disc_pkt(netif, p);
  } else {
    pbuf_free(p);
  }
}

void
pppoe_data_input(struct netif *netif, struct pbuf *pb)
{
  u16_t session, plen;
  struct pppoe_softc *sc;
  struct pppoehdr *ph;
#ifdef PPPOE_TERM_UNKNOWN_SESSIONS
  u8_t shost[ETHER_ADDR_LEN];
#endif

#ifdef PPPOE_TERM_UNKNOWN_SESSIONS
  MEMCPY(shost, ((struct eth_hdr *)pb->payload)->src.addr, sizeof(shost));
#endif
  if (pbuf_header(pb, -(int)sizeof(struct eth_hdr)) != 0) {
    /* bail out */
    PPPDEBUG(LOG_ERR, ("pppoe_data_input: pbuf_header failed\n"));
    LINK_STATS_INC(link.lenerr);
    goto drop;
  } 

  pb = pppSingleBuf (pb);

  if (pb->len <= PPPOE_HEADERLEN) {
    printf("pppoe (data): dropping too short packet: %d bytes\n", pb->len);
    goto drop;
  }

  if (pb->len < sizeof(*ph)) {
    printf("pppoe_data_input: could not get PPPoE header\n");
    goto drop;
  }
  ph = (struct pppoehdr *)pb->payload;

  if (ph->vertype != PPPOE_VERTYPE) {
    printf("pppoe (data): unknown version/type packet: 0x%x\n", ph->vertype);
    goto drop;
  }
  if (ph->code != 0) {
    goto drop;
  }

  session = ntohs(ph->session);
  sc = pppoe_find_softc_by_session(session, netif);
  if (sc == NULL) {
#ifdef PPPOE_TERM_UNKNOWN_SESSIONS
    printf("pppoe: input for unknown session 0x%x, sending PADT\n", session);
    pppoe_send_padt(netif, session, shost);
#endif
    goto drop;
  }

  plen = ntohs(ph->plen);

  if (pbuf_header(pb, -(int)(PPPOE_HEADERLEN)) != 0) {
    /* bail out */
    PPPDEBUG(LOG_ERR, ("pppoe_data_input: pbuf_header PPPOE_HEADERLEN failed\n"));
    LINK_STATS_INC(link.lenerr);
    goto drop;
  } 

  PPPDEBUG(LOG_DEBUG, ("pppoe_data_input: %c%c%"U16_F": pkthdr.len=%d, pppoe.len=%d\n",
        sc->sc_ethif->name[0], sc->sc_ethif->name[1], sc->sc_ethif->num,
        pb->len, plen));

  if (pb->len < plen) {
    goto drop;
  }

  pppInProcOverEthernet(sc->sc_pd, pb);

  return;

drop:
  pbuf_free(pb);
}

static err_t
pppoe_output(struct pppoe_softc *sc, struct pbuf *pb)
{
  struct eth_hdr *ethhdr;
  u16_t etype;
  err_t res;

  if (!sc->sc_ethif) {
    pbuf_free(pb);
    return ERR_IF;
  }

  ethhdr = (struct eth_hdr *)pb->payload;
  etype = sc->sc_state == PPPOE_STATE_SESSION ? ETHTYPE_PPPOE : ETHTYPE_PPPOEDISC;
  ethhdr->type = htons(etype);
  MEMCPY(ethhdr->dest.addr, sc->sc_dest.addr, sizeof(ethhdr->dest.addr));
  MEMCPY(ethhdr->src.addr, ((struct eth_addr *)sc->sc_ethif->hwaddr)->addr, sizeof(ethhdr->src.addr));

  PPPDEBUG(LOG_DEBUG, ("pppoe: %c%c%"U16_F" (%x) state=%d, session=0x%x output -> %02"X16_F":%02"X16_F":%02"X16_F":%02"X16_F":%02"X16_F":%02"X16_F", len=%d\n",
      sc->sc_ethif->name[0], sc->sc_ethif->name[1], sc->sc_ethif->num, etype,
      sc->sc_state, sc->sc_session,
      sc->sc_dest.addr[0], sc->sc_dest.addr[1], sc->sc_dest.addr[2], sc->sc_dest.addr[3], sc->sc_dest.addr[4], sc->sc_dest.addr[5],
      pb->tot_len));

  res = sc->sc_ethif->linkoutput(sc->sc_ethif, pb);

  pbuf_free(pb);

  return res;
}

static err_t
pppoe_send_padi(struct pppoe_softc *sc)
{
  struct pbuf *pb;
  u8_t *p;
  int len;
#ifdef PPPOE_TODO
  int l1 = 0, l2 = 0; /* XXX: gcc */
#endif /* PPPOE_TODO */

  if (sc->sc_state >PPPOE_STATE_PADI_SENT) {
    PPPDEBUG(LOG_ERR, ("ERROR: pppoe_send_padi in state %d", sc->sc_state));
  }

  /* calculate length of frame (excluding ethernet header + pppoe header) */
  len = 2 + 2 + 2 + 2 + sizeof sc;  /* service name tag is required, host unique is send too */
#ifdef PPPOE_TODO
  if (sc->sc_service_name != NULL) {
    l1 = (int)strlen(sc->sc_service_name);
    len += l1;
  }
  if (sc->sc_concentrator_name != NULL) {
    l2 = (int)strlen(sc->sc_concentrator_name);
    len += 2 + 2 + l2;
  }
#endif /* PPPOE_TODO */
  LWIP_ASSERT("sizeof(struct eth_hdr) + PPPOE_HEADERLEN + len <= 0xffff",
    sizeof(struct eth_hdr) + PPPOE_HEADERLEN + len <= 0xffff);

  /* allocate a buffer */
  pb = pbuf_alloc(PBUF_LINK, (u16_t)(sizeof(struct eth_hdr) + PPPOE_HEADERLEN + len), PBUF_RAM);
  if (!pb) {
    return ERR_MEM;
  }
  LWIP_ASSERT("pb->tot_len == pb->len", pb->tot_len == pb->len);

  p = (u8_t*)pb->payload + sizeof (struct eth_hdr);
  /* fill in pkt */
  PPPOE_ADD_HEADER(p, PPPOE_CODE_PADI, 0, (u16_t)len);
  PPPOE_ADD_16(p, PPPOE_TAG_SNAME);
#ifdef PPPOE_TODO
  if (sc->sc_service_name != NULL) {
    PPPOE_ADD_16(p, l1);
    MEMCPY(p, sc->sc_service_name, l1);
    p += l1;
  } else
#endif /* PPPOE_TODO */
  {
    PPPOE_ADD_16(p, 0);
  }
#ifdef PPPOE_TODO
  if (sc->sc_concentrator_name != NULL) {
    PPPOE_ADD_16(p, PPPOE_TAG_ACNAME);
    PPPOE_ADD_16(p, l2);
    MEMCPY(p, sc->sc_concentrator_name, l2);
    p += l2;
  }
#endif /* PPPOE_TODO */
  PPPOE_ADD_16(p, PPPOE_TAG_HUNIQUE);
  PPPOE_ADD_16(p, sizeof(sc));
  MEMCPY(p, &sc, sizeof sc);

  /* send pkt */
  return pppoe_output(sc, pb);
}

static void
pppoe_timeout(void *arg)
{
  int retry_wait, err;
  struct pppoe_softc *sc = (struct pppoe_softc*)arg;

  PPPDEBUG(LOG_DEBUG, ("pppoe: %c%c%"U16_F": timeout\n", sc->sc_ethif->name[0], sc->sc_ethif->name[1], sc->sc_ethif->num));

  switch (sc->sc_state) {
    case PPPOE_STATE_PADI_SENT:
      /*
       * We have two basic ways of retrying:
       *  - Quick retry mode: try a few times in short sequence
       *  - Slow retry mode: we already had a connection successfully
       *    established and will try infinitely (without user
       *    intervention)
       * We only enter slow retry mode if IFF_LINK1 (aka autodial)
       * is not set.
       */

      /* initialize for quick retry mode */
      retry_wait = PPPOE_DISC_TIMEOUT * (1 + sc->sc_padi_retried);

      sc->sc_padi_retried++;
      if (sc->sc_padi_retried >= PPPOE_DISC_MAXPADI) {
#if 0
        if ((sc->sc_sppp.pp_if.if_flags & IFF_LINK1) == 0) {
          /* slow retry mode */
          retry_wait = PPPOE_SLOW_RETRY;
        } else
#endif
        {
          pppoe_abort_connect(sc);
          return;
        }
      }
      if ((err = pppoe_send_padi(sc)) != 0) {
        sc->sc_padi_retried--;
        PPPDEBUG(LOG_DEBUG, ("pppoe: %c%c%"U16_F": failed to transmit PADI, error=%d\n", sc->sc_ethif->name[0], sc->sc_ethif->name[1], sc->sc_ethif->num, err));
      }
      sys_timeout(retry_wait, pppoe_timeout, sc);
      break;

    case PPPOE_STATE_PADR_SENT:
      sc->sc_padr_retried++;
      if (sc->sc_padr_retried >= PPPOE_DISC_MAXPADR) {
        MEMCPY(&sc->sc_dest, ethbroadcast.addr, sizeof(sc->sc_dest));
        sc->sc_state = PPPOE_STATE_PADI_SENT;
        sc->sc_padr_retried = 0;
        if ((err = pppoe_send_padi(sc)) != 0) {
          PPPDEBUG(LOG_DEBUG, ("pppoe: %c%c%"U16_F": failed to send PADI, error=%d\n", sc->sc_ethif->name[0], sc->sc_ethif->name[1], sc->sc_ethif->num, err));
        }
        sys_timeout(PPPOE_DISC_TIMEOUT * (1 + sc->sc_padi_retried), pppoe_timeout, sc);
        return;
      }
      if ((err = pppoe_send_padr(sc)) != 0) {
        sc->sc_padr_retried--;
        PPPDEBUG(LOG_DEBUG, ("pppoe: %c%c%"U16_F": failed to send PADR, error=%d\n", sc->sc_ethif->name[0], sc->sc_ethif->name[1], sc->sc_ethif->num, err));
      }
      sys_timeout(PPPOE_DISC_TIMEOUT * (1 + sc->sc_padr_retried), pppoe_timeout, sc);
      break;
    case PPPOE_STATE_CLOSING:
      pppoe_do_disconnect(sc);
      break;
    default:
      return;  /* all done, work in peace */
  }
}

/* Start a connection (i.e. initiate discovery phase) */
int
pppoe_connect(struct pppoe_softc *sc)
{
  int err;

  if (sc->sc_state != PPPOE_STATE_INITIAL) {
    return EBUSY;
  }

#ifdef PPPOE_SERVER
  /* wait PADI if IFF_PASSIVE */
  if ((sc->sc_sppp.pp_if.if_flags & IFF_PASSIVE)) {
    return 0;
  }
#endif
  /* save state, in case we fail to send PADI */
  sc->sc_state = PPPOE_STATE_PADI_SENT;
  sc->sc_padr_retried = 0;
  err = pppoe_send_padi(sc);
  PPPDEBUG(LOG_DEBUG, ("pppoe: %c%c%"U16_F": failed to send PADI, error=%d\n", sc->sc_ethif->name[0], sc->sc_ethif->name[1], sc->sc_ethif->num, err));
  sys_timeout(PPPOE_DISC_TIMEOUT, pppoe_timeout, sc);
  return err;
}

/* disconnect */
void
pppoe_disconnect(struct pppoe_softc *sc)
{
  if (sc->sc_state < PPPOE_STATE_SESSION) {
    return;
  }
  /*
   * Do not call pppoe_disconnect here, the upper layer state
   * machine gets confused by this. We must return from this
   * function and defer disconnecting to the timeout handler.
   */
  sc->sc_state = PPPOE_STATE_CLOSING;
  sys_timeout(20, pppoe_timeout, sc);
}

static int
pppoe_do_disconnect(struct pppoe_softc *sc)
{
  int err;

  if (sc->sc_state < PPPOE_STATE_SESSION) {
    err = EBUSY;
  } else {
    PPPDEBUG(LOG_DEBUG, ("pppoe: %c%c%"U16_F": disconnecting\n", sc->sc_ethif->name[0], sc->sc_ethif->name[1], sc->sc_ethif->num));
    err = pppoe_send_padt(sc->sc_ethif, sc->sc_session, (const u8_t *)&sc->sc_dest);
  }

  /* cleanup softc */
  sc->sc_state = PPPOE_STATE_INITIAL;
  MEMCPY(&sc->sc_dest, ethbroadcast.addr, sizeof(sc->sc_dest));
  sc->sc_ac_cookie_len = 0;
#ifdef PPPOE_SERVER
  if (sc->sc_hunique) {
    mem_free(sc->sc_hunique);
    sc->sc_hunique = NULL;
  }
  sc->sc_hunique_len = 0;
#endif
  sc->sc_session = 0;

  sc->sc_linkStatusCB(sc->sc_pd, 0); /* notify upper layers */

  return err;
}

/* Connection attempt aborted */
static void
pppoe_abort_connect(struct pppoe_softc *sc)
{
  printf("%c%c%"U16_F": could not establish connection\n", sc->sc_ethif->name[0], sc->sc_ethif->name[1], sc->sc_ethif->num);
  sc->sc_state = PPPOE_STATE_CLOSING;

  sc->sc_linkStatusCB(sc->sc_pd, 0); /* notify upper layers */

  /* clear connection state */
  MEMCPY(&sc->sc_dest, ethbroadcast.addr, sizeof(sc->sc_dest));
  sc->sc_state = PPPOE_STATE_INITIAL;
}

/* Send a PADR packet */
static err_t
pppoe_send_padr(struct pppoe_softc *sc)
{
  struct pbuf *pb;
  u8_t *p;
  size_t len;
#ifdef PPPOE_TODO
  size_t l1 = 0; /* XXX: gcc */
#endif /* PPPOE_TODO */

  if (sc->sc_state != PPPOE_STATE_PADR_SENT) {
    return ERR_CONN;
  }

  len = 2 + 2 + 2 + 2 + sizeof(sc);    /* service name, host unique */
#ifdef PPPOE_TODO
  if (sc->sc_service_name != NULL) {    /* service name tag maybe empty */
    l1 = strlen(sc->sc_service_name);
    len += l1;
  }
#endif /* PPPOE_TODO */
  if (sc->sc_ac_cookie_len > 0) {
    len += 2 + 2 + sc->sc_ac_cookie_len;  /* AC cookie */
  }
  LWIP_ASSERT("sizeof(struct eth_hdr) + PPPOE_HEADERLEN + len <= 0xffff",
    sizeof(struct eth_hdr) + PPPOE_HEADERLEN + len <= 0xffff);
  pb = pbuf_alloc(PBUF_LINK, (u16_t)(sizeof(struct eth_hdr) + PPPOE_HEADERLEN + len), PBUF_RAM);
  if (!pb) {
    return ERR_MEM;
  }
  LWIP_ASSERT("pb->tot_len == pb->len", pb->tot_len == pb->len);
  p = (u8_t*)pb->payload + sizeof (struct eth_hdr);
  PPPOE_ADD_HEADER(p, PPPOE_CODE_PADR, 0, len);
  PPPOE_ADD_16(p, PPPOE_TAG_SNAME);
#ifdef PPPOE_TODO
  if (sc->sc_service_name != NULL) {
    PPPOE_ADD_16(p, l1);
    MEMCPY(p, sc->sc_service_name, l1);
    p += l1;
  } else
#endif /* PPPOE_TODO */
  {
    PPPOE_ADD_16(p, 0);
  }
  if (sc->sc_ac_cookie_len > 0) {
    PPPOE_ADD_16(p, PPPOE_TAG_ACCOOKIE);
    PPPOE_ADD_16(p, sc->sc_ac_cookie_len);
    MEMCPY(p, sc->sc_ac_cookie, sc->sc_ac_cookie_len);
    p += sc->sc_ac_cookie_len;
  }
  PPPOE_ADD_16(p, PPPOE_TAG_HUNIQUE);
  PPPOE_ADD_16(p, sizeof(sc));
  MEMCPY(p, &sc, sizeof sc);

  return pppoe_output(sc, pb);
}

/* send a PADT packet */
static err_t
pppoe_send_padt(struct netif *outgoing_if, u_int session, const u8_t *dest)
{
  struct pbuf *pb;
  struct eth_hdr *ethhdr;
  err_t res;
  u8_t *p;

  pb = pbuf_alloc(PBUF_LINK, sizeof(struct eth_hdr) + PPPOE_HEADERLEN, PBUF_RAM);
  if (!pb) {
    return ERR_MEM;
  }
  LWIP_ASSERT("pb->tot_len == pb->len", pb->tot_len == pb->len);

  ethhdr = (struct eth_hdr *)pb->payload;
  ethhdr->type = PP_HTONS(ETHTYPE_PPPOEDISC);
  MEMCPY(ethhdr->dest.addr, dest, sizeof(ethhdr->dest.addr));
  MEMCPY(ethhdr->src.addr, ((struct eth_addr *)outgoing_if->hwaddr)->addr, sizeof(ethhdr->src.addr));

  p = (u8_t*)(ethhdr + 1);
  PPPOE_ADD_HEADER(p, PPPOE_CODE_PADT, session, 0);

  res = outgoing_if->linkoutput(outgoing_if, pb);

  pbuf_free(pb);

  return res;
}

#ifdef PPPOE_SERVER
static err_t
pppoe_send_pado(struct pppoe_softc *sc)
{
  struct pbuf *pb;
  u8_t *p;
  size_t len;

  if (sc->sc_state != PPPOE_STATE_PADO_SENT) {
    return ERR_CONN;
  }

  /* calc length */
  len = 0;
  /* include ac_cookie */
  len += 2 + 2 + sizeof(sc);
  /* include hunique */
  len += 2 + 2 + sc->sc_hunique_len;
  pb = pbuf_alloc(PBUF_LINK, sizeof(struct eth_hdr) + PPPOE_HEADERLEN + len, PBUF_RAM);
  if (!pb) {
    return ERR_MEM;
  }
  LWIP_ASSERT("pb->tot_len == pb->len", pb->tot_len == pb->len);
  p = (u8_t*)pb->payload + sizeof (struct eth_hdr);
  PPPOE_ADD_HEADER(p, PPPOE_CODE_PADO, 0, len);
  PPPOE_ADD_16(p, PPPOE_TAG_ACCOOKIE);
  PPPOE_ADD_16(p, sizeof(sc));
  MEMCPY(p, &sc, sizeof(sc));
  p += sizeof(sc);
  PPPOE_ADD_16(p, PPPOE_TAG_HUNIQUE);
  PPPOE_ADD_16(p, sc->sc_hunique_len);
  MEMCPY(p, sc->sc_hunique, sc->sc_hunique_len);
  return pppoe_output(sc, pb);
}

static err_t
pppoe_send_pads(struct pppoe_softc *sc)
{
  struct pbuf *pb;
  u8_t *p;
  size_t len, l1 = 0;  /* XXX: gcc */

  if (sc->sc_state != PPPOE_STATE_PADO_SENT) {
    return ERR_CONN;
  }

  sc->sc_session = mono_time.tv_sec % 0xff + 1;
  /* calc length */
  len = 0;
  /* include hunique */
  len += 2 + 2 + 2 + 2 + sc->sc_hunique_len;  /* service name, host unique*/
  if (sc->sc_service_name != NULL) {    /* service name tag maybe empty */
    l1 = strlen(sc->sc_service_name);
    len += l1;
  }
  pb = pbuf_alloc(PBUF_LINK, sizeof(struct eth_hdr) + PPPOE_HEADERLEN + len, PBUF_RAM);
  if (!pb) {
    return ERR_MEM;
  }
  LWIP_ASSERT("pb->tot_len == pb->len", pb->tot_len == pb->len);
  p = (u8_t*)pb->payload + sizeof (struct eth_hdr);
  PPPOE_ADD_HEADER(p, PPPOE_CODE_PADS, sc->sc_session, len);
  PPPOE_ADD_16(p, PPPOE_TAG_SNAME);
  if (sc->sc_service_name != NULL) {
    PPPOE_ADD_16(p, l1);
    MEMCPY(p, sc->sc_service_name, l1);
    p += l1;
  } else {
    PPPOE_ADD_16(p, 0);
  }
  PPPOE_ADD_16(p, PPPOE_TAG_HUNIQUE);
  PPPOE_ADD_16(p, sc->sc_hunique_len);
  MEMCPY(p, sc->sc_hunique, sc->sc_hunique_len);
  return pppoe_output(sc, pb);
}
#endif

err_t
pppoe_xmit(struct pppoe_softc *sc, struct pbuf *pb)
{
  u8_t *p;
  size_t len;

  /* are we ready to process data yet? */
  if (sc->sc_state < PPPOE_STATE_SESSION) {
    /*sppp_flush(&sc->sc_sppp.pp_if);*/
    pbuf_free(pb);
    return ERR_CONN;
  }

  len = pb->tot_len;

  /* make room for Ethernet header - should not fail */
  if (pbuf_header(pb, sizeof(struct eth_hdr) + PPPOE_HEADERLEN) != 0) {
    /* bail out */
    PPPDEBUG(LOG_ERR, ("pppoe: %c%c%"U16_F": pppoe_xmit: could not allocate room for header\n", sc->sc_ethif->name[0], sc->sc_ethif->name[1], sc->sc_ethif->num));
    LINK_STATS_INC(link.lenerr);
    pbuf_free(pb);
    return ERR_BUF;
  } 

  p = (u8_t*)pb->payload + sizeof(struct eth_hdr);
  PPPOE_ADD_HEADER(p, 0, sc->sc_session, len);

  return pppoe_output(sc, pb);
}

#if 0 /*def PFIL_HOOKS*/
static int
pppoe_ifattach_hook(void *arg, struct pbuf **mp, struct netif *ifp, int dir)
{
  struct pppoe_softc *sc;
  int s;

  if (mp != (struct pbuf **)PFIL_IFNET_DETACH) {
    return 0;
  }

  LIST_FOREACH(sc, &pppoe_softc_list, sc_list) {
    if (sc->sc_ethif != ifp) {
      continue;
    }
    if (sc->sc_sppp.pp_if.if_flags & IFF_UP) {
      sc->sc_sppp.pp_if.if_flags &= ~(IFF_UP|IFF_RUNNING);
      printf("%c%c%"U16_F": ethernet interface detached, going down\n",
          sc->sc_ethif->name[0], sc->sc_ethif->name[1], sc->sc_ethif->num);
    }
    sc->sc_ethif = NULL;
    pppoe_clear_softc(sc, "ethernet interface detached");
  }

  return 0;
}
#endif

static void
pppoe_clear_softc(struct pppoe_softc *sc, const char *message)
{
  LWIP_UNUSED_ARG(message);

  /* stop timer */
  sys_untimeout(pppoe_timeout, sc);
  PPPDEBUG(LOG_DEBUG, ("pppoe: %c%c%"U16_F": session 0x%x terminated, %s\n", sc->sc_ethif->name[0], sc->sc_ethif->name[1], sc->sc_ethif->num, sc->sc_session, message));

  /* fix our state */
  sc->sc_state = PPPOE_STATE_INITIAL;

  /* notify upper layers */
  sc->sc_linkStatusCB(sc->sc_pd, 0);

  /* clean up softc */
  MEMCPY(&sc->sc_dest, ethbroadcast.addr, sizeof(sc->sc_dest));
  sc->sc_ac_cookie_len = 0;
  sc->sc_session = 0;
}

#endif /* PPPOE_SUPPORT */

