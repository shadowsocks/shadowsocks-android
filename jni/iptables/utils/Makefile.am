# -*- Makefile -*-

AM_CFLAGS = ${regular_CFLAGS}
AM_CPPFLAGS = ${regular_CPPFLAGS} -I${top_builddir}/include -I${top_srcdir}/include

sbin_PROGRAMS = nfnl_osf
pkgdata_DATA = pf.os

nfnl_osf_LDADD = -lnfnetlink
