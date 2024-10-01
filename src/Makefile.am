# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:

bin_PROGRAMS = xfdesktop

xfdesktop_built_sources = \
	xfdesktop-file-manager-fdo-proxy.c \
	xfdesktop-file-manager-fdo-proxy.h \
	xfdesktop-file-manager-proxy.c \
	xfdesktop-file-manager-proxy.h \
	xfdesktop-thunar-proxy.c \
	xfdesktop-thunar-proxy.h \
	xfdesktop-trash-proxy.c \
	xfdesktop-trash-proxy.h

if HAVE_LIBNOTIFY
xfdesktop_notify_sources = \
	xfdesktop-notify.c \
	xfdesktop-notify.h
endif

xfdesktop_SOURCES = \
	$(xfdesktop_notify_sources) \
	main.c \
	menu.c \
	menu.h \
	windowlist.c \
	windowlist.h \
	xfce-desktop.c \
	xfce-desktop.h \
	xfdesktop-application.c \
	xfdesktop-application.h \
	xfdesktop-backdrop-cycler.c \
	xfdesktop-backdrop-cycler.h \
	xfdesktop-backdrop-manager.c \
	xfdesktop-backdrop-manager.h \
	xfdesktop-backdrop-renderer.c \
	xfdesktop-backdrop-renderer.h

if ENABLE_X11
xfdesktop_SOURCES += \
	xfdesktop-x11.c \
	xfdesktop-x11.h
endif

desktop_icon_sources = \
	xfdesktop-cell-renderer-icon-label.c \
	xfdesktop-cell-renderer-icon-label.h \
	xfdesktop-icon.c \
	xfdesktop-icon.h \
	xfdesktop-icon-view.c \
	xfdesktop-icon-view.h \
	xfdesktop-icon-view-holder.c \
	xfdesktop-icon-view-holder.h \
	xfdesktop-icon-view-manager.c \
	xfdesktop-icon-view-manager.h \
	xfdesktop-icon-view-model.c \
	xfdesktop-icon-view-model.h \
	xfdesktop-window-icon-manager.c \
	xfdesktop-window-icon-manager.h \
	xfdesktop-window-icon-model.c \
	xfdesktop-window-icon-model.h

desktop_file_icon_sources = \
	xfdesktop-clipboard-manager.c \
	xfdesktop-clipboard-manager.h \
	xfdesktop-file-icon.c \
	xfdesktop-file-icon.h \
	xfdesktop-file-icon-manager.c \
	xfdesktop-file-icon-manager.h \
	xfdesktop-file-icon-model.c \
	xfdesktop-file-icon-model.h \
	xfdesktop-file-icon-model-filter.c \
	xfdesktop-file-icon-model-filter.h \
	xfdesktop-file-utils.c \
	xfdesktop-file-utils.h \
	xfdesktop-icon-position-configs.c \
	xfdesktop-icon-position-configs.h \
	xfdesktop-icon-position-migration.c \
	xfdesktop-icon-position-migration.h \
	xfdesktop-regular-file-icon.c \
	xfdesktop-regular-file-icon.h \
	xfdesktop-special-file-icon.c \
	xfdesktop-special-file-icon.h \
	xfdesktop-volume-icon.c \
	xfdesktop-volume-icon.h

xfdesktop_CFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/common	\
	-I$(top_builddir)/common \
	-DBINDIR=\"$(bindir)\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DDATADIR=\"$(datadir)\" \
	-DSYSCONFDIR=\"$(sysconfdir)\" \
	-DXFCEMODDIR=\"$(libdir)/xfce4/modules\" \
	-DDEFAULT_BACKDROP=\"$(DEFAULT_BACKDROP)\" \
	$(GIO_CFLAGS) \
	$(GIO_UNIX_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTHREAD_CFLAGS) \
	$(GTK_CFLAGS) \
	$(LIBNOTIFY_CFLAGS) \
	$(LIBX11_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(LIBXFCE4WINDOWING_CFLAGS) \
	$(LIBXFCE4WINDOWINGUI_CFLAGS) \
	$(LIBXFCE4SMCLIENT_PRIVATE_CFLAGS) \
	$(XFCONF_CFLAGS) \
	-DEXO_API_SUBJECT_TO_CHANGE \
	$(LIBEXO_CFLAGS) \
	$(GTK_LAYER_SHELL_CFLAGS) \
	$(LIBYAML_CFLAGS)

