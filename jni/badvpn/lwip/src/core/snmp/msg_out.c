/**
 * @file
 * SNMP output message processing (RFC1157).
 *
 * Output responses and traps are build in two passes:
 *
 * Pass 0: iterate over the output message backwards to determine encoding lengths
 * Pass 1: the actual forward encoding of internal form into ASN1
 *
 * The single-pass encoding method described by Comer & Stevens
 * requires extra buffer space and copying for reversal of the packet.
 * The buffer requirement can be prohibitively large for big payloads
 * (>= 484) therefore we use the two encoding passes.
 */

/*
 * Copyright (c) 2006 Axon Digital Design B.V., The Netherlands.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * Author: Christiaan Simons <christiaan.simons@axon.tv>
 */

#include "lwip/opt.h"

#if LWIP_SNMP /* don't build if not configured for use in lwipopts.h */

#include "lwip/udp.h"
#include "lwip/netif.h"
#include "lwip/snmp.h"
#include "lwip/snmp_asn1.h"
#include "lwip/snmp_msg.h"

struct snmp_trap_dst
{
  /* destination IP address in network order */
  ip_addr_t dip;
  /* set to 0 when disabled, >0 when enabled */
  u8_t enable;
};
struct snmp_trap_dst trap_dst[SNMP_TRAP_DESTINATIONS];

/** TRAP message structure */
struct snmp_msg_trap trap_msg;

static u16_t snmp_resp_header_sum(struct snmp_msg_pstat *m_stat, u16_t vb_len);
static u16_t snmp_trap_header_sum(struct snmp_msg_trap *m_trap, u16_t vb_len);
static u16_t snmp_varbind_list_sum(struct snmp_varbind_root *root);

static u16_t snmp_resp_header_enc(struct snmp_msg_pstat *m_stat, struct pbuf *p);
static u16_t snmp_trap_header_enc(struct snmp_msg_trap *m_trap, struct pbuf *p);
static u16_t snmp_varbind_list_enc(struct snmp_varbind_root *root, struct pbuf *p, u16_t ofs);

/**
 * Sets enable switch for this trap destination.
 * @param dst_idx index in 0 .. SNMP_TRAP_DESTINATIONS-1
 * @param enable switch if 0 destination is disabled >0 enabled.
 */
void
snmp_trap_dst_enable(u8_t dst_idx, u8_t enable)
{
  if (dst_idx < SNMP_TRAP_DESTINATIONS)
  {
    trap_dst[dst_idx].enable = enable;
  }
}

/**
 * Sets IPv4 address for this trap destination.
 * @param dst_idx index in 0 .. SNMP_TRAP_DESTINATIONS-1
 * @param dst IPv4 address in host order.
 */
void
snmp_trap_dst_ip_set(u8_t dst_idx, ip_addr_t *dst)
{
  if (dst_idx < SNMP_TRAP_DESTINATIONS)
  {
    ip_addr_set(&trap_dst[dst_idx].dip, dst);
  }
}

/**
 * Sends a 'getresponse' message to the request originator.
 *
 * @param m_stat points to the current message request state source
 * @return ERR_OK when success, ERR_MEM if we're out of memory
 *
 * @note the caller is responsible for filling in outvb in the m_stat
 * and provide error-status and index (except for tooBig errors) ...
 */
