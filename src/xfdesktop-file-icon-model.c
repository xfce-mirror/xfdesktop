/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright(c) 2022 Brian Tarricone, <brian@tarricone.org>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cairo-gobject.h>

#include "xfdesktop-common.h"
#include "xfdesktop-extensions.h"
#include "xfdesktop-file-icon-model.h"
#include "xfdesktop-file-icon.h"
#include "xfdesktop-icon.h"
#include "xfdesktop-regular-file-icon.h"
#include "xfdesktop-special-file-icon.h"
#include "xfdesktop-volume-icon.h"

#define ITER_STAMP 1870614

struct _XfdesktopFileIconModel
{
    GObject parent;

    GList *items;
    GHashTable *icons;

    gint icon_width;
    gint icon_height;
    gint tooltip_icon_size;
    gint scale_factor;
};

struct _XfdesktopFileIconModelClass
{
    GObjectClass parent_class;
};

enum
{
    PROP0 = 0,
    PROP_ICON_WIDTH,
    PROP_ICON_HEIGHT,
    PROP_TOOLTIP_ICON_SIZE,
    PROP_SCALE_FACTOR,
};


static void xfdesktop_file_icon_model_tree_model_init(GtkTreeModelIface *iface);

static void xfdesktop_file_icon_model_set_property(GObject *obj,
                                                   guint prop_id,
                                                   const GValue *value,
                                                   GParamSpec *pspec);
static void xfdesktop_file_icon_model_get_property(GObject *obj,
                                                   guint prop_id,
                                                   GValue *value,
                                                   GParamSpec *pspec);
static void xfdesktop_file_icon_model_finalize(GObject *obj);

static GtkTreeModelFlags xfdesktop_file_icon_model_get_flags(GtkTreeModel *model);
static gint xfdesktop_file_icon_model_get_n_columns(GtkTreeModel *model);
static GType xfdesktop_file_icon_model_get_column_type(GtkTreeModel *model,
                                                       gint column);
static gboolean xfdesktop_file_icon_model_get_iter(GtkTreeModel *model,
                                                   GtkTreeIter *iter,
                                                   GtkTreePath *path);
static GtkTreePath *xfdesktop_file_icon_model_get_path(GtkTreeModel *model,
                                                       GtkTreeIter *iter);
static void xfdesktop_file_icon_model_get_value(GtkTreeModel *model,
                                                GtkTreeIter *iter,
                                                gint column,
                                                GValue *value);
static gboolean xfdesktop_file_icon_model_iter_previous(GtkTreeModel *model,
                                                        GtkTreeIter *iter);
static gboolean xfdesktop_file_icon_model_iter_next(GtkTreeModel *model,
                                                    GtkTreeIter *iter);
static gboolean xfdesktop_file_icon_model_iter_parent(GtkTreeModel *model,
                                                      GtkTreeIter *iter,
                                                      GtkTreeIter *child);
static gboolean xfdesktop_file_icon_model_iter_has_child(GtkTreeModel *model,
                                                         GtkTreeIter *iter);
static gint xfdesktop_file_icon_model_iter_n_children(GtkTreeModel *model,
                                                      GtkTreeIter *iter);
static gboolean xfdesktop_file_icon_model_iter_children(GtkTreeModel *model,
                                                        GtkTreeIter *iter,
                                                        GtkTreeIter *parent);
static gboolean xfdesktop_file_icon_model_iter_nth_child(GtkTreeModel *model,
                                                         GtkTreeIter *iter,
                                                         GtkTreeIter *parent,
                                                         gint n);


G_DEFINE_TYPE_WITH_CODE(XfdesktopFileIconModel,
                        xfdesktop_file_icon_model,
                        G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_MODEL, xfdesktop_file_icon_model_tree_model_init))


