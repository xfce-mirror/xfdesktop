bin_PROGRAMS = xfce4-menueditor

xfce4_menueditor_SOURCES =					\
	about_dialog.c							\
	about_dialog.h							\
	add_dialog.c							\
	add_dialog.h							\
	add_menu_dialog.c						\
	add_menu_dialog.h						\
	dnd.c									\
	dnd.h									\
	edit_dialog.c							\
	edit_dialog.h							\
	menueditor.c							\
	menueditor.h							\
	move.c									\
	move.h									\
	new_dialog.c									\
	new_dialog.h							
	

xfce4_menueditor_CFLAGS = 						\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/common						\
	-I$(top_srcdir)/pixmaps						\
	@LIBX11_CFLAGS@							\
	@LIBXFCE4MCS_CLIENT_CFLAGS@					\
	@LIBXFCEGUI4_CFLAGS@						\
	@LIBXML_CFLAGS@							\
	-DLOCALEDIR=\"$(localedir)\"					\
	-DDATADIR=\"$(pkgdatadir)\"					\
	-DSYSCONFDIR=\"$(sysconfdir)\"			\
	-DXFCEDATADIR=\"$(datadir)/xfce4\"
	

xfce4_menueditor_LDADD =							\
	$(top_builddir)/common/libxfdesktop.la				\
	@LIBX11_LDFLAGS@						\
	@LIBX11_LIBS@							\
	@LIBXFCE4MCS_CLIENT_LIBS@					\
	@LIBXFCEGUI4_LIBS@						\
	@LIBXML_LIBS@

appsdir = $(datadir)/applications
apps_DATA = xfce-menueditor.desktop

EXTRA_DIST = xfce-menueditor.desktop
