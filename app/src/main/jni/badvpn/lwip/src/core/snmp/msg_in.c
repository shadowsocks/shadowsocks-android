/**
 * @file
 * SNMP input message processing (RFC1157).
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

#include "lwip/snmp.h"
#include "lwip/snmp_asn1.h"
#include "lwip/snmp_msg.h"
#include "lwip/snmp_structs.h"
#include "lwip/ip_addr.h"
#include "lwip/memp.h"
#include "lwip/udp.h"
#include "lwip/stats.h"

#include <string.h>

/* public (non-static) constants */
/** SNMP v1 == 0 */
const s32_t snmp_version = 0;
/** default SNMP community string */
const char snmp_publiccommunity[7] = "public";

/* statically allocated buffers for SNMP_CONCURRENT_REQUESTS */
struct snmp_msg_pstat msg_input_list[SNMP_CONCURRENT_REQUESTS];
/* UDP Protocol Control Block */
struct udp_pcb *snmp1_pcb;

static void snmp_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, ip_addr_t *addr, u16_t port);
static err_t snmp_pdu_header_check(struct pbuf *p, u16_t ofs, u16_t pdu_len, u16_t *ofs_ret, struct snmp_msg_pstat *m_stat);
static err_t snmp_pdu_dec_varbindlist(struct pbuf *p, u16_t ofs, u16_t *ofs_ret, struct snmp_msg_pstat *m_stat);


/**
 * Starts SNMP Agent.
 * Allocates UDP pcb and binds it to IP_ADDR_ANY port 161.
 */
void
snmp_init(void)
{
  struct snmp_msg_pstat *msg_ps;
  u8_t i;

  snmp1_pcb = udp_new();
  if (snmp1_pcb != NULL)
  {
    udp_recv(snmp1_pcb, snmp_recv, (void *)SNMP_IN_PORT);
    udp_bind(snmp1_pcb, IP_ADDR_ANY, SNMP_IN_PORT);
  }
  msg_ps = &msg_input_list[0];
  for (i=0; i<SNMP_CONCURRENT_REQUESTS; i++)
  {
    msg_ps->state = SNMP_MSG_EMPTY;
    msg_ps->error_index = 0;
    msg_ps->error_status = SNMP_ES_NOERROR;
    msg_ps++;
  }
  trap_msg.pcb = snmp1_pcb;

#ifdef SNMP_PRIVATE_MIB_INIT
  /* If defined, this must be a function-like define to initialize the
   * private MIB after the stack has been initialized.
   * The private MIB can also be initialized in tcpip_callback (or after
   * the stack is initialized), this define is only for convenience. */
  SNMP_PRIVATE_MIB_INIT();
#endif /* SNMP_PRIVATE_MIB_INIT */

  /* The coldstart trap will only be output
     if our outgoing interface is up & configured  */
  snmp_coldstart_trap();
}

static void
snmp_error_response(struct snmp_msg_pstat *msg_ps, u8_t error)
{
  /* move names back from outvb to invb */
  int v;
  struct snmp_varbind *vbi = msg_ps->invb.head;
  struct snmp_varbind *vbo = msg_ps->outvb.head;
  for (v=0; v<msg_ps->vb_idx; v++) {
    vbi->ident_len = vbo->ident_len;
    vbo->ident_len = 0;
    vbi->ident = vbo->ident;
    vbo->ident = NULL;
    vbi = vbi->next;
    vbo = vbo->next;
  }
  /* free outvb */
  snmp_varbind_list_free(&msg_ps->outvb);
  /* we send invb back */
  msg_ps->outvb = msg_ps->invb;
  msg_ps->invb.head = NULL;
  msg_ps->invb.tail = NULL;
  msg_ps->invb.count = 0;
  msg_ps->error_status = error;
  /* error index must be 0 for error too big */
  msg_ps->error_index = (error != SNMP_ES_TOOBIG) ? (1 + msg_ps->vb_idx) : 0;
  snmp_send_response(msg_ps);
  snmp_varbind_list_free(&msg_ps->outvb);
  msg_ps->state = SNMP_MSG_EMPTY;
}

static void
snmp_ok_response(struct snmp_msg_pstat *msg_ps)
{
  err_t err_ret;

  err_ret = snmp_send_response(msg_ps);
  if (err_ret == ERR_MEM)
  {
    /* serious memory problem, can't return tooBig */
  }
  else
  {
    LWIP_DEBUGF(SNMP_MSG_DEBUG, ("snmp_msg_event = %"S32_F"\n",msg_ps->error_status));
  }
  /* free varbinds (if available) */
  snmp_varbind_list_free(&msg_ps->invb);
  snmp_varbind_list_free(&msg_ps->outvb);
  msg_ps->state = SNMP_MSG_EMPTY;
}

/**
 * Service an internal or external event for SNMP GET.
 *
 * @param request_id identifies requests from 0 to (SNMP_CONCURRENT_REQUESTS-1)
 * @param msg_ps points to the assosicated message process state
 */
