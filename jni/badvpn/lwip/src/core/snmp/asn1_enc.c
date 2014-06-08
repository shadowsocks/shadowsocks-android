/**
 * @file
 * Abstract Syntax Notation One (ISO 8824, 8825) encoding
 *
 * @todo not optimised (yet), favor correctness over speed, favor speed over size
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

#include "lwip/snmp_asn1.h"

/**
 * Returns octet count for length.
 *
 * @param length
 * @param octets_needed points to the return value
 */
void
snmp_asn1_enc_length_cnt(u16_t length, u8_t *octets_needed)
{
  if (length < 0x80U)
  {
    *octets_needed = 1;
  }
  else if (length < 0x100U)
  {
    *octets_needed = 2;
  }
  else
  {
    *octets_needed = 3;
  }
}

/**
 * Returns octet count for an u32_t.
 *
 * @param value
 * @param octets_needed points to the return value
 *
 * @note ASN coded integers are _always_ signed. E.g. +0xFFFF is coded
 * as 0x00,0xFF,0xFF. Note the leading sign octet. A positive value
 * of 0xFFFFFFFF is preceded with 0x00 and the length is 5 octets!!
 */
void
snmp_asn1_enc_u32t_cnt(u32_t value, u16_t *octets_needed)
{
  if (value < 0x80UL)
  {
    *octets_needed = 1;
  }
  else if (value < 0x8000UL)
  {
    *octets_needed = 2;
  }
  else if (value < 0x800000UL)
  {
    *octets_needed = 3;
  }
  else if (value < 0x80000000UL)
  {
    *octets_needed = 4;
  }
  else
  {
    *octets_needed = 5;
  }
}

/**
 * Returns octet count for an s32_t.
 *
 * @param value
 * @param octets_needed points to the return value
 *
 * @note ASN coded integers are _always_ signed.
 */
void
snmp_asn1_enc_s32t_cnt(s32_t value, u16_t *octets_needed)
{
  if (value < 0)
  {
    value = ~value;
  }
  if (value < 0x80L)
  {
    *octets_needed = 1;
  }
  else if (value < 0x8000L)
  {
    *octets_needed = 2;
  }
  else if (value < 0x800000L)
  {
    *octets_needed = 3;
  }
  else
  {
    *octets_needed = 4;
  }
}

/**
 * Returns octet count for an object identifier.
 *
 * @param ident_len object identifier array length
 * @param ident points to object identifier array
 * @param octets_needed points to the return value
 */
void
snmp_asn1_enc_oid_cnt(u8_t ident_len, s32_t *ident, u16_t *octets_needed)
{
  s32_t sub_id;
  u8_t cnt;

  cnt = 0;
  if (ident_len > 1)
  {
    /* compressed prefix in one octet */
    cnt++;
    ident_len -= 2;
    ident += 2;
  }
  while(ident_len > 0)
  {
    ident_len--;
    sub_id = *ident;

    sub_id >>= 7;
    cnt++;
    while(sub_id > 0)
    {
      sub_id >>= 7;
      cnt++;
    }
    ident++;
  }
  *octets_needed = cnt;
}

/**
 * Encodes ASN type field into a pbuf chained ASN1 msg.
 *
 * @param p points to output pbuf to encode value into
 * @param ofs points to the offset within the pbuf chain
 * @param type input ASN1 type
 * @return ERR_OK if successfull, ERR_ARG if we can't (or won't) encode
 */
err_t
snmp_asn1_enc_type(struct pbuf *p, u16_t ofs, u8_t type)
{
  u16_t plen, base;
  u8_t *msg_ptr;

  plen = 0;
  while (p != NULL)
  {
    base = plen;
    plen += p->len;
    if (ofs < plen)
    {
      msg_ptr = (u8_t*)p->payload;
      msg_ptr += ofs - base;
      *msg_ptr = type;
      return ERR_OK;
    }
    p = p->next;
  }
  /* p == NULL, ofs >= plen */
  return ERR_ARG;
}

/**
 * Encodes host order length field into a pbuf chained ASN1 msg.
 *
 * @param p points to output pbuf to encode length into
 * @param ofs points to the offset within the pbuf chain
 * @param length is the host order length to be encoded
 * @return ERR_OK if successfull, ERR_ARG if we can't (or won't) encode
 */
