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

xfdesktop_CFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/common	\
	@LIBX11_CFLAGS@	\
	@GMODULE_CFLAGS@ \
	@LIBXFCEGUI4_CFLAGS@ \
	@LIBXFCE4MCS_CLIENT_CFLAGS@ \
	-DLOCALEDIR=\"$(localedir)\" \
	-DDATADIR=\"$(datadir)\" \
	-DSYSCONFDIR=\"$(sysconfdir)\" \
	-DXFCEMODDIR=\"$(libdir)/xfce4/modules\"

xfdesktop_LDADD = \
	$(top_builddir)/common/libxfdesktop.la \
	@LIBX11_LDFLAGS@ \
	@LIBX11_LIBS@ \
	@LIBXFCEGUI4_LIBS@ \
	@LIBXFCE4MCS_CLIENT_LIBS@

if BUILD_DESKTOP_MENU
  xfdesktop_LDADD += $(top_builddir)/common/libxfdesktop-menu.la
endif
