/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2006-2009 Brian Tarricone, <brian@tarricone.org>
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_MATH_H
#include <math.h>
#endif

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <exo/exo.h>

#ifdef ENABLE_X11
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <gdk/gdkx.h>
#endif  /* ENABLE_X11 */

#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4windowing/libxfce4windowing.h>
#include <xfconf/xfconf.h>

#include "xfdesktop-cell-renderer-icon-label.h"
#include "xfdesktop-common.h"
#include "xfdesktop-icon-view.h"
#include "xfdesktop-marshal.h"

#define ICON_SIZE         (icon_view->priv->icon_size)
#define TEXT_WIDTH        ((icon_view->priv->cell_text_width_proportion) * ICON_SIZE)
#define ICON_WIDTH        (TEXT_WIDTH)
#define SLOT_PADDING      (icon_view->priv->slot_padding)
#define SLOT_SIZE         (TEXT_WIDTH + SLOT_PADDING * 2)
#define SPACING           (icon_view->priv->cell_spacing)
#define LABEL_RADIUS      (icon_view->priv->label_radius)
#define TEXT_HEIGHT       (SLOT_SIZE - ICON_SIZE - SPACING - (SLOT_PADDING * 2) - LABEL_RADIUS)
#define MIN_MARGIN        8

#define KEYBOARD_NAVIGATION_TIMEOUT  1500

#if defined(DEBUG) && DEBUG > 0
#define DUMP_GRID_LAYOUT(icon_view) \
{\
    gint my_i, my_maxi;\
    \
    DBG("grid layout dump:"); \
    my_maxi = icon_view->priv->nrows * icon_view->priv->ncols;\
    for(my_i = 0; my_i < my_maxi; my_i++)\
        g_printerr("%c ", icon_view->priv->grid_layout[my_i] ? '1' : '0');\
    g_printerr("\n\n");\
}
#else
#define DUMP_GRID_LAYOUT(icon_view)
#endif

enum
{
    SIG_ICON_SELECTION_CHANGED = 0,
    SIG_ICON_ACTIVATED,
    SIG_ICON_MOVED,
    SIG_QUERY_ICON_TOOLTIP,
    SIG_START_GRID_RESIZE,
    SIG_END_GRID_RESIZE,
    SIG_SELECT_ALL,
    SIG_UNSELECT_ALL,
    SIG_SELECT_CURSOR_ITEM,
    SIG_TOGGLE_CURSOR_ITEM,
    SIG_MOVE_CURSOR,
    SIG_ACTIVATE_SELECTED_ITEMS,
    SIG_RESIZE_EVENT,

    SIG_DRAG_ACTIONS_GET,
    SIG_DROP_ACTIONS_GET,
    SIG_DRAG_DROP_ASK,
    SIG_DRAG_DROP_ITEM,
    SIG_DRAG_DROP_ITEMS,
    SIG_DRAG_ITEM_DATA_RECEIVED,
    SIG_DROP_PROPOSE_ACTION,

    SIG_N_SIGNALS,
};

enum
{
    TARGET_XFDESKTOP_ICON = 9999,
};

enum
{
    PROP_0 = 0,
    PROP_CHANNEL,
    PROP_MODEL,
    PROP_ICON_SIZE,
    PROP_ICON_WIDTH,
    PROP_ICON_HEIGHT,
    PROP_ICON_FONT_SIZE,
    PROP_ICON_FONT_SIZE_SET,
    PROP_ICON_CENTER_TEXT,
    PROP_SHOW_TOOLTIPS,
    PROP_SINGLE_CLICK,
    PROP_SINGLE_CLICK_UNDERLINE_HOVER,
    PROP_GRAVITY,
    PROP_PIXBUF_COLUMN,
    PROP_TEXT_COLUMN,
    PROP_SEARCH_COLUMN,
    PROP_SORT_PRIORITY_COLUMN,
    PROP_TOOLTIP_ICON_COLUMN,
    PROP_TOOLTIP_TEXT_COLUMN,
    PROP_ROW_COLUMN,
    PROP_COL_COLUMN,
};

typedef struct
{
    union {
        // Valid if model flags include GTK_TREE_MODEL_ITERS_PERSIST
        GtkTreeIter iter;
        // Valid if model flags do not include GTK_TREE_MODEL_ITERS_PERSIST
        GtkTreeRowReference *row_ref;
    } ref;

    gint row;
    gint col;

    GdkRectangle icon_extents;
    GdkRectangle text_extents;

    GdkRectangle slot_extents;

    cairo_surface_t *pixbuf_surface;

    guint32 selected:1;
    guint32 sensitive:1;
    guint32 placed:1;
} ViewItem;

typedef gboolean (*ViewItemForeachFunc)(ViewItem *item, gpointer user_data);

static ViewItem *
view_item_new(GtkTreeModel *model, GtkTreeIter *iter)
{
    ViewItem *item;

    g_return_val_if_fail(model != NULL, NULL);
    g_return_val_if_fail(iter != NULL, NULL);

    item = g_slice_new0(ViewItem);
    item->row = -1;
    item->col = -1;
    item->sensitive = TRUE;

    if ((gtk_tree_model_get_flags(model) & GTK_TREE_MODEL_ITERS_PERSIST) != 0) {
        item->ref.iter = *iter;
    } else {
        GtkTreePath *path = gtk_tree_model_get_path(model, iter);
        if (path != NULL) {
            item->ref.row_ref = gtk_tree_row_reference_new(model, path);
            gtk_tree_path_free(path);
        }

        if (item->ref.row_ref == NULL) {
            g_warning("Invalid GtkTreeIter when creating ViewItem");
            g_slice_free(ViewItem, item);
            item = NULL;
        }
    }

    return item;
}

static gboolean
view_item_get_iter(ViewItem *item,
                   GtkTreeModel *model,
                   GtkTreeIter *iter_out)
{
    g_return_val_if_fail(model != NULL, FALSE);
    g_return_val_if_fail(iter_out != NULL, FALSE);

    if ((gtk_tree_model_get_flags(model) & GTK_TREE_MODEL_ITERS_PERSIST) != 0) {
        *iter_out = item->ref.iter;
        return TRUE;
    } else {
        gboolean valid = FALSE;
        GtkTreePath *path = gtk_tree_row_reference_get_path(item->ref.row_ref);

        if (path != NULL) {
            valid = gtk_tree_model_get_iter(model, iter_out, path);
            gtk_tree_path_free(path);
        }

        return valid;
    }
}

static GtkTreePath *
view_item_get_path(ViewItem *item,
                   GtkTreeModel *model)
{
    g_return_val_if_fail(model != NULL, FALSE);

    if ((gtk_tree_model_get_flags(model) & GTK_TREE_MODEL_ITERS_PERSIST) != 0) {
        return gtk_tree_model_get_path(model, &item->ref.iter);
    } else {
        return gtk_tree_row_reference_get_path(item->ref.row_ref);
    }
}

static void
view_item_free(ViewItem *item)
{
    if (item->pixbuf_surface != NULL) {
        cairo_surface_destroy(item->pixbuf_surface);
    }
    g_slice_free(ViewItem, item);
}


typedef struct
{
    ViewItem *item;
    gchar *label;
} SortItem;

static SortItem *
sort_item_new(ViewItem *item)
{
    SortItem *sort_item;

    g_return_val_if_fail(item != NULL, NULL);

    sort_item = g_slice_new0(SortItem);
    sort_item->item = item;

    return sort_item;
}

static gint
sort_item_compare(SortItem *a,
                  SortItem *b,
                  gpointer user_data)
{
    GtkSortType sort_type = GPOINTER_TO_INT(user_data);
    gchar *al, *bl;

    al = a->label != NULL ? a->label : "";
    bl = b->label != NULL ? b->label : "";

    return sort_type = GTK_SORT_ASCENDING
        ? g_utf8_collate(bl, al)
        : g_utf8_collate(al, bl);
}

static void
sort_item_free(SortItem *sort_item)
{
    g_return_if_fail(sort_item != NULL);

    g_free(sort_item->label);
    g_slice_free(SortItem, sort_item);
}

static gint
int_compare(gconstpointer a,
            gconstpointer b)
{
    gint ia = GPOINTER_TO_INT(a);
    gint ib = GPOINTER_TO_INT(b);
    return ia - ib;
}

struct _XfdesktopIconViewPrivate
{
    GtkWidget *parent_window;

    GtkTreeModel *model;
    gint pixbuf_column;
    gint text_column;
    gint search_column;
    gint sort_priority_column;
    gint tooltip_icon_column;
    gint tooltip_text_column;
    gint row_column;
    gint col_column;

    GtkCellRenderer *icon_renderer;
    GtkCellRenderer *text_renderer;

    gint icon_size;
    gdouble font_size;
    gboolean font_size_set;
    gboolean center_text;

    GList *items;
    GList *selected_items;

    gint width;
    gint height;

    gint xmargin;
    gint ymargin;
    gint xspacing;
    gint yspacing;

    gint nrows;
    gint ncols;
    ViewItem **grid_layout;

    GtkSelectionMode sel_mode;
    guint maybe_begin_drag:1,
          definitely_dragging:1,
          allow_rubber_banding:1,
          definitely_rubber_banding:1,
          control_click:1,
          double_click:1;
    gint press_start_x;
    gint press_start_y;
    GdkRectangle band_rect;

    XfconfChannel *channel;

    /* element-type gunichar */
    GArray *keyboard_navigation_state;
    guint keyboard_navigation_state_timeout;

    ViewItem *cursor;
    ViewItem *first_clicked_item;
    ViewItem *item_under_pointer;
    ViewItem *drop_dest_item;

    GtkTargetList *native_targets;
    GtkTargetList *source_targets;
    GtkTargetList *dest_targets;

    gboolean drag_source_set;
    GdkDragAction foreign_source_actions;
    GdkModifierType foreign_source_mask;

    gboolean drag_dest_set;
    GdkDragAction foreign_dest_actions;

    gboolean dropped;
    GdkDragAction proposed_drop_action;
    gint hover_row, hover_col;

    gint slot_padding;
    gint cell_spacing;
    gdouble label_radius;
    gdouble cell_text_width_proportion;

    gboolean ellipsize_icon_labels;

    gboolean show_tooltips;
    gint tooltip_icon_size_xfconf;
    gint tooltip_icon_size_style;

    gboolean single_click;
    gboolean single_click_underline_hover;
    XfdesktopIconViewGravity gravity;
};

static void xfdesktop_icon_view_constructed(GObject *object);
static void xfdesktop_icon_view_set_property(GObject *object,
                                             guint property_id,
                                             const GValue *value,
                                             GParamSpec *pspec);
static void xfdesktop_icon_view_get_property(GObject *object,
                                             guint property_id,
                                             GValue *value,
                                             GParamSpec *pspec);
static void xfdesktop_icon_view_dispose(GObject *obj);
static void xfdesktop_icon_view_finalize(GObject *obj);

static gboolean xfdesktop_icon_view_button_press(GtkWidget *widget,
                                                 GdkEventButton *evt,
                                                 gpointer user_data);
static gboolean xfdesktop_icon_view_button_release(GtkWidget *widget,
                                                   GdkEventButton *evt,
                                                   gpointer user_data);
static gboolean xfdesktop_icon_view_key_press(GtkWidget *widget,
                                              GdkEventKey *evt,
                                              gpointer user_data);
static gboolean xfdesktop_icon_view_focus_in(GtkWidget *widget,
                                             GdkEventFocus *evt,
                                             gpointer user_data);
static gboolean xfdesktop_icon_view_focus_out(GtkWidget *widget,
                                              GdkEventFocus *evt,
                                              gpointer user_data);
static gboolean xfdesktop_icon_view_motion_notify(GtkWidget *widget,
                                                  GdkEventMotion *evt,
                                                  gpointer user_data);
static gboolean xfdesktop_icon_view_leave_notify(GtkWidget *widget,
                                                 GdkEventCrossing *evt,
                                                 gpointer user_data);
static void xfdesktop_icon_view_style_updated(GtkWidget *widget);
static void xfdesktop_icon_view_size_allocate(GtkWidget *widget,
                                              GtkAllocation *allocation);
static void xfdesktop_icon_view_realize(GtkWidget *widget);
static void xfdesktop_icon_view_unrealize(GtkWidget *widget);
static gboolean xfdesktop_icon_view_draw(GtkWidget *widget,
                                         cairo_t *cr);
static void xfdesktop_icon_view_drag_begin(GtkWidget *widget,
                                           GdkDragContext *contest);
static gboolean xfdesktop_icon_view_drag_motion(GtkWidget *widget,
                                                GdkDragContext *context,
                                                gint x,
                                                gint y,
                                                guint time_);
static void xfdesktop_icon_view_drag_leave(GtkWidget *widget,
                                           GdkDragContext *context,
                                           guint time_);
static gboolean xfdesktop_icon_view_drag_drop(GtkWidget *widget,
                                              GdkDragContext *context,
                                              gint x,
                                              gint y,
                                              guint time_);
static void xfdesktop_icon_view_drag_data_received(GtkWidget *widget,
                                                   GdkDragContext *context,
                                                   gint x,
                                                   gint y,
                                                   GtkSelectionData *data,
                                                   guint info,
                                                   guint time_);

static inline void xfdesktop_icon_view_clear_drag_highlight(XfdesktopIconView *icon_view);

static void xfdesktop_icon_view_add_move_binding(GtkBindingSet *binding_set,
                                                 guint keyval,
                                                 guint modmask,
                                                 GtkMovementStep step,
                                                 gint count);

static void xfdesktop_icon_view_init_builtin_cell_renderers(XfdesktopIconView *icon_view);
static void xfdesktop_icon_view_populate_items(XfdesktopIconView *icon_view);
static gboolean xfdesktop_icon_view_place_item(XfdesktopIconView *icon_view,
                                               ViewItem *item,
                                               gboolean honor_model_position);
static gboolean xfdesktop_icon_view_place_item_at(XfdesktopIconView *icon_view,
                                                  ViewItem *item,
                                                  gint row,
                                                  gint col);
static void xfdesktop_icon_view_unplace_item(XfdesktopIconView *icon_view,
                                             ViewItem *item);
static void xfdesktop_icon_view_update_item_extents(XfdesktopIconView *icon_view,
                                                    ViewItem *item);

static void xfdesktop_icon_view_invalidate_all(XfdesktopIconView *icon_view,
                                               gboolean recalc_extents);
static void xfdesktop_icon_view_invalidate_item(XfdesktopIconView *icon_view,
                                                ViewItem *item,
                                                gboolean recalc_extents);

static void xfdesktop_icon_view_invalidate_pixbuf_cache(XfdesktopIconView *icon_view);

static void xfdesktop_icon_view_select_item_internal(XfdesktopIconView *icon_view,
                                                     ViewItem *item,
                                                     gboolean emit_signal);
static void xfdesktop_icon_view_unselect_item_internal(XfdesktopIconView *icon_view,
                                                       ViewItem *item,
                                                       gboolean emit_signal);

static void xfdesktop_icon_view_size_grid(XfdesktopIconView *icon_view);
static inline gboolean xfdesktop_grid_is_free_position(XfdesktopIconView *icon_view,
                                                       gint row,
                                                       gint col);
static inline void xfdesktop_grid_set_position_free(XfdesktopIconView *icon_view,
                                                    gint row,
                                                    gint col);
static inline gboolean xfdesktop_grid_unset_position_free_raw(XfdesktopIconView *icon_view,
                                                              gint row,
                                                              gint col,
                                                              gpointer data);
static void xfdesktop_icon_view_clear_grid_layout(XfdesktopIconView *icon_view);
static inline ViewItem *xfdesktop_icon_view_item_in_slot_raw(XfdesktopIconView *icon_view,
                                                             gint idx);
static inline ViewItem *xfdesktop_icon_view_item_in_slot(XfdesktopIconView *icon_view,
                                                         gint row,
                                                         gint col);
static ViewItem *xfdesktop_icon_view_widget_coords_to_item_internal(XfdesktopIconView *icon_view,
                                                                    gint wx,
                                                                    gint wy);

static gint xfdesktop_check_icon_clicked(gconstpointer data,
                                         gconstpointer user_data);

static inline void xfdesktop_xy_to_rowcol(XfdesktopIconView *icon_view,
                                          gint x,
                                          gint y,
                                          gint *row,
                                          gint *col);
static inline gboolean xfdesktop_rectangle_contains_point(GdkRectangle *rect,
                                                          gint x,
                                                          gint y);

static gboolean xfdesktop_icon_view_show_tooltip(GtkWidget *widget,
                                                 gint x,
                                                 gint y,
                                                 gboolean keyboard_tooltip,
                                                 GtkTooltip *tooltip,
                                                 gpointer user_data);
static void xfdesktop_icon_view_xfconf_tooltip_icon_size_changed(XfconfChannel *channel,
                                                                 const gchar *property,
                                                                 const GValue *value,
                                                                 XfdesktopIconView *icon_view);

static void xfdesktop_icon_view_real_select_all(XfdesktopIconView *icon_view);
static void xfdesktop_icon_view_real_unselect_all(XfdesktopIconView *icon_view);
static void xfdesktop_icon_view_real_select_cursor_item(XfdesktopIconView *icon_view);
static void xfdesktop_icon_view_real_toggle_cursor_item(XfdesktopIconView *icon_view);
static gboolean xfdesktop_icon_view_real_activate_selected_items(XfdesktopIconView *icon_view);
static gboolean xfdesktop_icon_view_real_move_cursor(XfdesktopIconView *icon_view,
                                                     GtkMovementStep step,
                                                     gint count);

static void xfdesktop_icon_view_select_between(XfdesktopIconView *icon_view,
                                               ViewItem *start_item,
                                               ViewItem *end_item);

static cairo_surface_t *xfdesktop_icon_view_get_surface_for_item(XfdesktopIconView *icon_view,
                                                                 ViewItem *item);

static void xfdesktop_icon_view_connect_model_signals(XfdesktopIconView *icon_view);
static void xfdesktop_icon_view_disconnect_model_signals(XfdesktopIconView *icon_view);

static void xfdesktop_icon_view_model_row_inserted(GtkTreeModel *model,
                                                   GtkTreePath *path,
                                                   GtkTreeIter *iter,
                                                   XfdesktopIconView *icon_view);
static void xfdesktop_icon_view_model_row_changed(GtkTreeModel *model,
                                                  GtkTreePath *path,
                                                  GtkTreeIter *iter,
                                                  XfdesktopIconView *icon_view);
static void xfdesktop_icon_view_model_row_deleted(GtkTreeModel *model,
                                                  GtkTreePath *path,
                                                  XfdesktopIconView *icon_view);

static void xfdesktop_icon_view_items_free(XfdesktopIconView *icon_view);


static const GtkTargetEntry icon_view_targets[] = {
    { "XFDESKTOP_ICON", GTK_TARGET_SAME_APP, TARGET_XFDESKTOP_ICON }
};
static const gint icon_view_n_targets = 1;

static guint __signals[SIG_N_SIGNALS] = { 0, };

static const struct
{
    const gchar *setting;
    GType setting_type;
    const gchar *property;
} setting_bindings[] = {
    { DESKTOP_ICONS_ICON_SIZE_PROP, G_TYPE_INT, "icon-size" },
    { DESKTOP_ICONS_CUSTOM_FONT_SIZE_PROP, G_TYPE_BOOLEAN, "icon-font-size-set" },
    { DESKTOP_ICONS_FONT_SIZE_PROP, G_TYPE_INT, "icon-font-size" },
    { DESKTOP_ICONS_SHOW_TOOLTIP_PROP, G_TYPE_BOOLEAN, "show-tooltips" },
    { DESKTOP_ICONS_SINGLE_CLICK_PROP, G_TYPE_BOOLEAN, "single-click" },
    { DESKTOP_ICONS_SINGLE_CLICK_ULINE_PROP, G_TYPE_BOOLEAN, "single-click-underline-hover" },
    { DESKTOP_ICONS_GRAVITY_PROP, G_TYPE_INT, "gravity" },
};

static ViewItem *TOMBSTONE = NULL;


G_DEFINE_TYPE_WITH_PRIVATE(XfdesktopIconView, xfdesktop_icon_view, GTK_TYPE_WIDGET)