err_t
snmp_asn1_enc_length(struct pbuf *p, u16_t ofs, u16_t length)
{
  u16_t plen, base;
  u8_t *msg_ptr;

  plen = 0;
  while (p != NULL)
  {
    base = plen;
    plen += p->len;
    if (ofs < plen)
    {
      msg_ptr = (u8_t*)p->payload;
      msg_ptr += ofs - base;

      if (length < 0x80)
      {
        *msg_ptr = (u8_t)length;
        return ERR_OK;
      }
      else if (length < 0x100)
      {
        *msg_ptr = 0x81;
        ofs += 1;
        if (ofs >= plen)
        {
          /* next octet in next pbuf */
          p = p->next;
          if (p == NULL) { return ERR_ARG; }
          msg_ptr = (u8_t*)p->payload;
        }
        else
        {
          /* next octet in same pbuf */
          msg_ptr++;
        }
        *msg_ptr = (u8_t)length;
        return ERR_OK;
      }
      else
      {
        u8_t i;

        /* length >= 0x100 && length <= 0xFFFF */
        *msg_ptr = 0x82;
        i = 2;
        while (i > 0)
        {
          i--;
          ofs += 1;
          if (ofs >= plen)
          {
            /* next octet in next pbuf */
            p = p->next;
            if (p == NULL) { return ERR_ARG; }
            msg_ptr = (u8_t*)p->payload;
            plen += p->len;
          }
          else
          {
            /* next octet in same pbuf */
            msg_ptr++;
          }
          if (i == 0)
          {
            /* least significant length octet */
            *msg_ptr = (u8_t)length;
          }
          else
          {
            /* most significant length octet */
            *msg_ptr = (u8_t)(length >> 8);
          }
        }
        return ERR_OK;
      }
    }
    p = p->next;
  }
  /* p == NULL, ofs >= plen */
  return ERR_ARG;
}

/**
 * Encodes u32_t (counter, gauge, timeticks) into a pbuf chained ASN1 msg.
 *
 * @param p points to output pbuf to encode value into
 * @param ofs points to the offset within the pbuf chain
 * @param octets_needed encoding length (from snmp_asn1_enc_u32t_cnt())
 * @param value is the host order u32_t value to be encoded
 * @return ERR_OK if successfull, ERR_ARG if we can't (or won't) encode
 *
 * @see snmp_asn1_enc_u32t_cnt()
 */
err_t
snmp_asn1_enc_u32t(struct pbuf *p, u16_t ofs, u16_t octets_needed, u32_t value)
{
  u16_t plen, base;
  u8_t *msg_ptr;

  plen = 0;
  while (p != NULL)
  {
    base = plen;
    plen += p->len;
    if (ofs < plen)
    {
      msg_ptr = (u8_t*)p->payload;
      msg_ptr += ofs - base;

      if (octets_needed == 5)
      {
        /* not enough bits in 'value' add leading 0x00 */
        octets_needed--;
        *msg_ptr = 0x00;
        ofs += 1;
        if (ofs >= plen)
        {
          /* next octet in next pbuf */
          p = p->next;
          if (p == NULL) { return ERR_ARG; }
          msg_ptr = (u8_t*)p->payload;
          plen += p->len;
        }
        else
        {
          /* next octet in same pbuf */
          msg_ptr++;
        }
      }
      while (octets_needed > 1)
      {
        octets_needed--;
        *msg_ptr = (u8_t)(value >> (octets_needed << 3));
        ofs += 1;
        if (ofs >= plen)
        {
          /* next octet in next pbuf */
          p = p->next;
          if (p == NULL) { return ERR_ARG; }
          msg_ptr = (u8_t*)p->payload;
          plen += p->len;
        }
        else
        {
          /* next octet in same pbuf */
          msg_ptr++;
        }
      }
      /* (only) one least significant octet */
      *msg_ptr = (u8_t)value;
      return ERR_OK;
    }
    p = p->next;
  }
  /* p == NULL, ofs >= plen */
  return ERR_ARG;
}

/**
 * Encodes s32_t integer into a pbuf chained ASN1 msg.
 *
 * @param p points to output pbuf to encode value into
 * @param ofs points to the offset within the pbuf chain
 * @param octets_needed encoding length (from snmp_asn1_enc_s32t_cnt())
 * @param value is the host order s32_t value to be encoded
 * @return ERR_OK if successfull, ERR_ARG if we can't (or won't) encode
 *
 * @see snmp_asn1_enc_s32t_cnt()
 */
err_t
snmp_asn1_enc_s32t(struct pbuf *p, u16_t ofs, u16_t octets_needed, s32_t value)
{
  u16_t plen, base;
  u8_t *msg_ptr;

  plen = 0;
  while (p != NULL)
  {
    base = plen;
    plen += p->len;
    if (ofs < plen)
    {
      msg_ptr = (u8_t*)p->payload;
      msg_ptr += ofs - base;

      while (octets_needed > 1)
      {
        octets_needed--;
        *msg_ptr = (u8_t)(value >> (octets_needed << 3));
        ofs += 1;
        if (ofs >= plen)
        {
          /* next octet in next pbuf */
          p = p->next;
          if (p == NULL) { return ERR_ARG; }
          msg_ptr = (u8_t*)p->payload;
          plen += p->len;
        }
        else
        {
          /* next octet in same pbuf */
          msg_ptr++;
        }
      }
      /* (only) one least significant octet */
      *msg_ptr = (u8_t)value;
      return ERR_OK;
    }
    p = p->next;
  }
  /* p == NULL, ofs >= plen */
  return ERR_ARG;
}

