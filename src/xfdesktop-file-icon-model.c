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

#include "xfdesktop-common.h"
#include "xfdesktop-extensions.h"
#include "xfdesktop-file-icon-model.h"
#include "xfdesktop-file-icon.h"
#include "xfdesktop-icon.h"
#include "xfdesktop-icon-view-model.h"
#include "xfdesktop-regular-file-icon.h"
#include "xfdesktop-special-file-icon.h"
#include "xfdesktop-volume-icon.h"

struct _XfdesktopFileIconModel
{
    XfdesktopIconViewModel parent;
};

struct _XfdesktopFileIconModelClass
{
    XfdesktopIconViewModelClass parent_class;
};


static void xfdesktop_file_icon_model_tree_model_init(GtkTreeModelIface *iface);

static void xfdesktop_file_icon_model_get_value(GtkTreeModel *model,
                                                GtkTreeIter *iter,
                                                gint column,
                                                GValue *value);

static void xfdesktop_file_icon_model_item_free(XfdesktopIconViewModel *ivmodel,
                                                gpointer item);


G_DEFINE_TYPE_WITH_CODE(XfdesktopFileIconModel,
                        xfdesktop_file_icon_model,
                        XFDESKTOP_TYPE_ICON_VIEW_MODEL,
                        G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_MODEL, xfdesktop_file_icon_model_tree_model_init))


static void
xfdesktop_file_icon_model_class_init(XfdesktopFileIconModelClass *klass)
{
    XfdesktopIconViewModelClass *ivmodel_class = XFDESKTOP_ICON_VIEW_MODEL_CLASS(klass);

    ivmodel_class->model_item_ref = g_object_ref;
    ivmodel_class->model_item_free = xfdesktop_file_icon_model_item_free;
    ivmodel_class->model_item_hash = xfdesktop_file_icon_hash;
    ivmodel_class->model_item_equal = xfdesktop_file_icon_equal;
}

static void
xfdesktop_file_icon_model_tree_model_init(GtkTreeModelIface *iface)
{
    iface->get_value = xfdesktop_file_icon_model_get_value;
}

static void
xfdesktop_file_icon_model_init(XfdesktopFileIconModel *fmodel)
{
}

static void
xfdesktop_file_icon_model_get_value(GtkTreeModel *model,
                                    GtkTreeIter *iter,
                                    gint column,
                                    GValue *value)
{
    gpointer model_item;
    XfdesktopFileIcon *icon;

    g_return_if_fail(iter != NULL);

    model_item = xfdesktop_icon_view_model_get_model_item(XFDESKTOP_ICON_VIEW_MODEL(model), iter);
    g_return_if_fail(model_item != NULL && XFDESKTOP_IS_FILE_ICON(model_item));
    icon = XFDESKTOP_FILE_ICON(model_item);

    switch (column) {
        case XFDESKTOP_ICON_VIEW_MODEL_COLUMN_IMAGE: {
            GIcon *gicon = xfdesktop_file_icon_get_gicon(icon);
            if (icon != NULL) {
                g_value_init(value, G_TYPE_ICON);
                g_value_set_object(value, gicon);
            }
            break;
        }

        case XFDESKTOP_ICON_VIEW_MODEL_COLUMN_LABEL: {
            const gchar *label = xfdesktop_icon_peek_label(XFDESKTOP_ICON(icon));
            if (label != NULL) {
                g_value_init(value, G_TYPE_STRING);
                g_value_set_string(value, label);
            }
            break;
        }

        case XFDESKTOP_ICON_VIEW_MODEL_COLUMN_ROW: {
            gint16 row = -1, col = -1;

            g_value_init(value, G_TYPE_INT);
            xfdesktop_icon_get_position(XFDESKTOP_ICON(icon), &row, &col);
            g_value_set_int(value, row);
            break;
        }

        case XFDESKTOP_ICON_VIEW_MODEL_COLUMN_COL: {
            gint16 row = -1, col = -1;

            g_value_init(value, G_TYPE_INT);
            xfdesktop_icon_get_position(XFDESKTOP_ICON(icon), &row, &col);
            g_value_set_int(value,col);
            break;
        }

        case XFDESKTOP_ICON_VIEW_MODEL_COLUMN_SORT_PRIORITY: {
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

        case XFDESKTOP_ICON_VIEW_MODEL_COLUMN_TOOLTIP_IMAGE: {
            GIcon *gicon = xfdesktop_file_icon_get_gicon(icon);
            if (icon != NULL) {
                g_value_init(value, G_TYPE_ICON);
                g_value_set_object(value, gicon);
            }
            break;
        }

        case XFDESKTOP_ICON_VIEW_MODEL_COLUMN_TOOLTIP_TEXT: {
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

static void
xfdesktop_file_icon_model_item_free(XfdesktopIconViewModel *ivmodel,
                                    gpointer item)
{
    XfdesktopFileIcon *icon = XFDESKTOP_FILE_ICON(item);

    g_signal_handlers_disconnect_by_func(icon,
                                         G_CALLBACK(xfdesktop_file_icon_model_changed),
                                         ivmodel);
    g_object_unref(icon);
}


XfdesktopFileIconModel *
xfdesktop_file_icon_model_new(void)
{
    return g_object_new(XFDESKTOP_TYPE_FILE_ICON_MODEL, NULL);
}

void
xfdesktop_file_icon_model_append(XfdesktopFileIconModel *fmodel,
                                 XfdesktopFileIcon *icon,
                                 GtkTreeIter *iter)
{
    g_return_if_fail(XFDESKTOP_IS_FILE_ICON_MODEL(fmodel));
    g_return_if_fail(XFDESKTOP_IS_FILE_ICON(icon));

    g_signal_connect_swapped(icon, "label-changed",
                             G_CALLBACK(xfdesktop_file_icon_model_changed), fmodel);
    g_signal_connect_swapped(icon, "pixbuf-changed",
                             G_CALLBACK(xfdesktop_file_icon_model_changed), fmodel);

    xfdesktop_icon_view_model_append(XFDESKTOP_ICON_VIEW_MODEL(fmodel), icon, icon, iter);
}

void
xfdesktop_file_icon_model_remove(XfdesktopFileIconModel *fmodel,
                                 XfdesktopFileIcon *icon)
{
    g_return_if_fail(XFDESKTOP_IS_FILE_ICON_MODEL(fmodel));
    g_return_if_fail(XFDESKTOP_IS_FILE_ICON(icon));

    xfdesktop_icon_view_model_remove(XFDESKTOP_ICON_VIEW_MODEL(fmodel), icon);
}

void
xfdesktop_file_icon_model_changed(XfdesktopFileIconModel *fmodel,
                                  XfdesktopFileIcon *icon)
{
    g_return_if_fail(XFDESKTOP_IS_FILE_ICON_MODEL(fmodel));
    g_return_if_fail(XFDESKTOP_IS_FILE_ICON(icon));

    xfdesktop_icon_view_model_changed(XFDESKTOP_ICON_VIEW_MODEL(fmodel), icon);
}

XfdesktopFileIcon *
xfdesktop_file_icon_model_get_icon(XfdesktopFileIconModel *fmodel,
                                   GtkTreeIter *iter)
{
    gpointer model_item;

    g_return_val_if_fail(XFDESKTOP_IS_FILE_ICON_MODEL(fmodel), NULL);

    model_item = xfdesktop_icon_view_model_get_model_item(XFDESKTOP_ICON_VIEW_MODEL(fmodel), iter);
    if (model_item != NULL) {
        return XFDESKTOP_FILE_ICON(model_item);
    } else {
        return NULL;
    }
}
