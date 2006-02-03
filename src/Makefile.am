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
	xfce-desktop.h

desktop_icon_sources = \
	xfdesktop-icon.c \
	xfdesktop-icon.h \
	xfdesktop-icon-view.c \
	xfdesktop-icon-view.h \
	xfdesktop-window-icon.c \
	xfdesktop-window-icon.h

desktop_file_icon_sources = \
	xfdesktop-file-icon.c \
	xfdesktop-file-icon.h

if ENABLE_DESKTOP_ICONS
  xfdesktop_SOURCES += $(desktop_icon_sources)
if HAVE_THUNAR_VFS
    xfdesktop_SOURCES += $(desktop_file_icon_sources)
endif
endif

EXTRA_DIST = $(desktop_icon_sources) $(desktop_file_icon_sources)

xfdesktop_CFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/common	\
	@LIBX11_CFLAGS@	\
	@GMODULE_CFLAGS@ \
	@LIBXFCEGUI4_CFLAGS@ \
	@LIBXFCE4MCS_CLIENT_CFLAGS@ \
	@THUNAR_VFS_CFLAGS@ \
	-DLOCALEDIR=\"$(localedir)\" \
	-DDATADIR=\"$(datadir)\" \
	-DSYSCONFDIR=\"$(sysconfdir)\" \
	-DXFCEMODDIR=\"$(libdir)/xfce4/modules\"

xfdesktop_LDFLAGS = \
	-export-dynamic

# cygwin link order requires this split
xfdesktop_LDADD = $(top_builddir)/common/libxfdesktop.la
if BUILD_DESKTOP_MENU
  xfdesktop_LDADD += $(top_builddir)/common/libxfdesktop-menu.la
endif
xfdesktop_LDADD += \
	@LIBX11_LDFLAGS@ \
	@LIBX11_LIBS@ \
	@LIBXFCEGUI4_LIBS@ \
	@LIBXFCE4MCS_CLIENT_LIBS@ \
	@THUNAR_VFS_LIBS@