err_t
snmp_send_response(struct snmp_msg_pstat *m_stat)
{
  struct snmp_varbind_root emptyvb = {NULL, NULL, 0, 0, 0};
  struct pbuf *p;
  u16_t tot_len;
  err_t err;

  /* pass 0, calculate length fields */
  tot_len = snmp_varbind_list_sum(&m_stat->outvb);
  tot_len = snmp_resp_header_sum(m_stat, tot_len);

  /* try allocating pbuf(s) for complete response */
  p = pbuf_alloc(PBUF_TRANSPORT, tot_len, PBUF_POOL);
  if (p == NULL)
  {
    LWIP_DEBUGF(SNMP_MSG_DEBUG, ("snmp_snd_response() tooBig\n"));

    /* can't construct reply, return error-status tooBig */
    m_stat->error_status = SNMP_ES_TOOBIG;
    m_stat->error_index = 0;
    /* pass 0, recalculate lengths, for empty varbind-list */
    tot_len = snmp_varbind_list_sum(&emptyvb);
    tot_len = snmp_resp_header_sum(m_stat, tot_len);
    /* retry allocation once for header and empty varbind-list */
    p = pbuf_alloc(PBUF_TRANSPORT, tot_len, PBUF_POOL);
  }
  if (p != NULL)
  {
    /* first pbuf alloc try or retry alloc success */
    u16_t ofs;

    LWIP_DEBUGF(SNMP_MSG_DEBUG, ("snmp_snd_response() p != NULL\n"));

    /* pass 1, size error, encode packet ino the pbuf(s) */
    ofs = snmp_resp_header_enc(m_stat, p);
    snmp_varbind_list_enc(&m_stat->outvb, p, ofs);

    switch (m_stat->error_status)
    {
      case SNMP_ES_TOOBIG:
        snmp_inc_snmpouttoobigs();
        break;
      case SNMP_ES_NOSUCHNAME:
        snmp_inc_snmpoutnosuchnames();
        break;
      case SNMP_ES_BADVALUE:
        snmp_inc_snmpoutbadvalues();
        break;
      case SNMP_ES_GENERROR:
        snmp_inc_snmpoutgenerrs();
        break;
    }
    snmp_inc_snmpoutgetresponses();
    snmp_inc_snmpoutpkts();

    /** @todo do we need separate rx and tx pcbs for threaded case? */
    /** connect to the originating source */
    udp_connect(m_stat->pcb, &m_stat->sip, m_stat->sp);
    err = udp_send(m_stat->pcb, p);
    if (err == ERR_MEM)
    {
      /** @todo release some memory, retry and return tooBig? tooMuchHassle? */
      err = ERR_MEM;
    }
    else
    {
      err = ERR_OK;
    }
    /** disassociate remote address and port with this pcb */
    udp_disconnect(m_stat->pcb);

    pbuf_free(p);
    LWIP_DEBUGF(SNMP_MSG_DEBUG, ("snmp_snd_response() done\n"));
    return err;
  }
  else
  {
    /* first pbuf alloc try or retry alloc failed
       very low on memory, couldn't return tooBig */
    return ERR_MEM;
  }
}


/**
 * Sends an generic or enterprise specific trap message.
 *
 * @param generic_trap is the trap code
 * @param eoid points to enterprise object identifier
 * @param specific_trap used for enterprise traps when generic_trap == 6
 * @return ERR_OK when success, ERR_MEM if we're out of memory
 *
 * @note the caller is responsible for filling in outvb in the trap_msg
 * @note the use of the enterpise identifier field
 * is per RFC1215.
 * Use .iso.org.dod.internet.mgmt.mib-2.snmp for generic traps
 * and .iso.org.dod.internet.private.enterprises.yourenterprise
 * (sysObjectID) for specific traps.
 */
err_t
snmp_send_trap(s8_t generic_trap, struct snmp_obj_id *eoid, s32_t specific_trap)
{
  struct snmp_trap_dst *td;
  struct netif *dst_if;
  ip_addr_t dst_ip;
  struct pbuf *p;
  u16_t i,tot_len;
  err_t err = ERR_OK;

  for (i=0, td = &trap_dst[0]; i<SNMP_TRAP_DESTINATIONS; i++, td++)
  {
    if ((td->enable != 0) && !ip_addr_isany(&td->dip))
    {
      /* network order trap destination */
      ip_addr_copy(trap_msg.dip, td->dip);
      /* lookup current source address for this dst */
      dst_if = ip_route(&td->dip);
      if (dst_if != NULL) {
        ip_addr_copy(dst_ip, dst_if->ip_addr);
        /* @todo: what about IPv6? */
        trap_msg.sip_raw[0] = ip4_addr1(&dst_ip);
        trap_msg.sip_raw[1] = ip4_addr2(&dst_ip);
        trap_msg.sip_raw[2] = ip4_addr3(&dst_ip);
        trap_msg.sip_raw[3] = ip4_addr4(&dst_ip);
        trap_msg.gen_trap = generic_trap;
        trap_msg.spc_trap = specific_trap;
        if (generic_trap == SNMP_GENTRAP_ENTERPRISESPC)
        {
          /* enterprise-Specific trap */
          trap_msg.enterprise = eoid;
        }
        else
        {
          /* generic (MIB-II) trap */
          snmp_get_snmpgrpid_ptr(&trap_msg.enterprise);
        }
        snmp_get_sysuptime(&trap_msg.ts);

        /* pass 0, calculate length fields */
        tot_len = snmp_varbind_list_sum(&trap_msg.outvb);
        tot_len = snmp_trap_header_sum(&trap_msg, tot_len);

        /* allocate pbuf(s) */
        p = pbuf_alloc(PBUF_TRANSPORT, tot_len, PBUF_POOL);
        if (p != NULL)
        {
          u16_t ofs;

          /* pass 1, encode packet ino the pbuf(s) */
          ofs = snmp_trap_header_enc(&trap_msg, p);
          snmp_varbind_list_enc(&trap_msg.outvb, p, ofs);

          snmp_inc_snmpouttraps();
          snmp_inc_snmpoutpkts();

          /** send to the TRAP destination */
          udp_sendto(trap_msg.pcb, p, &trap_msg.dip, SNMP_TRAP_PORT);

          pbuf_free(p);
        } else {
          err = ERR_MEM;
        }
      } else {
        /* routing error */
        err = ERR_RTE;
      }
    }
  }
  return err;
}

