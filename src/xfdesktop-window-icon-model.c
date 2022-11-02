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
#include "xfdesktop-window-icon-model.h"

#define ITER_STAMP 1870614

typedef struct _ModelItem
{
    XfwWindow *window;
    gint row;
    gint col;
} ModelItem;

static ModelItem *
model_item_new(XfwWindow *window)
{
    ModelItem *item;

    g_return_val_if_fail(XFW_IS_WINDOW(window), NULL);

    item = g_slice_new0(ModelItem);
    item->window = g_object_ref(window);
    item->row = -1;
    item->col = -1;

    return item;
}


static void
model_item_free(ModelItem *item)
{
    g_return_if_fail(item);

    g_object_unref(item->window);
    g_slice_free(ModelItem, item);
}

struct _XfdesktopWindowIconModel
{
    GObject parent;

    GList *items;
    GHashTable *windows;

    gint icon_size;
    gint scale_factor;
};

struct _XfdesktopWindowIconModelClass
{
    GObjectClass parent_class;
};

enum
{
    PROP0 = 0,
    PROP_ICON_SIZE,
    PROP_SCALE_FACTOR,
};


static void xfdesktop_window_icon_model_tree_model_init(GtkTreeModelIface *iface);

static void xfdesktop_window_icon_model_set_property(GObject *obj,
                                                     guint prop_id,
                                                     const GValue *value,
                                                     GParamSpec *pspec);
static void xfdesktop_window_icon_model_get_property(GObject *obj,
                                                     guint prop_id,
                                                     GValue *value,
                                                     GParamSpec *pspec);
static void xfdesktop_window_icon_model_finalize(GObject *obj);

static GtkTreeModelFlags xfdesktop_window_icon_model_get_flags(GtkTreeModel *model);
static gint xfdesktop_window_icon_model_get_n_columns(GtkTreeModel *model);
static GType xfdesktop_window_icon_model_get_column_type(GtkTreeModel *model,
                                                         gint column);
static gboolean xfdesktop_window_icon_model_get_iter(GtkTreeModel *model,
                                                     GtkTreeIter *iter,
                                                     GtkTreePath *path);
static GtkTreePath *xfdesktop_window_icon_model_get_path(GtkTreeModel *model,
                                                         GtkTreeIter *iter);
static void xfdesktop_window_icon_model_get_value(GtkTreeModel *model,
                                                  GtkTreeIter *iter,
                                                  gint column,
                                                  GValue *value);
static gboolean xfdesktop_window_icon_model_iter_previous(GtkTreeModel *model,
                                                          GtkTreeIter *iter);
static gboolean xfdesktop_window_icon_model_iter_next(GtkTreeModel *model,
                                                      GtkTreeIter *iter);
static gboolean xfdesktop_window_icon_model_iter_parent(GtkTreeModel *model,
                                                        GtkTreeIter *iter,
                                                        GtkTreeIter *child);
static gboolean xfdesktop_window_icon_model_iter_has_child(GtkTreeModel *model,
                                                           GtkTreeIter *iter);
static gint xfdesktop_window_icon_model_iter_n_children(GtkTreeModel *model,
                                                        GtkTreeIter *iter);
static gboolean xfdesktop_window_icon_model_iter_children(GtkTreeModel *model,
                                                          GtkTreeIter *iter,
                                                          GtkTreeIter *parent);
static gboolean xfdesktop_window_icon_model_iter_nth_child(GtkTreeModel *model,
                                                           GtkTreeIter *iter,
                                                           GtkTreeIter *parent,
                                                           gint n);


G_DEFINE_TYPE_WITH_CODE(XfdesktopWindowIconModel,
                        xfdesktop_window_icon_model,
                        G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_MODEL, xfdesktop_window_icon_model_tree_model_init))