static void
xfdesktop_icon_view_class_init(XfdesktopIconViewClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;
    GtkWidgetClass *widget_class = (GtkWidgetClass *)klass;
    GtkBindingSet *binding_set;

    binding_set = gtk_binding_set_by_class(klass);

    gobject_class->constructed = xfdesktop_icon_view_constructed;
    gobject_class->dispose = xfdesktop_icon_view_dispose;
    gobject_class->finalize = xfdesktop_icon_view_finalize;
    gobject_class->set_property = xfdesktop_icon_view_set_property;
    gobject_class->get_property = xfdesktop_icon_view_get_property;

    widget_class->style_updated = xfdesktop_icon_view_style_updated;
    widget_class->size_allocate = xfdesktop_icon_view_size_allocate;
    widget_class->realize = xfdesktop_icon_view_realize;
    widget_class->unrealize = xfdesktop_icon_view_unrealize;
    widget_class->draw = xfdesktop_icon_view_draw;
    widget_class->drag_begin = xfdesktop_icon_view_drag_begin;
    widget_class->drag_motion = xfdesktop_icon_view_drag_motion;
    widget_class->drag_leave = xfdesktop_icon_view_drag_leave;
    widget_class->drag_drop = xfdesktop_icon_view_drag_drop;
    widget_class->drag_data_received = xfdesktop_icon_view_drag_data_received;

    klass->select_all = xfdesktop_icon_view_real_select_all;
    klass->unselect_all = xfdesktop_icon_view_real_unselect_all;
    klass->select_cursor_item = xfdesktop_icon_view_real_select_cursor_item;
    klass->toggle_cursor_item = xfdesktop_icon_view_real_toggle_cursor_item;
    klass->activate_selected_items = xfdesktop_icon_view_real_activate_selected_items;
    klass->move_cursor = xfdesktop_icon_view_real_move_cursor;

    __signals[SIG_ICON_SELECTION_CHANGED] = g_signal_new("icon-selection-changed",
                                                         XFDESKTOP_TYPE_ICON_VIEW,
                                                         G_SIGNAL_RUN_LAST,
                                                         G_STRUCT_OFFSET(XfdesktopIconViewClass,
                                                                         icon_selection_changed),
                                                         NULL, NULL,
                                                         g_cclosure_marshal_VOID__VOID,
                                                         G_TYPE_NONE, 0);

    __signals[SIG_ICON_ACTIVATED] = g_signal_new("icon-activated",
                                                 XFDESKTOP_TYPE_ICON_VIEW,
                                                 G_SIGNAL_RUN_LAST,
                                                 G_STRUCT_OFFSET(XfdesktopIconViewClass,
                                                                 icon_activated),
                                                 NULL, NULL,
                                                 g_cclosure_marshal_VOID__VOID,
                                                 G_TYPE_NONE, 0);

    __signals[SIG_ICON_MOVED] = g_signal_new("icon-moved",
                                             XFDESKTOP_TYPE_ICON_VIEW,
                                             G_SIGNAL_RUN_LAST,
                                             G_STRUCT_OFFSET(XfdesktopIconViewClass, icon_moved),
                                             NULL, NULL,
                                             xfdesktop_marshal_VOID__BOXED_INT_INT,
                                             G_TYPE_NONE, 3,
                                             GTK_TYPE_TREE_ITER,
                                             G_TYPE_INT,
                                             G_TYPE_INT);

    __signals[SIG_QUERY_ICON_TOOLTIP] = g_signal_new("query-icon-tooltip",
                                                     XFDESKTOP_TYPE_ICON_VIEW,
                                                     G_SIGNAL_RUN_LAST,
                                                     G_STRUCT_OFFSET(XfdesktopIconViewClass, query_icon_tooltip),
                                                     NULL, NULL,
                                                     xfdesktop_marshal_BOOLEAN__BOXED_INT_INT_BOOLEAN_OBJECT,
                                                     G_TYPE_BOOLEAN, 5,
                                                     GTK_TYPE_TREE_ITER,
                                                     G_TYPE_INT,
                                                     G_TYPE_INT,
                                                     G_TYPE_BOOLEAN,
                                                     GTK_TYPE_TOOLTIP);

    __signals[SIG_START_GRID_RESIZE] = g_signal_new(I_("start-grid-resize"),
                                                    XFDESKTOP_TYPE_ICON_VIEW,
                                                    G_SIGNAL_RUN_LAST,
                                                    G_STRUCT_OFFSET(XfdesktopIconViewClass, start_grid_resize),
                                                    NULL, NULL,
                                                    xfdesktop_marshal_VOID__INT_INT,
                                                    G_TYPE_NONE, 2,
                                                    G_TYPE_INT,
                                                    G_TYPE_INT);

    __signals[SIG_END_GRID_RESIZE] = g_signal_new(I_("end-grid-resize"),
                                                    XFDESKTOP_TYPE_ICON_VIEW,
                                                    G_SIGNAL_RUN_LAST,
                                                    G_STRUCT_OFFSET(XfdesktopIconViewClass, start_grid_resize),
                                                    NULL, NULL,
                                                    g_cclosure_marshal_VOID__VOID,
                                                    G_TYPE_NONE, 0);

    __signals[SIG_SELECT_ALL] = g_signal_new(I_("select-all"),
                                             XFDESKTOP_TYPE_ICON_VIEW,
                                             G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                             G_STRUCT_OFFSET(XfdesktopIconViewClass,
                                                             select_all),
                                             NULL, NULL,
                                             g_cclosure_marshal_VOID__VOID,
                                             G_TYPE_NONE, 0);

    __signals[SIG_UNSELECT_ALL] = g_signal_new(I_("unselect-all"),
                                               XFDESKTOP_TYPE_ICON_VIEW,
                                               G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                               G_STRUCT_OFFSET(XfdesktopIconViewClass,
                                                               unselect_all),
                                               NULL, NULL,
                                               g_cclosure_marshal_VOID__VOID,
                                               G_TYPE_NONE, 0);

    __signals[SIG_SELECT_CURSOR_ITEM] = g_signal_new(I_("select-cursor-item"),
                                                     XFDESKTOP_TYPE_ICON_VIEW,
                                                     G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                                     G_STRUCT_OFFSET(XfdesktopIconViewClass,
                                                                     select_cursor_item),
                                                     NULL, NULL,
                                                     g_cclosure_marshal_VOID__VOID,
                                                     G_TYPE_NONE, 0);

    __signals[SIG_TOGGLE_CURSOR_ITEM] = g_signal_new(I_("toggle-cursor-item"),
                                                     XFDESKTOP_TYPE_ICON_VIEW,
                                                     G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                                     G_STRUCT_OFFSET(XfdesktopIconViewClass,
                                                                     toggle_cursor_item),
                                                     NULL, NULL,
                                                     g_cclosure_marshal_VOID__VOID,
                                                     G_TYPE_NONE, 0);

    __signals[SIG_ACTIVATE_SELECTED_ITEMS] = g_signal_new(I_("activate-selected-items"),
                                                       XFDESKTOP_TYPE_ICON_VIEW,
                                                       G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                                       G_STRUCT_OFFSET(XfdesktopIconViewClass,
                                                                       activate_selected_items),
                                                       NULL, NULL,
                                                       xfdesktop_marshal_BOOLEAN__VOID,
                                                       G_TYPE_BOOLEAN, 0);

    __signals[SIG_MOVE_CURSOR] = g_signal_new(I_("move-cursor"),
                                              XFDESKTOP_TYPE_ICON_VIEW,
                                              G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                              G_STRUCT_OFFSET(XfdesktopIconViewClass,
                                                              move_cursor),
                                              NULL, NULL,
                                              xfdesktop_marshal_BOOLEAN__ENUM_INT,
                                              G_TYPE_BOOLEAN, 2,
                                              GTK_TYPE_MOVEMENT_STEP,
                                              G_TYPE_INT);

    __signals[SIG_RESIZE_EVENT] = g_signal_new(I_("resize-event"),
                                               XFDESKTOP_TYPE_ICON_VIEW,
                                               G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                               G_STRUCT_OFFSET(XfdesktopIconViewClass,
                                                               resize_event),
                                               NULL, NULL,
                                               g_cclosure_marshal_VOID__VOID,
                                               G_TYPE_NONE, 0);

    // Asks for what drag actions are allowed for the given item
    __signals[SIG_DRAG_ACTIONS_GET] = g_signal_new(I_("drag-actions-get"),
                                                   XFDESKTOP_TYPE_ICON_VIEW,
                                                   G_SIGNAL_RUN_LAST,
                                                   0,
                                                   NULL, NULL,
                                                   xfdesktop_marshal_FLAGS__BOXED,
                                                   GDK_TYPE_DRAG_ACTION, 1,
                                                   GTK_TYPE_TREE_ITER);
    // Asks for what drop actions are allowed for the given item
    __signals[SIG_DROP_ACTIONS_GET] = g_signal_new(I_("drop-actions-get"),
                                                   XFDESKTOP_TYPE_ICON_VIEW,
                                                   G_SIGNAL_RUN_LAST,
                                                   0,
                                                   NULL, NULL,
                                                   xfdesktop_marshal_FLAGS__BOXED_POINTER,
                                                   GDK_TYPE_DRAG_ACTION, 2,
                                                   GTK_TYPE_TREE_ITER,
                                                   G_TYPE_POINTER);
    // Asks to present some UI to allow the user to select a drag action
    __signals[SIG_DRAG_DROP_ASK] = g_signal_new(I_("drag-drop-ask"),
                                                XFDESKTOP_TYPE_ICON_VIEW,
                                                G_SIGNAL_RUN_LAST,
                                                0,
                                                NULL, NULL,
                                                xfdesktop_marshal_FLAGS__OBJECT_BOXED_INT_INT_UINT,
                                                GDK_TYPE_DRAG_ACTION, 5,
                                                GDK_TYPE_DRAG_CONTEXT,
                                                GTK_TYPE_TREE_ITER,
                                                G_TYPE_INT,
                                                G_TYPE_INT,
                                                G_TYPE_UINT);
    // Analogous to GtkWidget::drag-drop, item can be present and is a drop site, or null (drop on an empty location)
    __signals[SIG_DRAG_DROP_ITEM] = g_signal_new(I_("drag-drop-item"),
                                                 XFDESKTOP_TYPE_ICON_VIEW,
                                                 G_SIGNAL_RUN_LAST,
                                                 0,
                                                 NULL, NULL,
                                                 xfdesktop_marshal_BOOLEAN__OBJECT_BOXED_INT_INT_UINT,
                                                 G_TYPE_BOOLEAN, 5,
                                                 GDK_TYPE_DRAG_CONTEXT,
                                                 GTK_TYPE_TREE_ITER,
                                                 G_TYPE_INT,
                                                 G_TYPE_INT,
                                                 G_TYPE_UINT);
    // Asks a (local) dest item to handle a drag of one or more (local) src items
    __signals[SIG_DRAG_DROP_ITEMS] = g_signal_new(I_("drag-drop-items"),
                                                  XFDESKTOP_TYPE_ICON_VIEW,
                                                  G_SIGNAL_RUN_LAST,
                                                  0,
                                                  NULL, NULL,
                                                  xfdesktop_marshal_BOOLEAN__OBJECT_BOXED_POINTER_FLAGS,
                                                  G_TYPE_BOOLEAN, 4,
                                                  GDK_TYPE_DRAG_CONTEXT,
                                                  GTK_TYPE_TREE_ITER,
                                                  G_TYPE_POINTER,
                                                  GDK_TYPE_DRAG_ACTION);
    // Fired when drag data is received to be dropped; item can be present and is a drop site, or null (drop on an empty location)
    __signals[SIG_DRAG_ITEM_DATA_RECEIVED] = g_signal_new(I_("drag-item-data-received"),
                                                          XFDESKTOP_TYPE_ICON_VIEW,
                                                          G_SIGNAL_RUN_LAST,
                                                          0,
                                                          NULL, NULL,
                                                          xfdesktop_marshal_VOID__OBJECT_BOXED_INT_INT_BOXED_UINT_UINT,
                                                          G_TYPE_NONE, 7,
                                                          GDK_TYPE_DRAG_CONTEXT,
                                                          GTK_TYPE_TREE_ITER,
                                                          G_TYPE_INT,
                                                          G_TYPE_INT,
                                                          GTK_TYPE_SELECTION_DATA,
                                                          G_TYPE_UINT,
                                                          G_TYPE_UINT);
    // Asks for a proposed action based on a drop item (or null, if dropping on an empty area), and the drag data provided
    __signals[SIG_DROP_PROPOSE_ACTION] = g_signal_new(I_("drop-propose-action"),
                                                      XFDESKTOP_TYPE_ICON_VIEW,
                                                      G_SIGNAL_RUN_LAST,
                                                      0,
                                                      NULL, NULL,
                                                      xfdesktop_marshal_FLAGS__OBJECT_BOXED_FLAGS_BOXED_UINT,
                                                      GDK_TYPE_DRAG_ACTION, 5,
                                                      GDK_TYPE_DRAG_CONTEXT,
                                                      GTK_TYPE_TREE_ITER,
                                                      GDK_TYPE_DRAG_ACTION,
                                                      GTK_TYPE_SELECTION_DATA,
                                                      G_TYPE_UINT);

    gtk_widget_class_install_style_property(widget_class,
                                            g_param_spec_int("cell-spacing",
                                                             "Cell spacing",
                                                             "Spacing between desktop icon cells",
                                                             0, 255, 2,
                                                             G_PARAM_READABLE));

    gtk_widget_class_install_style_property(widget_class,
                                            g_param_spec_int("cell-padding",
                                                             "Cell padding",
                                                             "Padding in desktop icon cell",
                                                             0, 255, 6,
                                                             G_PARAM_READABLE));

    gtk_widget_class_install_style_property(widget_class,
                                            g_param_spec_double("cell-text-width-proportion",
                                                                "Cell text width proportion",
                                                                "Width of text in desktop icon cell, "
                                                                "calculated as multiplier of the icon size",
                                                                1.0, 10.0, 1.9,
                                                                G_PARAM_READABLE));
    gtk_widget_class_install_style_property(widget_class,
                                            g_param_spec_boolean("ellipsize-icon-labels",
                                                                 "Ellipsize Icon Labels",
                                                                 "Ellipzize labels of unselected icons on desktop",
                                                                 TRUE,
                                                                 G_PARAM_READABLE));

    gtk_widget_class_install_style_property(widget_class,
                                            g_param_spec_double("label-radius",
                                                                "Label radius",
                                                                "The radius of the rounded corners of the text background",
                                                                0.0, 50.0, 4.0,
                                                                G_PARAM_READABLE));

    gtk_widget_class_install_style_property(widget_class,
                                            g_param_spec_uint("tooltip-size",
                                                              "Tooltip Image Size",
                                                              "The size of the tooltip image preview",
                                                              0, MAX_TOOLTIP_ICON_SIZE, DEFAULT_TOOLTIP_ICON_SIZE,
                                                              G_PARAM_READABLE));

#define PARAM_FLAGS  (G_PARAM_READWRITE \
                      | G_PARAM_CONSTRUCT \
                      | G_PARAM_STATIC_NAME \
                      | G_PARAM_STATIC_NICK \
                      | G_PARAM_STATIC_BLURB)

    g_object_class_install_property(gobject_class, PROP_CHANNEL,
                                    g_param_spec_object("channel",
                                                        "channel",
                                                        "channel",
                                                        XFCONF_TYPE_CHANNEL,
                                                        (PARAM_FLAGS | G_PARAM_CONSTRUCT_ONLY) & ~G_PARAM_CONSTRUCT));

    g_object_class_install_property(gobject_class, PROP_MODEL,
                                    g_param_spec_object("model",
                                                        "model",
                                                        "model",
                                                        GTK_TYPE_TREE_MODEL,
                                                        PARAM_FLAGS));

    g_object_class_install_property(gobject_class, PROP_ICON_SIZE,
                                    g_param_spec_int("icon-size",
                                                     "icon size",
                                                     "icon size",
                                                     MIN_ICON_SIZE, MAX_ICON_SIZE, DEFAULT_ICON_SIZE,
                                                     PARAM_FLAGS));

    g_object_class_install_property(gobject_class, PROP_ICON_WIDTH,
                                    g_param_spec_int("icon-width",
                                                     "icon width",
                                                     "allowed icon width, which can be larger than icon-size",
                                                     MIN_ICON_SIZE, MAX_ICON_SIZE, DEFAULT_ICON_SIZE,
                                                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_ICON_HEIGHT,
                                    g_param_spec_int("icon-height",
                                                     "icon height",
                                                     "allowed icon height, which is usually the same as icon-size",
                                                     MIN_ICON_SIZE, MAX_ICON_SIZE, DEFAULT_ICON_SIZE,
                                                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_ICON_FONT_SIZE,
                                    g_param_spec_double("icon-font-size",
                                                        "icon font size",
                                                        "icon font size",
                                                        MIN_ICON_FONT_SIZE, MAX_ICON_FONT_SIZE, DEFAULT_ICON_FONT_SIZE,
                                                        PARAM_FLAGS));

    g_object_class_install_property(gobject_class, PROP_ICON_FONT_SIZE_SET,
                                    g_param_spec_boolean("icon-font-size-set",
                                                         "icon font size set",
                                                         "icon font size set",
                                                         DEFAULT_ICON_FONT_SIZE_SET,
                                                         PARAM_FLAGS));

    g_object_class_install_property(gobject_class, PROP_ICON_CENTER_TEXT,
                                    g_param_spec_boolean("icon-center-text",
                                                         "icon center text",
                                                         "icon center text",
                                                         DEFAULT_ICON_CENTER_TEXT,
                                                         PARAM_FLAGS));

    g_object_class_install_property(gobject_class, PROP_SHOW_TOOLTIPS,
                                    g_param_spec_boolean("show-tooltips",
                                                         "show tooltips",
                                                         "show tooltips on icon hover",
                                                         DEFAULT_SHOW_TOOLTIPS,
                                                         PARAM_FLAGS));

    g_object_class_install_property(gobject_class, PROP_SINGLE_CLICK,
                                    g_param_spec_boolean("single-click",
                                                         "single-click",
                                                         "single-click",
                                                         DEFAULT_SINGLE_CLICK,
                                                         PARAM_FLAGS));

    g_object_class_install_property(gobject_class, PROP_SINGLE_CLICK_UNDERLINE_HOVER,
                                    g_param_spec_boolean("single-click-underline-hover",
                                                         "single-click-underline-hover",
                                                         "single-click-underline-hover",
                                                         DEFAULT_SINGLE_CLICK_ULINE,
                                                         PARAM_FLAGS));

    g_object_class_install_property(gobject_class, PROP_GRAVITY,
                                    g_param_spec_int("gravity",
                                                     "gravity",
                                                     "set gravity of icons placement",
                                                     MIN_GRAVITY, MAX_GRAVITY, DEFAULT_GRAVITY,
                                                     PARAM_FLAGS));
#define DECL_COLUMN_PROP(prop_id, name) \
    g_object_class_install_property(gobject_class, \
                                    prop_id, \
                                    g_param_spec_int(name, \
                                                     name, \
                                                     name, \
                                                     -1, \
                                                     G_MAXINT, \
                                                     -1, \
                                                     PARAM_FLAGS))

    DECL_COLUMN_PROP(PROP_PIXBUF_COLUMN, "pixbuf-column");
    DECL_COLUMN_PROP(PROP_TEXT_COLUMN, "text-column");
    DECL_COLUMN_PROP(PROP_SEARCH_COLUMN, "search-column");
    DECL_COLUMN_PROP(PROP_SORT_PRIORITY_COLUMN, "sort-priority-column");
    DECL_COLUMN_PROP(PROP_TOOLTIP_ICON_COLUMN, "tooltip-icon-column");
    DECL_COLUMN_PROP(PROP_TOOLTIP_TEXT_COLUMN, "tooltip-text-column");
    DECL_COLUMN_PROP(PROP_ROW_COLUMN, "row-column");
    DECL_COLUMN_PROP(PROP_COL_COLUMN, "col-column");

#undef DECL_COLUMN_PROP
#undef PARAM_FLAGS

    /* same binding entries as GtkIconView */
    gtk_binding_entry_add_signal(binding_set, GDK_KEY_a, GDK_CONTROL_MASK,
                                 "select-all", 0);
    gtk_binding_entry_add_signal(binding_set, GDK_KEY_a,
                                 GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                 "unselect-all", 0);
    gtk_binding_entry_add_signal(binding_set, GDK_KEY_space, GDK_CONTROL_MASK,
                                 "toggle-cursor-item", 0);
    gtk_binding_entry_add_signal(binding_set, GDK_KEY_KP_Space, GDK_CONTROL_MASK,
                                 "toggle-cursor-item", 0);

    gtk_binding_entry_add_signal(binding_set, GDK_KEY_space, 0,
                                 "activate-selected-items", 0);
    gtk_binding_entry_add_signal(binding_set, GDK_KEY_KP_Space, 0,
                                 "activate-selected-items", 0);
    gtk_binding_entry_add_signal(binding_set, GDK_KEY_Return, 0,
                                 "activate-selected-items", 0);
    gtk_binding_entry_add_signal(binding_set, GDK_KEY_ISO_Enter, 0,
                                 "activate-selected-items", 0);
    gtk_binding_entry_add_signal(binding_set, GDK_KEY_KP_Enter, 0,
                                 "activate-selected-items", 0);

    xfdesktop_icon_view_add_move_binding(binding_set, GDK_KEY_Up, 0,
                                         GTK_MOVEMENT_DISPLAY_LINES, -1);
    xfdesktop_icon_view_add_move_binding(binding_set, GDK_KEY_KP_Up, 0,
                                         GTK_MOVEMENT_DISPLAY_LINES, -1);

    xfdesktop_icon_view_add_move_binding(binding_set, GDK_KEY_Down, 0,
                                         GTK_MOVEMENT_DISPLAY_LINES, 1);
    xfdesktop_icon_view_add_move_binding(binding_set, GDK_KEY_KP_Down, 0,
                                         GTK_MOVEMENT_DISPLAY_LINES, 1);

    xfdesktop_icon_view_add_move_binding(binding_set, GDK_KEY_p, GDK_CONTROL_MASK,
                                         GTK_MOVEMENT_DISPLAY_LINES, -1);

    xfdesktop_icon_view_add_move_binding(binding_set, GDK_KEY_n, GDK_CONTROL_MASK,
                                         GTK_MOVEMENT_DISPLAY_LINES, 1);

    xfdesktop_icon_view_add_move_binding(binding_set, GDK_KEY_Home, 0,
                                         GTK_MOVEMENT_BUFFER_ENDS, -1);
    xfdesktop_icon_view_add_move_binding(binding_set, GDK_KEY_KP_Home, 0,
                                         GTK_MOVEMENT_BUFFER_ENDS, -1);

    xfdesktop_icon_view_add_move_binding(binding_set, GDK_KEY_End, 0,
                                         GTK_MOVEMENT_BUFFER_ENDS, 1);
    xfdesktop_icon_view_add_move_binding(binding_set, GDK_KEY_KP_End, 0,
                                         GTK_MOVEMENT_BUFFER_ENDS, 1);

    xfdesktop_icon_view_add_move_binding(binding_set, GDK_KEY_Right, 0,
                                         GTK_MOVEMENT_VISUAL_POSITIONS, 1);
    xfdesktop_icon_view_add_move_binding(binding_set, GDK_KEY_Left, 0,
                                         GTK_MOVEMENT_VISUAL_POSITIONS, -1);

    xfdesktop_icon_view_add_move_binding(binding_set, GDK_KEY_KP_Right, 0,
                                         GTK_MOVEMENT_VISUAL_POSITIONS, 1);
    xfdesktop_icon_view_add_move_binding(binding_set, GDK_KEY_KP_Left, 0,
                                         GTK_MOVEMENT_VISUAL_POSITIONS, -1);

    gtk_widget_class_set_css_name (widget_class, "XfdesktopIconView");

    TOMBSTONE = g_slice_new0(ViewItem);
}

static void
xfdesktop_icon_view_init(XfdesktopIconView *icon_view)
{
    icon_view->priv = xfdesktop_icon_view_get_instance_private(icon_view);

    icon_view->priv->pixbuf_column = -1;
    icon_view->priv->text_column = -1;
    icon_view->priv->search_column = -1;
    icon_view->priv->tooltip_icon_column = -1;
    icon_view->priv->tooltip_text_column = -1;
    icon_view->priv->row_column = -1;
    icon_view->priv->col_column = -1;

    icon_view->priv->icon_size = DEFAULT_ICON_SIZE;
    icon_view->priv->font_size = DEFAULT_ICON_FONT_SIZE;
    icon_view->priv->font_size_set = DEFAULT_ICON_FONT_SIZE_SET;
    icon_view->priv->gravity = DEFAULT_GRAVITY;
    icon_view->priv->show_tooltips = DEFAULT_SHOW_TOOLTIPS;
    icon_view->priv->tooltip_icon_size_xfconf = 0;
    icon_view->priv->tooltip_icon_size_style = 0;

    icon_view->priv->allow_rubber_banding = TRUE;
}

static void
xfdesktop_icon_view_constructed(GObject *object)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(object);
    GtkStyleContext *context;

    G_OBJECT_CLASS(xfdesktop_icon_view_parent_class)->constructed(object);

    icon_view->priv->native_targets = gtk_target_list_new(icon_view_targets,
                                                          icon_view_n_targets);

    icon_view->priv->source_targets = gtk_target_list_new(icon_view_targets,
                                                          icon_view_n_targets);
    gtk_drag_source_set(GTK_WIDGET(icon_view), 0, NULL, 0, GDK_ACTION_MOVE);

    icon_view->priv->dest_targets = gtk_target_list_new(icon_view_targets,
                                                        icon_view_n_targets);
    gtk_drag_dest_set(GTK_WIDGET(icon_view), 0, NULL, 0, GDK_ACTION_MOVE);

    g_object_bind_property(icon_view, "show-tooltips", icon_view, "has-tooltip", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
    g_signal_connect(G_OBJECT(icon_view), "query-tooltip",
                     G_CALLBACK(xfdesktop_icon_view_show_tooltip), NULL);
    icon_view->priv->tooltip_icon_size_xfconf = xfconf_channel_get_double(icon_view->priv->channel, DESKTOP_ICONS_TOOLTIP_SIZE_PROP, 0);
    g_signal_connect(icon_view->priv->channel, "property-changed::" DESKTOP_ICONS_TOOLTIP_SIZE_PROP,
                     G_CALLBACK(xfdesktop_icon_view_xfconf_tooltip_icon_size_changed), icon_view);

    gtk_widget_set_has_window(GTK_WIDGET(icon_view), FALSE);
    gtk_widget_set_can_focus(GTK_WIDGET(icon_view), TRUE);

    context = gtk_widget_get_style_context(GTK_WIDGET(icon_view));
    gtk_style_context_add_class(context, GTK_STYLE_CLASS_VIEW);

    icon_view->priv->icon_renderer = gtk_cell_renderer_pixbuf_new();
    icon_view->priv->text_renderer = xfdesktop_cell_renderer_icon_label_new();
    xfdesktop_icon_view_init_builtin_cell_renderers(icon_view);

    for (gsize i = 0; i < G_N_ELEMENTS(setting_bindings); ++i) {
        xfconf_g_property_bind(icon_view->priv->channel,
                               setting_bindings[i].setting,
                               setting_bindings[i].setting_type,
                               icon_view,
                               setting_bindings[i].property);
    }
}

static void
xfdesktop_icon_view_dispose(GObject *obj)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(obj);

    if (icon_view->priv->keyboard_navigation_state_timeout != 0) {
        g_source_remove(icon_view->priv->keyboard_navigation_state_timeout);
        icon_view->priv->keyboard_navigation_state_timeout = 0;
    }
    if (icon_view->priv->keyboard_navigation_state != NULL) {
        g_array_free(icon_view->priv->keyboard_navigation_state, TRUE);
        icon_view->priv->keyboard_navigation_state = NULL;
    }

    g_clear_object(&icon_view->priv->channel);
    g_clear_object(&icon_view->priv->model);

    G_OBJECT_CLASS(xfdesktop_icon_view_parent_class)->dispose(obj);
}

