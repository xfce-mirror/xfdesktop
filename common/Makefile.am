# common/Makefile.am
#

noinst_LTLIBRARIES = libxfdesktop.la

libxfdesktop_la_SOURCES =						\
	background-common.c						\
	background-common.h

if BUILD_DESKTOP_MENU_STUB
libxfdesktop_la_SOURCES += desktop-menu-stub.c desktop-menu-stub.h
endif
	
libxfdesktop_la_CFLAGS =						\
	@LIBXFCEGUI4_CFLAGS@						\
	@GMODULE_CFLAGS@						\
	-DXFCEMODDIR=\"$(libdir)/xfce4/modules\"