void
snmp_coldstart_trap(void)
{
  trap_msg.outvb.head = NULL;
  trap_msg.outvb.tail = NULL;
  trap_msg.outvb.count = 0;
  snmp_send_trap(SNMP_GENTRAP_COLDSTART, NULL, 0);
}

void
snmp_authfail_trap(void)
{
  u8_t enable;
  snmp_get_snmpenableauthentraps(&enable);
  if (enable == 1)
  {
    trap_msg.outvb.head = NULL;
    trap_msg.outvb.tail = NULL;
    trap_msg.outvb.count = 0;
    snmp_send_trap(SNMP_GENTRAP_AUTHFAIL, NULL, 0);
  }
}

/**
 * Sums response header field lengths from tail to head and
 * returns resp_header_lengths for second encoding pass.
 *
 * @param vb_len varbind-list length
 * @param rhl points to returned header lengths
 * @return the required lenght for encoding the response header
 */
static u16_t
snmp_resp_header_sum(struct snmp_msg_pstat *m_stat, u16_t vb_len)
{
  u16_t tot_len;
  struct snmp_resp_header_lengths *rhl;

  rhl = &m_stat->rhl;
  tot_len = vb_len;
  snmp_asn1_enc_s32t_cnt(m_stat->error_index, &rhl->erridxlen);
  snmp_asn1_enc_length_cnt(rhl->erridxlen, &rhl->erridxlenlen);
  tot_len += 1 + rhl->erridxlenlen + rhl->erridxlen;

  snmp_asn1_enc_s32t_cnt(m_stat->error_status, &rhl->errstatlen);
  snmp_asn1_enc_length_cnt(rhl->errstatlen, &rhl->errstatlenlen);
  tot_len += 1 + rhl->errstatlenlen + rhl->errstatlen;

  snmp_asn1_enc_s32t_cnt(m_stat->rid, &rhl->ridlen);
  snmp_asn1_enc_length_cnt(rhl->ridlen, &rhl->ridlenlen);
  tot_len += 1 + rhl->ridlenlen + rhl->ridlen;

  rhl->pdulen = tot_len;
  snmp_asn1_enc_length_cnt(rhl->pdulen, &rhl->pdulenlen);
  tot_len += 1 + rhl->pdulenlen;

  rhl->comlen = m_stat->com_strlen;
  snmp_asn1_enc_length_cnt(rhl->comlen, &rhl->comlenlen);
  tot_len += 1 + rhl->comlenlen + rhl->comlen;

  snmp_asn1_enc_s32t_cnt(snmp_version, &rhl->verlen);
  snmp_asn1_enc_length_cnt(rhl->verlen, &rhl->verlenlen);
  tot_len += 1 + rhl->verlen + rhl->verlenlen;

  rhl->seqlen = tot_len;
  snmp_asn1_enc_length_cnt(rhl->seqlen, &rhl->seqlenlen);
  tot_len += 1 + rhl->seqlenlen;

  return tot_len;
}

/**
 * Sums trap header field lengths from tail to head and
 * returns trap_header_lengths for second encoding pass.
 *
 * @param vb_len varbind-list length
 * @param thl points to returned header lengths
 * @return the required lenght for encoding the trap header
 */
