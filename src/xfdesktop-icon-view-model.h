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

#ifndef __XFDESKTOP_ICON_VIEW_MODEL_H__
#define __XFDESKTOP_ICON_VIEW_MODEL_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define XFDESKTOP_TYPE_ICON_VIEW_MODEL           (xfdesktop_icon_view_model_get_type())
#define XFDESKTOP_ICON_VIEW_MODEL(obj)           (G_TYPE_CHECK_INSTANCE_CAST((obj), XFDESKTOP_TYPE_ICON_VIEW_MODEL, XfdesktopIconViewModel))
#define XFDESKTOP_IS_ICON_VIEW_MODEL(obj)        (G_TYPE_CHECK_INSTANCE_TYPE((obj), XFDESKTOP_TYPE_ICON_VIEW_MODEL))
#define XFDESKTOP_ICON_VIEW_MODEL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), XFDESKTOP_TYPE_ICON_VIEW_MODEL, XfdesktopIconViewModelClass))
#define XFDESKTOP_ICON_VIEW_MODEL_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass), XFDESKTOP_TYPE_ICON_VIEW_MODEL, XfdesktopIconViewModelClass))

typedef enum
{
    XFDESKTOP_ICON_VIEW_MODEL_COLUMN_IMAGE,
    XFDESKTOP_ICON_VIEW_MODEL_COLUMN_IMAGE_OPACITY,
    XFDESKTOP_ICON_VIEW_MODEL_COLUMN_LABEL,
    XFDESKTOP_ICON_VIEW_MODEL_COLUMN_ROW,
    XFDESKTOP_ICON_VIEW_MODEL_COLUMN_COL,
    XFDESKTOP_ICON_VIEW_MODEL_COLUMN_SORT_PRIORITY,
    XFDESKTOP_ICON_VIEW_MODEL_COLUMN_TOOLTIP_IMAGE,
    XFDESKTOP_ICON_VIEW_MODEL_COLUMN_TOOLTIP_TEXT,

    XFDESKTOP_ICON_VIEW_MODEL_COLUMN_N_COLUMNS,
} XfdesktopIconViewModelColumn;

typedef struct _XfdesktopIconViewModel XfdesktopIconViewModel;
typedef struct _XfdesktopIconViewModelPrivate XfdesktopIconViewModelPrivate;
typedef struct _XfdesktopIconViewModelClass XfdesktopIconViewModelClass;

struct _XfdesktopIconViewModel
{
    GObject parent;

    /*< private >*/
    XfdesktopIconViewModelPrivate *priv;
};

struct _XfdesktopIconViewModelClass
{
    GObjectClass parent_class;

    gpointer (*model_item_ref)(gpointer model_item);
    void (*model_item_free)(XfdesktopIconViewModel *ivmodel, gpointer model_item);
    guint (*model_item_hash)(gconstpointer model_item);
    gint (*model_item_equal)(gconstpointer a, gconstpointer b);

    gboolean (*set_monitor)(XfdesktopIconViewModel *ivmodel,
                            GtkTreeIter *iter,
                            GdkMonitor *monitor);

};

GType xfdesktop_icon_view_model_get_type(void) G_GNUC_CONST;

/* The following should only be called by subclasses */

void xfdesktop_icon_view_model_clear(XfdesktopIconViewModel *ivmodel);

void xfdesktop_icon_view_model_append(XfdesktopIconViewModel *ivmodel,
                                      gpointer key,
                                      gpointer model_item,
                                      GtkTreeIter *iter);
void xfdesktop_icon_view_model_remove(XfdesktopIconViewModel *ivmodel,
                                      gpointer key);
void xfdesktop_icon_view_model_changed(XfdesktopIconViewModel *ivmodel,
                                       gpointer key);

gpointer xfdesktop_icon_view_model_get_model_item(XfdesktopIconViewModel *ivmodel,
                                                  GtkTreeIter *iter);
gboolean xfdesktop_icon_view_model_get_iter_for_key(XfdesktopIconViewModel *ivmodel,
                                                    gpointer key,
                                                    GtkTreeIter *iter);

G_END_DECLS

#endif  /* __XFDESKTOP_ICON_VIEW_MODEL_H__ */