static void
xfdesktop_file_icon_model_class_init(XfdesktopFileIconModelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->set_property = xfdesktop_file_icon_model_set_property;
    gobject_class->get_property = xfdesktop_file_icon_model_get_property;
    gobject_class->finalize = xfdesktop_file_icon_model_finalize;

#define PARAM_FLAGS (G_PARAM_READWRITE \
                     | G_PARAM_STATIC_NAME \
                     | G_PARAM_STATIC_NICK \
                     | G_PARAM_STATIC_BLURB)

    g_object_class_install_property(gobject_class,
                                    PROP_ICON_WIDTH,
                                    g_param_spec_int("icon-width",
                                                     "icon-width",
                                                     "width of icon images",
                                                     MIN_ICON_SIZE, MAX_ICON_SIZE, DEFAULT_ICON_SIZE,
                                                     PARAM_FLAGS));

    g_object_class_install_property(gobject_class,
                                    PROP_ICON_HEIGHT,
                                    g_param_spec_int("icon-height",
                                                     "icon-height",
                                                     "height of icon images",
                                                     MIN_ICON_SIZE, MAX_ICON_SIZE, DEFAULT_ICON_SIZE,
                                                     PARAM_FLAGS));

    g_object_class_install_property(gobject_class,
                                    PROP_TOOLTIP_ICON_SIZE,
                                    g_param_spec_int("tooltip-icon-size",
                                                     "tooltip-icon-size",
                                                     "size of tooltip images",
                                                     MIN_TOOLTIP_ICON_SIZE, MAX_TOOLTIP_ICON_SIZE, DEFAULT_TOOLTIP_ICON_SIZE,
                                                     PARAM_FLAGS));

    g_object_class_install_property(gobject_class,
                                    PROP_SCALE_FACTOR,
                                    g_param_spec_int("scale-factor",
                                                     "scale-factor",
                                                     "UI scale factor (used for rendering icons)",
                                                     1, G_MAXINT, 1,
                                                     PARAM_FLAGS));

#undef PARAM_FLAGS
}

static void
xfdesktop_file_icon_model_tree_model_init(GtkTreeModelIface *iface)
{
    iface->get_flags = xfdesktop_file_icon_model_get_flags;
    iface->get_n_columns = xfdesktop_file_icon_model_get_n_columns;
    iface->get_column_type = xfdesktop_file_icon_model_get_column_type;
    iface->get_iter = xfdesktop_file_icon_model_get_iter;
    iface->get_path = xfdesktop_file_icon_model_get_path;
    iface->get_value = xfdesktop_file_icon_model_get_value;
    iface->iter_previous = xfdesktop_file_icon_model_iter_previous;
    iface->iter_next = xfdesktop_file_icon_model_iter_next;
    iface->iter_parent = xfdesktop_file_icon_model_iter_parent;
    iface->iter_has_child = xfdesktop_file_icon_model_iter_has_child;
    iface->iter_n_children = xfdesktop_file_icon_model_iter_n_children;
    iface->iter_children = xfdesktop_file_icon_model_iter_children;
    iface->iter_nth_child = xfdesktop_file_icon_model_iter_nth_child;
}

static void
xfdesktop_file_icon_model_init(XfdesktopFileIconModel *fmodel)
{
    fmodel->icons = g_hash_table_new(xfdesktop_file_icon_hash, xfdesktop_file_icon_equal);
    fmodel->icon_width = DEFAULT_ICON_SIZE;
    fmodel->icon_height = DEFAULT_ICON_SIZE;
    fmodel->tooltip_icon_size = DEFAULT_TOOLTIP_ICON_SIZE;
    fmodel->scale_factor = 1;
}

static void
xfdesktop_file_icon_model_set_property(GObject *obj,
                                       guint prop_id,
                                       const GValue *value,
                                       GParamSpec *pspec)
{
    XfdesktopFileIconModel *fmodel = XFDESKTOP_FILE_ICON_MODEL(obj);

    switch (prop_id) {
        case PROP_ICON_WIDTH:
            xfdesktop_file_icon_model_set_icon_width(fmodel, g_value_get_int(value));
            break;

        case PROP_ICON_HEIGHT:
            xfdesktop_file_icon_model_set_icon_height(fmodel, g_value_get_int(value));
            break;

        case PROP_TOOLTIP_ICON_SIZE:
            xfdesktop_file_icon_model_set_tooltip_icon_size(fmodel, g_value_get_int(value));
            break;

        case PROP_SCALE_FACTOR:
            xfdesktop_file_icon_model_set_scale_factor(fmodel, g_value_get_int(value));
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
            break;
    }
}

static void
xfdesktop_file_icon_model_get_property(GObject *obj,
                                       guint prop_id,
                                       GValue *value,
                                       GParamSpec *pspec)
{
    XfdesktopFileIconModel *fmodel = XFDESKTOP_FILE_ICON_MODEL(obj);

    switch (prop_id) {
        case PROP_ICON_WIDTH:
            g_value_set_int(value, fmodel->icon_width);
            break;

        case PROP_ICON_HEIGHT:
            g_value_set_int(value, fmodel->icon_height);
            break;

        case PROP_TOOLTIP_ICON_SIZE:
            g_value_set_int(value, fmodel->tooltip_icon_size);
            break;

        case PROP_SCALE_FACTOR:
            g_value_set_int(value, fmodel->scale_factor);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
            break;
    }
}

