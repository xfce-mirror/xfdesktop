@SET_MAKE@

SUBDIRS = backdrops common doc po settings src

EXTRA_DIST = xfdesktop.spec.in TODO

confdir = $(sysconfdir)/xfce4

conf_DATA = menu.xml

distclean-local:
	-rm -rf xfdesktop.spec *.cache *~

rpm: dist
	rpmbuild -ta $(PACKAGE)-$(VERSION).tar.gz
	@rm -f $(PACKAGE)-$(VERSION).tar.gz

ACLOCAL_AMFLAGS = -I m4
