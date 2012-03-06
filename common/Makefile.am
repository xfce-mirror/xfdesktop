noinst_LTLIBRARIES = libxfdesktop.la

libxfdesktop_built_sources = \
	xfdesktop-marshal.c \
	xfdesktop-marshal.h

libxfdesktop_la_SOURCES = \
	$(libxfdesktop_built_sources) \
	xfdesktop-common.c \
	xfdesktop-common.h

libxfdesktop_la_CFLAGS = \
	-I$(top_srcdir)/src \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(GTK_CFLAGS)

if ENABLE_DESKTOP_ICONS
if ENABLE_FILE_ICONS

libxfdesktop_la_SOURCES += \
	xfdesktop-thumbnailer.c \
	xfdesktop-thumbnailer.h

libxfdesktop_la_CFLAGS += \
	-DDBUS_API_SUBJECT_TO_CHANGE \
	$(DBUS_CFLAGS)

endif
endif

if MAINTAINER_MODE

xfdesktop-marshal.h: xfdesktop-marshal.list Makefile
	$(AM_V_GEN) glib-genmarshal --prefix=xfdesktop_marshal --internal --header $< > $@

xfdesktop-marshal.c: xfdesktop-marshal.list Makefile
	$(AM_V_GEN) echo '#include "xfdesktop-marshal.h"' > $@ \
	&& glib-genmarshal --prefix=xfdesktop_marshal --body $< >> $@

DISTCLEANFILES = \
	$(libxfdesktop_built_sources)

BUILT_SOURCES = \
	$(libxfdesktop_built_sources)

endif

EXTRA_DIST = \
	xfdesktop-marshal.list