static u16_t
snmp_trap_header_sum(struct snmp_msg_trap *m_trap, u16_t vb_len)
{
  u16_t tot_len;
  struct snmp_trap_header_lengths *thl;

  thl = &m_trap->thl;
  tot_len = vb_len;

  snmp_asn1_enc_u32t_cnt(m_trap->ts, &thl->tslen);
  snmp_asn1_enc_length_cnt(thl->tslen, &thl->tslenlen);
  tot_len += 1 + thl->tslen + thl->tslenlen;

  snmp_asn1_enc_s32t_cnt(m_trap->spc_trap, &thl->strplen);
  snmp_asn1_enc_length_cnt(thl->strplen, &thl->strplenlen);
  tot_len += 1 + thl->strplen + thl->strplenlen;

  snmp_asn1_enc_s32t_cnt(m_trap->gen_trap, &thl->gtrplen);
  snmp_asn1_enc_length_cnt(thl->gtrplen, &thl->gtrplenlen);
  tot_len += 1 + thl->gtrplen + thl->gtrplenlen;

  thl->aaddrlen = 4;
  snmp_asn1_enc_length_cnt(thl->aaddrlen, &thl->aaddrlenlen);
  tot_len += 1 + thl->aaddrlen + thl->aaddrlenlen;

  snmp_asn1_enc_oid_cnt(m_trap->enterprise->len, &m_trap->enterprise->id[0], &thl->eidlen);
  snmp_asn1_enc_length_cnt(thl->eidlen, &thl->eidlenlen);
  tot_len += 1 + thl->eidlen + thl->eidlenlen;

  thl->pdulen = tot_len;
  snmp_asn1_enc_length_cnt(thl->pdulen, &thl->pdulenlen);
  tot_len += 1 + thl->pdulenlen;

  thl->comlen = sizeof(snmp_publiccommunity) - 1;
  snmp_asn1_enc_length_cnt(thl->comlen, &thl->comlenlen);
  tot_len += 1 + thl->comlenlen + thl->comlen;

  snmp_asn1_enc_s32t_cnt(snmp_version, &thl->verlen);
  snmp_asn1_enc_length_cnt(thl->verlen, &thl->verlenlen);
  tot_len += 1 + thl->verlen + thl->verlenlen;

  thl->seqlen = tot_len;
  snmp_asn1_enc_length_cnt(thl->seqlen, &thl->seqlenlen);
  tot_len += 1 + thl->seqlenlen;

  return tot_len;
}

/**
 * Sums varbind lengths from tail to head and
 * annotates lengths in varbind for second encoding pass.
 *
 * @param root points to the root of the variable binding list
 * @return the required lenght for encoding the variable bindings
 */
static u16_t
snmp_varbind_list_sum(struct snmp_varbind_root *root)
{
  struct snmp_varbind *vb;
  u32_t *uint_ptr;
  s32_t *sint_ptr;
  u16_t tot_len;

  tot_len = 0;
  vb = root->tail;
  while ( vb != NULL )
  {
    /* encoded value lenght depends on type */
    switch (vb->value_type)
    {
      case (SNMP_ASN1_UNIV | SNMP_ASN1_PRIMIT | SNMP_ASN1_INTEG):
        sint_ptr = (s32_t*)vb->value;
        snmp_asn1_enc_s32t_cnt(*sint_ptr, &vb->vlen);
        break;
      case (SNMP_ASN1_APPLIC | SNMP_ASN1_PRIMIT | SNMP_ASN1_COUNTER):
      case (SNMP_ASN1_APPLIC | SNMP_ASN1_PRIMIT | SNMP_ASN1_GAUGE):
      case (SNMP_ASN1_APPLIC | SNMP_ASN1_PRIMIT | SNMP_ASN1_TIMETICKS):
        uint_ptr = (u32_t*)vb->value;
        snmp_asn1_enc_u32t_cnt(*uint_ptr, &vb->vlen);
        break;
      case (SNMP_ASN1_UNIV | SNMP_ASN1_PRIMIT | SNMP_ASN1_OC_STR):
      case (SNMP_ASN1_UNIV | SNMP_ASN1_PRIMIT | SNMP_ASN1_NUL):
      case (SNMP_ASN1_APPLIC | SNMP_ASN1_PRIMIT | SNMP_ASN1_IPADDR):
      case (SNMP_ASN1_APPLIC | SNMP_ASN1_PRIMIT | SNMP_ASN1_OPAQUE):
        vb->vlen = vb->value_len;
        break;
      case (SNMP_ASN1_UNIV | SNMP_ASN1_PRIMIT | SNMP_ASN1_OBJ_ID):
        sint_ptr = (s32_t*)vb->value;
        snmp_asn1_enc_oid_cnt(vb->value_len / sizeof(s32_t), sint_ptr, &vb->vlen);
        break;
      default:
        /* unsupported type */
        vb->vlen = 0;
        break;
    };
    /* encoding length of value length field */
    snmp_asn1_enc_length_cnt(vb->vlen, &vb->vlenlen);
    snmp_asn1_enc_oid_cnt(vb->ident_len, vb->ident, &vb->olen);
    snmp_asn1_enc_length_cnt(vb->olen, &vb->olenlen);

    vb->seqlen = 1 + vb->vlenlen + vb->vlen;
    vb->seqlen += 1 + vb->olenlen + vb->olen;
    snmp_asn1_enc_length_cnt(vb->seqlen, &vb->seqlenlen);

    /* varbind seq */
    tot_len += 1 + vb->seqlenlen + vb->seqlen;

    vb = vb->prev;
  }

  /* varbind-list seq */
  root->seqlen = tot_len;
  snmp_asn1_enc_length_cnt(root->seqlen, &root->seqlenlen);
  tot_len += 1 + root->seqlenlen;

  return tot_len;
}

