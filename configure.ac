dnl configure.ac
dnl
dnl xfdesktop - desktop manager for the XFce Desktop environment
dnl

AC_INIT([configure.ac])

AM_INIT_AUTOMAKE([xfdesktop], [4.0.2])

AM_CONFIG_HEADER([config.h])

AM_MAINTAINER_MODE

dnl check for UNIX variants
AC_AIX
AC_ISC_POSIX
AC_MINIX

dnl check for basic programs
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL

dnl check for standard header files
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h signal.h stddef.h sys/wait.h time.h)
AC_CHECK_FUNCS(sigaction srandom)

dnl Check for i18n support
BM_I18N([xfdesktop], [az ca cs de es es_MX fi fr hi hu it ja pl ko lt ms nl
	 no pl pt_PT ru sv ta tr uk vi zh_CN zh_TW])

dnl Check for X11 installed
BM_LIBX11_REQUIRE

BM_DEPEND([LIBXFCEGUI4], libxfcegui4-1.0, 4.0.0)
BM_DEPEND([LIBXML], libxml-2.0, 2.4.0)
BM_DEPEND([LIBXFCE4MCS_CLIENT],  libxfce4mcs-client-1.0, [4.0.0])

dnl Configure the MCS plugins
XFCE_MCS_PLUGIN([XFCE_MCS_MANAGER], [4.0.0])

dnl Check for debuggin support
BM_DEBUG_SUPPORT

AC_OUTPUT([
backdrops/Makefile
common/Makefile
doc/Makefile
doc/C/Makefile
doc/C/images/Makefile
doc/fr/Makefile
doc/fr/images/Makefile
Makefile
po/Makefile.in
settings/Makefile
src/Makefile
xfdesktop.spec
])

