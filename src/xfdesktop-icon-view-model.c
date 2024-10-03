/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright(c) 2022-2024 Brian Tarricone, <brian@tarricone.org>
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

#include "xfdesktop-common.h"
#include "xfdesktop-extensions.h"
#include "xfdesktop-icon-view-model.h"

#define ITER_STAMP 1870614

struct _XfdesktopIconViewModelPrivate
{
    GList *items;
    GHashTable *model_items;
};

static void xfdesktop_icon_view_model_tree_model_init(GtkTreeModelIface *iface);

static void xfdesktop_icon_view_model_finalize(GObject *obj);

static GtkTreeModelFlags xfdesktop_icon_view_model_get_flags(GtkTreeModel *model);
static gint xfdesktop_icon_view_model_get_n_columns(GtkTreeModel *model);
static GType xfdesktop_icon_view_model_get_column_type(GtkTreeModel *model,
                                                       gint column);
static gboolean xfdesktop_icon_view_model_get_iter(GtkTreeModel *model,
                                                   GtkTreeIter *iter,
                                                   GtkTreePath *path);
static GtkTreePath *xfdesktop_icon_view_model_get_path(GtkTreeModel *model,
                                                       GtkTreeIter *iter);
static gboolean xfdesktop_icon_view_model_iter_previous(GtkTreeModel *model,
                                                        GtkTreeIter *iter);
static gboolean xfdesktop_icon_view_model_iter_next(GtkTreeModel *model,
                                                    GtkTreeIter *iter);
static gboolean xfdesktop_icon_view_model_iter_parent(GtkTreeModel *model,
                                                      GtkTreeIter *iter,
                                                      GtkTreeIter *child);
static gboolean xfdesktop_icon_view_model_iter_has_child(GtkTreeModel *model,
                                                         GtkTreeIter *iter);
static gint xfdesktop_icon_view_model_iter_n_children(GtkTreeModel *model,
                                                      GtkTreeIter *iter);
static gboolean xfdesktop_icon_view_model_iter_children(GtkTreeModel *model,
                                                        GtkTreeIter *iter,
                                                        GtkTreeIter *parent);
static gboolean xfdesktop_icon_view_model_iter_nth_child(GtkTreeModel *model,
                                                         GtkTreeIter *iter,
                                                         GtkTreeIter *parent,
                                                         gint n);


G_DEFINE_ABSTRACT_TYPE_WITH_CODE(XfdesktopIconViewModel,
                                 xfdesktop_icon_view_model,
                                 G_TYPE_OBJECT,
                                 G_ADD_PRIVATE(XfdesktopIconViewModel)
                                 G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_MODEL, xfdesktop_icon_view_model_tree_model_init))


static void
xfdesktop_icon_view_model_class_init(XfdesktopIconViewModelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = xfdesktop_icon_view_model_finalize;
}

static void
xfdesktop_icon_view_model_tree_model_init(GtkTreeModelIface *iface)
{
    iface->get_flags = xfdesktop_icon_view_model_get_flags;
    iface->get_n_columns = xfdesktop_icon_view_model_get_n_columns;
    iface->get_column_type = xfdesktop_icon_view_model_get_column_type;
    iface->get_iter = xfdesktop_icon_view_model_get_iter;
    iface->get_path = xfdesktop_icon_view_model_get_path;
    iface->iter_previous = xfdesktop_icon_view_model_iter_previous;
    iface->iter_next = xfdesktop_icon_view_model_iter_next;
    iface->iter_parent = xfdesktop_icon_view_model_iter_parent;
    iface->iter_has_child = xfdesktop_icon_view_model_iter_has_child;
    iface->iter_n_children = xfdesktop_icon_view_model_iter_n_children;
    iface->iter_children = xfdesktop_icon_view_model_iter_children;
    iface->iter_nth_child = xfdesktop_icon_view_model_iter_nth_child;
}

static void
xfdesktop_icon_view_model_init(XfdesktopIconViewModel *ivmodel)
{
    XfdesktopIconViewModelClass *klass = XFDESKTOP_ICON_VIEW_MODEL_GET_CLASS(ivmodel);

    ivmodel->priv = xfdesktop_icon_view_model_get_instance_private(ivmodel);

    ivmodel->priv->model_items = g_hash_table_new(klass->model_item_hash, klass->model_item_equal);
}

static void
xfdesktop_icon_view_model_finalize(GObject *obj)
{
    XfdesktopIconViewModel *ivmodel = XFDESKTOP_ICON_VIEW_MODEL(obj);
    XfdesktopIconViewModelClass *klass = XFDESKTOP_ICON_VIEW_MODEL_GET_CLASS(ivmodel);

    g_hash_table_destroy(ivmodel->priv->model_items);

    for (GList *l = ivmodel->priv->items; l != NULL;) {
        GList *link = l;
        l = l->next;
        klass->model_item_free(ivmodel, link->data);
        g_list_free_1(link);
    }

    G_OBJECT_CLASS(xfdesktop_icon_view_model_parent_class)->finalize(obj);
}

