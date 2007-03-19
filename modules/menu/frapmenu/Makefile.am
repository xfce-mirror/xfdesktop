# $Id: Makefile.am 25185 2007-03-18 02:23:12Z jannis $ 

INCLUDES = 								\
	-DG_LOG_DOMAIN=\"FrapMenu\"					\
	-DLIBFRAPMENU_COMPILATION					\
	$(PLATFORM_CFLAGS)

SUBDIRS = 								\
	tdb

noinst_LTLIBRARIES = 							\
	libfrapmenu.la

libfrapmenu_la_SOURCES =						\
	frap-menu.c							\
	frap-menu-separator.c						\
	frap-menu-layout.c						\
	frap-menu-directory.c						\
	frap-menu-rules.c						\
	frap-menu-standard-rules.c					\
	frap-menu-or-rules.c						\
	frap-menu-and-rules.c						\
	frap-menu-not-rules.c						\
	frap-menu-item-pool.c						\
	frap-menu-item-cache.c						\
	frap-menu-move.c						\
	frap-menu-environment.c						\
	frap-menu-item.c						

libfrapmenu_la_DEPENDENCIES = 						\
	$(top_builddir)/modules/menu/frapmenu/tdb/libtdb.la

noinst_HEADERS =							\
	libfrapmenu.h							\
	frap-menu.h							\
	frap-menu-separator.h						\
	frap-menu-layout.h						\
	frap-menu-directory.h						\
	frap-menu-rules.h						\
	frap-menu-standard-rules.h					\
	frap-menu-or-rules.h						\
	frap-menu-and-rules.h						\
	frap-menu-not-rules.h						\
	frap-menu-item-pool.h						\
	frap-menu-item-cache.h						\
	frap-menu-move.h						\
	frap-menu-environment.h						\
	frap-menu-item.h						

libfrapmenu_la_CFLAGS = 						\
	$(GLIB_CFLAGS)							\
	$(GTHREAD_CFLAGS)						\
	$(LIBXFCE4UTIL_CFLAGS)						\
	$(PLATFORM_CFLAGS)						

libfrapmenu_la_LDFLAGS =						\
	-no-undefined							\
	$(PLATFORM_LDFLAGS)
	
libfrapmenu_la_LIBADD = 						\
	$(GLIB_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(LIBXFCE4UTIL_LIBS)						\
	$(top_builddir)/modules/menu/frapmenu/tdb/libtdb.la

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
