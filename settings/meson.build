settings_generated_sources = []
settings_generated_sources += gnome.compile_resources(
  'xfdesktop-settings-ui',
  'xfdesktop-settings-ui.gresource.xml',
  export: false,
  extra_args: [
    '--manual-register',
  ],
)

i18n.merge_file(
  input: 'xfce-backdrop-settings.desktop.in',
  output: 'xfce-backdrop-settings.desktop',
  type: 'desktop',
  po_dir: '../po',
  install: true,
  install_dir: get_option('prefix') / get_option('datadir') / 'applications',
)

executable(
  'xfdesktop-settings',
  [
    'main.c',
    settings_generated_sources,
  ],
  c_args: [
    '-DG_LOG_DOMAIN="xfdesktop-settings"',
  ],
  include_directories: [
    include_directories('..'),
    include_directories('../common'),
    include_directories('../src'),
  ],
  dependencies: [
    gtk,
    x11_deps,
    libxfce4util,
    libxfce4ui,
    libxfce4windowing,
    xfconf,
    exo,
  ],
  link_with: [
    libxfdesktop,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('bindir'),
)
