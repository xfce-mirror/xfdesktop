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

#include <stdio.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <glib.h>
#include <gdk/gdkx.h>
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
xfce_desktop_get_menufile(void)
{
    XfceKiosk *kiosk;
    gboolean user_menu;
    gchar *menu_file = NULL;
    gchar **all_dirs;
    const gchar *userhome = xfce_get_homedir();
    gint i;

    kiosk = xfce_kiosk_new("xfdesktop");
    user_menu = xfce_kiosk_query(kiosk, "UserMenu");
    xfce_kiosk_free(kiosk);
    
    if(user_menu) {
        gchar *file = xfce_resource_save_location(XFCE_RESOURCE_CONFIG,
                                                  "menus/xfce-applications.menu",
                                                  FALSE);
        if(file) {
            DBG("checking %s", file);
            if(g_file_test(file, G_FILE_TEST_IS_REGULAR))
                return file;
            else
                g_free(file);
        }
    }
    
    all_dirs = xfce_resource_lookup_all(XFCE_RESOURCE_CONFIG,
                                        "menus/xfce-applications.menu");
    for(i = 0; all_dirs[i]; i++) {
        DBG("checking %s", all_dirs[i]);
        if(user_menu || strstr(all_dirs[i], userhome) != all_dirs[i]) {
            if(g_file_test(all_dirs[i], G_FILE_TEST_IS_REGULAR)) {
                menu_file = g_strdup(all_dirs[i]);
                break;
            }
        }
    }
    g_strfreev(all_dirs);

    if(!menu_file)
        g_warning("%s: Could not locate a menu definition file", PACKAGE);

    return menu_file;
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

/* Code taken from xfwm4/src/menu.c:grab_available().  This should fix the case
 * where binding 'xfdesktop -menu' to a keyboard shortcut sometimes works and
 * sometimes doesn't.  Credit for this one goes to Olivier.
 */
gboolean
xfdesktop_popup_grab_available (GdkWindow *win, guint32 timestamp)
{
    GdkEventMask mask =
        GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
        GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK |
        GDK_POINTER_MOTION_MASK;
    GdkGrabStatus g1;
    GdkGrabStatus g2;
    gboolean grab_failed = FALSE;
    gint i = 0;

    TRACE ("entering grab_available");

    g1 = gdk_pointer_grab (win, TRUE, mask, NULL, NULL, timestamp);
    g2 = gdk_keyboard_grab (win, TRUE, timestamp);

    while ((i++ < 300) && (grab_failed = ((g1 != GDK_GRAB_SUCCESS)
                || (g2 != GDK_GRAB_SUCCESS))))
    {
        TRACE ("grab not available yet, waiting... (%i)", i);
        g_usleep (100);
        if (g1 != GDK_GRAB_SUCCESS)
        {
            g1 = gdk_pointer_grab (win, TRUE, mask, NULL, NULL, timestamp);
        }
        if (g2 != GDK_GRAB_SUCCESS)
        {
            g2 = gdk_keyboard_grab (win, TRUE, timestamp);
        }
    }

    if (g1 == GDK_GRAB_SUCCESS)
    {
        gdk_pointer_ungrab (timestamp);
    }
    if (g2 == GDK_GRAB_SUCCESS)
    {
        gdk_keyboard_ungrab (timestamp);
    }

    return (!grab_failed);
}
