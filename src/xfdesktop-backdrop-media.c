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

struct _XfdesktopBackdropMedia {
    GObject __parent__;
    
    XfdesktopBackdropMediaKind kind;
    union {
        cairo_surface_t *image_surface;
        gchar *video_uri;
    };
};

G_DEFINE_FINAL_TYPE(XfdesktopBackdropMedia, xfdesktop_backdrop_media, G_TYPE_OBJECT)

static void
xfdesktop_backdrop_media_finalize(GObject *gobject) {
    XfdesktopBackdropMedia *bmedia = XFDESKTOP_BACKDROP_MEDIA(gobject);
    
    switch (bmedia->kind) {
    case XFDESKTOP_BACKDROP_MEDIA_KIND_IMAGE:
        cairo_surface_destroy(bmedia->image_surface);
        break;
#ifdef ENABLE_VIDEO_BACKDROP
    case XFDESKTOP_BACKDROP_MEDIA_KIND_VIDEO:
        g_free(bmedia->video_uri);
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
    bmedia->image_surface = cairo_surface_reference(image);
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
    return bmedia->image_surface;
}

gboolean
xfdesktop_backdrop_media_equal(XfdesktopBackdropMedia *a,
                               XfdesktopBackdropMedia *b) {
    XfdesktopBackdropMediaKind akind, bkind;
    
    if (a == NULL && b == NULL)
        return TRUE;

    if (a == NULL || b == NULL)
        return FALSE;

    akind = xfdesktop_backdrop_media_get_kind(a);
    bkind = xfdesktop_backdrop_media_get_kind(b);
    if (akind != bkind)
        return FALSE;

    switch (akind) {
    case XFDESKTOP_BACKDROP_MEDIA_KIND_IMAGE:
        return a->image_surface == b->image_surface;
#ifdef ENABLE_VIDEO_BACKDROP
    case XFDESKTOP_BACKDROP_MEDIA_KIND_VIDEO:
        return g_strcmp0(a->video_uri, b->video_uri) == 0;
#endif
    }

    g_return_val_if_reached(FALSE);
}

#ifdef ENABLE_VIDEO_BACKDROP
XfdesktopBackdropMedia *
xfdesktop_backdrop_media_new_from_video_uri(const gchar *video_uri) {
    XfdesktopBackdropMedia *bmedia = g_object_new(XFDESKTOP_TYPE_BACKDROP_MEDIA, NULL);
    bmedia->kind = XFDESKTOP_BACKDROP_MEDIA_KIND_VIDEO;
    bmedia->video_uri = g_strdup(video_uri);    
    return bmedia;
}

const gchar *
xfdesktop_backdrop_media_get_video_uri(XfdesktopBackdropMedia *bmedia) {
    g_return_val_if_fail(XFDESKTOP_IS_BACKDROP_MEDIA (bmedia), NULL);
    g_return_val_if_fail(bmedia->kind == XFDESKTOP_BACKDROP_MEDIA_KIND_VIDEO, NULL);
    return bmedia->video_uri;
}
#endif /* ENABLE_VIDEO_BACKDROP */
