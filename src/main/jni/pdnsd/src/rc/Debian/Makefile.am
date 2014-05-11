
install-exec-local:
	if [ "$(distribution)" = "Debian" ] ; then \
	  CURDIR=`pwd`; \
	  $(mkinstalldirs) "$(DESTDIR)/etc/init.d"; \
	  $(INSTALL_SCRIPT) $(srcdir)/pdnsd "$(DESTDIR)/etc/init.d"; \
	  update-rc.d pdnsd defaults 19 ;\
	fi