static void
snmp_msg_get_event(u8_t request_id, struct snmp_msg_pstat *msg_ps)
{
  LWIP_DEBUGF(SNMP_MSG_DEBUG, ("snmp_msg_get_event: msg_ps->state==%"U16_F"\n",(u16_t)msg_ps->state));

  if (msg_ps->state == SNMP_MSG_EXTERNAL_GET_OBJDEF)
  {
    struct mib_external_node *en;
    struct snmp_name_ptr np;

    /* get_object_def() answer*/
    en = msg_ps->ext_mib_node;
    np = msg_ps->ext_name_ptr;

    /* translate answer into a known lifeform */
    en->get_object_def_a(request_id, np.ident_len, np.ident, &msg_ps->ext_object_def);
    if ((msg_ps->ext_object_def.instance != MIB_OBJECT_NONE) &&
        (msg_ps->ext_object_def.access & MIB_ACCESS_READ))
    {
      msg_ps->state = SNMP_MSG_EXTERNAL_GET_VALUE;
      en->get_value_q(request_id, &msg_ps->ext_object_def);
    }
    else
    {
      en->get_object_def_pc(request_id, np.ident_len, np.ident);
      /* search failed, object id points to unknown object (nosuchname) */
      snmp_error_response(msg_ps,SNMP_ES_NOSUCHNAME);
    }
  }
  else if (msg_ps->state == SNMP_MSG_EXTERNAL_GET_VALUE)
  {
    struct mib_external_node *en;
    struct snmp_varbind *vb;

    /* get_value() answer */
    en = msg_ps->ext_mib_node;

    /* allocate output varbind */
    vb = (struct snmp_varbind *)memp_malloc(MEMP_SNMP_VARBIND);
    if (vb != NULL)
    {
      vb->next = NULL;
      vb->prev = NULL;

      /* move name from invb to outvb */
      vb->ident = msg_ps->vb_ptr->ident;
      vb->ident_len = msg_ps->vb_ptr->ident_len;
      /* ensure this memory is refereced once only */
      msg_ps->vb_ptr->ident = NULL;
      msg_ps->vb_ptr->ident_len = 0;

      vb->value_type = msg_ps->ext_object_def.asn_type;
      LWIP_ASSERT("invalid length", msg_ps->ext_object_def.v_len <= 0xff);
      vb->value_len = (u8_t)msg_ps->ext_object_def.v_len;
      if (vb->value_len > 0)
      {
        LWIP_ASSERT("SNMP_MAX_OCTET_STRING_LEN is configured too low", vb->value_len <= SNMP_MAX_VALUE_SIZE);
        vb->value = memp_malloc(MEMP_SNMP_VALUE);
        if (vb->value != NULL)
        {
          en->get_value_a(request_id, &msg_ps->ext_object_def, vb->value_len, vb->value);
          snmp_varbind_tail_add(&msg_ps->outvb, vb);
          /* search again (if vb_idx < msg_ps->invb.count) */
          msg_ps->state = SNMP_MSG_SEARCH_OBJ;
          msg_ps->vb_idx += 1;
        }
        else
        {
          en->get_value_pc(request_id, &msg_ps->ext_object_def);
          LWIP_DEBUGF(SNMP_MSG_DEBUG, ("snmp_msg_event: no variable space\n"));
          msg_ps->vb_ptr->ident = vb->ident;
          msg_ps->vb_ptr->ident_len = vb->ident_len;
          memp_free(MEMP_SNMP_VARBIND, vb);
          snmp_error_response(msg_ps,SNMP_ES_TOOBIG);
        }
      }
      else
      {
        /* vb->value_len == 0, empty value (e.g. empty string) */
        en->get_value_a(request_id, &msg_ps->ext_object_def, 0, NULL);
        vb->value = NULL;
        snmp_varbind_tail_add(&msg_ps->outvb, vb);
        /* search again (if vb_idx < msg_ps->invb.count) */
        msg_ps->state = SNMP_MSG_SEARCH_OBJ;
        msg_ps->vb_idx += 1;
      }
    }
    else
    {
      en->get_value_pc(request_id, &msg_ps->ext_object_def);
      LWIP_DEBUGF(SNMP_MSG_DEBUG, ("snmp_msg_event: no outvb space\n"));
      snmp_error_response(msg_ps,SNMP_ES_TOOBIG);
    }
  }

  while ((msg_ps->state == SNMP_MSG_SEARCH_OBJ) &&
         (msg_ps->vb_idx < msg_ps->invb.count))
  {
    struct mib_node *mn;
    struct snmp_name_ptr np;

    if (msg_ps->vb_idx == 0)
    {
      msg_ps->vb_ptr = msg_ps->invb.head;
    }
    else
    {
      msg_ps->vb_ptr = msg_ps->vb_ptr->next;
    }
    /** test object identifier for .iso.org.dod.internet prefix */
    if (snmp_iso_prefix_tst(msg_ps->vb_ptr->ident_len,  msg_ps->vb_ptr->ident))
    {
      mn = snmp_search_tree((struct mib_node*)&internet, msg_ps->vb_ptr->ident_len - 4,
                             msg_ps->vb_ptr->ident + 4, &np);
      if (mn != NULL)
      {
        if (mn->node_type == MIB_NODE_EX)
        {
          /* external object */
          struct mib_external_node *en = (struct mib_external_node*)mn;

          msg_ps->state = SNMP_MSG_EXTERNAL_GET_OBJDEF;
          /* save en && args in msg_ps!! */
          msg_ps->ext_mib_node = en;
          msg_ps->ext_name_ptr = np;

          en->get_object_def_q(en->addr_inf, request_id, np.ident_len, np.ident);
        }
        else
        {
          /* internal object */
          struct obj_def object_def;

          msg_ps->state = SNMP_MSG_INTERNAL_GET_OBJDEF;
          mn->get_object_def(np.ident_len, np.ident, &object_def);
          if ((object_def.instance != MIB_OBJECT_NONE) &&
            (object_def.access & MIB_ACCESS_READ))
          {
            mn = mn;
          }
          else
          {
            /* search failed, object id points to unknown object (nosuchname) */
            mn =  NULL;
          }
          if (mn != NULL)
          {
            struct snmp_varbind *vb;

            msg_ps->state = SNMP_MSG_INTERNAL_GET_VALUE;
            /* allocate output varbind */
            vb = (struct snmp_varbind *)memp_malloc(MEMP_SNMP_VARBIND);
            if (vb != NULL)
            {
              vb->next = NULL;
              vb->prev = NULL;

              /* move name from invb to outvb */
              vb->ident = msg_ps->vb_ptr->ident;
              vb->ident_len = msg_ps->vb_ptr->ident_len;
              /* ensure this memory is refereced once only */
              msg_ps->vb_ptr->ident = NULL;
              msg_ps->vb_ptr->ident_len = 0;

              vb->value_type = object_def.asn_type;
              LWIP_ASSERT("invalid length", object_def.v_len <= 0xff);
              vb->value_len = (u8_t)object_def.v_len;
              if (vb->value_len > 0)
              {
                LWIP_ASSERT("SNMP_MAX_OCTET_STRING_LEN is configured too low",
                  vb->value_len <= SNMP_MAX_VALUE_SIZE);
                vb->value = memp_malloc(MEMP_SNMP_VALUE);
                if (vb->value != NULL)
                {
                  mn->get_value(&object_def, vb->value_len, vb->value);
                  snmp_varbind_tail_add(&msg_ps->outvb, vb);
                  msg_ps->state = SNMP_MSG_SEARCH_OBJ;
                  msg_ps->vb_idx += 1;
                }
                else
                {
                  LWIP_DEBUGF(SNMP_MSG_DEBUG, ("snmp_msg_event: couldn't allocate variable space\n"));
                  msg_ps->vb_ptr->ident = vb->ident;
                  msg_ps->vb_ptr->ident_len = vb->ident_len;
                  vb->ident = NULL;
                  vb->ident_len = 0;
                  memp_free(MEMP_SNMP_VARBIND, vb);
                  snmp_error_response(msg_ps,SNMP_ES_TOOBIG);
                }
              }
              else
              {
                /* vb->value_len == 0, empty value (e.g. empty string) */
                vb->value = NULL;
                snmp_varbind_tail_add(&msg_ps->outvb, vb);
                msg_ps->state = SNMP_MSG_SEARCH_OBJ;
                msg_ps->vb_idx += 1;
              }
            }
            else
            {
              LWIP_DEBUGF(SNMP_MSG_DEBUG, ("snmp_msg_event: couldn't allocate outvb space\n"));
              snmp_error_response(msg_ps,SNMP_ES_TOOBIG);
            }
          }
        }
      }
    }
    else
    {
      mn = NULL;
    }
    if (mn == NULL)
    {
      /* mn == NULL, noSuchName */
      snmp_error_response(msg_ps,SNMP_ES_NOSUCHNAME);
    }
  }
  if ((msg_ps->state == SNMP_MSG_SEARCH_OBJ) &&
      (msg_ps->vb_idx == msg_ps->invb.count))
  {
    snmp_ok_response(msg_ps);
  }
}

