@SET_MAKE@

EXTRA_DIST = xfdesktop.spec xfdesktop.spec.in TODO

SUBDIRS = src settings

distclean-local:
	-rm -rf xfdesktop.spec *.cache *~

rpm: dist
	rpmbuild -ta $(PACKAGE)-$(VERSION).tar.gz
	@rm -f $(PACKAGE)-$(VERSION).tar.gz
