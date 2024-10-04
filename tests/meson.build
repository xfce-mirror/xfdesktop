if get_option('tests')

  test_progs = [
    'test-icon-position-parsing',
    'test-icon-position-saving',
  ]

  foreach test_prog : test_progs
    executable(
      test_prog,
      [
        '@0@.c'.format(test_prog),
        '..' / 'src' / 'xfdesktop-icon-position-configs.c',
      ],
      include_directories: [
        include_directories('../src'),
      ],
      dependencies: [
        gtk,
        libyaml,
        libxfce4util,
        libxfce4windowing,
      ],
      install: false,
    )
  endforeach

endif