static GtkTreeModelFlags
xfdesktop_icon_view_model_get_flags(GtkTreeModel *model)
{
    return GTK_TREE_MODEL_ITERS_PERSIST | GTK_TREE_MODEL_LIST_ONLY;
}

static gint
xfdesktop_icon_view_model_get_n_columns(GtkTreeModel *model)
{
    return XFDESKTOP_ICON_VIEW_MODEL_COLUMN_N_COLUMNS;
}

static GType
xfdesktop_icon_view_model_get_column_type(GtkTreeModel *model,
                                          gint column)
{
    g_return_val_if_fail(column >= 0 && column < XFDESKTOP_ICON_VIEW_MODEL_COLUMN_N_COLUMNS, G_TYPE_NONE);

    switch (column) {
        case XFDESKTOP_ICON_VIEW_MODEL_COLUMN_IMAGE:
            return G_TYPE_ICON;
        case XFDESKTOP_ICON_VIEW_MODEL_COLUMN_IMAGE_OPACITY:
            return G_TYPE_DOUBLE;
        case XFDESKTOP_ICON_VIEW_MODEL_COLUMN_LABEL:
            return G_TYPE_STRING;
        case XFDESKTOP_ICON_VIEW_MODEL_COLUMN_ROW:
            return G_TYPE_INT;
        case XFDESKTOP_ICON_VIEW_MODEL_COLUMN_COL:
            return G_TYPE_INT;
        case XFDESKTOP_ICON_VIEW_MODEL_COLUMN_SORT_PRIORITY:
            return G_TYPE_INT;
        case XFDESKTOP_ICON_VIEW_MODEL_COLUMN_TOOLTIP_IMAGE:
            return G_TYPE_ICON;
        case XFDESKTOP_ICON_VIEW_MODEL_COLUMN_TOOLTIP_TEXT:
            return G_TYPE_STRING;
        default:
            g_assert_not_reached();
    }
}

static gboolean
xfdesktop_icon_view_model_get_iter(GtkTreeModel *model,
                                   GtkTreeIter *iter,
                                   GtkTreePath *path)
{
    XfdesktopIconViewModel *ivmodel = XFDESKTOP_ICON_VIEW_MODEL(model);
    gint *indices = gtk_tree_path_get_indices(path);

    iter->stamp = 0;

    if (indices != NULL) {
        GList *item = g_list_nth(ivmodel->priv->items, indices[0]);
        if (item != NULL) {
            iter->stamp = ITER_STAMP;
            iter->user_data = item;
        }
    }

    return iter->stamp == ITER_STAMP;
}

static GtkTreePath *
xfdesktop_icon_view_model_get_path(GtkTreeModel *model,
                                   GtkTreeIter *iter)
{
    XfdesktopIconViewModel *ivmodel = XFDESKTOP_ICON_VIEW_MODEL(model);
    GList *item;
    gint index;

    g_return_val_if_fail(iter != NULL && iter->stamp == ITER_STAMP, NULL);

    item = (GList *)iter->user_data;
    index = g_list_index(ivmodel->priv->items, item->data);
    if (index >= 0) {
        return gtk_tree_path_new_from_indices(index, -1);
    } else {
        return NULL;
    }
}