xfdesktop_LDFLAGS = \
	-export-dynamic

# cygwin link order requires this split
xfdesktop_LDADD = $(top_builddir)/common/libxfdesktop.la
xfdesktop_LDADD += \
	$(GIO_LIBS) \
	$(GIO_UNIX_LIBS) \
	$(GLIB_LIBS) \
	$(GTHREAD_LIBS) \
	$(GTK_LIBS) \
	$(LIBNOTIFY_LIBS) \
	$(LIBX11_LDFLAGS) \
	$(LIBX11_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(LIBXFCE4WINDOWING_LIBS) \
	$(LIBXFCE4WINDOWINGUI_LIBS) \
	$(LIBXFCE4SMCLIENT_PRIVATE_LIBS) \
	$(XFCONF_LIBS) \
	$(LIBEXO_LIBS) \
	$(GTK_LAYER_SHELL_LIBS) \
	$(LIBYAML_LIBS) \
	-lm

if BUILD_DESKTOP_MENU

xfdesktop_CFLAGS += \
	$(GARCON_CFLAGS) \
	$(GARCON_GTK3_CFLAGS)

xfdesktop_LDADD += \
	$(GARCON_LIBS) \
	$(GARCON_GTK3_LIBS)

endif

if ENABLE_DESKTOP_ICONS

xfdesktop_SOURCES += $(desktop_icon_sources)

if ENABLE_FILE_ICONS

xfdesktop_built_sources += \
	xfdesktop-monitor-chooser-ui.c \
	xfdesktop-monitor-chooser-ui.h

xfdesktop_SOURCES += $(desktop_file_icon_sources)

xfdesktop_CFLAGS += \
	$(THUNARX_CFLAGS)

xfdesktop_LDADD += \
	$(THUNARX_LIBS)

endif
endif

nodist_xfdesktop_SOURCES = \
	$(xfdesktop_built_sources)

glade_files = \
	xfdesktop-monitor-chooser-ui.glade

BUILT_SOURCES = \
	$(xfdesktop_built_sources)

CLEANFILES = \
	$(xfdesktop_built_sources)

EXTRA_DIST = \
	$(desktop_icon_sources) \
	$(desktop_file_icon_sources) \
	$(glade_files) \
	meson.build \
	xfdesktop-file-manager-fdo-dbus.xml \
	xfdesktop-file-manager-dbus.xml \
	xfdesktop-monitor-chooser-ui.gresource.xml \
	xfdesktop-thunar-dbus.xml \
	xfdesktop-trash-dbus.xml

xfdesktop-file-manager-fdo-proxy.c xfdesktop-file-manager-fdo-proxy.h : $(srcdir)/xfdesktop-file-manager-fdo-dbus.xml Makefile
	$(AM_V_GEN) $(GDBUS_CODEGEN) \
		--c-namespace=Xfdesktop \
		--interface-prefix=org.freedesktop. \
		--generate-c-code=xfdesktop-file-manager-fdo-proxy \
		$<

xfdesktop-file-manager-proxy.c xfdesktop-file-manager-proxy.h : $(srcdir)/xfdesktop-file-manager-dbus.xml Makefile
	$(AM_V_GEN) $(GDBUS_CODEGEN) \
		--c-namespace=Xfdesktop \
		--interface-prefix=org.xfce. \
		--generate-c-code=xfdesktop-file-manager-proxy \
		$<

xfdesktop-thunar-proxy.c xfdesktop-thunar-proxy.h : $(srcdir)/xfdesktop-thunar-dbus.xml Makefile
	$(AM_V_GEN) $(GDBUS_CODEGEN) \
		--c-namespace=Xfdesktop \
		--interface-prefix=org.xfce. \
		--generate-c-code=xfdesktop-thunar-proxy \
		$<

xfdesktop-trash-proxy.c xfdesktop-trash-proxy.h : $(srcdir)/xfdesktop-trash-dbus.xml Makefile
	$(AM_V_GEN) $(GDBUS_CODEGEN) \
		--c-namespace=Xfdesktop \
		--interface-prefix=org.xfce. \
		--generate-c-code=xfdesktop-trash-proxy \
		$<

%-ui.h: $(srcdir)/%-ui.gresource.xml $(glade_files) Makefile
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header --manual-register $<

%-ui.c: $(srcdir)/%-ui.gresource.xml $(glade_files) Makefile
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source --manual-register $<