static void
xfdesktop_window_icon_model_class_init(XfdesktopWindowIconModelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->set_property = xfdesktop_window_icon_model_set_property;
    gobject_class->get_property = xfdesktop_window_icon_model_get_property;
    gobject_class->finalize = xfdesktop_window_icon_model_finalize;

#define PARAM_FLAGS (G_PARAM_READWRITE \
                     | G_PARAM_STATIC_NAME \
                     | G_PARAM_STATIC_NICK \
                     | G_PARAM_STATIC_BLURB)

    g_object_class_install_property(gobject_class,
                                    PROP_ICON_SIZE,
                                    g_param_spec_int("icon-size",
                                                     "icon-size",
                                                     "size of icon images",
                                                     MIN_ICON_SIZE, MAX_ICON_SIZE, DEFAULT_ICON_SIZE,
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
xfdesktop_window_icon_model_tree_model_init(GtkTreeModelIface *iface)
{
    iface->get_flags = xfdesktop_window_icon_model_get_flags;
    iface->get_n_columns = xfdesktop_window_icon_model_get_n_columns;
    iface->get_column_type = xfdesktop_window_icon_model_get_column_type;
    iface->get_iter = xfdesktop_window_icon_model_get_iter;
    iface->get_path = xfdesktop_window_icon_model_get_path;
    iface->get_value = xfdesktop_window_icon_model_get_value;
    iface->iter_previous = xfdesktop_window_icon_model_iter_previous;
    iface->iter_next = xfdesktop_window_icon_model_iter_next;
    iface->iter_parent = xfdesktop_window_icon_model_iter_parent;
    iface->iter_has_child = xfdesktop_window_icon_model_iter_has_child;
    iface->iter_n_children = xfdesktop_window_icon_model_iter_n_children;
    iface->iter_children = xfdesktop_window_icon_model_iter_children;
    iface->iter_nth_child = xfdesktop_window_icon_model_iter_nth_child;
}

static void
xfdesktop_window_icon_model_init(XfdesktopWindowIconModel *wmodel)
{
    wmodel->windows = g_hash_table_new(g_direct_hash, g_direct_equal);
    wmodel->icon_size = DEFAULT_ICON_SIZE;
    wmodel->scale_factor = 1;
}

static void
xfdesktop_window_icon_model_set_property(GObject *obj,
                                         guint prop_id,
                                         const GValue *value,
                                         GParamSpec *pspec)
{
    XfdesktopWindowIconModel *wmodel = XFDESKTOP_WINDOW_ICON_MODEL(obj);

    switch (prop_id) {
        case PROP_ICON_SIZE:
            xfdesktop_window_icon_model_set_icon_size(wmodel, g_value_get_int(value));
            break;

        case PROP_SCALE_FACTOR:
            xfdesktop_window_icon_model_set_scale_factor(wmodel, g_value_get_int(value));
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
            break;
    }
}

static void
xfdesktop_window_icon_model_get_property(GObject *obj,
                                         guint prop_id,
                                         GValue *value,
                                         GParamSpec *pspec)
{
    XfdesktopWindowIconModel *wmodel = XFDESKTOP_WINDOW_ICON_MODEL(obj);

    switch (prop_id) {
        case PROP_ICON_SIZE:
            g_value_set_int(value, wmodel->icon_size);
            break;

        case PROP_SCALE_FACTOR:
            g_value_set_int(value, wmodel->scale_factor);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
            break;
    }
}

static void
xfdesktop_window_icon_model_finalize(GObject *obj)
{
    XfdesktopWindowIconModel *wmodel = XFDESKTOP_WINDOW_ICON_MODEL(obj);

    g_hash_table_destroy(wmodel->windows);
    g_list_free_full(wmodel->items, (GDestroyNotify)model_item_free);

    G_OBJECT_CLASS(xfdesktop_window_icon_model_parent_class)->finalize(obj);
}

static GtkTreeModelFlags
xfdesktop_window_icon_model_get_flags(GtkTreeModel *model)
{
    return GTK_TREE_MODEL_ITERS_PERSIST | GTK_TREE_MODEL_LIST_ONLY;
}

static gint
xfdesktop_window_icon_model_get_n_columns(GtkTreeModel *model)
{
    return XFDESKTOP_WINDOW_ICON_MODEL_COLUMN_N_COLUMNS;
}

static GType
xfdesktop_window_icon_model_get_column_type(GtkTreeModel *model,
                                            gint column)
{
    g_return_val_if_fail(column >= 0 && column < XFDESKTOP_WINDOW_ICON_MODEL_COLUMN_N_COLUMNS, G_TYPE_NONE);

    switch (column) {
        case XFDESKTOP_WINDOW_ICON_MODEL_COLUMN_SURFACE:
            return CAIRO_GOBJECT_TYPE_SURFACE;
        case XFDESKTOP_WINDOW_ICON_MODEL_COLUMN_LABEL:
            return G_TYPE_STRING;
        case XFDESKTOP_WINDOW_ICON_MODEL_COLUMN_ROW:
            return G_TYPE_INT;
        case XFDESKTOP_WINDOW_ICON_MODEL_COLUMN_COL:
            return G_TYPE_INT;
        default:
            g_assert_not_reached();
    }
}

static gboolean
xfdesktop_window_icon_model_get_iter(GtkTreeModel *model,
                                     GtkTreeIter *iter,
                                     GtkTreePath *path)
{
    XfdesktopWindowIconModel *wmodel = XFDESKTOP_WINDOW_ICON_MODEL(model);
    gint *indices = gtk_tree_path_get_indices(path);

    iter->stamp = 0;

    if (indices != NULL) {
        GList *item = g_list_nth(wmodel->items, indices[0]);
        if (item != NULL) {
            iter->stamp = ITER_STAMP;
            iter->user_data = item;
        }
    }

    return iter->stamp == ITER_STAMP;
}

static GtkTreePath *
xfdesktop_window_icon_model_get_path(GtkTreeModel *model,
                                     GtkTreeIter *iter)
{
    XfdesktopWindowIconModel *wmodel = XFDESKTOP_WINDOW_ICON_MODEL(model);
    GList *item;
    gint index;

    g_return_val_if_fail(iter != NULL && iter->stamp == ITER_STAMP, NULL);

    item = (GList *)iter->user_data;
    index = g_list_index(wmodel->items, item->data);
    if (index >= 0) {
        return gtk_tree_path_new_from_indices(index, -1);
    } else {
        return NULL;
    }
}

static void
xfdesktop_window_icon_model_get_value(GtkTreeModel *model,
                                      GtkTreeIter *iter,
                                      gint column,
                                      GValue *value)
{
    XfdesktopWindowIconModel *wmodel = XFDESKTOP_WINDOW_ICON_MODEL(model);
    GList *item;
    ModelItem *model_item;

    g_return_if_fail(iter->stamp == ITER_STAMP);

    item = (GList *)iter->user_data;
    model_item = (ModelItem *)item->data;

    switch (column) {
        case XFDESKTOP_WINDOW_ICON_MODEL_COLUMN_SURFACE: {
            GdkPixbuf *pix = xfw_window_get_icon(model_item->window, wmodel->icon_size * wmodel->scale_factor);
            if (pix != NULL) {
                cairo_surface_t *surface = gdk_cairo_surface_create_from_pixbuf(pix, wmodel->scale_factor, NULL);
                g_value_init(value, CAIRO_GOBJECT_TYPE_SURFACE);
                g_value_take_boxed(value, surface);
            }
            break;
        }

        case XFDESKTOP_WINDOW_ICON_MODEL_COLUMN_LABEL: {
            const gchar *label = xfw_window_get_name(model_item->window);
            if (label != NULL) {
                g_value_init(value, G_TYPE_STRING);
                g_value_set_string(value, label);
            }
            break;
        }

        case XFDESKTOP_WINDOW_ICON_MODEL_COLUMN_ROW: {
            g_value_init(value, G_TYPE_INT);
            g_value_set_int(value, model_item->row);
            break;
        }

        case XFDESKTOP_WINDOW_ICON_MODEL_COLUMN_COL: {
            g_value_init(value, G_TYPE_INT);
            g_value_set_int(value, model_item->col);
            break;
        }

        default:
            g_warning("Invalid XfdesktopWindowIconManager column %d", column);
            break;
    }
}

static gboolean
xfdesktop_window_icon_model_iter_previous(GtkTreeModel *model,
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
xfdesktop_window_icon_model_iter_next(GtkTreeModel *model,
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
xfdesktop_window_icon_model_iter_parent(GtkTreeModel *model,
                                        GtkTreeIter *iter,
                                        GtkTreeIter *child)
{
    iter->stamp = 0;
    return FALSE;
}

static gboolean
xfdesktop_window_icon_model_iter_has_child(GtkTreeModel *model,
                                           GtkTreeIter *iter)
{
    return FALSE;
}

static gint
xfdesktop_window_icon_model_iter_n_children(GtkTreeModel *model,
                                            GtkTreeIter *iter)
{
    XfdesktopWindowIconModel *wmodel = XFDESKTOP_WINDOW_ICON_MODEL(model);

    g_return_val_if_fail(iter == NULL || iter->stamp == ITER_STAMP, -1);

    if (iter == NULL) {
        return g_list_length(wmodel->items);
    } else {
        return 0;
    }
}

static gboolean
xfdesktop_window_icon_model_iter_children(GtkTreeModel *model,
                                          GtkTreeIter *iter,
                                          GtkTreeIter *parent)
{
    XfdesktopWindowIconModel *wmodel = XFDESKTOP_WINDOW_ICON_MODEL(model);

    if (parent != NULL) {
        iter->stamp = 0;
        return FALSE;
    } else if (wmodel->items != NULL) {
        iter->stamp = ITER_STAMP;
        iter->user_data = wmodel->items;
        return TRUE;
    } else {
        iter->stamp = 0;
        return FALSE;
    }
}

static gboolean
xfdesktop_window_icon_model_iter_nth_child(GtkTreeModel *model,
                                           GtkTreeIter *iter,
                                           GtkTreeIter *parent,
                                           gint n)
{
    XfdesktopWindowIconModel *wmodel = XFDESKTOP_WINDOW_ICON_MODEL(model);

    if (parent != NULL) {
        iter->stamp = 0;
        return FALSE;
    } else {
        GList *item = g_list_nth(wmodel->items, n);
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
notify_all_rows_changed(XfdesktopWindowIconModel *wmodel)
{
    gint i = 0;
    for (GList *l = wmodel->items; l != NULL; l = l->next, ++i) {
        GtkTreePath *path = gtk_tree_path_new_from_indices(i, -1);
        GtkTreeIter iter = {
            .stamp = ITER_STAMP,
            .user_data = l,
        };
        gtk_tree_model_row_changed(GTK_TREE_MODEL(wmodel), path, &iter);
        gtk_tree_path_free(path);
    }
}


XfdesktopWindowIconModel *
xfdesktop_window_icon_model_new(void)
{
    return g_object_new(XFDESKTOP_TYPE_WINDOW_ICON_MODEL, NULL);
}

void
xfdesktop_window_icon_model_set_icon_size(XfdesktopWindowIconModel *wmodel,
                                          gint icon_size)
{
    g_return_if_fail(XFDESKTOP_IS_WINDOW_ICON_MODEL(wmodel));
    g_return_if_fail(icon_size > 0);

    if (icon_size != wmodel->icon_size) {
        wmodel->icon_size = icon_size;
        g_object_notify(G_OBJECT(wmodel), "icon-size");
        notify_all_rows_changed(wmodel);
    }
}

void
xfdesktop_window_icon_model_set_scale_factor(XfdesktopWindowIconModel *wmodel,
                                             gint scale_factor)
{
    g_return_if_fail(XFDESKTOP_IS_WINDOW_ICON_MODEL(wmodel));
    g_return_if_fail(scale_factor > 0);

    if (scale_factor != wmodel->scale_factor) {
        wmodel->scale_factor = scale_factor;
        g_object_notify(G_OBJECT(wmodel), "scale-factor");
        notify_all_rows_changed(wmodel);
    }
}

void
xfdesktop_window_icon_model_append(XfdesktopWindowIconModel *wmodel,
                                   XfwWindow *window,
                                   GtkTreeIter *iter)
{
    ModelItem *model_item;
    GList *new_link;
    guint new_length;
    GtkTreePath *path;
    GtkTreeIter new_iter = {
        .stamp = ITER_STAMP,
    };

    g_return_if_fail(XFDESKTOP_IS_WINDOW_ICON_MODEL(wmodel));
    g_return_if_fail(XFW_IS_WINDOW(window));

    model_item = model_item_new(window);

    wmodel->items = xfdesktop_g_list_append(wmodel->items, model_item, &new_link, &new_length);
    g_hash_table_insert(wmodel->windows, window, new_link);

    g_assert(new_length > 0);
    path = gtk_tree_path_new_from_indices(new_length - 1, -1);
    new_iter.user_data = new_link;

    gtk_tree_model_row_inserted(GTK_TREE_MODEL(wmodel), path, &new_iter);
    gtk_tree_path_free(path);

    if (iter != NULL) {
        *iter = new_iter;
    }
}

void
xfdesktop_window_icon_model_remove(XfdesktopWindowIconModel *wmodel,
                                   XfwWindow *window)
{
    GList *item;

    g_return_if_fail(XFDESKTOP_IS_WINDOW_ICON_MODEL(wmodel));
    g_return_if_fail(XFW_IS_WINDOW(window));

    item = g_hash_table_lookup(wmodel->windows, window);
    if (G_LIKELY(item != NULL)) {
        gint index = g_list_index(wmodel->items, item->data);
        GtkTreePath *path = gtk_tree_path_new_from_indices(index, -1);

        g_assert(index >= 0);

        g_hash_table_remove(wmodel->windows, window);
        model_item_free(item->data);
        wmodel->items = g_list_delete_link(wmodel->items, item);

        gtk_tree_model_row_deleted(GTK_TREE_MODEL(wmodel), path);
        gtk_tree_path_free(path);
    }
}

void
xfdesktop_window_icon_model_changed(XfdesktopWindowIconModel *wmodel,
                                    XfwWindow *window)
{
    GList *item;

    g_return_if_fail(XFDESKTOP_IS_WINDOW_ICON_MODEL(wmodel));
    g_return_if_fail(XFW_IS_WINDOW(window));

    item = g_hash_table_lookup(wmodel->windows, window);
    if (G_LIKELY(item != NULL)) {
        gint index = g_list_index(wmodel->items, item->data);
        GtkTreePath *path = gtk_tree_path_new_from_indices(index, -1);
        GtkTreeIter iter = {
            .stamp = ITER_STAMP,
            .user_data = item,
        };

        g_assert(index >= 0);

        gtk_tree_model_row_changed(GTK_TREE_MODEL(wmodel), path, &iter);
        gtk_tree_path_free(path);
    }
}

void
xfdesktop_window_icon_model_set_position(XfdesktopWindowIconModel *wmodel,
                                         GtkTreeIter *iter,
                                         gint row,
                                         gint col)
{
    GList *item;

    g_return_if_fail(XFDESKTOP_IS_WINDOW_ICON_MODEL(wmodel));
    g_return_if_fail(iter != NULL && iter->stamp == ITER_STAMP);
    g_return_if_fail(row >= -1 && col >= -1);

    item = (GList *)iter->user_data;
    if (item != NULL) {
        ModelItem *model_item = (ModelItem *)item->data;
        model_item->row = row;
        model_item->col = col;
    }
}

XfwWindow *
xfdesktop_window_icon_model_get_window(XfdesktopWindowIconModel *wmodel,
                                       GtkTreeIter *iter)
{
    GList *item;
    ModelItem *model_item;

    g_return_val_if_fail(XFDESKTOP_IS_WINDOW_ICON_MODEL(wmodel), NULL);
    g_return_val_if_fail(iter != NULL && iter->stamp == ITER_STAMP, NULL);

    item = (GList *)iter->user_data;
    model_item = (ModelItem *)item->data;
    return model_item->window;
}

gboolean
xfdesktop_window_icon_model_get_window_iter(XfdesktopWindowIconModel *wmodel,
                                            XfwWindow *window,
                                            GtkTreeIter *iter)
{
    GList *item;

    g_return_val_if_fail(XFDESKTOP_IS_WINDOW_ICON_MODEL(wmodel), FALSE);
    g_return_val_if_fail(XFW_IS_WINDOW(window), FALSE);

    item = g_hash_table_lookup(wmodel->windows, window);
    if (item != NULL) {
        if (iter != NULL) {
            iter->stamp = ITER_STAMP;
            iter->user_data = item;
        }
        return TRUE;
    } else {
        if (iter != NULL) {
            iter->stamp = 0;
        }
        return FALSE;
    }
}

void
xfdesktop_window_icon_model_clear(XfdesktopWindowIconModel *wmodel)
{
    guint n = 0;
    GList *last = NULL;

    g_return_if_fail(XFDESKTOP_IS_WINDOW_ICON_MODEL(wmodel));

    last = xfdesktop_g_list_last(wmodel->items, &n);
#ifdef G_ENABLE_DEBUG
    g_assert(n == g_list_length(wmodel->items));
#endif

    g_hash_table_remove_all(wmodel->windows);

    while (last != NULL) {
        GtkTreePath *path = gtk_tree_path_new_from_indices(n, -1);
        GList *item = last;
        last = last->prev;

        model_item_free(item->data);
        wmodel->items = g_list_delete_link(wmodel->items, item);
        gtk_tree_model_row_deleted(GTK_TREE_MODEL(wmodel), path);
        gtk_tree_path_free(path);
    }

    g_assert(wmodel->items == NULL);
}
