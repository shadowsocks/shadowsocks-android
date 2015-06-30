#ifndef _LIBXTC_H
#define _LIBXTC_H
/* Library which manipulates filtering rules. */

#include <libiptc/ipt_kernel_headers.h>
#include <linux/netfilter/x_tables.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef XT_MIN_ALIGN
/* xt_entry has pointers and u_int64_t's in it, so if you align to
   it, you'll also align to any crazy matches and targets someone
   might write */
#define XT_MIN_ALIGN (__alignof__(struct xt_entry))
#endif

#ifndef XT_ALIGN
#define XT_ALIGN(s) (((s) + ((XT_MIN_ALIGN)-1)) & ~((XT_MIN_ALIGN)-1))
#endif

#define XTC_LABEL_ACCEPT  "ACCEPT"
#define XTC_LABEL_DROP    "DROP"
#define XTC_LABEL_QUEUE   "QUEUE"
#define XTC_LABEL_RETURN  "RETURN"


#ifdef __cplusplus
}
#endif

#endif /* _LIBXTC_H */
