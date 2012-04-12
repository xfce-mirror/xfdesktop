@SET_MAKE@

SUBDIRS = \
	backgrounds \
	common \
	doc \
	po \
	settings \
	src \
	pixmaps

manpagedir = $(mandir)/man1
manpage_DATA = xfdesktop.1

EXTRA_DIST = \
	TODO \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in \
	$(conf_DATA) \
	$(manpage_DATA)

DISTCLEANFILES = \
	intltool-extract \
	intltool-merge \
	intltool-update

AUTOMAKE_OPTIONS = \
	1.8 \
	dist-bzip2

distuninstallcheck_listfiles = \
	find . -type f -print | grep -v ./share/icons/hicolor/icon-theme.cache

distclean-local:
	-rm -rf *.cache *~

.PHONY: ChangeLog

ChangeLog: Makefile
	(GIT_DIR=$(top_srcdir)/.git git log > .changelog.tmp \
	&& mv .changelog.tmp ChangeLog; rm -f .changelog.tmp) \
	|| (touch ChangeLog; echo 'Git directory not found: installing possibly empty changelog.' >&2)

dist-hook: ChangeLog
