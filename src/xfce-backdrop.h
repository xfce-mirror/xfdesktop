/*
 *  backdrop - xfce4's desktop manager
 *
 *  Copyright (c) 2004 Brian Tarricone, <bjt23@cornell.edu>
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
 */

#ifndef _XFCE_BACKDROP_H_
#define _XFCE_BACKDROP_H_

#include <glib-object.h>
#include <gdk/gdk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

#define XFCE_TYPE_BACKDROP              (xfce_backdrop_get_type())
#define XFCE_BACKDROP(object)           (G_TYPE_CHECK_INSTANCE_CAST((object), XFCE_TYPE_BACKDROP, XfceBackdrop))
#define XFCE_BACKDROP_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), XFCE_TYPE_BACKDROP, XfceBackdropClass))
#define XFCE_IS_BACKDROP(object)        (G_TYPE_CHECK_INSTANCE_TYPE((object), XFCE_TYPE_BACKDROP))
#define XFCE_IS_BACKDROP_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), XFCE_TYPE_BACKDROP))
#define XFCE_BACKDROP_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), XFCE_TYPE_BACKDROP, XfceBackdropClass))

typedef struct _XfceBackdrop XfceBackdrop;
typedef struct _XfceBackdropClass XfceBackdropClass;
typedef struct _XfceBackdropPriv XfceBackdropPriv;

typedef enum
{
    XFCE_BACKDROP_IMAGE_AUTO = 0,
    XFCE_BACKDROP_IMAGE_CENTERED,
    XFCE_BACKDROP_IMAGE_TILED,
    XFCE_BACKDROP_IMAGE_STRETCHED,
    XFCE_BACKDROP_IMAGE_SCALED,
    XFCE_BACKDROP_IMAGE_ZOOMED,
} XfceBackdropImageStyle;

typedef enum
{
    XFCE_BACKDROP_COLOR_SOLID = 0,
    XFCE_BACKDROP_COLOR_HORIZ_GRADIENT,
    XFCE_BACKDROP_COLOR_VERT_GRADIENT,
    XFCE_BACKDROP_COLOR_TRANSPARENT,
} XfceBackdropColorStyle;

struct _XfceBackdrop
{
    GObject gobject;
    
    /*< private >*/
    XfceBackdropPriv *priv;
};

struct _XfceBackdropClass
{
    GObjectClass parent_class;
    
    /*< signals >*/
    void (*changed)(XfceBackdrop *backdrop);
    void (*cycle)(XfceBackdrop *backdrop);
};

GType xfce_backdrop_get_type             (void) G_GNUC_CONST;

XfceBackdrop *xfce_backdrop_new          (GdkVisual *visual);

XfceBackdrop *xfce_backdrop_new_with_size(GdkVisual *visual,
                                          gint width,
                                          gint height);

void xfce_backdrop_set_size              (XfceBackdrop *backdrop,
                                          gint width,
                                          gint height);

void xfce_backdrop_set_color_style       (XfceBackdrop *backdrop,
                                          XfceBackdropColorStyle style);
XfceBackdropColorStyle xfce_backdrop_get_color_style
                                         (XfceBackdrop *backdrop);

void xfce_backdrop_set_first_color       (XfceBackdrop *backdrop,
                                          const GdkColor *color);
void xfce_backdrop_get_first_color       (XfceBackdrop *backdrop,
                                          GdkColor *color);

void xfce_backdrop_set_second_color      (XfceBackdrop *backdrop,
                                          const GdkColor *color);
void xfce_backdrop_get_second_color      (XfceBackdrop *backdrop,
                                          GdkColor *color);

void xfce_backdrop_set_show_image        (XfceBackdrop *backdrop,
                                          gboolean show_image);
gboolean xfce_backdrop_get_show_image    (XfceBackdrop *backdrop);

void xfce_backdrop_set_image_style       (XfceBackdrop *backdrop,
                                          XfceBackdropImageStyle style);
XfceBackdropImageStyle xfce_backdrop_get_image_style
                                         (XfceBackdrop *backdrop);

void xfce_backdrop_set_image_filename    (XfceBackdrop *backdrop,
                                          const gchar *filename);
G_CONST_RETURN gchar *xfce_backdrop_get_image_filename
                                         (XfceBackdrop *backdrop);

void xfce_backdrop_set_list              (XfceBackdrop *backdrop,
                                          gchar *backdrop_list);
G_CONST_RETURN gchar *xfce_backdrop_get_list
                                         (XfceBackdrop *backdrop);

void xfce_backdrop_set_brightness        (XfceBackdrop *backdrop,
                                          gint brightness);
gint xfce_backdrop_get_brightness        (XfceBackdrop *backdrop);

void xfce_backdrop_set_saturation        (XfceBackdrop *backdrop,
                                          gdouble saturation);
gdouble xfce_backdrop_get_saturation     (XfceBackdrop *backdrop);

void xfce_backdrop_set_cycle_backdrop    (XfceBackdrop *backdrop,
                                          gboolean cycle_backdrop);
gboolean xfce_backdrop_get_cycle_backdrop(XfceBackdrop *backdrop);

void xfce_backdrop_set_cycle_timer       (XfceBackdrop *backdrop,
                                          guint cycle_timer);
guint xfce_backdrop_get_cycle_timer      (XfceBackdrop *backdrop);

GdkPixbuf *xfce_backdrop_get_pixbuf      (XfceBackdrop *backdrop);

G_END_DECLS

#endif
