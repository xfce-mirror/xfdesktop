project(
  'xfdesktop',
  'c',
version : '4.20.0',
  license : 'GPL-2',
  meson_version : '>= 0.54.0',
  default_options : ['c_std=gnu11', 'buildtype=debugoptimized', 'warning_level=2', 'b_ndebug=if-release']
)

project_namespace = 'xfce'
pkgdatadir = get_option('datadir') / meson.project_name()

cc = meson.get_compiler('c')
pkgconfig = import('pkgconfig')
gnome = import('gnome')
i18n = import('i18n')

dependency_versions = {
  'glib': '>= 2.72.0',
  'gtk': '>= 3.24.0',
  'libnotify': '>= 0.4.0',
  'libx11': '>= 1.6.7',
  'libxfce4windowing': '>= 4.19.8',
  'libyaml': '>= 0.2.5',
  'xfce4': '>= 4.18.0',
  'xfconf': '>= 4.19.3',
  'gtk-layer-shell': '>= 0.7.0',
}

# Required
glib = dependency('glib-2.0', version: dependency_versions['glib'])
gio_unix = dependency('gio-unix-2.0', version: dependency_versions['glib'])
gtk = dependency('gtk+-3.0', version: dependency_versions['gtk'])
libxfce4util = dependency('libxfce4util-1.0', version: dependency_versions['xfce4'])
libxfce4ui = dependency('libxfce4ui-2', version: dependency_versions['xfce4'])
libxfce4kbd_private = dependency('libxfce4kbd-private-3', version: dependency_versions['xfce4'])
libxfce4windowing = dependency('libxfce4windowing-0', version: dependency_versions['libxfce4windowing'])
libxfce4windowingui = dependency('libxfce4windowingui-0', version: dependency_versions['libxfce4windowing'])
xfconf = dependency('libxfconf-0', version: dependency_versions['xfconf'])
exo = dependency('exo-2', version: dependency_versions['xfce4'])
libm = cc.find_library('m', required: false)

# Feature desktop-menu
garcon = dependency('garcon-1', version: dependency_versions['xfce4'], required: get_option('desktop-menu'))
garcon_gtk3 = dependency('garcon-gtk3-1', version: dependency_versions['xfce4'], required: get_option('desktop-menu'))

# Feature thunarx
thunarx = dependency('thunarx-3', version: dependency_versions['xfce4'], required: get_option('thunarx'))

# Feature libnotify
libnotify = dependency('libnotify', version: dependency_versions['libnotify'], required: get_option('notifications'))

# Feature x11
libx11 = dependency('x11', version: dependency_versions['libx11'], required: get_option('x11'))
gdk_x11 = dependency('gdk-x11-3.0', version: dependency_versions['gtk'], required: get_option('x11'))
libxfce4windowing_x11 = dependency('libxfce4windowing-x11-0', version: dependency_versions['libxfce4windowing'], required: get_option('x11'))

# Feature wayland
gdk_wayland = dependency('gdk-wayland-3.0', version: dependency_versions['gtk'], required: get_option('wayland'))
gtk_layer_shell = dependency('gtk-layer-shell-0', version: dependency_versions['gtk-layer-shell'], required: get_option('wayland'))

# Feature file-icons
libyaml = dependency('yaml-0.1', version: dependency_versions['libyaml'], required: get_option('desktop-icons') and get_option('file-icons'))

feature_cflags = []

enable_desktop_menu = garcon.found() and garcon_gtk3.found()
desktop_menu_deps = []
if enable_desktop_menu
  feature_cflags += '-DENABLE_DESKTOP_MENU=1'
  desktop_menu_deps += [garcon, garcon_gtk3]
endif

enable_desktop_icons = get_option('desktop-icons')
enable_file_icons = enable_desktop_icons and get_option('file-icons')
enable_thunarx = enable_file_icons and thunarx.found()
enable_libnotify = enable_file_icons and libnotify.found()
desktop_icons_deps = []
if enable_desktop_icons
  feature_cflags += '-DENABLE_DESKTOP_ICONS=1'
  if enable_file_icons
    feature_cflags += '-DENABLE_FILE_ICONS=1'
    desktop_icons_deps += libyaml
    if enable_thunarx
      feature_cflags += '-DHAVE_THUNARX=1'
      desktop_icons_deps += [thunarx]
    endif
    if enable_libnotify
      feature_cflags += '-DHAVE_LIBNOTIFY=1'
      desktop_icons_deps += libnotify
    endif
  endif
endif

enable_x11 = libx11.found() and gdk_x11.found() and libxfce4windowing_x11.found()
x11_deps = []
if enable_x11
  feature_cflags += '-DENABLE_X11=1'
  x11_deps += [libx11, gdk_x11]
endif

enable_wayland = gdk_wayland.found() and gtk_layer_shell.found()
wayland_deps = []
if enable_wayland
  feature_cflags += '-DENABLE_WAYLAND=1'
  wayland_deps += [gdk_wayland, gtk_layer_shell]
endif

file_manager_fallback = get_option('file-manager-fallback')
feature_cflags += '-DFILE_MANAGER_FALLBACK="@0@"'.format(file_manager_fallback)

default_backdrop_filename = get_option('default-backdrop-filename')
if not default_backdrop_filename.startswith('/')
  default_backdrop_filename = get_option('prefix') / get_option('datadir') / default_backdrop_filename