static void
xfdesktop_icon_view_finalize(GObject *obj)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(obj);

    gtk_target_list_unref(icon_view->priv->native_targets);
    gtk_target_list_unref(icon_view->priv->source_targets);
    gtk_target_list_unref(icon_view->priv->dest_targets);

    G_OBJECT_CLASS(xfdesktop_icon_view_parent_class)->finalize(obj);
}

static void
xfdesktop_icon_view_set_property(GObject *object,
                                 guint property_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(object);

    switch(property_id) {
        case PROP_CHANNEL:
            icon_view->priv->channel = g_value_dup_object(value);
            break;

        case PROP_MODEL:
            xfdesktop_icon_view_set_model(icon_view, g_value_get_object(value));
            break;

        case PROP_PIXBUF_COLUMN:
            xfdesktop_icon_view_set_pixbuf_column(icon_view, g_value_get_int(value));
            break;

        case PROP_TEXT_COLUMN:
            xfdesktop_icon_view_set_text_column(icon_view, g_value_get_int(value));
            break;

        case PROP_SEARCH_COLUMN:
            xfdesktop_icon_view_set_search_column(icon_view, g_value_get_int(value));
            break;

        case PROP_SORT_PRIORITY_COLUMN:
            xfdesktop_icon_view_set_sort_priority_column(icon_view, g_value_get_int(value));
            break;

        case PROP_TOOLTIP_ICON_COLUMN:
            xfdesktop_icon_view_set_tooltip_icon_column(icon_view, g_value_get_int(value));
            break;

        case PROP_TOOLTIP_TEXT_COLUMN:
            xfdesktop_icon_view_set_tooltip_text_column(icon_view, g_value_get_int(value));
            break;

        case PROP_ROW_COLUMN:
            xfdesktop_icon_view_set_row_column(icon_view, g_value_get_int(value));
            break;

        case PROP_COL_COLUMN:
            xfdesktop_icon_view_set_col_column(icon_view, g_value_get_int(value));
            break;

        case PROP_ICON_SIZE:
            xfdesktop_icon_view_set_icon_size(icon_view, g_value_get_int(value));
            break;

        case PROP_ICON_FONT_SIZE:
            xfdesktop_icon_view_set_font_size(icon_view, g_value_get_double(value));
            break;

        case PROP_ICON_FONT_SIZE_SET:
            xfdesktop_icon_view_set_use_font_size(icon_view, g_value_get_boolean(value));
            break;

        case PROP_ICON_CENTER_TEXT:
            xfdesktop_icon_view_set_center_text(icon_view, g_value_get_boolean(value));
            break;

        case PROP_SINGLE_CLICK:
            xfdesktop_icon_view_set_single_click(icon_view, g_value_get_boolean(value));
            break;

        case PROP_SINGLE_CLICK_UNDERLINE_HOVER:
            xfdesktop_icon_view_set_single_click_underline_hover(icon_view, g_value_get_boolean(value));
            break;

        case PROP_GRAVITY:
            xfdesktop_icon_view_set_gravity(icon_view, g_value_get_int(value));
            break;

        case PROP_SHOW_TOOLTIPS:
            xfdesktop_icon_view_set_show_tooltips(icon_view, g_value_get_boolean(value));
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
xfdesktop_icon_view_get_property(GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(object);

    switch(property_id) {
        case PROP_CHANNEL:
            g_value_set_object(value, icon_view->priv->channel);
            break;

        case PROP_MODEL:
            g_value_set_object(value, icon_view->priv->model);
            break;

        case PROP_PIXBUF_COLUMN:
            g_value_set_int(value, icon_view->priv->pixbuf_column);
            break;

        case PROP_TEXT_COLUMN:
            g_value_set_int(value, icon_view->priv->text_column);
            break;

        case PROP_SEARCH_COLUMN:
            g_value_set_int(value, icon_view->priv->search_column);
            break;

        case PROP_SORT_PRIORITY_COLUMN:
            g_value_set_int(value, icon_view->priv->sort_priority_column);
            break;

        case PROP_TOOLTIP_ICON_COLUMN:
            g_value_set_int(value, icon_view->priv->tooltip_icon_column);
            break;

        case PROP_TOOLTIP_TEXT_COLUMN:
            g_value_set_int(value, icon_view->priv->tooltip_text_column);
            break;

        case PROP_ROW_COLUMN:
            g_value_set_int(value, icon_view->priv->row_column);
            break;

        case PROP_COL_COLUMN:
            g_value_set_int(value, icon_view->priv->col_column);
            break;

        case PROP_ICON_SIZE:
            g_value_set_int(value, icon_view->priv->icon_size);
            break;

        case PROP_ICON_WIDTH:
            g_value_set_int(value, ICON_WIDTH);
            break;

        case PROP_ICON_HEIGHT:
            g_value_set_int(value, ICON_SIZE);
            break;

        case PROP_ICON_FONT_SIZE:
            g_value_set_double(value, icon_view->priv->font_size);
            break;

        case PROP_ICON_FONT_SIZE_SET:
            g_value_set_boolean(value, icon_view->priv->font_size_set);
            break;

        case PROP_ICON_CENTER_TEXT:
            g_value_set_boolean(value, icon_view->priv->center_text);
            break;

        case PROP_SINGLE_CLICK:
            g_value_set_boolean(value, icon_view->priv->single_click);
            break;

        case PROP_SINGLE_CLICK_UNDERLINE_HOVER:
            g_value_set_boolean(value, icon_view->priv->single_click_underline_hover);
            break;

        case PROP_GRAVITY:
            g_value_set_int(value, icon_view->priv->gravity);
            break;

        case PROP_SHOW_TOOLTIPS:
            g_value_set_boolean(value, icon_view->priv->show_tooltips);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
xfdesktop_icon_view_invalidate_all(XfdesktopIconView *icon_view,
                                   gboolean recalc_extents)
{
    for (GList *l = icon_view->priv->items; l != NULL; l = l->next) {
        xfdesktop_icon_view_invalidate_item(icon_view, l->data, recalc_extents);
    }
}

static void
xfdesktop_icon_view_invalidate_pixbuf_cache(XfdesktopIconView *icon_view)
{
    for (GList *l = icon_view->priv->items; l != NULL; l = l->next) {
        ViewItem *item = (ViewItem *)l->data;

        if (item->pixbuf_surface != NULL) {
            cairo_surface_destroy(item->pixbuf_surface);
            item->pixbuf_surface = NULL;
        }
    }
}

static void
xfdesktop_icon_view_add_move_binding(GtkBindingSet *binding_set,
                                     guint keyval,
                                     guint modmask,
                                     GtkMovementStep step,
                                     gint count)
{
    gtk_binding_entry_add_signal(binding_set, keyval, modmask,
                                 I_("move-cursor"), 2,
                                 G_TYPE_ENUM, step,
                                 G_TYPE_INT, count);

    gtk_binding_entry_add_signal(binding_set, keyval, GDK_SHIFT_MASK,
                                 I_("move-cursor"), 2,
                                 G_TYPE_ENUM, step,
                                 G_TYPE_INT, count);

    if(modmask & GDK_CONTROL_MASK)
        return;

    gtk_binding_entry_add_signal(binding_set, keyval,
                                 GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                 I_("move-cursor"), 2,
                                 G_TYPE_ENUM, step,
                                 G_TYPE_INT, count);

    gtk_binding_entry_add_signal(binding_set, keyval, GDK_CONTROL_MASK,
                                 I_("move-cursor"), 2,
                                 G_TYPE_ENUM, step,
                                 G_TYPE_INT, count);
}

static void
xfdesktop_icon_view_clear_drag_event(XfdesktopIconView *icon_view,
                                     GtkWidget *widget)
{
    DBG("unsetting stuff");
    icon_view->priv->control_click = FALSE;
    icon_view->priv->double_click = FALSE;
    icon_view->priv->maybe_begin_drag = FALSE;
    icon_view->priv->definitely_dragging = FALSE;
    xfdesktop_icon_view_clear_drag_highlight(icon_view);

    if (icon_view->priv->definitely_rubber_banding) {
        gint dx = 0, dy = 0;

        gtk_widget_translate_coordinates(widget, GTK_WIDGET(icon_view), 0, 0, &dx, &dy);

        icon_view->priv->definitely_rubber_banding = FALSE;
        gtk_widget_queue_draw_area(widget,
                                   icon_view->priv->band_rect.x - dx,
                                   icon_view->priv->band_rect.y - dy,
                                   icon_view->priv->band_rect.width,
                                   icon_view->priv->band_rect.height);
    }
}

static gboolean
xfdesktop_icon_view_button_press(GtkWidget *widget,
                                 GdkEventButton *evt,
                                 gpointer user_data)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(user_data);
    gint x, y;

    DBG("entering");

    if (!gtk_widget_translate_coordinates(widget, GTK_WIDGET(icon_view), evt->x, evt->y, &x, &y) || x < 0 || y < 0) {
        return FALSE;
    }

    if(evt->type == GDK_BUTTON_PRESS) {
        GList *item_l;
        GdkEventButton evt_copy;

        /* Clear drag event if ongoing */
        if(evt->button == 2 || evt->button == 3)
            xfdesktop_icon_view_clear_drag_event(icon_view, widget);

        /* Let xfce-desktop handle button 2 */
        if(evt->button == 2) {
            /* If we had the grab release it so the desktop gets the event */
            if(gtk_widget_has_grab(widget))
                gtk_grab_remove(widget);

            return FALSE;
        }

        /* Grab the focus, release on button release */
        if(!gtk_widget_has_grab(widget))
            gtk_grab_add(widget);

        memcpy(&evt_copy, evt, sizeof(evt_copy));
        evt_copy.x = x;
        evt_copy.y = y;
        item_l = g_list_find_custom(icon_view->priv->items, &evt_copy,
                                    (GCompareFunc)xfdesktop_check_icon_clicked);
        if (item_l != NULL) {
            ViewItem *item = item_l->data;

            if (item->selected) {
                /* clicked an already-selected icon */

                if(evt->state & GDK_CONTROL_MASK)
                    icon_view->priv->control_click = TRUE;

                icon_view->priv->cursor = item;
            } else {
                /* clicked a non-selected icon */
                if(icon_view->priv->sel_mode != GTK_SELECTION_MULTIPLE
                   || !(evt->state & GDK_CONTROL_MASK))
                {
                    /* unselect all of the other icons if we haven't held
                     * down the ctrl key.  we'll handle shift in the next block,
                     * but for shift we do need to unselect everything */
                    xfdesktop_icon_view_unselect_all(icon_view);

                    if(!(evt->state & GDK_SHIFT_MASK))
                        icon_view->priv->first_clicked_item = NULL;
                }

                icon_view->priv->cursor = item;

                if (icon_view->priv->first_clicked_item == NULL) {
                    icon_view->priv->first_clicked_item = item;
                }

                if(icon_view->priv->sel_mode == GTK_SELECTION_MULTIPLE
                   && evt->state & GDK_SHIFT_MASK
                   && icon_view->priv->first_clicked_item
                   && icon_view->priv->first_clicked_item != item)
                {
                    xfdesktop_icon_view_select_between(icon_view,
                                                       icon_view->priv->first_clicked_item,
                                                       item);
                } else {
                    xfdesktop_icon_view_select_item_internal(icon_view, item, TRUE);
                }
            }

            if(evt->button == 1 || evt->button == 3) {
                /* we might be the start of a drag */
                DBG("setting stuff");
                icon_view->priv->maybe_begin_drag = TRUE;
                icon_view->priv->definitely_dragging = FALSE;
                icon_view->priv->definitely_rubber_banding = FALSE;
                icon_view->priv->press_start_x = x;
                icon_view->priv->press_start_y = y;
            }

            return TRUE;
        } else {
            /* Button press wasn't over any icons */
            /* unselect previously selected icons if we didn't click one */
            if(icon_view->priv->sel_mode != GTK_SELECTION_MULTIPLE
               || !(evt->state & GDK_CONTROL_MASK))
            {
                xfdesktop_icon_view_unselect_all(icon_view);
            }

            icon_view->priv->cursor = NULL;
            icon_view->priv->first_clicked_item = NULL;

            if(icon_view->priv->allow_rubber_banding && evt->button == 1
               && !(evt->state & GDK_SHIFT_MASK))
            {
                icon_view->priv->maybe_begin_drag = TRUE;
                icon_view->priv->definitely_dragging = FALSE;
                icon_view->priv->press_start_x = x;
                icon_view->priv->press_start_y = y;
            }

            /* Since we're not over any icons this won't be the start of a
             * drag so we can pop up menu */
            if(evt->button == 3 || (evt->button == 1 && (evt->state & GDK_SHIFT_MASK))) {
                // Allow the parent (XfceDesktop) to handle it
                return FALSE;
            }
        }
    } else if(evt->type == GDK_2BUTTON_PRESS) {
        /* filter this event in single click mode */
        if(xfdesktop_icon_view_get_single_click(icon_view)) {
            icon_view->priv->double_click = TRUE;
            xfdesktop_icon_view_unselect_all(icon_view);
            return TRUE;
        }

        /* be sure to cancel any pending drags that might have snuck through.
         * this shouldn't happen, but it does sometimes (bug 3426).  */
        icon_view->priv->maybe_begin_drag = FALSE;
        icon_view->priv->definitely_dragging = FALSE;
        icon_view->priv->definitely_rubber_banding = FALSE;

        if(evt->button == 1) {
            GdkEventButton evt_copy;
            GList *icon_l;

            memcpy(&evt_copy, evt, sizeof(evt_copy));
            evt_copy.x = x;
            evt_copy.y = y;
            icon_l = g_list_find_custom(icon_view->priv->items, &evt_copy,
                                        (GCompareFunc)xfdesktop_check_icon_clicked);
            if (icon_l != NULL) {
                ViewItem *item = icon_l->data;
                icon_view->priv->cursor = item;
                g_signal_emit(G_OBJECT(icon_view), __signals[SIG_ICON_ACTIVATED], 0);
                xfdesktop_icon_view_unselect_all(icon_view);
            }
        }

        return TRUE;
    }

    return FALSE;
}

static gboolean
xfdesktop_icon_view_button_release(GtkWidget *widget,
                                   GdkEventButton *evt,
                                   gpointer user_data)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(user_data);
    gint x, y;
    GdkEventButton evt_copy;
    GList *icon_l = NULL;

    DBG("entering btn=%d", evt->button);

    if (!gtk_widget_translate_coordinates(widget, GTK_WIDGET(icon_view), evt->x, evt->y, &x, &y) || x < 0 || y < 0) {
        return FALSE;
    }

    memcpy(&evt_copy, evt, sizeof(evt_copy));
    evt_copy.x = x;
    evt_copy.y = y;

    /* single-click */
    if(xfdesktop_icon_view_get_single_click(icon_view)
       && evt->button == 1
       && !(evt->state & GDK_SHIFT_MASK)
       && !(evt->state & GDK_CONTROL_MASK)
       && !icon_view->priv->definitely_dragging
       && !icon_view->priv->definitely_rubber_banding
       && !icon_view->priv->double_click)
    {
        /* Find out if we clicked on an icon */
        icon_l = g_list_find_custom(icon_view->priv->items, &evt_copy,
                                    (GCompareFunc)xfdesktop_check_icon_clicked);
        if (icon_l != NULL) {
            ViewItem *item = icon_l->data;
            /* We did, activate it */
            icon_view->priv->cursor = item;
            g_signal_emit(G_OBJECT(icon_view), __signals[SIG_ICON_ACTIVATED], 0);
            xfdesktop_icon_view_unselect_all(icon_view);
        }
    }

    if((evt->button == 3 || (evt->button == 1 && (evt->state & GDK_SHIFT_MASK))) &&
       icon_view->priv->definitely_dragging == FALSE &&
       icon_view->priv->definitely_rubber_banding == FALSE &&
       icon_view->priv->maybe_begin_drag == TRUE)
    {
        /* If we're in single click mode we may already have the icon, don't
         * find it again. */
        if(icon_l == NULL) {
            icon_l = g_list_find_custom(icon_view->priv->items, &evt_copy,
                                        (GCompareFunc)xfdesktop_check_icon_clicked);
        }

        /* If we clicked an icon then we didn't pop up the menu during the
         * button press in order to support right click DND, pop up the menu
         * now. */
        if(icon_l && icon_l->data) {
            gboolean dummy;
            g_signal_emit_by_name(widget, "popup-menu", &dummy);
        }
    }

    if(evt->button == 1 && evt->state & GDK_CONTROL_MASK
       && icon_view->priv->control_click)
    {
        icon_l = g_list_find_custom(icon_view->priv->items, &evt_copy,
                                    (GCompareFunc)xfdesktop_check_icon_clicked);
        if (icon_l != NULL) {
            ViewItem *item = icon_l->data;
            if (item->selected) {
                /* clicked an already-selected icon; unselect it */
                xfdesktop_icon_view_unselect_item_internal(icon_view, item, TRUE);
            }
        }
    }

    if(evt->button == 1 || evt->button == 3 || evt->button == 0) {
        xfdesktop_icon_view_clear_drag_event(icon_view, widget);
    }

    gtk_grab_remove(widget);

    /* TRUE: stop other handlers from being invoked for the event. FALSE: propagate the event further. */
    /* On FALSE this method will be called twice in single-click-mode (possibly a gtk3 bug)            */
    return TRUE;
}

static gboolean
clear_keyboard_navigation_state(gpointer data)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(data);

    if (icon_view->priv->keyboard_navigation_state != NULL) {
        g_array_free(icon_view->priv->keyboard_navigation_state, TRUE);
        icon_view->priv->keyboard_navigation_state = NULL;
    }

    icon_view->priv->keyboard_navigation_state_timeout = 0;

    return G_SOURCE_REMOVE;
}

static gboolean
xfdesktop_icon_view_keyboard_navigate(XfdesktopIconView *icon_view,
                                      gunichar lower_char)
{
    gboolean found_match = FALSE;

    if (icon_view->priv->model == NULL || icon_view->priv->search_column == -1) {
        return FALSE;
    }

    if (icon_view->priv->keyboard_navigation_state == NULL) {
        icon_view->priv->keyboard_navigation_state = g_array_sized_new(TRUE, TRUE, sizeof(gunichar), 16);
        xfdesktop_icon_view_unselect_all(icon_view);
    }

    if (icon_view->priv->keyboard_navigation_state_timeout != 0) {
        g_source_remove(icon_view->priv->keyboard_navigation_state_timeout);
    }
    icon_view->priv->keyboard_navigation_state_timeout = g_timeout_add(KEYBOARD_NAVIGATION_TIMEOUT,
                                                                       clear_keyboard_navigation_state,
                                                                       icon_view);

    g_array_append_val(icon_view->priv->keyboard_navigation_state, lower_char);

    for (GList *l = icon_view->priv->items; l != NULL && !found_match; l = l->next) {
        ViewItem *item = l->data;
        GtkTreeIter iter;
        gchar *label = NULL;

        if (view_item_get_iter(item, icon_view->priv->model, &iter)) {
            gtk_tree_model_get(icon_view->priv->model, &iter,
                               icon_view->priv->search_column, &label,
                               -1);
        }

        if (label != NULL && g_utf8_validate(label, -1, NULL)) {
            gchar *p = label;
            gboolean matches = TRUE;

            for (guint i = 0; i < icon_view->priv->keyboard_navigation_state->len; ++i) {
                gunichar label_char;

                if (*p == '\0') {
                    matches = FALSE;
                    break;
                }

                label_char = g_unichar_tolower(g_utf8_get_char(p));
                if (label_char != g_array_index(icon_view->priv->keyboard_navigation_state, gunichar, i)) {
                    matches = FALSE;
                    break;
                }

                p = g_utf8_find_next_char(p, NULL);
            }

            if (matches) {
                xfdesktop_icon_view_unselect_all(icon_view);
                icon_view->priv->cursor = item;
                xfdesktop_icon_view_select_item_internal(icon_view, item, TRUE);
                found_match = TRUE;
            }
        }

        g_free(label);
    }

    return found_match;
}

static gboolean
xfdesktop_icon_view_key_press(GtkWidget *widget,
                              GdkEventKey *evt,
                              gpointer user_data)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(user_data);
    gboolean ret = FALSE;
    gboolean could_keyboard_navigate = FALSE;

    DBG("entering");

    /* since we're NO_WINDOW, events don't get delivered to us normally,
     * so we have to activate the bindings manually */
    ret = gtk_bindings_activate_event(G_OBJECT(icon_view), evt);
    if(ret == FALSE) {
        GdkModifierType ignore_modifiers = gtk_accelerator_get_default_mod_mask();
        if((evt->state & ignore_modifiers) == 0) {
            /* Binding not found and key press is not part of a combo.
             * Now inspect the pressed character. Let's try to find an
             * icon starting with this character and make the icon selected. */
            guint32 unicode = gdk_keyval_to_unicode(evt->keyval);
            if (unicode != 0 && g_unichar_isgraph(unicode)) {
                could_keyboard_navigate = TRUE;
                xfdesktop_icon_view_keyboard_navigate(icon_view, g_unichar_tolower(unicode));
            }
        }
    }

    if (!could_keyboard_navigate) {
        if (icon_view->priv->keyboard_navigation_state != NULL) {
            g_array_free(icon_view->priv->keyboard_navigation_state, TRUE);
            icon_view->priv->keyboard_navigation_state = NULL;
        }
    }

    return ret;
}

static gboolean
xfdesktop_icon_view_focus_in(GtkWidget *widget,
                             GdkEventFocus *evt,
                             gpointer user_data)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(user_data);

    gtk_widget_grab_focus(GTK_WIDGET(icon_view));
    DBG("GOT FOCUS");

    for (GList *l = icon_view->priv->selected_items; l != NULL; l = l->next) {
        xfdesktop_icon_view_invalidate_item(icon_view, (ViewItem *)l->data, FALSE);
    }

    return FALSE;
}

static gboolean
xfdesktop_icon_view_focus_out(GtkWidget *widget,
                              GdkEventFocus *evt,
                              gpointer user_data)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(user_data);

    DBG("LOST FOCUS");

    for (GList *l = icon_view->priv->selected_items; l != NULL; l = l->next) {
        xfdesktop_icon_view_invalidate_item(icon_view, (ViewItem *)l->data, FALSE);
    }

    if(G_UNLIKELY(icon_view->priv->single_click)) {
        if(G_LIKELY(gtk_widget_get_window(GTK_WIDGET(icon_view->priv->parent_window)) != NULL)) {
            gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(icon_view->priv->parent_window)), NULL);
        }
    }

    return FALSE;
}

static gboolean
xfdesktop_icon_view_maybe_begin_drag(XfdesktopIconView *icon_view,
                                     GdkEventMotion *evt,
                                     gint x,
                                     gint y)
{
    GdkDragAction actions;

    /* sanity check */
    g_return_val_if_fail(icon_view->priv->cursor, FALSE);

    if(!gtk_drag_check_threshold(GTK_WIDGET(icon_view),
                                 icon_view->priv->press_start_x,
                                 icon_view->priv->press_start_y,
                                 x, y))
    {
        return FALSE;
    }

    actions = GDK_ACTION_MOVE | (icon_view->priv->drag_source_set ?
                                 icon_view->priv->foreign_source_actions : 0);

    if(!(evt->state & GDK_BUTTON3_MASK)) {
        gtk_drag_begin_with_coordinates(GTK_WIDGET(icon_view),
                                        icon_view->priv->source_targets,
                                        actions,
                                        1,
                                        (GdkEvent *)evt,
                                        x,
                                        y);
    } else {
        gtk_drag_begin_with_coordinates(GTK_WIDGET(icon_view),
                                        icon_view->priv->source_targets,
                                        actions | GDK_ACTION_ASK,
                                        3,
                                        (GdkEvent *)evt,
                                        x,
                                        y);
    }

    return TRUE;
}

