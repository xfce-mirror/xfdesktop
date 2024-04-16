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
#include "xfdesktop-icon-view-model.h"
#include "xfdesktop-window-icon-model.h"

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
    XfdesktopIconViewModel parent;
};

struct _XfdesktopWindowIconModelClass
{
    XfdesktopIconViewModelClass parent_class;
};


static void xfdesktop_window_icon_model_tree_model_init(GtkTreeModelIface *iface);

static void xfdesktop_window_icon_model_get_value(GtkTreeModel *model,
                                                  GtkTreeIter *iter,
                                                  gint column,
                                                  GValue *value);

static void xfdesktop_window_icon_model_item_free(XfdesktopIconViewModel *ivmodel,
                                                  gpointer item);


G_DEFINE_TYPE_WITH_CODE(XfdesktopWindowIconModel,
                        xfdesktop_window_icon_model,
                        XFDESKTOP_TYPE_ICON_VIEW_MODEL,
                        G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_MODEL, xfdesktop_window_icon_model_tree_model_init))


static void
xfdesktop_window_icon_model_class_init(XfdesktopWindowIconModelClass *klass)
{
    XfdesktopIconViewModelClass *ivmodel_class = XFDESKTOP_ICON_VIEW_MODEL_CLASS(klass);

    ivmodel_class->model_item_ref = NULL;
    ivmodel_class->model_item_free = xfdesktop_window_icon_model_item_free;
    ivmodel_class->model_item_hash = g_direct_hash;
    ivmodel_class->model_item_equal = g_direct_equal;
}

static void
xfdesktop_window_icon_model_tree_model_init(GtkTreeModelIface *iface)
{
    iface->get_value = xfdesktop_window_icon_model_get_value;
}

static void
xfdesktop_window_icon_model_init(XfdesktopWindowIconModel *wmodel)
{
}

static void
xfdesktop_window_icon_model_get_value(GtkTreeModel *model,
                                      GtkTreeIter *iter,
                                      gint column,
                                      GValue *value)
{
    gpointer item;
    ModelItem *model_item;

    g_return_if_fail(iter != NULL);

    item = xfdesktop_icon_view_model_get_model_item(XFDESKTOP_ICON_VIEW_MODEL(model), iter);
    g_return_if_fail(item != NULL);
    model_item = (ModelItem *)item;

    switch (column) {
        case XFDESKTOP_ICON_VIEW_MODEL_COLUMN_IMAGE: {
            GIcon *icon = xfw_window_get_gicon(model_item->window);
            if (icon != NULL) {
                g_value_init(value, G_TYPE_ICON);
                g_value_set_object(value, icon);
            }
            break;
        }

        case XFDESKTOP_ICON_VIEW_MODEL_COLUMN_IMAGE_OPACITY:
            g_value_init(value, G_TYPE_DOUBLE);
            g_value_set_double(value, 0.65);
            break;

        case XFDESKTOP_ICON_VIEW_MODEL_COLUMN_LABEL: {
            const gchar *label = xfw_window_get_name(model_item->window);
            if (label != NULL) {
                g_value_init(value, G_TYPE_STRING);
                g_value_set_string(value, label);
            }
            break;
        }

        case XFDESKTOP_ICON_VIEW_MODEL_COLUMN_TOOLTIP_IMAGE:{
            GIcon *icon = xfw_window_get_gicon(model_item->window);
            if (icon != NULL) {
                g_value_init(value, G_TYPE_ICON);
                g_value_set_object(value, icon);
            }
            break;
        }

        case XFDESKTOP_ICON_VIEW_MODEL_COLUMN_TOOLTIP_TEXT: {
            const gchar *tip_text = xfw_window_get_name(model_item->window);
            if (tip_text != NULL) {
                g_value_init(value, G_TYPE_STRING);
                g_value_set_string(value, tip_text);
            }
            break;
        }

        case XFDESKTOP_ICON_VIEW_MODEL_COLUMN_ROW: {
            g_value_init(value, G_TYPE_INT);
            g_value_set_int(value, model_item->row);
            break;
        }

        case XFDESKTOP_ICON_VIEW_MODEL_COLUMN_COL: {
            g_value_init(value, G_TYPE_INT);
            g_value_set_int(value, model_item->col);
            break;
        }

        default:
            g_warning("Invalid XfdesktopWindowIconManager column %d", column);
            break;
    }
}