static void
xfdesktop_file_icon_model_finalize(GObject *obj)
{
    XfdesktopFileIconModel *fmodel = XFDESKTOP_FILE_ICON_MODEL(obj);

    g_hash_table_destroy(fmodel->icons);
    g_list_free_full(fmodel->items, g_object_unref);

    G_OBJECT_CLASS(xfdesktop_file_icon_model_parent_class)->finalize(obj);
}

static GtkTreeModelFlags
xfdesktop_file_icon_model_get_flags(GtkTreeModel *model)
{
    return GTK_TREE_MODEL_ITERS_PERSIST | GTK_TREE_MODEL_LIST_ONLY;
}

static gint
xfdesktop_file_icon_model_get_n_columns(GtkTreeModel *model)
{
    return XFDESKTOP_FILE_ICON_MODEL_COLUMN_N_COLUMNS;
}

static GType
xfdesktop_file_icon_model_get_column_type(GtkTreeModel *model,
                                          gint column)
{
    g_return_val_if_fail(column >= 0 && column < XFDESKTOP_FILE_ICON_MODEL_COLUMN_N_COLUMNS, G_TYPE_NONE);

    switch (column) {
        case XFDESKTOP_FILE_ICON_MODEL_COLUMN_SURFACE:
            return CAIRO_GOBJECT_TYPE_SURFACE;
        case XFDESKTOP_FILE_ICON_MODEL_COLUMN_LABEL:
            return G_TYPE_STRING;
        case XFDESKTOP_FILE_ICON_MODEL_COLUMN_ROW:
            return G_TYPE_INT;
        case XFDESKTOP_FILE_ICON_MODEL_COLUMN_COL:
            return G_TYPE_INT;
        case XFDESKTOP_FILE_ICON_MODEL_COLUMN_SORT_PRIORITY:
            return G_TYPE_INT;
        case XFDESKTOP_FILE_ICON_MODEL_COLUMN_TOOLTIP_SURFACE:
            return CAIRO_GOBJECT_TYPE_SURFACE;
        case XFDESKTOP_FILE_ICON_MODEL_COLUMN_TOOLTIP_TEXT:
            return G_TYPE_STRING;
        default:
            g_assert_not_reached();
    }
}

static gboolean
xfdesktop_file_icon_model_get_iter(GtkTreeModel *model,
                                   GtkTreeIter *iter,
                                   GtkTreePath *path)
{
    XfdesktopFileIconModel *fmodel = XFDESKTOP_FILE_ICON_MODEL(model);
    gint *indices = gtk_tree_path_get_indices(path);

    iter->stamp = 0;

    if (indices != NULL) {
        GList *item = g_list_nth(fmodel->items, indices[0]);
        if (item != NULL) {
            iter->stamp = ITER_STAMP;
            iter->user_data = item;
        }
    }

    return iter->stamp == ITER_STAMP;
}

static GtkTreePath *
xfdesktop_file_icon_model_get_path(GtkTreeModel *model,
                                   GtkTreeIter *iter)
{
    XfdesktopFileIconModel *fmodel = XFDESKTOP_FILE_ICON_MODEL(model);
    GList *item;
    gint index;

    g_return_val_if_fail(iter != NULL && iter->stamp == ITER_STAMP, NULL);

    item = (GList *)iter->user_data;
    index = g_list_index(fmodel->items, item->data);
    if (index >= 0) {
        return gtk_tree_path_new_from_indices(index, -1);
    } else {
        return NULL;
    }
}