static gboolean
xfdesktop_icon_view_show_tooltip(GtkWidget *widget,
                                 gint x,
                                 gint y,
                                 gboolean keyboard_tooltip,
                                 GtkTooltip *tooltip,
                                 gpointer user_data)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);
    GtkTreeIter iter;
    gboolean result = FALSE;

    if (icon_view->priv->model == NULL
        || icon_view->priv->item_under_pointer == NULL
        || icon_view->priv->definitely_dragging
        || !icon_view->priv->show_tooltips)
    {
        return FALSE;
    }

    if (!view_item_get_iter(icon_view->priv->item_under_pointer, icon_view->priv->model, &iter)) {
        return FALSE;
    }

    g_signal_emit(icon_view, __signals[SIG_QUERY_ICON_TOOLTIP], 0,
                  &iter, x, y, keyboard_tooltip, tooltip,
                  &result);

    if (!result && (icon_view->priv->tooltip_icon_column != -1 || icon_view->priv->tooltip_text_column != -1)) {
        GIcon *icon = NULL;
        gchar *tip_text = NULL;

        if (icon_view->priv->tooltip_icon_column != -1) {
            gtk_tree_model_get(icon_view->priv->model, &iter,
                               icon_view->priv->tooltip_icon_column, &icon,
                               -1);
        }

        if (icon_view->priv->tooltip_text_column != -1) {
            gtk_tree_model_get(icon_view->priv->model, &iter,
                               icon_view->priv->tooltip_text_column, &tip_text,
                               -1);
        }

        if (icon != NULL || tip_text != NULL) {
            GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

            if (icon != NULL) {
                GtkWidget *img = gtk_image_new_from_gicon(icon, GTK_ICON_SIZE_DIALOG);
                gtk_image_set_pixel_size(GTK_IMAGE(img), xfdesktop_icon_view_get_tooltip_icon_size(icon_view));
                g_object_unref(icon);
                gtk_box_pack_start(GTK_BOX(box), img, FALSE, FALSE, 0);
            }

            if (tip_text != NULL) {
                gchar *padded_tip_text = g_strdup_printf("%s\t", tip_text);
                GtkWidget *label = gtk_label_new(padded_tip_text);

                gtk_label_set_xalign(GTK_LABEL(label), 0.0);
                gtk_label_set_yalign(GTK_LABEL(label), 0.5);
                gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);

                g_free(padded_tip_text);
            }

            gtk_widget_show_all(box);
            gtk_tooltip_set_custom(tooltip, box);

            result = TRUE;
        }
    }

    return result;
}

static void
xfdesktop_icon_view_xfconf_tooltip_icon_size_changed(XfconfChannel *channel,
                                                     const gchar *property,
                                                     const GValue *value,
                                                     XfdesktopIconView *icon_view)
{
    icon_view->priv->tooltip_icon_size_xfconf = g_value_get_double(value);
}

static gboolean
xfdesktop_icon_view_motion_notify(GtkWidget *widget,
                                  GdkEventMotion *evt,
                                  gpointer user_data)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(user_data);
    gint x, y;
    gboolean ret = FALSE;

    if (!gtk_widget_translate_coordinates(widget, GTK_WIDGET(icon_view), evt->x, evt->y, &x, &y) || x < 0 || y < 0) {
        return FALSE;
    }

    if(icon_view->priv->maybe_begin_drag
       && icon_view->priv->item_under_pointer
       && !icon_view->priv->definitely_dragging)
    {
        /* we might have the start of an icon click + drag here */
        icon_view->priv->definitely_dragging = xfdesktop_icon_view_maybe_begin_drag(icon_view,
                                                                                    evt, x, y);
        if(icon_view->priv->definitely_dragging) {
            icon_view->priv->maybe_begin_drag = FALSE;
            ret = TRUE;
        }
    } else if(icon_view->priv->maybe_begin_drag
              && ((!icon_view->priv->item_under_pointer
                   && !icon_view->priv->definitely_rubber_banding)
                  || icon_view->priv->definitely_rubber_banding))
    {
        GdkRectangle old_rect, *new_rect, intersect;
        cairo_region_t *region;
        gint dx = 0, dy = 0;

        /* we're dragging with no icon under the cursor -> rubber band start
         * OR, we're already doin' the band -> update it */

        new_rect = &icon_view->priv->band_rect;

        if(!icon_view->priv->definitely_rubber_banding) {
            icon_view->priv->definitely_rubber_banding = TRUE;
            old_rect.x = icon_view->priv->press_start_x;
            old_rect.y = icon_view->priv->press_start_y;
            old_rect.width = old_rect.height = 1;
        } else
            memcpy(&old_rect, new_rect, sizeof(old_rect));

        new_rect->x = MIN(icon_view->priv->press_start_x, x);
        new_rect->y = MIN(icon_view->priv->press_start_y, y);
        new_rect->width = ABS(x - icon_view->priv->press_start_x) + 1;
        new_rect->height = ABS(y - icon_view->priv->press_start_y) + 1;

        region = cairo_region_create_rectangle(&old_rect);
        cairo_region_union_rectangle(region, new_rect);

        if(gdk_rectangle_intersect(&old_rect, new_rect, &intersect)
           && intersect.width > 2 && intersect.height > 2)
        {
            cairo_region_t *region_intersect;

            /* invalidate border too */
            intersect.x += 1;
            intersect.width -= 2;
            intersect.y += 1;
            intersect.height -= 2;

            region_intersect = cairo_region_create_rectangle(&intersect);
            cairo_region_subtract(region, region_intersect);
            cairo_region_destroy(region_intersect);
        }

        gtk_widget_translate_coordinates(widget, GTK_WIDGET(icon_view), 0, 0, &dx, &dy);
        cairo_region_translate(region, -dx, -dy);
        gdk_window_invalidate_region(gtk_widget_get_window(widget), region, TRUE);
        cairo_region_destroy(region);

        /* update list of selected icons */

        /* first pass: if the rubber band area got smaller at least in
         * one dimension, we can try first to just remove icons that
         * aren't in the band anymore */
        if(old_rect.width > new_rect->width
           || old_rect.height > new_rect->height)
        {
            GList *l = icon_view->priv->selected_items;
            while(l) {
                GdkRectangle dummy;
                ViewItem *item = l->data;

                /* To be removed, it must intersect the old rectangle and
                 * not intersect the new one. This way CTRL + rubber band
                 * works properly (Bug 10275) */
                if (gdk_rectangle_intersect(&item->slot_extents, &old_rect, NULL)
                    && !gdk_rectangle_intersect(&item->slot_extents, new_rect, &dummy))
                {
                    /* remove the icon from the selected list */
                    l = l->next;
                    xfdesktop_icon_view_unselect_item_internal(icon_view, item, TRUE);
                } else {
                    l = l->next;
                }
            }
        }

        /* second pass: if at least one dimension got larger, unfortunately
         * we have to figure out what icons to add to the selected list */
        if(old_rect.width < new_rect->width
           || old_rect.height < new_rect->height)
        {
            for (GList *l = icon_view->priv->items; l != NULL; l = l->next) {
                GdkRectangle dummy;
                ViewItem *item = l->data;

                if (!item->selected && gdk_rectangle_intersect(&item->slot_extents, new_rect, &dummy)) {
                    /* since _select_item() prepends to the list, we
                     * should be ok just calling this */
                    xfdesktop_icon_view_select_item_internal(icon_view, item, TRUE);
                }
            }
        }
    } else {

        /* normal movement; highlight icons as they go under the pointer */

        if(icon_view->priv->item_under_pointer) {
            ViewItem *item = icon_view->priv->item_under_pointer;

            if(G_UNLIKELY(icon_view->priv->single_click)) {
                GdkCursor *cursor = gdk_cursor_new_for_display(gtk_widget_get_display(widget), GDK_HAND2);
                gdk_window_set_cursor(evt->window, cursor);
                g_object_unref(cursor);
            }

            if (item->slot_extents.width < 0
                || item->slot_extents.height < 0
                || !xfdesktop_rectangle_contains_point(&item->slot_extents, x, y))
            {
                icon_view->priv->item_under_pointer = NULL;
                xfdesktop_icon_view_invalidate_item(icon_view, item, FALSE);
            }
        } else {
            ViewItem *item;

            if(G_UNLIKELY(icon_view->priv->single_click)) {
                gdk_window_set_cursor(evt->window, NULL);
            }

            item = xfdesktop_icon_view_widget_coords_to_item_internal(icon_view, x, y);
            if (item != NULL
                && xfdesktop_rectangle_contains_point(&item->slot_extents, x, y))
            {
                icon_view->priv->item_under_pointer = item;
                xfdesktop_icon_view_invalidate_item(icon_view, item, FALSE);
            }
        }
    }

    gdk_event_request_motions(evt);

    return ret;
}

static gboolean
xfdesktop_icon_view_leave_notify(GtkWidget *widget,
                                 GdkEventCrossing *evt,
                                 gpointer user_data)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(user_data);

    if(icon_view->priv->item_under_pointer) {
        ViewItem *item = icon_view->priv->item_under_pointer;
        icon_view->priv->item_under_pointer = NULL;

        xfdesktop_icon_view_invalidate_item(icon_view, item, FALSE);
    }

    if(G_UNLIKELY(icon_view->priv->single_click)) {
        if(gtk_widget_get_realized(widget)) {
            gdk_window_set_cursor(gtk_widget_get_window(widget), NULL);
        }
    }

    return FALSE;
}

static cairo_surface_t *
xfdesktop_icon_view_get_surface_for_item(XfdesktopIconView *icon_view,
                                         ViewItem *item)
{
    cairo_surface_t *surface = NULL;

    g_return_val_if_fail(icon_view->priv->model != NULL, NULL);

    if (item->pixbuf_surface != NULL) {
        surface = cairo_surface_reference(item->pixbuf_surface);
    } else {
        if (icon_view->priv->pixbuf_column != -1) {
            GtkTreeIter iter;

            if (view_item_get_iter(item, icon_view->priv->model, &iter)) {
                GIcon *icon = NULL;

                gtk_tree_model_get(icon_view->priv->model, &iter,
                                   icon_view->priv->pixbuf_column, &icon,
                                   -1);

                if (G_LIKELY(icon != NULL)) {
                    GtkStyleContext *context = gtk_widget_get_style_context(GTK_WIDGET(icon_view));
                    GtkIconTheme *icon_theme = gtk_icon_theme_get_for_screen(gtk_widget_get_screen(GTK_WIDGET(icon_view)));
                    gint scale_factor = gtk_widget_get_scale_factor(GTK_WIDGET(icon_view));
                    GdkPixbuf *pix = NULL;

                    if (G_IS_FILE_ICON(icon) || (G_IS_EMBLEMED_ICON(icon) && G_IS_FILE_ICON(g_emblemed_icon_get_icon(G_EMBLEMED_ICON(icon))))) {
                        // Special case for GFileIcon, which will usually be a thumbnail.  We
                        // allow thumbnails to be wider than the icon size that's set, as long
                        // as the height is no taller than the icon size.
                        GtkIconInfo *icon_info = gtk_icon_theme_lookup_by_gicon_for_scale(icon_theme,
                                                                                          icon,
                                                                                          ICON_WIDTH,
                                                                                          scale_factor,
                                                                                          GTK_ICON_LOOKUP_FORCE_SIZE);
                        if (G_LIKELY(icon_info != NULL)) {
                            pix = gtk_icon_info_load_symbolic_for_context(icon_info, context, NULL, NULL);
                            if (G_LIKELY(pix != NULL)) {
                                gint width = gdk_pixbuf_get_width(pix);
                                gint height = gdk_pixbuf_get_height(pix);

                                if (height > ICON_SIZE * scale_factor) {
                                    if (height < width) {
                                        // The height is less than the width, so it's probably
                                        // worth using the larger icon.
                                        GdkPixbuf *scaled = exo_gdk_pixbuf_scale_down(pix,
                                                                                      TRUE,
                                                                                      ICON_WIDTH * scale_factor,
                                                                                      ICON_SIZE * scale_factor);
                                        g_object_unref(pix);
                                        pix = scaled;
                                    } else {
                                        // The height is of equal size to the width, so we
                                        // should try to load the icon at the correct size.
                                        g_object_unref(pix);
                                        pix = NULL;
                                    }
                                }
                            }
                            g_object_unref(icon_info);
                        }
                    }

                    if (pix == NULL) {
                        // Either it's a regular themed icon, or the icon height ended up being
                        // too large when we tried to allow the icon to be wider.
                        GtkIconInfo *icon_info = gtk_icon_theme_lookup_by_gicon_for_scale(icon_theme,
                                                                                          icon,
                                                                                          ICON_SIZE,
                                                                                          scale_factor,
                                                                                          GTK_ICON_LOOKUP_FORCE_SIZE);
                        if (G_LIKELY(icon_info != NULL)) {
                            pix = gtk_icon_info_load_symbolic_for_context(icon_info, context, NULL, NULL);
                            g_object_unref(icon_info);
                        }
                    }

                    if (G_LIKELY(pix != NULL)) {
                        surface = gdk_cairo_surface_create_from_pixbuf(pix,
                                                                       gtk_widget_get_scale_factor(GTK_WIDGET(icon_view)),
                                                                       gtk_widget_get_window(GTK_WIDGET(icon_view)));
                        g_object_unref(pix);
                    }

                    g_object_unref(icon);
                }
            }

            if (surface != NULL) {
                item->pixbuf_surface = cairo_surface_reference(surface);
            }
        }
    }

    return surface;
}

static void
xfdesktop_icon_view_drag_begin(GtkWidget *widget,
                               GdkDragContext *context)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);
    ViewItem *item = icon_view->priv->cursor;
    cairo_surface_t *surface;

    g_return_if_fail(icon_view->priv->model != NULL);
    g_return_if_fail(item != NULL);

    surface = xfdesktop_icon_view_get_surface_for_item(icon_view, item);
    if (surface != NULL) {
        gtk_drag_set_icon_surface(context, surface);
        cairo_surface_destroy(surface);
    }
}

static inline void
xfdesktop_xy_to_rowcol(XfdesktopIconView *icon_view,
                       gint x,
                       gint y,
                       gint *row,
                       gint *col)
{
    g_return_if_fail(row && col);

    *row = (y - icon_view->priv->ymargin) / (SLOT_SIZE + icon_view->priv->yspacing);
    *col = (x - icon_view->priv->xmargin) / (SLOT_SIZE + icon_view->priv->xspacing);
}

static inline void
xfdesktop_icon_view_clear_drag_highlight(XfdesktopIconView *icon_view)
{
    ViewItem *item = icon_view->priv->drop_dest_item;

    if (item != NULL) {
        /* remove highlight from icon */
        icon_view->priv->drop_dest_item = NULL;
        xfdesktop_icon_view_invalidate_item(icon_view, item, FALSE);
    }
}

static inline void
xfdesktop_icon_view_draw_drag_highlight(XfdesktopIconView *icon_view,
                                        guint16 row,
                                        guint16 col)
{
    ViewItem *item = xfdesktop_icon_view_item_in_slot(icon_view, row, col);

    if (icon_view->priv->drop_dest_item != NULL && icon_view->priv->drop_dest_item != item) {
        xfdesktop_icon_view_invalidate_item(icon_view, icon_view->priv->drop_dest_item, FALSE);
        icon_view->priv->drop_dest_item = NULL;
    }

    if (item != NULL) {
        icon_view->priv->drop_dest_item = item;
        xfdesktop_icon_view_invalidate_item(icon_view, item, FALSE);
    }
}

static gboolean
xfdesktop_icon_view_drag_motion(GtkWidget *widget,
                                GdkDragContext *context,
                                gint x,
                                gint y,
                                guint time_)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);
    GdkAtom target;
    gint hover_row = -1, hover_col = -1;
    ViewItem *item_on_dest = NULL;
    GdkDragAction our_action = 0;
    gboolean is_local_drag;

    target = gtk_drag_dest_find_target(widget, context,
                                       icon_view->priv->native_targets);
    if(target == GDK_NONE) {
        target = gtk_drag_dest_find_target(widget, context,
                                           icon_view->priv->dest_targets);
        if(target == GDK_NONE)
            return FALSE;
    }

    /* can we drop here? */
    xfdesktop_xy_to_rowcol(icon_view, x, y, &hover_row, &hover_col);
    if(hover_row >= icon_view->priv->nrows || hover_col >= icon_view->priv->ncols)
        return FALSE;
    item_on_dest = xfdesktop_icon_view_item_in_slot(icon_view, hover_row,
                                                    hover_col);
    if (item_on_dest != NULL) {
        GtkTreeIter iter;
        if (view_item_get_iter(item_on_dest, icon_view->priv->model, &iter)) {
            GdkDragAction drop_actions = 0;
            GdkDragAction dummy;
            g_signal_emit(icon_view, __signals[SIG_DROP_ACTIONS_GET], 0, &iter, &dummy, &drop_actions);
            if (drop_actions == 0) {
                return FALSE;
            }
        }
    } else if(!xfdesktop_grid_is_free_position(icon_view, hover_row, hover_col))
        return FALSE;

    is_local_drag = (target == gdk_atom_intern("XFDESKTOP_ICON", FALSE));

    /* at this point there are four cases to account for:
     * 1.  local drag, empty space -> MOVE
     * 2.  local drag, icon is there -> depends on icon_on_dest
     * 3.  foreign drag, empty space -> depends on the source
     * 4.  foreign drag, icon is there -> depends on source and icon_on_dest
     */

    if (item_on_dest == NULL) {
        if(is_local_drag)  /* # 1 */
            our_action = GDK_ACTION_MOVE;
        else  /* #3 */
            our_action = gdk_drag_context_get_suggested_action(context);
    } else {
        /* start with all available actions (may be filtered by modifier keys) */
        GdkDragAction allowed_actions = gdk_drag_context_get_actions(context);

        if(is_local_drag) {  /* #2 */
            GtkTreeIter iter;
            gboolean action_ask = FALSE;

            /* check to make sure we aren't just hovering over ourself */
            for(GList *l = icon_view->priv->selected_items; l; l = l->next) {
                ViewItem *sel_item = l->data;
                if (sel_item->row == hover_row && sel_item->col == hover_col) {
                    xfdesktop_icon_view_clear_drag_highlight(icon_view);
                    return FALSE;
                }
            }

            if(allowed_actions & GDK_ACTION_ASK)
                action_ask = TRUE;

            if (view_item_get_iter(icon_view->priv->cursor, icon_view->priv->model, &iter)) {
                GdkDragAction allowed_drag_actions = 0;
                g_signal_emit(icon_view, __signals[SIG_DRAG_ACTIONS_GET], 0, &iter, &allowed_drag_actions);
                allowed_actions &= allowed_drag_actions;
            }

            if (view_item_get_iter(item_on_dest, icon_view->priv->model, &iter)) {
                /* for local drags, let the dest icon decide */
                GdkDragAction allowed_drop_actions = 0;
                g_signal_emit(icon_view, __signals[SIG_DROP_ACTIONS_GET], 0, &iter, &our_action, &allowed_drop_actions);
                allowed_actions &= allowed_drop_actions;
            }

            /* check if drag&drop menu should be triggered */
            if(action_ask) {
                if(allowed_actions == (GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK)) {
                    allowed_actions |= GDK_ACTION_ASK;
                    our_action = GDK_ACTION_ASK;
                }
            }
        } else {  /* #4 */
            GtkTreeIter iter;

            if (view_item_get_iter(item_on_dest, icon_view->priv->model, &iter)) {
                GdkDragAction allowed_drop_actions = 0;
                GdkDragAction dummy;
                g_signal_emit(icon_view, __signals[SIG_DROP_ACTIONS_GET], 0, &iter, &dummy, &allowed_drop_actions);
                allowed_actions &= allowed_drop_actions;
            }

            /* for foreign drags, take the action suggested by the source */
            our_action = gdk_drag_context_get_suggested_action(context);
        }

        /* #2 or #4 */

        /* fallback actions if the suggested action is not allowed,
         * priority: move, copy, link */
        if(!(our_action & allowed_actions)) {
            if(allowed_actions & GDK_ACTION_MOVE)
                our_action = GDK_ACTION_MOVE;
            else if(allowed_actions & GDK_ACTION_COPY)
                our_action = GDK_ACTION_COPY;
            else if(allowed_actions & GDK_ACTION_LINK)
                our_action = GDK_ACTION_LINK;
            else
                our_action = 0;
        }
    }

    /* allow the drag dest to override the selected action based on the drag data */
    icon_view->priv->hover_row = hover_row;
    icon_view->priv->hover_col = hover_col;
    icon_view->priv->proposed_drop_action = our_action;
    icon_view->priv->dropped = FALSE;
    g_object_set_data(G_OBJECT(context), "--xfdesktop-icon-view-drop-icon",
                      item_on_dest);
    gtk_drag_get_data(widget, context, target, time_);

    /* the actual call to gdk_drag_status() is deferred to
     * xfdesktop_icon_view_drag_data_received() */

    return TRUE;
}

static void
xfdesktop_icon_view_drag_leave(GtkWidget *widget,
                               GdkDragContext *context,
                               guint time_)
{
    xfdesktop_icon_view_clear_drag_highlight(XFDESKTOP_ICON_VIEW(widget));
}

static void
xfdesktop_next_slot(XfdesktopIconView *icon_view,
                    gint *col,
                    gint *row,
                    gint ncols,
                    gint nrows)
{
    gint scol = *col, srow = *row;

    if(icon_view->priv->gravity & XFDESKTOP_ICON_VIEW_GRAVITY_HORIZONTAL) {
        scol += (icon_view->priv->gravity & XFDESKTOP_ICON_VIEW_GRAVITY_RIGHT) ? -1 : 1;
        if(scol < 0) {
            scol = ncols - 1;
            srow += (icon_view->priv->gravity & XFDESKTOP_ICON_VIEW_GRAVITY_BOTTOM) ? -1 : 1;
        } else {
            if(scol >= ncols) {
                scol = 0;
                srow += (icon_view->priv->gravity & XFDESKTOP_ICON_VIEW_GRAVITY_BOTTOM) ? -1 : 1;
            }
        }
    } else {
        srow += (icon_view->priv->gravity & XFDESKTOP_ICON_VIEW_GRAVITY_BOTTOM) ? -1 : 1;
        if(srow < 0) {
            srow = nrows - 1;
            scol += (icon_view->priv->gravity & XFDESKTOP_ICON_VIEW_GRAVITY_RIGHT) ? -1 : 1;
        } else {
            if(srow >= nrows) {
                srow = 0;
                scol += (icon_view->priv->gravity & XFDESKTOP_ICON_VIEW_GRAVITY_RIGHT) ? -1 : 1;
            }
        }
    }

    *col = scol;
    *row = srow;
}

