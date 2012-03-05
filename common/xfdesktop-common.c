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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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

#ifndef O_BINARY
#define O_BINARY  0
#endif

gboolean
xfdesktop_backdrop_list_is_valid(const gchar *path)
{
    FILE *fp;
    gchar buf[512];
    gint size;
    gboolean is_list = FALSE;

    size = sizeof(LIST_TEXT);

    if(!(fp = fopen (path, "r")))
        return FALSE;

    if(fgets(buf, size, fp) && !strncmp(LIST_TEXT, buf, size - 1))
        is_list = TRUE;
    fclose(fp);

    return is_list;
}

gchar **
xfdesktop_backdrop_list_load(const gchar *filename,
                             gint *n_items,
                             GError **error)
{
    gchar *contents = NULL, **files = NULL, *p, *q;
    gsize length = 0;
    gint arr_size = 10, count = 0;

    g_return_val_if_fail(filename && (!error || !*error), NULL);

    if(!g_file_get_contents(filename, &contents, &length, error))
        return NULL;

    if(strncmp(LIST_TEXT, contents, sizeof(LIST_TEXT) - 1)) {
        if(error) {
            g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                        _("Backdrop list file is not valid"));
        }
        g_free(contents);
        return NULL;
    }

    /* i'd use g_strsplit() here, but then counting is slower.  we can
     * also filter out blank lines */
    files = g_malloc(sizeof(gchar *) * (arr_size+1));
    p = contents + sizeof(LIST_TEXT);
    while(p && *p) {
        q = strstr(p, "\n");
        if(q) {
            if(p == q)  /* blank line */
                continue;
            *q = 0;
        } else
            q = contents + length;  /* assume no trailing '\n' at EOF */

        if(count == arr_size) {
            arr_size += 10;
            files = g_realloc(files, sizeof(gchar *) * (arr_size+1));
        }

        files[count++] = g_strdup(p);
        if(q != contents + length)
            p = q + 1;
    }
    files[count] = NULL;
    files = g_realloc(files, sizeof(gchar *) * (count+1));

    if(n_items)
        *n_items = count;

    g_free(contents);

    return files;
}

gboolean
xfdesktop_backdrop_list_save(const gchar *filename,
                             gchar * const *files,
                             GError **error)
{
    gboolean ret = FALSE;
    gchar *filename_new;
    FILE *fp;
    gint i;

    g_return_val_if_fail(filename && (!error || !*error), FALSE);

    filename_new = g_strconcat(filename, ".new", NULL);
    fp = fopen(filename_new, "w");
    if(fp) {
        fprintf(fp, "%s\n", LIST_TEXT);
        if(files) {
            for(i = 0; files[i]; ++i)
                fprintf(fp, "%s\n", files[i]);
        }
        if(!fclose(fp)) {
            if(!rename(filename_new, filename))
                ret = TRUE;
            else  {
                if(error) {
                    g_set_error(error, G_FILE_ERROR,
                                g_file_error_from_errno(errno),
                                "%s", g_strerror(errno));
                }
                unlink(filename_new);
            }
        } else {
            if(error) {
                g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                            "%s", g_strerror(errno));
            }
            unlink(filename_new);
        }
    } else if(error) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                    "%s", g_strerror(errno));
    }

    g_free(filename_new);

    return ret;
}

gchar *
xfdesktop_backdrop_list_choose_random(const gchar *filename,
                                      GError **error)
{
    static gboolean __initialized = FALSE;
    static gint previndex = -1;
    gchar **files, *file = NULL;
    gint n_items = 0, cur_file, i, tries = 0;

    g_return_val_if_fail(filename && (!error || !*error), NULL);

    files = xfdesktop_backdrop_list_load(filename, &n_items, error);
    if(!files)
        return NULL;
    if(!n_items) {
        if(error) {
            g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                        _("Backdrop list file is not valid"));
        }
        g_strfreev(files);
        return NULL;
    }

    if(1 == n_items) {
        file = g_strdup(files[0]);
        g_strfreev(files);
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
            g_strfreev(files);
            return NULL;
        }

        do {
#ifdef HAVE_SRANDOM
            cur_file = random() % n_items;
#else
            cur_file = rand() % n_items;
#endif
        } while(cur_file == previndex && G_LIKELY(previndex != -1));

    } while(!xfdesktop_image_file_is_valid(files[cur_file]));

    previndex = cur_file;
    file = files[cur_file];

    /* don't use strfreev() and avoid an extra alloc */
    for(i = 0; files[i]; ++i) {
        if(i != cur_file)
            g_free(files[i]);
    }
    g_free(files);

    return file;
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
xfdesktop_image_file_is_valid(const gchar *filename)
{
    GdkPixbufLoader *loader;
    int fd;
    gboolean size_read = FALSE;
    guchar buf[4096];
    gssize len;

    g_return_val_if_fail(filename, FALSE);

    fd = open(filename, O_RDONLY|O_BINARY);
    if(fd < 0)
        return FALSE;

    loader = gdk_pixbuf_loader_new();
    g_signal_connect(G_OBJECT(loader), "size-prepared",
            G_CALLBACK(pixbuf_loader_size_cb), &size_read);

    do {
        len = read(fd, buf, sizeof(buf));
        if(len > 0) {
            if(!gdk_pixbuf_loader_write(loader, buf, len, NULL))
                break;
            if(size_read)
                break;
        }
    } while(len > 0);

    close(fd);
    gdk_pixbuf_loader_close(loader, NULL);
    g_object_unref(G_OBJECT(loader));

    return size_read;
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

    while ((i++ < 2500) && (grab_failed = ((g1 != GDK_GRAB_SUCCESS)
                || (g2 != GDK_GRAB_SUCCESS))))
    {
        TRACE ("grab not available yet, mouse reason: %d, keyboard reason: %d, waiting... (%i)", g1, g2, i);
        if(g1 == GDK_GRAB_INVALID_TIME || g2 == GDK_GRAB_INVALID_TIME)
            break;

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
