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


gint
xfdesktop_compare_paths(GFile *a, GFile *b)
{
    gchar *path_a, *path_b;
    gboolean ret;

    path_a = g_file_get_path(a);
    path_b = g_file_get_path(b);

    XF_DEBUG("a %s, b %s", path_a, path_b);

    ret = g_strcmp0(path_a, path_b);

    g_free(path_a);
    g_free(path_b);

    return ret;
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

/* Code taken from xfwm4/src/menu.c:grab_available().  This should fix the case
 * where binding 'xfdesktop -menu' to a keyboard shortcut sometimes works and
 * sometimes doesn't.  Credit for this one goes to Olivier.
 * Returns the grab time if successful, 0 on failure.
 */
guint32
xfdesktop_popup_keyboard_grab_available(GdkWindow *win)
{
    GdkGrabStatus grab;
    gboolean grab_failed = FALSE;
    guint32 timestamp;
    gint i = 0;

    TRACE("entering");

    timestamp = gtk_get_current_event_time();

    grab = gdk_keyboard_grab(win, TRUE, timestamp);

    while((i++ < 2500) && (grab_failed = ((grab != GDK_GRAB_SUCCESS))))
    {
        TRACE ("keyboard grab not available yet, reason: %d, waiting... (%i)", grab, i);
        if(grab == GDK_GRAB_INVALID_TIME)
            break;

        g_usleep (100);
        if(grab != GDK_GRAB_SUCCESS) {
            grab = gdk_keyboard_grab(win, TRUE, timestamp);
        }
    }

    if (grab == GDK_GRAB_SUCCESS) {
        gdk_keyboard_ungrab(timestamp);
    } else {
        timestamp = 0;
    }

    return timestamp;
}


/*
 * xfdesktop_remove_whitspaces:
 * remove all whitespaces from string (not only trailing or leading)
 */
gchar*
xfdesktop_remove_whitspaces(gchar* str)
{
    gchar* dest;
    gint offs, curr;

    g_return_val_if_fail(str, NULL);

    offs = 0;
    dest = str;
    for(curr=0; curr<=strlen(str); curr++) {
        if(*dest == ' ' || *dest == '\t')
            offs++;
        else if(0 != offs)
            *(dest-offs) = *dest;
        dest++;
    }

    return str;
}


#ifdef G_ENABLE_DEBUG
/* With --enable-debug=full turn on debugging messages from the start */
static gboolean enable_debug = TRUE;
#else
static gboolean enable_debug = FALSE;
#endif /* G_ENABLE_DEBUG */

#if defined(G_HAVE_ISO_VARARGS)
void
xfdesktop_debug(const char *func, const char *file, int line, const char *format, ...)
{
    va_list args;

    if(!enable_debug)
        return;

    va_start(args, format);

    fprintf(stdout, "DBG[%s:%d] %s(): ", file, line, func);
    vfprintf(stdout, format, args);
    fprintf(stdout, "\n");

    va_end(args);
}
#endif /* defined(G_HAVE_ISO_VARARGS) */

/**
 * xfdesktop_debug_set:
 * debug: TRUE to turn on the XF_DEBUG mesages.
 */
void
xfdesktop_debug_set(gboolean debug)
{
    enable_debug = debug;
    if(enable_debug)
        XF_DEBUG("debugging enabled");
}