static void
xfdesktop_file_icon_model_get_value(GtkTreeModel *model,
                                    GtkTreeIter *iter,
                                    gint column,
                                    GValue *value)
{
    XfdesktopFileIconModel *fmodel = XFDESKTOP_FILE_ICON_MODEL(model);
    GList *item;
    XfdesktopFileIcon *icon;

    g_return_if_fail(iter->stamp == ITER_STAMP);

    item = (GList *)iter->user_data;
    icon = XFDESKTOP_FILE_ICON(item->data);

    switch (column) {
        case XFDESKTOP_FILE_ICON_MODEL_COLUMN_SURFACE: {
            GdkPixbuf *pix = xfdesktop_icon_get_pixbuf(XFDESKTOP_ICON(icon),
                                                       fmodel->icon_width * fmodel->scale_factor,
                                                       fmodel->icon_height * fmodel->scale_factor);
            if (pix != NULL) {
                cairo_surface_t *surface = gdk_cairo_surface_create_from_pixbuf(pix, fmodel->scale_factor, NULL);
                g_value_init(value, CAIRO_GOBJECT_TYPE_SURFACE);
                g_value_take_boxed(value, surface);
                g_object_unref(pix);
            }
            break;
        }

        case XFDESKTOP_FILE_ICON_MODEL_COLUMN_LABEL: {
            const gchar *label = xfdesktop_icon_peek_label(XFDESKTOP_ICON(icon));
            if (label != NULL) {
                g_value_init(value, G_TYPE_STRING);
                g_value_set_string(value, label);
            }
            break;
        }

        case XFDESKTOP_FILE_ICON_MODEL_COLUMN_ROW: {
            gint16 row = -1, col = -1;

            g_value_init(value, G_TYPE_INT);
            xfdesktop_icon_get_position(XFDESKTOP_ICON(icon), &row, &col);
            g_value_set_int(value, row);
            break;
        }

        case XFDESKTOP_FILE_ICON_MODEL_COLUMN_COL: {
            gint16 row = -1, col = -1;

            g_value_init(value, G_TYPE_INT);
            xfdesktop_icon_get_position(XFDESKTOP_ICON(icon), &row, &col);
            g_value_set_int(value,col);
            break;
        }

        case XFDESKTOP_FILE_ICON_MODEL_COLUMN_SORT_PRIORITY: {
            gint priority;

            if (XFDESKTOP_IS_SPECIAL_FILE_ICON(icon)) {
                priority = 0;
            } else if (XFDESKTOP_IS_VOLUME_ICON(icon)) {
                priority = 1;
            } else if (XFDESKTOP_IS_REGULAR_FILE_ICON(icon)) {
                priority = 2;
            } else {
                priority = 3;
            }

            g_value_init(value, G_TYPE_INT);
            g_value_set_int(value, priority);
            break;
        }

        case XFDESKTOP_FILE_ICON_MODEL_COLUMN_TOOLTIP_SURFACE: {
            if (fmodel->tooltip_icon_size > 0) {
                GdkPixbuf *pix = xfdesktop_icon_get_pixbuf(XFDESKTOP_ICON(icon),
                                                           fmodel->tooltip_icon_size * fmodel->scale_factor,
                                                           fmodel->tooltip_icon_size * fmodel->scale_factor);
                if (pix != NULL) {
                    cairo_surface_t *surface = gdk_cairo_surface_create_from_pixbuf(pix, fmodel->scale_factor, NULL);
                    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
                        g_warning("failed to create cairo surface: %d", cairo_surface_status(surface));
                    } else {
                        g_value_init(value, CAIRO_GOBJECT_TYPE_SURFACE);
                        g_value_take_boxed(value, surface);
                        g_object_unref(pix);
                    }
                }
            }
            break;
        }

        case XFDESKTOP_FILE_ICON_MODEL_COLUMN_TOOLTIP_TEXT: {
            const gchar *tip_text = xfdesktop_icon_peek_tooltip(XFDESKTOP_ICON(icon));
            if (tip_text != NULL) {
                g_value_init(value, G_TYPE_STRING);
                g_value_set_string(value, tip_text);
            }
            break;
        }

        default:
            g_warning("Invalid XfdesktopWindowIconManager column %d", column);
            break;
    }
}

static gboolean
xfdesktop_file_icon_model_iter_previous(GtkTreeModel *model,
                                        GtkTreeIter *iter)
{
    GList *item;

    g_return_val_if_fail(iter->stamp == ITER_STAMP, FALSE);

    item = (GList *)iter->user_data;
    if (item->prev != NULL) {
        iter->user_data = item->prev;
        return TRUE;
    } else {
        return FALSE;
    }
}

static gboolean
xfdesktop_file_icon_model_iter_next(GtkTreeModel *model,
                                    GtkTreeIter *iter)
{
    GList *item;

    g_return_val_if_fail(iter->stamp == ITER_STAMP, FALSE);

    item = (GList *)iter->user_data;
    if (item->next != NULL) {
        iter->user_data = item->next;
        return TRUE;
    } else {
        return FALSE;
    }
}

static gboolean
xfdesktop_file_icon_model_iter_parent(GtkTreeModel *model,
                                      GtkTreeIter *iter,
                                      GtkTreeIter *child)
{
    iter->stamp = 0;
    return FALSE;
}

