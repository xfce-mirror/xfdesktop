/*
 *  menu-icon.[ch] - routines for locating themable menu icons
 *
 *  Copyright (C) 2004 Danny Milosavljevic <danny.milo@gmx.net>
 *                2004 Brian Tarricone <bjt23@cornell.edu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include <glib.h>
#include <gtk/gtk.h>

#include "menu-icon.h"
#include "dummy.h"

extern char icon_theme[128];
GdkPixbuf *dummy_icon = NULL;

/* nb: when adding paths here, make sure they have a trailing '/' */
static const gchar *pix_paths[] = {
	XFCEDATADIR "/themes/%s/",  /* for xfce4 theme-specific path */
	"/usr/share/icons/%s/scalable/apps/",  /* ditto */
	"/usr/share/icons/%s/48x48/apps/",  /* ditto */
	"/usr/share/icons/%s/32x32/apps/",  /* ditto */
	"/usr/share/pixmaps/",
	"/usr/share/icons/hicolor/scalable/apps/",
	"/usr/share/icons/hicolor/48x48/apps/",
	"/usr/share/icons/hicolor/32x32/apps/",
	"/usr/share/icons/gnome/scalable/apps/",  /* gnome's default */
	"/usr/share/icons/gnome/48x48/apps/",  /* ditto */
	"/usr/share/icons/gnome/32x32/apps/",  /* ditto */
	"/usr/share/icons/default.kde/scalable/apps/",  /* kde's default */
	"/usr/share/icons/default.kde/48x48/apps/",  /* ditto */
	"/usr/share/icons/default.kde/32x32/apps/",  /* ditto */
	"/usr/share/icons/locolor/scalable/apps/",  /* fallbacks */
	"/usr/share/icons/locolor/48x48/apps/",
	"/usr/share/icons/locolor/32x32/apps/",
	"/usr/share/icons/",  /* broken, but some apps do it anyway */
	NULL
};

static const gchar *user_paths[] = {
	"%s/.icons/%s",
	"%s/.icons/hicolor/scalable/apps/%s",
	"%s/.icons/hicolor/48x48/apps/%s",
	"%s/.icons/hicolor/32x32/apps/%s",
	"%s/.pixmaps/%s",
	NULL
};

#ifdef HAVE_GETENV
static const gchar *kdefmts[] = {
	"%s/share/icons/default.kde/scalable/apps/%s",
	"%s/share/icons/default.kde/48x48/apps/%s",
	"%s/share/icons/default.kde/32x32/apps/%s",
	"%s/share/icons/hicolor/scalable/apps/%s",
	"%s/share/icons/hicolor/48x48/apps/%s",
	"%s/share/icons/hicolor/32x32/apps/%s",
	NULL
};
#endif

static const gchar *pix_ext[] = {
#ifdef USE_LEAKY_SVG
	".svgz",
	".svg",
#endif
	".png",
	".xpm",
	NULL
};

static gint
get_menuitem_height()
{
	GtkWidget *tmp;
	GtkStyle *style;
	PangoFontDescription *pfdesc;
	gint totheight;
	
	tmp = gtk_label_new("foo");
	gtk_widget_set_name(tmp, "xfdesktopmenu");
	gtk_widget_show(tmp);
	style = gtk_rc_get_style(tmp);
	pfdesc = style->font_desc;
	totheight = PANGO_PIXELS(pango_font_description_get_size(pfdesc));
	totheight += 7;  /* FIXME: fudge factor */
	gtk_widget_destroy(tmp);

	return totheight;
}

static gboolean
menu_icon_find_prefix(const gchar *prefix, const gchar **dirs,
		const gchar *filename, gchar *icon_path)
{
	gint i, j;
	
	if(!prefix || !dirs || !filename || !icon_path)
		return FALSE;
	
	for(i=0; dirs[i]; i++) {
		g_snprintf(icon_path, PATH_MAX, dirs[i], prefix, filename);
		if(g_strrstr(icon_path, ".") <= g_strrstr(icon_path, "/")) {
			int len = strlen(icon_path);
			for(j=0; pix_ext[j]; j++) {
				icon_path[len] = 0;
				g_strlcat(icon_path, pix_ext[j], PATH_MAX);
				if(g_file_test(icon_path, G_FILE_TEST_EXISTS))
					return TRUE;
			}
		} else {
			if(g_file_test(icon_path, G_FILE_TEST_EXISTS))
				return TRUE;
		}
	}
	
	return FALSE;
}

