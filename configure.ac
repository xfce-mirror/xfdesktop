dnl configure.ac
dnl
dnl xfdesktop - desktop manager for the XFce Desktop environment
dnl

AC_INIT(common/backdrop.c)

AM_INIT_AUTOMAKE(xfdesktop, 0.1)

AM_CONFIG_HEADER(config.h)

AM_MAINTAINER_MODE

AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL

AC_HEADER_STDC
AC_CHECK_FUNCS(srandom)

dnl Check for X11 installed
BM_LIBX11_REQUIRE

BM_DEPEND(GLIB, glib-2.0, 2.0.0)
BM_DEPEND(PANGO, pango, 1.0.0)
BM_DEPEND(GTK, gtk+-2.0, 2.0.0)
BM_DEPEND(LIBXFCEGUI4, libxfcegui4-1.0, 0.0.17)
BM_DEPEND(LIBXFCE4UTIL, libxfce4util-1.0, 0.0)
BM_DEPEND(LIBXML, libxml-2.0, 2.4.0)
BM_DEPEND(LIBXFCE4MCS_CLIENT,  libxfce4mcs-client-1.0, 0.0.4)
BM_DEPEND(LIBXFCE4MCS_MANAGER, libxfce4mcs-manager-1.0, 0.0.4)

dnl Check for debuggin support
BM_DEBUG_SUPPORT

AC_OUTPUT([
xfdesktop.spec
Makefile
common/Makefile
doc/Makefile
doc/images/Makefile
doc/html/Makefile
doc/xml/Makefile
src/Makefile
settings/Makefile
backdrops/Makefile
])

