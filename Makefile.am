@SET_MAKE@

if BUILD_PANEL_PLUGIN
panel_plugin_dir = panel-plugin
endif

if BUILD_MENUEDITOR
menueditor_dir = menueditor
endif

SUBDIRS = \
	backdrops \
	common \
	doc \
	$(panel_plugin_dir) \
	po \
	po-doc \
	settings \
	src \
	$(menueditor_dir) \
	modules \
	pixmaps

confdir = $(sysconfdir)/xdg/xfce4/desktop

conf_DATA =		\
	menu.xml	\
	menu.xml.ca	\
	menu.xml.cs	\
	menu.xml.da	\
	menu.xml.de	\
	menu.xml.el	\
	menu.xml.es	\
	menu.xml.et	\
	menu.xml.eu	\
	menu.xml.fi	\
	menu.xml.fr	\
	menu.xml.he	\
	menu.xml.hu	\
	menu.xml.ja	\
	menu.xml.ko	\
	menu.xml.nl  	\
	menu.xml.pa \
	menu.xml.pl	\
	menu.xml.pt_BR	\
	menu.xml.ro	\
	menu.xml.ru	\
	menu.xml.sk	\
	menu.xml.sv	\
	menu.xml.uk	\
	menu.xml.vi 	\
	menu.xml.zh_TW

# the following have not been updated from the old 4.0.x menu
#
#	menu.xml.az 							
#	menu.xml.fa_IR 							
#	menu.xml.ms  							
#	menu.xml.ta 							
#	menu.xml.tr 							
#	menu.xml.zh_CN							


manpagedir = $(mandir)/man1
manpage_DATA = xfdesktop.1

EXTRA_DIST = \
	TODO \
	xfdesktop.spec \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in \
	$(conf_DATA) \
	$(manpage_DATA)

DISTCLEANFILES = \
	intltool-extract \
	intltool-merge \
	intltool-update

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-xsltproc \
	--enable-xml2po

AUTOMAKE_OPTIONS =							\
	1.8								\
	dist-bzip2

distclean-local:
	-rm -rf *.cache *~

rpm: distcheck
	rpmbuild -ta $(PACKAGE)-$(VERSION).tar.gz
	@rm -f $(PACKAGE)-$(VERSION).tar.gz
