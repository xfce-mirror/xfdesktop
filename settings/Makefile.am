# settings/Makefile.am
#

EXTRA_DIST =								\
	backdrop.png							\
	workspaces.png

#
# backdrop settings
#
backdropdir = $(libdir)/xfce-mcs-manager/plugins/

backdrop_LTLIBRARIES = backdrop_settings.la

backdrop_settings_la_LDFLAGS =						\
	-avoid-version							\
	-module

backdrop_settings_la_SOURCES =						\
	backdrop-icon.h 						\
	backdrop_settings.c						\
	backdrop-mgr.c							\
	backdrop-mgr.h							\
	settings_common.c						\
	settings_common.h

backdrop_settings_la_CFLAGS =						\
	-I$(top_srcdir)/common						\
	@LIBXFCEGUI4_CFLAGS@						\
	@LIBXFCE4MCS_MANAGER_CFLAGS@					\
	-DLOCALEDIR=\"$(localedir)\"					\
	-DDATADIR=\"$(datadir)\"

backdrop_settings_la_LIBADD =						\
	$(top_builddir)/common/libxfdesktop.la

backdrop-icon.h: $(srcdir)/backdrop.png
	gdk-pixbuf-csource --raw --build-list				\
	backdrop_icon_data $(srcdir)/backdrop.png > backdrop-icon.h

#
# workspaces settings
#
workspacesdir = $(libdir)/xfce-mcs-manager/plugins/

workspaces_LTLIBRARIES = workspaces_settings.la

workspaces_settings_la_LDFLAGS =					\
	-avoid-version							\
	-module

workspaces_settings_la_SOURCES =					\
	workspaces-icon.h						\
	workspaces_settings.c						\
	workspaces_settings.h						\
	settings_common.c						\
	settings_common.h

workspaces_settings_la_CFLAGS =						\
	-I$(top_srcdir)/common						\
	@LIBXFCEGUI4_CFLAGS@						\
	@LIBXFCE4MCS_MANAGER_CFLAGS@					\
	-DLOCALEDIR=\"$(localedir)\"					\
	-DDATADIR=\"$(datadir)\"

workspaces_settings_la_LIBADD =						\
	$(top_builddir)/common/libxfdesktop.la

# icons (left out: menu-icon.h)
noinst_DATA = backdrop-icon.h workspaces-icon.h 

workspaces-icon.h: $(srcdir)/workspaces.png
	gdk-pixbuf-csource --raw --build-list 				\
	workspaces_icon_data $(srcdir)/workspaces.png > workspaces-icon.h


