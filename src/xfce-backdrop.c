/*
 *  xfdesktop - xfce4's desktop manager
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
 *
 *  Random portions taken from or inspired by the original xfdesktop for xfce4:
 *     Copyright (C) 2002-2003 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *     Copyright (C) 2003 Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <glib.h>
#include <gdk/gdk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixdata.h>

#include <libxfce4util/libxfce4util.h> /* for DBG/TRACE */

#include "xfce-backdrop.h"
#include "xfce-desktop-enum-types.h"
#include "xfdesktop-common.h"  /* for DEFAULT_BACKDROP */

#ifndef abs
#define abs(x)  ( (x) < 0 ? -(x) : (x) )
#endif

static void xfce_backdrop_finalize(GObject *object);
static void xfce_backdrop_set_property(GObject *object,
                                       guint property_id,
                                       const GValue *value,
                                       GParamSpec *pspec);
static void xfce_backdrop_get_property(GObject *object,
                                       guint property_id,
                                       GValue *value,
                                       GParamSpec *pspec);
static gboolean xfce_backdrop_timer(XfceBackdrop *backdrop);

struct _XfceBackdropPriv
{
    gint width, height;
    gint bpp;
    
    XfceBackdropColorStyle color_style;
    GdkColor color1;
    GdkColor color2;
    
    gboolean show_image;
    XfceBackdropImageStyle image_style;
    gchar *image_path;
    gchar *backdrop_list;
    
    gint brightness;
    gdouble saturation;

    gboolean cycle_backdrop;
    guint cycle_timer;
    guint cycle_timer_id;
};

enum
{
    BACKDROP_CHANGED,
    BACKDROP_CYCLE,
    LAST_SIGNAL,
};

enum
{
    PROP_0 = 0,
    PROP_COLOR_STYLE,
    PROP_COLOR1,
    PROP_COLOR2,
    PROP_SHOW_IMAGE,
    PROP_IMAGE_STYLE,
    PROP_IMAGE_FILENAME,
    PROP_BRIGHTNESS,
    PROP_SATURATION,
    PROP_BACKDROP_CYCLE_ENABLE,
    PROP_BACKDROP_CYCLE_TIMER,
};

static guint backdrop_signals[LAST_SIGNAL] = { 0, };

/* helper functions */

static GdkPixbuf *
adjust_brightness(GdkPixbuf *src, gint amount)
{
    GdkPixbuf *newpix;
    GdkPixdata pdata;
    gboolean has_alpha = FALSE;
    gint i, len;
    GError *err = NULL;
    
    g_return_val_if_fail(src != NULL, NULL);
    if(amount == 0)
        return src;
    
    gdk_pixdata_from_pixbuf(&pdata, src, FALSE);
    has_alpha = (pdata.pixdata_type & GDK_PIXDATA_COLOR_TYPE_RGBA);
    if(pdata.length < 1)
        len = pdata.width * pdata.height * (has_alpha?4:3);
    else
        len = pdata.length - GDK_PIXDATA_HEADER_LENGTH;
    
    for(i = 0; i < len; i++) {
        gshort scaled;
        
        if(has_alpha && (i+1)%4)
            continue;
        
        scaled = pdata.pixel_data[i] + amount;
        if(scaled > 255)
            scaled = 255;
        if(scaled < 0)
            scaled = 0;
        pdata.pixel_data[i] = scaled;
    }
    
    newpix = gdk_pixbuf_from_pixdata(&pdata, TRUE, &err);
    if(!newpix) {
        g_warning("%s: Unable to modify image brightness: %s", PACKAGE,
                err->message);
        g_error_free(err);
        return src;
    }
    g_object_unref(G_OBJECT(src));
    
    return newpix;
}

static GdkPixbuf *
create_solid(GdkColor *color,
             gint width,
             gint height,
             gboolean has_alpha,
             gint alpha)
{
    GdkPixbuf *pix;
    guint32 rgba;
    
    pix = gdk_pixbuf_new(GDK_COLORSPACE_RGB, has_alpha, 8, width, height);
    
    rgba = ((((color->red & 0xff00) << 8) | ((color->green & 0xff00))
            | ((color->blue & 0xff00) >> 8)) << 8) | alpha;
    
    gdk_pixbuf_fill(pix, rgba);
    
    return pix;
}

