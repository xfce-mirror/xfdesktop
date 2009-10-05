SUBDIRS = \
	directory-data \
	menu-data

plugindir = $(libdir)/xfce4/modules

plugin_LTLIBRARIES = xfce4_desktop_menu.la

xfce4_desktop_menu_la_SOURCES = \
	desktop-menu.c \
	xfdesktop-app-menu-item.c \
	xfdesktop-app-menu-item.h

xfce4_desktop_menu_la_CFLAGS = \
	-I$(top_srcdir)/common \
	$(GMODULE_CFLAGS) \
	$(GTHREAD_CFLAGS) \
	$(LIBXFCE4MENU_CFLAGS) \
	$(LIBX11_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(THUNAR_VFS_CFLAGS) \
	-DSYSCONFDIR=\"$(sysconfdir)\" \
	-DDATADIR=\"$(datadir)\" \
	-DBINDIR=\"$(bindir)\"

xfce4_desktop_menu_la_DEPENDENCIES = \
	$(top_builddir)/common/libxfdesktop-menu-utils.la

xfce4_desktop_menu_la_LDFLAGS = \
	-export-dynamic \
	-avoid-version \
	-module

if HAVE_CYGWIN
xfce4_desktop_menu_la_LDFLAGS += \
	-no-undefined
endif

xfce4_desktop_menu_la_LIBADD = \
	$(top_builddir)/common/libxfdesktop-menu-utils.la \
	$(GMODULE_LIBS) \
	$(GTHREAD_LIBS) \
	$(LIBXFCE4MENU_LIBS) \
	$(LIBX11_LDFLAGS) \
	$(LIBX11_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(THUNAR_VFS_LIBS)