/**
 * Service an internal or external event for SNMP GETNEXT.
 *
 * @param request_id identifies requests from 0 to (SNMP_CONCURRENT_REQUESTS-1)
 * @param msg_ps points to the assosicated message process state
 */
static void
snmp_msg_getnext_event(u8_t request_id, struct snmp_msg_pstat *msg_ps)
{
  LWIP_DEBUGF(SNMP_MSG_DEBUG, ("snmp_msg_getnext_event: msg_ps->state==%"U16_F"\n",(u16_t)msg_ps->state));

  if (msg_ps->state == SNMP_MSG_EXTERNAL_GET_OBJDEF)
  {
    struct mib_external_node *en;

    /* get_object_def() answer*/
    en = msg_ps->ext_mib_node;

    /* translate answer into a known lifeform */
    en->get_object_def_a(request_id, 1, &msg_ps->ext_oid.id[msg_ps->ext_oid.len - 1], &msg_ps->ext_object_def);
    if (msg_ps->ext_object_def.instance != MIB_OBJECT_NONE)
    {
      msg_ps->state = SNMP_MSG_EXTERNAL_GET_VALUE;
      en->get_value_q(request_id, &msg_ps->ext_object_def);
    }
    else
    {
      en->get_object_def_pc(request_id, 1, &msg_ps->ext_oid.id[msg_ps->ext_oid.len - 1]);
      /* search failed, object id points to unknown object (nosuchname) */
      snmp_error_response(msg_ps,SNMP_ES_NOSUCHNAME);
    }
  }
  else if (msg_ps->state == SNMP_MSG_EXTERNAL_GET_VALUE)
  {
    struct mib_external_node *en;
    struct snmp_varbind *vb;

    /* get_value() answer */
    en = msg_ps->ext_mib_node;

    LWIP_ASSERT("invalid length", msg_ps->ext_object_def.v_len <= 0xff);
    vb = snmp_varbind_alloc(&msg_ps->ext_oid,
                            msg_ps->ext_object_def.asn_type,
                            (u8_t)msg_ps->ext_object_def.v_len);
    if (vb != NULL)
    {
      en->get_value_a(request_id, &msg_ps->ext_object_def, vb->value_len, vb->value);
      snmp_varbind_tail_add(&msg_ps->outvb, vb);
      msg_ps->state = SNMP_MSG_SEARCH_OBJ;
      msg_ps->vb_idx += 1;
    }
    else
    {
      en->get_value_pc(request_id, &msg_ps->ext_object_def);
      LWIP_DEBUGF(SNMP_MSG_DEBUG, ("snmp_msg_getnext_event: couldn't allocate outvb space\n"));
      snmp_error_response(msg_ps,SNMP_ES_TOOBIG);
    }
  }

  while ((msg_ps->state == SNMP_MSG_SEARCH_OBJ) &&
         (msg_ps->vb_idx < msg_ps->invb.count))
  {
    struct mib_node *mn;
    struct snmp_obj_id oid;

    if (msg_ps->vb_idx == 0)
    {
      msg_ps->vb_ptr = msg_ps->invb.head;
    }
    else
    {
      msg_ps->vb_ptr = msg_ps->vb_ptr->next;
    }
    if (snmp_iso_prefix_expand(msg_ps->vb_ptr->ident_len, msg_ps->vb_ptr->ident, &oid))
    {
      if (msg_ps->vb_ptr->ident_len > 3)
      {
        /* can offset ident_len and ident */
        mn = snmp_expand_tree((struct mib_node*)&internet,
                              msg_ps->vb_ptr->ident_len - 4,
                              msg_ps->vb_ptr->ident + 4, &oid);
      }
      else
      {
        /* can't offset ident_len -4, ident + 4 */
        mn = snmp_expand_tree((struct mib_node*)&internet, 0, NULL, &oid);
      }
    }
    else
    {
      mn = NULL;
    }
    if (mn != NULL)
    {
      if (mn->node_type == MIB_NODE_EX)
      {
        /* external object */
        struct mib_external_node *en = (struct mib_external_node*)mn;

        msg_ps->state = SNMP_MSG_EXTERNAL_GET_OBJDEF;
        /* save en && args in msg_ps!! */
        msg_ps->ext_mib_node = en;
        msg_ps->ext_oid = oid;

        en->get_object_def_q(en->addr_inf, request_id, 1, &oid.id[oid.len - 1]);
      }
      else
      {
        /* internal object */
        struct obj_def object_def;
        struct snmp_varbind *vb;

        msg_ps->state = SNMP_MSG_INTERNAL_GET_OBJDEF;
        mn->get_object_def(1, &oid.id[oid.len - 1], &object_def);

        LWIP_ASSERT("invalid length", object_def.v_len <= 0xff);
        vb = snmp_varbind_alloc(&oid, object_def.asn_type, (u8_t)object_def.v_len);
        if (vb != NULL)
        {
          msg_ps->state = SNMP_MSG_INTERNAL_GET_VALUE;
          mn->get_value(&object_def, object_def.v_len, vb->value);
          snmp_varbind_tail_add(&msg_ps->outvb, vb);
          msg_ps->state = SNMP_MSG_SEARCH_OBJ;
          msg_ps->vb_idx += 1;
        }
        else
        {
          LWIP_DEBUGF(SNMP_MSG_DEBUG, ("snmp_recv couldn't allocate outvb space\n"));
          snmp_error_response(msg_ps,SNMP_ES_TOOBIG);
        }
      }
    }
    if (mn == NULL)
    {
      /* mn == NULL, noSuchName */
      snmp_error_response(msg_ps,SNMP_ES_NOSUCHNAME);
    }
  }
  if ((msg_ps->state == SNMP_MSG_SEARCH_OBJ) &&
      (msg_ps->vb_idx == msg_ps->invb.count))
  {
    snmp_ok_response(msg_ps);
  }
}

/**
 * Service an internal or external event for SNMP SET.
 *
 * @param request_id identifies requests from 0 to (SNMP_CONCURRENT_REQUESTS-1)
 * @param msg_ps points to the assosicated message process state
 */
