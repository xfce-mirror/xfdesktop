AC_INIT(xfdesktop.spec.in)

VERSION=`date +%y%m%d`
AC_SUBST(VERSION)

AM_INIT_AUTOMAKE(xfdesktop, $VERSION)
AM_MAINTAINER_MODE

AM_CONFIG_HEADER(config.h)

# Honor aclocal flags
ACLOCAL="$ACLOCAL $ACLOCAL_FLAGS"

AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL

AC_PATH_XTRA
AC_HEADER_STDC
AC_CHECK_FUNCS(setlocale srandom strchr strrchr strstr)
AC_CHECK_HEADERS(stdlib.h string.h unistd.h)

AC_CHECK_LIB(X11, main,, AC_MSG_ERROR([libX11 is missing from your system: Make sure you have both the  X Window System libraries and development headers correctly installed]), $X_CFLAGS $X_PRE_LIBS $X_EXTRA_LIBS $X_LIBS)
AC_CHECK_LIB(Xext, main,, AC_MSG_ERROR([The X Window System installed on this machine does not support the shape extension]), $X_CFLAGS $X_PRE_LIBS $X_EXTRA_LIBS $X_LIBS)
AC_CHECK_LIB(Xpm, main,, AC_MSG_ERROR([libXpm is missing from your system: Make sure you have both the Xpm libraries and development headers correctly installed]), $X_CFLAGS $X_PRE_LIBS $X_EXTRA_LIBS $X_LIBS)

## Versions of dependencies
GLIB_REQUIRED_VERSION=2.0.0
GTK_REQUIRED_VERSION=2.0.0
PANGO_REQUIRED_VERSION=1.0.0
LIBXFCEGUI4_REQUIRED_VERSION=0.0.15
LIBXFCE4MCS_REQUIRED_VERSION=0.0.4

AC_DEFINE_UNQUOTED(GLIB_REQUIRED_VERSION, $GLIB_REQUIRED_VERSION, [Define glib required version.])
AC_DEFINE_UNQUOTED(GTK_REQUIRED_VERSION, $GTK_REQUIRED_VERSION, [Define gtk required version.])
AC_DEFINE_UNQUOTED(PANGO_REQUIRED_VERSION, $PANGO_REQUIRED_VERSION, [Define pango required version.])
AC_DEFINE_UNQUOTED(LIBXFCEGUI4_REQUIRED_VERSION, $LIBXFCEGUI4_REQUIRED_VERSION, [Define libxfcegui4 required version.])
AC_DEFINE_UNQUOTED(LIBXFCE4MCS_REQUIRED_VERSION, $LIBXFCE4MCS_REQUIRED_VERSION, [Define libxfce4mcs required version.])

AC_SUBST(GLIB_REQUIRED_VERSION)
AC_SUBST(GTK_REQUIRED_VERSION)
AC_SUBST(PANGO_REQUIRED_VERSION)
AC_SUBST(LIBXFCEGUI4_REQUIRED_VERSION)
AC_SUBST(LIBXFCE4MCS_REQUIRED_VERSION)

PKG_CHECK_MODULES(BASE_DEPENDENCIES, glib-2.0 >= $GLIB_REQUIRED_VERSION gtk+-2.0 >= $GTK_REQUIRED_VERSION pango >= $PANGO_REQUIRED_VERSION libxfcegui4-1.0 >= $LIBXFCEGUI4_REQUIRED_VERSION libxml-2.0 libxfce4util-1.0 >= 0.0.0)

PKG_CHECK_MODULES(MCS_CLIENT,  libxfce4mcs-client-1.0 >= $LIBXFCE4MCS_REQUIRED_VERSION)
PKG_CHECK_MODULES(MCS_MANAGER, libxfce4mcs-manager-1.0 >= $LIBXFCE4MCS_REQUIRED_VERSION xfce-mcs-manager >= 0.1)

AC_PATH_PROG(GDK_PIXBUF_CSOURCE, gdk-pixbuf-csource, no)

if test x"$GDK_PIXBUF_CSOURCE" = xno; then
  AC_MSG_ERROR([gdk-pixbuf-csource executable not found in your path - should be installed with GTK])
fi

AC_SUBST(GDK_PIXBUF_CSOURCE)

dnl Check for debuggin support
BM_DEBUG_SUPPORT

AC_OUTPUT([
xfdesktop.spec
Makefile
doc/Makefile
doc/images/Makefile
doc/html/Makefile
doc/xml/Makefile
src/Makefile
settings/Makefile
backdrops/Makefile
])

