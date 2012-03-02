/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright(c) 2006 Brian Tarricone, <bjt23@cornell.edu>
 *  Copyright(c) 2006 Benedikt Meurer, <benny@xfce.org>
 *  Copyright(c) 2010-2011 Jannis Pohlmann, <jannis@xfce.org>
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
 *
 *  xfdesktop-thumbnailer is based on thumbnailer code from Ristretto
 *  Copyright (c) Stephan Arts 2009-2011 <stephan@xfce.org>
 *
 *  Thumbnailer Spec
 *  http://live.gnome.org/ThumbnailerSpec
 *  Thumbnail Managing Standard
 *  http://people.freedesktop.org/~vuntz/thumbnail-spec-cache/creation.html
 */

#include <config.h>

#include <string.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

#include <dbus/dbus-glib.h>

#include <libxfce4util/libxfce4util.h>
#include "xfdesktop-thumbnailer.h"
#include "xfdesktop-marshal.h"

static void xfdesktop_thumbnailer_init(GObject *);
static void xfdesktop_thumbnailer_class_init(GObjectClass *);

static void xfdesktop_thumbnailer_dispose(GObject *object);
static void xfdesktop_thumbnailer_finalize(GObject *object);

static void xfdesktop_thumbnailer_request_finished_dbus(DBusGProxy *proxy,
                                                        gint handle,
                                                        gpointer data);

static void xfdesktop_thumbnailer_thumbnail_ready_dbus(DBusGProxy *proxy,
                                                       gint handle,
                                                       const gchar **uri,
                                                       gpointer data);

static gboolean xfdesktop_thumbnailer_queue_request_timer(XfdesktopThumbnailer *thumbnailer);

static GObjectClass *parent_class = NULL;
static XfdesktopThumbnailer *thumbnailer_object = NULL;

enum
{
    THUMBNAIL_READY,
    LAST_SIGNAL,
};

static guint thumbnailer_signals[LAST_SIGNAL] = { 0, };

GType
xfdesktop_thumbnailer_get_type(void)
{
    static GType xfdesktop_thumbnailer_type = 0;

    if(!xfdesktop_thumbnailer_type) {
        static const GTypeInfo xfdesktop_thumbnailer_info =
        {
            sizeof (XfdesktopThumbnailerClass),
            (GBaseInitFunc) NULL,
            (GBaseFinalizeFunc) NULL,
            (GClassInitFunc) xfdesktop_thumbnailer_class_init,
            (GClassFinalizeFunc) NULL,
            NULL,
            sizeof (XfdesktopThumbnailer),
            0,
            (GInstanceInitFunc) xfdesktop_thumbnailer_init,
            NULL
        };

        xfdesktop_thumbnailer_type = g_type_register_static(
                                                    G_TYPE_OBJECT,
                                                    "XfdesktopThumbnailer",
                                                    &xfdesktop_thumbnailer_info,
                                                    0);
    }
    return xfdesktop_thumbnailer_type;
}

struct _XfdesktopThumbnailerPriv
{
    DBusGProxy               *proxy;

    GSList                   *queue;
    gchar                   **supported_mimetypes;
    gboolean                  big_thumbnails;
    gint                      handle;

    gint                      request_timer_id;
};

static void
xfdesktop_thumbnailer_init(GObject *object)
{
    XfdesktopThumbnailer *thumbnailer;
    DBusGConnection      *connection;

    thumbnailer = XFDESKTOP_THUMBNAILER(object);

    thumbnailer->priv = g_new0(XfdesktopThumbnailerPriv, 1);

    connection = dbus_g_bus_get(DBUS_BUS_SESSION, NULL);

    if(connection) {
        thumbnailer->priv->proxy = dbus_g_proxy_new_for_name(
                                    connection,
                                    "org.freedesktop.thumbnails.Thumbnailer1",
                                    "/org/freedesktop/thumbnails/Thumbnailer1",
                                    "org.freedesktop.thumbnails.Thumbnailer1");

        if(thumbnailer->priv->proxy) {
            gchar **supported_uris = NULL;
            gchar **supported_flavors = NULL;

            dbus_g_object_register_marshaller(
                    (GClosureMarshal) xfdesktop_marshal_VOID__UINT_BOXED,
                    G_TYPE_NONE, G_TYPE_UINT,
                    G_TYPE_STRV, G_TYPE_INVALID);

            dbus_g_proxy_add_signal(
                    thumbnailer->priv->proxy,
                    "Finished", G_TYPE_UINT, G_TYPE_INVALID);
            dbus_g_proxy_add_signal(
                    thumbnailer->priv->proxy,
                    "Ready", G_TYPE_UINT, G_TYPE_STRV, G_TYPE_INVALID);

            dbus_g_proxy_connect_signal(
                    thumbnailer->priv->proxy,
                    "Finished", G_CALLBACK (xfdesktop_thumbnailer_request_finished_dbus),
                    thumbnailer, NULL);
            dbus_g_proxy_connect_signal(
                    thumbnailer->priv->proxy,
                    "Ready", G_CALLBACK(xfdesktop_thumbnailer_thumbnail_ready_dbus),
                    thumbnailer, NULL);

            dbus_g_proxy_call(thumbnailer->priv->proxy, "GetSupported", NULL, G_TYPE_INVALID,
                              G_TYPE_STRV, &supported_uris,
                              G_TYPE_STRV, &thumbnailer->priv->supported_mimetypes,
                              G_TYPE_INVALID);

            dbus_g_proxy_call(thumbnailer->priv->proxy, "GetFlavors", NULL, G_TYPE_INVALID,
                              G_TYPE_STRV, &supported_flavors,
                              G_TYPE_INVALID);

            if(supported_flavors != NULL) {
                gint n;
                for(n = 0; supported_flavors[n] != NULL; ++n) {
                    if(g_strcmp0(supported_flavors[n], "large")) {
                        thumbnailer->priv->big_thumbnails = TRUE;
                    }
                }
            } else {
                thumbnailer->priv->big_thumbnails = FALSE;
                g_warning("Thumbnailer failed calling GetFlavors");
            }

            g_strfreev(supported_flavors);
            g_strfreev(supported_uris);
        }

        dbus_g_connection_unref(connection);
    }
}

