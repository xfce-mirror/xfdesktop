# settings/Makefile.am
#

EXTRA_DIST = xfce-backdrop-settings.desktop

#
# backdrop settings
#
backdropdir = @XFCE_MCS_MANAGER_PLUGINSDIR@

backdrop_LTLIBRARIES = backdrop_settings.la

backdrop_settings_la_LDFLAGS = \
	-avoid-version \
	-module

if HAVE_CYGWIN
  backdrop_settings_la_LDFLAGS += \
	-no-undefined \
	-export-symbols $(datadir)/xfce4/devel/mcs-manager.def
endif

backdrop_settings_la_SOURCES = \
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
	-DDATADIR=\"$(datadir)\"					\
	-DBINDIR=\"$(bindir)\"

backdrop_settings_la_LIBADD =						\
	$(top_builddir)/common/libxfdesktop.la

if HAVE_CYGWIN
  backdrop_settings_la_LIBADD += \
	@XFCE_MCS_MANAGER_LIBS@ \
	@LIBX11_LIBS@
endif

appsdir = $(datadir)/applications
apps_DATA = xfce-backdrop-settings.desktop