endif
feature_cflags += '-DDEFAULT_BACKDROP="@0@"'.format(default_backdrop_filename)

headers = [
  'errno.h',
  'fcntl.h',
  'math.h',
  'memory.h',
  'pwd.h',
  'stdlib.h',
  'string.h',
  'sys/param.h',
  'sys/stat.h',
  'sys/types.h',
  'time.h',
  'unistd.h',
]
foreach header : headers
  if cc.check_header(header)
    feature_cflags += '-DHAVE_@0@=1'.format(header.underscorify().to_upper())
  endif
endforeach

extra_cflags = []
extra_cflags_check = [
  '-Wmissing-declarations',
  '-Wmissing-noreturn',
  '-Wold-style-definition',
  '-Wredundant-decls',
  '-Wpointer-arith',
  '-Wcast-align',
  '-Winit-self',
  '-Wshadow',
  '-Wmissing-include-dirs',
  '-Wundef',
  '-Wformat',
  '-Wformat-security',
  '-Wformat-y2k',
  '-Wnested-externs',
  '-Wno-unused-parameter',
  '-Wno-declaration-after-statement',
  '-Werror=implicit-function-declaration',
  '-Wno-error=deprecated-declarations',
]

buildtype = get_option('buildtype')
if buildtype == 'debug' or buildtype == 'debugoptimized'
  extra_cflags_check += '-fstack-protector-strong'
  extra_cflags += '-DDEBUG=1'

  if buildtype == 'debug'
    extra_cflags += [
      '-DG_ENABLE_DEBUG',
      '-DDEBUG_TRACE=1',
    ]
  endif
elif buildtype == 'release' or buildtype == 'minsize' or buildtype == 'plain'
  extra_cflags += '-DNDEBUG'

  if buildtype == 'minsize'
    extra_cflags += [
      '-DG_DISABLE_CAST_CHECKS',
      '-DG_DISABLE_ASSERT',
    ]
  endif
endif

if dependency_versions.has_key('glib')
  glib_version_parts = dependency_versions['glib'].split(' ')
  glib_min_version_parts = glib_version_parts[1].split('.')
  glib_min_version_define = 'GLIB_VERSION_@0@_@1@'.format(glib_min_version_parts[0], glib_min_version_parts[1])
  extra_cflags += [
    '-DGLIB_VERSION_MIN_REQUIRED=@0@'.format(glib_min_version_define),
    '-DGLIB_VERSION_MAX_ALLOWED=@0@'.format(glib_min_version_define),
  ]
endif

version_parts = meson.project_version().split('-dev')[0].split('.')
version_short = '@0@.@1@'.format(version_parts[0], version_parts[1])

extra_cflags += [
  '-DPACKAGE="@0@"'.format(meson.project_name()),
  '-DPACKAGE_NAME="@0@"'.format(meson.project_name()),
  '-DPACKAGE_VERSION="@0@"'.format(meson.project_version()),
  '-DVERSION="@0@"'.format(meson.project_version()),
  '-DVERSION_SHORT="@0@"'.format(version_short),
  '-DPACKAGE_STRING="@0@ @1@"'.format(meson.project_name(), meson.project_version()),
  '-DPACKAGE_DATADIR="@0@"'.format(pkgdatadir),
  '-DPACKAGE_LOCALE_DIR="@0@"'.format(get_option('prefix') / get_option('localedir')),
  '-DPACKAGE_BUGREPORT="https://gitlab.xfce.org/@0@/@1@/-/issues"'.format(project_namespace, meson.project_name()),
  '-DGETTEXT_PACKAGE="@0@"'.format(meson.project_name()),
  '-DPREFIX="@0@"'.format(get_option('prefix')),
  '-DBINDIR="@0@"'.format(get_option('prefix') / get_option('bindir')),
  '-DDATADIR="@0@"'.format(get_option('prefix') / get_option('datadir')),
  '-DINCLUDEDIR="@0@"'.format(get_option('prefix') / get_option('includedir')),
  '-DLIBDIR="@0@"'.format(get_option('prefix') / get_option('libdir')),
  '-DLIBEXECDIR="@0@"'.format(get_option('prefix') / get_option('libexecdir')),
  '-DLOCALEDIR="@0@"'.format(get_option('prefix') / get_option('localedir')),
  '-DLOCALSTATEDIR="@0@"'.format(get_option('prefix') / get_option('localstatedir')),
  '-DSBINDIR="@0@"'.format(get_option('prefix') / get_option('sbindir')),
  '-DSYSCONFDIR="@0@"'.format(get_option('prefix') / get_option('sysconfdir')),
  '-DHAVE_XFCE_REVISION_H=1',
]

add_project_arguments(cc.get_supported_arguments(extra_cflags_check), language: 'c')
add_project_arguments(extra_cflags, language: 'c')
add_project_arguments(feature_cflags, language: 'c')

xfce_revision_h = vcs_tag(
  command: ['git', 'rev-parse', '--short', 'HEAD'],
  fallback: 'UNKNOWN',
  input: 'xfce-revision.h.in',
  output: 'xfce-revision.h',
  replace_string: '@REVISION@',
)

install_man('xfdesktop.1')

subdir('backgrounds')
subdir('common')
subdir('settings')
subdir('src')
subdir('po')
subdir('pixmaps')
subdir('tests')
