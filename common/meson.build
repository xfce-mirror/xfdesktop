common_generated_sources = []

common_generated_sources += gnome.genmarshal(
  'xfdesktop-marshal',
  sources: 'xfdesktop-marshal.list',
  prefix: 'xfdesktop_marshal',
  internal: true,
  install_header: false,
)

common_generated_sources += gnome.gdbus_codegen(
  'tumbler',
  sources: 'tumbler-service-dbus.xml',
  interface_prefix: 'org.freedesktop.thumbnails.',
  namespace: 'Tumbler',
  install_header: false,
)

libxfdesktop = static_library(
  'libxfdesktop',
  [
    'xfdesktop-common.c',
    'xfdesktop-common.h',
    'xfdesktop-extensions.c',
    'xfdesktop-extensions.h',
    'xfdesktop-thumbnailer.c',
    'xfdesktop-thumbnailer.h',
  ] + common_generated_sources,
  include_directories: [
    include_directories('../src'),
  ],
  dependencies: [
    gio_unix,
    gtk,
    libxfce4util,
    libxfce4windowing,
    xfconf,
  ],
  install: false,
)
