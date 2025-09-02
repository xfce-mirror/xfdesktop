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

#ifndef __XFDESKTOP_BACKDROP_MEDIA_H__
#define __XFDESKTOP_BACKDROP_MEDIA_H__

#include <gtk/gtk.h>
#include <cairo.h>

#ifdef ENABLE_VIDEO_BACKDROP
#include <gst/gst.h>
#endif /* ENABLE_VIDEO_BACKDROP */

G_BEGIN_DECLS

typedef enum {
    XFDESKTOP_BACKDROP_MEDIA_KIND_IMAGE,
#ifdef ENABLE_VIDEO_BACKDROP
    XFDESKTOP_BACKDROP_MEDIA_KIND_VIDEO
#endif /* ENABLE_VIDEO_BACKDROP */
} XfdesktopBackdropMediaKind;

#define XFDESKTOP_TYPE_BACKDROP_MEDIA (xfdesktop_backdrop_media_get_type())
G_DECLARE_FINAL_TYPE (XfdesktopBackdropMedia, xfdesktop_backdrop_media, XFDESKTOP, BACKDROP_MEDIA, GObject)

XfdesktopBackdropMedia *xfdesktop_backdrop_media_new_from_image(cairo_surface_t *image_surface);

XfdesktopBackdropMediaKind xfdesktop_backdrop_media_get_kind(XfdesktopBackdropMedia *bmedia);

cairo_surface_t *xfdesktop_backdrop_media_get_image_surface(XfdesktopBackdropMedia *bmedia);

gboolean xfdesktop_backdrop_media_equal(XfdesktopBackdropMedia *a,
                                        XfdesktopBackdropMedia *b);

#ifdef ENABLE_VIDEO_BACKDROP
XfdesktopBackdropMedia *xfdesktop_backdrop_media_new_from_video_uri(const gchar *video_uri);

const gchar *xfdesktop_backdrop_media_get_video_uri(XfdesktopBackdropMedia *bmedia);

GtkWidget *xfdesktop_backdrop_media_get_video_widget(XfdesktopBackdropMedia *bmedia);

GstElement *xfdesktop_backdrop_media_get_video_playbin(XfdesktopBackdropMedia *bmedia);
#endif /* ENABLE_VIDEO_BACKDROP */

G_END_DECLS

#endif /* __XFDESKTOP_BACKDROP_MEDIA_H__ */
