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
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
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

#define XFCE_BACKDROP_BUFFER_SIZE 32768

#ifndef O_BINARY
#define O_BINARY  0
#endif

typedef struct _XfceBackdropImageData XfceBackdropImageData;

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

static GdkPixbuf *xfce_backdrop_generate_canvas(XfceBackdrop *backdrop);

static void xfce_backdrop_loader_size_prepared_cb(GdkPixbufLoader *loader,
                                                  gint width,
                                                  gint height,
                                                  gpointer user_data);

static void xfce_backdrop_loader_closed_cb(GdkPixbufLoader *loader,
                                           XfceBackdropImageData *image_data);

static void xfce_backdrop_file_ready_cb(GObject *source_object,
                                        GAsyncResult *res,
                                        gpointer user_data);

static void xfce_backdrop_file_input_stream_ready_cb(GObject *source_object,
                                                     GAsyncResult *res,
                                                     gpointer user_data);

static void xfce_backdrop_image_data_release(XfceBackdropImageData *image_data);

gchar *xfce_backdrop_choose_next         (XfceBackdrop *backdrop);
gchar *xfce_backdrop_choose_random       (XfceBackdrop *backdrop);
gchar *xfce_backdrop_choose_chronological(XfceBackdrop *backdrop);

struct _XfceBackdropPriv
{
    gint width, height;
    gint bpp;

    GdkPixbuf *pix;
    XfceBackdropImageData *image_data;
    gboolean cache_pixbuf;

    XfceBackdropColorStyle color_style;
    GdkColor color1;
    GdkColor color2;

    XfceBackdropImageStyle image_style;
    gchar *image_path;
    /* Cached list of images in the same folder as image_path */
    GList *image_files;
    /* monitor for the image_files directory */
    GFileMonitor *monitor;

    gboolean cycle_backdrop;
    guint cycle_timer;
    guint cycle_timer_id;
    XfceBackdropCyclePeriod cycle_period;
    gboolean random_backdrop_order;
};

struct _XfceBackdropImageData
{
    XfceBackdrop *backdrop;

    GdkPixbufLoader *loader;

    GCancellable *cancellable;

    guchar *image_buffer;
};

enum
{
    BACKDROP_CHANGED,
    BACKDROP_CYCLE,
    BACKDROP_READY,
    LAST_SIGNAL,
};

enum
{
    PROP_0 = 0,
    PROP_COLOR_STYLE,
    PROP_COLOR1,
    PROP_COLOR2,
    PROP_IMAGE_STYLE,
    PROP_IMAGE_FILENAME,
    PROP_BRIGHTNESS,
    PROP_BACKDROP_CYCLE_ENABLE,
    PROP_BACKDROP_CYCLE_PERIOD,
    PROP_BACKDROP_CYCLE_TIMER,
    PROP_BACKDROP_RANDOM_ORDER,
};

static guint backdrop_signals[LAST_SIGNAL] = { 0, };

/* helper functions */

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

static void
xfce_backdrop_clear_cached_image(XfceBackdrop *backdrop)
{
    g_return_if_fail(XFCE_IS_BACKDROP(backdrop));

    if(backdrop->priv->pix == NULL)
        return;

    g_object_unref(backdrop->priv->pix);
    backdrop->priv->pix = NULL;
}

/* we compare by the collate key so the image listing is the same as how
 * xfdesktop-settings displays the images */
static gint
compare_by_collate_key(const gchar *a, const gchar *b)
{
    gint ret;
    gchar *a_key = g_utf8_collate_key_for_filename(a, -1);
    gchar *b_key = g_utf8_collate_key_for_filename(b, -1);

    ret = g_strcmp0(a_key, b_key);

    g_free(a_key);
    g_free(b_key);

    return ret;
}

static void
cb_xfce_backdrop__image_files_changed(GFileMonitor     *monitor,
                                      GFile            *file,
                                      GFile            *other_file,
                                      GFileMonitorEvent event,
                                      gpointer          user_data)
{
    XfceBackdrop *backdrop = XFCE_BACKDROP(user_data);
    gchar *changed_file = NULL;
    GList *item;

    switch(event) {
        case G_FILE_MONITOR_EVENT_CREATED:
            changed_file = g_file_get_path(file);

            /* Make sure we don't already have the new file in the list */
            if(g_list_find(backdrop->priv->image_files, changed_file)) {
                g_free(changed_file);
                return;
            }

            /* If the new file is not an image then we don't have to do
             * anything */
            if(!xfdesktop_image_file_is_valid(changed_file)) {
                g_free(changed_file);
                return;
            }

            /* It is an image file and we don't have it in our list, add it
             * sorted to our list, don't free changed file, that will happen
             * when it is removed */
            backdrop->priv->image_files = g_list_insert_sorted(backdrop->priv->image_files,
                                                               changed_file,
                                                               (GCompareFunc)compare_by_collate_key);
            break;
        case G_FILE_MONITOR_EVENT_DELETED:
            changed_file = g_file_get_path(file);

            /* find the file in the list */
            item = g_list_find_custom(backdrop->priv->image_files,
                                      changed_file,
                                      (GCompareFunc)g_strcmp0);

            /* remove it */
            if(item)
                backdrop->priv->image_files = g_list_delete_link(backdrop->priv->image_files, item);

            g_free(changed_file);
            break;
        default:
            break;
    }
}

