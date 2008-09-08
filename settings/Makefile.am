bin_PROGRAMS = xfdesktop-settings

xfdesktop_settings_SOURCES = \
	xfdesktop-settings_glade.h \
	main.c

xfdesktop_settings_CFLAGS =						\
	-I$(top_srcdir)								\
	-I$(top_srcdir)/common						\
	-I$(top_srcdir)/src 						\
	$(LIBEXO_CFLAGS)							\
	$(LIBXFCEGUI4_CFLAGS)						\
	$(XFCONF_CFLAGS)							\
	$(LIBGLADE_CFLAGS)							\
	-DEXO_API_SUBJECT_TO_CHANGE					\
	-DLOCALEDIR=\"$(localedir)\"				\
	-DDATADIR=\"$(datadir)\"					\
	-DBINDIR=\"$(bindir)\"

xfdesktop_settings_LDADD =						\
	$(top_builddir)/common/libxfdesktop.la		\
	$(XFCONF_LIBS)								\
	$(LIBXFCEGUI4_LIBS)							\
	$(LIBGLADE_LIBS)							\
	$(LIBEXO_LIBS)

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
xfdesktop-settings_glade.h

xfdesktop-settings_glade.h: $(srcdir)/xfdesktop-settings.glade
	exo-csource --static --name=xfdesktop_settings_glade $< >$@

endif

EXTRA_DIST = \
	xfdesktop-settings.glade \
	$(desktop_in_files)

CLEANFILES =  \
$(BUILT_SOURCES) \
$(desktop_DATA)
