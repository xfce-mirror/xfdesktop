backdropdir = $(libdir)/xfce-mcs-manager/plugins/
#menudir = $(libdir)/xfce-mcs-manager/plugins/
workspacesdir = $(libdir)/xfce-mcs-manager/plugins/

EXTRA_DIST = backdrop.png workspaces.png

# backdrop
backdrop_LTLIBRARIES = backdrop_settings.la
backdrop_settings_la_LDFLAGS =  -module -avoid-version

backdrop_settings_la_SOURCES = backdrop-icon.h debug.h \
backdrop_settings.c backdrop_settings.h \
backdrop-mgr.c backdrop-mgr.h \
settings_common.c settings_common.h

backdrop_settings_la_CFLAGS = -Wall @CFLAGS@ @X_CFLAGS@ @BASE_DEPENDENCIES_CFLAGS@ @MCS_MANAGER_CFLAGS@ -DG_DISABLE_DEPRECATED -DSYSCONFDIR=\"$(sysconfdir)\" -DDATADIR=\"$(datadir)\" 
backdrop_settings_la_LIBADD = @LIBS@ @X_LIBS@  @BASE_DEPENDENCIES_LIBS@ @MCS_MANAGER_LIBS@

# menu
#menu_LTLIBRARIES = menu_settings.la
#menu_settings_la_LDFLAGS =  -module -avoid-version

#menu_settings_la_SOURCES =  debug.h \
#menu-icon.h menu_settings.c menu_settings.h \
#settings_common.c settings_common.h

#menu_settings_la_CFLAGS = -Wall @CFLAGS@ @X_CFLAGS@ @BASE_DEPENDENCIES_CFLAGS@ @MCS_MANAGER_CFLAGS@ -DG_DISABLE_DEPRECATED -DSYSCONFDIR=\"$(sysconfdir)\" -DDATADIR=\"$(datadir)\" 
#menu_settings_la_LIBADD = @LIBS@ @X_LIBS@  @BASE_DEPENDENCIES_LIBS@ @MCS_MANAGER_LIBS@

# workspaces
workspaces_LTLIBRARIES = workspaces_settings.la
workspaces_settings_la_LDFLAGS =  -module -avoid-version

workspaces_settings_la_SOURCES =  debug.h \
workspaces-icon.h workspaces_settings.c workspaces_settings.h \
settings_common.c settings_common.h

workspaces_settings_la_CFLAGS = -Wall @CFLAGS@ @X_CFLAGS@ @BASE_DEPENDENCIES_CFLAGS@ @MCS_MANAGER_CFLAGS@ -DG_DISABLE_DEPRECATED -DSYSCONFDIR=\"$(sysconfdir)\" -DDATADIR=\"$(datadir)\" 
workspaces_settings_la_LIBADD = @LIBS@ @X_LIBS@  @BASE_DEPENDENCIES_LIBS@ @MCS_MANAGER_LIBS@

# icons (left out: menu-icon.h)
noinst_DATA = backdrop-icon.h workspaces-icon.h 

BACKDROP_IMAGE=backdrop.png
BACKDROP_ARG=backdrop_icon_data $(srcdir)/$(BACKDROP_IMAGE)

backdrop-icon.h: $(BACKDROP_IMAGE)
	$(GDK_PIXBUF_CSOURCE) --raw --build-list $(BACKDROP_ARG) > $(srcdir)/backdrop-icon.h

#MENU_IMAGE=menu.png
#MENU_ARG=menu_icon_data $(srcdir)/$(MENU_IMAGE)

#menu-icon.h: $(MENU_IMAGE)
#	$(GDK_PIXBUF_CSOURCE) --raw --build-list $(MENU_ARG) > $(srcdir)/menu-icon.h

WORKSPACES_IMAGE=workspaces.png
WORKSPACES_ARG=workspaces_icon_data $(srcdir)/$(WORKSPACES_IMAGE)

workspaces-icon.h: $(WORKSPACES_IMAGE)
	$(GDK_PIXBUF_CSOURCE) --raw --build-list $(WORKSPACES_ARG) > $(srcdir)/workspaces-icon.h