static gboolean
xfdesktop_icon_view_drag_drop(GtkWidget *widget,
                              GdkDragContext *context,
                              gint x,
                              gint y,
                              guint time_)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);
    GdkAtom target;
    gint row, col, offset_col, offset_row;
    ViewItem *item_on_dest = NULL;

    DBG("entering: (%d,%d)", x, y);

    DBG("unsetting stuff");
    icon_view->priv->control_click = FALSE;
    icon_view->priv->double_click = FALSE;
    icon_view->priv->maybe_begin_drag = FALSE;
    icon_view->priv->definitely_dragging = FALSE;
    icon_view->priv->dropped = TRUE;

    target = gtk_drag_dest_find_target(widget, context,
                                       icon_view->priv->native_targets);
    if(target == GDK_NONE) {
        target = gtk_drag_dest_find_target(widget, context,
                                           icon_view->priv->dest_targets);
        if(target == GDK_NONE)
            return FALSE;
    }
    XF_DEBUG("target=%ld (%s)", (glong)target, gdk_atom_name(target));

    xfdesktop_xy_to_rowcol(icon_view, x, y, &row, &col);
    item_on_dest = xfdesktop_icon_view_item_in_slot(icon_view, row, col);

    if(target == gdk_atom_intern("XFDESKTOP_ICON", FALSE)) {
        GList *selected_items = NULL;

        if (item_on_dest != NULL) {
            gboolean ret = FALSE;
            GtkTreeIter iter;

            if (view_item_get_iter(item_on_dest, icon_view->priv->model, &iter)) {
                GdkDragAction action = gdk_drag_context_get_selected_action(context);

                if (action == GDK_ACTION_ASK) {
                    g_signal_emit(icon_view, __signals[SIG_DRAG_DROP_ASK], 0,
                                  context, &iter, row, col, time_, &action);
                }

                if (action == 0 || action == GDK_ACTION_ASK) {
                    ret = FALSE;
                } else {
                    GList *dropped_paths = NULL;

                    for (GList *l = icon_view->priv->selected_items; l != NULL; l = l->next) {
                        GtkTreePath *path = view_item_get_path((ViewItem *)l->data, icon_view->priv->model);
                        if (path != NULL) {
                            dropped_paths = g_list_prepend(dropped_paths, path);
                        }
                    }
                    dropped_paths = g_list_reverse(dropped_paths);

                    g_signal_emit(icon_view, __signals[SIG_DRAG_DROP_ITEMS], 0,
                                  context, &iter, dropped_paths, action, &ret);

                    g_list_free_full(dropped_paths, (GDestroyNotify)gtk_tree_path_free);
                }
           }

            gtk_drag_finish(context, ret, FALSE, time_);

            return ret;
        }

        g_return_val_if_fail(icon_view->priv->cursor != NULL, FALSE);

        /* 1: Get amount of offset between the old slot and new slot
         *    of the icon that's being dragged.
         * 2: Remove all the icons that are going to be moved from
         *    the desktop. That's in case the icons being moved
         *    want to rearrange themselves there.
         * 3: We move all the icons using the offset. */
        if (icon_view->priv->cursor->placed) {
            offset_col = icon_view->priv->cursor->col - col;
            offset_row = icon_view->priv->cursor->row - row;
        } else {
            offset_col = 0;
            offset_row = 0;
        }

        selected_items = g_list_copy(icon_view->priv->selected_items);
        for (GList *l = selected_items; l != NULL; l = l->next) {
            ViewItem *item = l->data;

            xfdesktop_icon_view_invalidate_item(icon_view, item, FALSE);

            /* use offset to figure out where to put the icon*/
            if (item->placed) {
                col = (item->col - offset_col) % icon_view->priv->ncols;
                row = (item->row - offset_row) % icon_view->priv->nrows;
                /* wrap around the view */
                while(col < 0)
                    col += icon_view->priv->ncols;
                while(row < 0)
                    row += icon_view->priv->nrows;

                xfdesktop_icon_view_unplace_item(icon_view, item);
            }

            /* Find the next available slot for an icon if offset slot is not available */
            while(!xfdesktop_grid_is_free_position(icon_view, row, col)) {
                xfdesktop_next_slot(icon_view, &col, &row, icon_view->priv->ncols, icon_view->priv->nrows);
            }

            /* set new position */
            xfdesktop_icon_view_place_item_at(icon_view, item, row, col);
        }
        g_list_free(selected_items);

        XF_DEBUG("drag succeeded");

        gtk_drag_finish(context, TRUE, FALSE, time_);
    } else {
        gboolean ret = FALSE;
        GtkTreeIter *iter = NULL;
        GtkTreeIter dest_iter;

        g_object_set_data(G_OBJECT(context), "--xfdesktop-icon-view-drop-icon",
                          item_on_dest);

        if (item_on_dest != NULL && view_item_get_iter(item_on_dest, icon_view->priv->model, &dest_iter)) {
            iter = &dest_iter;
        }

        g_signal_emit(icon_view, __signals[SIG_DRAG_DROP_ITEM], 0,
                      context, iter, row, col, time_, &ret);

        return ret;
    }

    return TRUE;
}

static void
xfdesktop_icon_view_drag_data_received(GtkWidget *widget,
                                       GdkDragContext *context,
                                       gint x,
                                       gint y,
                                       GtkSelectionData *data,
                                       guint info,
                                       guint time_)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);
    gint row, col;
    ViewItem *item_on_dest = NULL;
    GtkTreeIter *iter = NULL;
    GtkTreeIter dest_iter;

    DBG("entering");

    item_on_dest = g_object_get_data(G_OBJECT(context),
                                     "--xfdesktop-icon-view-drop-icon");
    if (item_on_dest != NULL && view_item_get_iter(item_on_dest, icon_view->priv->model, &dest_iter)) {
        iter = &dest_iter;
    }

    if(icon_view->priv->dropped) {
        icon_view->priv->dropped = FALSE;

        xfdesktop_icon_view_clear_drag_highlight(icon_view);

        xfdesktop_xy_to_rowcol(icon_view, x, y, &row, &col);
        if(row >= icon_view->priv->nrows || col >= icon_view->priv->ncols)
            return;

        g_signal_emit(icon_view, __signals[SIG_DRAG_ITEM_DATA_RECEIVED], 0,
                      context, iter, row, col, data, info, time_);
    } else {
        /* FIXME: cannot use x and y here, for they don't seem to have any
         * meaningful value */
        GdkDragAction action = icon_view->priv->proposed_drop_action;

        g_signal_emit(icon_view, __signals[SIG_DROP_PROPOSE_ACTION], 0,
                      context, iter, action, data, info, &action);

        if(action == 0)
            xfdesktop_icon_view_clear_drag_highlight(icon_view);
        else
            xfdesktop_icon_view_draw_drag_highlight(icon_view,
                                                    icon_view->priv->hover_row,
                                                    icon_view->priv->hover_col);

        gdk_drag_status(context, action, time_);
    }
}

void
xfdesktop_icon_view_sort_icons(XfdesktopIconView *icon_view,
                               GtkSortType sort_type)
{
    GHashTable *priority_buckets;
    GList *sorted_buckets;

    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));
    g_return_if_fail(icon_view->priv->model != NULL);

    priority_buckets = g_hash_table_new(g_direct_hash, g_direct_equal);

    for (GList *l = icon_view->priv->items; l != NULL; l = l->next) {
        ViewItem *item = (ViewItem *)l->data;
        GtkTreeIter iter;

        if (item->placed) {
            xfdesktop_icon_view_unplace_item(icon_view, item);
        }
        item->row = -1;
        item->col = -1;

        if (view_item_get_iter(item, icon_view->priv->model, &iter)) {
            SortItem *sort_item;
            gint priority = 0;
            GList *sort_items;

            sort_item = sort_item_new(item);

            if (icon_view->priv->sort_priority_column != -1) {
                gtk_tree_model_get(icon_view->priv->model, &iter,
                                   icon_view->priv->text_column, &sort_item->label,
                                   icon_view->priv->sort_priority_column, &priority,
                                   -1);
            } else {
                gtk_tree_model_get(icon_view->priv->model, &iter,
                                   icon_view->priv->text_column, &sort_item->label,
                                   -1);
                priority = 0;
            }

            sort_items = g_hash_table_lookup(priority_buckets, GINT_TO_POINTER(priority));
            sort_items = g_list_insert_sorted_with_data(sort_items, sort_item, (GCompareDataFunc)sort_item_compare, GINT_TO_POINTER(sort_type));
            g_hash_table_replace(priority_buckets, GINT_TO_POINTER(priority), sort_items);
        }
    }

    sorted_buckets = g_hash_table_get_keys(priority_buckets);
    // FIXME: should GtkSortType also affect the order of the priority buckets?
    sorted_buckets = g_list_sort(sorted_buckets, int_compare);

    xfdesktop_icon_view_clear_grid_layout(icon_view);

    for (GList *sbl = sorted_buckets; sbl != NULL; sbl = sbl->next) {
        GList *sort_items = g_hash_table_lookup(priority_buckets, sbl->data);

        for (GList *l = sort_items; l != NULL; l = l->next) {
            SortItem *sort_item = (SortItem *)l->data;

            xfdesktop_icon_view_place_item(icon_view, sort_item->item, FALSE);
            sort_item_free(sort_item);
        }

        g_list_free(sort_items);
    }
    g_list_free(sorted_buckets);

    g_hash_table_destroy(priority_buckets);
}

static void
xfdesktop_icon_view_icon_theme_changed(GtkIconTheme *icon_theme,
                                       gpointer user_data)
{
    gtk_widget_queue_draw(GTK_WIDGET(user_data));
}

static void
xfdesktop_icon_view_style_updated(GtkWidget *widget)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);
    gdouble cell_text_width_proportion;

    DBG("entering");

    gtk_widget_style_get(widget,
                         "cell-spacing", &icon_view->priv->cell_spacing,
                         "cell-padding", &icon_view->priv->slot_padding,
                         "cell-text-width-proportion", &cell_text_width_proportion,
                         "ellipsize-icon-labels", &icon_view->priv->ellipsize_icon_labels,
                         "label-radius", &icon_view->priv->label_radius,
                         "tooltip-size", &icon_view->priv->tooltip_icon_size_style,
                         NULL);

    if (cell_text_width_proportion != icon_view->priv->cell_text_width_proportion) {
        icon_view->priv->cell_text_width_proportion = cell_text_width_proportion;
        g_object_notify(G_OBJECT(widget), "icon-width");
    }

    XF_DEBUG("cell spacing is %d", icon_view->priv->cell_spacing);
    XF_DEBUG("cell padding is %d", icon_view->priv->slot_padding);
    XF_DEBUG("cell text width proportion is %f", icon_view->priv->cell_text_width_proportion);
    XF_DEBUG("ellipsize icon label is %s", icon_view->priv->ellipsize_icon_labels?"true":"false");
    XF_DEBUG("label radius is %f", icon_view->priv->label_radius);

    if (icon_view->priv->text_renderer != NULL) {
        gint width = TEXT_WIDTH + icon_view->priv->label_radius * 2;
        g_object_set(icon_view->priv->text_renderer,
                     "alignment", icon_view->priv->center_text
                                  ? PANGO_ALIGN_CENTER
                                  : (gtk_widget_get_direction(GTK_WIDGET(icon_view)) == GTK_TEXT_DIR_RTL
                                     ? PANGO_ALIGN_RIGHT
                                     : PANGO_ALIGN_LEFT),
                     "align-set", TRUE,
                     "ellipsize", icon_view->priv->ellipsize_icon_labels ? PANGO_ELLIPSIZE_END : PANGO_ELLIPSIZE_NONE,
                     "ellipsize-set", TRUE,
                     "unselected-height", (gint)(TEXT_HEIGHT + icon_view->priv->label_radius * 2),
                     "width", (gint)width,
                     "wrap-width", (gint)(width * PANGO_SCALE),
                     "xpad", (gint)icon_view->priv->label_radius,
                     "ypad", (gint)icon_view->priv->label_radius,
                     NULL);
    }

    if (gtk_widget_get_realized(widget)) {
        xfdesktop_icon_view_invalidate_pixbuf_cache(icon_view);
        xfdesktop_icon_view_invalidate_all(icon_view, TRUE);
        gtk_widget_queue_draw(widget);
    }

    GTK_WIDGET_CLASS(xfdesktop_icon_view_parent_class)->style_updated(widget);
}

static void
scale_factor_changed_cb(XfdesktopIconView *icon_view,
                        GParamSpec *pspec,
                        gpointer user_data)
{
    if (gtk_widget_get_realized(GTK_WIDGET(icon_view))) {
        xfdesktop_icon_view_invalidate_pixbuf_cache(icon_view);
        xfdesktop_icon_view_size_grid(icon_view);
    }
}

static void
xfdesktop_icon_view_size_allocate(GtkWidget *widget,
                                  GtkAllocation *allocation)
{
    DBG("got size allocation: %dx%d+%d+%d", allocation->width, allocation->height, allocation->x, allocation->y);
    gtk_widget_set_allocation(widget, allocation);

    if (gtk_widget_get_realized(widget)) {
        if (gtk_widget_get_has_window(widget)) {
            gdk_window_move_resize(gtk_widget_get_window(widget), allocation->x, allocation->y, allocation->width, allocation->height);
        }
        xfdesktop_icon_view_size_grid(XFDESKTOP_ICON_VIEW(widget));
    }
}

static void
xfdesktop_icon_view_realize(GtkWidget *widget)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);
    GdkWindow *topwin;
    GdkScreen *gscreen;

    if (gtk_widget_get_realized(widget)) {
        return;
    }

    icon_view->priv->parent_window = gtk_widget_get_toplevel(widget);
    g_return_if_fail(icon_view->priv->parent_window);
    topwin = gtk_widget_get_window(icon_view->priv->parent_window);
    gtk_widget_set_window(widget, topwin);

    g_signal_connect(icon_view, "notify::scale-factor",
                     G_CALLBACK(scale_factor_changed_cb), NULL);

    /* we need this call here to initalize some members of icon_view->priv,
     * those depend on custom style properties */
    xfdesktop_icon_view_style_updated(widget);

    gtk_widget_set_realized(widget, TRUE);

    gtk_window_set_accept_focus(GTK_WINDOW(icon_view->priv->parent_window),
                                TRUE);
    gtk_window_set_focus_on_map(GTK_WINDOW(icon_view->priv->parent_window),
                                FALSE);

    xfdesktop_icon_view_size_grid(icon_view);

    /* unfortunately GTK_NO_WINDOW widgets don't receive events, with the
     * exception of draw events. */
    gtk_widget_add_events(icon_view->priv->parent_window,
                          GDK_POINTER_MOTION_HINT_MASK | GDK_KEY_PRESS_MASK
                          | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
                          | GDK_FOCUS_CHANGE_MASK | GDK_EXPOSURE_MASK
                          | GDK_LEAVE_NOTIFY_MASK);
    g_signal_connect(G_OBJECT(icon_view->priv->parent_window),
                     "motion-notify-event",
                     G_CALLBACK(xfdesktop_icon_view_motion_notify), icon_view);
    g_signal_connect(G_OBJECT(icon_view->priv->parent_window),
                     "leave-notify-event",
                     G_CALLBACK(xfdesktop_icon_view_leave_notify), icon_view);
    g_signal_connect(G_OBJECT(icon_view->priv->parent_window),
                     "key-press-event",
                     G_CALLBACK(xfdesktop_icon_view_key_press), icon_view);
    g_signal_connect(G_OBJECT(icon_view->priv->parent_window),
                     "button-press-event",
                     G_CALLBACK(xfdesktop_icon_view_button_press), icon_view);
    g_signal_connect(G_OBJECT(icon_view->priv->parent_window),
                     "button-release-event",
                     G_CALLBACK(xfdesktop_icon_view_button_release), icon_view);
    g_signal_connect(G_OBJECT(icon_view->priv->parent_window),
                     "focus-in-event",
                     G_CALLBACK(xfdesktop_icon_view_focus_in), icon_view);
    g_signal_connect(G_OBJECT(icon_view->priv->parent_window),
                     "focus-out-event",
                     G_CALLBACK(xfdesktop_icon_view_focus_out), icon_view);

    gscreen = gtk_widget_get_screen(widget);
    g_signal_connect_after(G_OBJECT(gtk_icon_theme_get_for_screen(gscreen)),
                           "changed",
                           G_CALLBACK(xfdesktop_icon_view_icon_theme_changed),
                           icon_view);

    if (icon_view->priv->model != NULL && icon_view->priv->items == NULL) {
        xfdesktop_icon_view_connect_model_signals(icon_view);
        xfdesktop_icon_view_populate_items(icon_view);
    }
}

static void
xfdesktop_icon_view_unrealize(GtkWidget *widget)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);
    GdkScreen *gscreen;

    gtk_window_set_accept_focus(GTK_WINDOW(icon_view->priv->parent_window), FALSE);

    gscreen = gtk_widget_get_screen(widget);

    g_signal_handlers_disconnect_by_func(G_OBJECT(gtk_icon_theme_get_for_screen(gscreen)),
                     G_CALLBACK(xfdesktop_icon_view_icon_theme_changed),
                     icon_view);

    g_signal_handlers_disconnect_by_func(G_OBJECT(icon_view->priv->parent_window),
                     G_CALLBACK(xfdesktop_icon_view_motion_notify), icon_view);
    g_signal_handlers_disconnect_by_func(G_OBJECT(icon_view->priv->parent_window),
                     G_CALLBACK(xfdesktop_icon_view_leave_notify), icon_view);
    g_signal_handlers_disconnect_by_func(G_OBJECT(icon_view->priv->parent_window),
                     G_CALLBACK(xfdesktop_icon_view_key_press), icon_view);
    g_signal_handlers_disconnect_by_func(G_OBJECT(icon_view->priv->parent_window),
                     G_CALLBACK(xfdesktop_icon_view_button_press), icon_view);
    g_signal_handlers_disconnect_by_func(G_OBJECT(icon_view->priv->parent_window),
                     G_CALLBACK(xfdesktop_icon_view_button_release), icon_view);
    g_signal_handlers_disconnect_by_func(G_OBJECT(icon_view->priv->parent_window),
                     G_CALLBACK(xfdesktop_icon_view_focus_in), icon_view);
    g_signal_handlers_disconnect_by_func(G_OBJECT(icon_view->priv->parent_window),
                     G_CALLBACK(xfdesktop_icon_view_focus_out), icon_view);

    g_signal_handlers_disconnect_by_func(G_OBJECT(icon_view),
                                         G_CALLBACK(scale_factor_changed_cb),
                                         NULL);

    if (icon_view->priv->model != NULL) {
        xfdesktop_icon_view_disconnect_model_signals(icon_view);
    }
    xfdesktop_icon_view_items_free(icon_view);

    g_free(icon_view->priv->grid_layout);
    icon_view->priv->grid_layout = NULL;

    gtk_widget_set_window(widget, NULL);
    gtk_widget_set_realized(widget, FALSE);
}

static gboolean
xfdesktop_icon_view_shift_to_slot_area(XfdesktopIconView *icon_view,
                                       ViewItem *item,
                                       GdkRectangle *rect,
                                       GdkRectangle *slot_rect)
{
    g_return_val_if_fail(item->row >= 0 && item->row < icon_view->priv->nrows, FALSE);
    g_return_val_if_fail(item->col >= 0 && item->col < icon_view->priv->ncols, FALSE);

    slot_rect->x = icon_view->priv->xmargin + item->col * SLOT_SIZE + item->col * icon_view->priv->xspacing + rect->x;
    slot_rect->y = icon_view->priv->ymargin + item->row * SLOT_SIZE + item->row * icon_view->priv->yspacing + rect->y;
    slot_rect->width = rect->width;
    slot_rect->height = rect->height;

    return TRUE;
}

static gboolean
xfdesktop_icon_view_place_item(XfdesktopIconView *icon_view,
                               ViewItem *item,
                               gboolean honor_model_position)
{
    gint row = -1, col = -1;

    if (honor_model_position && icon_view->priv->row_column != -1 && icon_view->priv->col_column != -1) {
        GtkTreeIter iter;

        if (view_item_get_iter(item, icon_view->priv->model, &iter)) {
            gtk_tree_model_get(icon_view->priv->model,
                               &iter,
                               icon_view->priv->row_column, &row,
                               icon_view->priv->col_column, &col,
                               -1);

            if (row >= 0 && row < icon_view->priv->nrows && col >= 0 && row < icon_view->priv->ncols) {
                xfdesktop_icon_view_place_item_at(icon_view, item, row, col);
            }
        }
    }

    if (!item->placed && xfdesktop_icon_view_get_next_free_grid_position(icon_view, -1, -1, &row, &col)) {
        xfdesktop_icon_view_place_item_at(icon_view, item, row, col);
    }

    return item->placed;
}

static gboolean
xfdesktop_icon_view_place_item_at(XfdesktopIconView *icon_view,
                                  ViewItem *item,
                                  gint row,
                                  gint col)
{
    g_return_val_if_fail(row >= 0 && row < icon_view->priv->nrows, FALSE);
    g_return_val_if_fail(col >= 0 && col < icon_view->priv->ncols, FALSE);
    g_return_val_if_fail(icon_view->priv->grid_layout != NULL, FALSE);

    if (xfdesktop_icon_view_item_in_slot(icon_view, row, col) == NULL) {
        GtkTreeIter iter;

        item->row = row;
        item->col = col;
        icon_view->priv->grid_layout[col * icon_view->priv->nrows + row] = item;
        item->placed = TRUE;

        xfdesktop_icon_view_invalidate_item(icon_view, item, TRUE);

        if (view_item_get_iter(item, icon_view->priv->model, &iter)) {
            g_signal_emit(icon_view, __signals[SIG_ICON_MOVED], 0, &iter, item->row, item->col);
        }
    }

    return item->placed;
}

static void
xfdesktop_icon_view_unplace_item(XfdesktopIconView *icon_view,
                                 ViewItem *item)
{
    gint row = item->row;
    gint col = item->col;

    g_return_if_fail(item->placed);

    xfdesktop_icon_view_invalidate_item(icon_view, item, FALSE);

    item->placed = FALSE;
    if (icon_view->priv->grid_layout != NULL
        && row >= 0 && row < icon_view->priv->nrows
        && col >= 0 && row < icon_view->priv->ncols
        && xfdesktop_icon_view_item_in_slot(icon_view, row, col) == item)
    {
        icon_view->priv->grid_layout[col * icon_view->priv->nrows + row] = NULL;
    }

    if (icon_view->priv->item_under_pointer == item) {
        icon_view->priv->item_under_pointer = NULL;
    }

    if (icon_view->priv->cursor == item) {
        icon_view->priv->cursor = NULL;
    }

    if (icon_view->priv->first_clicked_item == item) {
        icon_view->priv->first_clicked_item = NULL;
    }

    if (item->selected) {
        item->selected = FALSE;
        icon_view->priv->selected_items = g_list_remove(icon_view->priv->selected_items, item);
        g_signal_emit(icon_view, __signals[SIG_ICON_SELECTION_CHANGED], 0);
    }
}

static void
xfdesktop_icon_view_set_cell_properties(XfdesktopIconView *icon_view,
                                        ViewItem *item)
{
    GtkTreeIter iter;

    g_return_if_fail(GTK_IS_TREE_MODEL(icon_view->priv->model));
    g_return_if_fail(icon_view->priv->icon_renderer != NULL);
    g_return_if_fail(icon_view->priv->text_renderer != NULL);

    if (icon_view->priv->pixbuf_column != -1) {
        cairo_surface_t *surface = xfdesktop_icon_view_get_surface_for_item(icon_view, item);
        g_object_set(icon_view->priv->icon_renderer,
                     "surface", surface,
                     NULL);
        if (surface != NULL) {
            cairo_surface_destroy(surface);
        }
    }

    if (icon_view->priv->text_column != -1 && view_item_get_iter(item, icon_view->priv->model, &iter)) {
        gchar *text = NULL;
        gtk_tree_model_get(icon_view->priv->model,
                           &iter,
                           icon_view->priv->text_column, &text,
                           -1);
        g_object_set(icon_view->priv->text_renderer,
                     "text", text,
                     NULL);
        g_free(text);
    }
}

static void
xfdesktop_icon_view_unset_cell_properties(XfdesktopIconView *icon_view)
{
    g_return_if_fail(icon_view->priv->icon_renderer != NULL);
    g_return_if_fail(icon_view->priv->text_renderer != NULL);

    g_object_set(icon_view->priv->icon_renderer,
                 "surface", NULL,
                 NULL);
    g_object_set(icon_view->priv->text_renderer,
                 "text", NULL,
                 NULL);
}

