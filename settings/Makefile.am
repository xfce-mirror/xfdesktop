bin_PROGRAMS = xfdesktop-settings

xfdesktop_settings_built_sources = \
	xfdesktop-settings-ui.h \
	xfdesktop-settings-ui.c

xfdesktop_settings_SOURCES = \
	main.c \
    xfdesktop-background-settings.c \
    xfdesktop-file-icon-settings.c \
    xfdesktop-icon-settings.c \
    xfdesktop-keyboard-shortcuts-settings.c \
    xfdesktop-menu-settings.c \
    xfdesktop-settings.h

nodist_xfdesktop_settings_SOURCES = \
	$(xfdesktop_settings_built_sources)

xfdesktop_settings_CFLAGS =						\
	-I$(top_srcdir)								\
	-I$(top_srcdir)/common						\
	-I$(top_srcdir)/src 						\
	-DDEFAULT_BACKDROP=\"$(DEFAULT_BACKDROP)\" \
	$(LIBXFCE4UI_CFLAGS)						\
	$(LIBXFCE4KBD_PRIVATE_CFLAGS)				\
	$(LIBXFCE4WINDOWING_CFLAGS)					\
	$(XFCONF_CFLAGS)							\
	$(GTHREAD_CFLAGS)							\
	$(LIBX11_CFLAGS)							\
	-DG_LOG_DOMAIN=\"xfdesktop-settings\"		\
	-DLOCALEDIR=\"$(localedir)\"				\
	-DDATADIR=\"$(datadir)\"					\
	-DBINDIR=\"$(bindir)\"

xfdesktop_settings_LDADD =						\
	$(top_builddir)/common/libxfdesktop.la		\
	$(XFCONF_LIBS)								\
	$(LIBXFCE4UI_LIBS)							\
	$(LIBXFCE4KBD_PRIVATE_LIBS)					\
	$(LIBXFCE4WINDOWING_LIBS)					\
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
	$(xfdesktop_settings_built_sources)

EXTRA_DIST = \
	$(glade_files) \
	$(desktop_in_files) \
	meson.build \
	xfdesktop-settings-ui.gresource.xml

CLEANFILES = \
	$(desktop_DATA) \
	$(xfdesktop_settings_built_sources)

%.desktop: $(srcdir)/%.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o - | \
		sed -e 's/\@VERSION_SHORT\@/$(VERSION_SHORT)/g' > $@

%-ui.h: $(srcdir)/%-ui.gresource.xml $(glade_files) Makefile
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header --manual-register $<

%-ui.c: $(srcdir)/%-ui.gresource.xml $(glade_files) Makefile
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source --manual-register $<