static GdkPixbuf *
create_gradient(GdkColor *color1, GdkColor *color2, gint width, gint height,
        XfceBackdropColorStyle style)
{
    GdkPixbuf *pix;
    gint i, j;
    GdkPixdata pixdata;
    guint8 rgb[3];
    GError *err = NULL;
    
    g_return_val_if_fail(color1 != NULL && color2 != NULL, NULL);
    g_return_val_if_fail(width > 0 && height > 0, NULL);
    g_return_val_if_fail(style == XFCE_BACKDROP_COLOR_HORIZ_GRADIENT
            || style == XFCE_BACKDROP_COLOR_VERT_GRADIENT, NULL);
    
    pixdata.magic = GDK_PIXBUF_MAGIC_NUMBER;
    pixdata.length = GDK_PIXDATA_HEADER_LENGTH + (width * height * 3);
    pixdata.pixdata_type = GDK_PIXDATA_COLOR_TYPE_RGB
            | GDK_PIXDATA_SAMPLE_WIDTH_8 | GDK_PIXDATA_ENCODING_RAW;
    pixdata.rowstride = width * 3;
    pixdata.width = width;
    pixdata.height = height;
    pixdata.pixel_data = g_malloc(width * height * 3);

    if(style == XFCE_BACKDROP_COLOR_HORIZ_GRADIENT) {
        for(i = 0; i < width; i++) {
            rgb[0] = (color1->red + (i * (color2->red - color1->red) / width)) >> 8;
            rgb[1] = (color1->green + (i * (color2->green - color1->green) / width)) >> 8;
            rgb[2] = (color1->blue + (i * (color2->blue - color1->blue) / width)) >> 8;
            memcpy(pixdata.pixel_data+(i*3), rgb, 3);
        }
        
        for(i = 1; i < height; i++) {
            memcpy(pixdata.pixel_data+(i*pixdata.rowstride),
                    pixdata.pixel_data, pixdata.rowstride);
        }
    } else if(XFCE_BACKDROP_COLOR_TRANSPARENT == style)
        memset(pixdata.pixel_data, 0x0, width * height * 3);
    else {
        for(i = 0; i < height; i++) {
            rgb[0] = (color1->red + (i * (color2->red - color1->red) / height)) >> 8;
            rgb[1] = (color1->green + (i * (color2->green - color1->green) / height)) >> 8;
            rgb[2] = (color1->blue + (i * (color2->blue - color1->blue) / height)) >> 8;
            for(j = 0; j < width; j++)
                memcpy(pixdata.pixel_data+(i*pixdata.rowstride)+(j*3), rgb, 3);
        }
    }
    
    pix = gdk_pixbuf_from_pixdata(&pixdata, TRUE, &err);
    if(!pix) {
        g_warning("%s: Unable to create color gradient: %s\n", PACKAGE,
                err->message);
        g_error_free(err);
    }
    
    g_free(pixdata.pixel_data);
    
    return pix;
}

/* gobject-related functions */


G_DEFINE_TYPE(XfceBackdrop, xfce_backdrop, G_TYPE_OBJECT)