static void
xfdesktop_icon_view_update_item_extents(XfdesktopIconView *icon_view,
                                        ViewItem *item)
{
    GtkRequisition min_req, nat_req;
    GtkRequisition *req;
    GdkRectangle total_extents;

    if (!item->placed) {
        if (G_UNLIKELY(!xfdesktop_icon_view_place_item(icon_view, item, TRUE))) {
            return;
        }
    }

    xfdesktop_icon_view_set_cell_properties(icon_view, item);

    total_extents.x = total_extents.y = 0;
    total_extents.width = total_extents.height = -1;

    // Icon renderer
    gtk_cell_renderer_get_preferred_size(icon_view->priv->icon_renderer, GTK_WIDGET(icon_view), &min_req, NULL);
    item->icon_extents.width = MIN(ICON_WIDTH, min_req.width);
    item->icon_extents.height = MIN(ICON_SIZE, min_req.height);
    item->icon_extents.x = MAX(0, (SLOT_SIZE - item->icon_extents.width) / 2);
    item->icon_extents.y = icon_view->priv->slot_padding + MAX(0, (ICON_SIZE - min_req.height) / 2);

    // Text renderer
    gtk_cell_renderer_get_preferred_size(icon_view->priv->text_renderer, GTK_WIDGET(icon_view), &min_req, &nat_req);
    req = item->selected ? &nat_req : &min_req;
    item->text_extents.width = MIN(SLOT_SIZE, req->width);
    item->text_extents.height = req->height;
    item->text_extents.x = MAX(0, (SLOT_SIZE - item->text_extents.width) / 2);
    item->text_extents.y = icon_view->priv->slot_padding + ICON_SIZE + icon_view->priv->slot_padding;

    gdk_rectangle_union(&item->icon_extents, &item->text_extents, &total_extents);
    item->slot_extents.x = item->slot_extents.y = item->slot_extents.width = item->slot_extents.height = 0;
    xfdesktop_icon_view_shift_to_slot_area(icon_view, item, &total_extents, &item->slot_extents);

#if 0
    DBG("new icon extents: %dx%d+%d+%d", item->icon_extents.width, item->icon_extents.height, item->icon_extents.x, item->icon_extents.y);
    DBG("new text extents: %dx%d+%d+%d", item->text_extents.width, item->text_extents.height, item->text_extents.x, item->text_extents.y);
    DBG("new slot extents: %dx%d+%d+%d", item->slot_extents.width, item->slot_extents.height, item->slot_extents.x, item->slot_extents.y);
#endif
}

static void
update_icon_surface_for_state(GtkCellRenderer *cell,
                              GtkStyleContext *style_context,
                              GtkCellRendererState flags)
{
    if (((flags & (GTK_CELL_RENDERER_SELECTED | GTK_CELL_RENDERER_PRELIT | GTK_CELL_RENDERER_INSENSITIVE)) != 0)) {
        cairo_surface_t *orig_surface = NULL;

        g_object_get(cell,
                     "surface", &orig_surface,
                     NULL);
        if (orig_surface != NULL && cairo_surface_get_type(orig_surface) == CAIRO_SURFACE_TYPE_IMAGE) {
            static cairo_user_data_key_t data_mem_key;
            cairo_surface_t *surface;
            cairo_t *cr;
            unsigned char *data;
            int height, stride;
            double xoff, yoff;
            double xscale, yscale;

            height = cairo_image_surface_get_height(orig_surface);
            stride = cairo_image_surface_get_stride(orig_surface);
            data = g_malloc(stride * height);
            memcpy(data, cairo_image_surface_get_data(orig_surface), stride * height);

            surface = cairo_image_surface_create_for_data(
                data,
                cairo_image_surface_get_format(orig_surface),
                cairo_image_surface_get_width(orig_surface),
                height,
                stride
            );
            cairo_surface_set_user_data(surface, &data_mem_key, data, g_free);
            cairo_surface_get_device_offset(orig_surface, &xoff, &yoff);
            cairo_surface_get_device_scale(orig_surface, &xscale, &yscale);
            cairo_surface_set_device_offset(surface, xoff, yoff);
            cairo_surface_set_device_scale(surface, xscale, yscale);
            cairo_surface_destroy(orig_surface);

            cr = cairo_create(surface);

            if ((flags & GTK_CELL_RENDERER_INSENSITIVE) != 0) {
                GdkRGBA color;

                gtk_style_context_get_color(style_context, GTK_STATE_FLAG_INSENSITIVE, &color);
                cairo_set_operator(cr, CAIRO_OPERATOR_MULTIPLY);
                gdk_cairo_set_source_rgba(cr, &color);
                cairo_mask_surface(cr, surface, 0, 0);
            }

            if ((flags & GTK_CELL_RENDERER_SELECTED) != 0) {
                GdkRGBA color;

                gtk_style_context_get_color(style_context, GTK_STATE_FLAG_ACTIVE, &color);
                cairo_set_operator(cr, CAIRO_OPERATOR_ATOP);
                cairo_set_source_rgba(cr, color.red, color.green, color.blue, 0.4);
                cairo_paint(cr);
            }

            if ((flags & GTK_CELL_RENDERER_PRELIT) != 0) {
                cairo_set_operator(cr, CAIRO_OPERATOR_COLOR_DODGE);
                cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
                cairo_mask_surface(cr, surface, 0, 0);
            }

            cairo_destroy(cr);

            g_object_set(cell,
                         "surface", surface,
                         NULL);
            cairo_surface_destroy(surface);
        }
    }
}

static void
xfdesktop_icon_view_draw_item_cell(XfdesktopIconView *icon_view,
                                   GtkStyleContext *style_context,
                                   cairo_t *cr,
                                   GdkRectangle *area,
                                   ViewItem *item,
                                   GtkCellRenderer *renderer,
                                   GtkCellRendererState flags,
                                   GdkRectangle *cell_extents)
{
    GdkRectangle cell_area;
    GdkRectangle draw_area;

    if (G_UNLIKELY(!gtk_cell_renderer_get_visible(renderer))) {
        return;
    }

    if (G_UNLIKELY(!xfdesktop_icon_view_shift_to_slot_area(icon_view, item, cell_extents, &cell_area))) {
        return;
    }

    if (G_UNLIKELY(!gdk_rectangle_intersect(area, &cell_area, &draw_area))) {
        return;
    }

    if (GTK_IS_CELL_RENDERER_PIXBUF(renderer)) {
        // Despite cell renderers supposedly following state, when you
        // set a surface (instead of a pixbuf) on a GtkCellRendererPixbuf
        // it doesn't apply icon effects.
        update_icon_surface_for_state(renderer, style_context, flags);
    }

    cairo_save(cr);

    gdk_cairo_rectangle(cr, &draw_area);
    cairo_clip(cr);

#if 0
    DBG("paint cell for (%d,%d) at %dx%d+%d+%d", item->row, item->col, cell_area.width, cell_area.height, cell_area.x, cell_area.y);
#endif

    gtk_cell_renderer_render(renderer,
                             cr,
                             GTK_WIDGET(icon_view),
                             &cell_area,
                             &cell_area,
                             flags);

#if 0
    cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
    cairo_rectangle(cr, cell_area.x, cell_area.y, cell_area.width, cell_area.height);
    cairo_stroke(cr);
#endif

    cairo_restore(cr);
}

static void
xfdesktop_icon_view_draw_item(XfdesktopIconView *icon_view,
                              cairo_t *cr,
                              GdkRectangle *area,
                              ViewItem *item)
{
    GtkStyleContext *style_context;
    GtkStateFlags state;
    gboolean has_focus;
    GtkCellRendererState flags = 0;

    g_return_if_fail(item->row >= 0 && item->row < icon_view->priv->nrows);
    g_return_if_fail(item->col >= 0 && item->col < icon_view->priv->ncols);

    // TODO: check grid slot area against extents and bail early if possible

    xfdesktop_icon_view_set_cell_properties(icon_view, item);

    style_context = gtk_widget_get_style_context(GTK_WIDGET(icon_view));
    state = gtk_widget_get_state_flags(GTK_WIDGET(icon_view));

    gtk_style_context_save(style_context);
    gtk_style_context_add_class(style_context, GTK_STYLE_CLASS_CELL);

    state &= ~(GTK_STATE_FLAG_SELECTED | GTK_STATE_FLAG_PRELIGHT | GTK_STATE_FLAG_FOCUSED);
    has_focus = gtk_widget_has_focus(GTK_WIDGET(icon_view));

    if (G_UNLIKELY(has_focus && item == icon_view->priv->cursor)) {
        flags |= GTK_CELL_RENDERER_FOCUSED;
    }

    if (G_UNLIKELY(item->selected)) {
        state |= GTK_STATE_FLAG_SELECTED;
        flags |= GTK_CELL_RENDERER_SELECTED;
    }

    if (G_UNLIKELY(icon_view->priv->item_under_pointer == item)) {
        state |= GTK_STATE_FLAG_PRELIGHT;
        flags |= GTK_CELL_RENDERER_PRELIT;
    }

    if (G_UNLIKELY(!item->sensitive)) {
        flags |= GTK_CELL_RENDERER_INSENSITIVE;
    }

    gtk_style_context_set_state(style_context, state);

    xfdesktop_icon_view_draw_item_cell(icon_view,
                                       style_context,
                                       cr,
                                       area,
                                       item,
                                       icon_view->priv->icon_renderer,
                                       flags,
                                       &item->icon_extents);
    xfdesktop_icon_view_draw_item_cell(icon_view,
                                       style_context,
                                       cr,
                                       area,
                                       item,
                                       icon_view->priv->text_renderer,
                                       flags,
                                       &item->text_extents);

    if (item == icon_view->priv->drop_dest_item) {
        GdkRectangle slot_rect = {
            .x = 0,
            .y = 0,
            .width = SLOT_SIZE,
            .height = SLOT_SIZE,
        };

        xfdesktop_icon_view_shift_to_slot_area(icon_view, item, &slot_rect, &slot_rect);
        gtk_render_focus(style_context, cr, slot_rect.x, slot_rect.y, slot_rect.width, slot_rect.height);
    }

    gtk_style_context_restore(style_context);
}

static gboolean
xfdesktop_icon_view_draw(GtkWidget *widget,
                         cairo_t *cr)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);
    cairo_rectangle_list_t *rects;
    cairo_rectangle_int_t temp;
    GdkRectangle clipbox;
    GtkStyleContext *context;
    gint i;

    /*DBG("entering");*/

    rects = cairo_copy_clip_rectangle_list(cr);

    if (rects->status != CAIRO_STATUS_SUCCESS) {
        cairo_rectangle_list_destroy(rects);
        return FALSE;
    }

    gdk_cairo_get_clip_rectangle(cr, &clipbox);
    TRACE("clipbox is %dx%d+%d+%d", clipbox.width, clipbox.height, clipbox.x, clipbox.y);

    for (GList *l = icon_view->priv->items; l != NULL; l = l->next) {
        ViewItem *item = l->data;
        if (item->placed && !item->selected) {
            xfdesktop_icon_view_draw_item(icon_view, cr, &clipbox, item);
        }
    }
    for (GList *l = icon_view->priv->items; l != NULL; l = l->next) {
        ViewItem *item = l->data;
        if (item->placed && item->selected) {
            xfdesktop_icon_view_draw_item(icon_view, cr, &clipbox, item);
        }
    }

    xfdesktop_icon_view_unset_cell_properties(icon_view);

    if(icon_view->priv->definitely_rubber_banding) {
        GdkRectangle intersect;

        context = gtk_widget_get_style_context(widget);
        gtk_style_context_save(context);
        gtk_style_context_add_class(context, GTK_STYLE_CLASS_RUBBERBAND);

        /* paint each rectangle in the expose region with the rubber
         * band color, semi-transparently */
        for(i = 0; i < rects->num_rectangles; ++i) {
            /* rects only contains cairo_rectangle_t's (with double values) which we
               cannot use in gdk_rectangle_intersect, so copy those values to temp.
               If there is a better way please update this part */
            temp.x = rects->rectangles[i].x;
            temp.y = rects->rectangles[i].y;
            temp.width = rects->rectangles[i].width;
            temp.height = rects->rectangles[i].height;

            if (!gdk_rectangle_intersect(&temp, &icon_view->priv->band_rect, &intersect))
            {
                continue;
            }

            cairo_save(cr);

            /* paint the rubber band area */
            gdk_cairo_rectangle(cr, &intersect);
            cairo_clip_preserve(cr);
            gtk_render_background(context, cr,
                                  icon_view->priv->band_rect.x,
                                  icon_view->priv->band_rect.y,
                                  icon_view->priv->band_rect.width,
                                  icon_view->priv->band_rect.height);
            gtk_render_frame(context, cr,
                             icon_view->priv->band_rect.x,
                             icon_view->priv->band_rect.y,
                             icon_view->priv->band_rect.width,
                             icon_view->priv->band_rect.height);

            cairo_restore(cr);
        }

        gtk_style_context_remove_class(context, GTK_STYLE_CLASS_RUBBERBAND);
        gtk_style_context_restore(context);
    }

    cairo_rectangle_list_destroy(rects);

    return FALSE;
}

static void
xfdesktop_icon_view_real_select_all(XfdesktopIconView *icon_view)
{
    DBG("entering");

    xfdesktop_icon_view_select_all(icon_view);
}

static void
xfdesktop_icon_view_real_unselect_all(XfdesktopIconView *icon_view)
{
    DBG("entering");

    xfdesktop_icon_view_unselect_all(icon_view);
}

static void
xfdesktop_icon_view_real_select_cursor_item(XfdesktopIconView *icon_view)
{
    DBG("entering");

    if(icon_view->priv->cursor)
        xfdesktop_icon_view_select_item_internal(icon_view, icon_view->priv->cursor, TRUE);
}

static void
xfdesktop_icon_view_real_toggle_cursor_item(XfdesktopIconView *icon_view)
{
    DBG("entering");

    if (icon_view->priv->cursor != NULL) {
        if (icon_view->priv->cursor->selected) {
            xfdesktop_icon_view_unselect_item_internal(icon_view, icon_view->priv->cursor, TRUE);
        } else {
            xfdesktop_icon_view_select_item_internal(icon_view, icon_view->priv->cursor, TRUE);
        }
    }
}

static gboolean
xfdesktop_icon_view_real_activate_selected_items(XfdesktopIconView *icon_view)
{
    DBG("entering");

    if (icon_view->priv->selected_items == NULL)
        return FALSE;

    g_signal_emit(G_OBJECT(icon_view), __signals[SIG_ICON_ACTIVATED], 0);

    return TRUE;
}

static void
xfdesktop_icon_view_select_between(XfdesktopIconView *icon_view,
                                   ViewItem *start_item,
                                   ViewItem *end_item)
{
    gint start_row, start_col, end_row, end_col;

    g_return_if_fail(start_item->row >= 0 && start_item->row < icon_view->priv->nrows);
    g_return_if_fail(start_item->col >= 0 && start_item->col < icon_view->priv->ncols);
    g_return_if_fail(end_item->row >= 0 && end_item->row < icon_view->priv->nrows);
    g_return_if_fail(end_item->col >= 0 && end_item->col < icon_view->priv->ncols);

    start_row = start_item->row;
    start_col = start_item->col;
    end_row = end_item->row;
    end_col = end_item->col;

    if(start_row > end_row || (start_row == end_row && start_col > end_col)) {
        /* flip start and end */
        gint tmpr = start_row, tmpc = start_col;

        start_row = end_row;
        start_col = end_col;
        end_row = tmpr;
        end_col = tmpc;
    }

    for(gint i = start_row; i <= end_row; ++i) {
        for(gint j = (i == start_row ? start_col : 0);
            (i == end_row ? j <= end_col : j < icon_view->priv->ncols);
            ++j)
        {
            ViewItem *item = xfdesktop_icon_view_item_in_slot(icon_view, i, j);
            if (item != NULL) {
                xfdesktop_icon_view_select_item_internal(icon_view, item, TRUE);
            }
        }
    }
}

static ViewItem *
xfdesktop_icon_view_find_first_icon(XfdesktopIconView *icon_view)
{
    ViewItem *item = NULL;

    if (icon_view->priv->items != NULL) {
        for (gint i = 0; i < icon_view->priv->nrows && item == NULL; ++i) {
            for (gint j = 0; j < icon_view->priv->ncols && item == NULL; ++j) {
                item = xfdesktop_icon_view_item_in_slot(icon_view, i, j);
                if (item != NULL) {
                    return item;
                }
            }
        }
    }

    return item;
}

static ViewItem *
xfdesktop_icon_view_find_last_icon(XfdesktopIconView *icon_view)
{
    ViewItem *item = NULL;

    if (icon_view->priv->items != NULL) {
        for (gint i = icon_view->priv->nrows - 1; i >= 0 && item == NULL; --i) {
            for (gint j = icon_view->priv->ncols - 1; j >= 0 && item == NULL; --j) {
                item = xfdesktop_icon_view_item_in_slot(icon_view, i, j);
                if (item != NULL) {
                    return item;
                }
            }
        }
    }

    return item;
}

static void
xfdesktop_icon_view_move_cursor_direction(XfdesktopIconView *icon_view,
                                          GtkDirectionType direction,
                                          guint count,
                                          GdkModifierType modmask)
{
    g_return_if_fail(direction == GTK_DIR_UP
                     || direction == GTK_DIR_DOWN
                     || direction == GTK_DIR_LEFT
                     || direction == GTK_DIR_RIGHT);

    if (icon_view->priv->cursor == NULL) {
        ViewItem *item = NULL;

        /* choose first or last item depending on left/up or right/down */
        if (direction == GTK_DIR_LEFT || direction == GTK_DIR_UP) {
            item = xfdesktop_icon_view_find_last_icon(icon_view);
        } else {
            item = xfdesktop_icon_view_find_first_icon(icon_view);
        }

        if (item != NULL) {
            if ((modmask & GDK_CONTROL_MASK) == 0) {
                xfdesktop_icon_view_unselect_all(icon_view);
            }
            icon_view->priv->cursor = item;
            xfdesktop_icon_view_select_item_internal(icon_view, item, TRUE);
        }
    } else {
        guint remaining = count;
        gint step = direction == GTK_DIR_UP || direction == GTK_DIR_LEFT ? -1 : 1;
        gint row = icon_view->priv->cursor->row;
        gint col = icon_view->priv->cursor->col;
        gboolean row_major = direction == GTK_DIR_LEFT || direction == GTK_DIR_RIGHT;

        if (row < 0 || col < 0) {
            return;
        }

        if (!icon_view->priv->cursor->selected) {
            xfdesktop_icon_view_invalidate_item(icon_view, icon_view->priv->cursor, FALSE);
        }

        if ((modmask & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) == 0) {
            xfdesktop_icon_view_unselect_all(icon_view);
        }

        for(gint i = row_major ? row : col;
            remaining > 0 && (row_major
                              ? (step < 0 ? i >= 0 : i < icon_view->priv->nrows)
                              : (step < 0 ? i >= 0 : i < icon_view->priv->ncols));
            i += step)
        {
            for(gint j = row_major
                         ? (i == row ? col + step : (step < 0) ? icon_view->priv->ncols - 1 : 0)
                         : (i == col ? row + step : (step < 0) ? icon_view->priv->nrows - 1 : 0);
                remaining > 0 && (row_major
                                  ? (step < 0 ? j >= 0 : j < icon_view->priv->ncols)
                                  : (step < 0 ? j >= 0 : j < icon_view->priv->nrows));
                j += step)
            {
                gint slot_row = row_major ? i : j;
                gint slot_col = row_major ? j : i;
                ViewItem *item = xfdesktop_icon_view_item_in_slot(icon_view, slot_row, slot_col);

                if (item != NULL) {
                    icon_view->priv->cursor = item;
                    if ((modmask & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) != 0 || remaining == 1) {
                        xfdesktop_icon_view_select_item_internal(icon_view, item, TRUE);
                    }
                    remaining -= 1;
                }
            }
        }

        if (icon_view->priv->selected_items == NULL) {
            ViewItem *item;
            if (step < 0) {
                item = xfdesktop_icon_view_find_first_icon(icon_view);
            } else {
                item = xfdesktop_icon_view_find_last_icon(icon_view);
            }

            if (item != NULL) {
                icon_view->priv->cursor = item;
                xfdesktop_icon_view_select_item_internal(icon_view, item, TRUE);
            }
        }
    }
}

static void
xfdesktop_icon_view_move_cursor_begin_end(XfdesktopIconView *icon_view,
                                          gint count,
                                          GdkModifierType modmask)
{
    ViewItem *item = NULL;
    if (count < 0) {
        item = xfdesktop_icon_view_find_first_icon(icon_view);
    } else {
        item = xfdesktop_icon_view_find_last_icon(icon_view);
    }

    if (item != NULL) {
        ViewItem *old_cursor = icon_view->priv->cursor;
        icon_view->priv->cursor = item;

        if (old_cursor != NULL && !old_cursor->selected) {
            xfdesktop_icon_view_invalidate_item(icon_view, old_cursor, FALSE);
        }

        if(!old_cursor || !(modmask & (GDK_SHIFT_MASK|GDK_CONTROL_MASK))) {
            xfdesktop_icon_view_unselect_all(icon_view);
            xfdesktop_icon_view_select_item_internal(icon_view, item, TRUE);
        } else if(old_cursor) {
            if(modmask & GDK_SHIFT_MASK) {
                /* select everything between the cursor and the old_cursor */
                xfdesktop_icon_view_select_between(icon_view, old_cursor, item);
            } else if(modmask & GDK_CONTROL_MASK) {
                /* add the icon to the selection */
                xfdesktop_icon_view_select_item_internal(icon_view, item, TRUE);
            }

        }
    }
}

static gboolean
xfdesktop_icon_view_real_move_cursor(XfdesktopIconView *icon_view,
                                     GtkMovementStep step,
                                     gint count)
{
    GdkModifierType modmask = 0;

    g_return_val_if_fail(step == GTK_MOVEMENT_VISUAL_POSITIONS
                         || step == GTK_MOVEMENT_DISPLAY_LINES
                         || step == GTK_MOVEMENT_BUFFER_ENDS, FALSE);

    if(count == 0)
        return FALSE;

    gtk_widget_grab_focus(GTK_WIDGET(icon_view));
    gtk_get_current_event_state(&modmask);

    switch(step) {
        case GTK_MOVEMENT_VISUAL_POSITIONS:
            xfdesktop_icon_view_move_cursor_direction(icon_view,
                                                      count > 0 ? GTK_DIR_RIGHT : GTK_DIR_LEFT,
                                                      ABS(count),
                                                      modmask);
            break;

        case GTK_MOVEMENT_DISPLAY_LINES:
            xfdesktop_icon_view_move_cursor_direction(icon_view,
                                                      count > 0 ? GTK_DIR_DOWN : GTK_DIR_UP,
                                                      ABS(count),
                                                      modmask);
            break;

        case GTK_MOVEMENT_BUFFER_ENDS:
            xfdesktop_icon_view_move_cursor_begin_end(icon_view, count, modmask);
            break;

        default:
            g_assert_not_reached();
    }

    return TRUE;
}

static inline gboolean
xfdesktop_rectangle_equal(GdkRectangle *rect1, GdkRectangle *rect2)
{
    return (rect1->x == rect2->x && rect1->y == rect2->y
            && rect1->width == rect2->width && rect1->height == rect2->height);
}

static inline gboolean
xfdesktop_rectangle_is_bounded_by(GdkRectangle *rect,
                                  GdkRectangle *bounds)
{
    GdkRectangle intersection;

    if(gdk_rectangle_intersect(rect, bounds, &intersection)) {
        if(xfdesktop_rectangle_equal(rect, &intersection))
            return TRUE;
    }

    return FALSE;
}

static void
xfdesktop_icon_view_temp_unplace_items(XfdesktopIconView *icon_view)
{
    for (GList *l = icon_view->priv->items; l != NULL; l = l->next) {
        ViewItem *item = l->data;
        if (item->placed) {
            xfdesktop_icon_view_unplace_item(icon_view, item);
        }
    }
}

static void
xfdesktop_icon_view_replace_items(XfdesktopIconView *icon_view)
{
    // First try to place items that already had locations set
    for (GList *l = icon_view->priv->items; l != NULL; l = l->next) {
        ViewItem *item = l->data;

        if (!item->placed) {
            if (item->row < 0 || item->row >= icon_view->priv->nrows
                || item->col < 0 || item->col >= icon_view->priv->ncols)
            {
                item->row = -1;
                item->col = -1;
            } else {
                if (!xfdesktop_icon_view_place_item_at(icon_view, item, item->row, item->col)) {
                    item->row = -1;
                    item->col = -1;
                }
            }
        }
    }

    // Then try to place the rest
    for (GList *l = icon_view->priv->items; l != NULL; l = l->next) {
        ViewItem *item = l->data;
        if (!item->placed) {
            xfdesktop_icon_view_place_item(icon_view, item, TRUE);
        }
    }
}

