bin_PROGRAMS = xfce4-menueditor

xfce4_menueditor_SOURCES =						\
	dummy_icon.h							\
	menueditor-add-dialog.h						\
	menueditor-add-dialog.c						\
	menueditor-add-external-dialog.h				\
	menueditor-add-external-dialog.c				\
	menueditor-edit-dialog.h					\
	menueditor-edit-dialog.c					\
	menueditor-edit-external-dialog.h				\
	menueditor-edit-external-dialog.c				\
	menueditor-main.c						\
	menueditor-main-window.h					\
	menueditor-main-window.c					
	

xfce4_menueditor_CFLAGS = 						\
	-DEXO_API_SUBJECT_TO_CHANGE					\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/common						\
	-I$(top_srcdir)/pixmaps						\
	$(LIBEXO_CFLAGS)						\
	$(LIBX11_CFLAGS)						\
	$(LIBXFCEGUI4_CFLAGS)						\
	-DLOCALEDIR=\"$(localedir)\"					\
	-DDATADIR=\"$(datadir)\"					\
	-DBINDIR=\"$(bindir)\"						\
	-DSYSCONFDIR=\"$(sysconfdir)\"					\
	-DXFCEDATADIR=\"$(datadir)/xfce4\"
	

xfce4_menueditor_LDADD =						\
	$(top_builddir)/common/libxfdesktop.la				\
	$(LIBEXO_LIBS)							\
	$(LIBX11_LDFLAGS)						\
	$(LIBX11_LIBS)							\
	$(LIBXFCE4MCS_CLIENT_LIBS)					\
	$(LIBXFCEGUI4_LIBS)						

appsdir = $(datadir)/applications
apps_in_files = xfce-menueditor.desktop.in
apps_DATA = $(apps_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

uidir = $(datadir)/xfce4-menueditor
ui_DATA = xfce4-menueditor.ui

EXTRA_DIST = $(apps_in_files)						\
	     $(ui_DATA)

DISTCLEANFILES = $(apps_DATA)