/**
 * Encodes response header from head to tail.
 */
static u16_t
snmp_resp_header_enc(struct snmp_msg_pstat *m_stat, struct pbuf *p)
{
  u16_t ofs;

  ofs = 0;
  snmp_asn1_enc_type(p, ofs, (SNMP_ASN1_UNIV | SNMP_ASN1_CONSTR | SNMP_ASN1_SEQ));
  ofs += 1;
  snmp_asn1_enc_length(p, ofs, m_stat->rhl.seqlen);
  ofs += m_stat->rhl.seqlenlen;

  snmp_asn1_enc_type(p, ofs, (SNMP_ASN1_UNIV | SNMP_ASN1_PRIMIT | SNMP_ASN1_INTEG));
  ofs += 1;
  snmp_asn1_enc_length(p, ofs, m_stat->rhl.verlen);
  ofs += m_stat->rhl.verlenlen;
  snmp_asn1_enc_s32t(p, ofs, m_stat->rhl.verlen, snmp_version);
  ofs += m_stat->rhl.verlen;

  snmp_asn1_enc_type(p, ofs, (SNMP_ASN1_UNIV | SNMP_ASN1_PRIMIT | SNMP_ASN1_OC_STR));
  ofs += 1;
  snmp_asn1_enc_length(p, ofs, m_stat->rhl.comlen);
  ofs += m_stat->rhl.comlenlen;
  snmp_asn1_enc_raw(p, ofs, m_stat->rhl.comlen, m_stat->community);
  ofs += m_stat->rhl.comlen;

  snmp_asn1_enc_type(p, ofs, (SNMP_ASN1_CONTXT | SNMP_ASN1_CONSTR | SNMP_ASN1_PDU_GET_RESP));
  ofs += 1;
  snmp_asn1_enc_length(p, ofs, m_stat->rhl.pdulen);
  ofs += m_stat->rhl.pdulenlen;

  snmp_asn1_enc_type(p, ofs, (SNMP_ASN1_UNIV | SNMP_ASN1_PRIMIT | SNMP_ASN1_INTEG));
  ofs += 1;
  snmp_asn1_enc_length(p, ofs, m_stat->rhl.ridlen);
  ofs += m_stat->rhl.ridlenlen;
  snmp_asn1_enc_s32t(p, ofs, m_stat->rhl.ridlen, m_stat->rid);
  ofs += m_stat->rhl.ridlen;

  snmp_asn1_enc_type(p, ofs, (SNMP_ASN1_UNIV | SNMP_ASN1_PRIMIT | SNMP_ASN1_INTEG));
  ofs += 1;
  snmp_asn1_enc_length(p, ofs, m_stat->rhl.errstatlen);
  ofs += m_stat->rhl.errstatlenlen;
  snmp_asn1_enc_s32t(p, ofs, m_stat->rhl.errstatlen, m_stat->error_status);
  ofs += m_stat->rhl.errstatlen;

  snmp_asn1_enc_type(p, ofs, (SNMP_ASN1_UNIV | SNMP_ASN1_PRIMIT | SNMP_ASN1_INTEG));
  ofs += 1;
  snmp_asn1_enc_length(p, ofs, m_stat->rhl.erridxlen);
  ofs += m_stat->rhl.erridxlenlen;
  snmp_asn1_enc_s32t(p, ofs, m_stat->rhl.erridxlen, m_stat->error_index);
  ofs += m_stat->rhl.erridxlen;

  return ofs;
}