static void
xfdesktop_thumbnailer_class_init (GObjectClass *object_class)
{
    XfdesktopThumbnailerClass *thumbnailer_class = XFDESKTOP_THUMBNAILER_CLASS(object_class);

    parent_class = g_type_class_peek_parent(thumbnailer_class);

    object_class->dispose = xfdesktop_thumbnailer_dispose;
    object_class->finalize = xfdesktop_thumbnailer_finalize;

    thumbnailer_signals[THUMBNAIL_READY] = g_signal_new (
                        "thumbnail-ready",
                        G_OBJECT_CLASS_TYPE (object_class),
                        G_SIGNAL_RUN_LAST,
                        G_STRUCT_OFFSET(XfdesktopThumbnailerClass, thumbnail_ready),
                        NULL, NULL,
                        xfdesktop_marshal_VOID__STRING_STRING,
                        G_TYPE_NONE, 2,
                        G_TYPE_STRING, G_TYPE_STRING);
}

/**
 * xfdesktop_thumbnailer_dispose:
 * @object:
 *
 */
static void
xfdesktop_thumbnailer_dispose(GObject *object)
{
    XfdesktopThumbnailer *thumbnailer = XFDESKTOP_THUMBNAILER(object);

    if(thumbnailer->priv->proxy)
        g_object_unref(thumbnailer->priv->proxy);

    if(thumbnailer->priv->supported_mimetypes)
        g_free(thumbnailer->priv->supported_mimetypes);

    if(thumbnailer->priv) {
        g_free(thumbnailer->priv);
        thumbnailer->priv = NULL;
    }

    thumbnailer_object = NULL;
}

/**
 * xfdesktop_thumbnailer_finalize:
 * @object:
 *
 */
static void
xfdesktop_thumbnailer_finalize(GObject *object)
{
}

/**
 * xfdesktop_thumbnailer_new:
 *
 *
 * Singleton
 */
XfdesktopThumbnailer *
xfdesktop_thumbnailer_new(void)
{
    if(thumbnailer_object == NULL) {
        thumbnailer_object = g_object_new(XFDESKTOP_TYPE_THUMBNAILER, NULL);
    } else {
        g_object_ref(thumbnailer_object);
    }

    return thumbnailer_object;
}

static gchar *
xfdesktop_get_file_mimetype(gchar *file)
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
xfdesktop_thumbnailer_is_supported(XfdesktopThumbnailer *thumbnailer,
                                   gchar *file)
{
    guint        n;
    gchar       *mime_type = NULL;

    g_return_val_if_fail(XFDESKTOP_IS_THUMBNAILER(thumbnailer), FALSE);
    g_return_val_if_fail(file != NULL, FALSE);

    mime_type = xfdesktop_get_file_mimetype(file);

    if(mime_type == NULL) {
        DBG("File %s has no mime type", file);
        return FALSE;
    }

    if(thumbnailer->priv->supported_mimetypes != NULL) {
        for(n = 0; thumbnailer->priv->supported_mimetypes[n] != NULL; ++n) {
            if(g_content_type_is_a (mime_type, thumbnailer->priv->supported_mimetypes[n])) {
                g_free(mime_type);
                return TRUE;
            }
        }
    }

    g_free(mime_type);
    return FALSE;
}

