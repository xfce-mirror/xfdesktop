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

#include "xfdesktop-backdrop-media.h"

typedef struct {
    cairo_surface_t *surface;
} XfdesktopBackdropMediaImageData;

#ifdef ENABLE_VIDEO_BACKDROP
typedef struct {
    gchar *uri;
    XfceBackdropImageStyle style;
    GstElement *playbin;
    GtkWidget *widget;
} XfdesktopBackdropMediaVideoData;
#endif /* ENABLE_VIDEO_BACKDROP */

struct _XfdesktopBackdropMedia {
    GObject __parent__;
    
    XfdesktopBackdropMediaKind kind;
    union {
        XfdesktopBackdropMediaImageData image_data;
#ifdef ENABLE_VIDEO_BACKDROP
        XfdesktopBackdropMediaVideoData video_data;
#endif /* ENABLE_VIDEO_BACKDROP */
    };
};

G_DEFINE_FINAL_TYPE(XfdesktopBackdropMedia, xfdesktop_backdrop_media, G_TYPE_OBJECT)

static void
xfdesktop_backdrop_media_finalize(GObject *gobject) {
    XfdesktopBackdropMedia *bmedia = XFDESKTOP_BACKDROP_MEDIA(gobject);
    switch (bmedia->kind) {
        case XFDESKTOP_BACKDROP_MEDIA_KIND_IMAGE:
            cairo_surface_destroy(bmedia->image_data.surface);
            break;
#ifdef ENABLE_VIDEO_BACKDROP
        case XFDESKTOP_BACKDROP_MEDIA_KIND_VIDEO:
            g_free(bmedia->video_data.uri);
            if (bmedia->video_data.playbin != NULL) {
                gst_element_set_state(bmedia->video_data.playbin, GST_STATE_NULL);
                g_clear_object(&bmedia->video_data.widget);
                g_clear_object(&bmedia->video_data.playbin);
            }
            break;
#endif
    }
}

static void
xfdesktop_backdrop_media_class_init(XfdesktopBackdropMediaClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = xfdesktop_backdrop_media_finalize;
}

static void
xfdesktop_backdrop_media_init(XfdesktopBackdropMedia *self) {
}

XfdesktopBackdropMedia *
xfdesktop_backdrop_media_new_from_image(cairo_surface_t *image) {
    XfdesktopBackdropMedia *bmedia = g_object_new(XFDESKTOP_TYPE_BACKDROP_MEDIA, NULL);
    bmedia->kind = XFDESKTOP_BACKDROP_MEDIA_KIND_IMAGE;
    bmedia->image_data.surface = cairo_surface_reference(image);
    return bmedia;
}

XfdesktopBackdropMediaKind
xfdesktop_backdrop_media_get_kind(XfdesktopBackdropMedia *bmedia) {
    g_assert(XFDESKTOP_IS_BACKDROP_MEDIA (bmedia));
    return bmedia->kind;
}

cairo_surface_t *
xfdesktop_backdrop_media_get_image_surface(XfdesktopBackdropMedia *bmedia) {
    g_return_val_if_fail(XFDESKTOP_IS_BACKDROP_MEDIA (bmedia), NULL);
    g_return_val_if_fail(bmedia->kind == XFDESKTOP_BACKDROP_MEDIA_KIND_IMAGE, NULL);
    return bmedia->image_data.surface;
}

gboolean
xfdesktop_backdrop_media_equal(XfdesktopBackdropMedia *a, XfdesktopBackdropMedia *b) {
    if (a == NULL && b == NULL) {
        return TRUE;
    }

    if (a == NULL || b == NULL) {
        return FALSE;
    }

    XfdesktopBackdropMediaKind akind = xfdesktop_backdrop_media_get_kind(a);
    XfdesktopBackdropMediaKind bkind = xfdesktop_backdrop_media_get_kind(b);
    if (akind != bkind) {
        return FALSE;
    }

    switch (akind) {
        case XFDESKTOP_BACKDROP_MEDIA_KIND_IMAGE:
            return a->image_data.surface == b->image_data.surface;
#ifdef ENABLE_VIDEO_BACKDROP
        case XFDESKTOP_BACKDROP_MEDIA_KIND_VIDEO:
            return g_strcmp0(a->video_data.uri, b->video_data.uri) == 0 &&
                   a->video_data.style == b->video_data.style;
#endif
    }

    g_return_val_if_reached(FALSE);
}

#ifdef ENABLE_VIDEO_BACKDROP
XfdesktopBackdropMedia *
xfdesktop_backdrop_media_new_from_video_uri(const gchar *video_uri, XfceBackdropImageStyle style) {
    XfdesktopBackdropMedia *bmedia = g_object_new(XFDESKTOP_TYPE_BACKDROP_MEDIA, NULL);
    bmedia->kind = XFDESKTOP_BACKDROP_MEDIA_KIND_VIDEO;
    bmedia->video_data.uri = g_strdup(video_uri);
    bmedia->video_data.style = style;
    return bmedia;
}

