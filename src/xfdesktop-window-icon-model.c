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

#include <cairo-gobject.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4windowing/libxfce4windowing.h>

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

    XfwScreen *screen;
};

struct _XfdesktopWindowIconModelClass
{
    XfdesktopIconViewModelClass parent_class;
};

enum {
    PROP0,
    PROP_SCREEN,
};

static void xfdesktop_window_icon_model_constructed(GObject *object);
static void xfdesktop_window_icon_model_set_property(GObject *object,
                                                     guint property_id,
                                                     const GValue *value,
                                                     GParamSpec *pspec);
static void xfdesktop_window_icon_model_get_property(GObject *object,
                                                     guint property_id,
                                                     GValue *value,
                                                     GParamSpec *pspec);
static void xfdesktop_window_icon_model_finalize(GObject *object);

static void xfdesktop_window_icon_model_tree_model_init(GtkTreeModelIface *iface);

static void xfdesktop_window_icon_model_get_value(GtkTreeModel *model,
                                                  GtkTreeIter *iter,
                                                  gint column,
                                                  GValue *value);

static void xfdesktop_window_icon_model_item_free(XfdesktopIconViewModel *ivmodel,
                                                  gpointer item);

static void window_opened(XfwScreen *screen,
                          XfwWindow *window,
                          XfdesktopWindowIconModel *wmodel);


G_DEFINE_TYPE_WITH_CODE(XfdesktopWindowIconModel,
                        xfdesktop_window_icon_model,
                        XFDESKTOP_TYPE_ICON_VIEW_MODEL,
                        G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_MODEL, xfdesktop_window_icon_model_tree_model_init))


static void
xfdesktop_window_icon_model_class_init(XfdesktopWindowIconModelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->constructed = xfdesktop_window_icon_model_constructed;
    gobject_class->set_property = xfdesktop_window_icon_model_set_property;
    gobject_class->get_property = xfdesktop_window_icon_model_get_property;
    gobject_class->finalize = xfdesktop_window_icon_model_finalize;

    g_object_class_install_property(gobject_class,
                                    PROP_SCREEN,
                                    g_param_spec_object("screen",
                                                        "screen",
                                                        "XfwScreen",
                                                        XFW_TYPE_SCREEN,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

    XfdesktopIconViewModelClass *ivmodel_class = XFDESKTOP_ICON_VIEW_MODEL_CLASS(klass);
    ivmodel_class->model_item_ref = NULL;
    ivmodel_class->model_item_free = xfdesktop_window_icon_model_item_free;
    ivmodel_class->model_item_hash = g_direct_hash;
    ivmodel_class->model_item_equal = g_direct_equal;
}

static void
xfdesktop_window_icon_model_init(XfdesktopWindowIconModel *wmodel) {}


static void
xfdesktop_window_icon_model_constructed(GObject *object) {
    XfdesktopWindowIconModel *wmodel = XFDESKTOP_WINDOW_ICON_MODEL(object);

    G_OBJECT_CLASS(xfdesktop_window_icon_model_parent_class)->constructed(object);

    for (GList *l = xfw_screen_get_windows(wmodel->screen); l != NULL; l = l->next) {
        XfwWindow *window = XFW_WINDOW(l->data);
        window_opened(wmodel->screen, window, wmodel);
    }
    g_signal_connect(wmodel->screen, "window-opened",
                     G_CALLBACK(window_opened), wmodel);
}

static void
xfdesktop_window_icon_model_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
    XfdesktopWindowIconModel *wmodel = XFDESKTOP_WINDOW_ICON_MODEL(object);

    switch (property_id) {
        case PROP_SCREEN:
            wmodel->screen = g_value_dup_object(value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
xfdesktop_window_icon_model_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
    XfdesktopWindowIconModel *wmodel = XFDESKTOP_WINDOW_ICON_MODEL(object);

    switch (property_id) {
        case PROP_SCREEN:
            g_value_set_object(value, wmodel->screen);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
xfdesktop_window_icon_model_finalize(GObject *object) {
    XfdesktopWindowIconModel *wmodel = XFDESKTOP_WINDOW_ICON_MODEL(object);

    XfwScreen *screen = wmodel->screen;
    g_signal_handlers_disconnect_by_data(screen, wmodel);

    G_OBJECT_CLASS(xfdesktop_window_icon_model_parent_class)->finalize(object);

    // Ensure screen stays alive while the parent class finalizes all the
    // ModelItems, since it has to disconnect signals from the XfwWindow
    // instances, and we don't own references to those.
    g_object_unref(screen);
}

static void
xfdesktop_window_icon_model_tree_model_init(GtkTreeModelIface *iface)
{
    iface->get_value = xfdesktop_window_icon_model_get_value;
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
    g_signal_handlers_disconnect_by_data(model_item->window, ivmodel);
    model_item_free(model_item);
}

static void
window_monitors_changed(XfwWindow *window, GParamSpec *pspec, XfdesktopWindowIconModel *wmodel) {
    xfdesktop_window_icon_model_changed(wmodel, window);
}

static void
window_state_changed(XfwWindow *window,
                     XfwWindowState changed_mask,
                     XfwWindowState new_state,
                     XfdesktopWindowIconModel *wmodel)
{
    if ((changed_mask & (XFW_WINDOW_STATE_SKIP_TASKLIST | XFW_WINDOW_STATE_MINIMIZED)) != 0) {
        xfdesktop_window_icon_model_changed(wmodel, window);
    }
}

static void
window_closed(XfwWindow *window, XfdesktopWindowIconModel *wmodel) {
    xfdesktop_icon_view_model_remove(XFDESKTOP_ICON_VIEW_MODEL(wmodel), window);
}

static void
window_opened(XfwScreen *screen, XfwWindow *window, XfdesktopWindowIconModel *wmodel) {
    g_signal_connect_swapped(window, "name-changed",
                             G_CALLBACK(xfdesktop_window_icon_model_changed), wmodel);
    g_signal_connect_swapped(window, "icon-changed",
                             G_CALLBACK(xfdesktop_window_icon_model_changed), wmodel);
    g_signal_connect_swapped(window, "workspace-changed",
                             G_CALLBACK(xfdesktop_window_icon_model_changed), wmodel);
    g_signal_connect(window, "notify::monitors",
                     G_CALLBACK(window_monitors_changed), wmodel);
    g_signal_connect(window, "state-changed",
                     G_CALLBACK(window_state_changed), wmodel);
    g_signal_connect(window, "closed",
                     G_CALLBACK(window_closed), wmodel);

    ModelItem *model_item = model_item_new(window);
    GtkTreeIter iter;
    xfdesktop_icon_view_model_append(XFDESKTOP_ICON_VIEW_MODEL(wmodel), window, model_item, &iter);

    DBG("added window \"%s\"", xfw_window_get_name(window));
}

XfdesktopWindowIconModel *
xfdesktop_window_icon_model_new(XfwScreen *screen)
{
    return g_object_new(XFDESKTOP_TYPE_WINDOW_ICON_MODEL,
                        "screen", screen,
                        NULL);
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
