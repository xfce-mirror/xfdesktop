/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright(c) 2006 Brian Tarricone, <brian@tarricone.org>
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
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 *  xfdesktop-thumbnailer is based on thumbnailer code from Ristretto
 *  Copyright (c) Stephan Arts 2009-2011 <stephan@xfce.org>
 *
 *  Thumbnailer Spec
 *  https://wiki.gnome.org/action/show/DraftSpecs/ThumbnailerSpec
 *  Thumbnail Managing Standard
 *  https://specifications.freedesktop.org/thumbnail-spec/thumbnail-spec-latest.html
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <gio/gio.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>

#include "tumbler.h"
#include "xfdesktop-common.h"
#include "xfdesktop-marshal.h"
#include "xfdesktop-thumbnailer.h"

struct _XfdesktopThumbnailer {
    GObject parent_instance;

    TumblerThumbnailer1 *proxy;

    GSList *queue;
    gchar **supported_mimetypes;
    gboolean big_thumbnails;
    guint handle;

    guint request_timer_id;
};

static void xfdesktop_thumbnailer_dispose(GObject *object);
static void xfdesktop_thumbnailer_finalize(GObject *object);

static void xfdesktop_thumbnailer_request_finished_dbus(TumblerThumbnailer1 *proxy,
                                                        guint arg_handle,
                                                        gpointer data);

static void xfdesktop_thumbnailer_thumbnail_ready_dbus(TumblerThumbnailer1 *proxy,
                                                       guint handle,
                                                       const gchar *const *uri,
                                                       gpointer data);

static gboolean xfdesktop_thumbnailer_queue_request_timer(gpointer user_data);

static XfdesktopThumbnailer *thumbnailer_object = NULL;

enum
{
    THUMBNAIL_READY,
    LAST_SIGNAL,
};

static guint thumbnailer_signals[LAST_SIGNAL] = { 0, };


G_DEFINE_TYPE(XfdesktopThumbnailer, xfdesktop_thumbnailer, G_TYPE_OBJECT);


static void
xfdesktop_thumbnailer_class_init(XfdesktopThumbnailerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->dispose = xfdesktop_thumbnailer_dispose;
    object_class->finalize = xfdesktop_thumbnailer_finalize;

    thumbnailer_signals[THUMBNAIL_READY] = g_signal_new (
                        "thumbnail-ready",
                        G_OBJECT_CLASS_TYPE (object_class),
                        G_SIGNAL_RUN_LAST,
                        0,
                        NULL, NULL,
                        xfdesktop_marshal_VOID__STRING_STRING,
                        G_TYPE_NONE, 2,
                        G_TYPE_STRING, G_TYPE_STRING);
}

static void
xfdesktop_thumbnailer_init(XfdesktopThumbnailer *thumbnailer)
{
    GDBusConnection *connection;

    connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);

    if(connection) {
        thumbnailer->proxy = tumbler_thumbnailer1_proxy_new_sync(connection,
                                                                 G_DBUS_PROXY_FLAGS_NONE,
                                                                 "org.freedesktop.thumbnails.Thumbnailer1",
                                                                 "/org/freedesktop/thumbnails/Thumbnailer1",
                                                                 NULL,
                                                                 NULL);

        if(thumbnailer->proxy) {
            gchar **supported_uris = NULL;
            gchar **supported_flavors = NULL;


            g_signal_connect(thumbnailer->proxy,
                             "finished",
                             G_CALLBACK (xfdesktop_thumbnailer_request_finished_dbus),
                             thumbnailer);
            g_signal_connect(thumbnailer->proxy,
                             "ready",
                             G_CALLBACK(xfdesktop_thumbnailer_thumbnail_ready_dbus),
                             thumbnailer);

            tumbler_thumbnailer1_call_get_supported_sync(thumbnailer->proxy,
                                                         &supported_uris,
                                                         &thumbnailer->supported_mimetypes,
                                                         NULL,
                                                         NULL);

            tumbler_thumbnailer1_call_get_flavors_sync(thumbnailer->proxy,
                                                       &supported_flavors,
                                                       NULL,
                                                       NULL);

            if(supported_flavors != NULL) {
                gint n;
                for(n = 0; supported_flavors[n] != NULL; ++n) {
                    if(g_strcmp0(supported_flavors[n], "large")) {
                        thumbnailer->big_thumbnails = TRUE;
                    }
                }
            } else {
                thumbnailer->big_thumbnails = FALSE;
                g_warning("Thumbnailer failed calling GetFlavors");
            }

            g_strfreev(supported_flavors);
            g_strfreev(supported_uris);
        }

        g_object_unref(connection);
    }
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

    if (thumbnailer->request_timer_id != 0) {
        g_source_remove(thumbnailer->request_timer_id);
        thumbnailer->request_timer_id = 0;
    }

    g_clear_object(&thumbnailer->proxy);

    if (thumbnailer == thumbnailer_object) {
        thumbnailer_object = NULL;
    }

    G_OBJECT_CLASS(xfdesktop_thumbnailer_parent_class)->dispose(object);
}

