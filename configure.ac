dnl configure.ac
dnl
dnl xfdesktop - desktop manager for the XFce Desktop environment
dnl

dnl version information
m4_define([xfdesktop_version_major], [4])
m4_define([xfdesktop_version_minor], [1])
m4_define([xfdesktop_version_micro], [1])
m4_define([xfdesktop_version], [xfdesktop_version_major.xfdesktop_version_minor.xfdesktop_version_micro])

dnl init autoconf
AC_COPYRIGHT([Copyright (c) 2002-2004 The Xfce development team. All rights reserved.])
AC_INIT([xfdesktop], [xfdesktop_version], [xfce4-dev@xfce.org])
AC_PREREQ([2.50])

dnl init automake
XFDESKTOP_VERSION=xfdesktop_version
AC_SUBST(XFDESKTOP_VERSION)
AM_INIT_AUTOMAKE([xfdesktop], [$XFDESKTOP_VERSION])

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
AC_CHECK_HEADERS(fcntl.h signal.h stddef.h string.h sys/stat.h sys/types.h sys/wait.h time.h)
AC_CHECK_FUNCS(getenv mmap sigaction srandom)

dnl Check for i18n support
BM_I18N([xfdesktop], [ar az bg ca cs de es es_MX eu fa_IR fi fr hi hu it ja pl ko lt
	 ms nl pt_BR pt_PT ru sv ta tr uk vi zh_CN zh_TW])

dnl Check for X11 installed
BM_LIBX11_REQUIRE

XFCE_PANEL_PLUGIN([XFCE4_PANEL], [4.0.0])
BM_DEPEND([LIBXFCEGUI4], libxfcegui4-1.0, 4.1.10)
BM_DEPEND([LIBXFCE4UTIL], libxfce4util-1.0, 4.1.3)
BM_DEPEND([LIBXFCE4MCS_CLIENT],  libxfce4mcs-client-1.0, [4.0.0])
BM_DEPEND([GMODULE], gmodule-2.0, 2.0.6)
BM_DEPEND([GTHREAD], gthread-2.0, 2.0.6)

dnl check for libxml2 for menueditor
BM_DEPEND_CHECK([LIBXML], libxml-2.0, 2.4.0, [libxml2], [libxml2 (for xfce4-menueditor)], [yes])
if test "x$LIBXML_FOUND" = "xyes"; then
	MENUEDITOR_DIR="menueditor"
	build_menueditor="yes"
else
	MENUEDITOR_DIR=""
	build_menueditor="no"
fi
AC_SUBST(MENUEDITOR_DIR)

dnl Configure the MCS plugins
XFCE_MCS_PLUGIN([XFCE_MCS_MANAGER], [4.0.0])

dnl Build options

AC_ARG_ENABLE([desktop-menu],
	AC_HELP_STRING([--disable-desktop-menu],
		[Do not build the desktop menu module (default=enabled)]),
	[ac_cv_enable_desktop_menu=$enableval],
	[ac_cv_enable_desktop_menu=yes])
if test "x$ac_cv_enable_desktop_menu" = "xno"; then
	DESKTOP_MENU_DIR=""
	build_desktop_menu="no"
else
	DESKTOP_MENU_DIR="menu"
	build_desktop_menu="yes"
	AC_DEFINE([USE_DESKTOP_MENU], [1], [Define if we should build the desktop menu module])
fi
AC_SUBST(DESKTOP_MENU_DIR)
AM_CONDITIONAL([BUILD_DESKTOP_MENU_STUB],
	[test "x$build_desktop_menu" = "xyes" ])

AC_ARG_ENABLE([panel-plugin],
	[AC_HELP_STRING([--disable-panel-plugin],
		[Do not build the panel plugin (default=enabled)])],
	[ac_cv_enable_panel_plugin=$enableval],
	[ac_cv_enable_panel_plugin=yes])
if test "x$ac_cv_enable_panel_plugin" = "xno" -o "x$build_desktop_menu" = "xno"; then
	PANEL_PLUGIN_DIR=""
	build_panel_plugin="no"
else
	PANEL_PLUGIN_DIR="panel-plugin"
	build_panel_plugin="yes"
fi
AC_SUBST(PANEL_PLUGIN_DIR)

dnl Check for debugging support
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
menueditor/Makefile
modules/Makefile
modules/menu/Makefile
panel-plugin/Makefile
pixmaps/Makefile
po/Makefile.in
settings/Makefile
src/Makefile
xfdesktop.spec
])

echo
echo "Build Configuration:"
echo "  Build menu editor (requires libxml2):      $build_menueditor"
echo "  Build desktop menu module:                 $build_desktop_menu"
echo "  Build desktop menu panel plugin:           $build_panel_plugin"
echo
