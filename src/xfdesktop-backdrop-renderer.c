/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2004,2024 Brian Tarricone, <brian@tarricone.org>
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
 *  Random portions taken from or inspired by the original xfdesktop for xfce4:
 *     Copyright (C) 2002-2003 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *     Copyright (C) 2003 Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de>
 */

#include <math.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixdata.h>
#include <cairo.h>

#include <libxfce4util/libxfce4util.h>

#include "xfdesktop-common.h"
#include "xfdesktop-backdrop-renderer.h"

#define XFCE_BACKDROP_BUFFER_SIZE 32768

static const double backdrop_gamma = 2.2;

typedef struct
{
    GCancellable *cancellable;

    GdkPixbuf *canvas;
    XfceBackdropImageStyle image_style;
    GFile *image_file;
    gint width;
    gint height;

    GdkPixbufLoader *loader;
    guchar *image_buffer;

    RenderCompleteCallback callback;
    gpointer callback_user_data;
} ImageData;

static void
image_data_complete(ImageData *image_data, cairo_surface_t *surface) {
    image_data->callback(surface, image_data->width, image_data->height, NULL, image_data->callback_user_data);
}

static void
image_data_error(ImageData *image_data, GError *error) {
    GError *cb_error;
    if (error == NULL) {
        cb_error = g_error_new_literal(G_IO_ERROR, G_IO_ERROR_UNKNOWN, "Unknown error");
    } else {
        cb_error = error;
    }

    cairo_surface_t *surface = NULL;
    if (!g_error_matches(cb_error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
        // If there was an error loading the image file, at least return the
        // canvas, which has the solid/gradient color the user has chosen.
        surface = gdk_cairo_surface_create_from_pixbuf(image_data->canvas, 1, NULL);
    }

    image_data->callback(surface,
                         surface != NULL ? cairo_image_surface_get_width(surface) : -1,
                         surface != NULL ? cairo_image_surface_get_height(surface) : -1,
                         cb_error,
                         image_data->callback_user_data);

    if (error == NULL) {
        g_error_free(cb_error);
    }
}

static void
image_data_cancelled(ImageData *image_data) {
    GError *error = g_error_new_literal(G_IO_ERROR, G_IO_ERROR_CANCELLED, "Image loading was cancelled");
    image_data_error(image_data, error);
    g_error_free(error);
}

static void
image_data_free(ImageData *image_data) {
    g_signal_handlers_disconnect_by_data(image_data->loader, image_data);
    gdk_pixbuf_loader_close(image_data->loader, NULL);
    g_object_unref(image_data->loader);

    if (image_data->image_file != NULL) {
        g_object_unref(image_data->image_file);
    }
    g_object_unref(image_data->cancellable);
    g_object_unref(image_data->canvas);
    g_free(image_data->image_buffer);
    g_free(image_data);
}

static GdkPixbuf *
create_solid(GdkRGBA *color, gint width, gint height) {
    GdkWindow *root;
    GdkPixbuf *pix;
    cairo_surface_t *surface;
    cairo_t *cr;

    root = gdk_screen_get_root_window(gdk_screen_get_default ());
    surface = gdk_window_create_similar_surface(root, CAIRO_CONTENT_COLOR_ALPHA, width, height);
    cr = cairo_create(surface);

    cairo_set_source_rgba(cr, color->red, color->green, color->blue, color->alpha);

    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);

    cairo_surface_flush(surface);

    pix = gdk_pixbuf_get_from_surface(surface, 0, 0, width, height);

    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    return pix;
}

static void
apply_gamma(GdkRGBA *dest, const GdkRGBA *src) {
    dest->red = pow(src->red, backdrop_gamma);
    dest->green = pow(src->green, backdrop_gamma);
    dest->blue = pow(src->blue, backdrop_gamma);
    dest->alpha = src->alpha;
}

static unsigned int
round_threshold(double x, double threshold) {
    double ipart;
    double fpart = modf(x, &ipart);
    if (fpart >= threshold)
        return ipart + 1;
    else
        return ipart;
}

