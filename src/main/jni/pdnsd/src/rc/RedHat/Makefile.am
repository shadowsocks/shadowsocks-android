
# no need to create links. 'chkconfig' will take care of this.
# In the spec case, chkconfig is called during rpm install
install-exec-local:
	if [ "$(distribution)" = "RedHat" ] ; then \
	  $(mkinstalldirs) "$(DESTDIR)/etc/rc.d/init.d"; \
	  $(INSTALL_SCRIPT) $(srcdir)/pdnsd "$(DESTDIR)/etc/rc.d/init.d/pdnsd"; \
	    if [ "$(specbuild)" = "no" ] ; then \
	      /sbin/chkconfig --add pdnsd; \
	    fi \
	fi

