bin_PROGRAMS = xfdesktop

xfdesktop_SOURCES =							\
	backdrop.c							\
	backdrop.h							\
	main.c								\
	main.h								\
	menu.c								\
	menu.h								\
	menu-file.c							\
	menu-file.h							\
	menu-dentry.c						\
	menu-dentry.h						\
	settings.c							\
	settings.h							\
	menuspec.c							\
	menuspec.h

xfdesktop_CFLAGS = 							\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/common						\
	@LIBX11_CFLAGS@							\
	@LIBXFCE4MCS_CLIENT_CFLAGS@					\
	@LIBXFCEGUI4_CFLAGS@						\
	@LIBXML_CFLAGS@							\
	-DLOCALEDIR=\"$(localedir)\"					\
	-DDATADIR=\"$(pkgdatadir)\"					\
	-DSYSCONFDIR=\"$(sysconfdir)\"			\
	-DXFCEDATADIR=\"$(datadir)/xfce4\"
	

xfdesktop_LDADD =							\
	$(top_builddir)/common/libxfdesktop.la				\
	@LIBX11_LDFLAGS@						\
	@LIBX11_LIBS@							\
	@LIBXFCE4MCS_CLIENT_LIBS@					\
	@LIBXFCEGUI4_LIBS@						\
	@LIBXML_LIBS@

xfcedir = $(datadir)/xfce4
xfce_DATA = xfce-registered-categories.xml
