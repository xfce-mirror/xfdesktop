# common/Makefile.am
#

noinst_LTLIBRARIES = libxfdesktop.la

libxfdesktop_la_SOURCES =						\
	backdrop.c							\
	backdrop.h

libxfdesktop_la_CFLAGS =						\
	@LIBXFCEGUI4_CFLAGS@						\
	@GTK_CFLAGS@

libxfdesktop_la_LDFLAGS =						\
	-export-dynamic

libxfdesktop_la_LIBADD =						\
	@GTK_LIBS@
