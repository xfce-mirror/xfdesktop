directorydir = $(datadir)/desktop-directories
directory_in_files = \
	xfce-accessories.directory.in \
	xfce-development.directory.in \
	xfce-education.directory.in \
	xfce-games.directory.in \
	xfce-graphics.directory.in \
	xfce-multimedia.directory.in \
	xfce-network.directory.in \
	xfce-office.directory.in \
	xfce-other.directory.in \
	xfce-science.directory.in \
	xfce-settings.directory.in \
	xfce-system.directory.in
directory_DATA = $(directory_in_files:.directory.in=.directory)
@INTLTOOL_DIRECTORY_RULE@

EXTRA_DIST = $(directory_in_files)

DISTCLEANFILES = $(directory_DATA)