static void
snmp_msg_set_event(u8_t request_id, struct snmp_msg_pstat *msg_ps)
{
  LWIP_DEBUGF(SNMP_MSG_DEBUG, ("snmp_msg_set_event: msg_ps->state==%"U16_F"\n",(u16_t)msg_ps->state));

  if (msg_ps->state == SNMP_MSG_EXTERNAL_GET_OBJDEF)
  {
    struct mib_external_node *en;
    struct snmp_name_ptr np;

    /* get_object_def() answer*/
    en = msg_ps->ext_mib_node;
    np = msg_ps->ext_name_ptr;

    /* translate answer into a known lifeform */
    en->get_object_def_a(request_id, np.ident_len, np.ident, &msg_ps->ext_object_def);
    if (msg_ps->ext_object_def.instance != MIB_OBJECT_NONE)
    {
      msg_ps->state = SNMP_MSG_EXTERNAL_SET_TEST;
      en->set_test_q(request_id, &msg_ps->ext_object_def);
    }
    else
    {
      en->get_object_def_pc(request_id, np.ident_len, np.ident);
      /* search failed, object id points to unknown object (nosuchname) */
      snmp_error_response(msg_ps,SNMP_ES_NOSUCHNAME);
    }
  }
  else if (msg_ps->state == SNMP_MSG_EXTERNAL_SET_TEST)
  {
    struct mib_external_node *en;

    /* set_test() answer*/
    en = msg_ps->ext_mib_node;

    if (msg_ps->ext_object_def.access & MIB_ACCESS_WRITE)
    {
       if ((msg_ps->ext_object_def.asn_type == msg_ps->vb_ptr->value_type) &&
           (en->set_test_a(request_id,&msg_ps->ext_object_def,
                           msg_ps->vb_ptr->value_len,msg_ps->vb_ptr->value) != 0))
      {
        msg_ps->state = SNMP_MSG_SEARCH_OBJ;
        msg_ps->vb_idx += 1;
      }
      else
      {
        en->set_test_pc(request_id,&msg_ps->ext_object_def);
        /* bad value */
        snmp_error_response(msg_ps,SNMP_ES_BADVALUE);
      }
    }
    else
    {
      en->set_test_pc(request_id,&msg_ps->ext_object_def);
      /* object not available for set */
      snmp_error_response(msg_ps,SNMP_ES_NOSUCHNAME);
    }
  }
  else if (msg_ps->state == SNMP_MSG_EXTERNAL_GET_OBJDEF_S)
  {
    struct mib_external_node *en;
    struct snmp_name_ptr np;

    /* get_object_def() answer*/
    en = msg_ps->ext_mib_node;
    np = msg_ps->ext_name_ptr;

    /* translate answer into a known lifeform */
    en->get_object_def_a(request_id, np.ident_len, np.ident, &msg_ps->ext_object_def);
    if (msg_ps->ext_object_def.instance != MIB_OBJECT_NONE)
    {
      msg_ps->state = SNMP_MSG_EXTERNAL_SET_VALUE;
      en->set_value_q(request_id, &msg_ps->ext_object_def,
                      msg_ps->vb_ptr->value_len,msg_ps->vb_ptr->value);
    }
    else
    {
      en->get_object_def_pc(request_id, np.ident_len, np.ident);
      /* set_value failed, object has disappeared for some odd reason?? */
      snmp_error_response(msg_ps,SNMP_ES_GENERROR);
    }
  }
  else if (msg_ps->state == SNMP_MSG_EXTERNAL_SET_VALUE)
  {
    struct mib_external_node *en;

    /** set_value_a() */
    en = msg_ps->ext_mib_node;
    en->set_value_a(request_id, &msg_ps->ext_object_def,
      msg_ps->vb_ptr->value_len, msg_ps->vb_ptr->value);

    /** @todo use set_value_pc() if toobig */
    msg_ps->state = SNMP_MSG_INTERNAL_SET_VALUE;
    msg_ps->vb_idx += 1;
  }

  /* test all values before setting */
  while ((msg_ps->state == SNMP_MSG_SEARCH_OBJ) &&
         (msg_ps->vb_idx < msg_ps->invb.count))
  {
    struct mib_node *mn;
    struct snmp_name_ptr np;

    if (msg_ps->vb_idx == 0)
    {
      msg_ps->vb_ptr = msg_ps->invb.head;
    }
    else
    {
      msg_ps->vb_ptr = msg_ps->vb_ptr->next;
    }
    /** test object identifier for .iso.org.dod.internet prefix */
    if (snmp_iso_prefix_tst(msg_ps->vb_ptr->ident_len,  msg_ps->vb_ptr->ident))
    {
      mn = snmp_search_tree((struct mib_node*)&internet, msg_ps->vb_ptr->ident_len - 4,
                             msg_ps->vb_ptr->ident + 4, &np);
      if (mn != NULL)
      {
        if (mn->node_type == MIB_NODE_EX)
        {
          /* external object */
          struct mib_external_node *en = (struct mib_external_node*)mn;

          msg_ps->state = SNMP_MSG_EXTERNAL_GET_OBJDEF;
          /* save en && args in msg_ps!! */
          msg_ps->ext_mib_node = en;
          msg_ps->ext_name_ptr = np;

          en->get_object_def_q(en->addr_inf, request_id, np.ident_len, np.ident);
        }
        else
        {
          /* internal object */
          struct obj_def object_def;

          msg_ps->state = SNMP_MSG_INTERNAL_GET_OBJDEF;
          mn->get_object_def(np.ident_len, np.ident, &object_def);
          if (object_def.instance != MIB_OBJECT_NONE)
          {
            mn = mn;
          }
          else
          {
            /* search failed, object id points to unknown object (nosuchname) */
            mn = NULL;
          }
          if (mn != NULL)
          {
            msg_ps->state = SNMP_MSG_INTERNAL_SET_TEST;

            if (object_def.access & MIB_ACCESS_WRITE)
            {
              if ((object_def.asn_type == msg_ps->vb_ptr->value_type) &&
                  (mn->set_test(&object_def,msg_ps->vb_ptr->value_len,msg_ps->vb_ptr->value) != 0))
              {
                msg_ps->state = SNMP_MSG_SEARCH_OBJ;
                msg_ps->vb_idx += 1;
              }
              else
              {
                /* bad value */
                snmp_error_response(msg_ps,SNMP_ES_BADVALUE);
              }
            }
            else
            {
              /* object not available for set */
              snmp_error_response(msg_ps,SNMP_ES_NOSUCHNAME);
            }
          }
        }
      }
    }
    else
    {
      mn = NULL;
    }
    if (mn == NULL)
    {
      /* mn == NULL, noSuchName */
      snmp_error_response(msg_ps,SNMP_ES_NOSUCHNAME);
    }
  }

  if ((msg_ps->state == SNMP_MSG_SEARCH_OBJ) &&
      (msg_ps->vb_idx == msg_ps->invb.count))
  {
    msg_ps->vb_idx = 0;
    msg_ps->state = SNMP_MSG_INTERNAL_SET_VALUE;
  }

  /* set all values "atomically" (be as "atomic" as possible) */
  while ((msg_ps->state == SNMP_MSG_INTERNAL_SET_VALUE) &&
         (msg_ps->vb_idx < msg_ps->invb.count))
  {
    struct mib_node *mn;
    struct snmp_name_ptr np;

    if (msg_ps->vb_idx == 0)
    {
      msg_ps->vb_ptr = msg_ps->invb.head;
    }
    else
    {
      msg_ps->vb_ptr = msg_ps->vb_ptr->next;
    }
    /* skip iso prefix test, was done previously while settesting() */
    mn = snmp_search_tree((struct mib_node*)&internet, msg_ps->vb_ptr->ident_len - 4,
                           msg_ps->vb_ptr->ident + 4, &np);
    /* check if object is still available
       (e.g. external hot-plug thingy present?) */
    if (mn != NULL)
    {
      if (mn->node_type == MIB_NODE_EX)
      {
        /* external object */
        struct mib_external_node *en = (struct mib_external_node*)mn;

        msg_ps->state = SNMP_MSG_EXTERNAL_GET_OBJDEF_S;
        /* save en && args in msg_ps!! */
        msg_ps->ext_mib_node = en;
        msg_ps->ext_name_ptr = np;

        en->get_object_def_q(en->addr_inf, request_id, np.ident_len, np.ident);
      }
      else
      {
        /* internal object */
        struct obj_def object_def;

        msg_ps->state = SNMP_MSG_INTERNAL_GET_OBJDEF_S;
        mn->get_object_def(np.ident_len, np.ident, &object_def);
        msg_ps->state = SNMP_MSG_INTERNAL_SET_VALUE;
        mn->set_value(&object_def,msg_ps->vb_ptr->value_len,msg_ps->vb_ptr->value);
        msg_ps->vb_idx += 1;
      }
    }
  }
  if ((msg_ps->state == SNMP_MSG_INTERNAL_SET_VALUE) &&
      (msg_ps->vb_idx == msg_ps->invb.count))
  {
    /* simply echo the input if we can set it
       @todo do we need to return the actual value?
       e.g. if value is silently modified or behaves sticky? */
    msg_ps->outvb = msg_ps->invb;
    msg_ps->invb.head = NULL;
    msg_ps->invb.tail = NULL;
    msg_ps->invb.count = 0;
    snmp_ok_response(msg_ps);
  }
}


