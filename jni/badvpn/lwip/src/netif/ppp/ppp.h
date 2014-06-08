/*****************************************************************************
* ppp.h - Network Point to Point Protocol header file.
*
* Copyright (c) 2003 by Marc Boucher, Services Informatiques (MBSI) inc.
* portions Copyright (c) 1997 Global Election Systems Inc.
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
*   Original derived from BSD codes.
*****************************************************************************/

#ifndef PPP_H
#define PPP_H

#include "lwip/opt.h"

#if PPP_SUPPORT /* don't build if not configured for use in lwipopts.h */

#include "lwip/def.h"
#include "lwip/sio.h"
#include "lwip/stats.h"
#include "lwip/mem.h"
#include "lwip/netif.h"
#include "lwip/sys.h"
#include "lwip/timers.h"


#ifndef __u_char_defined

/* Type definitions for BSD code. */
typedef unsigned long  u_long;
typedef unsigned int   u_int;
typedef unsigned short u_short;
typedef unsigned char  u_char;

#endif


/*************************
*** PUBLIC DEFINITIONS ***
*************************/

/* Error codes. */
#define PPPERR_NONE      0 /* No error. */
#define PPPERR_PARAM    -1 /* Invalid parameter. */
#define PPPERR_OPEN     -2 /* Unable to open PPP session. */
#define PPPERR_DEVICE   -3 /* Invalid I/O device for PPP. */
#define PPPERR_ALLOC    -4 /* Unable to allocate resources. */
#define PPPERR_USER     -5 /* User interrupt. */
#define PPPERR_CONNECT  -6 /* Connection lost. */
#define PPPERR_AUTHFAIL -7 /* Failed authentication challenge. */
#define PPPERR_PROTOCOL -8 /* Failed to meet protocol. */

/*
 * PPP IOCTL commands.
 */
/*
 * Get the up status - 0 for down, non-zero for up.  The argument must
 * point to an int.
 */
#define PPPCTLG_UPSTATUS 100 /* Get the up status - 0 down else up */
#define PPPCTLS_ERRCODE  101 /* Set the error code */
#define PPPCTLG_ERRCODE  102 /* Get the error code */
#define PPPCTLG_FD       103 /* Get the fd associated with the ppp */

/************************
*** PUBLIC DATA TYPES ***
************************/

struct ppp_addrs {
  ip_addr_t our_ipaddr, his_ipaddr, netmask, dns1, dns2;
};


/***********************
*** PUBLIC FUNCTIONS ***
***********************/

/* Initialize the PPP subsystem. */
void pppInit(void);

/* Warning: Using PPPAUTHTYPE_ANY might have security consequences.
 * RFC 1994 says:
 *
 * In practice, within or associated with each PPP server, there is a
 * database which associates "user" names with authentication
 * information ("secrets").  It is not anticipated that a particular
 * named user would be authenticated by multiple methods.  This would
 * make the user vulnerable to attacks which negotiate the least secure
 * method from among a set (such as PAP rather than CHAP).  If the same
 * secret was used, PAP would reveal the secret to be used later with
 * CHAP.
 *
 * Instead, for each user name there should be an indication of exactly
 * one method used to authenticate that user name.  If a user needs to
 * make use of different authentication methods under different
 * circumstances, then distinct user names SHOULD be employed, each of
 * which identifies exactly one authentication method.
 *
 */
enum pppAuthType {
    PPPAUTHTYPE_NONE,
    PPPAUTHTYPE_ANY,
    PPPAUTHTYPE_PAP,
    PPPAUTHTYPE_CHAP
};

void pppSetAuth(enum pppAuthType authType, const char *user, const char *passwd);

/* Link status callback function prototype */
typedef void (*pppLinkStatusCB_fn)(void *ctx, int errCode, void *arg);

#if PPPOS_SUPPORT
/*
 * Open a new PPP connection using the given serial I/O device.
 * This initializes the PPP control block but does not
 * attempt to negotiate the LCP session.
 * Return a new PPP connection descriptor on success or
 * an error code (negative) on failure. 
 */
int pppOverSerialOpen(sio_fd_t fd, pppLinkStatusCB_fn linkStatusCB, void *linkStatusCtx);
#endif /* PPPOS_SUPPORT */

#if PPPOE_SUPPORT
/*
 * Open a new PPP Over Ethernet (PPPOE) connection.
 */
int pppOverEthernetOpen(struct netif *ethif, const char *service_name, const char *concentrator_name,
                        pppLinkStatusCB_fn linkStatusCB, void *linkStatusCtx);
#endif /* PPPOE_SUPPORT */

/* for source code compatibility */
#define pppOpen(fd,cb,ls) pppOverSerialOpen(fd,cb,ls)

/*
 * Close a PPP connection and release the descriptor. 
 * Any outstanding packets in the queues are dropped.
 * Return 0 on success, an error code on failure. 
 */
int pppClose(int pd);

/*
 * Indicate to the PPP process that the line has disconnected.
 */
void pppSigHUP(int pd);

/*
 * Get and set parameters for the given connection.
 * Return 0 on success, an error code on failure. 
 */
int  pppIOCtl(int pd, int cmd, void *arg);

/*
 * Return the Maximum Transmission Unit for the given PPP connection.
 */
u_short pppMTU(int pd);

#if PPPOS_SUPPORT && !PPP_INPROC_OWNTHREAD
/*
 * PPP over Serial: this is the input function to be called for received data.
 * If PPP_INPROC_OWNTHREAD==1, a seperate input thread using the blocking
 * sio_read() is used, so this is deactivated.
 */
void pppos_input(int pd, u_char* data, int len);
#endif /* PPPOS_SUPPORT && !PPP_INPROC_OWNTHREAD */


#if LWIP_NETIF_STATUS_CALLBACK
/* Set an lwIP-style status-callback for the selected PPP device */
void ppp_set_netif_statuscallback(int pd, netif_status_callback_fn status_callback);
#endif /* LWIP_NETIF_STATUS_CALLBACK */
#if LWIP_NETIF_LINK_CALLBACK
/* Set an lwIP-style link-callback for the selected PPP device */
void ppp_set_netif_linkcallback(int pd, netif_status_callback_fn link_callback);
#endif /* LWIP_NETIF_LINK_CALLBACK */

#endif /* PPP_SUPPORT */

#endif /* PPP_H */