/**
 * Encodes trap header from head to tail.
 */
static u16_t
snmp_trap_header_enc(struct snmp_msg_trap *m_trap, struct pbuf *p)
{
  u16_t ofs;

  ofs = 0;
  snmp_asn1_enc_type(p, ofs, (SNMP_ASN1_UNIV | SNMP_ASN1_CONSTR | SNMP_ASN1_SEQ));
  ofs += 1;
  snmp_asn1_enc_length(p, ofs, m_trap->thl.seqlen);
  ofs += m_trap->thl.seqlenlen;

  snmp_asn1_enc_type(p, ofs, (SNMP_ASN1_UNIV | SNMP_ASN1_PRIMIT | SNMP_ASN1_INTEG));
  ofs += 1;
  snmp_asn1_enc_length(p, ofs, m_trap->thl.verlen);
  ofs += m_trap->thl.verlenlen;
  snmp_asn1_enc_s32t(p, ofs, m_trap->thl.verlen, snmp_version);
  ofs += m_trap->thl.verlen;

  snmp_asn1_enc_type(p, ofs, (SNMP_ASN1_UNIV | SNMP_ASN1_PRIMIT | SNMP_ASN1_OC_STR));
  ofs += 1;
  snmp_asn1_enc_length(p, ofs, m_trap->thl.comlen);
  ofs += m_trap->thl.comlenlen;
  snmp_asn1_enc_raw(p, ofs, m_trap->thl.comlen, (u8_t *)&snmp_publiccommunity[0]);
  ofs += m_trap->thl.comlen;

  snmp_asn1_enc_type(p, ofs, (SNMP_ASN1_CONTXT | SNMP_ASN1_CONSTR | SNMP_ASN1_PDU_TRAP));
  ofs += 1;
  snmp_asn1_enc_length(p, ofs, m_trap->thl.pdulen);
  ofs += m_trap->thl.pdulenlen;

  snmp_asn1_enc_type(p, ofs, (SNMP_ASN1_UNIV | SNMP_ASN1_PRIMIT | SNMP_ASN1_OBJ_ID));
  ofs += 1;
  snmp_asn1_enc_length(p, ofs, m_trap->thl.eidlen);
  ofs += m_trap->thl.eidlenlen;
  snmp_asn1_enc_oid(p, ofs, m_trap->enterprise->len, &m_trap->enterprise->id[0]);
  ofs += m_trap->thl.eidlen;

  snmp_asn1_enc_type(p, ofs, (SNMP_ASN1_APPLIC | SNMP_ASN1_PRIMIT | SNMP_ASN1_IPADDR));
  ofs += 1;
  snmp_asn1_enc_length(p, ofs, m_trap->thl.aaddrlen);
  ofs += m_trap->thl.aaddrlenlen;
  snmp_asn1_enc_raw(p, ofs, m_trap->thl.aaddrlen, &m_trap->sip_raw[0]);
  ofs += m_trap->thl.aaddrlen;

  snmp_asn1_enc_type(p, ofs, (SNMP_ASN1_UNIV | SNMP_ASN1_PRIMIT | SNMP_ASN1_INTEG));
  ofs += 1;
  snmp_asn1_enc_length(p, ofs, m_trap->thl.gtrplen);
  ofs += m_trap->thl.gtrplenlen;
  snmp_asn1_enc_u32t(p, ofs, m_trap->thl.gtrplen, m_trap->gen_trap);
  ofs += m_trap->thl.gtrplen;

  snmp_asn1_enc_type(p, ofs, (SNMP_ASN1_UNIV | SNMP_ASN1_PRIMIT | SNMP_ASN1_INTEG));
  ofs += 1;
  snmp_asn1_enc_length(p, ofs, m_trap->thl.strplen);
  ofs += m_trap->thl.strplenlen;
  snmp_asn1_enc_u32t(p, ofs, m_trap->thl.strplen, m_trap->spc_trap);
  ofs += m_trap->thl.strplen;

  snmp_asn1_enc_type(p, ofs, (SNMP_ASN1_APPLIC | SNMP_ASN1_PRIMIT | SNMP_ASN1_TIMETICKS));
  ofs += 1;
  snmp_asn1_enc_length(p, ofs, m_trap->thl.tslen);
  ofs += m_trap->thl.tslenlen;
  snmp_asn1_enc_u32t(p, ofs, m_trap->thl.tslen, m_trap->ts);
  ofs += m_trap->thl.tslen;

  return ofs;
}

