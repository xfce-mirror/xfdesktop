# common/Makefile.am
#

noinst_LTLIBRARIES = libxfdesktop.la

libxfdesktop_la_SOURCES =						\
	backdrop.c							\
	backdrop.h

libxfdesktop_la_CFLAGS =						\
	@CFLAGS@							\
	@GLIB_CFLAGS@							\
	@LIBXFCEGUI4_CFLAGS@						\
	@PANGO_CFLAGS@							\
	-DG_DISABLE_DEPRECATED						\
	-DGTK_DISABLE_DEPRECATED					\
	-DGDK_DISABLE_DEPRECATED					\
	-DGDK_PIXBUF_DISABLE_DEPRECATED

libxfdesktop_la_LDFLAGS =						\
	-export-dynamic

