/**
 * @file
 * Abstract Syntax Notation One (ISO 8824, 8825) decoding
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
 * Retrieves type field from incoming pbuf chain.
 *
 * @param p points to a pbuf holding an ASN1 coded type field
 * @param ofs points to the offset within the pbuf chain of the ASN1 coded type field
 * @param type return ASN1 type
 * @return ERR_OK if successfull, ERR_ARG if we can't (or won't) decode
 */
err_t
snmp_asn1_dec_type(struct pbuf *p, u16_t ofs, u8_t *type)
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
      *type = *msg_ptr;
      return ERR_OK;
    }
    p = p->next;
  }
  /* p == NULL, ofs >= plen */
  return ERR_ARG;
}

/**
 * Decodes length field from incoming pbuf chain into host length.
 *
 * @param p points to a pbuf holding an ASN1 coded length
 * @param ofs points to the offset within the pbuf chain of the ASN1 coded length
 * @param octets_used returns number of octets used by the length code
 * @param length return host order length, upto 64k
 * @return ERR_OK if successfull, ERR_ARG if we can't (or won't) decode
 */
err_t
snmp_asn1_dec_length(struct pbuf *p, u16_t ofs, u8_t *octets_used, u16_t *length)
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

      if (*msg_ptr < 0x80)
      {
        /* primitive definite length format */
        *octets_used = 1;
        *length = *msg_ptr;
        return ERR_OK;
      }
      else if (*msg_ptr == 0x80)
      {
        /* constructed indefinite length format, termination with two zero octets */
        u8_t zeros;
        u8_t i;

        *length = 0;
        zeros = 0;
        while (zeros != 2)
        {
          i = 2;
          while (i > 0)
          {
            i--;
            (*length) += 1;
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
            if (*msg_ptr == 0)
            {
              zeros++;
              if (zeros == 2)
              {
                /* stop while (i > 0) */
                i = 0;
              }
            }
            else
            {
              zeros = 0;
            }
          }
        }
        *octets_used = 1;
        return ERR_OK;
      }
      else if (*msg_ptr == 0x81)
      {
        /* constructed definite length format, one octet */
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
        *length = *msg_ptr;
        *octets_used = 2;
        return ERR_OK;
      }
      else if (*msg_ptr == 0x82)
      {
        u8_t i;

        /* constructed definite length format, two octets */
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
            *length |= *msg_ptr;
          }
          else
          {
            /* most significant length octet */
            *length = (*msg_ptr) << 8;
          }
        }
        *octets_used = 3;
        return ERR_OK;
      }
      else
      {
        /* constructed definite length format 3..127 octets, this is too big (>64k) */
        /**  @todo: do we need to accept inefficient codings with many leading zero's? */
        *octets_used = 1 + ((*msg_ptr) & 0x7f);
        return ERR_ARG;
      }
    }
    p = p->next;
  }

  /* p == NULL, ofs >= plen */
  return ERR_ARG;
}

/**
 * Decodes positive integer (counter, gauge, timeticks) into u32_t.
 *
 * @param p points to a pbuf holding an ASN1 coded integer
 * @param ofs points to the offset within the pbuf chain of the ASN1 coded integer
 * @param len length of the coded integer field
 * @param value return host order integer
 * @return ERR_OK if successfull, ERR_ARG if we can't (or won't) decode
 *
 * @note ASN coded integers are _always_ signed. E.g. +0xFFFF is coded
 * as 0x00,0xFF,0xFF. Note the leading sign octet. A positive value
 * of 0xFFFFFFFF is preceded with 0x00 and the length is 5 octets!!
 */
err_t
snmp_asn1_dec_u32t(struct pbuf *p, u16_t ofs, u16_t len, u32_t *value)
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
      if ((len > 0) && (len < 6))
      {
        /* start from zero */
        *value = 0;
        if (*msg_ptr & 0x80)
        {
          /* negative, expecting zero sign bit! */
          return ERR_ARG;
        }
        else
        {
          /* positive */
          if ((len > 1) && (*msg_ptr == 0))
          {
            /* skip leading "sign byte" octet 0x00 */
            len--;
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
        }
        /* OR octets with value */
        while (len > 1)
        {
          len--;
          *value |= *msg_ptr;
          *value <<= 8;
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
        *value |= *msg_ptr;
        return ERR_OK;
      }
      else
      {
        return ERR_ARG;
      }
    }
    p = p->next;
  }
  /* p == NULL, ofs >= plen */
  return ERR_ARG;
}

/**
 * Decodes integer into s32_t.
 *
 * @param p points to a pbuf holding an ASN1 coded integer
 * @param ofs points to the offset within the pbuf chain of the ASN1 coded integer
 * @param len length of the coded integer field
 * @param value return host order integer
 * @return ERR_OK if successfull, ERR_ARG if we can't (or won't) decode
 *
 * @note ASN coded integers are _always_ signed!
 */