/**
 * Encodes varbind list from head to tail.
 */
static u16_t
snmp_varbind_list_enc(struct snmp_varbind_root *root, struct pbuf *p, u16_t ofs)
{
  struct snmp_varbind *vb;
  s32_t *sint_ptr;
  u32_t *uint_ptr;
  u8_t *raw_ptr;

  snmp_asn1_enc_type(p, ofs, (SNMP_ASN1_UNIV | SNMP_ASN1_CONSTR | SNMP_ASN1_SEQ));
  ofs += 1;
  snmp_asn1_enc_length(p, ofs, root->seqlen);
  ofs += root->seqlenlen;

  vb = root->head;
  while ( vb != NULL )
  {
    snmp_asn1_enc_type(p, ofs, (SNMP_ASN1_UNIV | SNMP_ASN1_CONSTR | SNMP_ASN1_SEQ));
    ofs += 1;
    snmp_asn1_enc_length(p, ofs, vb->seqlen);
    ofs += vb->seqlenlen;

    snmp_asn1_enc_type(p, ofs, (SNMP_ASN1_UNIV | SNMP_ASN1_PRIMIT | SNMP_ASN1_OBJ_ID));
    ofs += 1;
    snmp_asn1_enc_length(p, ofs, vb->olen);
    ofs += vb->olenlen;
    snmp_asn1_enc_oid(p, ofs, vb->ident_len, &vb->ident[0]);
    ofs += vb->olen;

    snmp_asn1_enc_type(p, ofs, vb->value_type);
    ofs += 1;
    snmp_asn1_enc_length(p, ofs, vb->vlen);
    ofs += vb->vlenlen;

    switch (vb->value_type)
    {
      case (SNMP_ASN1_UNIV | SNMP_ASN1_PRIMIT | SNMP_ASN1_INTEG):
        sint_ptr = (s32_t*)vb->value;
        snmp_asn1_enc_s32t(p, ofs, vb->vlen, *sint_ptr);
        break;
      case (SNMP_ASN1_APPLIC | SNMP_ASN1_PRIMIT | SNMP_ASN1_COUNTER):
      case (SNMP_ASN1_APPLIC | SNMP_ASN1_PRIMIT | SNMP_ASN1_GAUGE):
      case (SNMP_ASN1_APPLIC | SNMP_ASN1_PRIMIT | SNMP_ASN1_TIMETICKS):
        uint_ptr = (u32_t*)vb->value;
        snmp_asn1_enc_u32t(p, ofs, vb->vlen, *uint_ptr);
        break;
      case (SNMP_ASN1_UNIV | SNMP_ASN1_PRIMIT | SNMP_ASN1_OC_STR):
      case (SNMP_ASN1_APPLIC | SNMP_ASN1_PRIMIT | SNMP_ASN1_IPADDR):
      case (SNMP_ASN1_APPLIC | SNMP_ASN1_PRIMIT | SNMP_ASN1_OPAQUE):
        raw_ptr = (u8_t*)vb->value;
        snmp_asn1_enc_raw(p, ofs, vb->vlen, raw_ptr);
        break;
      case (SNMP_ASN1_UNIV | SNMP_ASN1_PRIMIT | SNMP_ASN1_NUL):
        break;
      case (SNMP_ASN1_UNIV | SNMP_ASN1_PRIMIT | SNMP_ASN1_OBJ_ID):
        sint_ptr = (s32_t*)vb->value;
        snmp_asn1_enc_oid(p, ofs, vb->value_len / sizeof(s32_t), sint_ptr);
        break;
      default:
        /* unsupported type */
        break;
    };
    ofs += vb->vlen;
    vb = vb->next;
  }
  return ofs;
}

#endif /* LWIP_SNMP */