static unsigned char
interpolate(double x1, double x2, int position, int max, double threshold) {
    const double t = (double)position / (max - 1); // we want to interpolate all the way to max-1
    const double xi = x1 * (1 - t) + x2 * t;
    return CLAMP((int)round_threshold(pow(xi, 1 / backdrop_gamma) * 255, threshold), 0, 255);
}

static double
gen_threshold(unsigned int *seed) {
    return (double)rand_r(seed) / RAND_MAX;
}

static GdkPixbuf *
create_gradient(GdkRGBA *color1, GdkRGBA *color2, gint width, gint height, XfceBackdropColorStyle style) {
    GdkWindow *root;
    GdkPixbuf *pix;
    cairo_surface_t *surface;
    gint scale_factor;
    unsigned char *data;
    int stride;
    gint ax1;
    gint ax1_max;
    gint ax2;
    gint ax2_max;
    GdkRGBA color1_lin;
    GdkRGBA color2_lin;
    unsigned int seed;

    g_return_val_if_fail(color1 != NULL && color2 != NULL, NULL);
    g_return_val_if_fail(width > 0 && height > 0, NULL);
    g_return_val_if_fail(style == XFCE_BACKDROP_COLOR_HORIZ_GRADIENT || style == XFCE_BACKDROP_COLOR_VERT_GRADIENT, NULL);

    root = gdk_screen_get_root_window(gdk_screen_get_default());
    scale_factor = gdk_window_get_scale_factor(root);
    surface = gdk_window_create_similar_image_surface(root, CAIRO_FORMAT_RGB24, width, height, scale_factor);
    data = cairo_image_surface_get_data(surface);
    stride = cairo_image_surface_get_stride(surface);

    if (style == XFCE_BACKDROP_COLOR_VERT_GRADIENT) {
        ax1_max = height;
        ax2_max = width;
    } else {
        ax1_max = width;
        ax2_max = height;
    }

    apply_gamma(&color1_lin, color1);
    apply_gamma(&color2_lin, color2);
    seed = 0;

    for (ax1 = 0; ax1 < ax1_max; ax1++) {
        for (ax2 = 0; ax2 < ax2_max; ax2++) {
            guint x, y;
            guint32 *p;
            unsigned char r, g, b;
            double threshold = gen_threshold(&seed);
            r = interpolate(color1_lin.red, color2_lin.red, ax1, ax1_max, threshold);
            g = interpolate(color1_lin.green, color2_lin.green, ax1, ax1_max, threshold);
            b = interpolate(color1_lin.blue, color2_lin.blue, ax1, ax1_max, threshold);
            if (style == XFCE_BACKDROP_COLOR_VERT_GRADIENT) {
                x = ax2;
                y = ax1;
            } else {
                x = ax1;
                y = ax2;
            }
            p = (guint32 *)(gpointer)(data + y * stride + x * sizeof(guint32));
            *p = (r << 16) | (g << 8) | (b << 0);
        }
    }
    cairo_surface_mark_dirty(surface);

    pix = gdk_pixbuf_get_from_surface(surface, 0, 0, width, height);
    cairo_surface_destroy(surface);

    return pix;
}

static GdkPixbuf *
generate_canvas(XfceBackdropColorStyle color_style, GdkRGBA *color1, GdkRGBA *color2, gint width, gint height) {
    if (color_style == XFCE_BACKDROP_COLOR_SOLID)
        return create_solid(color1, width, height);
    else if (color_style == XFCE_BACKDROP_COLOR_TRANSPARENT) {
        GdkRGBA c = { 1.0f, 1.0f, 1.0f, 0.0f };
        return create_solid(&c, width, height);
    } else {
        GdkPixbuf *canvas = create_gradient(color1, color2, width, height, color_style);
        if (canvas != NULL) {
            return canvas;
        } else {
            return create_solid(color1, width, height);
        }
    }
}

