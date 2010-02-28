desktopdir = $(datadir)/applications
desktop_in_files = \
	xfce4-about-xfce.desktop.in \
	xfce4-help.desktop.in \
	xfce4-logout.desktop.in \
	xfce4-run-program.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

menudir = $(sysconfdir)/xdg/menus
menu_DATA = xfce-applications.menu

EXTRA_DIST = $(desktop_in_files) $(menu_DATA)

DISTCLEANFILES = $(desktop_DATA)
