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

#define XFDESKTOP_TYPE_ICON_VIEW (xfdesktop_icon_view_get_type())
G_DECLARE_FINAL_TYPE(XfdesktopIconView, xfdesktop_icon_view, XFDESKTOP, ICON_VIEW, GtkEventBox)

typedef enum
{
    XFDESKTOP_ICON_VIEW_GRAVITY_HORIZONTAL = 1 << 0,
    XFDESKTOP_ICON_VIEW_GRAVITY_RIGHT      = 1 << 1,
    XFDESKTOP_ICON_VIEW_GRAVITY_BOTTOM     = 1 << 2,
} XfdesktopIconViewGravity;

typedef struct {
    XfdesktopIconView *source_icon_view;
    GList *dragged_icons;  // GtkTreeIter
} XfdesktopDraggedIconList;

guint xfdesktop_icon_view_get_icon_drag_info(void);
GdkAtom xfdesktop_icon_view_get_icon_drag_target(void);

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

void xfdesktop_icon_view_draw_highlight(XfdesktopIconView *icon_view,
                                        gint row,
                                        gint col);
void xfdesktop_icon_view_unset_highlight(XfdesktopIconView *icon_view);

GtkTargetList *xfdesktop_icon_view_get_drag_dest_targets(XfdesktopIconView *icon_view);

gboolean xfdesktop_icon_view_widget_coords_to_item(XfdesktopIconView *icon_view,
                                                   gint wx,
                                                   gint wy,
                                                   GtkTreeIter *iter);
gboolean xfdesktop_icon_view_widget_coords_to_slot_coords(XfdesktopIconView *icon_view,
                                                          gint wx,
                                                          gint wy,
                                                          gint *row,
                                                          gint *col);

gboolean xfdesktop_icon_view_slot_coords_to_widget_coords(XfdesktopIconView *icon_view,
                                                          gint row,
                                                          gint col,
                                                          gint *wx,
                                                          gint *wy);

gboolean xfdesktop_icon_view_get_cursor(XfdesktopIconView *icon_view,
                                        GtkTreeIter *iter,
                                        gint *row,
                                        gint *col);

GList *xfdesktop_icon_view_get_selected_items(XfdesktopIconView *icon_view) G_GNUC_WARN_UNUSED_RESULT;

void xfdesktop_icon_view_select_item(XfdesktopIconView *icon_view,
                                     GtkTreeIter *iter);
void xfdesktop_icon_view_toggle_cursor(XfdesktopIconView *icon_view);
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

void xfdesktop_icon_view_set_icon_label_fg_color(XfdesktopIconView *icon_view,
                                                 GdkRGBA *color);
void xfdesktop_icon_view_set_use_icon_label_fg_color(XfdesktopIconView *icon_view,
                                                     gboolean use);

void xfdesktop_icon_view_set_icon_label_bg_color(XfdesktopIconView *icon_view,
                                                 GdkRGBA *color);
void xfdesktop_icon_view_set_use_icon_label_bg_color(XfdesktopIconView *icon_view,
                                                     gboolean use);

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