/* Returns a GList of all the image files in the parent directory of filename */
static GList *
list_image_files_in_dir(const gchar *filename)
{
    GDir *dir;
    gboolean needs_slash = TRUE;
    const gchar *file;
    GList *files = NULL;
    gchar *dir_name;

    dir_name = g_path_get_dirname(filename);

    dir = g_dir_open(dir_name, 0, 0);
    if(!dir) {
        g_free(dir_name);
        return NULL;
    }

    if(dir_name[strlen(dir_name)-1] == '/')
        needs_slash = FALSE;

    while((file = g_dir_read_name(dir))) {
        gchar *current_file = g_strdup_printf(needs_slash ? "%s/%s" : "%s%s",
                                              dir_name, file);
        if(xfdesktop_image_file_is_valid(current_file))
            files = g_list_insert_sorted(files, current_file, (GCompareFunc)compare_by_collate_key);
        else
            g_free(current_file);
    }

    g_dir_close(dir);
    g_free(dir_name);

    return files;
}

static void
xfce_backdrop_load_image_files(XfceBackdrop *backdrop)
{
    /* generate the image_files list if it doesn't exist and monitor that
     * directory so we can update the list */
    if(backdrop->priv->image_files == NULL && backdrop->priv->image_path) {
        gchar *dir_name = g_path_get_dirname(backdrop->priv->image_path);
        GFile *gfile = g_file_new_for_path(dir_name);

        backdrop->priv->image_files = list_image_files_in_dir(backdrop->priv->image_path);

        if(backdrop->priv->monitor)
            g_object_unref(backdrop->priv->monitor);

        /* monitor the new directory for changes */
        backdrop->priv->monitor = g_file_monitor(gfile, G_FILE_MONITOR_NONE, NULL, NULL);
        g_signal_connect(backdrop->priv->monitor, "changed",
                         G_CALLBACK(cb_xfce_backdrop__image_files_changed),
                         backdrop);

        g_free(dir_name);
        g_object_unref(gfile);
    }
}

/* Gets the next valid image file in the folder. Free when done using it
 * returns NULL on fail. */
gchar *
xfce_backdrop_choose_next(XfceBackdrop *backdrop)
{
    GList *current_file;
    const gchar *filename;

    TRACE("entering");

    g_return_val_if_fail(XFCE_IS_BACKDROP(backdrop), NULL);

    filename = backdrop->priv->image_path;

    if(backdrop->priv->image_files == NULL)
        xfce_backdrop_load_image_files(backdrop);

    if(!backdrop->priv->image_files)
        return NULL;

    /* Get the our current background in the list */
    current_file = g_list_find_custom(backdrop->priv->image_files, filename, (GCompareFunc)g_strcmp0);

    /* if somehow we don't have a valid file, grab the first one available */
    if(current_file == NULL) {
        current_file = g_list_first(backdrop->priv->image_files);
    } else {
        /* We want the next valid image file in the dir */
        current_file = g_list_next(current_file);

        /* we hit the end of the list, wrap around to the front */
        if(current_file == NULL)
            current_file = g_list_first(backdrop->priv->image_files);
    }

    /* return a copy of our new item */
    return g_strdup(current_file->data);
}

/* Gets a random valid image file in the folder. Free when done using it.
 * returns NULL on fail. */
gchar *
xfce_backdrop_choose_random(XfceBackdrop *backdrop)
{
    static gint previndex = -1;
    gint n_items = 0, cur_file;

    TRACE("entering");

    g_return_val_if_fail(XFCE_IS_BACKDROP(backdrop), NULL);

    xfce_backdrop_load_image_files(backdrop);

    if(!backdrop->priv->image_files)
        return NULL;

    n_items = g_list_length(backdrop->priv->image_files);

    /* If there's only 1 item, just return it, easy */
    if(1 == n_items) {
        return g_strdup(g_list_first(backdrop->priv->image_files)->data);
    }

    do {
        /* g_random_int_range bounds to n_items-1 */
        cur_file = g_random_int_range(0, n_items);
    } while(cur_file == previndex && G_LIKELY(previndex != -1));

    previndex = cur_file;

    /* return a copy of the new random item */
    return g_strdup(g_list_nth(backdrop->priv->image_files, cur_file)->data);
}

/* Provides a mapping of image files in the parent folder of file. It selects
 * the image based on the hour of the day scaled for how many images are in
 * the directory, using the first 24 if there are more.
 * Returns a new image path or NULL on failure. Free when done using it. */
