noinst_LTLIBRARIES = libxfdesktop.la

libxfdesktop_la_SOURCES = \
	xfdesktop-common.c \
	xfdesktop-common.h \
	xfdesktop-marshal.c \
	xfdesktop-marshal.h

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

DISTCLEANFILES = \
	$(xfdesktop_built_sources) \
	stamp-xfdesktop-marshal.h \
	xfdesktop-marshal.c \
	xfdesktop-marshal.h

xfdesktop-marshal.h: stamp-xfdesktop-marshal.h
	@true
stamp-xfdesktop-marshal.h: xfdesktop-marshal.list Makefile
	$(AM_V_GEN) glib-genmarshal --prefix=xfdesktop_marshal xfdesktop-marshal.list --header > xfdesktop-marshal.h && \
	echo timestamp > $(@F)
xfdesktop-marshal.c: xfdesktop-marshal.list Makefile
	$(AM_V_GEN) echo '#include "xfdesktop-marshal.h"' > xfdesktop-marshal.c && \
	glib-genmarshal --prefix=xfdesktop_marshal xfdesktop-marshal.list --body >> xfdesktop-marshal.c && \
	glib-genmarshal --prefix=xfdesktop_marshal xfdesktop-marshal.list --header > xfdesktop-marshal.h

EXTRA_DIST = \
	xfdesktop-marshal.list
