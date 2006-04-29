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
	menu.xml.ca							\
	menu.xml.cs							\
	menu.xml.da							\
	menu.xml.de 							\
	menu.xml.el							\
	menu.xml.es							\
	menu.xml.et							\
	menu.xml.eu							\
	menu.xml.fi							\
	menu.xml.fr 							\
	menu.xml.he							\
	menu.xml.hu							\
	menu.xml.ja							\
	menu.xml.ko							\
	menu.xml.nl  							\
	menu.xml.pl							\
	menu.xml.pt_BR							\
	menu.xml.ro							\
	menu.xml.ru							\
	menu.xml.sk							\
	menu.xml.sv							\
	menu.xml.uk							\
	menu.xml.vi 							\
	menu.xml.zh_TW


# the following have not been updated from the old 4.0.x menu
#
#	menu.xml.az 							
#	menu.xml.fa_IR 							
#	menu.xml.ms  							
#	menu.xml.ta 							
#	menu.xml.tr 							
#	menu.xml.zh_CN							


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

dist-bz2: dist
	zcat $(PACKAGE)-$(VERSION).tar.gz | bzip2 --best -c > $(PACKAGE)-$(VERSION).tar.bz2

distcheck-bz2: distcheck
	zcat $(PACKAGE)-$(VERSION).tar.gz | bzip2 --best -c > $(PACKAGE)-$(VERSION).tar.bz2
