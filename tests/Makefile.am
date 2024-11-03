if ENABLE_TESTS

if ENABLE_FILE_ICONS

noinst_PROGRAMS = \
	test-icon-position-parsing \
	test-icon-position-saving

tests_cflags = \
	-I$(top_srcdir)/src \
	$(GTK_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4WINDOWING_CFLAGS) \
	$(LIBYAML_CFLAGS)

tests_ldadd = \
	$(GTK_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCE4WINDOWING_LIBS) \
	$(LIBYAML_LIBS)

test_icon_position_parsing_SOURCES = \
	test-icon-position-parsing.c \
	$(top_srcdir)/src/xfdesktop-icon-position-configs.c
test_icon_position_parsing_CFLAGS = $(tests_cflags)
test_icon_position_parsing_LDADD = $(tests_ldadd)

test_icon_position_saving_SOURCES = \
	test-icon-position-saving.c \
	$(top_srcdir)/src/xfdesktop-icon-position-configs.c
test_icon_position_saving_CFLAGS = $(tests_cflags)
test_icon_position_saving_LDADD = $(tests_ldadd)

endif

endif

EXTRA_DIST = \
	meson.build \
	test-icon-position-parsing.c \
	test-icon-position-saving.c