GdkPixbuf *
menu_icon_find(const gchar *filename)
{
	static guchar icon_sizes[] = { 12, 16, 24, 32, 48, 72, 96, 0 };
	static gshort icon_size = -1;
	gushort mi_height;
    gboolean found = FALSE;
    GdkPixbuf *miicon = NULL;
    gint i, j;
    gint w, h;
    gchar icon_path[PATH_MAX];
#ifdef HAVE_GETENV
    const char *kdedir = getenv("KDEDIR");
#endif

#ifndef USE_LEAKY_SVG
	if(g_str_has_suffix(filename, ".svg") || g_str_has_suffix(filename, ".svgz"))
		return dummy_icon;
#endif

	if(icon_size == -1) {
		/* figure out an ideal icon size */
		mi_height = get_menuitem_height();
		for(i=0; icon_sizes[i]; i++) {
			if(icon_sizes[i] < mi_height)
				icon_size = icon_sizes[i];
			else
				break;
		}
	}
	
	/* size the dummy icon so we maintain proper menuitem size */
	if(!dummy_icon) {
		GdkPixbuf *tmpicon = gdk_pixbuf_new_from_inline(-1, my_pixbuf, FALSE, NULL);
		dummy_icon = gdk_pixbuf_scale_simple(tmpicon, icon_size,
				icon_size, GDK_INTERP_BILINEAR);
		g_object_unref(G_OBJECT(tmpicon));
	}
	
	if(!filename)
		return dummy_icon;
	
	if(*filename == '/') {
		g_strlcpy(icon_path, filename, PATH_MAX);
		if(g_file_test(icon_path, G_FILE_TEST_EXISTS))
			found = TRUE;
	} else {
		for(i=0; pix_paths[i] && !found; i++) {
			if(strstr(pix_paths[i], "%s")) {
				if(!icon_theme[0])
					continue;
				g_snprintf(icon_path, PATH_MAX, pix_paths[i], icon_theme);
			} else
				g_strlcpy(icon_path, pix_paths[i], PATH_MAX);
			g_strlcat(icon_path, filename, PATH_MAX);
			if(g_strrstr(icon_path, ".") <= g_strrstr(icon_path, "/")) {
				int len = strlen(icon_path);
				for(j=0; pix_ext[j] && !found; j++) {
					icon_path[len] = 0;
					g_strlcat(icon_path, pix_ext[j], PATH_MAX);
					if(g_file_test(icon_path, G_FILE_TEST_EXISTS))
						found = TRUE;
				}
			} else {
				if(g_file_test(icon_path, G_FILE_TEST_EXISTS))
					found = TRUE;
			}
		}
#ifdef HAVE_GETENV
		if(!found && kdedir && *kdedir=='/' && strcmp(kdedir, "/usr"))
			found = menu_icon_find_prefix(kdedir, kdefmts, filename, icon_path);
#endif
		if(!found)
			found = menu_icon_find_prefix((const gchar *)xfce_get_homedir(),
					user_paths,	filename, icon_path);
	}
	if (found) {
		miicon = gdk_pixbuf_new_from_file (icon_path, NULL);
		
		if (miicon) {
			w = gdk_pixbuf_get_width (miicon);
			h = gdk_pixbuf_get_height (miicon);
			if (w != icon_size || h != icon_size) {
				GdkPixbuf *tmp;
				tmp = gdk_pixbuf_scale_simple (miicon, icon_size, icon_size,
						GDK_INTERP_BILINEAR);
				g_object_unref (G_OBJECT (miicon));
				miicon = tmp;
			}
		}
	}
    
    return (found ? miicon : dummy_icon);
}
