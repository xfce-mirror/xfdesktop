@SET_MAKE@

SUBDIRS = \
	backdrops \
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
	xfdesktop.spec \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in \
	$(conf_DATA) \
	$(manpage_DATA)

DISTCLEANFILES = \
	intltool-extract \
	intltool-merge \
	intltool-update

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-gen-doc

AUTOMAKE_OPTIONS = \
	1.8 \
	dist-bzip2

distclean-local:
	-rm -rf *.cache *~

rpm: distcheck
	rpmbuild -ta $(PACKAGE)-$(VERSION).tar.gz
	@rm -f $(PACKAGE)-$(VERSION).tar.gz

.PHONY: ChangeLog

ChangeLog: Makefile
	(GIT_DIR=$(top_srcdir)/.git git log > .changelog.tmp \
	&& mv .changelog.tmp ChangeLog; rm -f .changelog.tmp) \
	|| (touch ChangeLog; echo 'Git directory not found: installing possibly empty changelog.' >&2)

dist-hook: ChangeLog
