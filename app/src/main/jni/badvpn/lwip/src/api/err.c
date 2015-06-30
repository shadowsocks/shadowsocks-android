/**
 * @file
 * Error Management module
 *
 */

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
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
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

#include "lwip/err.h"

#ifdef LWIP_DEBUG

static const char *err_strerr[] = {
           "Ok.",                    /* ERR_OK          0  */
           "Out of memory error.",   /* ERR_MEM        -1  */
           "Buffer error.",          /* ERR_BUF        -2  */
           "Timeout.",               /* ERR_TIMEOUT    -3  */
           "Routing problem.",       /* ERR_RTE        -4  */
           "Operation in progress.", /* ERR_INPROGRESS -5  */
           "Illegal value.",         /* ERR_VAL        -6  */
           "Operation would block.", /* ERR_WOULDBLOCK -7  */
           "Address in use.",        /* ERR_USE        -8  */
           "Already connected.",     /* ERR_ISCONN     -9  */
           "Connection aborted.",    /* ERR_ABRT       -10 */
           "Connection reset.",      /* ERR_RST        -11 */
           "Connection closed.",     /* ERR_CLSD       -12 */
           "Not connected.",         /* ERR_CONN       -13 */
           "Illegal argument.",      /* ERR_ARG        -14 */
           "Low-level netif error.", /* ERR_IF         -15 */
};

/**
 * Convert an lwip internal error to a string representation.
 *
 * @param err an lwip internal err_t
 * @return a string representation for err
 */
const char *
lwip_strerr(err_t err)
{
  return err_strerr[-err];

}

#endif /* LWIP_DEBUG */
