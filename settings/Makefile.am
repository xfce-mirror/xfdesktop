bin_PROGRAMS = xfdesktop-settings

xfdesktop_settings_SOURCES = \
	xfdesktop-settings-ui.h \
	xfdesktop-settings-ui.c \
	main.c

xfdesktop_settings_CFLAGS =						\
	-I$(top_srcdir)								\
	-I$(top_srcdir)/common						\
	-I$(top_srcdir)/src 						\
	$(LIBEXO_CFLAGS)							\
	$(LIBXFCE4UI_CFLAGS)						\
	$(LIBXFCE4WINDOWING_CFLAGS)					\
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
	$(LIBXFCE4WINDOWING_LIBS)					\
	$(LIBEXO_LIBS)								\
	$(GTHREAD_LIBS)								\
	$(LIBX11_LIBS)								\
	$(GTK_LIBS)

desktop_in_files = xfce-backdrop-settings.desktop.in
desktopdir = $(datadir)/applications
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

glade_files = \
	xfdesktop-settings-ui.glade \
	xfdesktop-settings-appearance-frame-ui.glade

BUILT_SOURCES = \
	xfdesktop-settings-ui.h \
	xfdesktop-settings-ui.c

EXTRA_DIST = \
	$(glade_files) \
	$(desktop_in_files)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(desktop_DATA)

%.desktop: $(srcdir)/%.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

%-ui.h: $(srcdir)/%-ui.gresource.xml $(glade_files) Makefile
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header --manual-register $<

%-ui.c: $(srcdir)/%-ui.gresource.xml $(glade_files) Makefile
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source --manual-register $<
