bin_PROGRAMS = xfdesktop

xfdesktop_SOURCES =							\
	backdrop.c							\
	main.c								\
	main.h								\
	menu.c								\
	menu.h								\
	settings.c							\
	settings.h							\
	workspaces.c							\
	workspaces.h

xfdesktop_CFLAGS = 							\
	-I$(top_srcdir)/common						\
	@CFLAGS@							\
	@GTK_CFLAGS@							\
	@LIBX11_CFLAGS@							\
	@LIBXFCE4MCS_CLIENT_CFLAGS@					\
	@LIBXFCE4UTIL_CFLAGS@						\
	@LIBXFCEGUI4_CFLAGS@						\
	@LIBXML_CFLAGS@							\
	-DDATADIR=\"$(pkgdatadir)\"					\
	-DSYSCONFDIR=\"$(sysconfdir)\"					\
	-DVERSION=\"@VERSION@\"

xfdesktop_LDADD =							\
	$(top_srcdir)/common/libxfdesktop.la				\
	@LIBS@								\
	@GTK_LIBS@							\
	@LIBX11_LDFLAGS@						\
	@LIBX11_LIBS@							\
	@LIBXFCE4MCS_CLIENT_LIBS@					\
	@LIBXFCE4UTIL_LIBS@						\
	@LIBXFCEGUI4_LIBS@						\
	@LIBXML_LIBS@