/**
 * Handle one internal or external event.
 * Called for one async event. (recv external/private answer)
 *
 * @param request_id identifies requests from 0 to (SNMP_CONCURRENT_REQUESTS-1)
 */
void
snmp_msg_event(u8_t request_id)
{
  struct snmp_msg_pstat *msg_ps;

  if (request_id < SNMP_CONCURRENT_REQUESTS)
  {
    msg_ps = &msg_input_list[request_id];
    if (msg_ps->rt == SNMP_ASN1_PDU_GET_NEXT_REQ)
    {
      snmp_msg_getnext_event(request_id, msg_ps);
    }
    else if (msg_ps->rt == SNMP_ASN1_PDU_GET_REQ)
    {
      snmp_msg_get_event(request_id, msg_ps);
    }
    else if(msg_ps->rt == SNMP_ASN1_PDU_SET_REQ)
    {
      snmp_msg_set_event(request_id, msg_ps);
    }
  }
}


/* lwIP UDP receive callback function */
static void
snmp_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, ip_addr_t *addr, u16_t port)
{
  struct snmp_msg_pstat *msg_ps;
  u8_t req_idx;
  err_t err_ret;
  u16_t payload_len = p->tot_len;
  u16_t payload_ofs = 0;
  u16_t varbind_ofs = 0;

  /* suppress unused argument warning */
  LWIP_UNUSED_ARG(arg);

  /* traverse input message process list, look for SNMP_MSG_EMPTY */
  msg_ps = &msg_input_list[0];
  req_idx = 0;
  while ((req_idx < SNMP_CONCURRENT_REQUESTS) && (msg_ps->state != SNMP_MSG_EMPTY))
  {
    req_idx++;
    msg_ps++;
  }
  if (req_idx == SNMP_CONCURRENT_REQUESTS)
  {
    /* exceeding number of concurrent requests */
    pbuf_free(p);
    return;
  }

  /* accepting request */
  snmp_inc_snmpinpkts();
  /* record used 'protocol control block' */
  msg_ps->pcb = pcb;
  /* source address (network order) */
  msg_ps->sip = *addr;
  /* source port (host order (lwIP oddity)) */
  msg_ps->sp = port;

  /* check total length, version, community, pdu type */
  err_ret = snmp_pdu_header_check(p, payload_ofs, payload_len, &varbind_ofs, msg_ps);
  /* Only accept requests and requests without error (be robust) */
  /* Reject response and trap headers or error requests as input! */
  if ((err_ret != ERR_OK) ||
      ((msg_ps->rt != SNMP_ASN1_PDU_GET_REQ) &&
       (msg_ps->rt != SNMP_ASN1_PDU_GET_NEXT_REQ) &&
       (msg_ps->rt != SNMP_ASN1_PDU_SET_REQ)) ||
      ((msg_ps->error_status != SNMP_ES_NOERROR) ||
       (msg_ps->error_index != 0)) )
  {
    /* header check failed drop request silently, do not return error! */
    pbuf_free(p);
    LWIP_DEBUGF(SNMP_MSG_DEBUG, ("snmp_pdu_header_check() failed\n"));
    return;
  }
  LWIP_DEBUGF(SNMP_MSG_DEBUG, ("snmp_recv ok, community %s\n", msg_ps->community));

  /* Builds a list of variable bindings. Copy the varbinds from the pbuf
    chain to glue them when these are divided over two or more pbuf's. */
  err_ret = snmp_pdu_dec_varbindlist(p, varbind_ofs, &varbind_ofs, msg_ps);
  /* we've decoded the incoming message, release input msg now */
  pbuf_free(p);
  if ((err_ret != ERR_OK) || (msg_ps->invb.count == 0))
  {
    /* varbind-list decode failed, or varbind list empty.
       drop request silently, do not return error!
       (errors are only returned for a specific varbind failure) */
    LWIP_DEBUGF(SNMP_MSG_DEBUG, ("snmp_pdu_dec_varbindlist() failed\n"));
    return;
  }

  msg_ps->error_status = SNMP_ES_NOERROR;
  msg_ps->error_index = 0;
  /* find object for each variable binding */
  msg_ps->state = SNMP_MSG_SEARCH_OBJ;
  /* first variable binding from list to inspect */
  msg_ps->vb_idx = 0;

  LWIP_DEBUGF(SNMP_MSG_DEBUG, ("snmp_recv varbind cnt=%"U16_F"\n",(u16_t)msg_ps->invb.count));

  /* handle input event and as much objects as possible in one go */
  snmp_msg_event(req_idx);
}

