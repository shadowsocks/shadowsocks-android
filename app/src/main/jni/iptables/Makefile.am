# -*- Makefile -*-

ACLOCAL_AMFLAGS  = -I m4
AUTOMAKE_OPTIONS = foreign subdir-objects

SUBDIRS          = extensions libiptc iptables
if ENABLE_DEVEL
SUBDIRS         += include
endif
if ENABLE_LIBIPQ
SUBDIRS         += libipq
endif
if HAVE_LIBNFNETLINK
SUBDIRS         += utils
endif

.PHONY: tarball
tarball:
	rm -Rf /tmp/${PACKAGE_TARNAME}-${PACKAGE_VERSION};
	pushd ${top_srcdir} && git archive --prefix=${PACKAGE_TARNAME}-${PACKAGE_VERSION}/ HEAD | tar -C /tmp -x && popd;
	pushd /tmp/${PACKAGE_TARNAME}-${PACKAGE_VERSION} && ./autogen.sh && popd;
	tar -C /tmp -cjf ${PACKAGE_TARNAME}-${PACKAGE_VERSION}.tar.bz2 --owner=root --group=root ${PACKAGE_TARNAME}-${PACKAGE_VERSION}/;
	rm -Rf /tmp/${PACKAGE_TARNAME}-${PACKAGE_VERSION};

config.status: extensions/GNUmakefile.in \
	include/xtables.h.in include/iptables/internal.h.in
