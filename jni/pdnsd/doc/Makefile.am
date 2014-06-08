
man_MANS = pdnsd.8 pdnsd-ctl.8 pdnsd.conf.5

# Note: pdnsd-ctl.8, pdnsd.conf.5.in, dl.html and the txt docs are handled by dist-hook rule.
EXTRA_DIST = pdnsd.conf.in pdnsd.8.in \
	html/dl.html.in html/doc.html html/faq.html html/index.html \
	doc_makefile html/doc_makefile txt/doc_makefile \
	html2confman.pl html/htmlsubst.pl 

# XXX: Do not insist to set the config file owner to root to avoid breaking RPM
# builds
install-data-hook:
	$(mkinstalldirs) "$(DESTDIR)$(sysconfdir)"
	if test `whoami` = "root"; then \
	  $(INSTALL) -o 0 -g 0 -m 644 pdnsd.conf "$(DESTDIR)$(sysconfdir)/pdnsd.conf.sample" ; \
	else \
	  $(INSTALL) -m 644 pdnsd.conf "$(DESTDIR)$(sysconfdir)/pdnsd.conf.sample" ; \
	fi

dist-hook: doc_makefile
	@$(MAKE) -f doc_makefile doc
	cp -p --parents pdnsd-ctl.8 pdnsd.conf.5.in \
	                html/dl.html txt/faq.txt txt/intro.txt txt/manual.txt \
	                $(distdir)

distclean-local: doc_makefile
	@$(MAKE) -f doc_makefile clean