/**
 * Checks and decodes incoming SNMP message header, logs header errors.
 *
 * @param p points to pbuf chain of SNMP message (UDP payload)
 * @param ofs points to first octet of SNMP message
 * @param pdu_len the length of the UDP payload
 * @param ofs_ret returns the ofset of the variable bindings
 * @param m_stat points to the current message request state return
 * @return
 * - ERR_OK SNMP header is sane and accepted
 * - ERR_ARG SNMP header is either malformed or rejected
 */
static err_t
snmp_pdu_header_check(struct pbuf *p, u16_t ofs, u16_t pdu_len, u16_t *ofs_ret, struct snmp_msg_pstat *m_stat)
{
  err_t derr;
  u16_t len, ofs_base;
  u8_t  len_octets;
  u8_t  type;
  s32_t version;

  ofs_base = ofs;
  snmp_asn1_dec_type(p, ofs, &type);
  derr = snmp_asn1_dec_length(p, ofs+1, &len_octets, &len);
  if ((derr != ERR_OK) ||
      (pdu_len != (1 + len_octets + len)) ||
      (type != (SNMP_ASN1_UNIV | SNMP_ASN1_CONSTR | SNMP_ASN1_SEQ)))
  {
    snmp_inc_snmpinasnparseerrs();
    return ERR_ARG;
  }
  ofs += (1 + len_octets);
  snmp_asn1_dec_type(p, ofs, &type);
  derr = snmp_asn1_dec_length(p, ofs+1, &len_octets, &len);
  if ((derr != ERR_OK) || (type != (SNMP_ASN1_UNIV | SNMP_ASN1_PRIMIT | SNMP_ASN1_INTEG)))
  {
    /* can't decode or no integer (version) */
    snmp_inc_snmpinasnparseerrs();
    return ERR_ARG;
  }
  derr = snmp_asn1_dec_s32t(p, ofs + 1 + len_octets, len, &version);
  if (derr != ERR_OK)
  {
    /* can't decode */
    snmp_inc_snmpinasnparseerrs();
    return ERR_ARG;
  }
  if (version != 0)
  {
    /* not version 1 */
    snmp_inc_snmpinbadversions();
    return ERR_ARG;
  }
  ofs += (1 + len_octets + len);
  snmp_asn1_dec_type(p, ofs, &type);
  derr = snmp_asn1_dec_length(p, ofs+1, &len_octets, &len);
  if ((derr != ERR_OK) || (type != (SNMP_ASN1_UNIV | SNMP_ASN1_PRIMIT | SNMP_ASN1_OC_STR)))
  {
    /* can't decode or no octet string (community) */
    snmp_inc_snmpinasnparseerrs();
    return ERR_ARG;
  }
  derr = snmp_asn1_dec_raw(p, ofs + 1 + len_octets, len, SNMP_COMMUNITY_STR_LEN, m_stat->community);
  if (derr != ERR_OK)
  {
    snmp_inc_snmpinasnparseerrs();
    return ERR_ARG;
  }
  /* add zero terminator */
  len = ((len < (SNMP_COMMUNITY_STR_LEN))?(len):(SNMP_COMMUNITY_STR_LEN));
  m_stat->community[len] = 0;
  m_stat->com_strlen = (u8_t)len;
  if (strncmp(snmp_publiccommunity, (const char*)m_stat->community, SNMP_COMMUNITY_STR_LEN) != 0)
  {
    /** @todo: move this if we need to check more names */
    snmp_inc_snmpinbadcommunitynames();
    snmp_authfail_trap();
    return ERR_ARG;
  }
  ofs += (1 + len_octets + len);
  snmp_asn1_dec_type(p, ofs, &type);
  derr = snmp_asn1_dec_length(p, ofs+1, &len_octets, &len);
  if (derr != ERR_OK)
  {
    snmp_inc_snmpinasnparseerrs();
    return ERR_ARG;
  }
  switch(type)
  {
    case (SNMP_ASN1_CONTXT | SNMP_ASN1_CONSTR | SNMP_ASN1_PDU_GET_REQ):
      /* GetRequest PDU */
      snmp_inc_snmpingetrequests();
      derr = ERR_OK;
      break;
    case (SNMP_ASN1_CONTXT | SNMP_ASN1_CONSTR | SNMP_ASN1_PDU_GET_NEXT_REQ):
      /* GetNextRequest PDU */
      snmp_inc_snmpingetnexts();
      derr = ERR_OK;
      break;
    case (SNMP_ASN1_CONTXT | SNMP_ASN1_CONSTR | SNMP_ASN1_PDU_GET_RESP):
      /* GetResponse PDU */
      snmp_inc_snmpingetresponses();
      derr = ERR_ARG;
      break;
    case (SNMP_ASN1_CONTXT | SNMP_ASN1_CONSTR | SNMP_ASN1_PDU_SET_REQ):
      /* SetRequest PDU */
      snmp_inc_snmpinsetrequests();
      derr = ERR_OK;
      break;
    case (SNMP_ASN1_CONTXT | SNMP_ASN1_CONSTR | SNMP_ASN1_PDU_TRAP):
      /* Trap PDU */
      snmp_inc_snmpintraps();
      derr = ERR_ARG;
      break;
    default:
      snmp_inc_snmpinasnparseerrs();
      derr = ERR_ARG;
      break;
  }
  if (derr != ERR_OK)
  {
    /* unsupported input PDU for this agent (no parse error) */
    return ERR_ARG;
  }
  m_stat->rt = type & 0x1F;
  ofs += (1 + len_octets);
  if (len != (pdu_len - (ofs - ofs_base)))
  {
    /* decoded PDU length does not equal actual payload length */
    snmp_inc_snmpinasnparseerrs();
    return ERR_ARG;
  }
  snmp_asn1_dec_type(p, ofs, &type);
  derr = snmp_asn1_dec_length(p, ofs+1, &len_octets, &len);
  if ((derr != ERR_OK) || (type != (SNMP_ASN1_UNIV | SNMP_ASN1_PRIMIT | SNMP_ASN1_INTEG)))
  {
    /* can't decode or no integer (request ID) */
    snmp_inc_snmpinasnparseerrs();
    return ERR_ARG;
  }
  derr = snmp_asn1_dec_s32t(p, ofs + 1 + len_octets, len, &m_stat->rid);
  if (derr != ERR_OK)
  {
    /* can't decode */
    snmp_inc_snmpinasnparseerrs();
    return ERR_ARG;
  }
  ofs += (1 + len_octets + len);
  snmp_asn1_dec_type(p, ofs, &type);
  derr = snmp_asn1_dec_length(p, ofs+1, &len_octets, &len);
  if ((derr != ERR_OK) || (type != (SNMP_ASN1_UNIV | SNMP_ASN1_PRIMIT | SNMP_ASN1_INTEG)))
  {
    /* can't decode or no integer (error-status) */
    snmp_inc_snmpinasnparseerrs();
    return ERR_ARG;
  }
  /* must be noError (0) for incoming requests.
     log errors for mib-2 completeness and for debug purposes */
  derr = snmp_asn1_dec_s32t(p, ofs + 1 + len_octets, len, &m_stat->error_status);
  if (derr != ERR_OK)
  {
    /* can't decode */
    snmp_inc_snmpinasnparseerrs();
    return ERR_ARG;
  }
  switch (m_stat->error_status)
  {
    case SNMP_ES_TOOBIG:
      snmp_inc_snmpintoobigs();
      break;
    case SNMP_ES_NOSUCHNAME:
      snmp_inc_snmpinnosuchnames();
      break;
    case SNMP_ES_BADVALUE:
      snmp_inc_snmpinbadvalues();
      break;
    case SNMP_ES_READONLY:
      snmp_inc_snmpinreadonlys();
      break;
    case SNMP_ES_GENERROR:
      snmp_inc_snmpingenerrs();
      break;
  }
  ofs += (1 + len_octets + len);
  snmp_asn1_dec_type(p, ofs, &type);
  derr = snmp_asn1_dec_length(p, ofs+1, &len_octets, &len);
  if ((derr != ERR_OK) || (type != (SNMP_ASN1_UNIV | SNMP_ASN1_PRIMIT | SNMP_ASN1_INTEG)))
  {
    /* can't decode or no integer (error-index) */
    snmp_inc_snmpinasnparseerrs();
    return ERR_ARG;
  }
  /* must be 0 for incoming requests.
     decode anyway to catch bad integers (and dirty tricks) */
  derr = snmp_asn1_dec_s32t(p, ofs + 1 + len_octets, len, &m_stat->error_index);
  if (derr != ERR_OK)
  {
    /* can't decode */
    snmp_inc_snmpinasnparseerrs();
    return ERR_ARG;
  }
  ofs += (1 + len_octets + len);
  *ofs_ret = ofs;
  return ERR_OK;
}