static void
loader_closed_cb(GdkPixbufLoader *loader, ImageData *image_data) {
    TRACE("entering");

    if (g_cancellable_is_cancelled(image_data->cancellable)) {
        image_data_cancelled(image_data);
        image_data_free(image_data);
    } else {
        GdkPixbuf *final_image = image_data->canvas;

        GdkPixbuf *image = gdk_pixbuf_loader_get_pixbuf(loader);
        if (image == NULL) {
            XF_DEBUG("image failed to load, displaying canvas only");

            cairo_surface_t *surface = gdk_cairo_surface_create_from_pixbuf(final_image, 1, NULL);
            image_data_complete(image_data, surface);
            image_data_free(image_data);
        } else {
            /* If the image is supposed to be rotated, do that now */
            GdkPixbuf *temp = gdk_pixbuf_apply_embedded_orientation(image);

            gint iw_orig = gdk_pixbuf_get_width(image);
            image = temp;  // Do not unref image, gdk_pixbuf_loader_get_pixbuf is transfer none
            gint iw = gdk_pixbuf_get_width(image);
            gint ih = gdk_pixbuf_get_height(image);

            gboolean rotated = (iw_orig != iw);

            gint w, h;
            if (image_data->width == 0 || image_data->height == 0) {
                w = iw;
                h = ih;
            } else {
                w = image_data->width;
                h = image_data->height;
            }

            XfceBackdropImageStyle istyle;
            if (w == iw && h == ih) {
                /* if the image is the same as the screen size, there's no reason to do
                 * any scaling at all */
                istyle = XFCE_BACKDROP_IMAGE_CENTERED;
            } else {
                istyle = image_data->image_style;
            }

            GdkInterpType interp;
            if(XFCE_BACKDROP_IMAGE_TILED == istyle || XFCE_BACKDROP_IMAGE_CENTERED == istyle) {
                /* if we don't need to do any scaling, don't do any interpolation.  this
                 * fixes a problem where hyper/bilinear filtering causes blurriness in
                 * some images.  https://bugzilla.xfce.org/show_bug.cgi?id=2939 */
                interp = GDK_INTERP_NEAREST;
            } else {
                // GDK_INTERP_HYPER does nothing as of some old version of gdk-pixbuf
                interp = GDK_INTERP_BILINEAR;
            }

            gdouble xscale = (gdouble)w / iw;
            gdouble yscale = (gdouble)h / ih;

            switch(istyle) {
                case XFCE_BACKDROP_IMAGE_NONE:
                    break;

                case XFCE_BACKDROP_IMAGE_CENTERED: {
                    gint dx = MAX((w - iw) / 2, 0);
                    gint dy = MAX((h - ih) / 2, 0);
                    gint xo = MIN((w - iw) / 2, dx);
                    gint yo = MIN((h - ih) / 2, dy);
                    gdk_pixbuf_composite(image,
                                         final_image,
                                         dx, dy,
                                         MIN(w, iw), MIN(h, ih),
                                         xo, yo,
                                         1.0,
                                         1.0,
                                         interp,
                                         255);
                    break;
                }

                case XFCE_BACKDROP_IMAGE_TILED: {
                    GdkPixbuf *tmp = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, w, h);
                    /* Now that the image has been loaded, recalculate the image
                     * size because gdk_pixbuf_get_file_info doesn't always return
                     * the correct size */
                    iw = gdk_pixbuf_get_width(image);
                    ih = gdk_pixbuf_get_height(image);

                    for (gint i = 0; (i * iw) < w; i++) {
                        for (gint j = 0; (j * ih) < h; j++) {
                            gint newx = iw * i, newy = ih * j;
                            gint neww = iw, newh = ih;

                            if ((newx + neww) > w) {
                                neww = w - newx;
                            }
                            if ((newy + newh) > h) {
                                newh = h - newy;
                            }

                            gdk_pixbuf_copy_area(image,
                                                 0, 0,
                                                 neww, newh,
                                                 tmp,
                                                 newx, newy);
                        }
                    }

                    gdk_pixbuf_composite(tmp,
                                         final_image,
                                         0, 0,
                                         w, h,
                                         0, 0,
                                         1.0,
                                         1.0,
                                         interp,
                                         255);
                    g_object_unref(G_OBJECT(tmp));
                    break;
                }

                case XFCE_BACKDROP_IMAGE_STRETCHED:
                    gdk_pixbuf_composite(image,
                                         final_image,
                                         0, 0,
                                         w, h,
                                         0, 0,
                                         rotated ? xscale : 1,
                                         rotated ? yscale : 1,
                                         interp,
                                         255);
                    break;

                case XFCE_BACKDROP_IMAGE_SCALED: {
                    gint xo, yo;
                    if (xscale < yscale) {
                        yscale = xscale;
                        xo = 0;
                        yo = (h - (ih * yscale)) / 2;
                    } else {
                        xscale = yscale;
                        xo = (w - (iw * xscale)) / 2;
                        yo = 0;
                    }
                    gint dx = xo;
                    gint dy = yo;

                    gdk_pixbuf_composite(image,
                                         final_image,
                                         dx, dy,
                                         iw * xscale, ih * yscale,
                                         xo, yo,
                                         rotated ? xscale : 1,
                                         rotated ? yscale : 1,
                                         interp,
                                         255);
                    break;
                }

                case XFCE_BACKDROP_IMAGE_ZOOMED:
                case XFCE_BACKDROP_IMAGE_SPANNING_SCREENS: {
                    gint xo, yo;
                    if (xscale < yscale) {
                        xscale = yscale;
                        xo = (w - (iw * xscale)) * 0.5;
                        yo = 0;
                    } else {
                        yscale = xscale;
                        xo = 0;
                        yo = (h - (ih * yscale)) * 0.5;
                    }

                    gdk_pixbuf_composite(image,
                                         final_image,
                                         0, 0,
                                         w, h,
                                         xo, yo,
                                         rotated ? xscale : 1,
                                         rotated ? yscale : 1,
                                         interp,
                                         255);
                    break;
                }

                default:
                    g_critical("Invalid image style: %d\n", (gint)istyle);
            }

            if (!g_cancellable_is_cancelled(image_data->cancellable)) {
                cairo_surface_t *surface = gdk_cairo_surface_create_from_pixbuf(final_image, 1, NULL);
                image_data_complete(image_data, surface);
            } else {
                image_data_cancelled(image_data);
            }

            g_object_unref(image);
            image_data_free(image_data);
        }
    }
}