/* FIXME: add a cache for this so we don't have to compute this EVERY time */
static void
xfdesktop_icon_view_setup_grids_xinerama(XfdesktopIconView *icon_view)
{
    GdkDisplay *display;
    GdkRectangle *monitor_geoms, cell_rect;
    gint nmonitors, i, row, col;
    gint dx = 0, dy = 0;

    DBG("entering");

    display = gtk_widget_get_display(GTK_WIDGET(icon_view));
    nmonitors = gdk_display_get_n_monitors(display);

    if(nmonitors == 1)  /* optimisation */
        return;

    gtk_widget_translate_coordinates(xfdesktop_icon_view_get_window_widget(icon_view),
                                     GTK_WIDGET(icon_view),
                                     0, 0, &dx, &dy);

    monitor_geoms = g_new0(GdkRectangle, nmonitors);

    for(i = 0; i < nmonitors; ++i) {
        gdk_monitor_get_geometry(gdk_display_get_monitor(display, i), &monitor_geoms[i]);
    }

    /* cubic time; w00t! */
    cell_rect.width = cell_rect.height = SLOT_SIZE;
    for(row = 0; row < icon_view->priv->nrows; ++row) {
        for(col = 0; col < icon_view->priv->ncols; ++col) {
            gboolean bounded = FALSE;

            cell_rect.x = icon_view->priv->xmargin + col * SLOT_SIZE + col * icon_view->priv->xspacing - dx;
            cell_rect.y = icon_view->priv->ymargin + row * SLOT_SIZE + row * icon_view->priv->yspacing - dy;

            for(i = 0; i < nmonitors; ++i) {
                if(xfdesktop_rectangle_is_bounded_by(&cell_rect,
                                                     &monitor_geoms[i]))
                {
                    bounded = TRUE;
                    break;
                }
            }

            if(!bounded) {
                xfdesktop_grid_unset_position_free_raw(icon_view, row, col, TOMBSTONE);
            } else if (xfdesktop_icon_view_item_in_slot(icon_view, row, col) == TOMBSTONE) {
                xfdesktop_grid_set_position_free(icon_view, row, col);
            }
        }
    }

    g_free(monitor_geoms);

    DBG("exiting");
}

static void
xfdesktop_icon_view_size_grid(XfdesktopIconView *icon_view)
{
    gint xrest = 0, yrest = 0, width = 0, height = 0;
    gsize old_size, new_size;
    gint old_nrows, old_ncols;
    gint new_nrows, new_ncols;
    gboolean grid_changed;

    DBG("entering");

    gtk_widget_get_size_request(GTK_WIDGET(icon_view), &width, &height);
    if (width == -1 || height == -1) {
        GtkRequisition req;
        gtk_widget_get_preferred_size(GTK_WIDGET(icon_view), &req, NULL);
        width = req.width;
        height = req.height;
    }
    DBG("icon view size: %dx%d", width, height);

    old_size = (guint)icon_view->priv->nrows * icon_view->priv->ncols * sizeof(ViewItem *);
    old_nrows = icon_view->priv->nrows;
    old_ncols = icon_view->priv->ncols;

    new_nrows = MAX((height - MIN_MARGIN * 2) / SLOT_SIZE, 0);
    new_ncols = MAX((width - MIN_MARGIN * 2) / SLOT_SIZE, 0);
    if (new_nrows <= 0 || new_ncols <= 0) {
        return;
    }
    grid_changed = old_nrows != new_nrows || old_ncols != new_ncols;

    if (!grid_changed && icon_view->priv->width == width && icon_view->priv->height == height) {
        return;
    }

    if (grid_changed) {
        g_signal_emit(icon_view, __signals[SIG_START_GRID_RESIZE], 0, new_nrows, new_ncols);
        xfdesktop_icon_view_temp_unplace_items(icon_view);
    }

    icon_view->priv->width = width;
    icon_view->priv->height = height;
    icon_view->priv->nrows = new_nrows;
    icon_view->priv->ncols = new_ncols;

    new_size = (guint)icon_view->priv->nrows * icon_view->priv->ncols * sizeof(ViewItem *);

    xrest = icon_view->priv->width - icon_view->priv->ncols * SLOT_SIZE;
    if (icon_view->priv->ncols > 1) {
        icon_view->priv->xspacing = (xrest - MIN_MARGIN * 2) / (icon_view->priv->ncols - 1);
    } else {
        /* Let's not try to divide by 0 */
        icon_view->priv->xspacing = 1;
    }
    icon_view->priv->xmargin = (xrest - (icon_view->priv->ncols - 1) * icon_view->priv->xspacing) / 2;

    yrest = icon_view->priv->height - icon_view->priv->nrows * SLOT_SIZE;
    if (icon_view->priv->nrows > 1) {
        icon_view->priv->yspacing = (yrest - MIN_MARGIN * 2) / (icon_view->priv->nrows - 1);
    } else {
        /* Let's not try to divide by 0 */
        icon_view->priv->yspacing = 1;
    }
    icon_view->priv->ymargin = (yrest - (icon_view->priv->nrows - 1) * icon_view->priv->yspacing) / 2;

    if (icon_view->priv->grid_layout == NULL) {
        icon_view->priv->grid_layout = g_malloc0(new_size);
        xfdesktop_icon_view_setup_grids_xinerama(icon_view);
    } else if (old_size != new_size) {
        DBG("old_size != new_size; resizing grid");
        icon_view->priv->grid_layout = g_realloc(icon_view->priv->grid_layout,
                                                 new_size);

        if (new_size > old_size) {
            memset(((guint8 *)icon_view->priv->grid_layout) + old_size, 0,
                   new_size - old_size);
        }
        xfdesktop_icon_view_setup_grids_xinerama(icon_view);
    } else if (old_nrows != new_nrows || old_ncols != new_ncols) {
        xfdesktop_icon_view_setup_grids_xinerama(icon_view);
    }

    if (grid_changed) {
        g_signal_emit(icon_view, __signals[SIG_END_GRID_RESIZE], 0);
        xfdesktop_icon_view_replace_items(icon_view);
    }
    g_signal_emit(G_OBJECT(icon_view), __signals[SIG_RESIZE_EVENT], 0, NULL);

    DBG("SLOT_SIZE=%0.3f, TEXT_WIDTH=%0.3f, ICON_SIZE=%u", SLOT_SIZE, TEXT_WIDTH, ICON_SIZE);
    DBG("grid size is %dx%d", icon_view->priv->nrows, icon_view->priv->ncols);

    XF_DEBUG("created grid_layout with %lu positions", (gulong)(new_size/sizeof(gpointer)));
    DUMP_GRID_LAYOUT(icon_view);

    if (gtk_widget_get_realized(GTK_WIDGET(icon_view))) {
        gtk_widget_queue_draw(GTK_WIDGET(icon_view));
    }
}

static gboolean
xfdesktop_icon_view_queue_draw_item(XfdesktopIconView *icon_view,
                                    ViewItem *item)
{
    gint dx, dy;

    gtk_widget_translate_coordinates(xfdesktop_icon_view_get_window_widget(icon_view),
                                     GTK_WIDGET(icon_view),
                                     0, 0, &dx, &dy);

    if (icon_view->priv->drop_dest_item == item) {
        GdkRectangle slot_rect = {
            .x = 0,
            .y = 0,
            .width = SLOT_SIZE,
            .height = SLOT_SIZE,
        };
        xfdesktop_icon_view_shift_to_slot_area(icon_view, item, &slot_rect, &slot_rect);
        gtk_widget_queue_draw_area(GTK_WIDGET(icon_view),
                                   slot_rect.x - dx, slot_rect.y - dy,
                                   slot_rect.width, slot_rect.height);
    }

    if (item->slot_extents.width > 0 && item->slot_extents.height > 0) {
        gtk_widget_queue_draw_area(GTK_WIDGET(icon_view),
                                   item->slot_extents.x - dx, item->slot_extents.y - dy,
                                   item->slot_extents.width, item->slot_extents.height);
        return TRUE;
    }
    return FALSE;
}

static void
xfdesktop_icon_view_invalidate_item(XfdesktopIconView *icon_view,
                                    ViewItem *item,
                                    gboolean recalc_extents)
{
    g_return_if_fail(item != NULL);

    if (!xfdesktop_icon_view_queue_draw_item(icon_view, item)) {
        recalc_extents = TRUE;
    }

    if (recalc_extents) {
        xfdesktop_icon_view_update_item_extents(icon_view, item);
        xfdesktop_icon_view_queue_draw_item(icon_view, item);
    }
}

static void
xfdesktop_icon_view_select_item_internal(XfdesktopIconView *icon_view,
                                         ViewItem *item,
                                         gboolean emit_signal)
{
    if (!item->selected) {
        if (icon_view->priv->sel_mode == GTK_SELECTION_SINGLE) {
            xfdesktop_icon_view_unselect_all(icon_view);
        }

        item->selected = TRUE;
        icon_view->priv->selected_items = g_list_prepend(icon_view->priv->selected_items, item);

        xfdesktop_icon_view_invalidate_item(icon_view, item, TRUE);

        if (emit_signal) {
            g_signal_emit(icon_view, __signals[SIG_ICON_SELECTION_CHANGED], 0);
        }
    }
}

static void
xfdesktop_icon_view_unselect_item_internal(XfdesktopIconView *icon_view,
                                           ViewItem *item,
                                           gboolean emit_signal)
{
    if (item->selected) {
        item->selected = FALSE;
        icon_view->priv->selected_items = g_list_remove(icon_view->priv->selected_items, item);

        xfdesktop_icon_view_invalidate_item(icon_view, item, TRUE);

        if (emit_signal) {
            g_signal_emit(icon_view, __signals[SIG_ICON_SELECTION_CHANGED], 0);
        }
    }
}

static inline gboolean
xfdesktop_grid_is_free_position(XfdesktopIconView *icon_view,
                                gint row,
                                gint col)
{
    if(icon_view->priv->grid_layout == NULL) {
        return FALSE;
    }

    if(row >= icon_view->priv->nrows || col >= icon_view->priv->ncols || row < 0 || col < 0)
    {
        return FALSE;
    }

    return !icon_view->priv->grid_layout[col * icon_view->priv->nrows + row];
}

static inline gboolean
next_pos(XfdesktopIconView *icon_view,
         gint row,
         gint col,
         gint *next_row,
         gint *next_col)
{
    g_return_val_if_fail((row == -1 && col == -1) || (row != -1 && col != -1), FALSE);
    g_return_val_if_fail(next_row != NULL && next_col != NULL, FALSE);

    if (row == -1 && col == -1) {
        if ((icon_view->priv->gravity & XFDESKTOP_ICON_VIEW_GRAVITY_BOTTOM) != 0) {
            *next_row = icon_view->priv->nrows - 1;
        } else {
            *next_row = 0;
        }

        if ((icon_view->priv->gravity & XFDESKTOP_ICON_VIEW_GRAVITY_RIGHT) != 0) {
            *next_col = icon_view->priv->ncols - 1;
        } else {
            *next_col = 0;
        }
    } else if ((icon_view->priv->gravity & XFDESKTOP_ICON_VIEW_GRAVITY_HORIZONTAL) != 0) {
        if ((icon_view->priv->gravity & XFDESKTOP_ICON_VIEW_GRAVITY_RIGHT) != 0) {
            col -= 1;
        } else {
            col += 1;
        }

        if (col < 0 || col >= icon_view->priv->ncols) {
            if (col < 0) {
                col = icon_view->priv->ncols - 1;
            } else {
                col = 0;
            }

            if ((icon_view->priv->gravity & XFDESKTOP_ICON_VIEW_GRAVITY_BOTTOM) != 0) {
                row -= 1;
                if (row < 0) {
                    return FALSE;
                }
            } else {
                row += 1;
                if (row >= icon_view->priv->nrows) {
                    return FALSE;
                }
            }
        }

        *next_row = row;
        *next_col = col;
    } else {
        if ((icon_view->priv->gravity & XFDESKTOP_ICON_VIEW_GRAVITY_BOTTOM) != 0) {
            row -= 1;
        } else {
            row += 1;
        }

        if (row < 0 || row >= icon_view->priv->nrows) {
            if (row < 0) {
                row = icon_view->priv->nrows - 1;
            } else {
                row = 0;
            }

            if ((icon_view->priv->gravity & XFDESKTOP_ICON_VIEW_GRAVITY_RIGHT) != 0) {
                col -= 1;
                if (col < 0) {
                    return FALSE;
                }
            } else {
                col += 1;
                if (col >= icon_view->priv->ncols) {
                    return FALSE;
                }
            }
        }

        *next_row = row;
        *next_col = col;
    }

    return TRUE;
}

gboolean
xfdesktop_icon_view_get_next_free_grid_position(XfdesktopIconView *icon_view,
                                                gint row,
                                                gint col,
                                                gint *next_row,
                                                gint *next_col)
{
    gint cur_row = row;
    gint cur_col = col;

    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view), FALSE);
    g_return_val_if_fail(row >= -1 && row < icon_view->priv->nrows, FALSE);
    g_return_val_if_fail(col >= -1 && col < icon_view->priv->ncols, FALSE);
    g_return_val_if_fail(next_row != NULL && next_col != NULL, FALSE);

    while (next_pos(icon_view, cur_row, cur_col, &cur_row, &cur_col)) {
        gint idx = cur_col * icon_view->priv->nrows + cur_row;
        if (icon_view->priv->grid_layout[idx] == NULL) {
            *next_row = cur_row;
            *next_col = cur_col;
            return TRUE;
        }
    }

    return FALSE;
}

static inline void
xfdesktop_grid_set_position_free(XfdesktopIconView *icon_view,
                                 gint row,
                                 gint col)
{
    g_return_if_fail(row < icon_view->priv->nrows
                     && col < icon_view->priv->ncols
                     && row >= 0 && col >= 0);

#if 0 /*def DEBUG*/
    DUMP_GRID_LAYOUT(icon_view);
#endif

    icon_view->priv->grid_layout[col * icon_view->priv->nrows + row] = NULL;

#if 0 /*def DEBUG*/
    DUMP_GRID_LAYOUT(icon_view);
#endif
}

static inline gboolean
xfdesktop_grid_unset_position_free_raw(XfdesktopIconView *icon_view,
                                       gint row,
                                       gint col,
                                       gpointer data)
{
    gint idx;

    g_return_val_if_fail(row < icon_view->priv->nrows
                         && col < icon_view->priv->ncols
                         && row >= 0 && col >= 0, FALSE);

    idx = col * icon_view->priv->nrows + row;
    if(icon_view->priv->grid_layout[idx])
        return FALSE;

#if 0 /*def DEBUG*/
    DUMP_GRID_LAYOUT(icon_view);
#endif

    icon_view->priv->grid_layout[idx] = data;

#if 0 /*def DEBUG*/
    DUMP_GRID_LAYOUT(icon_view);
#endif

    return TRUE;
}

static inline ViewItem *
xfdesktop_icon_view_item_in_slot_raw(XfdesktopIconView *icon_view,
                                     gint idx)
{
    ViewItem *item = icon_view->priv->grid_layout[idx];

    if (TOMBSTONE == item) {
        return NULL;
    } else {
        return item;
    }
}

static inline ViewItem *
xfdesktop_icon_view_item_in_slot(XfdesktopIconView *icon_view,
                                 gint row,
                                 gint col)
{
    gint idx;

    g_return_val_if_fail(row < icon_view->priv->nrows
                         && col < icon_view->priv->ncols, NULL);

    idx = col * icon_view->priv->nrows + row;

    /* FIXME: that's why we can't drag icons to monitors on the left or above,
     * the array maps positions on the grid starting from the icons_on_primary monitor. */
    if (idx < 0)
        return NULL;

    return xfdesktop_icon_view_item_in_slot_raw(icon_view, idx);
}

static inline gboolean
xfdesktop_rectangle_contains_point(GdkRectangle *rect, gint x, gint y)
{
    if(x > rect->x + rect->width
            || x < rect->x
            || y > rect->y + rect->height
            || y < rect->y)
    {
        return FALSE;
    }

    return TRUE;
}

static gint
xfdesktop_check_icon_clicked(gconstpointer data,
                             gconstpointer user_data)
{
    ViewItem *item = (ViewItem *)data;
    GdkEventButton *evt = (GdkEventButton *)user_data;

    return xfdesktop_rectangle_contains_point(&item->slot_extents, evt->x, evt->y) ? 0 : 1;
}

static void
xfdesktop_icon_view_init_builtin_cell_renderers(XfdesktopIconView *icon_view)
{
    PangoAttrList *attr_list = NULL;

    g_return_if_fail(icon_view->priv->icon_renderer != NULL);
    g_return_if_fail(icon_view->priv->text_renderer != NULL);

#if PANGO_VERSION_CHECK (1, 44, 0)
    attr_list = pango_attr_list_new();
    {
        PangoAttribute *attr = pango_attr_insert_hyphens_new(FALSE);
        attr->start_index = 0;
        attr->end_index = -1;
        pango_attr_list_change(attr_list, attr);
    }
#endif

    g_object_set(icon_view->priv->text_renderer,
                 "attributes", attr_list,
                 "underline-when-prelit", icon_view->priv->single_click &&
                                          icon_view->priv->single_click_underline_hover,
                 "wrap-mode", PANGO_WRAP_WORD_CHAR,
                 "xalign", (gfloat)0.5,
                 "yalign", (gfloat)0.0,
                 NULL);

    if (attr_list != NULL) {
        pango_attr_list_unref(attr_list);
    }
}

static void
xfdesktop_icon_view_populate_items(XfdesktopIconView *icon_view)
{
    GQueue *pending_items;
    GtkTreeIter iter;
    ViewItem *pending_item;

    g_return_if_fail(icon_view->priv->model != NULL);
    g_return_if_fail(icon_view->priv->items == NULL);

    pending_items = g_queue_new();

    if (gtk_tree_model_get_iter_first(icon_view->priv->model, &iter)) {
        do {
            ViewItem *item = view_item_new(icon_view->priv->model, &iter);
            GtkTreePath *path;
            gint index;
            gboolean placed = FALSE;

            path = gtk_tree_model_get_path(icon_view->priv->model, &iter);
            index = gtk_tree_path_get_indices(path)[0];
            gtk_tree_path_free(path);

            icon_view->priv->items = g_list_insert(icon_view->priv->items, item, index);

            if (icon_view->priv->row_column != -1 && icon_view->priv->col_column != -1) {
                gint row, col;

                gtk_tree_model_get(icon_view->priv->model, &iter,
                                   icon_view->priv->row_column, &row,
                                   icon_view->priv->col_column, &col,
                                   -1);
                if (row >= 0 && row < icon_view->priv->nrows && col >= 0 && col < icon_view->priv->ncols) {
                    if (xfdesktop_icon_view_place_item_at(icon_view, item, row, col)) {
                        placed = TRUE;
                    }
                }
            }

            if (!placed) {
                g_queue_push_tail(pending_items, item);
            }

        } while (gtk_tree_model_iter_next(icon_view->priv->model, &iter));
    }

    while ((pending_item = g_queue_pop_head(pending_items))) {
        if (xfdesktop_icon_view_place_item(icon_view, pending_item, TRUE)) {
            DBG("placed new icon at (%d, %d)", pending_item->row, pending_item->col);
        } else {
            DBG("failed to place new icon");
        }
    }

    g_queue_free(pending_items);
}

static void
xfdesktop_icon_view_connect_model_signals(XfdesktopIconView *icon_view)
{
    g_signal_connect(icon_view->priv->model, "row-inserted",
                     G_CALLBACK(xfdesktop_icon_view_model_row_inserted), icon_view);
    g_signal_connect(icon_view->priv->model, "row-changed",
                     G_CALLBACK(xfdesktop_icon_view_model_row_changed), icon_view);
    g_signal_connect(icon_view->priv->model, "row-deleted",
                     G_CALLBACK(xfdesktop_icon_view_model_row_deleted), icon_view);

}

static void
xfdesktop_icon_view_disconnect_model_signals(XfdesktopIconView *icon_view)
{
    g_return_if_fail(icon_view->priv->model != NULL);

    g_signal_handlers_disconnect_by_func(icon_view->priv->model,
                                         G_CALLBACK(xfdesktop_icon_view_model_row_inserted),
                                         icon_view);
    g_signal_handlers_disconnect_by_func(icon_view->priv->model,
                                         G_CALLBACK(xfdesktop_icon_view_model_row_changed),
                                         icon_view);
    g_signal_handlers_disconnect_by_func(icon_view->priv->model,
                                         G_CALLBACK(xfdesktop_icon_view_model_row_deleted),
                                         icon_view);
}

static void
xfdesktop_icon_view_model_row_inserted(GtkTreeModel *model,
                                       GtkTreePath *path,
                                       GtkTreeIter *iter,
                                       XfdesktopIconView *icon_view)
{
    ViewItem *item = view_item_new(icon_view->priv->model, iter);
    gint idx = gtk_tree_path_get_indices(path)[0];

    DBG("entering, index=%d", gtk_tree_path_get_indices(path)[0]);

    icon_view->priv->items = g_list_insert(icon_view->priv->items, item, idx);

    if (gtk_widget_get_realized(GTK_WIDGET(icon_view))) {
        if (xfdesktop_icon_view_place_item(icon_view, item, TRUE)) {
            DBG("placed new icon at (%d, %d)", item->row, item->col);
        } else {
            DBG("failed to place new icon");
        }
    }
}

static void
xfdesktop_icon_view_model_row_changed(GtkTreeModel *model,
                                      GtkTreePath *path,
                                      GtkTreeIter *iter,
                                      XfdesktopIconView *icon_view)
{
    ViewItem *item = g_list_nth_data(icon_view->priv->items, gtk_tree_path_get_indices(path)[0]);

    if (item != NULL) {
        if (item->pixbuf_surface != NULL) {
            cairo_surface_destroy(item->pixbuf_surface);
            item->pixbuf_surface = NULL;
        }
        xfdesktop_icon_view_invalidate_item(icon_view, item, TRUE);
    }
}

static void
xfdesktop_icon_view_model_row_deleted(GtkTreeModel *model,
                                      GtkTreePath *path,
                                      XfdesktopIconView *icon_view)
{
    GList *item_l = g_list_nth(icon_view->priv->items, gtk_tree_path_get_indices(path)[0]);

    if (item_l != NULL) {
        ViewItem *item = item_l->data;

        if (item->placed) {
            xfdesktop_icon_view_unplace_item(icon_view, item);
        }

        icon_view->priv->items = g_list_delete_link(icon_view->priv->items, item_l);

        view_item_free(item);
    }
}

static void
xfdesktop_icon_view_clear_grid_layout(XfdesktopIconView *icon_view)
{
    if (icon_view->priv->nrows > 0 && icon_view->priv->ncols > 0 && icon_view->priv->grid_layout != NULL) {
        for (gint i = 0; i < icon_view->priv->nrows * icon_view->priv->ncols; ++i) {
            if (icon_view->priv->grid_layout[i] != NULL && icon_view->priv->grid_layout[i] != TOMBSTONE) {
                icon_view->priv->grid_layout[i] = NULL;
            }
        }
    }
}

static void
xfdesktop_icon_view_items_free(XfdesktopIconView *icon_view)
{
    xfdesktop_icon_view_clear_grid_layout(icon_view);

    icon_view->priv->item_under_pointer = NULL;
    icon_view->priv->cursor = NULL;
    icon_view->priv->first_clicked_item = NULL;

    g_list_free(icon_view->priv->selected_items);
    icon_view->priv->selected_items = NULL;

    g_list_free_full(icon_view->priv->items, (GDestroyNotify)view_item_free);
    icon_view->priv->items = NULL;
}


/* public api */


