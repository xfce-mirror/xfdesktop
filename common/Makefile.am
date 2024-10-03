noinst_LTLIBRARIES = libxfdesktop.la

libxfdesktop_built_sources = \
	xfdesktop-marshal.c \
	xfdesktop-marshal.h \
	tumbler.c \
	tumbler.h

libxfdesktop_la_SOURCES = \
	$(libxfdesktop_built_sources) \
	xfdesktop-common.c \
	xfdesktop-common.h \
	xfdesktop-extensions.c \
	xfdesktop-extensions.h \
	xfdesktop-keyboard-shortcuts.c \
	xfdesktop-keyboard-shortcuts.h \
	xfdesktop-thumbnailer.c \
	xfdesktop-thumbnailer.h

libxfdesktop_la_CFLAGS = \
	-I$(top_srcdir)/src \
	-DBACKGROUNDS_DIR=\"$(datadir)/backgrounds/xfce\" \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4WINDOWING_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GIO_UNIX_CFLAGS) \
	$(XFCONF_CFLAGS)

DISTCLEANFILES = \
	$(libxfdesktop_built_sources)

BUILT_SOURCES = \
	$(libxfdesktop_built_sources)

EXTRA_DIST = \
	meson.build \
	tumbler-service-dbus.xml \
	xfdesktop-marshal.list

xfdesktop-marshal.h: xfdesktop-marshal.list Makefile
	$(AM_V_GEN) $(GLIB_GENMARSHAL) \
		--quiet \
		--prefix=xfdesktop_marshal \
		--internal \
		--header \
		$< > $@ || { rm -f $@; exit 1; }

xfdesktop-marshal.c: xfdesktop-marshal.list Makefile
	$(AM_V_GEN) $(GLIB_GENMARSHAL) \
		--quiet \
		--include-header "xfdesktop-marshal.h" \
		--prefix=xfdesktop_marshal \
		--body \
		$< > $@ || { rm -f $@; exit 1; }

tumbler.c tumbler.h : $(srcdir)/tumbler-service-dbus.xml Makefile.am
	$(AM_V_GEN) $(GDBUS_CODEGEN) \
		--c-namespace=Tumbler \
		--interface-prefix=org.freedesktop.thumbnails. \
		--generate-c-code=tumbler \
		$<