/**
 * xfdesktop_thumbnailer_queue_thumbnail:
 *
 * Queues a file for thumbnail creation.
 * A "thumbnail-ready" signal will be emitted when the thumbnail is ready.
 * The signal will pass 2 parameters: a gchar *file which will be file
 * that's passed in here and a gchar *thumbnail_file which will be the
 * location of the thumbnail.
 */
gboolean
xfdesktop_thumbnailer_queue_thumbnail(XfdesktopThumbnailer *thumbnailer,
                                      gchar *file)
{
    g_return_val_if_fail(XFDESKTOP_IS_THUMBNAILER(thumbnailer), FALSE);
    g_return_val_if_fail(file != NULL, FALSE);

    if(!xfdesktop_thumbnailer_is_supported(thumbnailer, file)) {
        DBG("file: %s not supported", file);
        return FALSE;
    }
    if(thumbnailer->priv->request_timer_id) {
        g_source_remove(thumbnailer->priv->request_timer_id);

        if(thumbnailer->priv->handle && thumbnailer->priv->proxy != NULL) {
            if(dbus_g_proxy_call(thumbnailer->priv->proxy,
                                 "Dequeue",
                                 NULL,
                                 G_TYPE_UINT, thumbnailer->priv->handle,
                                 G_TYPE_INVALID) == FALSE)
            {
                g_warning("Dequeue of thumbnailer->priv->handle: %d failed",
                          thumbnailer->priv->handle);
            }

            thumbnailer->priv->handle = 0;
        }
    }

    if(g_slist_find(thumbnailer->priv->queue, file) == NULL) {
        thumbnailer->priv->queue = g_slist_prepend(thumbnailer->priv->queue,
                                                   file);
    }

    thumbnailer->priv->request_timer_id = g_timeout_add_full(
                        G_PRIORITY_LOW,
                        300,
                        (GSourceFunc)xfdesktop_thumbnailer_queue_request_timer,
                        thumbnailer,
                        NULL);

    return TRUE;
}

/**
 * xfdesktop_thumbnailer_dequeue_thumbnail:
 * 
 * Removes a file from the list of pending thumbnail creations.
 * This is not guaranteed to always remove the file, if processing
 * of that thumbnail has started it won't stop.
 */
void
xfdesktop_thumbnailer_dequeue_thumbnail(XfdesktopThumbnailer *thumbnailer,
                                        gchar *file)
{
    g_return_if_fail(XFDESKTOP_IS_THUMBNAILER(thumbnailer));
    g_return_if_fail(file != NULL);

    if(thumbnailer->priv->request_timer_id) {
        g_source_remove(thumbnailer->priv->request_timer_id);

        if(thumbnailer->priv->handle && thumbnailer->priv->proxy) {
            if(dbus_g_proxy_call(thumbnailer->priv->proxy,
                                 "Dequeue",
                                 NULL,
                                 G_TYPE_UINT, thumbnailer->priv->handle,
                                 G_TYPE_INVALID) == FALSE)
            {
                g_warning("Dequeue of thumbnailer->priv->handle: %d failed",
                          thumbnailer->priv->handle);
            }
        }
        thumbnailer->priv->handle = 0;
    }

    if(g_slist_find(thumbnailer->priv->queue, file) != NULL) {
            thumbnailer->priv->queue = g_slist_remove_all(
                                                    thumbnailer->priv->queue,
                                                    file);
    }

    thumbnailer->priv->request_timer_id = g_timeout_add_full(
                        G_PRIORITY_LOW,
                        300,
                        (GSourceFunc)xfdesktop_thumbnailer_queue_request_timer,
                        thumbnailer,
                        NULL);
}

static gboolean
xfdesktop_thumbnailer_queue_request_timer(XfdesktopThumbnailer *thumbnailer)
{
    gchar **uris;
    gchar **mimetypes;
    GSList *iter;
    gint i = 0;
    GFile *file;
    GError *error = NULL;
    gchar *thumbnail_flavor;

    g_return_val_if_fail(XFDESKTOP_IS_THUMBNAILER(thumbnailer), FALSE);

    uris = g_new0(gchar *,
                  g_slist_length(thumbnailer->priv->queue) + 1);
    mimetypes = g_new0(gchar *,
                       g_slist_length (thumbnailer->priv->queue) + 1);

    iter = thumbnailer->priv->queue;
    while(iter) {
        if(iter->data) {
            file = g_file_new_for_path(iter->data);
            uris[i] = g_file_get_uri(file);
            mimetypes[i] = xfdesktop_get_file_mimetype(iter->data);
            g_object_unref(file);
        }
        iter = g_slist_next(iter);
        i++;
    }

    if(thumbnailer->priv->big_thumbnails == TRUE)
        thumbnail_flavor = "large";
    else
        thumbnail_flavor = "normal";

    if(thumbnailer->priv->proxy != NULL) {
        if(dbus_g_proxy_call(thumbnailer->priv->proxy,
                             "Queue",
                             &error,
                             G_TYPE_STRV, uris,
                             G_TYPE_STRV, mimetypes,
                             G_TYPE_STRING, thumbnail_flavor,
                             G_TYPE_STRING, "default",
                             G_TYPE_UINT, 0,
                             G_TYPE_INVALID,
                             G_TYPE_UINT, &thumbnailer->priv->handle,
                             G_TYPE_INVALID) == FALSE)
        {
            if(error != NULL)
                g_warning("DBUS-call failed: %s", error->message);
        }
    }

    g_free(uris);
    g_free(mimetypes);

    if(error)
        g_error_free(error);

    thumbnailer->priv->request_timer_id = 0;

    return FALSE;
}

