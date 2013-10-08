/*
 *  Copyright (C) 2002 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *  Copyright (C) 2003 Benedikt Meurer (benedikt.meurer@unix-ag.uni-siegen.de)
 *  Copyright (c) 2004-2007 Brian Tarricone <bjt23@cornell.edu>
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
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <glib.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>

#include "xfdesktop-common.h"
#include "xfce-backdrop.h" /* for XfceBackdropImageStyle */

#ifndef O_BINARY
#define O_BINARY  0
#endif

static GList *
list_files_in_dir(const gchar *path)
{
    GDir *dir;
    gboolean needs_slash = TRUE;
    const gchar *file;
    GList *files = NULL;

    dir = g_dir_open(path, 0, 0);
    if(!dir)
        return NULL;

    if(path[strlen(path)-1] == '/')
        needs_slash = FALSE;

    while((file = g_dir_read_name(dir))) {
        gchar *current_file = g_strdup_printf(needs_slash ? "%s/%s" : "%s%s",
                                              path, file);

        files = g_list_insert_sorted(files, current_file, (GCompareFunc)g_strcmp0);
    }

    g_dir_close(dir);

    return files;
}


gchar *
xfdesktop_backdrop_choose_next(const gchar *filename)
{
    GList *files, *current_file, *start_file;
    gchar *file = NULL;

    g_return_val_if_fail(filename, NULL);

    files = list_files_in_dir(g_path_get_dirname(filename));
    if(!files)
        return NULL;

    /* Get the our current background in the list */
    current_file = g_list_find_custom(files, filename, (GCompareFunc)g_strcmp0);

    /* if somehow we don't have a valid file, grab the first one available */
    if(current_file == NULL)
        current_file = g_list_first(files);

    start_file = current_file;

    /* We want the next valid image file in the dir while making sure
     * we don't loop on ourselves */
    do {
        current_file = g_list_next(current_file);

        /* we hit the end of the list */
        if(current_file == NULL)
            current_file = g_list_first(files);

        /* We went through every item in the list */
        if(g_strcmp0(start_file->data, current_file->data) == 0)
            break;

    } while(!xfdesktop_image_file_is_valid(current_file->data));

    file = g_strdup(current_file->data);
    g_list_free_full(files, g_free);

    return file;
}

gchar *
xfdesktop_backdrop_choose_random(const gchar *filename)
{
    static gboolean __initialized = FALSE;
    static gint previndex = -1;
    GList *files;
    gchar *file = NULL;
    gint n_items = 0, cur_file, tries = 0;

    g_return_val_if_fail(filename, NULL);

    files = list_files_in_dir(g_path_get_dirname(filename));
    if(!files)
        return NULL;

    n_items = g_list_length(files);

    if(1 == n_items) {
        file = g_strdup(g_list_first(files)->data);
        g_list_free_full(files, g_free);
        return file;
    }

    /* NOTE: 4.3BSD random()/srandom() are a) stronger and b) faster than
    * ANSI-C rand()/srand(). So we use random() if available */
    if(G_UNLIKELY(!__initialized))    {
        guint seed = time(NULL) ^ (getpid() + (getpid() << 15));
#ifdef HAVE_SRANDOM
        srandom(seed);
#else
        srand(seed);
#endif
        __initialized = TRUE;
    }

    do {
        if(tries++ == n_items) {
            /* this isn't precise, but if we've failed to get a good
             * image after all this time, let's just give up */
            g_warning("Unable to find good image from list; giving up");
            g_list_free_full(files, g_free);
            return NULL;
        }

        do {
#ifdef HAVE_SRANDOM
            cur_file = random() % n_items;
#else
            cur_file = rand() % n_items;
#endif
        } while(cur_file == previndex && G_LIKELY(previndex != -1));

    } while(!xfdesktop_image_file_is_valid(g_list_nth(files, cur_file)->data));

    previndex = cur_file;

    file = g_strdup(g_list_nth(files, cur_file)->data);
    g_list_free_full(files, g_free);

    return file;
}

gchar *
xfdesktop_get_file_mimetype(const gchar *file)
{
    GFile *temp_file;
    GFileInfo *file_info;
    gchar *mime_type = NULL;

    g_return_val_if_fail(file != NULL, NULL);

    temp_file = g_file_new_for_path(file);

    g_return_val_if_fail(temp_file != NULL, NULL);

    file_info = g_file_query_info(temp_file,
                                  "standard::content-type",
                                  0,
                                  NULL,
                                  NULL);

    if(file_info != NULL) {
        mime_type = g_strdup(g_file_info_get_content_type(file_info));

        g_object_unref(file_info);
    }

    g_object_unref(temp_file);

    return mime_type;
}

gboolean
xfdesktop_image_file_is_valid(const gchar *filename)
{
    static GSList *pixbuf_formats = NULL;
    GSList *l;
    gboolean image_valid = FALSE;
    gchar *file_mimetype;

    g_return_val_if_fail(filename, FALSE);

    if(pixbuf_formats == NULL) {
        pixbuf_formats = gdk_pixbuf_get_formats();
    }

    file_mimetype = xfdesktop_get_file_mimetype(filename);

    if(file_mimetype == NULL)
        return FALSE;

    /* Every pixbuf format has a list of mime types we can compare against */
    for(l = pixbuf_formats; l != NULL && image_valid == FALSE; l = g_slist_next(l)) {
        gint i;
        gchar ** mimetypes = gdk_pixbuf_format_get_mime_types(l->data);

        for(i = 0; mimetypes[i] != NULL && image_valid == FALSE; i++) {
            if(g_strcmp0(file_mimetype, mimetypes[i]) == 0)
                image_valid = TRUE;
        }
         g_strfreev(mimetypes);
    }

    g_free(file_mimetype);

    return image_valid;
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
    selection_atom = XInternAtom(gdk_x11_get_default_xdisplay(), selection_name, False);

    if((*xid = XGetSelectionOwner(gdk_x11_get_default_xdisplay(), selection_atom)))
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
    gev.window = gtk_widget_get_window(win);
    gev.send_event = TRUE;
    gev.message_type = gdk_atom_intern("STRING", FALSE);
    gev.data_format = 8;
    strcpy(gev.data.b, msg);

    gdk_event_send_client_message((GdkEvent *)&gev, (GdkNativeWindow)xid);
    gdk_flush();

    gtk_widget_destroy(win);
}

/* The image styles changed from versions prior to 4.11.
 * Auto isn't an option anymore, additionally we should handle invalid
 * values. Set them to the default of stretched. */
gint
xfce_translate_image_styles(gint input)
{
    gint style = input;

    if(style <= 0 || style > XFCE_BACKDROP_IMAGE_SPANNING_SCREENS)
        style = XFCE_BACKDROP_IMAGE_STRETCHED;

    return style;
}
