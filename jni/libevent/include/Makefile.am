# include/Makefile.am for libevent
# Copyright 2000-2007 Niels Provos
# Copyright 2007-2012 Niels Provos and Nick Mathewson
#
# See LICENSE for copying information.

AUTOMAKE_OPTIONS = foreign

EVENT2_EXPORT = \
	event2/buffer.h \
	event2/buffer_compat.h \
	event2/bufferevent.h \
	event2/bufferevent_compat.h \
	event2/bufferevent_ssl.h \
	event2/bufferevent_struct.h \
	event2/dns.h \
	event2/dns_compat.h \
	event2/dns_struct.h \
	event2/event.h \
	event2/event_compat.h \
	event2/event_struct.h \
	event2/http.h \
	event2/http_compat.h \
	event2/http_struct.h \
	event2/keyvalq_struct.h \
	event2/listener.h \
	event2/rpc.h \
	event2/rpc_compat.h \
	event2/rpc_struct.h \
	event2/tag.h \
	event2/tag_compat.h \
	event2/thread.h \
	event2/util.h

EXTRA_SRC = $(EVENT2_EXPORT)

## Without the nobase_ prefixing, Automake would strip "event2/" from
## the source header filename to derive the installed header filename.
## With nobase_ the installed path is $(includedir)/event2/ev*.h.

if INSTALL_LIBEVENT
nobase_include_HEADERS =	$(EVENT2_EXPORT)
nobase_nodist_include_HEADERS = ./event2/event-config.h
else
noinst_HEADERS =		$(EVENT2_EXPORT)
nodist_noinst_HEADERS =		./event2/event-config.h
endif
