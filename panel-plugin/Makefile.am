plugindir = @XFCE4_PANEL_PLUGINSDIR@

plugin_LTLIBRARIES = libdesktopmenu.la

libdesktopmenu_la_LDFLAGS = \
	-avoid-version \
	-export-dynamic \
	-module

if HAVE_CYGWIN
  libdesktopmenu_la_LDFLAGS += \
	-no-undefined \
	-export-symbols $(datadir)/xfce4/devel/panel.def
endif

libdesktopmenu_la_SOURCES = \
	desktop-menu-plugin.c

libdesktopmenu_la_CFLAGS = \
	-I$(top_srcdir)/common \
	@XFCE4_PANEL_CFLAGS@ \
	-DDATADIR=\"$(datadir)\"

libdesktopmenu_la_LIBADD = \
	@XFCE4_PANEL_LIBS@ \
	$(top_builddir)/common/libxfdesktop-menu.la
