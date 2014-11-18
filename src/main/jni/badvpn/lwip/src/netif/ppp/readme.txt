About the PPP code:

The PPP code is not our "own" code - we just copied it from pppd (http://ppp.samba.org/) and adapted it to lwIP.
Unfortunately, not many here know their way around it too well. Back in 2009, we took the effort to see which
version of pppd our code relates to and we're pretty much on 2.3.11 with some bugs from 2.4.x backported.

Aside from simple code adaptions, there are some files that are different, however:
- chpms.c/.h are named chap_ms.c/.h in the original pppd 2.3.11 sources
- pap.c/.h are named upap.c/.h in the original pppd 2.3.11 sources
- randm.c is a random generator not included in the original pppd
- magic.c does not use the C library's random functions, but uses randm.c instead
- vj.c/.h is an implementation of the Van Jacobson header compression algorithm adapted to lwIP pbufs,
  probably copied from one of the vjcompress.c files from pppd.
- ppp.c, ppp.h and ppp_impl.h contain the adaption from pppd to lwIP. This is the "OS"-dependent part like there
  is an implementation for linux, xBSD etc. in the pppd sources.
- ppp_oe.c is Marc Boucher's implementation based on NetBSD's if_pppoe.c

There is of course potential for bugs in it, but when analyzing of reporting bugs, it is strongly encouraged to
compare the code in question to pppd 2.3.11 (our basis) and newer versions (perhaps it's already fixed?) and to
share this knowledge with us when reporting a bug.

