bin_PROGRAMS =  xfdesktop

xfdesktop_SOURCES = \
backdrop.c \
backdrop.h \
main.c \
main.h \
margins.c \
margins.h \
menu.c \
menu.h \
settings.c \
settings.h

xfdesktop_CFLAGS = -Wall -I$(top_srcdir) @CFLAGS@ @X_CFLAGS@ @BASE_DEPENDENCIES_CFLAGS@ @MCS_CLIENT_CFLAGS@ -DG_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED -DDATADIR=\"$(pkgdatadir)\" -DSYSCONFDIR=\"$(sysconfdir)\" -DVERSION=\"@VERSION@\"  -I..
xfdesktop_LDADD = @LIBS@ @X_LIBS@  @BASE_DEPENDENCIES_LIBS@ @MCS_CLIENT_LIBS@


