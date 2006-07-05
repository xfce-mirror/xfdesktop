bin_PROGRAMS = xfdesktop

xfdesktop_SOURCES = \
	main.c \
	main.h \
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
	xfce-desktop-settings.h

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
	xfdesktop-file-icon.c \
	xfdesktop-file-icon.h \
	xfdesktop-file-icon-manager.c \
	xfdesktop-file-icon-manager.h \
	xfdesktop-file-utils.c \
	xfdesktop-file-utils.h

xfdesktop_CFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/common	\
	-DLOCALEDIR=\"$(localedir)\" \
	-DDATADIR=\"$(datadir)\" \
	-DSYSCONFDIR=\"$(sysconfdir)\" \
	-DXFCEMODDIR=\"$(libdir)/xfce4/modules\" \
	$(LIBX11_CFLAGS)	\
	$(GMODULE_CFLAGS) \
	$(LIBXFCEGUI4_CFLAGS) \
	$(LIBXFCE4MCS_CLIENT_CFLAGS)

xfdesktop_LDFLAGS = \
	-export-dynamic

# cygwin link order requires this split
xfdesktop_LDADD = $(top_builddir)/common/libxfdesktop.la
if BUILD_DESKTOP_MENU
  xfdesktop_LDADD += $(top_builddir)/common/libxfdesktop-menu.la
endif
xfdesktop_LDADD += \
	$(LIBX11_LDFLAGS) \
	$(LIBX11_LIBS) \
	$(LIBXFCEGUI4_LIBS) \
	$(LIBXFCE4MCS_CLIENT_LIBS)


if ENABLE_DESKTOP_ICONS
  xfdesktop_SOURCES += $(desktop_icon_sources)

if HAVE_THUNAR_VFS
    xfdesktop_SOURCES += $(desktop_file_icon_sources)
endif

xfdesktop_CFLAGS += \
	$(LIBEXO_CFLAGS) \
	$(THUNAR_VFS_CFLAGS) \
	$(THUNARX_CFLAGS)

xfdesktop_LDADD += \
	$(LIBEXO_LIBS) \
	$(THUNAR_VFS_LIBS) \
	$(THUNARX_LIBS)

endif

EXTRA_DIST = $(desktop_icon_sources) $(desktop_file_icon_sources)