/**
 * Encodes object identifier into a pbuf chained ASN1 msg.
 *
 * @param p points to output pbuf to encode oid into
 * @param ofs points to the offset within the pbuf chain
 * @param ident_len object identifier array length
 * @param ident points to object identifier array
 * @return ERR_OK if successfull, ERR_ARG if we can't (or won't) encode
 */
err_t
snmp_asn1_enc_oid(struct pbuf *p, u16_t ofs, u8_t ident_len, s32_t *ident)
{
  u16_t plen, base;
  u8_t *msg_ptr;

  plen = 0;
  while (p != NULL)
  {
    base = plen;
    plen += p->len;
    if (ofs < plen)
    {
      msg_ptr = (u8_t*)p->payload;
      msg_ptr += ofs - base;

      if (ident_len > 1)
      {
        if ((ident[0] == 1) && (ident[1] == 3))
        {
          /* compressed (most common) prefix .iso.org */
          *msg_ptr = 0x2b;
        }
        else
        {
          /* calculate prefix */
          *msg_ptr = (u8_t)((ident[0] * 40) + ident[1]);
        }
        ofs += 1;
        if (ofs >= plen)
        {
          /* next octet in next pbuf */
          p = p->next;
          if (p == NULL) { return ERR_ARG; }
          msg_ptr = (u8_t*)p->payload;
          plen += p->len;
        }
        else
        {
          /* next octet in same pbuf */
          msg_ptr++;
        }
        ident_len -= 2;
        ident += 2;
      }
      else
      {
/* @bug:  allow empty varbinds for symmetry (we must decode them for getnext), allow partial compression??  */
        /* ident_len <= 1, at least we need zeroDotZero (0.0) (ident_len == 2) */
        return ERR_ARG;
      }
      while (ident_len > 0)
      {
        s32_t sub_id;
        u8_t shift, tail;

        ident_len--;
        sub_id = *ident;
        tail = 0;
        shift = 28;
        while(shift > 0)
        {
          u8_t code;

          code = (u8_t)(sub_id >> shift);
          if ((code != 0) || (tail != 0))
          {
            tail = 1;
            *msg_ptr = code | 0x80;
            ofs += 1;
            if (ofs >= plen)
            {
              /* next octet in next pbuf */
              p = p->next;
              if (p == NULL) { return ERR_ARG; }
              msg_ptr = (u8_t*)p->payload;
              plen += p->len;
            }
            else
            {
              /* next octet in same pbuf */
              msg_ptr++;
            }
          }
          shift -= 7;
        }
        *msg_ptr = (u8_t)sub_id & 0x7F;
        if (ident_len > 0)
        {
          ofs += 1;
          if (ofs >= plen)
          {
            /* next octet in next pbuf */
            p = p->next;
            if (p == NULL) { return ERR_ARG; }
            msg_ptr = (u8_t*)p->payload;
            plen += p->len;
          }
          else
          {
            /* next octet in same pbuf */
            msg_ptr++;
          }
        }
        /* proceed to next sub-identifier */
        ident++;
      }
      return ERR_OK;
    }
    p = p->next;
  }
  /* p == NULL, ofs >= plen */
  return ERR_ARG;
}

/**
 * Encodes raw data (octet string, opaque) into a pbuf chained ASN1 msg.
 *
 * @param p points to output pbuf to encode raw data into
 * @param ofs points to the offset within the pbuf chain
 * @param raw_len raw data length
 * @param raw points raw data
 * @return ERR_OK if successfull, ERR_ARG if we can't (or won't) encode
 */
err_t
snmp_asn1_enc_raw(struct pbuf *p, u16_t ofs, u16_t raw_len, u8_t *raw)
{
  u16_t plen, base;
  u8_t *msg_ptr;

  plen = 0;
  while (p != NULL)
  {
    base = plen;
    plen += p->len;
    if (ofs < plen)
    {
      msg_ptr = (u8_t*)p->payload;
      msg_ptr += ofs - base;

      while (raw_len > 1)
      {
        /* copy raw_len - 1 octets */
        raw_len--;
        *msg_ptr = *raw;
        raw++;
        ofs += 1;
        if (ofs >= plen)
        {
          /* next octet in next pbuf */
          p = p->next;
          if (p == NULL) { return ERR_ARG; }
          msg_ptr = (u8_t*)p->payload;
          plen += p->len;
        }
        else
        {
          /* next octet in same pbuf */
          msg_ptr++;
        }
      }
      if (raw_len > 0)
      {
        /* copy last or single octet */
        *msg_ptr = *raw;
      }
      return ERR_OK;
    }
    p = p->next;
  }
  /* p == NULL, ofs >= plen */
  return ERR_ARG;
}

#endif /* LWIP_SNMP */
