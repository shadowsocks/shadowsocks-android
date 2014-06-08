
SUBDIRS = src doc contrib

EXTRA_DIST = version ChangeLog.old COPYING.BSD README.par README.par.old PKGBUILD

# The sample configuration is handled in doc/Makefile.am
install-data-hook:
	$(mkinstalldirs) "$(DESTDIR)$(cachedir)"
	test -f "$(DESTDIR)$(cachedir)/pdnsd.cache" || \
	  touch "$(DESTDIR)$(cachedir)/pdnsd.cache"
	if test `whoami` = "root"; then \
	  chown $(def_id) "$(DESTDIR)$(cachedir)/pdnsd.cache"; \
	  chown $(def_id) "$(DESTDIR)$(cachedir)"; \
	fi
	chmod 0640 "$(DESTDIR)$(cachedir)/pdnsd.cache"
	chmod 0750 "$(DESTDIR)$(cachedir)"

dist-hook: $(PACKAGE).spec.in
	sed -e '/^%{!?distro: %define distro /c\
	%if 0%{!?distro:1}\
	%if "%{_vendor}" == "redhat"\
	%define distro RedHat\
	%else\
	%if "%{_vendor}" == "suse"\
	%define distro SuSE\
	%else\
	%if "%{_vendor}" == "SuSE"\
	%define distro SuSE\
	%endif\
	%endif\
	%endif\
	%endif' \
	-e 's:[@]PACKAGE[@]:$(PACKAGE):g' \
	-e 's:[@]VERSION[@]:$(VERSION):g' \
	-e 's:[@]fullversion[@]:$(fullversion):g' \
	-e 's:[@]packagerelease[@]:$(packagerelease):g' \
	-e 's:[@]cachedir[@]:/var/cache/$(PACKAGE):g' \
	-e 's:[@]def_id[@]:$(PACKAGE):g' \
	$(PACKAGE).spec.in > $(distdir)/$(PACKAGE).spec

