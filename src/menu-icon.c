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
static gchar const *pix_paths[] = {
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
	NULL
};

#ifdef HAVE_GETENV
static gchar *kdefmts[] = {
	"%s/share/icons/default.kde/scalable/apps/%s",
	"%s/share/icons/default.kde/48x48/apps/%s",
	"%s/share/icons/default.kde/32x32/apps/%s",
	"%s/share/icons/hicolor/scalable/apps/%s",
	"%s/share/icons/hicolor/48x48/apps/%s",
	"%s/share/icons/hicolor/32x32/apps/%s",
	NULL
};
#endif

static gchar const *pix_ext[] = {
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
	PangoContext *pcontext;
	PangoFontDescription *pfdesc;
	PangoLanguage *plang;
	PangoFontMetrics *pfmetrics;
	gint totheight;
	
	tmp = gtk_label_new("foo");
	gtk_widget_show(tmp);
	pcontext = gtk_widget_get_pango_context(tmp);
	g_object_ref(G_OBJECT(pcontext));
	gtk_widget_destroy(tmp);
	pfdesc = pango_context_get_font_description(pcontext);
	plang = pango_context_get_language(pcontext);
	pfmetrics = pango_context_get_metrics(pcontext, pfdesc, plang);
	totheight = PANGO_PIXELS((pango_font_metrics_get_ascent(pfmetrics) +
			pango_font_metrics_get_descent(pfmetrics)));
	totheight += 7;  /* FIXME: fudge factor */
	//g_object_unref(G_OBJECT(pfmetrics));  /* FIXME: crashes with, leaks without */
	g_object_unref(G_OBJECT(pcontext));

	return totheight;
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
		if(!found && kdedir && *kdedir=='/' && strcmp(kdedir, "/usr")) {
			for(i=0; kdefmts[i] && !found; i++) {
				g_snprintf(icon_path, PATH_MAX, kdefmts[i], kdedir, filename);
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
		}
#endif
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
