if get_option('tests')

  test_progs = [
    'test-icon-position-parsing',
    'test-icon-position-saving',
  ]

  test_exes = {}

  foreach test_prog : test_progs
    test_exe = executable(
      test_prog,
      '@0@.c'.format(test_prog),
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
    test_exes += { test_prog: test_exe }
  endforeach

  test(
    'icon position parsing',
    test_exes['test-icon-position-parsing'],
    args: [
      meson.project_source_root() / 'tests' / 'icon-positions-test.yaml',
    ]
  )

  test(
    'icon position saving',
    test_exes['test-icon-position-saving'],
    args: [
      meson.project_build_root() / 'tests' / 'icon-postion-saving-test.yaml',
    ],
  )
endif
