@SET_MAKE@

SUBDIRS = \
	backdrops \
	common \
	doc \
	po \
	settings \
	src \
	menueditor \
	modules \
	panel-plugin \
	pixmaps

confdir = $(sysconfdir)/xfce4

conf_DATA = 								\
	menu.xml 							\
	menu.xml.az 							\
	menu.xml.ca 							\
	menu.xml.de 							\
	menu.xml.fa_IR 							\
	menu.xml.fr 							\
	menu.xml.hu 							\
	menu.xml.ms  							\
	menu.xml.nl  							\
	menu.xml.ta 							\
	menu.xml.tr 							\
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