static err_t
snmp_pdu_dec_varbindlist(struct pbuf *p, u16_t ofs, u16_t *ofs_ret, struct snmp_msg_pstat *m_stat)
{
  err_t derr;
  u16_t len, vb_len;
  u8_t  len_octets;
  u8_t type;

  /* variable binding list */
  snmp_asn1_dec_type(p, ofs, &type);
  derr = snmp_asn1_dec_length(p, ofs+1, &len_octets, &vb_len);
  if ((derr != ERR_OK) ||
      (type != (SNMP_ASN1_UNIV | SNMP_ASN1_CONSTR | SNMP_ASN1_SEQ)))
  {
    snmp_inc_snmpinasnparseerrs();
    return ERR_ARG;
  }
  ofs += (1 + len_octets);

  /* start with empty list */
  m_stat->invb.count = 0;
  m_stat->invb.head = NULL;
  m_stat->invb.tail = NULL;

  while (vb_len > 0)
  {
    struct snmp_obj_id oid, oid_value;
    struct snmp_varbind *vb;

    snmp_asn1_dec_type(p, ofs, &type);
    derr = snmp_asn1_dec_length(p, ofs+1, &len_octets, &len);
    if ((derr != ERR_OK) ||
        (type != (SNMP_ASN1_UNIV | SNMP_ASN1_CONSTR | SNMP_ASN1_SEQ)) ||
        (len == 0) || (len > vb_len))
    {
      snmp_inc_snmpinasnparseerrs();
      /* free varbinds (if available) */
      snmp_varbind_list_free(&m_stat->invb);
      return ERR_ARG;
    }
    ofs += (1 + len_octets);
    vb_len -= (1 + len_octets);

    snmp_asn1_dec_type(p, ofs, &type);
    derr = snmp_asn1_dec_length(p, ofs+1, &len_octets, &len);
    if ((derr != ERR_OK) || (type != (SNMP_ASN1_UNIV | SNMP_ASN1_PRIMIT | SNMP_ASN1_OBJ_ID)))
    {
      /* can't decode object name length */
      snmp_inc_snmpinasnparseerrs();
      /* free varbinds (if available) */
      snmp_varbind_list_free(&m_stat->invb);
      return ERR_ARG;
    }
    derr = snmp_asn1_dec_oid(p, ofs + 1 + len_octets, len, &oid);
    if (derr != ERR_OK)
    {
      /* can't decode object name */
      snmp_inc_snmpinasnparseerrs();
      /* free varbinds (if available) */
      snmp_varbind_list_free(&m_stat->invb);
      return ERR_ARG;
    }
    ofs += (1 + len_octets + len);
    vb_len -= (1 + len_octets + len);

    snmp_asn1_dec_type(p, ofs, &type);
    derr = snmp_asn1_dec_length(p, ofs+1, &len_octets, &len);
    if (derr != ERR_OK)
    {
      /* can't decode object value length */
      snmp_inc_snmpinasnparseerrs();
      /* free varbinds (if available) */
      snmp_varbind_list_free(&m_stat->invb);
      return ERR_ARG;
    }

    switch (type)
    {
      case (SNMP_ASN1_UNIV | SNMP_ASN1_PRIMIT | SNMP_ASN1_INTEG):
        vb = snmp_varbind_alloc(&oid, type, sizeof(s32_t));
        if (vb != NULL)
        {
          s32_t *vptr = (s32_t*)vb->value;

          derr = snmp_asn1_dec_s32t(p, ofs + 1 + len_octets, len, vptr);
          snmp_varbind_tail_add(&m_stat->invb, vb);
        }
        else
        {
          derr = ERR_ARG;
        }
        break;
      case (SNMP_ASN1_APPLIC | SNMP_ASN1_PRIMIT | SNMP_ASN1_COUNTER):
      case (SNMP_ASN1_APPLIC | SNMP_ASN1_PRIMIT | SNMP_ASN1_GAUGE):
      case (SNMP_ASN1_APPLIC | SNMP_ASN1_PRIMIT | SNMP_ASN1_TIMETICKS):
        vb = snmp_varbind_alloc(&oid, type, sizeof(u32_t));
        if (vb != NULL)
        {
          u32_t *vptr = (u32_t*)vb->value;

          derr = snmp_asn1_dec_u32t(p, ofs + 1 + len_octets, len, vptr);
          snmp_varbind_tail_add(&m_stat->invb, vb);
        }
        else
        {
          derr = ERR_ARG;
        }
        break;
      case (SNMP_ASN1_UNIV | SNMP_ASN1_PRIMIT | SNMP_ASN1_OC_STR):
      case (SNMP_ASN1_APPLIC | SNMP_ASN1_PRIMIT | SNMP_ASN1_OPAQUE):
        LWIP_ASSERT("invalid length", len <= 0xff);
        vb = snmp_varbind_alloc(&oid, type, (u8_t)len);
        if (vb != NULL)
        {
          derr = snmp_asn1_dec_raw(p, ofs + 1 + len_octets, len, vb->value_len, (u8_t*)vb->value);
          snmp_varbind_tail_add(&m_stat->invb, vb);
        }
        else
        {
          derr = ERR_ARG;
        }
        break;
      case (SNMP_ASN1_UNIV | SNMP_ASN1_PRIMIT | SNMP_ASN1_NUL):
        vb = snmp_varbind_alloc(&oid, type, 0);
        if (vb != NULL)
        {
          snmp_varbind_tail_add(&m_stat->invb, vb);
          derr = ERR_OK;
        }
        else
        {
          derr = ERR_ARG;
        }
        break;
      case (SNMP_ASN1_UNIV | SNMP_ASN1_PRIMIT | SNMP_ASN1_OBJ_ID):
        derr = snmp_asn1_dec_oid(p, ofs + 1 + len_octets, len, &oid_value);
        if (derr == ERR_OK)
        {
          vb = snmp_varbind_alloc(&oid, type, oid_value.len * sizeof(s32_t));
          if (vb != NULL)
          {
            u8_t i = oid_value.len;
            s32_t *vptr = (s32_t*)vb->value;

            while(i > 0)
            {
              i--;
              vptr[i] = oid_value.id[i];
            }
            snmp_varbind_tail_add(&m_stat->invb, vb);
            derr = ERR_OK;
          }
          else
          {
            derr = ERR_ARG;
          }
        }
        break;
      case (SNMP_ASN1_APPLIC | SNMP_ASN1_PRIMIT | SNMP_ASN1_IPADDR):
        if (len == 4)
        {
          /* must be exactly 4 octets! */
          vb = snmp_varbind_alloc(&oid, type, 4);
          if (vb != NULL)
          {
            derr = snmp_asn1_dec_raw(p, ofs + 1 + len_octets, len, vb->value_len, (u8_t*)vb->value);
            snmp_varbind_tail_add(&m_stat->invb, vb);
          }
          else
          {
            derr = ERR_ARG;
          }
        }
        else
        {
          derr = ERR_ARG;
        }
        break;
      default:
        derr = ERR_ARG;
        break;
    }
    if (derr != ERR_OK)
    {
      snmp_inc_snmpinasnparseerrs();
      /* free varbinds (if available) */
      snmp_varbind_list_free(&m_stat->invb);
      return ERR_ARG;
    }
    ofs += (1 + len_octets + len);
    vb_len -= (1 + len_octets + len);
  }

  if (m_stat->rt == SNMP_ASN1_PDU_SET_REQ)
  {
    snmp_add_snmpintotalsetvars(m_stat->invb.count);
  }
  else
  {
    snmp_add_snmpintotalreqvars(m_stat->invb.count);
  }

  *ofs_ret = ofs;
  return ERR_OK;
}

