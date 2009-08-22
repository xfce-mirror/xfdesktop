icons32dir = $(datadir)/icons/hicolor/32x32/apps

icons32_DATA = \
	32x32/xfce4-backdrop.png		\
	32x32/xfce4-menueditor.png

icons48dir = $(datadir)/icons/hicolor/48x48/apps

icons48_DATA = \
	48x48/xfce4-backdrop.png		\
	48x48/xfce4-menueditor.png

iconsscdir = $(datadir)/icons/hicolor/scalable/apps

iconssc_DATA = \
	scalable/xfce4-backdrop.svg		\
	scalable/xfce4-menueditor.svg

pixmapsdir = $(datadir)/pixmaps

pixmaps_DATA = \
	xfce4_xicon.png \
	xfce4_xicon1.png \
	xfce4_xicon2.png \
	xfce4_xicon3.png \
	xfce4_xicon4.png

pixmapsxdir = $(datadir)/pixmaps/xfdesktop

pixmapsx_DATA = \
	xfdesktop-fallback-icon.png

EXTRA_DIST = \
	$(icons32_DATA) \
	$(icons48_DATA) \
	$(iconssc_DATA) \
	$(pixmaps_DATA) \
	$(pixmapsx_DATA)

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

install-data-hook:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  Remember to run:"; \
		echo "***"; \
		echo "***   $(gtk_update_icon_cache)"; \
		echo "***"; \
	fi
