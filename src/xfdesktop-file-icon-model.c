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


G_DEFINE_TYPE_WITH_CODE(XfdesktopFileIconModel,
                        xfdesktop_file_icon_model,
                        XFDESKTOP_TYPE_ICON_VIEW_MODEL,
                        G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_MODEL, xfdesktop_file_icon_model_tree_model_init))


static void
xfdesktop_file_icon_model_class_init(XfdesktopFileIconModelClass *klass)
{
    XfdesktopIconViewModelClass *ivmodel_class = XFDESKTOP_ICON_VIEW_MODEL_CLASS(klass);

    ivmodel_class->model_item_ref = g_object_ref;
    ivmodel_class->model_item_free = g_object_unref;
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
        case XFDESKTOP_ICON_VIEW_MODEL_COLUMN_SURFACE: {
            gint icon_width, icon_height, scale_factor;
            GdkPixbuf *pix;

            g_object_get(model,
                         "icon-width", &icon_width,
                         "icon-height", &icon_height,
                         "scale-factor", &scale_factor,
                         NULL);

            pix = xfdesktop_icon_get_pixbuf(XFDESKTOP_ICON(icon),
                                            icon_width * scale_factor,
                                            icon_height * scale_factor);
            if (pix != NULL) {
                cairo_surface_t *surface = gdk_cairo_surface_create_from_pixbuf(pix, scale_factor, NULL);
                g_value_init(value, CAIRO_GOBJECT_TYPE_SURFACE);
                g_value_take_boxed(value, surface);
                g_object_unref(pix);
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

        case XFDESKTOP_ICON_VIEW_MODEL_COLUMN_TOOLTIP_SURFACE: {
            gint tooltip_icon_size, scale_factor;

            g_object_get(model,
                         "tooltip-icon-size", &tooltip_icon_size,
                         "scale-factor", &scale_factor,
                         NULL);

            if (tooltip_icon_size > 0) {
                GdkPixbuf *pix = xfdesktop_icon_get_pixbuf(XFDESKTOP_ICON(icon),
                                                           tooltip_icon_size * scale_factor,
                                                           tooltip_icon_size * scale_factor);
                if (pix != NULL) {
                    cairo_surface_t *surface = gdk_cairo_surface_create_from_pixbuf(pix, scale_factor, NULL);
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