static void
xfce_backdrop_class_init(XfceBackdropClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;
    
    g_type_class_add_private(klass, sizeof(XfceBackdropPriv));
    
    gobject_class->finalize = xfce_backdrop_finalize;
    gobject_class->set_property = xfce_backdrop_set_property;
    gobject_class->get_property = xfce_backdrop_get_property;
    
    backdrop_signals[BACKDROP_CHANGED] = g_signal_new("changed",
            G_OBJECT_CLASS_TYPE(gobject_class), G_SIGNAL_RUN_FIRST,
            G_STRUCT_OFFSET(XfceBackdropClass, changed), NULL, NULL,
            g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
    backdrop_signals[BACKDROP_CYCLE] = g_signal_new("cycle",
            G_OBJECT_CLASS_TYPE(gobject_class), G_SIGNAL_RUN_FIRST,
            G_STRUCT_OFFSET(XfceBackdropClass, cycle), NULL, NULL,
            g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

#define XFDESKTOP_PARAM_FLAGS  (G_PARAM_READWRITE \
                                | G_PARAM_CONSTRUCT \
                                | G_PARAM_STATIC_NAME \
                                | G_PARAM_STATIC_NICK \
                                | G_PARAM_STATIC_BLURB)

    g_object_class_install_property(gobject_class, PROP_COLOR_STYLE,
                                    g_param_spec_enum("color-style",
                                                      "color style",
                                                      "color style",
                                                      XFCE_TYPE_BACKDROP_COLOR_STYLE,
                                                      XFCE_BACKDROP_COLOR_SOLID,
                                                      XFDESKTOP_PARAM_FLAGS));

    g_object_class_install_property(gobject_class, PROP_COLOR1,
                                    g_param_spec_boxed("first-color",
                                                       "first color",
                                                       "first color",
                                                       GDK_TYPE_COLOR,
                                                       XFDESKTOP_PARAM_FLAGS));

    g_object_class_install_property(gobject_class, PROP_COLOR2,
                                    g_param_spec_boxed("second-color",
                                                       "second color",
                                                       "second color",
                                                       GDK_TYPE_COLOR,
                                                       XFDESKTOP_PARAM_FLAGS));

    g_object_class_install_property(gobject_class, PROP_SHOW_IMAGE,
                                    g_param_spec_boolean("show-image",
                                                         "show image",
                                                         "show image",
                                                         TRUE,
                                                         XFDESKTOP_PARAM_FLAGS));

    g_object_class_install_property(gobject_class, PROP_IMAGE_STYLE,
                                    g_param_spec_enum("image-style",
                                                      "image style",
                                                      "image style",
                                                      XFCE_TYPE_BACKDROP_IMAGE_STYLE,
                                                      XFCE_BACKDROP_IMAGE_AUTO,
                                                      XFDESKTOP_PARAM_FLAGS));

    g_object_class_install_property(gobject_class, PROP_IMAGE_FILENAME,
                                    g_param_spec_string("image-filename",
                                                        "image filename",
                                                        "image filename",
                                                        DEFAULT_BACKDROP,
                                                        XFDESKTOP_PARAM_FLAGS));

    g_object_class_install_property(gobject_class, PROP_BRIGHTNESS,
                                    g_param_spec_int("brightness",
                                                     "brightness",
                                                     "brightness",
                                                     -128, 127, 0,
                                                     XFDESKTOP_PARAM_FLAGS));

    g_object_class_install_property(gobject_class, PROP_SATURATION,
                                    g_param_spec_double("saturation",
                                                        "saturation",
                                                        "saturation",
                                                        -10.0, 10.0, 1.0,
                                                        XFDESKTOP_PARAM_FLAGS));

    g_object_class_install_property(gobject_class, PROP_BACKDROP_CYCLE_ENABLE,
                                    g_param_spec_boolean("backdrop-cycle-enable",
                                                         "backdrop-cycle-enable",
                                                         "backdrop-cycle-enable",
                                                         FALSE,
                                                         XFDESKTOP_PARAM_FLAGS));

    g_object_class_install_property(gobject_class, PROP_BACKDROP_CYCLE_TIMER,
                                    g_param_spec_uint("backdrop-cycle-timer",
                                                     "backdrop-cycle-timer",
                                                     "backdrop-cycle-timer",
                                                     0, G_MAXUSHORT, 10,
                                                     XFDESKTOP_PARAM_FLAGS));

#undef XFDESKTOP_PARAM_FLAGS
}

static void
xfce_backdrop_init(XfceBackdrop *backdrop)
{
    backdrop->priv = G_TYPE_INSTANCE_GET_PRIVATE(backdrop, XFCE_TYPE_BACKDROP,
                                                 XfceBackdropPriv);
    backdrop->priv->show_image = TRUE;
    backdrop->priv->cycle_timer_id = 0;
    backdrop->priv->backdrop_list = NULL;

    /* color defaults */
    backdrop->priv->color1.red = 0x1515;
    backdrop->priv->color1.green = 0x2222;
    backdrop->priv->color1.blue = 0x3333;
    backdrop->priv->color2.red = 0x1515;
    backdrop->priv->color2.green = 0x2222;
    backdrop->priv->color2.blue = 0x3333;
}

static void
xfce_backdrop_finalize(GObject *object)
{
    XfceBackdrop *backdrop = XFCE_BACKDROP(object);
    
    g_return_if_fail(backdrop != NULL);
    
    if(backdrop->priv->image_path)
        g_free(backdrop->priv->image_path);

    if(backdrop->priv->cycle_timer_id != 0) {
        g_source_remove(backdrop->priv->cycle_timer_id);
        backdrop->priv->cycle_timer_id = 0;
    }

    if(backdrop->priv->backdrop_list != NULL) {
        g_free(backdrop->priv->backdrop_list);
        backdrop->priv->backdrop_list = NULL;
    }

    G_OBJECT_CLASS(xfce_backdrop_parent_class)->finalize(object);
}

static void
xfce_backdrop_set_property(GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
    XfceBackdrop *backdrop = XFCE_BACKDROP(object);
    GdkColor *color;

    switch(property_id) {
        case PROP_COLOR_STYLE:
            xfce_backdrop_set_color_style(backdrop, g_value_get_enum(value));
            break;

        case PROP_COLOR1:
            color = g_value_get_boxed(value);
            if(color)
                xfce_backdrop_set_first_color(backdrop, color);
            break;

        case PROP_COLOR2:
            color = g_value_get_boxed(value);
            if(color)
                xfce_backdrop_set_second_color(backdrop, color);
            break;

        case PROP_SHOW_IMAGE:
            xfce_backdrop_set_show_image(backdrop, g_value_get_boolean(value));
            break;

        case PROP_IMAGE_STYLE:
            xfce_backdrop_set_image_style(backdrop, g_value_get_enum(value));
            break;

        case PROP_IMAGE_FILENAME:
            xfce_backdrop_set_image_filename(backdrop,
                                             g_value_get_string(value));
            break;

        case PROP_BRIGHTNESS:
            xfce_backdrop_set_brightness(backdrop, g_value_get_int(value));
            break;

        case PROP_SATURATION:
            xfce_backdrop_set_saturation(backdrop, g_value_get_double(value));
            break;

        case PROP_BACKDROP_CYCLE_ENABLE:
            xfce_backdrop_set_cycle_backdrop(backdrop, g_value_get_boolean(value));
            break;

        case PROP_BACKDROP_CYCLE_TIMER:
            xfce_backdrop_set_cycle_timer(backdrop, g_value_get_uint(value));
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
xfce_backdrop_get_property(GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
    XfceBackdrop *backdrop = XFCE_BACKDROP(object);

    switch(property_id) {
        case PROP_COLOR_STYLE:
            g_value_set_enum(value, xfce_backdrop_get_color_style(backdrop));
            break;

        case PROP_COLOR1:
            g_value_set_boxed(value, &backdrop->priv->color1);
            break;

        case PROP_COLOR2:
            g_value_set_boxed(value, &backdrop->priv->color2);
            break;

        case PROP_SHOW_IMAGE:
            g_value_set_boolean(value, xfce_backdrop_get_show_image(backdrop));
            break;

        case PROP_IMAGE_STYLE:
            g_value_set_enum(value, xfce_backdrop_get_image_style(backdrop));
            break;

        case PROP_IMAGE_FILENAME:
            g_value_set_string(value,
                               xfce_backdrop_get_image_filename(backdrop));
            break;

        case PROP_BRIGHTNESS:
            g_value_set_int(value, xfce_backdrop_get_brightness(backdrop));
            break;

        case PROP_SATURATION:
            g_value_set_double(value, xfce_backdrop_get_saturation(backdrop));
            break;

        case PROP_BACKDROP_CYCLE_ENABLE:
            g_value_set_boolean(value, xfce_backdrop_get_cycle_backdrop(backdrop));
            break;

        case PROP_BACKDROP_CYCLE_TIMER:
            g_value_set_uint(value, xfce_backdrop_get_cycle_timer(backdrop));
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}



/* public api */

/**
 * xfce_backdrop_new:
 * @visual: The current X visual in use.
 *
 * Creates a new #XfceBackdrop.  The @visual parameter is needed to decide the
 * optimal dithering method.
 *
 * Return value: A new #XfceBackdrop.
 **/
XfceBackdrop *
xfce_backdrop_new(GdkVisual *visual)
{
    XfceBackdrop *backdrop;
    
    g_return_val_if_fail(GDK_IS_VISUAL(visual), NULL);
    
    backdrop = g_object_new(XFCE_TYPE_BACKDROP, NULL);
    backdrop->priv->bpp = visual->depth;
    
    return backdrop;
}

/**
 * xfce_backdrop_new_with_size:
 * @width: The width of the #XfceBackdrop.
 * @height: The height of the #XfceBackdrop.
 *
 * Creates a new #XfceBackdrop with the specified @width and @height.
 *
 * Return value: A new #XfceBackdrop.
 **/
XfceBackdrop *
xfce_backdrop_new_with_size(GdkVisual *visual,
                            gint width,
                            gint height)
{
    XfceBackdrop *backdrop;
    
    g_return_val_if_fail(GDK_IS_VISUAL(visual), NULL);
    
    backdrop = g_object_new(XFCE_TYPE_BACKDROP, NULL);
    
    backdrop->priv->bpp = visual->depth;
    backdrop->priv->width = width;
    backdrop->priv->height = height;

    return backdrop;
}

/**
 * xfce_backdrop_set_size:
 * @backdrop: An #XfceBackdrop.
 * @width: The new width.
 * @height: The new height.
 *
 * Sets the backdrop's size, e.g., after a screen size change.  Note: This will
 * not emit the 'changed' signal; owners of #XfceBackdrop objects are expected
 * to manually refresh the backdrop data after calling xfce_backdrop_set_size().
 **/
void
xfce_backdrop_set_size(XfceBackdrop *backdrop, gint width, gint height)
{
    g_return_if_fail(XFCE_IS_BACKDROP(backdrop));
    
    backdrop->priv->width = width;
    backdrop->priv->height = height;
}

/**
 * xfce_backdrop_set_color_style:
 * @backdrop: An #XfceBackdrop.
 * @style: An #XfceBackdropColorStyle.
 *
 * Sets the color style for the #XfceBackdrop to the specified @style.
 **/
void
xfce_backdrop_set_color_style(XfceBackdrop *backdrop,
        XfceBackdropColorStyle style)
{
    g_return_if_fail(XFCE_IS_BACKDROP(backdrop));
    g_return_if_fail((int)style >= 0 && style <= XFCE_BACKDROP_COLOR_TRANSPARENT);
    
    if(style != backdrop->priv->color_style) {
        backdrop->priv->color_style = style;
        g_signal_emit(G_OBJECT(backdrop), backdrop_signals[BACKDROP_CHANGED], 0);
    }
}

XfceBackdropColorStyle
xfce_backdrop_get_color_style(XfceBackdrop *backdrop)
{
    g_return_val_if_fail(XFCE_IS_BACKDROP(backdrop),
                         XFCE_BACKDROP_COLOR_SOLID);
    return backdrop->priv->color_style;
}

/**
 * xfce_backdrop_set_first_color:
 * @backdrop: An #XfceBackdrop.
 * @color: A #GdkColor.
 *
 * Sets the "first" color for the #XfceBackdrop.  This is the color used if
 * the color style is set to XFCE_BACKDROP_COLOR_SOLID.  It is used as the
 * left-side color or top color if the color style is set to
 * XFCE_BACKDROP_COLOR_HORIZ_GRADIENT or XFCE_BACKDROP_COLOR_VERT_GRADIENT,
 * respectively.
 **/
void
xfce_backdrop_set_first_color(XfceBackdrop *backdrop,
                              const GdkColor *color)
{
    g_return_if_fail(XFCE_IS_BACKDROP(backdrop) && color != NULL);
    
    if(color->red != backdrop->priv->color1.red
            || color->green != backdrop->priv->color1.green
            || color->blue != backdrop->priv->color1.blue)
    {
        backdrop->priv->color1.red = color->red;
        backdrop->priv->color1.green = color->green;
        backdrop->priv->color1.blue = color->blue;
        g_signal_emit(G_OBJECT(backdrop), backdrop_signals[BACKDROP_CHANGED], 0);
    }
}

void
xfce_backdrop_get_first_color(XfceBackdrop *backdrop,
                              GdkColor *color)
{
    g_return_if_fail(XFCE_IS_BACKDROP(backdrop) && color);
    
    memcpy(color, &backdrop->priv->color1, sizeof(GdkColor));
}

/**
 * xfce_backdrop_set_second_color:
 * @backdrop: An #XfceBackdrop.
 * @color: A #GdkColor.
 *
 * Sets the "second" color for the #XfceBackdrop.  This is the color used as the
 * right-side color or bottom color if the color style is set to
 * XFCE_BACKDROP_COLOR_HORIZ_GRADIENT or XFCE_BACKDROP_COLOR_VERT_GRADIENT,
 * respectively.
 **/
void
xfce_backdrop_set_second_color(XfceBackdrop *backdrop,
                               const GdkColor *color)
{
    g_return_if_fail(XFCE_IS_BACKDROP(backdrop) && color != NULL);
    
    if(color->red != backdrop->priv->color2.red
            || color->green != backdrop->priv->color2.green
            || color->blue != backdrop->priv->color2.blue)
    {
        backdrop->priv->color2.red = color->red;
        backdrop->priv->color2.green = color->green;
        backdrop->priv->color2.blue = color->blue;
        if(backdrop->priv->color_style != XFCE_BACKDROP_COLOR_SOLID)
            g_signal_emit(G_OBJECT(backdrop), backdrop_signals[BACKDROP_CHANGED], 0);
    }
}

void
xfce_backdrop_get_second_color(XfceBackdrop *backdrop,
                               GdkColor *color)
{
    g_return_if_fail(XFCE_IS_BACKDROP(backdrop) && color);
    
    memcpy(color, &backdrop->priv->color2, sizeof(GdkColor));
}

/**
 * xfce_backdrop_set_show_image:
 * @backdrop: An #XfceBackdrop.
 * @show_image: Whether or not to composite the image on top of the color.
 *
 * Sets whether or not the #XfceBackdrop should consist of an image composited
 * on top of a color (or color gradient), or just a color (or color gradient).
 **/
void
xfce_backdrop_set_show_image(XfceBackdrop *backdrop, gboolean show_image)
{
    g_return_if_fail(XFCE_IS_BACKDROP(backdrop));
    
    if(backdrop->priv->show_image != show_image) {
        backdrop->priv->show_image = show_image;
        g_signal_emit(G_OBJECT(backdrop), backdrop_signals[BACKDROP_CHANGED], 0);
    }
}

gboolean
xfce_backdrop_get_show_image(XfceBackdrop *backdrop)
{
    g_return_val_if_fail(XFCE_IS_BACKDROP(backdrop), FALSE);
    return backdrop->priv->show_image;
}

/**
 * xfce_backdrop_set_image_style:
 * @backdrop: An #XfceBackdrop.
 * @style: An XfceBackdropImageStyle.
 *
 * Sets the image style to be used for the #XfceBackdrop.  "AUTO" attempts to
 * pick the best of "TILED" or "STRETCHED" based on the image size.
 * "STRETCHED" will stretch the image to the full width and height of the
 * #XfceBackdrop, while "SCALED" will resize the image to fit the desktop
 * while maintaining the image's aspect ratio.
 **/
void
xfce_backdrop_set_image_style(XfceBackdrop *backdrop,
        XfceBackdropImageStyle style)
{
    g_return_if_fail(XFCE_IS_BACKDROP(backdrop));
    
    if(style != backdrop->priv->image_style) {
        backdrop->priv->image_style = style;
        g_signal_emit(G_OBJECT(backdrop), backdrop_signals[BACKDROP_CHANGED], 0);
    }
}

XfceBackdropImageStyle
xfce_backdrop_get_image_style(XfceBackdrop *backdrop)
{
    g_return_val_if_fail(XFCE_IS_BACKDROP(backdrop),
                         XFCE_BACKDROP_IMAGE_TILED);
    return backdrop->priv->image_style;
}

/**
 * xfce_backdrop_set_image_filename:
 * @backdrop: An #XfceBackdrop.
 * @filename: A filename.
 *
 * Sets the image that should be used with the #XfceBackdrop.  The image will
 * be composited on top of the color (or color gradient).  To clear the image,
 * use this call with a @filename argument of %NULL.
 **/
void
xfce_backdrop_set_image_filename(XfceBackdrop *backdrop, const gchar *filename)
{
    g_return_if_fail(XFCE_IS_BACKDROP(backdrop));
    
    g_free(backdrop->priv->image_path);
    
    if(filename)
        backdrop->priv->image_path = g_strdup(filename);
    else
        backdrop->priv->image_path = NULL;
    
    g_signal_emit(G_OBJECT(backdrop), backdrop_signals[BACKDROP_CHANGED], 0);
}

G_CONST_RETURN gchar *
xfce_backdrop_get_image_filename(XfceBackdrop *backdrop)
{
    g_return_val_if_fail(XFCE_IS_BACKDROP(backdrop), NULL);
    return backdrop->priv->image_path;
}

/**
 * xfce_backdrop_set_list:
 * @backdrop: An #XfceBackdrop.
 * @backdrop_list: A list of backdrop images.
 *
 * Sets the internal list of wallpaper images assigned to this backdrop.
 * Frees any previous backdrop list before setting the new one.
 * [Transfer full]
 **/
void
xfce_backdrop_set_list(XfceBackdrop *backdrop,
                       gchar *backdrop_list)
{
    g_return_if_fail(XFCE_IS_BACKDROP(backdrop));

    if(backdrop->priv->backdrop_list != NULL) {
        g_free(backdrop->priv->backdrop_list);
        backdrop->priv->backdrop_list = NULL;
    }

    backdrop->priv->backdrop_list = backdrop_list;
}

G_CONST_RETURN gchar*
xfce_backdrop_get_list(XfceBackdrop *backdrop)
{
    g_return_val_if_fail(XFCE_IS_BACKDROP(backdrop), NULL);

    return backdrop->priv->backdrop_list;
}

/**
 * xfce_backdrop_set_brightness:
 * @backdrop: An #XfceBackdrop.
 * @brightness: A brightness value.
 *
 * Modifies the brightness of the backdrop using a value between -128 and 127.
 * A value of 0 indicates that the brightness should not be changed.  This value
 * is applied to the entire image, after compositing.
 **/
void
xfce_backdrop_set_brightness(XfceBackdrop *backdrop, gint brightness)
{
    g_return_if_fail(XFCE_IS_BACKDROP(backdrop));
    
    if(brightness != backdrop->priv->brightness) {
        backdrop->priv->brightness = brightness;
        g_signal_emit(G_OBJECT(backdrop), backdrop_signals[BACKDROP_CHANGED], 0);
    }
}

gint
xfce_backdrop_get_brightness(XfceBackdrop *backdrop)
{
    g_return_val_if_fail(XFCE_IS_BACKDROP(backdrop), 0);
    return backdrop->priv->brightness;
}

void
xfce_backdrop_set_saturation(XfceBackdrop *backdrop,
                             gdouble saturation)
{
    g_return_if_fail(XFCE_IS_BACKDROP(backdrop));
    if(abs(saturation - backdrop->priv->saturation) > 0.05) {
        backdrop->priv->saturation = saturation;
        g_signal_emit(G_OBJECT(backdrop), backdrop_signals[BACKDROP_CHANGED], 0);
    }
}

gdouble
xfce_backdrop_get_saturation(XfceBackdrop *backdrop)
{
    g_return_val_if_fail(XFCE_IS_BACKDROP(backdrop), 1.0);
    return backdrop->priv->saturation;
}

static gboolean
xfce_backdrop_timer(XfceBackdrop *backdrop)
{
    TRACE("entering");

    g_return_val_if_fail(XFCE_IS_BACKDROP(backdrop), FALSE);

    g_signal_emit(G_OBJECT(backdrop), backdrop_signals[BACKDROP_CYCLE], 0);

    return TRUE;
}

/**
 * xfce_backdrop_set_cycle_timer:
 * @backdrop: An #XfceBackdrop.
 * @cycle_timer: The amount of time, in minutes, to wait before changing the
 *               background image.
 *
 * If cycle_backdrop is enabled then this function will change the backdrop
 * image based on the cycle_timer, a value between 0 and G_MAXUSHORT.
 * A value of 0 indicates that the backdrop should not be changed.
 **/
void
xfce_backdrop_set_cycle_timer(XfceBackdrop *backdrop, guint cycle_timer)
{
    g_return_if_fail(XFCE_IS_BACKDROP(backdrop));

    TRACE("entering, cycle_timer = %d", cycle_timer);

    if(cycle_timer > G_MAXUSHORT)
        cycle_timer = G_MAXUSHORT;

    backdrop->priv->cycle_timer = cycle_timer;

    if(backdrop->priv->cycle_timer_id != 0) {
        g_source_remove(backdrop->priv->cycle_timer_id);
        backdrop->priv->cycle_timer_id = 0;
    }

    if(backdrop->priv->cycle_timer != 0 &&
       backdrop->priv->cycle_backdrop == TRUE) {
        backdrop->priv->cycle_timer_id = g_timeout_add_seconds(backdrop->priv->cycle_timer * 60,
                                                               (GSourceFunc)xfce_backdrop_timer,
                                                               backdrop);
    }
}

guint
xfce_backdrop_get_cycle_timer(XfceBackdrop *backdrop)
{
    g_return_val_if_fail(XFCE_IS_BACKDROP(backdrop), 0);
    return backdrop->priv->cycle_timer;
}

void
xfce_backdrop_set_cycle_backdrop(XfceBackdrop *backdrop,
                                 gboolean cycle_backdrop)
{
    g_return_if_fail(XFCE_IS_BACKDROP(backdrop));

    TRACE("entering, cycle_backdrop ? %s", cycle_backdrop == TRUE ? "TRUE" : "FALSE");

    if(backdrop->priv->cycle_backdrop != cycle_backdrop) {
        backdrop->priv->cycle_backdrop = cycle_backdrop;
        /* Start or stop the backdrop changing */
        xfce_backdrop_set_cycle_timer(backdrop,
                                      xfce_backdrop_get_cycle_timer(backdrop));
    }
}

gboolean
xfce_backdrop_get_cycle_backdrop(XfceBackdrop *backdrop)
{
    g_return_val_if_fail(XFCE_IS_BACKDROP(backdrop), 0);

    return backdrop->priv->cycle_backdrop;
}

/**
 * xfce_backdrop_get_pixbuf:
 * @backdrop: An #XfceBackdrop.
 *
 * Generates the final composited, resized image from the #XfceBackdrop.  Free
 * it with g_object_unref() when you are finished.
 *
 * Return value: A #GdkPixbuf.
 **/
GdkPixbuf *
xfce_backdrop_get_pixbuf(XfceBackdrop *backdrop)
{
    GdkPixbuf *final_image, *image = NULL, *tmp;
    GdkPixbufFormat *format = NULL;
    gboolean apply_backdrop_image = FALSE;
    gint i, j;
    gint w, h, iw = 0, ih = 0;
    XfceBackdropImageStyle istyle;
    gint dx, dy, xo, yo;
    gdouble xscale, yscale;
    GdkInterpType interp;
    
    g_return_val_if_fail(XFCE_IS_BACKDROP(backdrop), NULL);
    
    if(backdrop->priv->show_image && backdrop->priv->image_path) {
        format = gdk_pixbuf_get_file_info(backdrop->priv->image_path, &iw, &ih);
        /* make sure we have a usable backdrop image */
        if(format != NULL)
            apply_backdrop_image = TRUE;
    }

    if(backdrop->priv->width == 0 || backdrop->priv->height == 0) {
        w = iw;
        h = ih;
    } else {
        w = backdrop->priv->width;
        h = backdrop->priv->height;
    }
    
    if(backdrop->priv->color_style == XFCE_BACKDROP_COLOR_SOLID)
        final_image = create_solid(&backdrop->priv->color1, w, h, FALSE, 0xff);
    else if(backdrop->priv->color_style == XFCE_BACKDROP_COLOR_TRANSPARENT) {
        GdkColor c = { 0, 0xffff, 0xffff, 0xffff };
        final_image = create_solid(&c, w, h, TRUE, 0x00);
    } else {
        final_image = create_gradient(&backdrop->priv->color1,
                &backdrop->priv->color2, w, h, backdrop->priv->color_style);
        if(!final_image)
            final_image = create_solid(&backdrop->priv->color1, w, h, FALSE, 0xff);
    }
    
    if(!apply_backdrop_image) {
        if(backdrop->priv->brightness != 0)
            final_image = adjust_brightness(final_image, backdrop->priv->brightness);
        
        return final_image;
    }
    
    if(backdrop->priv->image_style == XFCE_BACKDROP_IMAGE_AUTO) {
        if(ih <= h / 2 && iw <= w / 2)
            istyle = XFCE_BACKDROP_IMAGE_TILED;
        else
            istyle = XFCE_BACKDROP_IMAGE_ZOOMED;
    } else
        istyle = backdrop->priv->image_style;
    
    /* if the image is the same as the screen size, there's no reason to do
     * any scaling at all */
    if(w == iw && h == ih)
        istyle = XFCE_BACKDROP_IMAGE_CENTERED;
    
    /* if we don't need to do any scaling, don't do any interpolation.  this
     * fixes a problem where hyper/bilinear filtering causes blurriness in
     * some images.  http://bugzilla.xfce.org/show_bug.cgi?id=2939 */
    if(XFCE_BACKDROP_IMAGE_TILED == istyle
       || XFCE_BACKDROP_IMAGE_CENTERED == istyle)
    {
        interp = GDK_INTERP_NEAREST;
    } else {
        /* if the screen has a bit depth of less than 24bpp, using bilinear
         * filtering looks crappy (mainly with gradients). */
        if(backdrop->priv->bpp < 24)
            interp = GDK_INTERP_HYPER;
        else
            interp = GDK_INTERP_BILINEAR;
    }
    
    switch(istyle) {
        case XFCE_BACKDROP_IMAGE_CENTERED:
            image = gdk_pixbuf_new_from_file(backdrop->priv->image_path, NULL);
            dx = MAX((w - iw) / 2, 0);
            dy = MAX((h - ih) / 2, 0);
            xo = MIN((w - iw) / 2, dx);
            yo = MIN((h - ih) / 2, dy);
            gdk_pixbuf_composite(image, final_image, dx, dy,
                    MIN(w, iw), MIN(h, ih), xo, yo, 1.0, 1.0,
                    interp, 255);
            break;
        
        case XFCE_BACKDROP_IMAGE_TILED:
            image = gdk_pixbuf_new_from_file(backdrop->priv->image_path, NULL);
            tmp = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, w, h);
            /* Now that the image has been loaded, recalculate the image
             * size because gdk_pixbuf_get_file_info doesn't always return
             * the correct size */
            iw = gdk_pixbuf_get_width(image);
            ih = gdk_pixbuf_get_height(image);
            for(i = 0; (i * iw) < w; i++) {
                for(j = 0; (j * ih) < h; j++) {
                    gint newx = iw * i, newy = ih * j;
                    gint neww = iw, newh = ih;
                    
                    if((newx + neww) > w)
                        neww = w - newx;
                    if((newy + newh) > h)
                        newh = h - newy;
                    
                    gdk_pixbuf_copy_area(image, 0, 0,
                            neww, newh, tmp, newx, newy);
                }
            }
            
            gdk_pixbuf_composite(tmp, final_image, 0, 0, w, h,
                    0, 0, 1.0, 1.0, interp, 255);
            g_object_unref(G_OBJECT(tmp));
            break;
        
        case XFCE_BACKDROP_IMAGE_STRETCHED:
            image = gdk_pixbuf_new_from_file_at_scale(
                            backdrop->priv->image_path, w, h, FALSE, NULL);
            gdk_pixbuf_composite(image, final_image, 0, 0, w, h,
                    0, 0, 1, 1, interp, 255);
            break;
        
        case XFCE_BACKDROP_IMAGE_SCALED:
            xscale = (gdouble)w / iw;
            yscale = (gdouble)h / ih;
            if(xscale < yscale) {
                yscale = xscale;
                xo = 0;
                yo = (h - (ih * yscale)) / 2;
            } else {
                xscale = yscale;
                xo = (w - (iw * xscale)) / 2;
                yo = 0;
            }
            dx = xo;
            dy = yo;

            image = gdk_pixbuf_new_from_file_at_scale(
                        backdrop->priv->image_path, iw * xscale,
                        ih * yscale, TRUE, NULL);
            gdk_pixbuf_composite(image, final_image, dx, dy,
                    iw * xscale, ih * yscale, xo, yo, 1, 1,
                    interp, 255);
            break;
        
        case XFCE_BACKDROP_IMAGE_ZOOMED:
            xscale = (gdouble)w / iw;
            yscale = (gdouble)h / ih;
            if(xscale < yscale) {
                xscale = yscale;
                xo = (w - (iw * xscale)) * 0.5;
                yo = 0;
            } else {
                yscale = xscale;
                xo = 0;
                yo = (h - (ih * yscale)) * 0.5;
            }

            image = gdk_pixbuf_new_from_file_at_scale(
                                backdrop->priv->image_path, iw * xscale,
                                ih * yscale, TRUE, NULL);
            gdk_pixbuf_composite(image, final_image, 0, 0,
                    w, h, xo, yo, 1, 1, interp, 255);
            break;
        
        default:
            g_critical("Invalid image style: %d\n", (gint)istyle);
    }
    
    if(image)
        g_object_unref(G_OBJECT(image));
    
    if(backdrop->priv->brightness != 0)
        final_image = adjust_brightness(final_image, backdrop->priv->brightness);

    if(backdrop->priv->saturation > 1.01 || backdrop->priv->saturation < 0.99) {
        gdk_pixbuf_saturate_and_pixelate(final_image, final_image,
                                         (gfloat)backdrop->priv->saturation,
                                         FALSE);
    }
    
    return final_image;
}