static gboolean
xfdesktop_file_icon_model_iter_has_child(GtkTreeModel *model,
                                         GtkTreeIter *iter)
{
    return FALSE;
}

static gint
xfdesktop_file_icon_model_iter_n_children(GtkTreeModel *model,
                                          GtkTreeIter *iter)
{
    XfdesktopFileIconModel *fmodel = XFDESKTOP_FILE_ICON_MODEL(model);

    g_return_val_if_fail(iter == NULL || iter->stamp == ITER_STAMP, -1);

    if (iter == NULL) {
        return g_list_length(fmodel->items);
    } else {
        return 0;
    }
}

static gboolean
xfdesktop_file_icon_model_iter_children(GtkTreeModel *model,
                                        GtkTreeIter *iter,
                                        GtkTreeIter *parent)
{
    XfdesktopFileIconModel *fmodel = XFDESKTOP_FILE_ICON_MODEL(model);

    if (parent != NULL) {
        iter->stamp = 0;
        return FALSE;
    } else if (fmodel->items != NULL) {
        iter->stamp = ITER_STAMP;
        iter->user_data = fmodel->items;
        return TRUE;
    } else {
        iter->stamp = 0;
        return FALSE;
    }
}

static gboolean
xfdesktop_file_icon_model_iter_nth_child(GtkTreeModel *model,
                                         GtkTreeIter *iter,
                                         GtkTreeIter *parent,
                                         gint n)
{
    XfdesktopFileIconModel *fmodel = XFDESKTOP_FILE_ICON_MODEL(model);

    if (parent != NULL) {
        iter->stamp = 0;
        return FALSE;
    } else {
        GList *item = g_list_nth(fmodel->items, n);
        if (item != NULL) {
            iter->stamp = ITER_STAMP;
            iter->user_data = item;
            return TRUE;
        } else {
            iter->stamp = 0;
            return FALSE;
        }
    }
}

static void
notify_all_rows_changed(XfdesktopFileIconModel *fmodel)
{
    gint i = 0;
    for (GList *l = fmodel->items; l != NULL; l = l->next, ++i) {
        GtkTreePath *path = gtk_tree_path_new_from_indices(i, -1);
        GtkTreeIter iter = {
            .stamp = ITER_STAMP,
            .user_data = l,
        };
        gtk_tree_model_row_changed(GTK_TREE_MODEL(fmodel), path, &iter);
        gtk_tree_path_free(path);
    }
}


XfdesktopFileIconModel *
xfdesktop_file_icon_model_new(void)
{
    return g_object_new(XFDESKTOP_TYPE_FILE_ICON_MODEL, NULL);
}

void
xfdesktop_file_icon_model_set_icon_width(XfdesktopFileIconModel *fmodel,
                                        gint width)
{
    g_return_if_fail(XFDESKTOP_IS_FILE_ICON_MODEL(fmodel));
    g_return_if_fail(width > 0);

    if (width != fmodel->icon_width) {
        fmodel->icon_width = width;
        g_object_notify(G_OBJECT(fmodel), "icon-width");
        notify_all_rows_changed(fmodel);
    }
}

void
xfdesktop_file_icon_model_set_icon_height(XfdesktopFileIconModel *fmodel,
                                          gint height)
{
    g_return_if_fail(XFDESKTOP_IS_FILE_ICON_MODEL(fmodel));
    g_return_if_fail(height > 0);

    if (height != fmodel->icon_height) {
        fmodel->icon_height = height;
        g_object_notify(G_OBJECT(fmodel), "icon-height");
        notify_all_rows_changed(fmodel);
    }
}

void
xfdesktop_file_icon_model_set_tooltip_icon_size(XfdesktopFileIconModel *fmodel,
                                                gint size)
{
    g_return_if_fail(XFDESKTOP_IS_FILE_ICON_MODEL(fmodel));
    g_return_if_fail(size >= 0);

    if (size != fmodel->tooltip_icon_size) {
        fmodel->tooltip_icon_size = size;
        g_object_notify(G_OBJECT(fmodel), "tooltip-icon-size");
        notify_all_rows_changed(fmodel);
    }
}

void
xfdesktop_file_icon_model_set_scale_factor(XfdesktopFileIconModel *fmodel,
                                           gint scale_factor)
{
    g_return_if_fail(XFDESKTOP_IS_FILE_ICON_MODEL(fmodel));
    g_return_if_fail(scale_factor > 0);

    if (scale_factor != fmodel->scale_factor) {
        fmodel->scale_factor = scale_factor;
        g_object_notify(G_OBJECT(fmodel), "scale-factor");
        notify_all_rows_changed(fmodel);
    }
}

