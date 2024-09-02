/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2006 Brian Tarricone, <brian@tarricone.org>
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

#ifndef __XFDESKTOP_ICON_VIEW_H__
#define __XFDESKTOP_ICON_VIEW_H__

#include <gtk/gtk.h>
#include <libxfce4windowing/libxfce4windowing.h>
#include <xfconf/xfconf.h>

G_BEGIN_DECLS

#define XFDESKTOP_TYPE_ICON_VIEW     (xfdesktop_icon_view_get_type())
#define XFDESKTOP_ICON_VIEW(obj)     (G_TYPE_CHECK_INSTANCE_CAST((obj), XFDESKTOP_TYPE_ICON_VIEW, XfdesktopIconView))
#define XFDESKTOP_IS_ICON_VIEW(obj)  (G_TYPE_CHECK_INSTANCE_TYPE((obj), XFDESKTOP_TYPE_ICON_VIEW))

typedef struct _XfdesktopIconView         XfdesktopIconView;
typedef struct _XfdesktopIconViewClass    XfdesktopIconViewClass;
typedef struct _XfdesktopIconViewPrivate  XfdesktopIconViewPrivate;

typedef enum
{
    XFDESKTOP_ICON_VIEW_GRAVITY_HORIZONTAL = 1 << 0,
    XFDESKTOP_ICON_VIEW_GRAVITY_RIGHT      = 1 << 1,
    XFDESKTOP_ICON_VIEW_GRAVITY_BOTTOM     = 1 << 2,
} XfdesktopIconViewGravity;

struct _XfdesktopIconView
{
    GtkWidget parent;

    /*< private >*/
    XfdesktopIconViewPrivate *priv;
};

struct _XfdesktopIconViewClass
{
    GtkWidgetClass parent;

    /*< signals >*/
    void (*icon_selection_changed)(XfdesktopIconView *icon_view);
    void (*icon_activated)(XfdesktopIconView *icon_view);
    void (*icon_moved)(XfdesktopIconView *icon_view,
                       GtkTreeIter *iter,
                       gint new_row,
                       gint new_col);
    gboolean (*query_icon_tooltip)(XfdesktopIconView *icon_view,
                                   GtkTreeIter *iter,
                                   gint x,
                                   gint y,
                                   gboolean keyboard_tooltip,
                                   GtkTooltip *tooltip);

    void (*start_grid_resize)(XfdesktopIconView *icon_view,
                              gint new_rows,
                              gint new_cols);
    void (*end_grid_resize)(XfdesktopIconView *icon_view);

    void (*select_all)(XfdesktopIconView *icon_view);
    void (*unselect_all)(XfdesktopIconView *icon_view);

    void (*select_cursor_item)(XfdesktopIconView *icon_view);
    void (*toggle_cursor_item)(XfdesktopIconView *icon_view);

    gboolean (*activate_selected_items)(XfdesktopIconView *icon_view);

    gboolean (*move_cursor)(XfdesktopIconView *icon_view,
                            GtkMovementStep step,
                            gint count);

    void (*resize_event)(XfdesktopIconView *icon_view);
};

GType xfdesktop_icon_view_get_type(void) G_GNUC_CONST;

GtkWidget *xfdesktop_icon_view_new(XfconfChannel *channel,
                                   XfwScreen *screen) G_GNUC_WARN_UNUSED_RESULT;
GtkWidget *xfdesktop_icon_view_new_with_model(XfconfChannel *channel,
                                              XfwScreen *screen,
                                              GtkTreeModel *model) G_GNUC_WARN_UNUSED_RESULT;

void xfdesktop_icon_view_set_model(XfdesktopIconView *icon_view,
                                   GtkTreeModel *model);
GtkTreeModel *xfdesktop_icon_view_get_model(XfdesktopIconView *icon_view);

void xfdesktop_icon_view_set_pixbuf_column(XfdesktopIconView *icon_view,
                                           gint column);
void xfdesktop_icon_view_set_icon_opacity_column(XfdesktopIconView *icon_view,
                                                 gint column);
void xfdesktop_icon_view_set_text_column(XfdesktopIconView *icon_view,
                                         gint column);
void xfdesktop_icon_view_set_search_column(XfdesktopIconView *icon_view,
                                           gint column);
void xfdesktop_icon_view_set_sort_priority_column(XfdesktopIconView *icon_view,
                                                  gint column);
void xfdesktop_icon_view_set_tooltip_icon_column(XfdesktopIconView *icon_view,
                                                 gint column);
void xfdesktop_icon_view_set_tooltip_text_column(XfdesktopIconView *icon_view,
                                                 gint column);
void xfdesktop_icon_view_set_row_column(XfdesktopIconView *icon_view,
                                        gint column);
