
sbin_PROGRAMS = pdnsd-ctl

pdnsd_ctl_SOURCES = pdnsd-ctl.c
pdnsd_ctl_LDADD =  rr_types.o
pdnsd_ctl_DEPENDENCIES = rr_types.o

# These are Symlinks we want to have in the package
#EXTRA_DIST = rr_types.h

pdnsd-ctl.o rr_types.o: ../rr_types.h

../rr_types.h: ../make_rr_types_h.pl ../rr_types.in
	perl ../make_rr_types_h.pl ../rr_types.in > ../rr_types.h

rr_types.o: %.o: ../%.c
	$(COMPILE) -DCLIENT_ONLY -c $<

