plugindir = $(libdir)/xfce4/modules

plugin_LTLIBRARIES = libxfce4_desktop_menu.la

libxfce4_desktop_menu_la_SOURCES = \
	desktop-menu.c \
	desktop-menu-file.c \
	desktop-menu-dentry.c \
	desktop-menuspec.c

libxfce4_desktop_menu_la_CFLAGS = \
	@GMODULE_CFLAGS@ \
	@LIBXFCEGUI4_CFLAGS@ \
	-DXFCEDATADIR=\"$(datadir)/xfce4\" \
	-DSYSCONFDIR=\"$(sysconfdir)\"

libxfce4_desktop_menu_la_LDFLAGS = \
	-export-dynamic \
	-avoid-version

libxfce4_desktop_menu_la_LIBADD = \
	@GMODULE_LIBS@ \
	@LIBXFCEGUI4_LIBS@

xfceconfdir = $(sysconfdir)/xfce4
xfceconf_DATA = xfce-registered-categories.xml
