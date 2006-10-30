plugindir = $(libdir)/xfce4/modules

plugin_LTLIBRARIES = xfce4_desktop_menu.la

xfce4_desktop_menu_la_SOURCES = \
	desktop-menu.c \
	desktop-menu.h \
	desktop-menu-cache.c \
	desktop-menu-cache.h \
	desktop-menu-file.c \
	desktop-menu-file.h \
	desktop-menu-dentry.c \
	desktop-menu-dentry.h \
	desktop-menuspec.c \
	desktop-menuspec.h \
	desktop-menu-private.h \
	dummy_icon.h

xfce4_desktop_menu_la_CFLAGS = \
	$(GMODULE_CFLAGS) \
	$(LIBX11_CFLAGS) \
	$(LIBXFCEGUI4_CFLAGS) \
	-DSYSCONFDIR=\"$(sysconfdir)\" \
	-DDATADIR=\"$(datadir)\" \
	-DBINDIR=\"$(bindir)\"

xfce4_desktop_menu_la_LDFLAGS = \
	$(top_builddir)/common/libxfdesktop.la \
	-export-dynamic \
	-avoid-version \
	-module

if HAVE_CYGWIN
xfce4_desktop_menu_la_LDFLAGS += \
	-no-undefined
endif

xfce4_desktop_menu_la_LIBADD = \
	$(GMODULE_LIBS) \
	$(LIBX11_LDFLAGS) \
	$(LIBX11_LIBS) \
	$(LIBXFCEGUI4_LIBS)

xfceconfdir = $(sysconfdir)/xdg/xfce4/desktop
xfceconf_DATA = \
	xfce-registered-categories.xml \
	xfce-registered-categories.xml.ca \
	xfce-registered-categories.xml.cs \
	xfce-registered-categories.xml.de \
	xfce-registered-categories.xml.el \
	xfce-registered-categories.xml.es \
	xfce-registered-categories.xml.eu \
	xfce-registered-categories.xml.fi \
	xfce-registered-categories.xml.fr \
	xfce-registered-categories.xml.he \
	xfce-registered-categories.xml.hu \
	xfce-registered-categories.xml.ja \
	xfce-registered-categories.xml.nl \
	xfce-registered-categories.xml.pl \
	xfce-registered-categories.xml.pt_BR \
	xfce-registered-categories.xml.ro \
	xfce-registered-categories.xml.ru \
	xfce-registered-categories.xml.sk \
	xfce-registered-categories.xml.sv \
	xfce-registered-categories.xml.zh_CN \
	xfce-registered-categories.xml.zh_TW

EXTRA_DIST = $(xfceconf_DATA)

