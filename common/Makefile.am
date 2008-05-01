noinst_LTLIBRARIES = libxfdesktop.la

libxfdesktop_la_SOURCES = \
	xfdesktop-common.c \
	xfdesktop-common.h

libxfdesktop_la_CFLAGS = \
	-I$(top_srcdir)/src \
	$(LIBXFCEGUI4_CFLAGS) \
	$(GTK_CFLAGS)

if BUILD_DESKTOP_MENU

noinst_LTLIBRARIES += \
	libxfdesktop-menu.la \
	libxfdesktop-menu-utils.la

libxfdesktop_menu_la_SOURCES = \
	desktop-menu-stub.c \
	desktop-menu-stub.h

libxfdesktop_menu_la_CFLAGS = \
	$(GMODULE_CFLAGS) \
	$(GTK_CFLAGS) \
	-DXFCEMODDIR=\"$(libdir)/xfce4/modules\"

libxfdesktop_menu_utils_la_SOURCES = \
	desktop-menu-utils.c \
	desktop-menu-utils.h

libxfdesktop_menu_utils_la_CFLAGS = \
	$(LIBXFCE4UTIL_CFLAGS)

endif

if HAVE_CYGWIN
  libxfdesktop_menu_la_LDLAGS = -no-undefined
endif
