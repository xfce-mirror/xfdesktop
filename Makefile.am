@SET_MAKE@

SUBDIRS = backdrops common doc po settings src

confdir = $(sysconfdir)/xfce4

conf_DATA = 								\
	menu.xml 							\
	menu.xml.az 							\
	menu.xml.de 							\
	menu.xml.fr 							\
	menu.xml.nl  							\
	menu.xml.ca 							\
	menu.xml.hu 							\
	menu.xml.ta 							\
	menu.xml.vi 							\
	menu.xml.zh_CN							\
	menu.xml.zh_TW

EXTRA_DIST = $(conf_DATA) xfdesktop.spec TODO

distclean-local:
	-rm -rf xfdesktop.spec *.cache *~

rpm: dist
	rpmbuild -ta $(PACKAGE)-$(VERSION).tar.gz
	@rm -f $(PACKAGE)-$(VERSION).tar.gz

html: Makefile
	make -C doc html

ACLOCAL_AMFLAGS = -I m4
