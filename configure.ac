dnl
dnl Copyright (c) 2003-2024
dnl         The Xfce development team. All rights reserved.
dnl
dnl xfdesktop - xfce4's desktop manager
dnl

dnl minimum required versions
m4_define([libx11_minimum_version], [1.6.7])
m4_define([glib_minimum_version], [2.72.0])
m4_define([gtk_minimum_version], [3.24.0])
m4_define([libxfce4util_minimum_version], [4.13.0])
m4_define([libxfce4ui_minimum_version], [4.18.0])
m4_define([libxfce4windowing_minimum_version], [4.19.8])
m4_define([garcon_minimum_version], [0.6.0])
m4_define([exo_minimum_version], [0.11.0])
m4_define([thunar_minimum_version], [4.17.10])
m4_define([xfconf_minimum_version], [4.19.3])
m4_define([cairo_minimum_version], [1.16])
m4_define([gtk_layer_shell_minimum_version], [0.7.0])
m4_define([libyaml_minimum_version], [0.2.5])

dnl version info
XDT_VERSION_INIT([4.20.0])
m4_define([xfdesktop_version_short], [xdt_version_major].[xdt_version_minor])

dnl init autoconf
AC_COPYRIGHT([Copyright (c) 2003-2024 The Xfce development team])
AC_INIT([xfdesktop], [xdt_version], [https://gitlab.xfce.org/xfce/xfdesktop])
AC_PREREQ([2.69])

dnl init automake
AM_INIT_AUTOMAKE([1.8 dist-bzip2 tar-ustar no-dist-gzip foreign subdir-objects])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([m4])
AM_MAINTAINER_MODE
AM_SILENT_RULES([yes])

dnl check for UNIX variants
AC_USE_SYSTEM_EXTENSIONS
AC_SEARCH_LIBS([strerror],[cposix])

dnl check for basic programs
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL
m4_ifdef([AC_PROG_SED], [AC_PROG_SED], [AC_CHECK_PROG(SED, sed, sed)])

dnl prepare libtool
LT_PREREQ([2.2.6])
LT_INIT([disable-static])

dnl check for standard header files
AC_CHECK_HEADERS([ctype.h errno.h fcntl.h math.h pwd.h signal.h \
                  string.h sys/param.h sys/stat.h  \
                  sys/types.h time.h \
                  unistd.h])

GETTEXT_PACKAGE="$PACKAGE"
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [Name of default gettext domain])
AC_SUBST([GETTEXT_PACKAGE])
AM_GNU_GETTEXT_VERSION([0.19.8])
AM_GNU_GETTEXT([external])

AC_DEFINE([VERSION_FULL], [VERSION], [Full version string of xfdesktop])
dnl define xfdesktop's short version (major.minor)
AC_DEFINE([VERSION_SHORT], ["xfdesktop_version_short"], [Short version string of xfdesktop])
dnl replace the version in settings/xfce-backdrop-settings.desktop.in
AC_SUBST([VERSION_VERSION], [xfdesktop_version_short])

dnl required
XDT_CHECK_PACKAGE([GIO], [gio-2.0], [glib_minimum_version])
XDT_CHECK_PACKAGE([GIO_UNIX], [gio-unix-2.0], [glib_minimum_version])
XDT_CHECK_PACKAGE([GMODULE], [gmodule-2.0], [glib_minimum_version])
XDT_CHECK_PACKAGE([GOBJECT], [gobject-2.0], [glib_minimum_version])
XDT_CHECK_PACKAGE([GTHREAD], [gthread-2.0], [glib_minimum_version])
XDT_CHECK_PACKAGE([GTK], [gtk+-3.0], [gtk_minimum_version])
XDT_CHECK_PACKAGE([LIBXFCE4UTIL], [libxfce4util-1.0],
                  [libxfce4util_minimum_version])
XDT_CHECK_PACKAGE([LIBXFCE4UI], [libxfce4ui-2],
                  [libxfce4ui_minimum_version])
XDT_CHECK_PACKAGE([LIBXFCE4KBD_PRIVATE], [libxfce4kbd-private-3],
                  [libxfce4ui_minimum_version])
XDT_CHECK_PACKAGE([LIBXFCE4WINDOWING], [libxfce4windowing-0],
                  [libxfce4windowing_minimum_version])
XDT_CHECK_PACKAGE([LIBXFCE4WINDOWINGUI], [libxfce4windowingui-0],
                  [libxfce4windowing_minimum_version])
XDT_CHECK_PACKAGE([XFCONF], [libxfconf-0], [xfconf_minimum_version])
XDT_CHECK_PACKAGE([LIBEXO], [exo-2], [exo_minimum_version])
XDT_CHECK_PACKAGE([CAIRO], [cairo], [cairo_minimum_version])

XDT_CHECK_OPTIONAL_FEATURE([X11],
                           [x11],
                           [
                               XDT_FEATURE_DEPENDENCY([LIBX11], [x11], [libx11_minimum_version])
                               XDT_FEATURE_DEPENDENCY([GDK_X11], [gdk-x11-3.0], [gtk_minimum_version])
                               XDT_FEATURE_DEPENDENCY([LIBXFCE4WINDOWING_X11], [libxfce4windowing-x11-0], [libxfce4windowing_minimum_version])
                           ],
                           [the X11 windowing system])
XDT_CHECK_OPTIONAL_FEATURE([WAYLAND],
                           [wayland],
                           [
                               XDT_FEATURE_DEPENDENCY([GDK_WAYLAND], [gdk-wayland-3.0], [gtk_minimum_version])
                               XDT_FEATURE_DEPENDENCY([GTK_LAYER_SHELL], [gtk-layer-shell-0], [gtk_layer_shell_minimum_version])
                           ],
                           [the Wayland windowing system])


if test x"$ENABLE_X11" != x"yes" -a x"$ENABLE_WAYLAND" != x"yes"; then
    AC_MSG_ERROR([Either both X11 and Wayland support was disabled, or required dependencies are missing. One of the two must be enabled.])
fi

XDT_CHECK_PACKAGE_BINARY([GDBUS_CODEGEN], [gio-2.0], [gdbus_codegen], [gdbus-codegen])
XDT_CHECK_PACKAGE_BINARY([GLIB_COMPILE_RESOURCES], [gio-2.0], [glib_compile_resources], [glib-compile-resources])
XDT_CHECK_PACKAGE_BINARY([GLIB_GENMARSHAL], [glib-2.0], [glib_genmarshal], [glib-genmarshal])
XDT_CHECK_PACKAGE_BINARY([GLIB_MKENUMS], [glib-2.0], [glib_mkenums], [glib-mkenums])

dnl do we want desktop icons at all?
AC_ARG_ENABLE([desktop-icons],
    [AS_HELP_STRING([--disable-desktop-icons],
            [Do not compile in support for desktop icons (default=enabled)])],
        [enable_desktop_icons=$enableval],
        [enable_desktop_icons=yes])
if test "x$enable_desktop_icons" = "xyes"; then
    AC_DEFINE([ENABLE_DESKTOP_ICONS], [1], [Set if desktop icon support is enabled])
fi
AM_CONDITIONAL([ENABLE_DESKTOP_ICONS], [test "x$enable_desktop_icons" = "xyes"])

dnl if we want desktop icons, do we also want file icons?
if test x"$enable_desktop_icons" != x"yes"; then
    enable_file_icons=no
fi
XDT_CHECK_OPTIONAL_FEATURE([FILE_ICONS],
                           [file-icons],
                           [
                               XDT_FEATURE_DEPENDENCY([LIBYAML], [yaml-0.1], [libyaml_minimum_version])
                           ],
                           [Whether to compile in support for desktop file icons (default=enabled)])

if test x"$ENABLE_FILE_ICONS" != x"yes" -o "x$enable_desktop_icons" != "xyes"; then
    enable_file_icons="no"
else
    enable_file_icons="yes"
fi

dnl i'd rather have these two only checked conditionally, but this macro also
dnl calls AM_CONDITIONAL(), which cannot be in an 'if' block

XDT_CHECK_OPTIONAL_PACKAGE([THUNARX], [thunarx-3], [thunar_minimum_version],
    [thunarx],
    [Thunar's extension mechanism, to add external features to the desktop icon implementation])


AC_ARG_ENABLE([desktop-menu],
        AS_HELP_STRING([--disable-desktop-menu],
                [Do not build the desktop menu module (default=enabled)]),
        [ac_cv_enable_desktop_menu=$enableval],
        [ac_cv_enable_desktop_menu=yes])
if test "x$ac_cv_enable_desktop_menu" = "xno"; then
    build_desktop_menu="no"
else
    dnl time for more abuse!
    XDT_CHECK_PACKAGE([GARCON], [garcon-1],
                      [garcon_minimum_version], [build_desktop_menu="yes"],
                      [
echo "*** Optional package garcon-1 was either not found on your system"
echo "*** or is too old.  Please install or upgrade to at least version"
echo "*** garcon_minimum_version, or adjust the PKG_CONFIG_PATH environment variable"
echo "*** if you installed the new version of the package in a nonstandard"
echo "*** prefix.  Desktop menu will be disabled."
build_desktop_menu="no"
])
    XDT_CHECK_PACKAGE([GARCON_GTK3], [garcon-gtk3-1],
                      [garcon_minimum_version], [build_desktop_menu="yes"],
                      [
echo "*** Optional package garcon-gtk3-1 was either not found on your system"
echo "*** or is too old.  Please install or upgrade to at least version"
echo "*** garcon_minimum_version, or adjust the PKG_CONFIG_PATH environment variable"
echo "*** if you installed the new version of the package in a nonstandard"
echo "*** prefix.  Desktop menu will be disabled."
build_desktop_menu="no"
])
    if test "x$build_desktop_menu" = "xyes"; then
        AC_DEFINE([ENABLE_DESKTOP_MENU], [1],
                  [Define if we should build the desktop menu module])
    fi
fi
AM_CONDITIONAL([BUILD_DESKTOP_MENU], [test "x$build_desktop_menu" = "xyes"])

AC_ARG_WITH([file-manager-fallback],
    [AS_HELP_STRING([--with-file-manager-fallback=PATH],
            [Default file manager fallback to handle external folders and applications (default=Thunar)])],
    [ac_cv_file_manager_fallback="$withval"],
    [ac_cv_file_manager_fallback=Thunar])
AC_DEFINE_UNQUOTED([FILE_MANAGER_FALLBACK], ["$ac_cv_file_manager_fallback"],
    [Set to the file manager to use as a fallback])

dnl **************************************
dnl *** Optional support for libnotify ***
dnl **************************************
XDT_CHECK_OPTIONAL_PACKAGE([LIBNOTIFY], [libnotify], [0.4.0], [notifications],
                           [Mount notification support], [yes])

AC_ARG_WITH([default-backdrop-filename],
            [AS_HELP_STRING([--with-default-backdrop-filename=PATH],
                            [Specify the path to the default desktop backdrop image (default=${datadir}/backgrounds/xfce/xfce-x.svg)])],
            [default_backdrop_filename=$withval], [default_backdrop_filename="${datadir}/backgrounds/xfce/xfce-x.svg"])
dnl can't use AC_DEFINE_UNQUOTED here as it only expands shell variables once,
dnl so ${datadir} just becomes ${datarootdir}, which of course cpp won't expand
DEFAULT_BACKDROP="$default_backdrop_filename"
AC_SUBST([DEFAULT_BACKDROP])

AC_ARG_ENABLE([tests],
              [
AS_HELP_STRING([--enable-tests], [Build some small test programs (default=enabled)])
AS_HELP_STRING([--disable-tests], [Don't build test programs (default=enabled)])
              ],
              [enable_tests=$enableval], [enable_tests=yes])
AM_CONDITIONAL([ENABLE_TESTS], [test x"$enable_tests" = x"yes"])

dnl check for debugging support
XDT_FEATURE_DEBUG([xdt_debug_default])

AC_CONFIG_FILES([
Makefile
backgrounds/Makefile
common/Makefile
doc/Makefile
pixmaps/Makefile
po/Makefile.in
settings/Makefile
src/Makefile
tests/Makefile
])
AC_OUTPUT

echo
echo "Build Configuration:"
echo "* Build desktop menu module:                    $build_desktop_menu"
echo "* Build support for desktop icons:              $enable_desktop_icons"
echo "      Include support for file/launcher icons:  $enable_file_icons"
if test x"$LIBNOTIFY_FOUND" = x"yes"; then
echo "* Mount notification support:                   yes"
else
echo "* Mount notification support:                   no"
fi
echo "* Windowing:"
echo "      X11:                                      $ENABLE_X11"
echo "      Wayland:                                  $ENABLE_WAYLAND"
echo "* Test programs:                                $enable_tests"
echo