static void
xfdesktop_window_icon_model_item_free(XfdesktopIconViewModel *ivmodel,
                                      gpointer item)

{
    ModelItem *model_item = (ModelItem *)item;

    g_signal_handlers_disconnect_by_func(model_item->window,
                                         G_CALLBACK(xfdesktop_window_icon_model_changed),
                                         ivmodel);
    model_item_free(model_item);
}


XfdesktopWindowIconModel *
xfdesktop_window_icon_model_new(void)
{
    return g_object_new(XFDESKTOP_TYPE_WINDOW_ICON_MODEL, NULL);
}

void
xfdesktop_window_icon_model_append(XfdesktopWindowIconModel *wmodel,
                                   XfwWindow *window,
                                   GtkTreeIter *iter)
{
    ModelItem *model_item;

    g_return_if_fail(XFDESKTOP_IS_WINDOW_ICON_MODEL(wmodel));
    g_return_if_fail(XFW_IS_WINDOW(window));

    g_signal_connect_swapped(window, "name-changed",
                             G_CALLBACK(xfdesktop_window_icon_model_changed), wmodel);
    g_signal_connect_swapped(window, "icon-changed",
                             G_CALLBACK(xfdesktop_window_icon_model_changed), wmodel);

    model_item = model_item_new(window);
    xfdesktop_icon_view_model_append(XFDESKTOP_ICON_VIEW_MODEL(wmodel), window, model_item, iter);
}

void
xfdesktop_window_icon_model_remove(XfdesktopWindowIconModel *wmodel,
                                   XfwWindow *window)
{
    g_return_if_fail(XFDESKTOP_IS_WINDOW_ICON_MODEL(wmodel));
    g_return_if_fail(XFW_IS_WINDOW(window));

    xfdesktop_icon_view_model_remove(XFDESKTOP_ICON_VIEW_MODEL(wmodel), window);
}

void
xfdesktop_window_icon_model_changed(XfdesktopWindowIconModel *wmodel,
                                    XfwWindow *window)
{
    g_return_if_fail(XFDESKTOP_IS_WINDOW_ICON_MODEL(wmodel));
    g_return_if_fail(XFW_IS_WINDOW(window));

    xfdesktop_icon_view_model_changed(XFDESKTOP_ICON_VIEW_MODEL(wmodel), window);
}

void
xfdesktop_window_icon_model_set_position(XfdesktopWindowIconModel *wmodel,
                                         GtkTreeIter *iter,
                                         gint row,
                                         gint col)
{
    ModelItem *model_item;

    g_return_if_fail(XFDESKTOP_IS_WINDOW_ICON_MODEL(wmodel));
    g_return_if_fail(iter != NULL);
    g_return_if_fail(row >= -1 && col >= -1);

    model_item = xfdesktop_icon_view_model_get_model_item(XFDESKTOP_ICON_VIEW_MODEL(wmodel), iter);
    if (model_item != NULL) {
        model_item->row = row;
        model_item->col = col;
    }
}

XfwWindow *
xfdesktop_window_icon_model_get_window(XfdesktopWindowIconModel *wmodel,
                                       GtkTreeIter *iter)
{
    ModelItem *model_item;

    g_return_val_if_fail(XFDESKTOP_IS_WINDOW_ICON_MODEL(wmodel), NULL);
    g_return_val_if_fail(iter != NULL, NULL);

    model_item = xfdesktop_icon_view_model_get_model_item(XFDESKTOP_ICON_VIEW_MODEL(wmodel), iter);
    if (model_item != NULL) {
        return model_item->window;
    } else {
        return NULL;
    }
}

gboolean
xfdesktop_window_icon_model_get_window_iter(XfdesktopWindowIconModel *wmodel,
                                            XfwWindow *window,
                                            GtkTreeIter *iter)
{
    g_return_val_if_fail(XFDESKTOP_IS_WINDOW_ICON_MODEL(wmodel), FALSE);
    g_return_val_if_fail(XFW_IS_WINDOW(window), FALSE);

    return xfdesktop_icon_view_model_get_iter_for_key(XFDESKTOP_ICON_VIEW_MODEL(wmodel), window, iter);
}