err_t
snmp_asn1_dec_s32t(struct pbuf *p, u16_t ofs, u16_t len, s32_t *value)
{
  u16_t plen, base;
  u8_t *msg_ptr;
#if BYTE_ORDER == LITTLE_ENDIAN
  u8_t *lsb_ptr = (u8_t*)value;
#endif
#if BYTE_ORDER == BIG_ENDIAN
  u8_t *lsb_ptr = (u8_t*)value + sizeof(s32_t) - 1;
#endif
  u8_t sign;

  plen = 0;
  while (p != NULL)
  {
    base = plen;
    plen += p->len;
    if (ofs < plen)
    {
      msg_ptr = (u8_t*)p->payload;
      msg_ptr += ofs - base;
      if ((len > 0) && (len < 5))
      {
        if (*msg_ptr & 0x80)
        {
          /* negative, start from -1 */
          *value = -1;
          sign = 1;
        }
        else
        {
          /* positive, start from 0 */
          *value = 0;
          sign = 0;
        }
        /* OR/AND octets with value */
        while (len > 1)
        {
          len--;
          if (sign)
          {
            *lsb_ptr &= *msg_ptr;
            *value <<= 8;
            *lsb_ptr |= 255;
          }
          else
          {
            *lsb_ptr |= *msg_ptr;
            *value <<= 8;
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
        }
        if (sign)
        {
          *lsb_ptr &= *msg_ptr;
        }
        else
        {
          *lsb_ptr |= *msg_ptr;
        }
        return ERR_OK;
      }
      else
      {
        return ERR_ARG;
      }
    }
    p = p->next;
  }
  /* p == NULL, ofs >= plen */
  return ERR_ARG;
}

/**
 * Decodes object identifier from incoming message into array of s32_t.
 *
 * @param p points to a pbuf holding an ASN1 coded object identifier
 * @param ofs points to the offset within the pbuf chain of the ASN1 coded object identifier
 * @param len length of the coded object identifier
 * @param oid return object identifier struct
 * @return ERR_OK if successfull, ERR_ARG if we can't (or won't) decode
 */
err_t
snmp_asn1_dec_oid(struct pbuf *p, u16_t ofs, u16_t len, struct snmp_obj_id *oid)
{
  u16_t plen, base;
  u8_t *msg_ptr;
  s32_t *oid_ptr;

  plen = 0;
  while (p != NULL)
  {
    base = plen;
    plen += p->len;
    if (ofs < plen)
    {
      msg_ptr = (u8_t*)p->payload;
      msg_ptr += ofs - base;

      oid->len = 0;
      oid_ptr = &oid->id[0];
      if (len > 0)
      {
        /* first compressed octet */
        if (*msg_ptr == 0x2B)
        {
          /* (most) common case 1.3 (iso.org) */
          *oid_ptr = 1;
          oid_ptr++;
          *oid_ptr = 3;
          oid_ptr++;
        }
        else if (*msg_ptr < 40)
        {
          *oid_ptr = 0;
          oid_ptr++;
          *oid_ptr = *msg_ptr;
          oid_ptr++;
        }
        else if (*msg_ptr < 80)
        {
          *oid_ptr = 1;
          oid_ptr++;
          *oid_ptr = (*msg_ptr) - 40;
          oid_ptr++;
        }
        else
        {
          *oid_ptr = 2;
          oid_ptr++;
          *oid_ptr = (*msg_ptr) - 80;
          oid_ptr++;
        }
        oid->len = 2;
      }
      else
      {
        /* accepting zero length identifiers e.g. for
           getnext operation. uncommon but valid */
        return ERR_OK;
      }
      len--;
      if (len > 0)
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
      while ((len > 0) && (oid->len < LWIP_SNMP_OBJ_ID_LEN))
      {
        /* sub-identifier uses multiple octets */
        if (*msg_ptr & 0x80)
        {
          s32_t sub_id = 0;

          while ((*msg_ptr & 0x80) && (len > 1))
          {
            len--;
            sub_id = (sub_id << 7) + (*msg_ptr & ~0x80);
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
          if (!(*msg_ptr & 0x80) && (len > 0))
          {
            /* last octet sub-identifier */
            len--;
            sub_id = (sub_id << 7) + *msg_ptr;
            *oid_ptr = sub_id;
          }
        }
        else
        {
          /* !(*msg_ptr & 0x80) sub-identifier uses single octet */
          len--;
          *oid_ptr = *msg_ptr;
        }
        if (len > 0)
        {
          /* remaining oid bytes available ... */
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
        oid_ptr++;
        oid->len++;
      }
      if (len == 0)
      {
        /* len == 0, end of oid */
        return ERR_OK;
      }
      else
      {
        /* len > 0, oid->len == LWIP_SNMP_OBJ_ID_LEN or malformed encoding */
        return ERR_ARG;
      }

    }
    p = p->next;
  }
  /* p == NULL, ofs >= plen */
  return ERR_ARG;
}

/**
 * Decodes (copies) raw data (ip-addresses, octet strings, opaque encoding)
 * from incoming message into array.
 *
 * @param p points to a pbuf holding an ASN1 coded raw data
 * @param ofs points to the offset within the pbuf chain of the ASN1 coded raw data
 * @param len length of the coded raw data (zero is valid, e.g. empty string!)
 * @param raw_len length of the raw return value
 * @param raw return raw bytes
 * @return ERR_OK if successfull, ERR_ARG if we can't (or won't) decode
 */
err_t
snmp_asn1_dec_raw(struct pbuf *p, u16_t ofs, u16_t len, u16_t raw_len, u8_t *raw)
{
  u16_t plen, base;
  u8_t *msg_ptr;

  if (len > 0)
  {
    plen = 0;
    while (p != NULL)
    {
      base = plen;
      plen += p->len;
      if (ofs < plen)
      {
        msg_ptr = (u8_t*)p->payload;
        msg_ptr += ofs - base;
        if (raw_len >= len)
        {
          while (len > 1)
          {
            /* copy len - 1 octets */
            len--;
            *raw = *msg_ptr;
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
          /* copy last octet */
          *raw = *msg_ptr;
          return ERR_OK;
        }
        else
        {
          /* raw_len < len, not enough dst space */
          return ERR_ARG;
        }
      }
      p = p->next;
    }
    /* p == NULL, ofs >= plen */
    return ERR_ARG;
  }
  else
  {
    /* len == 0, empty string */
    return ERR_OK;
  }
}

#endif /* LWIP_SNMP */
