# common/Makefile.am
#

noinst_LTLIBRARIES = libxfdesktop.la

libxfdesktop_la_SOURCES =						\
	background-common.c						\
	background-common.h						\
	desktop-menu-stub.c						\
	desktop-menu-stub.h

libxfdesktop_la_CFLAGS =						\
	@LIBXFCEGUI4_CFLAGS@						\
	@GMODULE_CFLAGS@						\
	-DXFCEMODDIR=\"$(libdir)/xfce4/modules\"
