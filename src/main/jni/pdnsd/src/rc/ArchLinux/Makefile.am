
install-exec-local:
	if [ "$(distribution)" = "ArchLinux" ] ; then			\
	  $(mkinstalldirs) "$(DESTDIR)/etc/rc.d";			\
	  $(INSTALL_SCRIPT) $(srcdir)/pdnsd "$(DESTDIR)/etc/rc.d/pdnsd";\
	fi