gchar *
xfce_backdrop_choose_chronological(XfceBackdrop *backdrop)
{
    GDateTime *datetime;
    GList *new_file;
    gint n_items = 0, epoch;

    TRACE("entering");

    g_return_val_if_fail(XFCE_IS_BACKDROP(backdrop), NULL);

    xfce_backdrop_load_image_files(backdrop);

    if(!backdrop->priv->image_files)
        return NULL;

    n_items = g_list_length(backdrop->priv->image_files);

    /* If there's only 1 item, just return it, easy */
    if(1 == n_items) {
        return g_strdup(g_list_first(backdrop->priv->image_files)->data);
    }

    datetime = g_date_time_new_now_local();

    /* Figure out which image to display based on what time of day it is
     * and how many images we have to work with */
    epoch = (gdouble)g_date_time_get_hour(datetime) / (24.0f / MIN(n_items, 24.0f));
    DBG("epoch %d, hour %d, items %d", epoch, g_date_time_get_hour(datetime), n_items);

    new_file = g_list_nth(backdrop->priv->image_files, epoch);

    g_date_time_unref(datetime);

    /* return a copy of our new file */
    return g_strdup(new_file->data);
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
    backdrop_signals[BACKDROP_READY] = g_signal_new("ready",
            G_OBJECT_CLASS_TYPE(gobject_class), G_SIGNAL_RUN_FIRST,
            G_STRUCT_OFFSET(XfceBackdropClass, ready), NULL, NULL,
            g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

#define XFDESKTOP_PARAM_FLAGS  (G_PARAM_READWRITE \
                                | G_PARAM_CONSTRUCT \
                                | G_PARAM_STATIC_NAME \
                                | G_PARAM_STATIC_NICK \
                                | G_PARAM_STATIC_BLURB)

    /* Defaults to an invalid color style so that
     * xfce_workspace_migrate_backdrop_color_style can handle it properly */
    g_object_class_install_property(gobject_class, PROP_COLOR_STYLE,
                                    g_param_spec_enum("color-style",
                                                      "color style",
                                                      "color style",
                                                      XFCE_TYPE_BACKDROP_COLOR_STYLE,
                                                      XFCE_BACKDROP_COLOR_INVALID,
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

    /* Defaults to an invalid image style so that
     * xfce_workspace_migrate_backdrop_image_style can handle it properly */
    g_object_class_install_property(gobject_class, PROP_IMAGE_STYLE,
                                    g_param_spec_enum("image-style",
                                                      "image style",
                                                      "image style",
                                                      XFCE_TYPE_BACKDROP_IMAGE_STYLE,
                                                      XFCE_BACKDROP_IMAGE_INVALID,
                                                      XFDESKTOP_PARAM_FLAGS));

    /* The DEFAULT_BACKDROP is provided in the function
     * xfce_workspace_migrate_backdrop_image instead of here */
    g_object_class_install_property(gobject_class, PROP_IMAGE_FILENAME,
                                    g_param_spec_string("image-filename",
                                                        "image filename",
                                                        "image filename",
                                                        NULL,
                                                        XFDESKTOP_PARAM_FLAGS));

    g_object_class_install_property(gobject_class, PROP_BACKDROP_CYCLE_ENABLE,
                                    g_param_spec_boolean("backdrop-cycle-enable",
                                                         "backdrop-cycle-enable",
                                                         "backdrop-cycle-enable",
                                                         FALSE,
                                                         XFDESKTOP_PARAM_FLAGS));

    g_object_class_install_property(gobject_class, PROP_BACKDROP_CYCLE_PERIOD,
                                    g_param_spec_enum("backdrop-cycle-period",
                                                      "backdrop-cycle-period",
                                                      "backdrop-cycle-period",
                                                      XFCE_TYPE_BACKDROP_CYCLE_PERIOD,
                                                      XFCE_BACKDROP_PERIOD_MINUES,
                                                      XFDESKTOP_PARAM_FLAGS));

    g_object_class_install_property(gobject_class, PROP_BACKDROP_CYCLE_TIMER,
                                    g_param_spec_uint("backdrop-cycle-timer",
                                                      "backdrop-cycle-timer",
                                                      "backdrop-cycle-timer",
                                                      0, G_MAXUSHORT, 10,
                                                      XFDESKTOP_PARAM_FLAGS));

    g_object_class_install_property(gobject_class, PROP_BACKDROP_RANDOM_ORDER,
                                    g_param_spec_boolean("backdrop-cycle-random-order",
                                                         "backdrop-cycle-random-order",
                                                         "backdrop-cycle-random-order",
                                                         FALSE,
                                                         XFDESKTOP_PARAM_FLAGS));

#undef XFDESKTOP_PARAM_FLAGS
}

static void
xfce_backdrop_init(XfceBackdrop *backdrop)
{
    backdrop->priv = G_TYPE_INSTANCE_GET_PRIVATE(backdrop, XFCE_TYPE_BACKDROP,
                                                 XfceBackdropPriv);
    backdrop->priv->cycle_timer_id = 0;

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

    xfce_backdrop_clear_cached_image(backdrop);

    /* Free the image files list */
    if(backdrop->priv->image_files) {
        g_list_free_full(backdrop->priv->image_files, g_free);
        backdrop->priv->image_files = NULL;
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

        case PROP_IMAGE_STYLE:
            xfce_backdrop_set_image_style(backdrop, g_value_get_enum(value));
            break;

        case PROP_IMAGE_FILENAME:
            xfce_backdrop_set_image_filename(backdrop,
                                             g_value_get_string(value));
            break;

        case PROP_BACKDROP_CYCLE_ENABLE:
            xfce_backdrop_set_cycle_backdrop(backdrop, g_value_get_boolean(value));
            break;

        case PROP_BACKDROP_CYCLE_PERIOD:
            xfce_backdrop_set_cycle_period(backdrop, g_value_get_enum(value));
            break;

        case PROP_BACKDROP_CYCLE_TIMER:
            xfce_backdrop_set_cycle_timer(backdrop, g_value_get_uint(value));
            break;

        case PROP_BACKDROP_RANDOM_ORDER:
            xfce_backdrop_set_random_order(backdrop, g_value_get_boolean(value));
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

        case PROP_IMAGE_STYLE:
            g_value_set_enum(value, xfce_backdrop_get_image_style(backdrop));
            break;

        case PROP_IMAGE_FILENAME:
            g_value_set_string(value,
                               xfce_backdrop_get_image_filename(backdrop));
            break;

        case PROP_BACKDROP_CYCLE_ENABLE:
            g_value_set_boolean(value, xfce_backdrop_get_cycle_backdrop(backdrop));
            break;

        case PROP_BACKDROP_CYCLE_PERIOD:
            g_value_set_enum(value, xfce_backdrop_get_cycle_period(backdrop));
            break;

        case PROP_BACKDROP_CYCLE_TIMER:
            g_value_set_uint(value, xfce_backdrop_get_cycle_timer(backdrop));
            break;

        case PROP_BACKDROP_RANDOM_ORDER:
            g_value_set_boolean(value, xfce_backdrop_get_random_order(backdrop));
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
    backdrop->priv->bpp = gdk_visual_get_depth(visual);
    
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
    
    backdrop->priv->bpp = gdk_visual_get_depth(visual);
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

    if(backdrop->priv->width != width ||
       backdrop->priv->height != height) {
        xfce_backdrop_clear_cached_image(backdrop);
        backdrop->priv->width = width;
        backdrop->priv->height = height;
    }
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
    g_return_if_fail((int)style >= -1 && style <= XFCE_BACKDROP_COLOR_TRANSPARENT);

    if(style != backdrop->priv->color_style) {
        xfce_backdrop_clear_cached_image(backdrop);
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
        xfce_backdrop_clear_cached_image(backdrop);
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
        xfce_backdrop_clear_cached_image(backdrop);
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
 * xfce_backdrop_set_image_style:
 * @backdrop: An #XfceBackdrop.
 * @style: An XfceBackdropImageStyle.
 *
 * Sets the image style to be used for the #XfceBackdrop.
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
        xfce_backdrop_clear_cached_image(backdrop);
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
 * use this call with a @filename argument of %NULL. Makes a copy of @filename.
 **/
void
xfce_backdrop_set_image_filename(XfceBackdrop *backdrop, const gchar *filename)
{
    gchar *old_dir = NULL, *new_dir = NULL;
    g_return_if_fail(XFCE_IS_BACKDROP(backdrop));

    TRACE("entering, filename %s", filename);

    /* Don't do anything if the filename doesn't change */
    if(g_strcmp0(backdrop->priv->image_path, filename) == 0)
        return;

    /* We need to free the image_files if image_path changed directories */
    if(backdrop->priv->image_files) {
        if(backdrop->priv->image_path)
            old_dir = g_path_get_dirname(backdrop->priv->image_path);
        if(filename)
            new_dir = g_path_get_dirname(filename);

        /* Directories did change, free list */
        if(g_strcmp0(old_dir, new_dir) != 0) {
            g_list_free_full(backdrop->priv->image_files, g_free);
            backdrop->priv->image_files = NULL;
        }

        g_free(old_dir);
        g_free(new_dir);
    }

    /* Now we can free the old path and setup the new one */
    g_free(backdrop->priv->image_path);
    
    if(filename)
        backdrop->priv->image_path = g_strdup(filename);
    else
        backdrop->priv->image_path = NULL;

    xfce_backdrop_clear_cached_image(backdrop);

    g_signal_emit(G_OBJECT(backdrop), backdrop_signals[BACKDROP_CHANGED], 0);
}

G_CONST_RETURN gchar *
xfce_backdrop_get_image_filename(XfceBackdrop *backdrop)
{
    g_return_val_if_fail(XFCE_IS_BACKDROP(backdrop), NULL);
    return backdrop->priv->image_path;
}

static void
xfce_backdrop_cycle_backdrop(XfceBackdrop *backdrop)
{
    XfceBackdropCyclePeriod period;
    gchar *new_backdrop;

    TRACE("entering");

    g_return_if_fail(XFCE_IS_BACKDROP(backdrop));

    period = backdrop->priv->cycle_period;

    /* sanity checks */
    if(backdrop->priv->image_path == NULL || !backdrop->priv->cycle_backdrop)
        return;

    if(period == XFCE_BACKDROP_PERIOD_CHRONOLOGICAL) {
        /* chronological first */
        new_backdrop = xfce_backdrop_choose_chronological(backdrop);
    } else if(backdrop->priv->random_backdrop_order) {
        /* then random */
        new_backdrop = xfce_backdrop_choose_random(backdrop);
    } else {
        /* sequential, the default */
        new_backdrop = xfce_backdrop_choose_next(backdrop);
    }

    /* Only emit the cycle signal if something changed */
    if(g_strcmp0(backdrop->priv->image_path, new_backdrop) != 0) {
        xfce_backdrop_set_image_filename(backdrop, new_backdrop);
        g_signal_emit(G_OBJECT(backdrop), backdrop_signals[BACKDROP_CYCLE], 0);
    }

    g_free(new_backdrop);
}

static void
xfce_backdrop_remove_backdrop_timer(XfceBackdrop *backdrop)
{
    g_return_if_fail(XFCE_IS_BACKDROP(backdrop));

    /* always remove the old timer */
    if(backdrop->priv->cycle_timer_id != 0) {
        g_source_remove(backdrop->priv->cycle_timer_id);
        backdrop->priv->cycle_timer_id = 0;
    }
}

static gboolean
xfce_backdrop_timer(XfceBackdrop *backdrop)
{
    GDateTime *local_time = NULL;
    gint hour, minute, second;
    guint cycle_interval = 0;

    TRACE("entering");

    g_return_val_if_fail(XFCE_IS_BACKDROP(backdrop), FALSE);

    /* Don't bother with trying to cycle a backdrop if we're not using images */
    if(backdrop->priv->image_style != XFCE_BACKDROP_IMAGE_NONE)
        xfce_backdrop_cycle_backdrop(backdrop);

    /* Now to complicate things; we have to handle some special cases */
    switch(backdrop->priv->cycle_period) {
        case XFCE_BACKDROP_PERIOD_STARTUP:
            /* no more cycling */
            return FALSE;

        case XFCE_BACKDROP_PERIOD_CHRONOLOGICAL:
        case XFCE_BACKDROP_PERIOD_HOURLY:
            local_time = g_date_time_new_now_local();
            minute = g_date_time_get_minute(local_time);
            second = g_date_time_get_second(local_time);

            /* find out how long until the next hour so we cycle on the hour */
            cycle_interval = ((59 - minute) * 60) + (60 - second);
            break;

        case XFCE_BACKDROP_PERIOD_DAILY:
            local_time = g_date_time_new_now_local();
            hour   = g_date_time_get_hour  (local_time);
            minute = g_date_time_get_minute(local_time);
            second = g_date_time_get_second(local_time);

            /* find out how long until the next day so we cycle on the day */
            cycle_interval = ((23 - hour) * 60 * 60) + ((59 - minute) * 60) + (60 - second);
            break;

        default:
            /* no changes required */
            break;
    }

    /* Update the timer if we're trying to keep things on the hour/day */
    if(cycle_interval != 0) {
        /* remove old timer first */
        xfce_backdrop_remove_backdrop_timer(backdrop);

        DBG("calling g_timeout_add_seconds, interval is %d", cycle_interval);
        backdrop->priv->cycle_timer_id = g_timeout_add_seconds(cycle_interval,
                                                               (GSourceFunc)xfce_backdrop_timer,
                                                               backdrop);

        if(local_time != NULL)
            g_date_time_unref(local_time);

        /* We created a new instance, kill this one */
        return FALSE;
    }

    /* continue cycling (for seconds, minutes, hours, etc) */
    return TRUE;
}

/**
 * xfce_backdrop_set_cycle_timer:
 * @backdrop: An #XfceBackdrop.
 * @cycle_timer: The amount of time, based on the cycle_period, to wait before
 *               changing the background image.
 *
 * If cycle_backdrop is enabled then this function will change the backdrop
 * image based on the cycle_timer and cycle_period. The cycle_timer is a value
 * between 0 and G_MAXUSHORT where a value of 0 indicates that the backdrop
 * should not be changed.
 **/
void
xfce_backdrop_set_cycle_timer(XfceBackdrop *backdrop, guint cycle_timer)
{
    GDateTime *local_time = NULL;
    guint cycle_interval = 0;
    gint hour, minute, second;

    g_return_if_fail(XFCE_IS_BACKDROP(backdrop));

    TRACE("entering, cycle_timer = %d", cycle_timer);

    /* Sanity check to help prevent overflows */
    if(cycle_timer > G_MAXUSHORT)
        cycle_timer = G_MAXUSHORT;

    backdrop->priv->cycle_timer = cycle_timer;

    /* remove old timer first */
    xfce_backdrop_remove_backdrop_timer(backdrop);

    if(backdrop->priv->cycle_timer != 0 && backdrop->priv->cycle_backdrop == TRUE) {
        switch(backdrop->priv->cycle_period) {
            case XFCE_BACKDROP_PERIOD_SECONDS:
                cycle_interval = backdrop->priv->cycle_timer;
                break;

            case XFCE_BACKDROP_PERIOD_MINUES:
                cycle_interval = backdrop->priv->cycle_timer * 60;
                break;

            case XFCE_BACKDROP_PERIOD_HOURS:
                cycle_interval = backdrop->priv->cycle_timer * 60 * 60;
                break;

            case XFCE_BACKDROP_PERIOD_CHRONOLOGICAL:
            case XFCE_BACKDROP_PERIOD_STARTUP:
                /* Startup and chronological will be triggered at once */
                cycle_interval = 1;
                break;

            case XFCE_BACKDROP_PERIOD_HOURLY:
                local_time = g_date_time_new_now_local();
                minute = g_date_time_get_minute(local_time);
                second = g_date_time_get_second(local_time);

                /* find out how long until the next hour so we cycle on the hour */
                cycle_interval = ((59 - minute) * 60) + (60 - second);
                break;

            case XFCE_BACKDROP_PERIOD_DAILY:
                local_time = g_date_time_new_now_local();
                hour   = g_date_time_get_hour  (local_time);
                minute = g_date_time_get_minute(local_time);
                second = g_date_time_get_second(local_time);

                /* find out how long until the next day so we cycle on the day */
                cycle_interval = ((23 - hour) * 60 * 60) + ((59 - minute) * 60) + (60 - second);
                break;

            default:
                g_critical("Unknown backdrop-cycle-period set");
                break;
            }

        if(cycle_interval != 0) {
            DBG("calling g_timeout_add_seconds, interval is %d", cycle_interval);
            backdrop->priv->cycle_timer_id = g_timeout_add_seconds(cycle_interval,
                                                                   (GSourceFunc)xfce_backdrop_timer,
                                                                   backdrop);
        }
    }

    if(local_time != NULL)
        g_date_time_unref(local_time);
}

guint
xfce_backdrop_get_cycle_timer(XfceBackdrop *backdrop)
{
    g_return_val_if_fail(XFCE_IS_BACKDROP(backdrop), 0);
    return backdrop->priv->cycle_timer;
}

/**
 * xfce_backdrop_set_cycle_backdrop:
 * @backdrop: An #XfceBackdrop.
 * @cycle_backdrop: True to cycle backdrops every cycle_timer minutes
 *
 * Setting cycle_backdrop to TRUE will begin cycling this backdrop based on
 * the cycle_timer value set in xfce_backdrop_set_cycle_timer.
 **/
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

    /* If we're not cycling anymore, free the image files list */
    if(!backdrop->priv->cycle_backdrop && backdrop->priv->image_files) {
        g_list_free_full(backdrop->priv->image_files, g_free);
        backdrop->priv->image_files = NULL;
    }
}

gboolean
xfce_backdrop_get_cycle_backdrop(XfceBackdrop *backdrop)
{
    g_return_val_if_fail(XFCE_IS_BACKDROP(backdrop), 0);

    return backdrop->priv->cycle_backdrop;
}

/**
 * xfce_backdrop_set_cycle_period:
 * @backdrop: An #XfceBackdrop.
 * @period: Determines how often the backdrop will cycle.
 *
 * When cycling backdrops, the period setting will determine how fast the timer
 * will fire (seconds vs minutes) or if the periodic backdrop will be based on
 * actual wall clock values or just at xfdesktop's start up.
 **/
void
xfce_backdrop_set_cycle_period(XfceBackdrop *backdrop,
                               XfceBackdropCyclePeriod period)
{
    g_return_if_fail(XFCE_IS_BACKDROP(backdrop));
    g_return_if_fail(period != XFCE_BACKDROP_PERIOD_INVALID);

    TRACE("entering");

    if(backdrop->priv->cycle_period != period) {
        backdrop->priv->cycle_period = period;
        /* Start or stop the backdrop changing */
        xfce_backdrop_set_cycle_timer(backdrop,
                                      xfce_backdrop_get_cycle_timer(backdrop));
    }
}

XfceBackdropCyclePeriod
xfce_backdrop_get_cycle_period(XfceBackdrop *backdrop)
{
    g_return_val_if_fail(XFCE_IS_BACKDROP(backdrop), XFCE_BACKDROP_PERIOD_INVALID);

    return backdrop->priv->cycle_period;
}

/**
 * xfce_backdrop_set_random_order:
 * @backdrop: An #XfceBackdrop.
 * @random_order: When TRUE and the backdrops are set to cycle, a random image
 *                image will be choosen when it cycles.
 *
 * When cycling backdrops, they will either be show sequentially (and this value
 * will be FALSE) or they will be selected at random. The images are choosen from
 * the same folder the current backdrop image file is in.
 **/
void
xfce_backdrop_set_random_order(XfceBackdrop *backdrop,
                               gboolean random_order)
{
    g_return_if_fail(XFCE_IS_BACKDROP(backdrop));

    TRACE("entering");

    backdrop->priv->random_backdrop_order = random_order;
}

gboolean
xfce_backdrop_get_random_order(XfceBackdrop *backdrop)
{
    g_return_val_if_fail(XFCE_IS_BACKDROP(backdrop), FALSE);

    return backdrop->priv->random_backdrop_order;
}

/**
 * xfce_backdrop_set_cache_pixbuf:
 * @backdrop: An #XfceBackdrop.
 * @cache_pixbuf: When TRUE XfceBackdrop will keep a reference to the current
 *                pixbuf until another one is set.
 *
 * XfceBackdrop can be set to keep a reference to the current backdrop and
 * increment the reference count when xfce_backdrop_get_pixbuf is called.
 * Setting cache_pixbuf to FALSE will cause xfce_backdrop_get_pixbuf to
 * return XfceBackdrop's only reference and free any memory allocated. This
 * will use less memory, especially when the backdrop isn't expected to change
 * as in single workspace mode.
 **/
void
xfce_backdrop_set_cache_pixbuf(XfceBackdrop *backdrop,
                               gboolean cache_pixbuf)
{
    g_return_if_fail(XFCE_IS_BACKDROP(backdrop));

    TRACE("entering");

    if(backdrop->priv->cache_pixbuf == cache_pixbuf) {
        DBG("No change, cache_pixbuf %s", cache_pixbuf ? "TRUE" : "FALSE");
        return;
    }

    backdrop->priv->cache_pixbuf = cache_pixbuf;

    DBG("cache_pixbuf now %s", cache_pixbuf ? "TRUE" : "FALSE");

    /* release any cached pixbuf now to save memory */
    if(!cache_pixbuf)
        xfce_backdrop_clear_cached_image(backdrop);
}

gboolean
xfce_backdrop_get_cache_pixbuf(XfceBackdrop *backdrop)
{
    g_return_val_if_fail(XFCE_IS_BACKDROP(backdrop), FALSE);

    return backdrop->priv->cache_pixbuf;
}

/* Generates the background that will either be displayed or will have the
 * image drawn on top of */
static GdkPixbuf *
xfce_backdrop_generate_canvas(XfceBackdrop *backdrop)
{
    gint w, h;
    GdkPixbuf *final_image;

    w = backdrop->priv->width;
    h = backdrop->priv->height;

    /* In case we somehow end up here, give a warning and apply a temp fix */
    if(backdrop->priv->color_style == XFCE_BACKDROP_COLOR_INVALID) {
        g_warning("xfce_backdrop_generate_canvas: Invalid color style");
        backdrop->priv->color_style = XFCE_BACKDROP_COLOR_SOLID;
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

    return final_image;
}

static void
xfce_backdrop_image_data_release(XfceBackdropImageData *image_data)
{
    TRACE("entering");

    if(!image_data)
        return;

    /* Only set the backdrop's image_data to NULL if it's current */
    if(image_data->backdrop->priv->image_data == image_data)
        image_data->backdrop->priv->image_data = NULL;

    if(image_data->cancellable)
        g_object_unref(image_data->cancellable);

    if(image_data->image_buffer)
        g_free(image_data->image_buffer);

    if(image_data->loader)
        g_object_unref(image_data->loader);

    g_free(image_data);
    image_data = NULL;

}

/**
 * xfce_backdrop_get_pixbuf:
 * @backdrop: An #XfceBackdrop.
 *
 * Returns the composited backdrop image if one has been generated. If it
 * returns NULL, call xfce_backdrop_generate_async to create the pixbuf.
 * Free with g_object_unref() when you are finished.
 **/
GdkPixbuf *
xfce_backdrop_get_pixbuf(XfceBackdrop *backdrop)
{
    TRACE("entering");

    if(backdrop->priv->pix && backdrop->priv->cache_pixbuf) {
        /* return a reference so we can cache it */
        return g_object_ref(backdrop->priv->pix);
    } else if(backdrop->priv->pix) {
        /* We're not going to cache it so return our reference and set our
         * pointer to NULL */
        GdkPixbuf *pix = backdrop->priv->pix;
        backdrop->priv->pix = NULL;

        return pix;
    }

    /* !backdrop->priv->pix, call xfce_backdrop_generate_async */
    return NULL;
}

/**
 * xfce_backdrop_generate_async:
 * @backdrop: An #XfceBackdrop.
 *
 * Generates the final composited, resized image from the #XfceBackdrop.
 * Emits the "ready" signal when the image has been created.
 **/
void
xfce_backdrop_generate_async(XfceBackdrop *backdrop)
{
    GFile *file;
    XfceBackdropImageData *image_data = NULL;
    const gchar *image_path;

    TRACE("entering");

    if(backdrop->priv->width == 0 || backdrop->priv->height == 0) {
        g_critical("attempting to create a backdrop without setting the width/height");
        return;
    }

    if(backdrop->priv->image_data && backdrop->priv->image_data->cancellable) {
        g_cancellable_cancel(backdrop->priv->image_data->cancellable);
        backdrop->priv->image_data = NULL;
    }

    /* If we aren't going to display an image then just create the canvas */
    if(backdrop->priv->image_style == XFCE_BACKDROP_IMAGE_NONE) {
        backdrop->priv->pix = xfce_backdrop_generate_canvas(backdrop);
        g_signal_emit(G_OBJECT(backdrop), backdrop_signals[BACKDROP_READY], 0);
        return;
    }

    /* If we're trying to display an image, attempt to use the one the user
     * set. If there's none set at all, fall back to our default */
    if(backdrop->priv->image_path != NULL)
        image_path = backdrop->priv->image_path;
    else
        image_path = DEFAULT_BACKDROP;

    DBG("loading image %s", image_path);

    file = g_file_new_for_path(image_path);

    image_data = g_new0(XfceBackdropImageData, 1);
    backdrop->priv->image_data = image_data;

    image_data->backdrop = backdrop;
    image_data->loader = gdk_pixbuf_loader_new();
    image_data->cancellable = g_cancellable_new();
    image_data->image_buffer = g_new0(guchar, XFCE_BACKDROP_BUFFER_SIZE);

    g_signal_connect(image_data->loader, "size-prepared", G_CALLBACK(xfce_backdrop_loader_size_prepared_cb), image_data);
    g_signal_connect(image_data->loader, "closed", G_CALLBACK(xfce_backdrop_loader_closed_cb), image_data);

    g_file_read_async(file,
                      G_PRIORITY_DEFAULT,
                      image_data->cancellable,
                      xfce_backdrop_file_ready_cb,
                      image_data);
}


static void
xfce_backdrop_loader_size_prepared_cb(GdkPixbufLoader *loader,
                                      gint width,
                                      gint height,
                                      gpointer user_data)
{
    XfceBackdropImageData *image_data = user_data;
    XfceBackdrop *backdrop = image_data->backdrop;
    gdouble xscale, yscale;

    TRACE("entering");

    if(backdrop->priv->image_style == XFCE_BACKDROP_IMAGE_INVALID) {
        g_warning("Invalid image style, setting to XFCE_BACKDROP_IMAGE_STRETCHED");
        backdrop->priv->image_style = XFCE_BACKDROP_IMAGE_STRETCHED;
    }

    switch(backdrop->priv->image_style) {
        case XFCE_BACKDROP_IMAGE_CENTERED:
        case XFCE_BACKDROP_IMAGE_TILED:
            /* do nothing */
            break;

        case XFCE_BACKDROP_IMAGE_STRETCHED:
            gdk_pixbuf_loader_set_size(loader,
                                       backdrop->priv->width,
                                       backdrop->priv->height);
            break;

        case XFCE_BACKDROP_IMAGE_SCALED:
            xscale = (gdouble)backdrop->priv->width / width;
            yscale = (gdouble)backdrop->priv->height / height;
            if(xscale < yscale) {
                yscale = xscale;
            } else {
                xscale = yscale;
            }

            gdk_pixbuf_loader_set_size(loader,
                                       width * xscale,
                                       height * yscale);
            break;

        case XFCE_BACKDROP_IMAGE_ZOOMED:
        case XFCE_BACKDROP_IMAGE_SPANNING_SCREENS:
            xscale = (gdouble)backdrop->priv->width / width;
            yscale = (gdouble)backdrop->priv->height / height;
            if(xscale < yscale) {
                xscale = yscale;
            } else {
                yscale = xscale;
            }

            gdk_pixbuf_loader_set_size(loader,
                                       width * xscale,
                                       height * yscale);
            break;

        default:
            g_critical("Invalid image style: %d\n", (gint)backdrop->priv->image_style);
    }
}

static void
xfce_backdrop_loader_closed_cb(GdkPixbufLoader *loader,
                               XfceBackdropImageData *image_data)
{
    XfceBackdrop *backdrop = image_data->backdrop;
    GdkPixbuf *final_image, *image, *tmp;
    gint i, j;
    gint w, h, iw = 0, ih = 0;
    XfceBackdropImageStyle istyle;
    gint dx, dy, xo, yo;
    gdouble xscale, yscale;
    GdkInterpType interp;

    TRACE("entering");

    g_return_if_fail(XFCE_IS_BACKDROP(backdrop));

    /* canceled? quit now */
    if(g_cancellable_is_cancelled(image_data->cancellable)) {
        xfce_backdrop_image_data_release(image_data);
        return;
    }

    image = gdk_pixbuf_loader_get_pixbuf(loader);
    if(image) {
        iw = gdk_pixbuf_get_width(image);
        ih = gdk_pixbuf_get_height(image);
    }

    if(backdrop->priv->width == 0 || backdrop->priv->height == 0) {
        w = iw;
        h = ih;
    } else {
        w = backdrop->priv->width;
        h = backdrop->priv->height;
    }
    
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

    final_image = xfce_backdrop_generate_canvas(backdrop);

    /* no image and not canceled? return just the canvas */
    if(!image && !g_cancellable_is_cancelled(image_data->cancellable)) {
        DBG("image failed to load, displaying canvas only");
        backdrop->priv->pix = final_image;

        g_signal_emit(G_OBJECT(backdrop), backdrop_signals[BACKDROP_READY], 0);

        backdrop->priv->image_data = NULL;
        xfce_backdrop_image_data_release(image_data);
        return;
    }

    switch(istyle) {
        case XFCE_BACKDROP_IMAGE_CENTERED:
            dx = MAX((w - iw) / 2, 0);
            dy = MAX((h - ih) / 2, 0);
            xo = MIN((w - iw) / 2, dx);
            yo = MIN((h - ih) / 2, dy);
            gdk_pixbuf_composite(image, final_image, dx, dy,
                    MIN(w, iw), MIN(h, ih), xo, yo, 1.0, 1.0,
                    interp, 255);
            break;
        
        case XFCE_BACKDROP_IMAGE_TILED:
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

            gdk_pixbuf_composite(image, final_image, dx, dy,
                    iw * xscale, ih * yscale, xo, yo, 1, 1,
                    interp, 255);
            break;
        
        case XFCE_BACKDROP_IMAGE_ZOOMED:
        case XFCE_BACKDROP_IMAGE_SPANNING_SCREENS:
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

            gdk_pixbuf_composite(image, final_image, 0, 0,
                    w, h, xo, yo, 1, 1, interp, 255);
            break;
        
        default:
            g_critical("Invalid image style: %d\n", (gint)istyle);
    }

    /* keep the backdrop and emit the signal if it hasn't been canceled */
    if(!g_cancellable_is_cancelled(image_data->cancellable)) {
        backdrop->priv->pix = final_image;
        g_signal_emit(G_OBJECT(backdrop), backdrop_signals[BACKDROP_READY], 0);
    }

    backdrop->priv->image_data = NULL;
    xfce_backdrop_image_data_release(image_data);
}

static void
xfce_backdrop_file_ready_cb(GObject *source_object,
                            GAsyncResult *res,
                            gpointer user_data)
{
    GFile *file = G_FILE(source_object);
    XfceBackdropImageData *image_data = user_data;
    GFileInputStream *input_stream = g_file_read_finish(file, res, NULL);

    TRACE("entering");

    /* Finished with the file, clean it up */
    g_object_unref(file);

    /* If this fails then close the loader, it will only display the selected
     * backdrop color */
    if(input_stream == NULL) {
        gdk_pixbuf_loader_close(image_data->loader, NULL);
        return;
    }

    g_input_stream_read_async(G_INPUT_STREAM(input_stream),
                              image_data->image_buffer,
                              XFCE_BACKDROP_BUFFER_SIZE,
                              G_PRIORITY_LOW,
                              image_data->cancellable,
                              xfce_backdrop_file_input_stream_ready_cb,
                              image_data);
}

static void
xfce_backdrop_file_input_stream_ready_cb(GObject *source_object,
                                         GAsyncResult *res,
                                         gpointer user_data)
{
    GInputStream *stream = G_INPUT_STREAM(source_object);
    XfceBackdropImageData *image_data = user_data;
    gssize bytes = g_input_stream_read_finish(stream, res, NULL);

    if(bytes == -1 || bytes == 0) {
        /* If there was an error reading the stream or it completed, clean
         * up and close the pixbuf loader (which will handle both conditions) */
        g_input_stream_close(stream, image_data->cancellable, NULL);
        g_object_unref(source_object);

        gdk_pixbuf_loader_close(image_data->loader, NULL);
        return;
    }

    if(gdk_pixbuf_loader_write(image_data->loader, image_data->image_buffer, bytes, NULL)) {
        g_input_stream_read_async(stream,
                                  image_data->image_buffer,
                                  XFCE_BACKDROP_BUFFER_SIZE,
                                  G_PRIORITY_LOW,
                                  image_data->cancellable,
                                  xfce_backdrop_file_input_stream_ready_cb,
                                  image_data);
    } else {
        /* If we got here, the loader will be closed, and will not accept
         * further writes. */
        g_input_stream_close(stream, image_data->cancellable, NULL);
        g_object_unref(source_object);
    }
}

/* returns TRUE if they have identical settings. */
gboolean
xfce_backdrop_compare_backdrops(XfceBackdrop *backdrop_a,
                                XfceBackdrop *backdrop_b)
{
    if(g_strcmp0(backdrop_a->priv->image_path, backdrop_b->priv->image_path) != 0) {
        DBG("filename different");
        return FALSE;
    }

    if(backdrop_a->priv->image_style != backdrop_b->priv->image_style) {
        DBG("image_style different");
        return FALSE;
    }

    if(backdrop_a->priv->color_style != backdrop_b->priv->color_style) {
        DBG("color_style different");
        return FALSE;
    }

    /* Every color style uses color1 except for transparent which does not need
     * a color check */
    if(backdrop_a->priv->color_style != XFCE_BACKDROP_COLOR_TRANSPARENT &&
       !gdk_color_equal(&backdrop_a->priv->color1, &backdrop_b->priv->color1)) {
        DBG("colors different");
        return FALSE;
    }

    /* When the style is set to gradient then we should check color2 as well */
    if((backdrop_a->priv->color_style == XFCE_BACKDROP_COLOR_HORIZ_GRADIENT ||
        backdrop_a->priv->color_style == XFCE_BACKDROP_COLOR_VERT_GRADIENT) &&
       !gdk_color_equal(&backdrop_a->priv->color2, &backdrop_b->priv->color2)) {
        DBG("colors different");
        return FALSE;
    }

    if(backdrop_a->priv->cycle_backdrop != backdrop_b->priv->cycle_backdrop ||
       backdrop_a->priv->cycle_backdrop == TRUE) {
        DBG("backdrop cycle different");
        return FALSE;
    }

    return TRUE;
}
