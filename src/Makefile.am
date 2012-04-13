# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:

bin_PROGRAMS = xfdesktop

xfdesktop_built_sources = \
	xfce-desktop-enum-types.c \
	xfce-desktop-enum-types.h

if HAVE_LIBNOTIFY
xfdesktop_notify_sources = \
	xfdesktop-notify.c \
	xfdesktop-notify.h
endif

xfdesktop_SOURCES = \
	$(xfdesktop_built_sources) \
	$(xfdesktop_notify_sources) \
	main.c \
	menu.c \
	menu.h \
	windowlist.c \
	windowlist.h \
	xfce-backdrop.c \
	xfce-backdrop.h \
	xfce-desktop.c \
	xfce-desktop.h

desktop_icon_sources = \
	xfdesktop-icon.c \
	xfdesktop-icon.h \
	xfdesktop-icon-view.c \
	xfdesktop-icon-view.h \
	xfdesktop-icon-view-manager.c \
	xfdesktop-icon-view-manager.h \
	xfdesktop-window-icon.c \
	xfdesktop-window-icon.h \
	xfdesktop-window-icon-manager.c \
	xfdesktop-window-icon-manager.h

desktop_menu_sources = \
	xfce-desktop-menu.c \
	xfce-desktop-menu.h \
	xfdesktop-app-menu-item.c \
	xfdesktop-app-menu-item.h

desktop_file_icon_sources = \
	xfdesktop-clipboard-manager.c \
	xfdesktop-clipboard-manager.h \
	xfdesktop-file-icon.c \
	xfdesktop-file-icon.h \
	xfdesktop-file-icon-manager.c \
	xfdesktop-file-icon-manager.h \
	xfdesktop-file-utils.c \
	xfdesktop-file-utils.h \
	xfdesktop-regular-file-icon.c \
	xfdesktop-regular-file-icon.h \
	xfdesktop-special-file-icon.c \
	xfdesktop-special-file-icon.h \
	xfdesktop-volume-icon.c \
	xfdesktop-volume-icon.h

xfdesktop_CFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/common	\
	-DWNCK_I_KNOW_THIS_IS_UNSTABLE \
	-DBINDIR=\"$(bindir)\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DDATADIR=\"$(datadir)\" \
	-DSYSCONFDIR=\"$(sysconfdir)\" \
	-DXFCEMODDIR=\"$(libdir)/xfce4/modules\" \
        $(GIO_CFLAGS) \
	$(GIO_UNIX_CFLAGS) \
        $(GLIB_CFLAGS) \
	$(GTHREAD_CFLAGS) \
        $(GTK_CFLAGS) \
	$(LIBNOTIFY_CFLAGS) \
	$(LIBX11_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(LIBXFCE4SMCLIENT_PRIVATE_CFLAGS) \
	$(LIBWNCK_CFLAGS) \
	$(XFCONF_CFLAGS)

xfdesktop_LDFLAGS = \
	-export-dynamic