static gboolean
xfdesktop_icon_view_model_iter_previous(GtkTreeModel *model,
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
xfdesktop_icon_view_model_iter_next(GtkTreeModel *model,
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
xfdesktop_icon_view_model_iter_parent(GtkTreeModel *model,
                                      GtkTreeIter *iter,
                                      GtkTreeIter *child)
{
    iter->stamp = 0;
    return FALSE;
}

static gboolean
xfdesktop_icon_view_model_iter_has_child(GtkTreeModel *model,
                                         GtkTreeIter *iter)
{
    return FALSE;
}

static gint
xfdesktop_icon_view_model_iter_n_children(GtkTreeModel *model,
                                          GtkTreeIter *iter)
{
    XfdesktopIconViewModel *ivmodel = XFDESKTOP_ICON_VIEW_MODEL(model);

    g_return_val_if_fail(iter == NULL || iter->stamp == ITER_STAMP, -1);

    if (iter == NULL) {
        return g_list_length(ivmodel->priv->items);
    } else {
        return 0;
    }
}

static gboolean
xfdesktop_icon_view_model_iter_children(GtkTreeModel *model,
                                        GtkTreeIter *iter,
                                        GtkTreeIter *parent)
{
    XfdesktopIconViewModel *ivmodel = XFDESKTOP_ICON_VIEW_MODEL(model);

    if (parent != NULL) {
        iter->stamp = 0;
        return FALSE;
    } else if (ivmodel->priv->items != NULL) {
        iter->stamp = ITER_STAMP;
        iter->user_data = ivmodel->priv->items;
        return TRUE;
    } else {
        iter->stamp = 0;
        return FALSE;
    }
}

static gboolean
xfdesktop_icon_view_model_iter_nth_child(GtkTreeModel *model,
                                         GtkTreeIter *iter,
                                         GtkTreeIter *parent,
                                         gint n)
{
    XfdesktopIconViewModel *ivmodel = XFDESKTOP_ICON_VIEW_MODEL(model);

    if (parent != NULL) {
        iter->stamp = 0;
        return FALSE;
    } else {
        GList *item = g_list_nth(ivmodel->priv->items, n);
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


void
xfdesktop_icon_view_model_append(XfdesktopIconViewModel *ivmodel,
                                 gpointer key,
                                 gpointer model_item,
                                 GtkTreeIter *iter)
{
    XfdesktopIconViewModelClass *klass;
    GList *new_link;
    guint new_length;
    GtkTreePath *path;
    GtkTreeIter new_iter = {
        .stamp = ITER_STAMP,
    };

    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW_MODEL(ivmodel));
    g_return_if_fail(model_item != NULL);

    klass = XFDESKTOP_ICON_VIEW_MODEL_GET_CLASS(ivmodel);
    if (klass->model_item_ref != NULL) {
        klass->model_item_ref(model_item);
    }

    ivmodel->priv->items = xfdesktop_g_list_append(ivmodel->priv->items, model_item, &new_link, &new_length);
    g_hash_table_insert(ivmodel->priv->model_items, key, new_link);

    g_assert(new_length > 0);
    path = gtk_tree_path_new_from_indices(new_length - 1, -1);
    new_iter.user_data = new_link;

    gtk_tree_model_row_inserted(GTK_TREE_MODEL(ivmodel), path, &new_iter);
    gtk_tree_path_free(path);

    if (iter != NULL) {
        *iter = new_iter;
    }
}

void
xfdesktop_icon_view_model_remove(XfdesktopIconViewModel *ivmodel,
                                 gpointer key)
{
    GList *item;

    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW_MODEL(ivmodel));
    g_return_if_fail(key != NULL);

    item = g_hash_table_lookup(ivmodel->priv->model_items, key);
    if (G_LIKELY(item != NULL)) {
        XfdesktopIconViewModelClass *klass = XFDESKTOP_ICON_VIEW_MODEL_GET_CLASS(ivmodel);
        gint index = g_list_index(ivmodel->priv->items, item->data);
        GtkTreePath *path = gtk_tree_path_new_from_indices(index, -1);

        g_assert(index >= 0);

        g_hash_table_remove(ivmodel->priv->model_items, key);
        klass->model_item_free(ivmodel, item->data);
        ivmodel->priv->items = g_list_delete_link(ivmodel->priv->items, item);

        gtk_tree_model_row_deleted(GTK_TREE_MODEL(ivmodel), path);
        gtk_tree_path_free(path);
    }
}

void
xfdesktop_icon_view_model_changed(XfdesktopIconViewModel *ivmodel,
                                  gpointer key)
{
    GList *item;

    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW_MODEL(ivmodel));
    g_return_if_fail(key != NULL);

    item = g_hash_table_lookup(ivmodel->priv->model_items, key);
    if (G_LIKELY(item != NULL)) {
        gint index = g_list_index(ivmodel->priv->items, item->data);
        GtkTreePath *path = gtk_tree_path_new_from_indices(index, -1);
        GtkTreeIter iter = {
            .stamp = ITER_STAMP,
            .user_data = item,
        };

        g_assert(index >= 0);

        gtk_tree_model_row_changed(GTK_TREE_MODEL(ivmodel), path, &iter);
        gtk_tree_path_free(path);
    }
}

gpointer
xfdesktop_icon_view_model_get_model_item(XfdesktopIconViewModel *ivmodel,
                                         GtkTreeIter *iter)
{
    GList *item;

    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW_MODEL(ivmodel), NULL);
    g_return_val_if_fail(iter != NULL && iter->stamp == ITER_STAMP, NULL);

    item = (GList *)iter->user_data;
    return item->data;
}

gboolean
xfdesktop_icon_view_model_get_iter_for_key(XfdesktopIconViewModel *ivmodel,
                                           gpointer key,
                                           GtkTreeIter *iter)
{
    GList *item;

    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW_MODEL(ivmodel), FALSE);
    g_return_val_if_fail(key != NULL, FALSE);

    item = g_hash_table_lookup(ivmodel->priv->model_items, key);
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
xfdesktop_icon_view_model_clear(XfdesktopIconViewModel *ivmodel)
{
    XfdesktopIconViewModelClass *klass;
    guint n = 0;
    GList *last = NULL;

    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW_MODEL(ivmodel));

    klass = XFDESKTOP_ICON_VIEW_MODEL_GET_CLASS(ivmodel);

    last = xfdesktop_g_list_last(ivmodel->priv->items, &n);
#ifdef G_ENABLE_DEBUG
    g_assert(n == g_list_length(ivmodel->priv->items));
#endif

    g_hash_table_remove_all(ivmodel->priv->model_items);

    while (last != NULL) {
        GtkTreePath *path = gtk_tree_path_new_from_indices(n, -1);
        GList *item = last;
        last = last->prev;

        klass->model_item_free(ivmodel, item->data);
        ivmodel->priv->items = g_list_delete_link(ivmodel->priv->items, item);
        gtk_tree_model_row_deleted(GTK_TREE_MODEL(ivmodel), path);
        gtk_tree_path_free(path);
    }

    g_assert(ivmodel->priv->items == NULL);
}