static void
loader_size_prepared_cb(GdkPixbufLoader *loader, gint width, gint height, gpointer user_data) {
    ImageData *image_data = user_data;

    TRACE("entering");

    switch (image_data->image_style) {
        case XFCE_BACKDROP_IMAGE_CENTERED:
        case XFCE_BACKDROP_IMAGE_TILED:
        case XFCE_BACKDROP_IMAGE_NONE:
            /* do nothing */
            break;

        case XFCE_BACKDROP_IMAGE_STRETCHED:
            gdk_pixbuf_loader_set_size(loader, image_data->width, image_data->height);
            break;

        case XFCE_BACKDROP_IMAGE_SCALED: {
            gdouble xscale = (gdouble)image_data->width / width;
            gdouble yscale = (gdouble)image_data->height / height;
            if (xscale < yscale) {
                yscale = xscale;
            } else {
                xscale = yscale;
            }

            gdk_pixbuf_loader_set_size(loader, width * xscale, height * yscale);
            break;
        }

        case XFCE_BACKDROP_IMAGE_ZOOMED:
        case XFCE_BACKDROP_IMAGE_SPANNING_SCREENS: {
            gdouble xscale = (gdouble)image_data->width / width;
            gdouble yscale = (gdouble)image_data->height / height;
            if(xscale < yscale) {
                xscale = yscale;
            } else {
                yscale = xscale;
            }

            gdk_pixbuf_loader_set_size(loader, width * xscale, height * yscale);
            break;
        }

        default:
            g_critical("Invalid image style: %d\n", (gint)image_data->image_style);
            break;
    }
}

