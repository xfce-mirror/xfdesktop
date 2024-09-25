xfdesktop_generated_sources = []

dbus_codegen = {
  'xfdesktop-file-manager-fdo': 'org.freedesktop.',
  'xfdesktop-file-manager': 'org.xfce.',
  'xfdesktop-thunar': 'org.xfce.',
  'xfdesktop-trash': 'org.xfce.',
}

foreach prefix, interface : dbus_codegen
  xfdesktop_generated_sources += gnome.gdbus_codegen(
    '@0@-proxy'.format(prefix),
    sources: '@0@-dbus.xml'.format(prefix),
    interface_prefix: interface,
    namespace: 'Xfdesktop',
    install_header: false,
  )
endforeach

xfdesktop_sources = [
  'main.c',
  'menu.c',
  'windowlist.c',
  'xfce-desktop.c',
  'xfdesktop-application.c',
  'xfdesktop-backdrop-cycler.c',
  'xfdesktop-backdrop-manager.c',
  'xfdesktop-backdrop-renderer.c',
]

xfdesktop_sources += xfdesktop_generated_sources

if enable_x11
  xfdesktop_sources += 'xfdesktop-x11.c'
endif

if enable_desktop_icons
  xfdesktop_sources += [
    'xfdesktop-cell-renderer-icon-label.c',
    'xfdesktop-icon.c',
    'xfdesktop-icon-view.c',
    'xfdesktop-icon-view-holder.c',
    'xfdesktop-icon-view-manager.c',
    'xfdesktop-icon-view-model.c',
    'xfdesktop-window-icon-manager.c',
    'xfdesktop-window-icon-model.c',
  ]
endif

if enable_file_icons
  xfdesktop_sources += [
    'xfdesktop-clipboard-manager.c',
    'xfdesktop-file-icon.c',
    'xfdesktop-file-icon-manager.c',
    'xfdesktop-file-icon-model.c',
    'xfdesktop-file-icon-model-filter.c',
    'xfdesktop-file-utils.c',
    'xfdesktop-icon-position-configs.c',
    'xfdesktop-icon-position-migration.c',
    'xfdesktop-regular-file-icon.c',
    'xfdesktop-special-file-icon.c',
    'xfdesktop-volume-icon.c',
  ]

  xfdesktop_sources += gnome.compile_resources(
    'xfdesktop-monitor-chooser-ui',
    'xfdesktop-monitor-chooser-ui.gresource.xml',
    export: false,
    extra_args: [
      '--manual-register',
      ],
    )
endif

if enable_libnotify
  xfdesktop_sources += [
    'xfdesktop-notify.c',
  ]
endif

xfdesktop_sources += xfce_revision_h

executable(
  'xfdesktop',
  xfdesktop_sources,
  c_args: [
    '-DXFCEMODDIR="@0@/xfce4/modules"'.format(get_option('prefix') / get_option('libdir')),
    '-DG_LOG_DOMAIN="xfdesktop"',
  ],
  include_directories: [
    include_directories('..'),
    include_directories('../common'),
  ],
  dependencies: [
    gio_unix,
    gtk,
    libxfce4util,
    libxfce4ui,
    libxfce4windowing,
    libxfce4windowingui,
    xfconf,
    exo,
    desktop_menu_deps,
    desktop_icons_deps,
    x11_deps,
    wayland_deps,
    libm,
  ],
  link_with: [
    libxfdesktop,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('bindir'),
)
