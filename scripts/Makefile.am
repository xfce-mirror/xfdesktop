migrationscriptdir = $(datadir)/xfce4/xfconf-migration
migrationscript_in_files = xfdesktop-xfconf-migration.pl.in
migrationscript_DATA = $(migrationscript_in_files:.pl.in=.pl)

%.pl: $(srcdir)/%.pl.in
	sed 's,\@sysconfdir\@,$(sysconfdir),g' < $< > $@

autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_files = xfconf-xfdesktop-migration-4.6.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

%.desktop: $(srcdir)/%.desktop.in
	sed 's,\@datadir\@,$(datadir),' < $< >$@

EXTRA_DIST = \
	$(migrationscript_in_files) \
	$(autostart_in_files)

DISTCLEANFILES = \
	$(migrationscript_DATA) \
	$(autostart_DATA)
