/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright(c) 2025 The Xfce Development Team
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
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "xfdesktop-mime-type.h"
#include "xfdesktop-common.h"

#ifdef ENABLE_VIDEO_BACKDROP
static const gchar *video_mime_type_list[] = {
    "video/mp4",
    "video/webm",
    NULL
};
#endif /* ENABLE_VIDEO_BACKDROP */

static GHashTable *pixbuf_mime_type_set = NULL;

gchar *
xfdesktop_get_file_mime_type(GFile *file) {
    GFileInfo *file_info;
    gchar *mime_type = NULL;

    g_return_val_if_fail(G_IS_FILE(file), NULL);

    file_info = g_file_query_info(file,
                                  "standard::content-type",
                                  0,
                                  NULL,
                                  NULL);

    if(file_info != NULL) {
        mime_type = g_strdup(g_file_info_get_content_type(file_info));
        g_object_unref(file_info);
    }

    return mime_type;
}

static void
build_pixbuf_mime_type_set(void) {
    GSList *formats, *l;
    gchar **mimetypes;
    guint i;

    g_return_if_fail(pixbuf_mime_type_set == NULL);

    pixbuf_mime_type_set = g_hash_table_new(g_str_hash, g_str_equal);
    formats = gdk_pixbuf_get_formats();
    for(l = formats; l != NULL; l = g_slist_next(l)) {
        mimetypes = gdk_pixbuf_format_get_mime_types(l->data);
        for (i = 0; mimetypes[i] != NULL; ++i) 
            g_hash_table_insert(pixbuf_mime_type_set, mimetypes[i], NULL);
        g_free(mimetypes);
    }
    g_slist_free(formats);
}

static gboolean
is_pixbuf_mimetype(const gchar *mimetype) {
    g_return_val_if_fail(mimetype != NULL, FALSE);
    
    if (pixbuf_mime_type_set == NULL)
        build_pixbuf_mime_type_set();

    return g_hash_table_contains(pixbuf_mime_type_set, mimetype);
}

void
xfdesktop_media_mime_type_to_filter(GtkFileFilter *filter) {
    g_return_if_fail(filter != NULL);
    
    gtk_file_filter_add_pixbuf_formats(filter);

#ifdef ENABLE_VIDEO_BACKDROP
    for (guint i = 0; video_mime_type_list[i] != NULL; ++i) {
        gtk_file_filter_add_mime_type(filter, video_mime_type_list[i]);
    }
#endif /* ENABLE_VIDEO_BACKDROP */
}

gboolean
xfdesktop_file_has_media_mime_type(GFile *file) {
    gchar *file_mimetype;
    gboolean has = FALSE;

    g_return_val_if_fail(file != NULL, FALSE);

    file_mimetype = xfdesktop_get_file_mime_type(file);
    if (file_mimetype == NULL) 
        return FALSE;

    if (!has) 
        has = is_pixbuf_mimetype(file_mimetype);

#ifdef ENABLE_VIDEO_BACKDROP
    if (!has)
        has = g_strv_contains(video_mime_type_list, file_mimetype);
#endif /* ENABLE_VIDEO_BACKDROP */

    g_free(file_mimetype);
    return has;
}

#ifdef ENABLE_VIDEO_BACKDROP
gboolean
xfdesktop_file_has_video_mime_type(GFile *file) {
    gchar *file_mimetype;
    gboolean has;
    
    file_mimetype = xfdesktop_get_file_mime_type(file);
    has = g_strv_contains(video_mime_type_list, file_mimetype);
    g_free(file_mimetype);
    return has;
}
#endif /* ENABLE_VIDEO_BACKDROP */