# cygwin link order requires this split
xfdesktop_LDADD = $(top_builddir)/common/libxfdesktop.la
xfdesktop_LDADD += \
        $(GIO_LIBS) \
	$(GIO_UNIX_LIBS) \
        $(GLIB_LIBS) \
	$(GTHREAD_LIBS) \
        $(GTK_LIBS) \
	$(LIBNOTIFY_LIBS) \
	$(LIBX11_LDFLAGS) \
	$(LIBX11_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(LIBXFCE4SMCLIENT_PRIVATE_LIBS) \
	$(LIBWNCK_LIBS) \
	$(XFCONF_LIBS)

if BUILD_DESKTOP_MENU

xfdesktop_SOURCES += $(desktop_menu_sources)

xfdesktop_CFLAGS += \
	$(GARCON_CFLAGS)

xfdesktop_LDADD += \
	$(GARCON_LIBS)

endif

if ENABLE_DESKTOP_ICONS

xfdesktop_SOURCES += $(desktop_icon_sources)

xfdesktop_CFLAGS += \
	-DEXO_API_SUBJECT_TO_CHANGE \
	$(LIBEXO_CFLAGS)

xfdesktop_LDADD += \
	$(LIBEXO_LIBS)

if ENABLE_FILE_ICONS

xfdesktop_SOURCES += $(desktop_file_icon_sources)

xfdesktop_CFLAGS += \
	-DDBUS_API_SUBJECT_TO_CHANGE \
	$(THUNARX_CFLAGS) \
	$(DBUS_CFLAGS)

xfdesktop_LDADD += \
	$(THUNARX_LIBS) \
	$(DBUS_LIBS)

endif
endif

if MAINTAINER_MODE

BUILT_SOURCES = \
	$(xfdesktop_built_sources)

DISTCLEANFILES = \
	$(xfdesktop_built_sources) \
	stamp-xfce-desktop-enum-types.h


enum_headers = \
	xfce-backdrop.h \
	xfce-desktop.h

xfce-desktop-enum-types.h: stamp-xfce-desktop-enum-types.h
	@true
stamp-xfce-desktop-enum-types.h: $(enum_headers) Makefile
	$(AM_V_GEN) ( cd $(srcdir) && glib-mkenums \
	      --fhead "#ifndef __XFCE_DESKTOP_ENUM_TYPES_H__\n#define __XFCE_DESKTOP_ENUM_TYPES_H__\nG_BEGIN_DECLS\n" \
	      --fprod "/* enumerations from \"@filename@\" */\n" \
	      --vhead "GType @enum_name@_get_type (void) G_GNUC_CONST;\n#define XFCE_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" \
	      --ftail "G_END_DECLS\n\n#endif /* !__XFCE_DESKTOP_ENUM_TYPES_H__ */" \
	      $(enum_headers) ) >> xgen-xdeth \
	      && ( cmp -s xgen-xdeth xfce-desktop-enum-types.h || cp xgen-xdeth xfce-desktop-enum-types.h ) \
	      && rm -f xgen-xdeth \
		  && echo timestamp > $(@F)
xfce-desktop-enum-types.c: $(enum_headers) Makefile
	$(AM_V_GEN) ( cd $(srcdir) && glib-mkenums \
	      --fhead "#undef GTK_DISABLE_DEPRECATED\n#define GTK_ENABLE_BROKEN\n#include \"xfce-desktop.h\"\n#include \"xfce-backdrop.h\"\n#include \"xfce-desktop-enum-types.h\"" \
	      --fprod "\n/* enumerations from \"@filename@\" */" \
	      --vhead "GType\n@enum_name@_get_type (void)\n{\n\tstatic GType type = 0;\n\tif (type == 0) {\n\tstatic const G@Type@Value values[] = {"\
	      --vprod "\t{ @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
	      --vtail "\t{ 0, NULL, NULL }\n\t};\n\ttype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n\treturn type;\n}\n" \
	      $(enum_headers) ) >> xgen-xdetc \
	      && cp xgen-xdetc xfce-desktop-enum-types.c \
	      && rm -f xgen-xdetc


if ENABLE_FILE_ICONS

xfdesktop_built_sources += \
	xfdesktop-file-manager-proxy.h \
	xfdesktop-thunar-proxy.h \
	xfdesktop-trash-proxy.h

xfdesktop-file-manager-proxy.h: $(srcdir)/xfdesktop-file-manager-dbus.xml Makefile
	$(AM_V_GEN) ( \
		dbus-binding-tool --mode=glib-client \
			$(srcdir)/xfdesktop-file-manager-dbus.xml > xfdesktop-file-manager-proxy.h \
		&& $(SED) -i -e 's/org_xfce_FileManager/xfdesktop_file_manager_proxy/g' \
			xfdesktop-file-manager-proxy.h \
	)
			
xfdesktop-thunar-proxy.h: $(srcdir)/xfdesktop-thunar-dbus.xml Makefile
	$(AM_V_GEN) ( \
		dbus-binding-tool --mode=glib-client \
			$(srcdir)/xfdesktop-thunar-dbus.xml > xfdesktop-thunar-proxy.h \
		&& $(SED) -i -e 's/org_xfce_Thunar/xfdesktop_thunar_proxy/g' \
			xfdesktop-thunar-proxy.h \
	)

xfdesktop-trash-proxy.h: $(srcdir)/xfdesktop-trash-dbus.xml Makefile
	$(AM_V_GEN) ( \
		dbus-binding-tool --mode=glib-client \
			$(srcdir)/xfdesktop-trash-dbus.xml > xfdesktop-trash-proxy.h \
		&& $(SED) -i -e 's/org_xfce_Trash/xfdesktop_trash_proxy/g' \
			xfdesktop-trash-proxy.h \
	)

endif

endif

EXTRA_DIST = \
	$(desktop_menu_sources) \
	$(desktop_icon_sources) \
	$(desktop_file_icon_sources)
