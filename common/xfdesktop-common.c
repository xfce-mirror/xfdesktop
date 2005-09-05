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
#include <libxfcegui4/libxfcegui4.h>

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

void
xfdesktop_send_client_message(Window xid, const gchar *msg)
{
	GdkEventClient gev;
	GtkWidget *win;
	
	win = gtk_invisible_new();
	gtk_widget_realize(win);
	
	gev.type = GDK_CLIENT_EVENT;
	gev.window = win->window;
	gev.send_event = TRUE;
	gev.message_type = gdk_atom_intern("STRING", FALSE);
	gev.data_format = 8;
	strcpy(gev.data.b, msg);
	
	gdk_event_send_client_message((GdkEvent *)&gev, (GdkNativeWindow)xid);
	gdk_flush();
	
	gtk_widget_destroy(win);
}

gboolean
xfdesktop_check_is_running(Window *xid)
{
	const gchar *display = g_getenv("DISPLAY");
	gchar *p;
	gint xscreen = -1;
	gchar selection_name[100];
	Atom selection_atom;
	
	if(display) {
		if((p=g_strrstr(display, ".")))
			xscreen = atoi(p);
	}
	if(xscreen == -1)
		xscreen = 0;

	g_snprintf(selection_name, 100, XFDESKTOP_SELECTION_FMT, xscreen);
	selection_atom = XInternAtom(GDK_DISPLAY(), selection_name, False);

	if((*xid = XGetSelectionOwner(GDK_DISPLAY(), selection_atom)))
		return TRUE;

	return FALSE;
}
