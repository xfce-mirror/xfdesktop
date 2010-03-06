bin_PROGRAMS = xfdesktop-settings

xfdesktop_settings_SOURCES = \
	xfdesktop-settings-appearance-frame-ui.h \
	xfdesktop-settings-ui.h \
	main.c

xfdesktop_settings_CFLAGS =						\
	-I$(top_srcdir)								\
	-I$(top_srcdir)/common						\
	-I$(top_srcdir)/src 						\
	$(LIBEXO_CFLAGS)							\
	$(LIBXFCE4UI_CFLAGS)						\
	$(XFCONF_CFLAGS)							\
	$(GTHREAD_CFLAGS)							\
	$(LIBX11_CFLAGS)							\
	-DG_LOG_DOMAIN=\"xfdesktop-settings\"		\
	-DEXO_API_SUBJECT_TO_CHANGE					\
	-DLOCALEDIR=\"$(localedir)\"				\
	-DDATADIR=\"$(datadir)\"					\
	-DBINDIR=\"$(bindir)\"

xfdesktop_settings_LDADD =						\
	$(top_builddir)/common/libxfdesktop.la		\
	$(XFCONF_LIBS)								\
	$(LIBXFCE4UI_LIBS)							\
	$(LIBEXO_LIBS)								\
	$(GTHREAD_LIBS)								\
	$(LIBX11_LIBS)

if HAVE_CYGWIN
  xfdesktop_settings_LDADD += \
	$(XFCONF_LIBS) \
	$(LIBX11_LIBS)
endif

desktop_in_files = xfce-backdrop-settings.desktop.in
desktopdir = $(datadir)/applications
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

if MAINTAINER_MODE

BUILT_SOURCES = \
	xfdesktop-settings-appearance-frame-ui.h \
	xfdesktop-settings-ui.h

SUFFIXES: .ui

%-ui.h: $(srcdir)/%-ui.xml Makefile
	exo-csource --static --name=`basename $< .xml | sed -e 's/-/_/g'`  $< >$@

endif

EXTRA_DIST = \
	xfdesktop-settings-ui.xml \
	xfdesktop-settings-appearance-frame-ui.xml \
	$(desktop_in_files)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(desktop_DATA)