struct snmp_varbind*
snmp_varbind_alloc(struct snmp_obj_id *oid, u8_t type, u8_t len)
{
  struct snmp_varbind *vb;

  vb = (struct snmp_varbind *)memp_malloc(MEMP_SNMP_VARBIND);
  if (vb != NULL)
  {
    u8_t i;

    vb->next = NULL;
    vb->prev = NULL;
    i = oid->len;
    vb->ident_len = i;
    if (i > 0)
    {
      LWIP_ASSERT("SNMP_MAX_TREE_DEPTH is configured too low", i <= SNMP_MAX_TREE_DEPTH);
      /* allocate array of s32_t for our object identifier */
      vb->ident = (s32_t*)memp_malloc(MEMP_SNMP_VALUE);
      if (vb->ident == NULL)
      {
        LWIP_DEBUGF(SNMP_MSG_DEBUG, ("snmp_varbind_alloc: couldn't allocate ident value space\n"));
        memp_free(MEMP_SNMP_VARBIND, vb);
        return NULL;
      }
      while(i > 0)
      {
        i--;
        vb->ident[i] = oid->id[i];
      }
    }
    else
    {
      /* i == 0, pass zero length object identifier */
      vb->ident = NULL;
    }
    vb->value_type = type;
    vb->value_len = len;
    if (len > 0)
    {
      LWIP_ASSERT("SNMP_MAX_OCTET_STRING_LEN is configured too low", vb->value_len <= SNMP_MAX_VALUE_SIZE);
      /* allocate raw bytes for our object value */
      vb->value = memp_malloc(MEMP_SNMP_VALUE);
      if (vb->value == NULL)
      {
        LWIP_DEBUGF(SNMP_MSG_DEBUG, ("snmp_varbind_alloc: couldn't allocate value space\n"));
        if (vb->ident != NULL)
        {
          memp_free(MEMP_SNMP_VALUE, vb->ident);
        }
        memp_free(MEMP_SNMP_VARBIND, vb);
        return NULL;
      }
    }
    else
    {
      /* ASN1_NUL type, or zero length ASN1_OC_STR */
      vb->value = NULL;
    }
  }
  else
  {
    LWIP_DEBUGF(SNMP_MSG_DEBUG, ("snmp_varbind_alloc: couldn't allocate varbind space\n"));
  }
  return vb;
}

void
snmp_varbind_free(struct snmp_varbind *vb)
{
  if (vb->value != NULL )
  {
    memp_free(MEMP_SNMP_VALUE, vb->value);
  }
  if (vb->ident != NULL )
  {
    memp_free(MEMP_SNMP_VALUE, vb->ident);
  }
  memp_free(MEMP_SNMP_VARBIND, vb);
}

void
snmp_varbind_list_free(struct snmp_varbind_root *root)
{
  struct snmp_varbind *vb, *prev;

  vb = root->tail;
  while ( vb != NULL )
  {
    prev = vb->prev;
    snmp_varbind_free(vb);
    vb = prev;
  }
  root->count = 0;
  root->head = NULL;
  root->tail = NULL;
}

void
snmp_varbind_tail_add(struct snmp_varbind_root *root, struct snmp_varbind *vb)
{
  if (root->count == 0)
  {
    /* add first varbind to list */
    root->head = vb;
    root->tail = vb;
  }
  else
  {
    /* add nth varbind to list tail */
    root->tail->next = vb;
    vb->prev = root->tail;
    root->tail = vb;
  }
  root->count += 1;
}

struct snmp_varbind*
snmp_varbind_tail_remove(struct snmp_varbind_root *root)
{
  struct snmp_varbind* vb;

  if (root->count > 0)
  {
    /* remove tail varbind */
    vb = root->tail;
    root->tail = vb->prev;
    vb->prev->next = NULL;
    root->count -= 1;
  }
  else
  {
    /* nothing to remove */
    vb = NULL;
  }
  return vb;
}

#endif /* LWIP_SNMP */
