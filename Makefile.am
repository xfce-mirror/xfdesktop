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
	scripts \
	settings \
	src \
	$(menueditor_dir) \
	modules \
	pixmaps

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
