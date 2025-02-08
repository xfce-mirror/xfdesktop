if get_option('tests')

  test_progs = [
    'test-gradient-benchmarking',
    'test-icon-position-parsing',
    'test-icon-position-saving',
  ]

  test_exes = {}

  foreach test_prog : test_progs
    test_exe = executable(
      test_prog,
      '@0@.c'.format(test_prog),
      include_directories: [
        include_directories('../common'),
        include_directories('../src'),
      ],
      c_args: [
        '-DBACKGROUNDS_DIR="/usr/share/backgrounds/xfce"',
      ],
      dependencies: [
        gtk,
        libm,
        libyaml,
        libxfce4util,
        libxfce4windowing,
        xfconf,
      ],
      install: false,
    )
    test_exes += { test_prog: test_exe }
  endforeach

endif
