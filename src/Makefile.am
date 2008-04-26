bin_PROGRAMS = xfdesktop

xfdesktop_SOURCES = \
	main.c \
	menu.c \
	menu.h \
	settings.c \
	settings.h \
	windowlist.c \
	windowlist.h \
	xfce-backdrop.c \
	xfce-backdrop.h \
	xfce-desktop.c \
	xfce-desktop.h \
	xfce-desktop-settings.c \
	xfce-desktop-settings.h \
	xfdesktop-marshal.c \
	xfdesktop-marshal.h

desktop_icon_sources = \
	xfdesktop-icon.c \
	xfdesktop-icon.h \
	xfdesktop-icon-view.c \
	xfdesktop-icon-view.h \
	xfdesktop-icon-view-manager.c \
	xfdesktop-icon-view-manager.h \
	xfdesktop-window-icon.c \
	xfdesktop-window-icon.h \
	xfdesktop-window-icon-manager.c \
	xfdesktop-window-icon-manager.h

desktop_file_icon_sources = \
	xfdesktop-clipboard-manager.c \
	xfdesktop-clipboard-manager.h \
	xfdesktop-dbus-bindings-filemanager.h \
	xfdesktop-dbus-bindings-trash.h \
	xfdesktop-file-icon.c \
	xfdesktop-file-icon.h \
	xfdesktop-file-icon-manager.c \
	xfdesktop-file-icon-manager.h \
	xfdesktop-file-properties-dialog.c \
	xfdesktop-file-properties-dialog.h \
	xfdesktop-file-utils.c \
	xfdesktop-file-utils.h \
	xfdesktop-regular-file-icon.c \
	xfdesktop-regular-file-icon.h \
	xfdesktop-special-file-icon.c \
	xfdesktop-special-file-icon.h \
	xfdesktop-volume-icon.c \
	xfdesktop-volume-icon.h

xfdesktop_CFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/common	\
	-DWNCK_I_KNOW_THIS_IS_UNSTABLE \
	-DBINDIR=\"$(bindir)\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DDATADIR=\"$(datadir)\" \
	-DSYSCONFDIR=\"$(sysconfdir)\" \
	-DXFCEMODDIR=\"$(libdir)/xfce4/modules\" \
	$(LIBX11_CFLAGS)	\
	$(GTHREAD_CFLAGS) \
	$(GMODULE_CFLAGS) \
	$(LIBXFCEGUI4_CFLAGS) \
	$(LIBXFCE4MCS_CLIENT_CFLAGS) \
	$(LIBWNCK_CFLAGS)

xfdesktop_LDFLAGS = \
	-export-dynamic

# cygwin link order requires this split
xfdesktop_LDADD = $(top_builddir)/common/libxfdesktop.la
if BUILD_DESKTOP_MENU
  xfdesktop_LDADD += $(top_builddir)/common/libxfdesktop-menu.la
  xfdesktop_DEPENDENCIES = $(top_builddir)/common/libxfdesktop-menu.la
endif
xfdesktop_LDADD += \
	$(LIBX11_LDFLAGS) \
	$(LIBX11_LIBS) \
	$(GTHREAD_LIBS) \
	$(LIBXFCEGUI4_LIBS) \
	$(LIBXFCE4MCS_CLIENT_LIBS) \
	$(LIBWNCK_LIBS)


if ENABLE_DESKTOP_ICONS

xfdesktop_SOURCES += $(desktop_icon_sources)

xfdesktop_CFLAGS += \
	-DEXO_API_SUBJECT_TO_CHANGE \
	$(LIBEXO_CFLAGS)

xfdesktop_LDADD += \
	$(LIBEXO_LIBS)


if ENABLE_FILE_ICONS

xfdesktop_SOURCES += $(desktop_file_icon_sources)

xfdesktop_CFLAGS += \
	-DDBUS_API_SUBJECT_TO_CHANGE \
	$(THUNAR_VFS_CFLAGS) \
	$(THUNARX_CFLAGS) \
	$(DBUS_CFLAGS)

xfdesktop_LDADD += \
	$(THUNAR_VFS_LIBS) \
	$(THUNARX_LIBS) \
	$(DBUS_LIBS)

endif
endif

if MAINTAINER_MODE

BUILT_SOURCES = \
	xfdesktop-marshal.c \
	xfdesktop-marshal.h

xfdesktop-marshal.h: stamp-xfdesktop-marshal.h
	@true
stamp-xfdesktop-marshal.h: xfdesktop-marshal.list Makefile
	glib-genmarshal --prefix=xfdesktop_marshal xfdesktop-marshal.list --header > xfdesktop-marshal.h
	echo timestamp > $(@F)
xfdesktop-marshal.c: xfdesktop-marshal.list Makefile
	echo '#include "xfdesktop-marshal.h"' > xfdesktop-marshal.c
	glib-genmarshal --prefix=xfdesktop_marshal xfdesktop-marshal.list --body >> xfdesktop-marshal.c


if ENABLE_FILE_ICONS

BUILT_SOURCES += \
	xfdesktop-dbus-bindings-filemanager.h \
	xfdesktop-dbus-bindings-trash.h
DISTCLEANFILES = \
	xfdesktop-dbus-bindings-filemanager.h \
	xfdesktop-dbus-bindings-trash.h

xfdesktop-dbus-bindings-filemanager.h: $(srcdir)/xfdesktop-dbus-bindings-filemanager.xml Makefile
	dbus-binding-tool --mode=glib-client --prefix=org_xfce_FileManager $(srcdir)/xfdesktop-dbus-bindings-filemanager.xml > xfdesktop-dbus-bindings-filemanager.h

xfdesktop-dbus-bindings-trash.h: $(srcdir)/xfdesktop-dbus-bindings-trash.xml Makefile
	dbus-binding-tool --mode=glib-client --prefix=org_xfce_Trash $(srcdir)/xfdesktop-dbus-bindings-trash.xml > xfdesktop-dbus-bindings-trash.h

endif

endif

EXTRA_DIST = \
	$(desktop_icon_sources) \
	$(desktop_file_icon_sources) \
	xfdesktop-marshal.list
