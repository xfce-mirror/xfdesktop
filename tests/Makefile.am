noinst_PROGRAMS = \
	test-icon-position-parsing \
	test-icon-position-saving

tests_cflags = \
	$(GTK_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4WINDOWING_CFLAGS) \
	$(LIBYAML_CFLAGS)

tests_ldadd = \
	$(GTK_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCE4WINDOWING_LIBS) \
	$(LIBYAML_LIBS)

test_icon_position_parsing_SOURCES = xfw-enum-monitors.c
test_icon_position_parsing_CFLAGS = $(tests_cflags)
test_icon_position_parsing_LDADD = $(tests_ldadd)

test_icon_position_saving_SOURCES = xfw-enum-windows.c
test_icon_position_saving_CFLAGS = $(tests_cflags)
test_icon_position_saving_LDADD = $(tests_ldadd)
