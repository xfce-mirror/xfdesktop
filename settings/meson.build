settings_generated_sources = []
settings_generated_sources += gnome.compile_resources(
  'xfdesktop-settings-ui',
  'xfdesktop-settings-ui.gresource.xml',
  export: false,
  extra_args: [
    '--manual-register',
  ],
)

settings_desktop_file_tmp = configure_file(
  input: 'xfce-backdrop-settings.desktop.in',
  output: 'xfce-backdrop-settings.desktop.in.tmp',
  configuration: {
    'VERSION_SHORT': version_short,
  },
  install: false,
)

i18n.merge_file(
  input: settings_desktop_file_tmp,
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
    'xfdesktop-background-settings.c',
    'xfdesktop-file-icon-settings.c',
    'xfdesktop-icon-settings.c',
    'xfdesktop-keyboard-shortcuts-settings.c',
    'xfdesktop-menu-settings.c',
    settings_generated_sources,
    xfce_revision_h,
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
    libxfce4kbd_private,
    libxfce4windowing,
    xfconf,
  ],
  link_with: [
    libxfdesktop,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('bindir'),
)
