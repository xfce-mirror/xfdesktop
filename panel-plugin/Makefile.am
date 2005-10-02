plugindir = $(libexecdir)/xfce4/panel-plugins
plugin_PROGRAMS = xfce4-menu-plugin

xfce4_menu_plugin_SOURCES = \
	desktop-menu-plugin.c

xfce4_menu_plugin_CFLAGS = \
	-I$(top_srcdir)/common \
	@LIBXFCEGUI4_CFLAGS@ \
	@LIBXFCE4PANEL_CFLAGS@ \
	-DDATADIR=\"$(datadir)\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DBINDIR=\"$(bindir)\"

xfce4_menu_plugin_LDADD = \
	$(top_builddir)/common/libxfdesktop-menu.la \
	@LIBXFCEGUI4_LIBS@ \
	@LIBXFCE4PANEL_LIBS@

if HAVE_CYGWIN
  xfce4_menu_plugin_LDFLAGS = \
	-no-undefined \
	-export-symbols $(datadir)/xfce4/devel/panel.def
endif

# .desktop file
#
# Some automake trickery here. Because we cannot use $(libexecdir) in the
# automake stage, we'll use sed to get the full path into the .desktop file.
# We also need to let intltool merge the translated fields, so we add an
# additional level of indirection: a <name>.desktop.in.in file.
#
desktop_in_in_files = xfce4-menu.desktop.in.in
desktop_in_files = $(desktop_in_in_files:.desktop.in.in=.desktop.in)

desktopdir = $(datadir)/xfce4/panel-plugins
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = $(desktop_in_in_files)

DISTCLEANFILES = \
	$(desktop_DATA) $(desktop_in_files)

# get full path into .desktop file
%.desktop.in: %.desktop.in.in
	sed -e "s^@PLUGIN_PATH@^$(libexecdir)/xfce4/panel-plugins^" \
		$< > $@

