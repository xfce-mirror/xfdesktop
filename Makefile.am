@SET_MAKE@

confdir=$(sysconfdir)/xfce4

EXTRA_DIST = xfdesktop.spec xfdesktop.spec.in TODO
conf_DATA = menu.xml

SUBDIRS = src settings backdrops

distclean-local:
	-rm -rf xfdesktop.spec *.cache *~

rpm: dist
	rpmbuild -ta $(PACKAGE)-$(VERSION).tar.gz
	@rm -f $(PACKAGE)-$(VERSION).tar.gz