/**
 * xfdesktop_thumbnailer_finalize:
 * @object:
 *
 */
static void
xfdesktop_thumbnailer_finalize(GObject *object)
{
    XfdesktopThumbnailer *thumbnailer = XFDESKTOP_THUMBNAILER(object);

    g_slist_free(thumbnailer->queue);

    if (thumbnailer->supported_mimetypes != NULL) {
        g_strfreev(thumbnailer->supported_mimetypes);
    }

    G_OBJECT_CLASS(xfdesktop_thumbnailer_parent_class)->finalize(object);
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

gboolean xfdesktop_thumbnailer_service_available(XfdesktopThumbnailer *thumbnailer)
{
    g_return_val_if_fail(XFDESKTOP_IS_THUMBNAILER(thumbnailer), FALSE);
    return thumbnailer->proxy != NULL;
}

gboolean
xfdesktop_thumbnailer_is_supported(XfdesktopThumbnailer *thumbnailer,
                                   gchar *filename)
{
    guint        n;
    gchar       *mime_type = NULL;

    g_return_val_if_fail(XFDESKTOP_IS_THUMBNAILER(thumbnailer), FALSE);
    g_return_val_if_fail(filename != NULL, FALSE);

    GFile *file = g_file_new_for_path(filename);
    mime_type = xfdesktop_get_file_mimetype(file);
    g_object_unref(file);

    if(mime_type == NULL) {
        XF_DEBUG("File %s has no mime type", filename);
        return FALSE;
    }

    if (thumbnailer->supported_mimetypes != NULL) {
        for(n = 0; thumbnailer->supported_mimetypes[n] != NULL; ++n) {
            if(g_content_type_is_a (mime_type, thumbnailer->supported_mimetypes[n])) {
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
        XF_DEBUG("file: %s not supported", file);
        return FALSE;
    }
    if (thumbnailer->request_timer_id != 0) {
        g_source_remove(thumbnailer->request_timer_id);

        if (thumbnailer->handle != 0 && thumbnailer->proxy != NULL) {
            if (!tumbler_thumbnailer1_call_dequeue_sync(thumbnailer->proxy,
                                                        thumbnailer->handle,
                                                        NULL,
                                                        NULL))
            {
                /* If this fails it usually means there's a thumbnail already
                 * being processed, no big deal */
                XF_DEBUG("Dequeue of thumbnailer->handle: %d failed",
                         thumbnailer->handle);
            }

            thumbnailer->handle = 0;
        }
    }

    if (g_slist_find(thumbnailer->queue, file) == NULL) {
        thumbnailer->queue = g_slist_prepend(thumbnailer->queue, g_strdup(file));
    }

    thumbnailer->request_timer_id = g_timeout_add_full(G_PRIORITY_LOW,
                                                       300,
                                                       xfdesktop_thumbnailer_queue_request_timer,
                                                       thumbnailer,
                                                       NULL);

    return TRUE;
}

static void
xfdesktop_thumbnailer_dequeue_foreach(gpointer data, gpointer user_data)
{
    xfdesktop_thumbnailer_dequeue_thumbnail(user_data, data);
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
    GSList *item;

    g_return_if_fail(XFDESKTOP_IS_THUMBNAILER(thumbnailer));
    g_return_if_fail(file != NULL);

    if (thumbnailer->request_timer_id != 0) {
        g_source_remove(thumbnailer->request_timer_id);

        if (thumbnailer->handle != 0 && thumbnailer->proxy != NULL) {
            if (!tumbler_thumbnailer1_call_dequeue_sync(thumbnailer->proxy,
                                                        thumbnailer->handle,
                                                        NULL,
                                                        NULL))
            {
                /* If this fails it usually means there's a thumbnail already
                 * being processed, no big deal */
                XF_DEBUG("Dequeue of thumbnailer->handle: %d failed",
                         thumbnailer->handle);
            }
        }
        thumbnailer->handle = 0;
    }

    item = g_slist_find(thumbnailer->queue, file);
    if(item != NULL) {
        g_free(item->data);
        thumbnailer->queue = g_slist_remove(thumbnailer->queue,
                                                  file);
    }

    thumbnailer->request_timer_id = g_timeout_add_full(G_PRIORITY_LOW,
                                                       300,
                                                       xfdesktop_thumbnailer_queue_request_timer,
                                                       thumbnailer,
                                                       NULL);
}

void xfdesktop_thumbnailer_dequeue_all_thumbnails(XfdesktopThumbnailer *thumbnailer)
{
    g_return_if_fail(XFDESKTOP_IS_THUMBNAILER(thumbnailer));

    g_slist_foreach(thumbnailer->queue, (GFunc)xfdesktop_thumbnailer_dequeue_foreach, thumbnailer);
}

static gboolean
xfdesktop_thumbnailer_queue_request_timer(gpointer user_data)
{
    XfdesktopThumbnailer *thumbnailer = user_data;
    gchar **uris;
    gchar **mimetypes;
    GSList *iter;
    gint i = 0;
    GFile *file;
    GError *error = NULL;
    gchar *thumbnail_flavor;

    g_return_val_if_fail(XFDESKTOP_IS_THUMBNAILER(thumbnailer), FALSE);

    uris = g_new0(gchar *, g_slist_length(thumbnailer->queue) + 1);
    mimetypes = g_new0(gchar *, g_slist_length (thumbnailer->queue) + 1);

    iter = thumbnailer->queue;
    while(iter) {
        if(iter->data) {
            file = g_file_new_for_path(iter->data);
            uris[i] = g_file_get_uri(file);
            mimetypes[i] = xfdesktop_get_file_mimetype(file);

            g_object_unref(file);
        }
        iter = g_slist_next(iter);
        i++;
    }

    if (thumbnailer->big_thumbnails) {
        thumbnail_flavor = "large";
    } else {
        thumbnail_flavor = "normal";
    }

    if (thumbnailer->proxy != NULL) {
        if (!tumbler_thumbnailer1_call_queue_sync(thumbnailer->proxy,
                                                  (const gchar * const*)uris,
                                                  (const gchar * const*)mimetypes,
                                                  thumbnail_flavor,
                                                  "default",
                                                  0,
                                                  &thumbnailer->handle,
                                                  NULL,
                                                  &error))
        {
            if(error != NULL)
                g_warning("DBUS-call failed: %s", error->message);
        }
    }

    /* Free the memory */
    i = 0;
    iter = thumbnailer->queue;
    while(iter) {
        if(iter->data) {
            g_free(uris[i]);
            g_free(mimetypes[i]);
        }
        iter = g_slist_next(iter);
        i++;
    }

    g_free(uris);
    g_free(mimetypes);
    g_clear_error(&error);

    thumbnailer->request_timer_id = 0;

    return FALSE;
}

static void
xfdesktop_thumbnailer_request_finished_dbus(TumblerThumbnailer1 *proxy,
                                            guint arg_handle,
                                            gpointer data)
{
    XfdesktopThumbnailer *thumbnailer = XFDESKTOP_THUMBNAILER(data);

    g_return_if_fail(XFDESKTOP_IS_THUMBNAILER(thumbnailer));

    thumbnailer->handle = 0;
}

static void
xfdesktop_thumbnailer_thumbnail_ready_dbus(TumblerThumbnailer1 *proxy,
                                           guint handle,
                                           const gchar *const *uri,
                                           gpointer data)
{
    XfdesktopThumbnailer *thumbnailer = XFDESKTOP_THUMBNAILER(data);
    gchar *thumbnail_location;
    GFile *file;
    GSList *iter = thumbnailer->queue;
    gchar *f_uri, *f_uri_checksum, *filename, *temp;
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
             * $XDG_CACHE_HOME/thumbnails/(nromal|large)/MD5_Hash_Of_URI.png
             * for version 0.8.0 if XDG_CACHE_HOME is defined, otherwise
             * /homedir/.thumbnails/(normal|large)/MD5_Hash_Of_URI.png
             * will be used, which is also always used for versions prior
             * to 0.7.0.
             */
            f_uri_checksum = g_compute_checksum_for_string(G_CHECKSUM_MD5,
                                                           f_uri, strlen (f_uri));

            if (thumbnailer->big_thumbnails) {
                thumbnail_flavor = "large";
            } else {
                thumbnail_flavor = "normal";
            }

            filename = g_strconcat(f_uri_checksum, ".png", NULL);

            /* build and check if the thumbnail is in the new location */
            thumbnail_location = g_build_path("/", g_get_user_cache_dir(),
                                              "thumbnails", thumbnail_flavor,
                                              filename, NULL);

            if(!g_file_test(thumbnail_location, G_FILE_TEST_EXISTS)) {
                /* Fallback to old version */
                g_free(thumbnail_location);

                thumbnail_location = g_build_path("/", g_get_home_dir(),
                                                  ".thumbnails", thumbnail_flavor,
                                                  filename, NULL);
            }

            XF_DEBUG("thumbnail-ready src: %s thumbnail: %s",
                     (char*)iter->data,
                     thumbnail_location);

            if(g_file_test(thumbnail_location, G_FILE_TEST_EXISTS)) {
                g_signal_emit(G_OBJECT(thumbnailer),
                              thumbnailer_signals[THUMBNAIL_READY],
                              0,
                              iter->data,
                              thumbnail_location);
            }

            temp = iter->data;
            thumbnailer->queue = g_slist_remove(thumbnailer->queue, temp);

            iter = thumbnailer->queue;
            x++;

            g_free(filename);
            g_free(f_uri_checksum);
            g_free(thumbnail_location);
            g_free(temp);
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
    GDBusConnection *connection;
    GVariantBuilder builder;
    GFile *file;
    GError *error = NULL;
    static GDBusProxy *cache = NULL;

    if(!cache) {
        connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
        if (connection != NULL) {
            cache = g_dbus_proxy_new_sync(connection,
                                          G_DBUS_PROXY_FLAGS_NONE,
                                          NULL,
                                          "org.freedesktop.thumbnails.Cache1",
                                          "/org/freedesktop/thumbnails/Cache1",
                                          "org.freedesktop.thumbnails.Cache1",
                                          NULL,
                                          NULL);

        g_object_unref(connection);
        }
    }

    file = g_file_new_for_path(src_file);

    if(cache) {
        g_variant_builder_init(&builder, G_VARIANT_TYPE ("as"));
        g_variant_builder_add(&builder, "s", g_file_get_uri(file));
        g_dbus_proxy_call_sync(cache,
                               "Delete",
                               g_variant_new("(as)", &builder),
                               G_DBUS_CALL_FLAGS_NONE,
                               -1,
                               NULL,
                               &error);
        if(error != NULL) {
            g_warning("DBUS-call failed:%s", error->message);
        }
    }

    g_object_unref(file);
    g_clear_error(&error);
}
