@SET_MAKE@

SUBDIRS = \
	backdrops \
	common \
	doc \
	po \
	settings \
	src \
	modules \
	pixmaps

if BUILD_PANEL_PLUGIN
SUBDIRS += panel-plugin
endif

if BUILD_MENUEDITOR
SUBDIRS += menueditor
endif

confdir = $(sysconfdir)/xfce4

conf_DATA = 								\
	menu.xml 							\
	menu.xml.az 							\
	menu.xml.ca 							\
	menu.xml.de 							\
	menu.xml.es								\
	menu.xml.eu								\
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