void
xfdesktop_file_icon_model_append(XfdesktopFileIconModel *fmodel,
                                 XfdesktopFileIcon *icon,
                                 GtkTreeIter *iter)
{
    GList *new_link;
    guint new_length;
    GtkTreePath *path;
    GtkTreeIter new_iter = {
        .stamp = ITER_STAMP,
    };

    g_return_if_fail(XFDESKTOP_IS_FILE_ICON_MODEL(fmodel));
    g_return_if_fail(XFDESKTOP_IS_FILE_ICON(icon));

    fmodel->items = xfdesktop_g_list_append(fmodel->items, g_object_ref(icon), &new_link, &new_length);
    g_hash_table_insert(fmodel->icons, icon, new_link);

    g_assert(new_length > 0);
    path = gtk_tree_path_new_from_indices(new_length - 1, -1);
    new_iter.user_data = new_link;

    gtk_tree_model_row_inserted(GTK_TREE_MODEL(fmodel), path, &new_iter);
    gtk_tree_path_free(path);

    if (iter != NULL) {
        *iter = new_iter;
    }
}

void
xfdesktop_file_icon_model_remove(XfdesktopFileIconModel *fmodel,
                                 XfdesktopFileIcon *icon)
{
    GList *item;

    g_return_if_fail(XFDESKTOP_IS_FILE_ICON_MODEL(fmodel));
    g_return_if_fail(XFDESKTOP_IS_FILE_ICON(icon));

    item = g_hash_table_lookup(fmodel->icons, icon);
    if (G_LIKELY(item != NULL)) {
        gint index = g_list_index(fmodel->items, item->data);
        GtkTreePath *path = gtk_tree_path_new_from_indices(index, -1);

        g_assert(index >= 0);

        g_hash_table_remove(fmodel->icons, icon);
        g_object_unref(item->data);
        fmodel->items = g_list_delete_link(fmodel->items, item);

        gtk_tree_model_row_deleted(GTK_TREE_MODEL(fmodel), path);
        gtk_tree_path_free(path);
    }
}

void
xfdesktop_file_icon_model_changed(XfdesktopFileIconModel *fmodel,
                                  XfdesktopFileIcon *icon)
{
    GList *item;

    g_return_if_fail(XFDESKTOP_IS_FILE_ICON_MODEL(fmodel));
    g_return_if_fail(XFDESKTOP_IS_FILE_ICON(icon));

    item = g_hash_table_lookup(fmodel->icons, icon);
    if (G_LIKELY(item != NULL)) {
        gint index = g_list_index(fmodel->items, item->data);
        GtkTreePath *path = gtk_tree_path_new_from_indices(index, -1);
        GtkTreeIter iter = {
            .stamp = ITER_STAMP,
            .user_data = item,
        };

        g_assert(index >= 0);

        gtk_tree_model_row_changed(GTK_TREE_MODEL(fmodel), path, &iter);
        gtk_tree_path_free(path);
    }
}

XfdesktopFileIcon *
xfdesktop_file_icon_model_get_icon(XfdesktopFileIconModel *fmodel,
                                   GtkTreeIter *iter)
{
    GList *item;

    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON_MODEL(fmodel), NULL);
    g_return_val_if_fail(iter != NULL && iter->stamp == ITER_STAMP, NULL);

    item = (GList *)iter->user_data;
    return XFDESKTOP_FILE_ICON(item->data);
}

void
xfdesktop_file_icon_model_clear(XfdesktopFileIconModel *fmodel)
{
    guint n = 0;
    GList *last = NULL;

    g_return_if_fail(XFDESKTOP_IS_FILE_ICON_MODEL(fmodel));

    last = xfdesktop_g_list_last(fmodel->items, &n);
#ifdef G_ENABLE_DEBUG
    g_assert(n == g_list_length(fmodel->items));
#endif

    g_hash_table_remove_all(fmodel->icons);

    while (last != NULL) {
        GtkTreePath *path = gtk_tree_path_new_from_indices(n, -1);
        GList *item = last;
        last = last->prev;

        g_object_unref(item->data);
        fmodel->items = g_list_delete_link(fmodel->items, item);
        gtk_tree_model_row_deleted(GTK_TREE_MODEL(fmodel), path);
        gtk_tree_path_free(path);
    }

    g_assert(fmodel->items == NULL);
}
