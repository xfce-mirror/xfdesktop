@SET_MAKE@

DIST_SUBDIRS = \
	backdrops \
	common \
	doc \
	po \
	settings \
	src \
	modules \
	pixmaps \
	panel-plugin \
	menueditor

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

confdir = $(sysconfdir)/xdg/xfce4/desktop

conf_DATA = 								\
	menu.xml 							\
	menu.xml.az 							\
	menu.xml.ca 							\
	menu.xml.de 							\
	menu.xml.es							\
	menu.xml.et							\
	menu.xml.eu							\
	menu.xml.fa_IR 							\
	menu.xml.fr 							\
	menu.xml.he							\
	menu.xml.hu 							\
	menu.xml.ms  							\
	menu.xml.nl  							\
	menu.xml.ro							\
	menu.xml.ru							\
	menu.xml.sk							\
	menu.xml.ta 							\
	menu.xml.tr 							\
	menu.xml.vi 							\
	menu.xml.zh_CN							\
	menu.xml.zh_TW

EXTRA_DIST = $(conf_DATA) xfdesktop.spec TODO xfdesktop.1

manpagedir = $(mandir)/man1
manpage_DATA = xfdesktop.1

distclean-local:
	-rm -rf xfdesktop.spec *.cache *~

rpm: dist
	rpmbuild -ta $(PACKAGE)-$(VERSION).tar.gz
	@rm -f $(PACKAGE)-$(VERSION).tar.gz

html: Makefile
	make -C doc html

ACLOCAL_AMFLAGS = -I m4
