plugindir = $(libdir)/xfce4/modules

plugin_LTLIBRARIES = libxfce4_desktop_menu.la

libxfce4_desktop_menu_la_SOURCES = \
	desktop-menu.c \
	desktop-menu.h \
	desktop-menu-file.c \
	desktop-menu-file.h \
	desktop-menu-dentry.c \
	desktop-menu-dentry.h \
	desktop-menuspec.c \
	desktop-menuspec.h \
	desktop-menu-private.h

libxfce4_desktop_menu_la_CFLAGS = \
	@GMODULE_CFLAGS@ \
	@LIBXFCEGUI4_CFLAGS@ \
	-DSYSCONFDIR=\"$(sysconfdir)\" \
	-DDATADIR=\"$(datadir)\"

libxfce4_desktop_menu_la_LDFLAGS = \
	-export-dynamic \
	-avoid-version

libxfce4_desktop_menu_la_LIBADD = \
	@GMODULE_LIBS@ \
	@LIBXFCEGUI4_LIBS@

xfceconfdir = $(sysconfdir)/xfce4
xfceconf_DATA = xfce-registered-categories.xml

EXTRA_DIST = xfce-registered-categories.xml
