# common/Makefile.am
#

noinst_LTLIBRARIES = libxfdesktop.la

libxfdesktop_la_SOURCES =						\
	backdrop-common.c						\
	backdrop-common.h

if BUILD_DESKTOP_MENU
libxfdesktop_la_SOURCES += desktop-menu-stub.c desktop-menu-stub.h
endif
	
libxfdesktop_la_CFLAGS =						\
	@LIBXFCEGUI4_CFLAGS@						\
	@GMODULE_CFLAGS@						\
	-DXFCEMODDIR=\"$(libdir)/xfce4/modules\"