static void
file_input_stream_ready_cb(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    ImageData *image_data = user_data;
    GInputStream *stream = G_INPUT_STREAM(source_object);

    GError *error = NULL;
    gssize bytes = g_input_stream_read_finish(stream, res, &error);

    if (bytes < 0) {
        g_input_stream_close(stream, NULL, NULL);
        g_object_unref(stream);
        image_data_error(image_data, error);
        image_data_free(image_data);
        g_error_free(error);
    } else if (bytes == 0) {
        g_input_stream_close(stream, NULL, NULL);
        g_object_unref(stream);
        if (!gdk_pixbuf_loader_close(image_data->loader, &error)) {
            image_data_error(image_data, error);
            image_data_free(image_data);
            g_error_free(error);
        }
    } else if (gdk_pixbuf_loader_write(image_data->loader, image_data->image_buffer, bytes, &error)) {
        g_input_stream_read_async(stream,
                                  image_data->image_buffer,
                                  XFCE_BACKDROP_BUFFER_SIZE,
                                  G_PRIORITY_LOW,
                                  image_data->cancellable,
                                  file_input_stream_ready_cb,
                                  image_data);
    } else {
        g_input_stream_close(stream, NULL, NULL);
        g_object_unref(stream);
        image_data_error(image_data, error);
        image_data_free(image_data);
        g_error_free(error);
    }
}

static void
file_ready_cb(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    ImageData *image_data = user_data;

    TRACE("entering");

    GFile *file = G_FILE(source_object);
    GError *error = NULL;
    GFileInputStream *input_stream = g_file_read_finish(file, res, &error);

    if (input_stream == NULL) {
        image_data_error(image_data, error);
        image_data_free(image_data);
        g_error_free(error);
    } else {
        g_input_stream_read_async(G_INPUT_STREAM(input_stream),
                                  image_data->image_buffer,
                                  XFCE_BACKDROP_BUFFER_SIZE,
                                  G_PRIORITY_LOW,
                                  image_data->cancellable,
                                  file_input_stream_ready_cb,
                                  image_data);
    }
}

void
xfdesktop_backdrop_render(GCancellable *cancellable,
                          XfceBackdropColorStyle color_style,
                          GdkRGBA *color1,
                          GdkRGBA *color2,
                          XfceBackdropImageStyle image_style,
                          GFile *image_file,
                          gint width,
                          gint height,
                          RenderCompleteCallback callback,
                          gpointer callback_user_data)
{
    g_return_if_fail(color1 != NULL);
    g_return_if_fail(color2 != NULL);
    g_return_if_fail(width > 0);
    g_return_if_fail(height > 0);
    g_return_if_fail(callback != NULL);
    
    ImageData *image_data = NULL;

    TRACE("entering");

    if (color_style == XFCE_BACKDROP_COLOR_INVALID) {
        color_style = XFCE_BACKDROP_COLOR_SOLID;
    }
    if (image_style == XFCE_BACKDROP_IMAGE_INVALID) {
        image_style = XFCE_BACKDROP_IMAGE_ZOOMED;
    }

    GdkPixbuf *canvas = generate_canvas(color_style, color1, color2, width, height);

    if (image_style == XFCE_BACKDROP_IMAGE_NONE) {
        // If we aren't going to display an image then just return the canvas
        cairo_surface_t *surface = gdk_cairo_surface_create_from_pixbuf(canvas, 1, NULL);
        g_object_unref(canvas);
        callback(surface, width, height, NULL, callback_user_data);
    } else {
        if (image_file == NULL) {
            image_file = g_file_new_for_path(DEFAULT_BACKDROP);
        } else {
            image_file = g_object_ref(image_file);
        }

        XF_DEBUG("loading image %s", g_file_peek_path(image_file));

        image_data = g_new0(ImageData, 1);
        image_data->cancellable = g_object_ref(cancellable);
        image_data->canvas = canvas;
        image_data->image_style = image_style;
        image_data->image_file = image_file;
        image_data->width = width;
        image_data->height = height;
        image_data->callback = callback;
        image_data->callback_user_data = callback_user_data;

        image_data->loader = gdk_pixbuf_loader_new();
        image_data->image_buffer = g_new0(guchar, XFCE_BACKDROP_BUFFER_SIZE);

        g_signal_connect(image_data->loader, "size-prepared",
                         G_CALLBACK(loader_size_prepared_cb), image_data);
        g_signal_connect(image_data->loader, "closed",
                         G_CALLBACK(loader_closed_cb), image_data);

        g_file_read_async(image_data->image_file,
                          G_PRIORITY_DEFAULT,
                          image_data->cancellable,
                          file_ready_cb,
                          image_data);
    }
}
