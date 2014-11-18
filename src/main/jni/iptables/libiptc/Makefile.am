# -*- Makefile -*-

AM_CFLAGS        = ${regular_CFLAGS}
AM_CPPFLAGS      = ${regular_CPPFLAGS} -I${top_builddir}/include -I${top_srcdir}/include ${kinclude_CPPFLAGS}

pkgconfig_DATA      = libiptc.pc

lib_LTLIBRARIES     = libip4tc.la libip6tc.la libiptc.la
libiptc_la_SOURCES  =
libiptc_la_LIBADD   = libip4tc.la libip6tc.la
libiptc_la_LDFLAGS  = -version-info 0:0:0 ${libiptc_LDFLAGS2}
libip4tc_la_SOURCES = libip4tc.c
libip4tc_la_LDFLAGS = -version-info 0:0:0
libip6tc_la_SOURCES = libip6tc.c
libip6tc_la_LDFLAGS = -version-info 0:0:0 ${libiptc_LDFLAGS2}
