# -*- Makefile -*-

AM_CFLAGS = ${regular_CFLAGS}
AM_CPPFLAGS = ${regular_CPPFLAGS} -I${top_builddir}/include -I${top_srcdir}/include

libipq_la_SOURCES = libipq.c
lib_LTLIBRARIES   = libipq.la
man_MANS         = ipq_create_handle.3 ipq_destroy_handle.3 ipq_errstr.3 \
                   ipq_get_msgerr.3 ipq_get_packet.3 ipq_message_type.3 \
                   ipq_perror.3 ipq_read.3 ipq_set_mode.3 ipq_set_verdict.3 \
                   libipq.3
