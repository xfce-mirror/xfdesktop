bin_PROGRAMS = xfdesktop

xfdesktop_SOURCES =							\
	backdrop.c							\
	backdrop.h							\
	main.c								\
	main.h								\
	menu.c								\
	menu.h								\
	settings.c							\
	settings.h

xfdesktop_CFLAGS = 							\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/common						\
	@LIBX11_CFLAGS@							\
	@LIBXFCE4MCS_CLIENT_CFLAGS@					\
	@LIBXFCEGUI4_CFLAGS@						\
	@GMODULE_CFLAGS@							\
	-DLOCALEDIR=\"$(localedir)\"					\
	-DDATADIR=\"$(pkgdatadir)\"					\
	-DSYSCONFDIR=\"$(sysconfdir)\"			\
	-DXFCEDATADIR=\"$(datadir)/xfce4\"		\
	-DXFCEMODDIR=\"$(libdir)/xfce4/modules\"

xfdesktop_LDADD =							\
	$(top_builddir)/common/libxfdesktop.la				\
	@LIBX11_LDFLAGS@						\
	@LIBX11_LIBS@							\
	@LIBXFCE4MCS_CLIENT_LIBS@					\
	@LIBXFCEGUI4_LIBS@						\
	@GMODULE_LIBS@
