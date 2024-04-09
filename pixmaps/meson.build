pixmaps_icon_sizes = [
  '16x16',
  '24x24',
  '32x32',
  '48x48',
  '128x128',
  'scalable',
]
foreach size : pixmaps_icon_sizes
  if size == 'scalable'
    ext = 'svg'
  else
    ext = 'png'
  endif

  install_data(
    '@0@/org.xfce.xfdesktop.@1@'.format(size, ext),
    install_dir: get_option('prefix') / get_option('datadir') / 'icons' / 'hicolor' / size / 'apps',
  )
endforeach

install_data(
  [
    'xfce4_xicon.png',
    'xfce4_xicon1.png',
    'xfce4_xicon2.png',
    'xfce4_xicon3.png',
    'xfce4_xicon4.png',
  ],
  install_dir: get_option('prefix') / get_option('datadir') / 'pixmaps',
)

install_data(
  'xfdesktop-fallback-icon.png',
  install_dir: get_option('prefix') / get_option('datadir') / 'pixmaps' / 'xfdesktop',
)
