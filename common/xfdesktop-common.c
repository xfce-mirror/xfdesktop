/*
 *  Copyright (C) 2002 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *  Copyright (C) 2003 Benedikt Meurer (benedikt.meurer@unix-ag.uni-siegen.de)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <stdio.h>

#include <glib.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/dialogs.h>

#include "xfdesktop-common.h"

gboolean
is_backdrop_list(const gchar *path)
{
	FILE *fp;
	gchar buf[512];
	gint size;
	gboolean is_list = FALSE;
	
	size = sizeof(LIST_TEXT);
	
	if(!(fp = fopen (path, "r")))
		return FALSE;
	
	if(fgets(buf, size, fp) > 0 && !strncmp(LIST_TEXT, buf, size - 1))
		is_list = TRUE;
	fclose(fp);
	
	return is_list;
}

gchar **
get_list_from_file(const gchar *filename)
{
	gchar *contents;
	GError *error = NULL;
	gchar **files = NULL;
	gsize length;
	
	files = NULL;
	
	if(!g_file_get_contents(filename, &contents, &length, &error)) {
		xfce_err("Unable to get backdrop image list from file %s: %s",
				filename, error->message);
		g_error_free(error);
		return NULL;
	}
	
	if(strncmp(LIST_TEXT, contents, sizeof(LIST_TEXT) - 1)) {
		xfce_err("Not a backdrop image list file: %s", filename);
		goto finished;
	}
	
	files = g_strsplit(contents + sizeof(LIST_TEXT), "\n", -1);
	
	finished:
	g_free(contents);
	
	return files;
}

static void
pixbuf_loader_size_cb(GdkPixbufLoader *loader, gint width, gint height,
		gpointer user_data)
{
	gboolean *size_read = user_data;
	
	if(width > 0 && height > 0)
		*size_read = TRUE;
}

gboolean
xfdesktop_check_image_file(const gchar *filename)
{
	GdkPixbufLoader *loader;
	FILE *fp;
	gboolean size_read = FALSE;
	guchar buf[4096];
	gint len;
	
	fp = fopen(filename, "rb");
	if(!fp)
		return FALSE;
	
	loader = gdk_pixbuf_loader_new();
	g_signal_connect(G_OBJECT(loader), "size-prepared",
			G_CALLBACK(pixbuf_loader_size_cb), &size_read);
	
	while(!feof(fp) && !ferror(fp)) {
		if((len=fread(buf, 1, sizeof(buf), fp)) > 0) {
			if(!gdk_pixbuf_loader_write(loader, buf, len, NULL))
				break;
			if(size_read)
				break;
		}
	}
	
	fclose(fp);
	gdk_pixbuf_loader_close(loader, NULL);
	g_object_unref(G_OBJECT(loader));
	
	return size_read;
}

gchar *
desktop_menu_file_get_menufile()
{
	XfceKiosk *kiosk;
	gboolean user_menu;
	gchar filename[PATH_MAX], searchpath[PATH_MAX*3+2], **all_dirs;
	gint i;

	kiosk = xfce_kiosk_new("xfdesktop");
	user_menu = xfce_kiosk_query(kiosk, "UserMenu");
	xfce_kiosk_free(kiosk);
	
	if(!user_menu) {
		const gchar *userhome = xfce_get_homedir();
		all_dirs = xfce_resource_lookup_all(XFCE_RESOURCE_CONFIG,
				"xfce4/desktop/");
		
		for(i = 0; all_dirs[i]; i++) {
			if(strstr(all_dirs[i], userhome) != all_dirs[i]) {
				g_snprintf(searchpath, PATH_MAX*3+2,
						"%s%%F.%%L:%s%%F.%%l:%s%%F",
						all_dirs[i], all_dirs[i], all_dirs[i]);
				if(xfce_get_path_localized(filename, PATH_MAX, searchpath,
						"menu.xml", G_FILE_TEST_IS_REGULAR))
				{
					g_strfreev(all_dirs);
					return g_strdup(filename);
				}
			}			
		}
		g_strfreev(all_dirs);
	} else {
		gchar *menu_file = xfce_resource_save_location(XFCE_RESOURCE_CONFIG,
				"xfce4/desktop/menu.xml", FALSE);
		if(menu_file && g_file_test(menu_file, G_FILE_TEST_IS_REGULAR))
			return menu_file;
		else if(menu_file)
			g_free(menu_file);
		
		all_dirs = xfce_resource_lookup_all(XFCE_RESOURCE_CONFIG,
				"xfce4/desktop/");
		for(i = 0; all_dirs[i]; i++) {
			g_snprintf(searchpath, PATH_MAX*3+2,
					"%s%%F.%%L:%s%%F.%%l:%s%%F",
					all_dirs[i], all_dirs[i], all_dirs[i]);
			if(xfce_get_path_localized(filename, PATH_MAX, searchpath,
					"menu.xml", G_FILE_TEST_IS_REGULAR))
			{
				g_strfreev(all_dirs);
				return g_strdup(filename);
			}		
		}
		g_strfreev(all_dirs);
	}

    g_warning("%s: Could not locate a menu definition file", PACKAGE);

    return NULL;
}
