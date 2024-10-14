@SET_MAKE@

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS = \
	backgrounds \
	common \
	doc \
	settings \
	src \
	po \
	pixmaps \
	tests

manpagedir = $(mandir)/man1
manpage_DATA = xfdesktop.1

EXTRA_DIST = \
	README.md \
	meson_options.txt \
	meson.build \
	po/meson.build \
	xfce-revision.h.in \
	$(conf_DATA) \
	$(manpage_DATA)

DISTCLEANFILES =

distuninstallcheck_listfiles = \
	find . -type f -print | grep -v ./share/icons/hicolor/icon-theme.cache

distclean-local:
	-rm -rf *.cache *~

.PHONY: ChangeLog

ChangeLog: Makefile
	(GIT_DIR=$(top_srcdir)/.git git log > .changelog.tmp \
	&& mv .changelog.tmp ChangeLog; rm -f .changelog.tmp) \
	|| (touch ChangeLog; echo 'Git directory not found: installing possibly empty changelog.' >&2)

dist-hook: ChangeLog

if MAINTAINER_MODE

DISTCLEANFILES += \
	compile_flags.txt

all-local: compile_flags.txt

compile_flags.txt: $(shell find . -name Makefile)
	rm -f $@
	for flag in \
		-DPACKAGE=\"xfdesktop\" \
		-DGETTEXT_PACKAGE=\"xfdesktop\" \
		-DVERSION=\"4.0.0\" \
		-DVERSION_SHORT=\"4.0\" \
		-DVERSION_FULL=VERSION \
		-DENABLE_DESKTOP_ICONS=1 \
		-DENABLE_X11=1 \
		-DENABLE_WAYLAND=1 \
		-DENABLE_FILE_ICONS=1 \
		-DLOCALEDIR=\"$(localedir)\" \
		-DEXO_API_SUBJECT_TO_CHANGE \
		-DDEFAULT_BACKDROP=\"\" \
		-DBACKGROUNDS_DIR=\"\" \
		-I$(top_srcdir) \
		-I$(top_srcdir)/common	\
		-I$(top_builddir)/common \
		$(GIO_CFLAGS) \
		$(GIO_UNIX_CFLAGS) \
		$(GLIB_CFLAGS) \
		$(GTHREAD_CFLAGS) \
		$(GTK_CFLAGS) \
		$(LIBNOTIFY_CFLAGS) \
		$(LIBX11_CFLAGS) \
		$(LIBXFCE4UTIL_CFLAGS) \
		$(LIBXFCE4UI_CFLAGS) \
		$(LIBXFCE4KBD_PRIVATE_CFLAGS) \
		$(LIBXFCE4SMCLIENT_PRIVATE_CFLAGS) \
		$(LIBXFCE4WINDOWING_CFLAGS) \
		$(LIBXFCE4WINDOWINGUI_CFLAGS) \
		$(GTK_LAYER_SHELL_CFLAGS) \
		$(XFCONF_CFLAGS) \
		$(LIBEXO_CFLAGS) \
		$(CAIRO_CFLAGS) \
		$(THUNARX_CFLAGS) \
		$(GARCON_CFLAGS) \
		$(GARCON_GTK3_CFLAGS) \
		$(LIBNOTIFY_CFLAGS) \
	; do \
		echo "$$flag" >> $@ || exit 1; \
	done

endif