static void
xfdesktop_thumbnailer_request_finished_dbus(DBusGProxy *proxy,
                                            gint handle,
                                            gpointer data)
{
    XfdesktopThumbnailer *thumbnailer = XFDESKTOP_THUMBNAILER(data);

    g_return_if_fail(XFDESKTOP_IS_THUMBNAILER(thumbnailer));

    thumbnailer->priv->handle = 0;
}

static void
xfdesktop_thumbnailer_thumbnail_ready_dbus(DBusGProxy *proxy,
                                           gint handle,
                                           const gchar **uri,
                                           gpointer data)
{
    XfdesktopThumbnailer *thumbnailer = XFDESKTOP_THUMBNAILER(data);
    gchar *thumbnail_location;
    GFile *file;
    GSList *iter = thumbnailer->priv->queue;
    gchar *f_uri, *f_uri_checksum, *filename;
    gchar *thumbnail_flavor;
    gint x = 0;

    g_return_if_fail(XFDESKTOP_IS_THUMBNAILER(thumbnailer));

    while(iter) {
        if((uri[x] == NULL) || (iter->data == NULL)) {
            break;
        }

        file = g_file_new_for_path(iter->data);
        f_uri = g_file_get_uri(file);

        if(strcmp (uri[x], f_uri) == 0) {
            /* The thumbnail is in the format/location
             * /homedir/.thumbnails/(normal|large)/MD5_Hash_Of_URI.png
             */
            f_uri_checksum = g_compute_checksum_for_string(G_CHECKSUM_MD5,
                                                           f_uri, strlen (f_uri));

            if(thumbnailer->priv->big_thumbnails == TRUE)
                thumbnail_flavor = "large";
            else
                thumbnail_flavor = "normal";

            filename = g_strconcat(f_uri_checksum, ".png", NULL);

            thumbnail_location = g_build_path("/", g_get_home_dir(),
                                              ".thumbnails", thumbnail_flavor,
                                              filename, NULL);

            DBG("thumbnail-ready src: %s thumbnail: %s",
                    (char*)iter->data,
                    thumbnail_location);

            g_signal_emit(G_OBJECT(thumbnailer),
                          thumbnailer_signals[THUMBNAIL_READY],
                          0,
                          iter->data,
                          thumbnail_location);

            thumbnailer->priv->queue = g_slist_remove(thumbnailer->priv->queue,
                                                      iter->data);

            iter = thumbnailer->priv->queue;
            x++;
            
            g_free(filename);
            g_free(f_uri_checksum);
        } else {
            iter = g_slist_next(iter);
        }
        
        g_object_unref(file);
        g_free(f_uri);
    }
}

/**
 * xfdesktop_thumbnailer_delete_thumbnail:
 * 
 * Tells the thumbnail service the src_file will be deleted.
 * This function should be called when the file is deleted or moved so
 * the thumbnail file doesn't take up space on the user's drive.
 */
void
xfdesktop_thumbnailer_delete_thumbnail(XfdesktopThumbnailer *thumbnailer, gchar *src_file)
{
    DBusGConnection *connection;
    gchar **uris;
    GFile *file;
    GError *error = NULL;
    static DBusGProxy *cache = NULL;

    if(!cache) {
        connection = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);
        if (connection != NULL) {
            cache = dbus_g_proxy_new_for_name(connection,
                                           "org.freedesktop.thumbnails.Cache1",
                                           "/org/freedesktop/thumbnails/Cache1",
                                           "org.freedesktop.thumbnails.Cache1");

        dbus_g_connection_unref(connection);
        }
    }

    file = g_file_new_for_path(src_file);

    if(cache) {
        uris = g_new0 (gchar *, 2);
        uris[0] = g_file_get_uri(file);
        dbus_g_proxy_call(cache, "Delete", &error, G_TYPE_STRV, uris, G_TYPE_INVALID, G_TYPE_INVALID);
        if(error != NULL) {
            g_warning("DBUS-call failed:%s", error->message);
        }
        g_free(uris);
    }

    g_object_unref(file);
    if(error)
        g_error_free(error);
}