const gchar *
xfdesktop_backdrop_media_get_video_uri(XfdesktopBackdropMedia *bmedia) {
    g_return_val_if_fail(XFDESKTOP_IS_BACKDROP_MEDIA (bmedia), NULL);
    g_return_val_if_fail(bmedia->kind == XFDESKTOP_BACKDROP_MEDIA_KIND_VIDEO, NULL);
    return bmedia->video_data.uri;
}

gboolean
xfdesktop_backdrop_media_video_materialize(XfdesktopBackdropMedia *bmedia, gboolean gl_enabled, gboolean *gl_status) {
    g_return_val_if_fail(XFDESKTOP_IS_BACKDROP_MEDIA (bmedia), FALSE);
    g_return_val_if_fail(bmedia->kind == XFDESKTOP_BACKDROP_MEDIA_KIND_VIDEO, FALSE);
    g_clear_object(&bmedia->video_data.widget);
    g_clear_object(&bmedia->video_data.playbin);

    bmedia->video_data.playbin = gst_element_factory_make("playbin", "playbin");
    g_return_val_if_fail(bmedia->video_data.playbin != NULL, FALSE);

     /* https://gstreamer.freedesktop.org/documentation/playback/playsink.html?gi-language=c#named-constants */
    const guint gst_flag_soft_colorbalance = 0x00000400;
    const guint gst_flag_deinterlace = 0x00000200;
    const guint gst_flag_buffering = 0x00000100;
    const guint gst_flag_video = 0x00000001;
    const guint gst_flags = gst_flag_soft_colorbalance |
                            gst_flag_deinterlace |
                            gst_flag_buffering |
                            gst_flag_video;

    gboolean force_aspect_ratio = bmedia->video_data.style != XFCE_BACKDROP_IMAGE_STRETCHED;
    g_object_set(bmedia->video_data.playbin,
                 "flags", gst_flags,
                 "force-aspect-ratio", force_aspect_ratio,
                 NULL);

    GstElement *videosink = NULL, *gtkglsink = NULL;

    if (gl_enabled) {
        videosink = gst_element_factory_make("glsinkbin", "glsinkbin");
        gtkglsink = gst_element_factory_make("gtkglsink", "gtkglsink");

        gboolean nogl_fallback = gtkglsink == NULL || videosink == NULL;
        if (nogl_fallback) {
            g_printerr("Failed to create gstreamer gtkglsink/glsinkbin\n");
            g_clear_object(&gtkglsink);
            g_clear_object(&videosink);
            videosink = gst_element_factory_make("gtksink", "gtksink");
            g_object_get(videosink, "widget", &bmedia->video_data.widget, NULL);
            *gl_status = FALSE;
        } else {
            g_object_set(videosink, "sink", gtkglsink, NULL);
            g_object_get(gtkglsink, "widget", &bmedia->video_data.widget, NULL);
            *gl_status = TRUE;
        }
    } else {
        videosink = gst_element_factory_make("gtksink", "gtksink");
        g_object_get(videosink, "widget", &bmedia->video_data.widget, NULL);
        *gl_status = FALSE;
    }

    if (videosink == NULL) {
        g_clear_object(&bmedia->video_data.widget);
        g_clear_object(&gtkglsink);
        g_clear_object(&videosink);
        g_clear_object(&bmedia->video_data.playbin);
        g_printerr("Failed to create gstreamer videosink\n");
        return FALSE;
    } else {
        g_object_set(bmedia->video_data.playbin,
                     "uri", bmedia->video_data.uri,
                     "video-sink", videosink,
                     NULL);
        return TRUE;
    }
}

GtkWidget *
xfdesktop_backdrop_media_get_video_widget(XfdesktopBackdropMedia *bmedia) {
    g_return_val_if_fail(XFDESKTOP_IS_BACKDROP_MEDIA (bmedia), NULL);
    g_return_val_if_fail(bmedia->kind == XFDESKTOP_BACKDROP_MEDIA_KIND_VIDEO, NULL);
    g_return_val_if_fail(bmedia->video_data.playbin != NULL, NULL);

    return bmedia->video_data.widget;
}

GstElement *
xfdesktop_backdrop_media_get_video_playbin(XfdesktopBackdropMedia *bmedia) {
    g_return_val_if_fail(XFDESKTOP_IS_BACKDROP_MEDIA (bmedia), NULL);
    g_return_val_if_fail(bmedia->kind == XFDESKTOP_BACKDROP_MEDIA_KIND_VIDEO, NULL);
    g_return_val_if_fail(bmedia->video_data.playbin != NULL, NULL);

    return bmedia->video_data.playbin;
}
#endif /* ENABLE_VIDEO_BACKDROP */
