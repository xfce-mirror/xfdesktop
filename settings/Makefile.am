# settings/Makefile.am
#

EXTRA_DIST = xfce-backdrop-settings.desktop

#
# backdrop settings
#
backdropdir = @XFCE_MCS_MANAGER_PLUGINSDIR@

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
	settings_common.h						\
	menu_settings.c							\
	menu_settings.h

backdrop_settings_la_CFLAGS =						\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/common						\
	@XFCE_MCS_MANAGER_CFLAGS@					\
	-DLOCALEDIR=\"$(localedir)\"					\
	-DDATADIR=\"$(datadir)\"

backdrop_settings_la_LIBADD =						\
	$(top_builddir)/common/libxfdesktop.la

backdrop_settings.c: backdrop-icon.h

backdrop-icon.h: $(top_srcdir)/pixmaps/32x32/xfce4-backdrop.png
	gdk-pixbuf-csource --raw --build-list backdrop_icon_data \
		$(top_srcdir)/pixmaps/32x32/xfce4-backdrop.png > backdrop-icon.h

# icons (left out: menu-icon.h)
noinst_DATA = backdrop-icon.h 

appsdir = $(datadir)/applications
apps_DATA = xfce-backdrop-settings.desktop
