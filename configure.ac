dnl configure.ac
dnl
dnl xfdesktop - xfce4's desktop manager
dnl

dnl version info
m4_define([xfdesktop_version], [4.3.0])

dnl init autoconf
AC_INIT([xfdesktop], [xfdesktop_version], [bjt23@cornell.edu])
AC_PREREQ([2.50])

dnl init automake
XFDESKTOP_VERSION=xfdesktop_version
AM_INIT_AUTOMAKE([xfdesktop], [$XFDESKTOP_VERSION])
AM_CONFIG_HEADER([config.h])
AM_MAINTAINER_MODE

dnl check for UNIX variants
AC_AIX
AC_ISC_POSIX
AC_MINIX
AM_CONDITIONAL([HAVE_CYGWIN], [test "`uname | grep \"CYGWIN\"`" != ""])

dnl check for basic programs
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AC_PROG_INTLTOOL

dnl check for standard header files
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h math.h signal.h stddef.h string.h sys/stat.h sys/types.h sys/wait.h time.h unistd.h)
AC_CHECK_FUNCS(mmap sigaction srandom)

dnl Check for i18n support
XDT_I18N([ar az be bg bn_IN ca cs de el en_GB es es_MX et eu fa fi fr \
          gu he hi hu hy it ja pl ko lt mr ms nb_NO nl pa pt_BR pt_PT \
          ro ru sk sv ta tr uk vi zh_CN zh_TW])

dnl Check for X11 installed
XDT_CHECK_LIBX11_REQUIRE
XDT_CHECK_LIBSM

dnl required
XDT_CHECK_PACKAGE([GMODULE], [gmodule-2.0], [2.2.0])
XDT_CHECK_PACKAGE([GOBJECT], [gobject-2.0], [2.2.0])
XDT_CHECK_PACKAGE([GTK], [gtk+-2.0], [2.2.0])
XDT_CHECK_PACKAGE([LIBXFCE4UTIL], [libxfce4util-1.0], [4.2.0])
XDT_CHECK_PACKAGE([LIBXFCEGUI4], [libxfcegui4-1.0], [4.3.4])
XDT_CHECK_PACKAGE([LIBXFCE4MCS_CLIENT],  [libxfce4mcs-client-1.0], [4.2.0])
XDT_XFCE_PANEL_PLUGIN([XFCE4_PANEL], [4.2.0])
XDT_XFCE_MCS_PLUGIN([XFCE_MCS_MANAGER], [4.2.0])

dnl Build options
AC_ARG_ENABLE([menu-editor],
        AC_HELP_STRING([--disable-menueditor],
                [Do not build the menueditor (default=enabled)]),
        [ac_cv_enable_menueditor=$enableval],
        [ac_cv_enable_menueditor=yes])
if test "x$ac_cv_enable_menueditor" = "xno"; then
        build_menueditor="no"
else
        build_menueditor="yes"
fi
AM_CONDITIONAL([BUILD_MENUEDITOR], [test "x$build_menueditor" = "xyes"])

AC_ARG_ENABLE([desktop-menu],
        AC_HELP_STRING([--disable-desktop-menu],
                [Do not build the desktop menu module (default=enabled)]),
        [ac_cv_enable_desktop_menu=$enableval],
        [ac_cv_enable_desktop_menu=yes])
if test "x$ac_cv_enable_desktop_menu" = "xno"; then
        build_desktop_menu="no"
else
        build_desktop_menu="yes"
        AC_DEFINE([USE_DESKTOP_MENU], [1],
                [Define if we should build the desktop menu module])
fi
AC_SUBST(DESKTOP_MENU_DIR)
AM_CONDITIONAL([BUILD_DESKTOP_MENU], [test "x$build_desktop_menu" = "xyes"])

AC_ARG_ENABLE([panel-plugin],
        [AC_HELP_STRING([--disable-panel-plugin],
                [Do not build the panel plugin (default=enabled)])],
        [ac_cv_enable_panel_plugin=$enableval],
        [ac_cv_enable_panel_plugin=yes])
if test "x$ac_cv_enable_panel_plugin" = "xno" -o "x$build_desktop_menu" = "xno"; then
        build_panel_plugin="no"
else
        build_panel_plugin="yes"
fi
AM_CONDITIONAL([BUILD_PANEL_PLUGIN], [test "x$build_panel_plugin" = "xyes"])

dnl check for debugging support
BM_DEBUG_SUPPORT

AC_OUTPUT([
Makefile
backdrops/Makefile
common/Makefile
doc/Makefile
doc/C/Makefile
doc/C/images/Makefile
doc/fr/Makefile
doc/fr/images/Makefile
doc/it/Makefile
doc/it/images/Makefile
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
echo "  Build menu editor:                            $build_menueditor"
echo "  Build desktop menu module:                    $build_desktop_menu"
echo "  Build desktop menu panel plugin:              $build_panel_plugin"
echo