GtkWidget *
xfdesktop_icon_view_new(XfconfChannel *channel)
{
    return g_object_new(XFDESKTOP_TYPE_ICON_VIEW,
                        "channel", channel,
                        NULL);
}

GtkWidget *
xfdesktop_icon_view_new_with_model(XfconfChannel *channel,
                                   GtkTreeModel *model)
{
    g_return_val_if_fail(GTK_IS_TREE_MODEL(model), NULL);

    return g_object_new(XFDESKTOP_TYPE_ICON_VIEW,
                        "channel", channel,
                        "model", model,
                        NULL);
}

static gboolean
xfdesktop_icon_view_validate_column_type(XfdesktopIconView *icon_view,
                                         GtkTreeModel *model,
                                         gint column,
                                         GType required_type,
                                         const gchar *property_name)
{
    if (column == -1) {
        return TRUE;
    } else {
        GType model_col_type = gtk_tree_model_get_column_type(model, column);
        if (!g_type_is_a(model_col_type, required_type)) {
            g_warning("XfdesktopIconView requires %s to be of type %s, but got %s",
                      property_name, g_type_name(required_type), g_type_name(model_col_type));
            return FALSE;
        } else {
            return TRUE;
        }
    }
}

void
xfdesktop_icon_view_set_model(XfdesktopIconView *icon_view,
                              GtkTreeModel *model)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));
    g_return_if_fail(model == NULL || GTK_IS_TREE_MODEL(model));

    if (model == icon_view->priv->model) {
        return;
    }

    if (model != NULL) {
        if (!xfdesktop_icon_view_validate_column_type(icon_view, model, icon_view->priv->pixbuf_column, G_TYPE_ICON, "pixbuf-column")
            || !xfdesktop_icon_view_validate_column_type(icon_view, model, icon_view->priv->text_column, G_TYPE_STRING, "text-column")
            || !xfdesktop_icon_view_validate_column_type(icon_view, model, icon_view->priv->search_column, G_TYPE_STRING, "search-column")
            || !xfdesktop_icon_view_validate_column_type(icon_view, model, icon_view->priv->tooltip_icon_column, G_TYPE_ICON, "tooltip-icon-column")
            || !xfdesktop_icon_view_validate_column_type(icon_view, model, icon_view->priv->tooltip_text_column, G_TYPE_STRING, "tooltip-text-column")
            || !xfdesktop_icon_view_validate_column_type(icon_view, model, icon_view->priv->row_column, G_TYPE_INT, "row-column")
            || !xfdesktop_icon_view_validate_column_type(icon_view, model, icon_view->priv->col_column, G_TYPE_INT, "col-column"))
        {
            return;
        }
    }

    if (icon_view->priv->model != NULL) {
        if (gtk_widget_get_realized(GTK_WIDGET(icon_view))) {
            xfdesktop_icon_view_disconnect_model_signals(icon_view);
        }
        xfdesktop_icon_view_items_free(icon_view);
        g_clear_object(&icon_view->priv->model);
    }

    if (model != NULL) {
        icon_view->priv->model = g_object_ref(model);

        if (gtk_widget_get_realized(GTK_WIDGET(icon_view))) {
            xfdesktop_icon_view_connect_model_signals(icon_view);
            xfdesktop_icon_view_populate_items(icon_view);
        }
    }

    g_object_notify(G_OBJECT(icon_view), "model");

    if (gtk_widget_get_realized(GTK_WIDGET(icon_view))) {
        gtk_widget_queue_draw(GTK_WIDGET(icon_view));
    }
}

GtkTreeModel *
xfdesktop_icon_view_get_model(XfdesktopIconView *icon_view)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view), NULL);
    return icon_view->priv->model;
}

static gboolean
xfdesktop_icon_view_set_column(XfdesktopIconView *icon_view,
                               gint column,
                               gint *column_store_location,
                               GType required_type,
                               const gchar *property_name)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view), FALSE);
    g_return_val_if_fail(column >= -1, FALSE);

    if (*column_store_location == column) {
        return FALSE;
    } else if (icon_view->priv->model != NULL
               && !xfdesktop_icon_view_validate_column_type(icon_view,
                                                            icon_view->priv->model,
                                                            column,
                                                            required_type,
                                                            property_name))
    {
        return FALSE;
    } else {
        *column_store_location = column;
        g_object_notify(G_OBJECT(icon_view), property_name);
        return TRUE;
    }
}

void
xfdesktop_icon_view_set_pixbuf_column(XfdesktopIconView *icon_view,
                                      gint column)
{
    gboolean changed;

    // Delay notify for pixbuf-column until after we set up the column
    g_object_freeze_notify(G_OBJECT(icon_view));

    changed = xfdesktop_icon_view_set_column(icon_view, column, &icon_view->priv->pixbuf_column, G_TYPE_ICON, "pixbuf-column");
    if (changed) {
        xfdesktop_icon_view_invalidate_all(icon_view, TRUE);
    }

    g_object_thaw_notify(G_OBJECT(icon_view));
}

void
xfdesktop_icon_view_set_text_column(XfdesktopIconView *icon_view,
                                    gint column)
{
    gboolean changed;

    // Delay notify for text-column until after we set up the column
    g_object_freeze_notify(G_OBJECT(icon_view));

    changed = xfdesktop_icon_view_set_column(icon_view, column, &icon_view->priv->text_column, G_TYPE_STRING, "text-column");
    if (changed) {
        xfdesktop_icon_view_invalidate_all(icon_view, TRUE);
    }

    g_object_thaw_notify(G_OBJECT(icon_view));
}

void
xfdesktop_icon_view_set_search_column(XfdesktopIconView *icon_view,
                                      gint column)
{
    xfdesktop_icon_view_set_column(icon_view, column, &icon_view->priv->search_column, G_TYPE_STRING, "search-column");
}

void
xfdesktop_icon_view_set_sort_priority_column(XfdesktopIconView *icon_view,
                                             gint column)
{
    xfdesktop_icon_view_set_column(icon_view, column, &icon_view->priv->sort_priority_column, G_TYPE_INT, "sort-priority-column");
}

void
xfdesktop_icon_view_set_tooltip_icon_column(XfdesktopIconView *icon_view,
                                               gint column)
{
    xfdesktop_icon_view_set_column(icon_view, column, &icon_view->priv->tooltip_icon_column, G_TYPE_ICON, "tooltip-icon-column");
}

void
xfdesktop_icon_view_set_tooltip_text_column(XfdesktopIconView *icon_view,
                                            gint column)
{
    xfdesktop_icon_view_set_column(icon_view, column, &icon_view->priv->tooltip_text_column, G_TYPE_STRING, "tooltip-text-column");
}

void
xfdesktop_icon_view_set_row_column(XfdesktopIconView *icon_view,
                                   gint column)
{
    xfdesktop_icon_view_set_column(icon_view, column, &icon_view->priv->row_column, G_TYPE_INT, "row-column");
}

void
xfdesktop_icon_view_set_col_column(XfdesktopIconView *icon_view,
                                   gint column)
{
    xfdesktop_icon_view_set_column(icon_view, column, &icon_view->priv->col_column, G_TYPE_INT, "col-column");
}

void
xfdesktop_icon_view_set_selection_mode(XfdesktopIconView *icon_view,
                                       GtkSelectionMode mode)
{
    GtkSelectionMode new_mode = GTK_SELECTION_SINGLE;

    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));
    g_return_if_fail(mode <= GTK_SELECTION_MULTIPLE);

    switch(mode) {
        case GTK_SELECTION_NONE:
            g_warning("GTK_SELECTION_NONE is not implemented for " \
                      "XfdesktopIconView.  Falling back to " \
                      "GTK_SELECTION_SINGLE.");
            /* fall through */
        case GTK_SELECTION_SINGLE:
            new_mode = GTK_SELECTION_SINGLE;
            icon_view->priv->allow_rubber_banding = FALSE;

            if (icon_view->priv->selected_items != NULL) {
                for(GList *l = icon_view->priv->selected_items->next; l != NULL; l = l->next) {
                    ViewItem *item = l->data;
                    xfdesktop_icon_view_unselect_item_internal(icon_view, item, FALSE);
                }
                g_signal_emit(icon_view, __signals[SIG_ICON_SELECTION_CHANGED], 0);
            }
            break;

        case GTK_SELECTION_BROWSE:
            g_warning("GTK_SELECTION_BROWSE is not implemented for " \
                  "XfdesktopIconView.  Falling back to " \
                  "GTK_SELECTION_MULTIPLE.");
            /* fall through */
        default:
            new_mode = GTK_SELECTION_MULTIPLE;
            icon_view->priv->allow_rubber_banding = TRUE;
            break;
    }

    if (new_mode != icon_view->priv->sel_mode) {
        icon_view->priv->sel_mode = mode;
    }
}

GtkSelectionMode
xfdesktop_icon_view_get_selection_mode(XfdesktopIconView *icon_view)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view),
                         GTK_SELECTION_NONE);

    return icon_view->priv->sel_mode;
}

void
xfdesktop_icon_view_enable_drag_source(XfdesktopIconView *icon_view,
                                       GdkModifierType start_button_mask,
                                       const GtkTargetEntry *targets,
                                       gint n_targets,
                                       GdkDragAction actions)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));

    if(icon_view->priv->drag_source_set) {
        gtk_target_list_unref(icon_view->priv->source_targets);
        icon_view->priv->source_targets = gtk_target_list_new(icon_view_targets,
                                                              icon_view_n_targets);
    }

    icon_view->priv->foreign_source_actions = actions;
    icon_view->priv->foreign_source_mask = start_button_mask;

    gtk_target_list_add_table(icon_view->priv->source_targets, targets,
                              n_targets);

    gtk_drag_source_set(GTK_WIDGET(icon_view), start_button_mask, NULL, 0,
                        GDK_ACTION_MOVE | actions);
    gtk_drag_source_set_target_list(GTK_WIDGET(icon_view),
                                    icon_view->priv->source_targets);

    icon_view->priv->drag_source_set = TRUE;
}

void
xfdesktop_icon_view_enable_drag_dest(XfdesktopIconView *icon_view,
                                     const GtkTargetEntry *targets,
                                     gint n_targets,
                                     GdkDragAction actions)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));

    if(icon_view->priv->drag_dest_set) {
        gtk_target_list_unref(icon_view->priv->dest_targets);
        icon_view->priv->dest_targets = gtk_target_list_new(icon_view_targets,
                                                            icon_view_n_targets);
    }

    icon_view->priv->foreign_dest_actions = actions;

    gtk_target_list_add_table(icon_view->priv->dest_targets, targets,
                              n_targets);

    gtk_drag_dest_set(GTK_WIDGET(icon_view), 0, NULL, 0,
                      GDK_ACTION_MOVE | actions);
    gtk_drag_dest_set_target_list(GTK_WIDGET(icon_view),
                                  icon_view->priv->dest_targets);

    icon_view->priv->drag_dest_set = TRUE;
}

void
xfdesktop_icon_view_unset_drag_source(XfdesktopIconView *icon_view)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));

    if(!icon_view->priv->drag_source_set)
        return;

    if(icon_view->priv->source_targets)
        gtk_target_list_unref(icon_view->priv->source_targets);

    icon_view->priv->source_targets = gtk_target_list_new(icon_view_targets,
                                                              icon_view_n_targets);

    gtk_drag_source_set(GTK_WIDGET(icon_view), 0, NULL, 0, GDK_ACTION_MOVE);
    gtk_drag_source_set_target_list(GTK_WIDGET(icon_view),
                                    icon_view->priv->source_targets);

    icon_view->priv->drag_source_set = FALSE;
}

void
xfdesktop_icon_view_unset_drag_dest(XfdesktopIconView *icon_view)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));

    if(!icon_view->priv->drag_dest_set)
        return;

    if(icon_view->priv->dest_targets)
        gtk_target_list_unref(icon_view->priv->dest_targets);

    icon_view->priv->dest_targets = gtk_target_list_new(icon_view_targets,
                                                        icon_view_n_targets);

    gtk_drag_dest_set(GTK_WIDGET(icon_view), 0, NULL, 0, GDK_ACTION_MOVE);
    gtk_drag_dest_set_target_list(GTK_WIDGET(icon_view),
                                  icon_view->priv->dest_targets);

    icon_view->priv->drag_dest_set = FALSE;
}

static ViewItem *
xfdesktop_icon_view_widget_coords_to_item_internal(XfdesktopIconView *icon_view,
                                                   gint wx,
                                                   gint wy)
{
    gint row, col;

    xfdesktop_xy_to_rowcol(icon_view, wx, wy, &row, &col);
    if (row >= icon_view->priv->nrows || col >= icon_view->priv->ncols || row < 0 || col < 0) {
        return NULL;
    }

    return xfdesktop_icon_view_item_in_slot(icon_view, row, col);
}

gboolean
xfdesktop_icon_view_widget_coords_to_item(XfdesktopIconView *icon_view,
                                          gint wx,
                                          gint wy,
                                          GtkTreeIter *iter)
{
    ViewItem *item;

    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view), FALSE);
    g_return_val_if_fail(icon_view->priv->model != NULL, FALSE);
    g_return_val_if_fail(iter != NULL, FALSE);

    item = xfdesktop_icon_view_widget_coords_to_item_internal(icon_view, wx, wy);
    if (item != NULL) {
        return view_item_get_iter(item, icon_view->priv->model, iter);
    } else {
        return FALSE;
    }
}

GList *
xfdesktop_icon_view_get_selected_items(XfdesktopIconView *icon_view)
{
    GList *paths = NULL;

    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view), NULL);

    for (GList *l = icon_view->priv->selected_items; l != NULL; l = l->next) {
        ViewItem *item = l->data;
        GtkTreePath *path = view_item_get_path(item, icon_view->priv->model);
        if (path != NULL) {
            paths = g_list_prepend(paths, path);
        }
    }

    return g_list_reverse(paths);
}

static ViewItem *
xfdesktop_icon_view_find_item(XfdesktopIconView *icon_view,
                              GtkTreeIter *iter)
{
    ViewItem *item = NULL;
    GtkTreePath *path;

    g_return_val_if_fail(icon_view->priv->model != NULL, NULL);

    path = gtk_tree_model_get_path(icon_view->priv->model, iter);
    if (G_LIKELY(path != NULL)) {
        item = g_list_nth_data(icon_view->priv->items, gtk_tree_path_get_indices(path)[0]);
        gtk_tree_path_free(path);
    } 

    return item;
}

void
xfdesktop_icon_view_select_item(XfdesktopIconView *icon_view,
                                GtkTreeIter *iter)
{
    ViewItem *item;

    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));
    g_return_if_fail(icon_view->priv->model != NULL);
    g_return_if_fail(iter != NULL);

    item = xfdesktop_icon_view_find_item(icon_view, iter);
    if (item != NULL && !item->selected) {
        xfdesktop_icon_view_select_item_internal(icon_view, item, TRUE);
    }
}

void
xfdesktop_icon_view_select_all(XfdesktopIconView *icon_view)
{
    gboolean selected_something = FALSE;

    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));
    g_return_if_fail(xfdesktop_icon_view_get_selection_mode(icon_view) == GTK_SELECTION_MULTIPLE);

    for (GList *l = icon_view->priv->items; l != NULL; l = l->next) {
        ViewItem *item = l->data;
        if (!item->selected) {
            xfdesktop_icon_view_select_item_internal(icon_view, item, FALSE);
            selected_something = TRUE;
        }
    }

    if (selected_something) {
        g_signal_emit(icon_view, __signals[SIG_ICON_SELECTION_CHANGED], 0);
    }
}

void
xfdesktop_icon_view_unselect_item(XfdesktopIconView *icon_view,
                                  GtkTreeIter *iter)
{
    ViewItem *item;

    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));
    g_return_if_fail(icon_view->priv->model != NULL);
    g_return_if_fail(iter != NULL);

    item = xfdesktop_icon_view_find_item(icon_view, iter);
    if (item != NULL && item->selected) {
        xfdesktop_icon_view_unselect_item_internal(icon_view, item, TRUE);
    }
}

void
xfdesktop_icon_view_unselect_all(XfdesktopIconView *icon_view)
{
    gboolean unselected_something = FALSE;

    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));

    for (GList *l = icon_view->priv->items; l != NULL; l = l->next) {
        ViewItem *item = l->data;
        if (item->selected) {
            xfdesktop_icon_view_unselect_item_internal(icon_view, item, FALSE);
            unselected_something = TRUE;
        }
    }

    if (unselected_something) {
        g_signal_emit(icon_view, __signals[SIG_ICON_SELECTION_CHANGED], 0);
    }
}

void
xfdesktop_icon_view_set_item_sensitive(XfdesktopIconView *icon_view,
                                       GtkTreeIter *iter,
                                       gboolean sensitive)
{
    ViewItem *item;

    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));
    g_return_if_fail(iter != NULL);

    item = xfdesktop_icon_view_find_item(icon_view, iter);
    if (item != NULL) {
        if (item->sensitive != sensitive) {
            item->sensitive = sensitive;
            xfdesktop_icon_view_invalidate_item(icon_view, item, FALSE);
        }
    }
}

void
xfdesktop_icon_view_set_icon_size(XfdesktopIconView *icon_view,
                                  gint icon_size)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));

    if(icon_size == icon_view->priv->icon_size)
        return;

    icon_view->priv->icon_size = icon_size;

    if(gtk_widget_get_realized(GTK_WIDGET(icon_view))) {
        xfdesktop_icon_view_invalidate_pixbuf_cache(icon_view);
        xfdesktop_icon_view_size_grid(icon_view);
    }

    g_object_freeze_notify(G_OBJECT(icon_view));
    g_object_notify(G_OBJECT(icon_view), "icon-size");
    g_object_notify(G_OBJECT(icon_view), "icon-width");
    g_object_notify(G_OBJECT(icon_view), "icon-height");
    g_object_thaw_notify(G_OBJECT(icon_view));
}

gint
xfdesktop_icon_view_get_icon_size(XfdesktopIconView *icon_view)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view), 0);
    return icon_view->priv->icon_size;
}

void
xfdesktop_icon_view_set_font_size(XfdesktopIconView *icon_view,
                                  gdouble font_size_points)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));

    if(font_size_points == icon_view->priv->font_size)
        return;

    icon_view->priv->font_size = font_size_points;

    if (icon_view->priv->text_renderer != NULL) {
        g_object_set(icon_view->priv->text_renderer,
                     "size-points", font_size_points,
                     NULL);
    }

    if (icon_view->priv->font_size_set && gtk_widget_get_realized(GTK_WIDGET(icon_view))) {
        xfdesktop_icon_view_size_grid(icon_view);
        xfdesktop_icon_view_invalidate_all(icon_view, TRUE);
    }

    g_object_notify(G_OBJECT(icon_view), "icon-font-size");
}

gdouble
xfdesktop_icon_view_get_font_size(XfdesktopIconView *icon_view)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view), 0.0);
    return icon_view->priv->font_size;
}

void
xfdesktop_icon_view_set_use_font_size(XfdesktopIconView *icon_view,
                                      gboolean use_font_size)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));

    if (use_font_size == icon_view->priv->font_size_set) {
        return;
    }

    icon_view->priv->font_size_set = use_font_size;

    if (icon_view->priv->text_renderer != NULL) {
        g_object_set(icon_view->priv->text_renderer,
                     "size-points-set", use_font_size,
                     NULL);
    }

    if (gtk_widget_get_realized(GTK_WIDGET(icon_view))) {
        xfdesktop_icon_view_size_grid(icon_view);
        xfdesktop_icon_view_invalidate_all(icon_view, TRUE);
    }

    g_object_notify(G_OBJECT(icon_view), "icon-font-size-set");
}

void
xfdesktop_icon_view_set_center_text(XfdesktopIconView *icon_view,
                                    gboolean center_text)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));

    if (center_text == icon_view->priv->center_text)
        return;

    icon_view->priv->center_text = center_text;

    if (icon_view->priv->text_renderer != NULL) {
        g_object_set(icon_view->priv->text_renderer,
                     "alignment", center_text
                     ? PANGO_ALIGN_CENTER
                     : (gtk_widget_get_direction(GTK_WIDGET(icon_view)) == GTK_TEXT_DIR_RTL
                        ? PANGO_ALIGN_RIGHT
                        : PANGO_ALIGN_LEFT),
                     "align-set", TRUE,
                     NULL);
    }

    if (gtk_widget_get_realized(GTK_WIDGET(icon_view))) {
        xfdesktop_icon_view_invalidate_all(icon_view, TRUE);
        gtk_widget_queue_draw(GTK_WIDGET(icon_view));
    }

    g_object_notify(G_OBJECT(icon_view), "icon-center-text");
}

gboolean
xfdesktop_icon_view_get_single_click(XfdesktopIconView *icon_view)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view), FALSE);

    return icon_view->priv->single_click;
}

void
xfdesktop_icon_view_set_single_click(XfdesktopIconView *icon_view,
                                     gboolean single_click)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));

    if (single_click == icon_view->priv->single_click) {
        return;
    }

    icon_view->priv->single_click = single_click;

    if (icon_view->priv->text_renderer != NULL) {
        g_object_set(icon_view->priv->text_renderer,
                     "underline-when-prelit", icon_view->priv->single_click &&
                                              icon_view->priv->single_click_underline_hover,
                     NULL);
    }

    if(gtk_widget_get_realized(GTK_WIDGET(icon_view))) {
        gtk_widget_queue_draw(GTK_WIDGET(icon_view));
    }

    g_object_notify(G_OBJECT(icon_view), "single-click");
}

void
xfdesktop_icon_view_set_single_click_underline_hover(XfdesktopIconView *icon_view,
                                                     gboolean single_click_underline_hover)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));

    if (single_click_underline_hover != icon_view->priv->single_click_underline_hover) {
        icon_view->priv->single_click_underline_hover = single_click_underline_hover;

        if (icon_view->priv->text_renderer != NULL) {
            g_object_set(icon_view->priv->text_renderer,
                         "underline-when-prelit", icon_view->priv->single_click &&
                         icon_view->priv->single_click_underline_hover,
                         NULL);
        }

        if (gtk_widget_get_realized(GTK_WIDGET(icon_view)) && icon_view->priv->item_under_pointer != NULL) {
            ViewItem *item = icon_view->priv->item_under_pointer;
            gtk_widget_queue_draw_area(GTK_WIDGET(icon_view),
                                       item->text_extents.x,
                                       item->text_extents.y,
                                       item->text_extents.width,
                                       item->text_extents.height);
        }

        g_object_notify(G_OBJECT(icon_view), "single-click-underline-hover");
    }
}

void
xfdesktop_icon_view_set_gravity(XfdesktopIconView *icon_view,
                                XfdesktopIconViewGravity gravity)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));

    if (gravity == icon_view->priv->gravity)
        return;

    icon_view->priv->gravity = gravity;

    if(gtk_widget_get_realized(GTK_WIDGET(icon_view))) {
        gtk_widget_queue_draw(GTK_WIDGET(icon_view));
    }

    g_object_notify(G_OBJECT(icon_view), "gravity");
}

void
xfdesktop_icon_view_set_show_tooltips(XfdesktopIconView *icon_view,
                                      gboolean show_tooltips)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));

    if (show_tooltips == icon_view->priv->show_tooltips)
        return;

    icon_view->priv->show_tooltips = show_tooltips;
    g_object_notify(G_OBJECT(icon_view), "show-tooltips");
}

gint
xfdesktop_icon_view_get_tooltip_icon_size(XfdesktopIconView *icon_view)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view), DEFAULT_TOOLTIP_ICON_SIZE);

    if (icon_view->priv->tooltip_icon_size_xfconf > 0) {
        return icon_view->priv->tooltip_icon_size_xfconf;
    } else if (icon_view->priv->tooltip_icon_size_style > 0) {
        return icon_view->priv->tooltip_icon_size_style;
    } else {
        return DEFAULT_TOOLTIP_ICON_SIZE;
    }
}


GtkWidget *
xfdesktop_icon_view_get_window_widget(XfdesktopIconView *icon_view)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view), NULL);

    return icon_view->priv->parent_window;
}