void xfdesktop_icon_view_set_col_column(XfdesktopIconView *icon_view,
                                        gint column);

void xfdesktop_icon_view_set_selection_mode(XfdesktopIconView *icon_view,
                                            GtkSelectionMode mode);
GtkSelectionMode xfdesktop_icon_view_get_selection_mode(XfdesktopIconView *icon_view);

void xfdesktop_icon_view_enable_drag_source(XfdesktopIconView *icon_view,
                                            GdkModifierType start_button_mask,
                                            const GtkTargetEntry *targets,
                                            gint n_targets,
                                            GdkDragAction actions);
void xfdesktop_icon_view_enable_drag_dest(XfdesktopIconView *icon_view,
                                          const GtkTargetEntry *targets,
                                          gint n_targets,
                                          GdkDragAction actions);
void xfdesktop_icon_view_unset_drag_source(XfdesktopIconView *icon_view);
void xfdesktop_icon_view_unset_drag_dest(XfdesktopIconView *icon_view);

gboolean xfdesktop_icon_view_widget_coords_to_item(XfdesktopIconView *icon_view,
                                                   gint wx,
                                                   gint wy,
                                                   GtkTreeIter *iter);
gboolean xfdesktop_icon_view_widget_coords_to_slot_coords(XfdesktopIconView *icon_view,
                                                          gint wx,
                                                          gint wy,
                                                          gint *row,
                                                          gint *col);

GList *xfdesktop_icon_view_get_selected_items(XfdesktopIconView *icon_view) G_GNUC_WARN_UNUSED_RESULT;

void xfdesktop_icon_view_select_item(XfdesktopIconView *icon_view,
                                     GtkTreeIter *iter);
void xfdesktop_icon_view_select_all(XfdesktopIconView *icon_view);
void xfdesktop_icon_view_unselect_item(XfdesktopIconView *icon_view,
                                       GtkTreeIter *iter);
void xfdesktop_icon_view_unselect_all(XfdesktopIconView *icon_view);

void xfdesktop_icon_view_set_item_sensitive(XfdesktopIconView *icon_view,
                                            GtkTreeIter *iter,
                                            gboolean sensitive);

void xfdesktop_icon_view_set_icon_size(XfdesktopIconView *icon_view,
                                       gint icon_size);
gint xfdesktop_icon_view_get_icon_size(XfdesktopIconView *icon_view);

void xfdesktop_icon_view_set_show_icons_on_primary(XfdesktopIconView *icon_view,
                                                   gboolean primary);

void xfdesktop_icon_view_set_font_size(XfdesktopIconView *icon_view,
                                       gdouble font_size_points);
gdouble xfdesktop_icon_view_get_font_size(XfdesktopIconView *icon_view);
void xfdesktop_icon_view_set_use_font_size(XfdesktopIconView *icon_view,
                                           gboolean use_font_size);

void xfdesktop_icon_view_set_center_text(XfdesktopIconView *icon_view,
                                         gboolean center_text);

gboolean xfdesktop_icon_view_get_single_click(XfdesktopIconView *icon_view);
void xfdesktop_icon_view_set_single_click(XfdesktopIconView *icon_view,
                                          gboolean single_click);

void xfdesktop_icon_view_set_single_click_underline_hover(XfdesktopIconView *icon_view,
                                                          gboolean single_click_underline_hover);

void xfdesktop_icon_view_set_gravity(XfdesktopIconView *icon_view,
                                     XfdesktopIconViewGravity gravity);

void xfdesktop_icon_view_set_show_tooltips(XfdesktopIconView *icon_view,
                                           gboolean show_tooltips);
gint xfdesktop_icon_view_get_tooltip_icon_size(XfdesktopIconView *icon_view);

GtkWidget *xfdesktop_icon_view_get_window_widget(XfdesktopIconView *icon_view);

void xfdesktop_icon_view_sort_icons(XfdesktopIconView *icon_view,
                                    GtkSortType sort_type);

gboolean xfdesktop_icon_view_get_next_free_grid_position(XfdesktopIconView *icon_view,
                                                         gint row,
                                                         gint col,
                                                         gint *next_row,
                                                         gint *next_col);

// This is used only for migration from previous icon position configuration formats
gboolean xfdesktop_icon_view_grid_geometry_for_metrics(XfdesktopIconView *icon_view,
                                                       GdkRectangle *total_workarea,
                                                       GdkRectangle *monitor_workarea,
                                                       gint *first_row,
                                                       gint *first_col,
                                                       gint *last_row,
                                                       gint *last_col);

G_END_DECLS

#endif  /* __XFDESKTOP_ICON_VIEW_H__ */
