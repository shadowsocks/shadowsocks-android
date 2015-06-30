#ifndef UUID_67C91670_FCCB_4855_BDF7_609F1EECB8B4
#define UUID_67C91670_FCCB_4855_BDF7_609F1EECB8B4

/* all these definitions, are included into bits/in.h from libc6-dev 2.15-0ubuntu10
 * from Ubuntu 12.04 and is not included into libc6-dev 2.11.1-0ubuntu7.10 from
 * Ubuntu 10.04.
 * linux/in.h is not included directly because of lots of redefinitions,
 * extracting single value from linux/in.h is not done because it looks like
 * autotools reinvention */
#ifndef IP_ORIGDSTADDR
#   warning Using hardcoded value for IP_ORIGDSTADDR as libc headers do not define it.
#   define IP_ORIGDSTADDR 20
#endif

#ifndef IP_RECVORIGDSTADDR
#   warning Using hardcoded value for IP_RECVORIGDSTADDR as libc headers do not define it.
#   define IP_RECVORIGDSTADDR IP_ORIGDSTADDR
#endif

#ifndef IP_TRANSPARENT
#   warning Using hardcoded value for IP_TRANSPARENT as libc headers do not define it.
#   define IP_TRANSPARENT 19
#endif

#endif // 67C91670_FCCB_4855_BDF7_609F1EECB8B4
