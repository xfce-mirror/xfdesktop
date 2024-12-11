/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2006-2009,2022-2024 Brian Tarricone, <brian@tarricone.org>
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

#include <cairo.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <glib-object.h>
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

#define ICON_SIZE         (icon_view->icon_size)
#define TEXT_WIDTH        ((icon_view->cell_text_width_proportion) * ICON_SIZE)
#define ICON_WIDTH        (TEXT_WIDTH)
#define SLOT_PADDING      (icon_view->slot_padding)
#define SLOT_SIZE         (TEXT_WIDTH + SLOT_PADDING * 2)
#define SPACING           (icon_view->cell_spacing)
#define LABEL_RADIUS      (icon_view->label_radius)
#define TEXT_HEIGHT       (SLOT_SIZE - ICON_SIZE - SPACING - (SLOT_PADDING * 2) - LABEL_RADIUS)
#define MIN_MARGIN        8

#define KEYBOARD_NAVIGATION_TIMEOUT  1500

#define XFDESKTOP_ICON_NAME "XFDESKTOP_ICON"

#define LABEL_BG_COLOR_CSS_FMT \
    "XfdesktopIconView.view.label {" \
    "    background-color: rgba(%u, %u, %u, %s);" \
    "}"

#define GDK_RECT_FROM_CAIRO(crect) (GdkRectangle){ .x = (crect)->x, .y = (crect)->y, .width = (crect)->width, .height = (crect)->height }
#define CAIRO_RECT_INT_FROM_GDK(grect) (cairo_rectangle_int_t){ .x = (crect)->x, .y = (crect)->y, .width = (crect)->width, .height = (crect)->height }

#if defined(DEBUG) && DEBUG > 0
#define DUMP_GRID_LAYOUT(icon_view) \
{\
    gint my_i, my_maxi;\
    \
    DBG("grid layout dump:"); \
    my_maxi = icon_view->nrows * icon_view->ncols;\
    for(my_i = 0; my_i < my_maxi; my_i++)\
        g_printerr("%c ", icon_view->grid_layout[my_i] ? '1' : '0');\
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
    SIG_MOVE_CURSOR,
    SIG_RESIZE_EVENT,

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
    PROP_SCREEN,
    PROP_MODEL,
    PROP_ICON_SIZE,
    PROP_ICON_WIDTH,
    PROP_ICON_HEIGHT,
    PROP_ICON_FONT_SIZE,
    PROP_ICON_FONT_SIZE_SET,
    PROP_ICON_CENTER_TEXT,
    PROP_ICON_LABEL_FG_COLOR,
    PROP_ICON_LABEL_FG_COLOR_SET,
    PROP_ICON_LABEL_BG_COLOR,
    PROP_ICON_LABEL_BG_COLOR_SET,
    PROP_SHOW_TOOLTIPS,
    PROP_SINGLE_CLICK,
    PROP_SINGLE_CLICK_UNDERLINE_HOVER,
    PROP_GRAVITY,
    PROP_PIXBUF_COLUMN,
    PROP_ICON_OPACITY_COLUMN,
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
    cairo_region_t *icon_slot_region;

    cairo_surface_t *pixbuf_surface;

    guint32 has_iter:1;
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
    item->has_iter = (gtk_tree_model_get_flags(model) & GTK_TREE_MODEL_ITERS_PERSIST) != 0;
    item->sensitive = TRUE;
    item->icon_slot_region = cairo_region_create();

    if (item->has_iter) {
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

    if (item->has_iter) {
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

    if (item->has_iter) {
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
    if (!item->has_iter && item->ref.row_ref != NULL) {
        gtk_tree_row_reference_free(item->ref.row_ref);
    }
    cairo_region_destroy(item->icon_slot_region);
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

    return sort_type == GTK_SORT_ASCENDING
        ? g_utf8_collate(al, bl)
        : g_utf8_collate(bl, al);
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

struct _XfdesktopIconView {
    GtkEventBox parent_instance;

    XfwScreen *screen;

    GtkTreeModel *model;
    gint pixbuf_column;
    gint icon_opacity_column;
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

    GdkRGBA label_fg_color;
    gboolean label_fg_color_set;

    GdkRGBA label_bg_color;
    gboolean label_bg_color_set;
    GtkCssProvider *label_bg_color_provider;

    GList *items;  // ViewItem
    GList *selected_items;  // ViewItem

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

    GdkEventButton *drag_timer_event;
    guint drag_timer_id;

    XfconfChannel *channel;

    /* element-type gunichar */
    GArray *keyboard_navigation_state;
    guint keyboard_navigation_state_timeout;

    gboolean draw_focus;
    ViewItem *cursor;
    ViewItem *first_clicked_item;
    ViewItem *item_under_pointer;

    GtkTargetList *source_targets;
    GtkTargetList *dest_targets;

    gboolean drag_source_set;
    GdkDragAction foreign_source_actions;
    GdkModifierType foreign_source_mask;

    gboolean drag_dest_set;
    GdkDragAction foreign_dest_actions;

    gboolean drag_dropped;
    gint drag_drop_row;
    gint drag_drop_col;
    gint highlight_row;
    gint highlight_col;

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

typedef struct {
    gint nrows;
    gint ncols;

    gint xspacing;
    gint yspacing;

    gint xmargin;
    gint ymargin;
} GridParams;

typedef struct {
    /*< public >*/
    // To consumers, they think they are just getting this GtkTreeIter pointer.
    GtkTreeIter iter;

    /*< private >*/
    ViewItem *item;
    gint dest_row;
    gint dest_col;
} XfdesktopDraggedIcon;

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
                                                 GdkEventButton *evt);
static gboolean xfdesktop_icon_view_button_release(GtkWidget *widget,
                                                   GdkEventButton *evt);
static gboolean xfdesktop_icon_view_key_press(GtkWidget *widget,
                                              GdkEventKey *evt);
static gboolean xfdesktop_icon_view_focus_in(GtkWidget *widget,
                                             GdkEventFocus *evt);
static gboolean xfdesktop_icon_view_focus_out(GtkWidget *widget,
                                              GdkEventFocus *evt);
static gboolean xfdesktop_icon_view_motion_notify(GtkWidget *widget,
                                                  GdkEventMotion *evt);
static gboolean xfdesktop_icon_view_leave_notify(GtkWidget *widget,
                                                 GdkEventCrossing *evt);
static void xfdesktop_icon_view_style_updated(GtkWidget *widget);
static void xfdesktop_icon_view_size_allocate(GtkWidget *widget,
                                              GtkAllocation *allocation);
static void xfdesktop_icon_view_realize(GtkWidget *widget);
static void xfdesktop_icon_view_unrealize(GtkWidget *widget);
static gboolean xfdesktop_icon_view_draw(GtkWidget *widget,
                                         cairo_t *cr);
static void xfdesktop_icon_view_drag_begin(GtkWidget *widget,
                                           GdkDragContext *contest);
static void free_dragged_icons(gpointer data);
static void xfdesktop_icon_view_drag_data_get(GtkWidget *widget,
                                              GdkDragContext *context,
                                              GtkSelectionData *data,
                                              guint info,
                                              guint time);
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

static void xfdesktop_icon_view_add_move_binding(GtkBindingSet *binding_set,
                                                 guint keyval,
                                                 guint modmask,
                                                 GtkMovementStep step,
                                                 gint count);

static void xfdesktop_icon_view_populate_items(XfdesktopIconView *icon_view);
static gboolean xfdesktop_icon_view_place_item(XfdesktopIconView *icon_view,
                                               ViewItem *item,
                                               gboolean honor_model_position);
static gboolean xfdesktop_icon_view_place_item_in_grid_at(XfdesktopIconView *icon_view,
                                                          ViewItem **grid_layout,
                                                          ViewItem *item,
                                                          gint row,
                                                          gint col);
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
static void xfdesktop_icon_view_invalidate_item_text(XfdesktopIconView *icon_view,
                                                     ViewItem *item);

static void xfdesktop_icon_view_invalidate_pixbuf_cache(XfdesktopIconView *icon_view);

static void xfdesktop_icon_view_select_item_internal(XfdesktopIconView *icon_view,
                                                     ViewItem *item,
                                                     gboolean emit_signal);
static void xfdesktop_icon_view_unselect_item_internal(XfdesktopIconView *icon_view,
                                                       ViewItem *item,
                                                       gboolean emit_signal);

static void xfdesktop_icon_view_size_grid(XfdesktopIconView *icon_view);
static void xfdesktop_icon_view_clear_grid_layout(XfdesktopIconView *icon_view);
static inline ViewItem *xfdesktop_icon_view_item_in_grid_slot(XfdesktopIconView *icon_view,
                                                              ViewItem **grid_layout,
                                                              gint row,
                                                              gint col);
static inline ViewItem *xfdesktop_icon_view_item_in_slot(XfdesktopIconView *icon_view,
                                                         gint row,
                                                         gint col);
static ViewItem *xfdesktop_icon_view_widget_coords_to_item_internal(XfdesktopIconView *icon_view,
                                                                    gint wx,
                                                                    gint wy);

static gboolean xfdesktop_icon_view_get_next_free_grid_position_for_grid(XfdesktopIconView *icon_view,
                                                                         ViewItem **grid_layout,
                                                                         gint row,
                                                                         gint col,
                                                                         gint *next_row,
                                                                         gint *next_col);
static gint xfdesktop_check_icon_clicked(gconstpointer data,
                                         gconstpointer user_data);

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

static gboolean xfdesktop_icon_view_move_cursor(XfdesktopIconView *icon_view,
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

static void xfdesktop_icon_view_cancel_keyboard_navigation(XfdesktopIconView *icon_view);


static const GtkTargetEntry icon_view_targets[] = {
    { XFDESKTOP_ICON_NAME, GTK_TARGET_SAME_APP, TARGET_XFDESKTOP_ICON },
};
static const gint icon_view_n_targets = 1;

static guint __signals[SIG_N_SIGNALS] = { 0, };

static struct {
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
    { DESTKOP_ICONS_LABEL_TEXT_COLOR_PROP, G_TYPE_INVALID, "icon-label-fg-color" },
    { DESTKOP_ICONS_CUSTOM_LABEL_TEXT_COLOR_PROP, G_TYPE_BOOLEAN, "icon-label-fg-color-set" },
    { DESTKOP_ICONS_LABEL_BG_COLOR_PROP, G_TYPE_INVALID, "icon-label-bg-color" },
    { DESTKOP_ICONS_CUSTOM_LABEL_BG_COLOR_PROP, G_TYPE_BOOLEAN, "icon-label-bg-color-set" },
};


G_DEFINE_TYPE(XfdesktopIconView, xfdesktop_icon_view, GTK_TYPE_EVENT_BOX)


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
    widget_class->drag_data_get = xfdesktop_icon_view_drag_data_get;
    widget_class->drag_data_received = xfdesktop_icon_view_drag_data_received;

    widget_class->motion_notify_event = xfdesktop_icon_view_motion_notify;
    widget_class->leave_notify_event = xfdesktop_icon_view_leave_notify;
    widget_class->key_press_event = xfdesktop_icon_view_key_press;
    widget_class->button_press_event = xfdesktop_icon_view_button_press;
    widget_class->button_release_event = xfdesktop_icon_view_button_release;
    widget_class->focus_in_event = xfdesktop_icon_view_focus_in;
    widget_class->focus_out_event = xfdesktop_icon_view_focus_out;

    __signals[SIG_ICON_SELECTION_CHANGED] = g_signal_new("icon-selection-changed",
                                                         XFDESKTOP_TYPE_ICON_VIEW,
                                                         G_SIGNAL_RUN_LAST,
                                                         0,
                                                         NULL, NULL,
                                                         g_cclosure_marshal_VOID__VOID,
                                                         G_TYPE_NONE, 0);

    __signals[SIG_ICON_ACTIVATED] = g_signal_new("icon-activated",
                                                 XFDESKTOP_TYPE_ICON_VIEW,
                                                 G_SIGNAL_RUN_LAST,
                                                 0,
                                                 NULL, NULL,
                                                 g_cclosure_marshal_VOID__VOID,
                                                 G_TYPE_NONE, 0);

    /**
     * XfdesktopIconView::icon-moved:
     * @icon_view: the destination #XfdesktopIconView.
     * @source_icon_view: the source #XfdesktopIconView.
     * @source_iter: the #GtkTreeIter for the icon with respect to @source_icon_view.
     * @dest_row: the new row on @icon_view.
     * @dest_col: the new row on @icon_view.
     *
     * Emitted when @icon_view has recieved icons from @source_icon_view.
     * @source_iter refers to the icon with respect to @source_icon_view's
     * model.  (@dest_row, @dest_col) is the new location on @icon_view.
     **/
    __signals[SIG_ICON_MOVED] = g_signal_new("icon-moved",
                                             XFDESKTOP_TYPE_ICON_VIEW,
                                             G_SIGNAL_RUN_LAST,
                                             0,
                                             NULL, NULL,
                                             xfdesktop_marshal_VOID__OBJECT_BOXED_INT_INT,
                                             G_TYPE_NONE, 4,
                                             XFDESKTOP_TYPE_ICON_VIEW,
                                             GTK_TYPE_TREE_ITER,
                                             G_TYPE_INT,
                                             G_TYPE_INT);

    __signals[SIG_QUERY_ICON_TOOLTIP] = g_signal_new("query-icon-tooltip",
                                                     XFDESKTOP_TYPE_ICON_VIEW,
                                                     G_SIGNAL_RUN_LAST,
                                                     0,
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
                                                    0,
                                                    NULL, NULL,
                                                    xfdesktop_marshal_VOID__INT_INT,
                                                    G_TYPE_NONE, 2,
                                                    G_TYPE_INT,
                                                    G_TYPE_INT);

    __signals[SIG_END_GRID_RESIZE] = g_signal_new(I_("end-grid-resize"),
                                                    XFDESKTOP_TYPE_ICON_VIEW,
                                                    G_SIGNAL_RUN_LAST,
                                                    0,
                                                    NULL, NULL,
                                                    g_cclosure_marshal_VOID__VOID,
                                                    G_TYPE_NONE, 0);

    __signals[SIG_MOVE_CURSOR] = g_signal_new(I_("move-cursor"),
                                              XFDESKTOP_TYPE_ICON_VIEW,
                                              G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                              0,
                                              NULL, NULL,
                                              xfdesktop_marshal_BOOLEAN__ENUM_INT,
                                              G_TYPE_BOOLEAN, 2,
                                              GTK_TYPE_MOVEMENT_STEP,
                                              G_TYPE_INT);

    __signals[SIG_RESIZE_EVENT] = g_signal_new(I_("resize-event"),
                                               XFDESKTOP_TYPE_ICON_VIEW,
                                               G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                               0,
                                               NULL, NULL,
                                               g_cclosure_marshal_VOID__VOID,
                                               G_TYPE_NONE, 0);

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

    g_object_class_install_property(gobject_class, PROP_CHANNEL,
                                    g_param_spec_object("channel",
                                                        "channel",
                                                        "channel",
                                                        XFCONF_TYPE_CHANNEL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class,
                                    PROP_SCREEN,
                                    g_param_spec_object("screen",
                                                        "screen",
                                                        "XfwScreen",
                                                        XFW_TYPE_SCREEN,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_MODEL,
                                    g_param_spec_object("model",
                                                        "model",
                                                        "model",
                                                        GTK_TYPE_TREE_MODEL,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_ICON_SIZE,
                                    g_param_spec_int("icon-size",
                                                     "icon size",
                                                     "icon size",
                                                     MIN_ICON_SIZE, MAX_ICON_SIZE, DEFAULT_ICON_SIZE,
                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

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
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_ICON_FONT_SIZE_SET,
                                    g_param_spec_boolean("icon-font-size-set",
                                                         "icon font size set",
                                                         "icon font size set",
                                                         DEFAULT_ICON_FONT_SIZE_SET,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_ICON_CENTER_TEXT,
                                    g_param_spec_boolean("icon-center-text",
                                                         "icon center text",
                                                         "icon center text",
                                                         DEFAULT_ICON_CENTER_TEXT,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class,
                                    PROP_ICON_LABEL_FG_COLOR,
                                    g_param_spec_boxed("icon-label-fg-color",
                                                       "icon-label-fg-color",
                                                       "icon label foreground color",
                                                       GDK_TYPE_RGBA,
                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class,
                                    PROP_ICON_LABEL_FG_COLOR_SET,
                                    g_param_spec_boolean("icon-label-fg-color-set",
                                                         "icon-label-fg-color-set",
                                                         "icon label foreground color set",
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class,
                                    PROP_ICON_LABEL_BG_COLOR,
                                    g_param_spec_boxed("icon-label-bg-color",
                                                       "icon-label-bg-color",
                                                       "icon label background color",
                                                       GDK_TYPE_RGBA,
                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class,
                                    PROP_ICON_LABEL_BG_COLOR_SET,
                                    g_param_spec_boolean("icon-label-bg-color-set",
                                                         "icon-label-bg-color-set",
                                                         "icon label background color set",
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_SHOW_TOOLTIPS,
                                    g_param_spec_boolean("show-tooltips",
                                                         "show tooltips",
                                                         "show tooltips on icon hover",
                                                         DEFAULT_SHOW_TOOLTIPS,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_SINGLE_CLICK,
                                    g_param_spec_boolean("single-click",
                                                         "single-click",
                                                         "single-click",
                                                         DEFAULT_SINGLE_CLICK,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_SINGLE_CLICK_UNDERLINE_HOVER,
                                    g_param_spec_boolean("single-click-underline-hover",
                                                         "single-click-underline-hover",
                                                         "single-click-underline-hover",
                                                         DEFAULT_SINGLE_CLICK_ULINE,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_GRAVITY,
                                    g_param_spec_int("gravity",
                                                     "gravity",
                                                     "set gravity of icons placement",
                                                     MIN_GRAVITY, MAX_GRAVITY, DEFAULT_GRAVITY,
                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
#define DECL_COLUMN_PROP(prop_id, name) \
    g_object_class_install_property(gobject_class, \
                                    prop_id, \
                                    g_param_spec_int(name, \
                                                     name, \
                                                     name, \
                                                     -1, \
                                                     G_MAXINT, \
                                                     -1, \
                                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS))

    // NB: be sure to initialize these to -1 in xfdesktop_icon_view_init()
    DECL_COLUMN_PROP(PROP_PIXBUF_COLUMN, "pixbuf-column");
    DECL_COLUMN_PROP(PROP_ICON_OPACITY_COLUMN, "icon-opacity-column");
    DECL_COLUMN_PROP(PROP_TEXT_COLUMN, "text-column");
    DECL_COLUMN_PROP(PROP_SEARCH_COLUMN, "search-column");
    DECL_COLUMN_PROP(PROP_SORT_PRIORITY_COLUMN, "sort-priority-column");
    DECL_COLUMN_PROP(PROP_TOOLTIP_ICON_COLUMN, "tooltip-icon-column");
    DECL_COLUMN_PROP(PROP_TOOLTIP_TEXT_COLUMN, "tooltip-text-column");
    DECL_COLUMN_PROP(PROP_ROW_COLUMN, "row-column");
    DECL_COLUMN_PROP(PROP_COL_COLUMN, "col-column");

#undef DECL_COLUMN_PROP

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

    for (gsize i = 0; i < G_N_ELEMENTS(setting_bindings); ++i) {
        if (strcmp(setting_bindings[i].setting, DESTKOP_ICONS_LABEL_TEXT_COLOR_PROP) == 0) {
            setting_bindings[i].setting_type = G_TYPE_PTR_ARRAY;
        } else if (strcmp(setting_bindings[i].setting, DESTKOP_ICONS_LABEL_BG_COLOR_PROP) == 0) {
            setting_bindings[i].setting_type = G_TYPE_PTR_ARRAY;
        }
    }
}

static void
xfdesktop_icon_view_init(XfdesktopIconView *icon_view)
{
    icon_view->pixbuf_column = -1;
    icon_view->icon_opacity_column = -1;
    icon_view->text_column = -1;
    icon_view->search_column = -1;
    icon_view->sort_priority_column = -1;
    icon_view->tooltip_icon_column = -1;
    icon_view->tooltip_text_column = -1;
    icon_view->row_column = -1;
    icon_view->col_column = -1;

    icon_view->icon_size = DEFAULT_ICON_SIZE;
    icon_view->font_size = DEFAULT_ICON_FONT_SIZE;
    icon_view->font_size_set = DEFAULT_ICON_FONT_SIZE_SET;
    icon_view->label_fg_color = (GdkRGBA){
        .red = 1.0,
        .green = 1.0,
        .blue = 1.0,
        .alpha = 1.0,
    };
    icon_view->label_fg_color_set = FALSE;
    icon_view->label_bg_color = (GdkRGBA){
        .red = 0.0,
        .green = 0.0,
        .blue = 0.0,
        .alpha = 0.5,
    };
    icon_view->label_bg_color_set = FALSE;
    icon_view->gravity = DEFAULT_GRAVITY;
    icon_view->show_tooltips = DEFAULT_SHOW_TOOLTIPS;
    icon_view->tooltip_icon_size_xfconf = 0;
    icon_view->tooltip_icon_size_style = 0;

    icon_view->allow_rubber_banding = TRUE;

    icon_view->drag_drop_row = -1;
    icon_view->drag_drop_col = -1;
    icon_view->highlight_row = -1;
    icon_view->highlight_col = -1;

    icon_view->draw_focus = TRUE;

    icon_view->icon_renderer = gtk_cell_renderer_pixbuf_new();
    icon_view->text_renderer = xfdesktop_cell_renderer_icon_label_new();

    PangoAttrList *attr_list = NULL;
#if PANGO_VERSION_CHECK (1, 44, 0)
    attr_list = pango_attr_list_new();
    {
        PangoAttribute *attr = pango_attr_insert_hyphens_new(FALSE);
        attr->start_index = 0;
        attr->end_index = -1;
        pango_attr_list_change(attr_list, attr);
    }
#endif

    g_object_set(icon_view->text_renderer,
                 "attributes", attr_list,
                 "underline-when-prelit", icon_view->single_click && icon_view->single_click_underline_hover,
                 "wrap-mode", PANGO_WRAP_WORD_CHAR,
                 "xalign", (gfloat)0.5,
                 "yalign", (gfloat)0.0,
                 NULL);

    if (attr_list != NULL) {
        pango_attr_list_unref(attr_list);
    }
}

static void
xfdesktop_icon_view_constructed(GObject *object)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(object);
    GtkStyleContext *context;

    G_OBJECT_CLASS(xfdesktop_icon_view_parent_class)->constructed(object);

    icon_view->source_targets = gtk_target_list_new(icon_view_targets, icon_view_n_targets);
    xfdesktop_icon_view_enable_drag_source(icon_view, GDK_BUTTON1_MASK, NULL, 0, GDK_ACTION_MOVE);

    icon_view->dest_targets = gtk_target_list_new(icon_view_targets, icon_view_n_targets);
    xfdesktop_icon_view_enable_drag_dest(icon_view, NULL, 0, GDK_ACTION_MOVE);

    g_signal_connect(icon_view, "move-cursor",
                     G_CALLBACK(xfdesktop_icon_view_move_cursor), NULL);

    g_object_bind_property(icon_view, "show-tooltips", icon_view, "has-tooltip", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
    g_signal_connect(G_OBJECT(icon_view), "query-tooltip",
                     G_CALLBACK(xfdesktop_icon_view_show_tooltip), NULL);
    icon_view->tooltip_icon_size_xfconf = xfconf_channel_get_double(icon_view->channel, DESKTOP_ICONS_TOOLTIP_SIZE_PROP, 0);
    g_signal_connect(icon_view->channel, "property-changed::" DESKTOP_ICONS_TOOLTIP_SIZE_PROP,
                     G_CALLBACK(xfdesktop_icon_view_xfconf_tooltip_icon_size_changed), icon_view);

    gtk_widget_set_has_window(GTK_WIDGET(icon_view), TRUE);
    gtk_widget_set_can_focus(GTK_WIDGET(icon_view), TRUE);
    gtk_widget_add_events(GTK_WIDGET(icon_view), GDK_POINTER_MOTION_HINT_MASK
                          | GDK_KEY_PRESS_MASK
                          | GDK_BUTTON_PRESS_MASK
                          | GDK_BUTTON_RELEASE_MASK
                          | GDK_FOCUS_CHANGE_MASK
                          | GDK_EXPOSURE_MASK
                          | GDK_LEAVE_NOTIFY_MASK
                          | GDK_POINTER_MOTION_MASK);

    context = gtk_widget_get_style_context(GTK_WIDGET(icon_view));
    gtk_style_context_add_class(context, GTK_STYLE_CLASS_VIEW);

    for (gsize i = 0; i < G_N_ELEMENTS(setting_bindings); ++i) {
        xfconf_g_property_bind(icon_view->channel,
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

    if (icon_view->drag_timer_id != 0) {
        g_source_remove(icon_view->drag_timer_id);
    }

    if (icon_view->keyboard_navigation_state_timeout != 0) {
        g_source_remove(icon_view->keyboard_navigation_state_timeout);
        icon_view->keyboard_navigation_state_timeout = 0;
    }
    if (icon_view->keyboard_navigation_state != NULL) {
        g_array_free(icon_view->keyboard_navigation_state, TRUE);
        icon_view->keyboard_navigation_state = NULL;
    }

    if (icon_view->channel != NULL) {
        g_signal_handlers_disconnect_by_data(icon_view->channel, icon_view);
        g_clear_object(&icon_view->channel);
    }

    xfdesktop_icon_view_set_model(icon_view, NULL);  // Call so ->items are freed too

    G_OBJECT_CLASS(xfdesktop_icon_view_parent_class)->dispose(obj);
}

static void
xfdesktop_icon_view_finalize(GObject *obj)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(obj);

    if (icon_view->label_bg_color_provider != NULL) {
        gtk_style_context_remove_provider_for_screen(gdk_screen_get_default(),
                                                     GTK_STYLE_PROVIDER(icon_view->label_bg_color_provider));
        g_object_unref(icon_view->label_bg_color_provider);
    }

    gtk_target_list_unref(icon_view->source_targets);
    gtk_target_list_unref(icon_view->dest_targets);

    g_object_unref(icon_view->icon_renderer);
    g_object_unref(icon_view->text_renderer);

    g_object_unref(icon_view->screen);

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
            icon_view->channel = g_value_dup_object(value);
            break;

        case PROP_SCREEN:
            icon_view->screen = g_value_dup_object(value);
            break;

        case PROP_MODEL:
            xfdesktop_icon_view_set_model(icon_view, g_value_get_object(value));
            break;

        case PROP_PIXBUF_COLUMN:
            xfdesktop_icon_view_set_pixbuf_column(icon_view, g_value_get_int(value));
            break;

        case PROP_ICON_OPACITY_COLUMN:
            xfdesktop_icon_view_set_icon_opacity_column(icon_view, g_value_get_int(value));
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

        case PROP_ICON_LABEL_FG_COLOR:
            xfdesktop_icon_view_set_icon_label_fg_color(icon_view, g_value_get_boxed(value));
            break;

        case PROP_ICON_LABEL_FG_COLOR_SET:
            xfdesktop_icon_view_set_use_icon_label_fg_color(icon_view, g_value_get_boolean(value));
            break;

        case PROP_ICON_LABEL_BG_COLOR:
            xfdesktop_icon_view_set_icon_label_bg_color(icon_view, g_value_get_boxed(value));
            break;

        case PROP_ICON_LABEL_BG_COLOR_SET:
            xfdesktop_icon_view_set_use_icon_label_bg_color(icon_view, g_value_get_boolean(value));
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
            g_value_set_object(value, icon_view->channel);
            break;

        case PROP_SCREEN:
            g_value_set_object(value, icon_view->screen);
            break;

        case PROP_MODEL:
            g_value_set_object(value, icon_view->model);
            break;

        case PROP_PIXBUF_COLUMN:
            g_value_set_int(value, icon_view->pixbuf_column);
            break;

        case PROP_ICON_OPACITY_COLUMN:
            g_value_set_int(value, icon_view->icon_opacity_column);
            break;

        case PROP_TEXT_COLUMN:
            g_value_set_int(value, icon_view->text_column);
            break;

        case PROP_SEARCH_COLUMN:
            g_value_set_int(value, icon_view->search_column);
            break;

        case PROP_SORT_PRIORITY_COLUMN:
            g_value_set_int(value, icon_view->sort_priority_column);
            break;

        case PROP_TOOLTIP_ICON_COLUMN:
            g_value_set_int(value, icon_view->tooltip_icon_column);
            break;

        case PROP_TOOLTIP_TEXT_COLUMN:
            g_value_set_int(value, icon_view->tooltip_text_column);
            break;

        case PROP_ROW_COLUMN:
            g_value_set_int(value, icon_view->row_column);
            break;

        case PROP_COL_COLUMN:
            g_value_set_int(value, icon_view->col_column);
            break;

        case PROP_ICON_SIZE:
            g_value_set_int(value, icon_view->icon_size);
            break;

        case PROP_ICON_WIDTH:
            g_value_set_int(value, ICON_WIDTH);
            break;

        case PROP_ICON_HEIGHT:
            g_value_set_int(value, ICON_SIZE);
            break;

        case PROP_ICON_FONT_SIZE:
            g_value_set_double(value, icon_view->font_size);
            break;

        case PROP_ICON_FONT_SIZE_SET:
            g_value_set_boolean(value, icon_view->font_size_set);
            break;

        case PROP_ICON_CENTER_TEXT:
            g_value_set_boolean(value, icon_view->center_text);
            break;

        case PROP_ICON_LABEL_FG_COLOR:
            g_value_set_boxed(value, &icon_view->label_fg_color);
            break;

        case PROP_ICON_LABEL_FG_COLOR_SET:
            g_value_set_boolean(value, icon_view->label_fg_color_set);
            break;

        case PROP_ICON_LABEL_BG_COLOR:
            g_value_set_boxed(value, &icon_view->label_bg_color);
            break;

        case PROP_ICON_LABEL_BG_COLOR_SET:
            g_value_set_boolean(value, icon_view->label_bg_color_set);
            break;

        case PROP_SINGLE_CLICK:
            g_value_set_boolean(value, icon_view->single_click);
            break;

        case PROP_SINGLE_CLICK_UNDERLINE_HOVER:
            g_value_set_boolean(value, icon_view->single_click_underline_hover);
            break;

        case PROP_GRAVITY:
            g_value_set_int(value, icon_view->gravity);
            break;

        case PROP_SHOW_TOOLTIPS:
            g_value_set_boolean(value, icon_view->show_tooltips);
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
    for (GList *l = icon_view->items; l != NULL; l = l->next) {
        xfdesktop_icon_view_invalidate_item(icon_view, l->data, recalc_extents);
    }
}

static void
xfdesktop_icon_view_invalidate_pixbuf_cache(XfdesktopIconView *icon_view)
{
    for (GList *l = icon_view->items; l != NULL; l = l->next) {
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
xfdesktop_icon_view_clear_drag_event(XfdesktopIconView *icon_view) {
    DBG("unsetting stuff");
    icon_view->control_click = FALSE;
    icon_view->double_click = FALSE;
    icon_view->maybe_begin_drag = FALSE;
    icon_view->definitely_dragging = FALSE;
    xfdesktop_icon_view_unset_highlight(icon_view);

    if (icon_view->definitely_rubber_banding) {
        icon_view->definitely_rubber_banding = FALSE;
        gtk_widget_queue_draw_area(GTK_WIDGET(icon_view),
                                   icon_view->band_rect.x,
                                   icon_view->band_rect.y,
                                   icon_view->band_rect.width,
                                   icon_view->band_rect.height);
    }
}

static gboolean
context_menu_drag_timeout(gpointer data) {
    TRACE("entering");

    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(data);
    xfdesktop_icon_view_clear_drag_event(icon_view);

    GdkEventButton *evt = icon_view->drag_timer_event;
    gint orig_x = evt->x;
    gint orig_y = evt->y;

    GtkWidget *parent = gtk_widget_get_parent(GTK_WIDGET(icon_view));
    gboolean ret = FALSE;
    while (parent != NULL && !ret) {
        if ((gtk_widget_get_events(parent) & GDK_BUTTON_PRESS_MASK) != 0) {
            g_object_unref(evt->window);
            evt->window = g_object_ref(gtk_widget_get_window(parent));

            gint x, y;
            if (gtk_widget_translate_coordinates(GTK_WIDGET(icon_view), parent, orig_x, orig_y, &x, &y)) {
                evt->x = x;
                evt->y = y;
            }

            g_signal_emit_by_name(parent, "button-press-event", evt, &ret);
        }

        parent = gtk_widget_get_parent(parent);
    }

    return FALSE;
}

static void
context_menu_drag_timeout_destroy(XfdesktopIconView *icon_view) {
    icon_view->drag_timer_id = 0;
    gdk_event_free((GdkEvent *)icon_view->drag_timer_event);
    icon_view->drag_timer_event = NULL;
}

static void
xfdesktop_icon_view_set_cursor(XfdesktopIconView *icon_view, ViewItem *item, gboolean from_keyboard) {
    if (icon_view->cursor != NULL && icon_view->draw_focus) {
        xfdesktop_icon_view_invalidate_item(icon_view, icon_view->cursor, FALSE);
    }
    icon_view->cursor = item;
    icon_view->draw_focus = from_keyboard;
}

static void
update_item_under_pointer(XfdesktopIconView *icon_view, GdkWindow *event_window, gdouble x, gdouble y) {
    ViewItem *old_item_under_pointer = icon_view->item_under_pointer;

    icon_view->item_under_pointer = NULL;
    for (GList *l = icon_view->items; l != NULL; l = l->next) {
        ViewItem *item = l->data;

        if (item->placed && cairo_region_contains_point(item->icon_slot_region, x, y)) {
            icon_view->item_under_pointer = item;
            break;
        }
    }

    if (old_item_under_pointer != icon_view->item_under_pointer) {
        if (old_item_under_pointer != NULL) {
            xfdesktop_icon_view_invalidate_item(icon_view, old_item_under_pointer, FALSE);
        }

        if (icon_view->item_under_pointer != NULL) {
            if (icon_view->single_click) {
                GdkDisplay *display = gtk_widget_get_display(GTK_WIDGET(icon_view));
                GdkCursor *cursor = gdk_cursor_new_for_display(display, GDK_HAND2);
                gdk_window_set_cursor(event_window, cursor);
                g_object_unref(cursor);
            }

            xfdesktop_icon_view_invalidate_item(icon_view, icon_view->item_under_pointer, FALSE);
        } else {
            if (icon_view->single_click) {
                gdk_window_set_cursor(event_window, NULL);
            }
        }
    }
}

static gboolean
xfdesktop_icon_view_button_press(GtkWidget *widget, GdkEventButton *evt) {
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);

    DBG("entering");

    xfdesktop_icon_view_cancel_keyboard_navigation(icon_view);

    gtk_widget_grab_focus(widget);

    update_item_under_pointer(icon_view, evt->window, evt->x, evt->y);

    if(evt->type == GDK_BUTTON_PRESS) {
        GList *item_l;

        /* Clear drag event if ongoing */
        if(evt->button == 2 || evt->button == 3)
            xfdesktop_icon_view_clear_drag_event(icon_view);

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

        item_l = g_list_find_custom(icon_view->items, evt, (GCompareFunc)xfdesktop_check_icon_clicked);
        if (item_l != NULL) {
            ViewItem *item = item_l->data;

            if (item->selected) {
                /* clicked an already-selected icon */

                if(evt->state & GDK_CONTROL_MASK)
                    icon_view->control_click = TRUE;

                xfdesktop_icon_view_set_cursor(icon_view, item, FALSE);
            } else {
                /* clicked a non-selected icon */
                if (icon_view->sel_mode != GTK_SELECTION_MULTIPLE || (evt->state & GDK_CONTROL_MASK) == 0) {
                    /* unselect all of the other icons if we haven't held
                     * down the ctrl key.  we'll handle shift in the next block,
                     * but for shift we do need to unselect everything */
                    xfdesktop_icon_view_unselect_all(icon_view);

                    if(!(evt->state & GDK_SHIFT_MASK))
                        icon_view->first_clicked_item = NULL;
                }

                xfdesktop_icon_view_set_cursor(icon_view, item, FALSE);

                if (icon_view->first_clicked_item == NULL) {
                    icon_view->first_clicked_item = item;
                }

                if (icon_view->sel_mode == GTK_SELECTION_MULTIPLE
                    && (evt->state & GDK_SHIFT_MASK) != 0
                    && icon_view->first_clicked_item != NULL
                    && icon_view->first_clicked_item != item)
                {
                    xfdesktop_icon_view_select_between(icon_view, icon_view->first_clicked_item, item);
                } else {
                    xfdesktop_icon_view_select_item_internal(icon_view, item, TRUE);
                }
            }

            if(evt->button == 1 || evt->button == 3) {
                /* we might be the start of a drag */
                DBG("setting stuff");
                icon_view->maybe_begin_drag = TRUE;
                icon_view->definitely_dragging = FALSE;
                icon_view->definitely_rubber_banding = FALSE;
                icon_view->press_start_x = evt->x;
                icon_view->press_start_y = evt->y;
            }

            if (evt->button == 3) {
                // We'll want a context menu to pop up, but we have to wait a
                // short time just in case the user starts dragging.
                GtkSettings *settings = gtk_settings_get_for_screen(gtk_widget_get_screen(widget));
                gint delay = 0;
                g_object_get(settings,
                             "gtk-menu-popup-delay", &delay,
                             NULL);
                icon_view->drag_timer_event = (GdkEventButton *)gdk_event_copy((GdkEvent *)evt);
                icon_view->drag_timer_id = g_timeout_add_full(G_PRIORITY_DEFAULT,
                                                              MAX(225, delay),
                                                              context_menu_drag_timeout,
                                                              icon_view,
                                                              (GDestroyNotify) context_menu_drag_timeout_destroy);
            }

            return TRUE;
        } else {
            /* Button press wasn't over any icons */
            /* unselect previously selected icons if we didn't click one */
            if (icon_view->sel_mode != GTK_SELECTION_MULTIPLE || (evt->state & GDK_CONTROL_MASK) == 0) {
                xfdesktop_icon_view_unselect_all(icon_view);
            }

            xfdesktop_icon_view_set_cursor(icon_view, NULL, FALSE);
            icon_view->first_clicked_item = NULL;

            if (icon_view->allow_rubber_banding
                && evt->button == GDK_BUTTON_PRIMARY
                && (evt->state & GDK_SHIFT_MASK) == 0)
            {
                icon_view->maybe_begin_drag = TRUE;
                icon_view->definitely_dragging = FALSE;
                icon_view->press_start_x = evt->x;
                icon_view->press_start_y = evt->y;
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
            icon_view->double_click = TRUE;
            xfdesktop_icon_view_unselect_all(icon_view);
            return TRUE;
        }

        /* be sure to cancel any pending drags that might have snuck through.
         * this shouldn't happen, but it does sometimes (bug 3426).  */
        icon_view->maybe_begin_drag = FALSE;
        icon_view->definitely_dragging = FALSE;
        icon_view->definitely_rubber_banding = FALSE;

        if(evt->button == 1) {
            GList *icon_l;

            icon_l = g_list_find_custom(icon_view->items, evt,
                                        (GCompareFunc)xfdesktop_check_icon_clicked);
            if (icon_l != NULL) {
                ViewItem *item = icon_l->data;
                xfdesktop_icon_view_set_cursor(icon_view, item, FALSE);
                g_signal_emit(G_OBJECT(icon_view), __signals[SIG_ICON_ACTIVATED], 0);
                xfdesktop_icon_view_unselect_all(icon_view);
            }
        }

        return TRUE;
    }

    return FALSE;
}

static gboolean
xfdesktop_icon_view_button_release(GtkWidget *widget, GdkEventButton *evt) {
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);
    gboolean ret = FALSE;

    DBG("entering btn=%d", evt->button);

    /* single-click */
    if(xfdesktop_icon_view_get_single_click(icon_view)
       && evt->button == 1
       && !(evt->state & GDK_SHIFT_MASK)
       && !(evt->state & GDK_CONTROL_MASK)
       && !icon_view->definitely_dragging
       && !icon_view->definitely_rubber_banding
       && !icon_view->double_click)
    {
        /* Find out if we clicked on an icon */
        GList *icon_l = g_list_find_custom(icon_view->items, evt,
                                           (GCompareFunc)xfdesktop_check_icon_clicked);
        if (icon_l != NULL) {
            ViewItem *item = icon_l->data;
            /* We did, activate it */
            xfdesktop_icon_view_set_cursor(icon_view, item, FALSE);
            g_signal_emit(G_OBJECT(icon_view), __signals[SIG_ICON_ACTIVATED], 0);
            xfdesktop_icon_view_unselect_all(icon_view);
            ret = TRUE;
        }
    }

    if((evt->button == 3 || (evt->button == 1 && (evt->state & GDK_SHIFT_MASK))) &&
       icon_view->definitely_dragging == FALSE &&
       icon_view->definitely_rubber_banding == FALSE &&
       icon_view->maybe_begin_drag == TRUE)
    {
        if (evt->button == 3 && icon_view->drag_timer_id != 0) {
            context_menu_drag_timeout(icon_view);
            g_source_remove(icon_view->drag_timer_id);
        }
    }

    if (evt->button == GDK_BUTTON_PRIMARY && (evt->state & GDK_CONTROL_MASK) != 0 && icon_view->control_click) {
        GList *icon_l = g_list_find_custom(icon_view->items, evt,
                                           (GCompareFunc)xfdesktop_check_icon_clicked);
        if (icon_l != NULL) {
            ViewItem *item = icon_l->data;
            if (item->selected) {
                /* clicked an already-selected icon; unselect it */
                xfdesktop_icon_view_unselect_item_internal(icon_view, item, TRUE);
            }
            ret = TRUE;
        }
    }

    if(evt->button == 1 || evt->button == 3 || evt->button == 0) {
        xfdesktop_icon_view_clear_drag_event(icon_view);
    }

    gtk_grab_remove(widget);

    /* TRUE: stop other handlers from being invoked for the event. FALSE: propagate the event further. */
    /* On FALSE this method will be called twice in single-click-mode (possibly a gtk3 bug)            */
    return ret;
}

static void
xfdesktop_icon_view_cancel_keyboard_navigation(XfdesktopIconView *icon_view) {
    gtk_grab_remove(GTK_WIDGET(icon_view));

    if (icon_view->keyboard_navigation_state_timeout != 0) {
        g_source_remove(icon_view->keyboard_navigation_state_timeout);
        icon_view->keyboard_navigation_state_timeout = 0;
    }

    if (icon_view->keyboard_navigation_state != NULL) {
        g_array_free(icon_view->keyboard_navigation_state, TRUE);
        icon_view->keyboard_navigation_state = NULL;
    }
}

static gboolean
keyboard_navigation_timeout(gpointer data) {
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(data);
    icon_view->keyboard_navigation_state_timeout = 0;
    xfdesktop_icon_view_cancel_keyboard_navigation(icon_view);
    return G_SOURCE_REMOVE;
}

static gboolean
xfdesktop_icon_view_keyboard_navigate(XfdesktopIconView *icon_view,
                                      gunichar lower_char)
{
    gboolean found_match = FALSE;

    if (icon_view->model == NULL || icon_view->search_column == -1) {
        return FALSE;
    }

    if (icon_view->keyboard_navigation_state == NULL) {
        icon_view->keyboard_navigation_state = g_array_sized_new(TRUE, TRUE, sizeof(gunichar), 16);
        xfdesktop_icon_view_unselect_all(icon_view);
    }

    if (icon_view->keyboard_navigation_state_timeout != 0) {
        g_source_remove(icon_view->keyboard_navigation_state_timeout);
    }

    gtk_grab_add(GTK_WIDGET(icon_view));
    icon_view->keyboard_navigation_state_timeout = g_timeout_add(KEYBOARD_NAVIGATION_TIMEOUT,
                                                                       keyboard_navigation_timeout,
                                                                       icon_view);

    g_array_append_val(icon_view->keyboard_navigation_state, lower_char);

    for (GList *l = icon_view->items; l != NULL && !found_match; l = l->next) {
        ViewItem *item = l->data;
        GtkTreeIter iter;
        gchar *label = NULL;

        if (view_item_get_iter(item, icon_view->model, &iter)) {
            gtk_tree_model_get(icon_view->model, &iter,
                               icon_view->search_column, &label,
                               -1);
        }

        if (label != NULL && g_utf8_validate(label, -1, NULL)) {
            gchar *p = label;
            gboolean matches = TRUE;

            for (guint i = 0; i < icon_view->keyboard_navigation_state->len; ++i) {
                gunichar label_char;

                if (*p == '\0') {
                    matches = FALSE;
                    break;
                }

                label_char = g_unichar_tolower(g_utf8_get_char(p));
                if (label_char != g_array_index(icon_view->keyboard_navigation_state, gunichar, i)) {
                    matches = FALSE;
                    break;
                }

                p = g_utf8_find_next_char(p, NULL);
            }

            if (matches) {
                xfdesktop_icon_view_unselect_all(icon_view);
                xfdesktop_icon_view_set_cursor(icon_view, item, TRUE);
                xfdesktop_icon_view_select_item_internal(icon_view, item, TRUE);
                found_match = TRUE;
            }
        }

        g_free(label);
    }

    return found_match;
}

static gboolean
xfdesktop_icon_view_key_press(GtkWidget *widget, GdkEventKey *evt) {
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);
    gboolean ret = FALSE;

    if (icon_view->keyboard_navigation_state == NULL) {
        ret = GTK_WIDGET_CLASS(xfdesktop_icon_view_parent_class)->key_press_event(widget, evt);
    }

    if (!ret) {
        DBG("entering");

        GdkModifierType ignore_modifiers = gtk_accelerator_get_default_mod_mask();
        if ((evt->state & ignore_modifiers) == 0) {
            /* Now inspect the pressed character. Let's try to find an
             * icon starting with this character and make the icon selected. */
            guint32 unicode = gdk_keyval_to_unicode(evt->keyval);
            if (unicode != 0 && g_unichar_isprint(unicode)) {
                ret = TRUE;
                xfdesktop_icon_view_keyboard_navigate(icon_view, g_unichar_tolower(unicode));
            }
        }

        if (!ret) {
            if (icon_view->keyboard_navigation_state != NULL) {
                g_array_free(icon_view->keyboard_navigation_state, TRUE);
                icon_view->keyboard_navigation_state = NULL;

                // Couldn't navigate further, so try to let the superclass
                // handle it.
                ret = GTK_WIDGET_CLASS(xfdesktop_icon_view_parent_class)->key_press_event(widget, evt);
            }
        }
    }

    return ret;
}

static gboolean
xfdesktop_icon_view_focus_in(GtkWidget *widget, GdkEventFocus *evt) {
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);

    gtk_widget_grab_focus(GTK_WIDGET(icon_view));
    DBG("GOT FOCUS");

    for (GList *l = icon_view->selected_items; l != NULL; l = l->next) {
        xfdesktop_icon_view_invalidate_item_text(icon_view, (ViewItem *)l->data);
    }

    return FALSE;
}

static gboolean
xfdesktop_icon_view_focus_out(GtkWidget *widget, GdkEventFocus *evt) {
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);

    DBG("LOST FOCUS");

    xfdesktop_icon_view_cancel_keyboard_navigation(icon_view);

    for (GList *l = icon_view->selected_items; l != NULL; l = l->next) {
        xfdesktop_icon_view_invalidate_item_text(icon_view, (ViewItem *)l->data);
    }

    if(G_UNLIKELY(icon_view->single_click)) {
        gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(icon_view)), NULL);
    }

    return FALSE;
}

static gboolean
xfdesktop_icon_view_maybe_begin_drag(XfdesktopIconView *icon_view, GdkEventMotion *evt) {
    GdkDragAction actions;

    /* sanity check */
    g_return_val_if_fail(icon_view->cursor, FALSE);

    if (!gtk_drag_check_threshold(GTK_WIDGET(icon_view),
                                  icon_view->press_start_x,
                                  icon_view->press_start_y,
                                  evt->x,
                                  evt->y))
    {
        return FALSE;
    }

    actions = GDK_ACTION_MOVE | (icon_view->drag_source_set
                                 ? icon_view->foreign_source_actions
                                 : 0);

    if(!(evt->state & GDK_BUTTON3_MASK)) {
        gtk_drag_begin_with_coordinates(GTK_WIDGET(icon_view),
                                        icon_view->source_targets,
                                        actions,
                                        1,
                                        (GdkEvent *)evt,
                                        evt->x,
                                        evt->y);
    } else {
        gtk_drag_begin_with_coordinates(GTK_WIDGET(icon_view),
                                        icon_view->source_targets,
                                        actions | GDK_ACTION_ASK,
                                        3,
                                        (GdkEvent *)evt,
                                        evt->x,
                                        evt->y);
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

    if (icon_view->model == NULL
        || icon_view->item_under_pointer == NULL
        || icon_view->definitely_dragging
        || !icon_view->show_tooltips)
    {
        return FALSE;
    }

    if (!view_item_get_iter(icon_view->item_under_pointer, icon_view->model, &iter)) {
        return FALSE;
    }

    g_signal_emit(icon_view, __signals[SIG_QUERY_ICON_TOOLTIP], 0,
                  &iter, x, y, keyboard_tooltip, tooltip,
                  &result);

    if (!result && (icon_view->tooltip_icon_column != -1 || icon_view->tooltip_text_column != -1)) {
        GIcon *icon = NULL;
        gchar *tip_text = NULL;

        if (icon_view->tooltip_icon_column != -1) {
            gtk_tree_model_get(icon_view->model, &iter,
                               icon_view->tooltip_icon_column, &icon,
                               -1);
        }

        if (icon_view->tooltip_text_column != -1) {
            gtk_tree_model_get(icon_view->model, &iter,
                               icon_view->tooltip_text_column, &tip_text,
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
                g_free(tip_text);
            }

            gtk_widget_show_all(box);
            gtk_tooltip_set_custom(tooltip, box);

            cairo_rectangle_int_t cextents;
            cairo_region_get_extents(icon_view->item_under_pointer->icon_slot_region, &cextents);
            GdkRectangle extents = GDK_RECT_FROM_CAIRO(&cextents);
            gtk_tooltip_set_tip_area(tooltip, &extents);

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
    icon_view->tooltip_icon_size_xfconf = g_value_get_double(value);
}

static gboolean
xfdesktop_icon_view_motion_notify(GtkWidget *widget, GdkEventMotion *evt) {
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);
    gboolean ret = FALSE;

    if (icon_view->maybe_begin_drag
        && icon_view->cursor != NULL
        && icon_view->item_under_pointer != NULL
        && !icon_view->definitely_dragging)
    {
        /* we might have the start of an icon click + drag here */
        icon_view->definitely_dragging = xfdesktop_icon_view_maybe_begin_drag(icon_view, evt);
        if (icon_view->definitely_dragging) {
            if (icon_view->drag_timer_id != 0) {
                g_source_remove(icon_view->drag_timer_id);
            }

            icon_view->maybe_begin_drag = FALSE;
            ret = TRUE;
        }
    } else if (icon_view->maybe_begin_drag
               && ((icon_view->item_under_pointer == NULL
                    && !icon_view->definitely_rubber_banding)
                   || icon_view->definitely_rubber_banding))
    {

        /* we're dragging with no icon under the cursor -> rubber band start
         * OR, we're already doin' the band -> update it */

        cairo_rectangle_int_t *new_rect = &icon_view->band_rect;

        cairo_rectangle_int_t old_rect;
        if (!icon_view->definitely_rubber_banding) {
            icon_view->definitely_rubber_banding = TRUE;
            old_rect.x = icon_view->press_start_x;
            old_rect.y = icon_view->press_start_y;
            old_rect.width = old_rect.height = 1;
        } else {
            memcpy(&old_rect, new_rect, sizeof(old_rect));
        }

        new_rect->x = MIN(icon_view->press_start_x, evt->x);
        new_rect->y = MIN(icon_view->press_start_y, evt->y);
        new_rect->width = ABS(evt->x - icon_view->press_start_x) + 1;
        new_rect->height = ABS(evt->y - icon_view->press_start_y) + 1;

        cairo_region_t *region = cairo_region_create_rectangle(&old_rect);
        cairo_region_union_rectangle(region, new_rect);

        cairo_rectangle_int_t intersect;
        if (gdk_rectangle_intersect(&old_rect, new_rect, &intersect) && intersect.width > 2 && intersect.height > 2) {
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

        gdk_window_invalidate_region(gtk_widget_get_window(widget), region, TRUE);

        /* update list of selected icons */

        /* first pass: if the rubber band area got smaller at least in
         * one dimension, we can try first to just remove icons that
         * aren't in the band anymore */
        if (old_rect.width > new_rect->width || old_rect.height > new_rect->height) {
            GList *l = icon_view->selected_items;
            while(l) {
                ViewItem *item = l->data;

                /* To be removed, it must intersect the old rectangle and
                 * not intersect the new one. This way CTRL + rubber band
                 * works properly (Bug 10275) */

                if (cairo_region_contains_rectangle(item->icon_slot_region, &old_rect) != CAIRO_REGION_OVERLAP_OUT
                    && cairo_region_contains_rectangle(item->icon_slot_region, new_rect) == CAIRO_REGION_OVERLAP_OUT)
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
        if (old_rect.width < new_rect->width || old_rect.height < new_rect->height) {
            for (GList *l = icon_view->items; l != NULL; l = l->next) {
                ViewItem *item = l->data;

                if (!item->selected
                    && cairo_region_contains_rectangle(item->icon_slot_region, new_rect) != CAIRO_REGION_OVERLAP_OUT)
                {
                    /* since _select_item() prepends to the list, we
                     * should be ok just calling this */
                    xfdesktop_icon_view_select_item_internal(icon_view, item, TRUE);
                }
            }
        }

        cairo_region_destroy(region);
    } else {
        /* normal movement; highlight icons as they go under the pointer */
        update_item_under_pointer(icon_view, evt->window, evt->x, evt->y);
    }

    gdk_event_request_motions(evt);

    return ret;
}

static gboolean
xfdesktop_icon_view_leave_notify(GtkWidget *widget, GdkEventCrossing *evt) {
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);

    if (icon_view->item_under_pointer != NULL) {
        ViewItem *item = icon_view->item_under_pointer;
        icon_view->item_under_pointer = NULL;

        xfdesktop_icon_view_invalidate_item(icon_view, item, FALSE);
    }

    if (icon_view->single_click) {
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

    g_return_val_if_fail(icon_view->model != NULL, NULL);

    if (item->pixbuf_surface != NULL) {
        surface = cairo_surface_reference(item->pixbuf_surface);
    } else {
        if (icon_view->pixbuf_column != -1) {
            GtkTreeIter iter;

            if (view_item_get_iter(item, icon_view->model, &iter)) {
                GIcon *icon = NULL;

                gtk_tree_model_get(icon_view->model, &iter,
                                   icon_view->pixbuf_column, &icon,
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
                        if (icon_view->icon_opacity_column != -1) {
                            gdouble opacity = 1.0;
                            gtk_tree_model_get(icon_view->model, &iter,
                                               icon_view->icon_opacity_column, &opacity,
                                               -1);
                            opacity = CLAMP(opacity, 0.0, 1.0);
                            if (opacity < 1.0) {
                                GdkPixbuf *tmp = exo_gdk_pixbuf_lucent(pix, opacity * 100);
                                g_object_unref(pix);
                                pix = tmp;
                            }
                        }

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
    ViewItem *item = icon_view->cursor;
    cairo_surface_t *surface;

    g_return_if_fail(icon_view->model != NULL);
    g_return_if_fail(item != NULL);

    surface = xfdesktop_icon_view_get_surface_for_item(icon_view, item);
    if (surface != NULL) {
        gtk_drag_set_icon_surface(context, surface);
        cairo_surface_destroy(surface);
    }

    icon_view->drag_dropped = FALSE;
    icon_view->drag_drop_row = -1;
    icon_view->drag_drop_col = -1;
}

static void
free_dragged_icons(gpointer data) {
    g_list_free_full(data, g_free);
}

static void
xfdesktop_icon_view_drag_data_get(GtkWidget *widget,
                                  GdkDragContext *context,
                                  GtkSelectionData *data,
                                  guint info,
                                  guint time)
{
    TRACE("entering, %d", info);

    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);

    if (info == TARGET_XFDESKTOP_ICON) {
        XfdesktopDraggedIconList icon_list = {
            .source_icon_view = icon_view,
            .dragged_icons = NULL,
        };

        for (GList *l = icon_view->selected_items; l != NULL; l = l->next) {
            ViewItem *item = l->data;

            XfdesktopDraggedIcon *dragged_icon = g_new0(XfdesktopDraggedIcon, 1);
            if (view_item_get_iter(item, GTK_TREE_MODEL(icon_view->model), &dragged_icon->iter)) {
                dragged_icon->item = item;
                dragged_icon->dest_row = -1;
                dragged_icon->dest_col = -1;
                icon_list.dragged_icons = g_list_prepend(icon_list.dragged_icons, dragged_icon);
            } else {
                g_free(dragged_icon);
            }
        }
        icon_list.dragged_icons = g_list_reverse(icon_list.dragged_icons);

        if (icon_list.dragged_icons != NULL) {
            g_object_set_data_full(G_OBJECT(context),
                                   "--xfdesktop-icon-view-xfdesktop-icon-drag-data",
                                   icon_list.dragged_icons,
                                   (GDestroyNotify)free_dragged_icons);
            gtk_selection_data_set(data,
                                   gdk_atom_intern(XFDESKTOP_ICON_NAME, FALSE),
                                   8,
                                   (guchar *)&icon_list,
                                   sizeof(icon_list));
        } else {
            DBG("no valid source icons, cancelling");
            gtk_drag_cancel(context);
        }
    }
}

static gboolean
xfdesktop_icon_view_drag_motion(GtkWidget *widget,
                                GdkDragContext *context,
                                gint x,
                                gint y,
                                guint time_)
{
    TRACE("entering, (%d, %d)", x, y);

    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);

    gint drag_drop_row = -1, drag_drop_col = -1;
    gboolean on_grid = xfdesktop_icon_view_widget_coords_to_slot_coords(icon_view,
                                                                        x,
                                                                        y,
                                                                        &drag_drop_row,
                                                                        &drag_drop_col);

    gboolean slot_changed;
    if (drag_drop_row != icon_view->drag_drop_row || drag_drop_col != icon_view->drag_drop_col) {
        xfdesktop_icon_view_unset_highlight(icon_view);
        icon_view->drag_drop_row = drag_drop_row;
        icon_view->drag_drop_col = drag_drop_col;
        slot_changed = TRUE;
    } else {
        slot_changed = FALSE;
    }

    if (on_grid) {
        GdkAtom target = gtk_drag_dest_find_target(widget, context, icon_view->dest_targets);
        if (target == gdk_atom_intern(XFDESKTOP_ICON_NAME, FALSE)
            && xfdesktop_icon_view_item_in_slot(icon_view,
                                                icon_view->drag_drop_row,
                                                icon_view->drag_drop_col) == NULL)
        {
            DBG("icon moving to empty slot");
            if (slot_changed) {
                xfdesktop_icon_view_draw_highlight(icon_view,
                                                   icon_view->drag_drop_row,
                                                   icon_view->drag_drop_col);
            }
            gdk_drag_status(context, GDK_ACTION_MOVE, time_);
            return TRUE;
        } else {
            DBG("not icon source, or icon source moving over another icon");
            return FALSE;
        }
    } else {
        DBG("motion not on the grid");
        return FALSE;
    }
}

static void
xfdesktop_icon_view_drag_leave(GtkWidget *widget, GdkDragContext *context, guint time_) {
    TRACE("entering");
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);
    xfdesktop_icon_view_unset_highlight(icon_view);
}

static gsize
grid_layout_bytes(gint nrows, gint ncols) {
    if (nrows <= 0 || ncols <= 0) {
        return 0;
    } else {
        return nrows * ncols * sizeof(ViewItem *);
    }
}

static gboolean
xfdesktop_icon_view_drag_drop(GtkWidget *widget,
                              GdkDragContext *context,
                              gint x,
                              gint y,
                              guint time_)
{
    TRACE("entering");

    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);

    icon_view->drag_dropped = TRUE;
    icon_view->control_click = FALSE;
    icon_view->double_click = FALSE;
    icon_view->maybe_begin_drag = FALSE;
    icon_view->definitely_dragging = FALSE;
    xfdesktop_icon_view_unset_highlight(icon_view);

    GdkAtom target = gtk_drag_dest_find_target(widget, context, icon_view->dest_targets);
#ifdef DEBUG
    gchar *target_name = gdk_atom_name(target);
    DBG("dropped, target was %s, loc=(%d, %d)",
        target_name,
        icon_view->drag_drop_row,
        icon_view->drag_drop_col);
    g_free(target_name);
#endif

    if (target == gdk_atom_intern(XFDESKTOP_ICON_NAME, FALSE)
        // We have to find the row/col again because the view manager may have
        // eaten some drag-motion events.
        && xfdesktop_icon_view_widget_coords_to_slot_coords(icon_view,
                                                            x,
                                                            y,
                                                            &icon_view->drag_drop_row,
                                                            &icon_view->drag_drop_col)
        && xfdesktop_icon_view_item_in_slot(icon_view,
                                            icon_view->drag_drop_row,
                                            icon_view->drag_drop_col) == NULL)
    {
        DBG("got a moved icon");
        gtk_drag_get_data(widget, context, target, time_);
        return TRUE;
    } else {
        return FALSE;
    }
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
    TRACE("entering, %d", info);

    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);

#ifdef DEBUG
    gchar *data_type = gdk_atom_name(gtk_selection_data_get_data_type(data));
    gchar *target_name = gdk_atom_name(gtk_selection_data_get_target(data));
    DBG("selection data type: %s, target: %s", data_type, target_name);
    g_free(data_type);
    g_free(target_name);
#endif

    if (info == TARGET_XFDESKTOP_ICON) {
        gboolean slot_valid = icon_view->drag_drop_row != -1 && icon_view->drag_drop_col != -1;
        gboolean slot_empty = slot_valid && xfdesktop_icon_view_item_in_slot(icon_view,
                                                                             icon_view->drag_drop_row, 
                                                                             icon_view->drag_drop_col) == NULL;

        if (icon_view->drag_dropped && slot_valid && slot_empty) {
            g_assert(gtk_selection_data_get_length(data) == sizeof(XfdesktopDraggedIconList));
            XfdesktopDraggedIconList *icon_list = (gpointer)gtk_selection_data_get_data(data);
            g_assert(icon_list != NULL);
            g_assert(icon_list->dragged_icons != NULL);

            gint row_offset, col_offset;
            GtkTreeIter cursor_iter;
            gint cursor_row, cursor_col;
            if (xfdesktop_icon_view_get_cursor(icon_list->source_icon_view, &cursor_iter, &cursor_row, &cursor_col)) {
                row_offset = icon_view->drag_drop_row - cursor_row;
                col_offset = icon_view->drag_drop_col - cursor_col;
            } else {
                XfdesktopDraggedIcon *first = icon_list->dragged_icons->data;
                row_offset = icon_view->drag_drop_row - first->item->row;
                col_offset = icon_view->drag_drop_row - first->item->col;
            }
            DBG("move offset: (%d, %d)", row_offset, col_offset);

            if (icon_list->source_icon_view == icon_view) {
                // We're moving in the same icon view, so unplace the dropped
                // icons so we can find new places for them without conflicts.
                for (GList *l = icon_list->dragged_icons; l != NULL; l = l->next) {
                    XfdesktopDraggedIcon *dragged_icon = l->data;
                    xfdesktop_icon_view_unplace_item(icon_list->source_icon_view, dragged_icon->item);
                }
            }

            // Now we'll copy the existing grid and try to place the icons into
            // it, figuring out where they can go.
            gsize grid_size = grid_layout_bytes(icon_view->nrows, icon_view->ncols);
            ViewItem **temp_grid_layout = g_malloc0(grid_size);
            memcpy(temp_grid_layout, icon_view->grid_layout, grid_size);

            GList *unplaceable = NULL;
            for (GList *l = icon_list->dragged_icons; l != NULL; l = l->next) {
                XfdesktopDraggedIcon *dragged_icon = l->data;
                gint dest_row = dragged_icon->item->row + row_offset;
                gint dest_col = dragged_icon->item->col + col_offset;
                DBG("moving (%d, %d) -> (%d, %d)",
                    dragged_icon->item->row, dragged_icon->item->col,
                    dest_row, dest_col);

                if (xfdesktop_icon_view_place_item_in_grid_at(icon_view, temp_grid_layout, dragged_icon->item, dest_row, dest_col)) {
                    DBG("placement succeeded");
                    dragged_icon->dest_row = dest_row;
                    dragged_icon->dest_col = dest_col;
                } else {
                    DBG("placement failed");
                    unplaceable = g_list_prepend(unplaceable, dragged_icon);
                }
            }
            unplaceable = g_list_reverse(unplaceable);

            for (GList *l = unplaceable; l != NULL; l = l->next) {
                XfdesktopDraggedIcon *dragged_icon = l->data;
                gint dest_row = dragged_icon->item->row + row_offset;
                gint dest_col = dragged_icon->item->col + col_offset;

                if (xfdesktop_icon_view_get_next_free_grid_position_for_grid(icon_view,
                                                                             temp_grid_layout,
                                                                             dest_row,
                                                                             dest_col,
                                                                             &dest_row,
                                                                             &dest_col))
                {
                    DBG("next avail slot: (%d, %d)", dest_row, dest_col);
                    xfdesktop_icon_view_place_item_in_grid_at(icon_view,
                                                              temp_grid_layout,
                                                              dragged_icon->item,
                                                              dest_row,
                                                              dest_col);
                    dragged_icon->dest_row = dest_row;
                    dragged_icon->dest_col = dest_col;
                } else {
                    DBG("failed to find a new place");
                    dragged_icon->dest_row = -1;
                    dragged_icon->dest_col = -1;
                }
            }
            g_list_free(unplaceable);
            g_free(temp_grid_layout);

            for (GList *l = icon_list->dragged_icons; l != NULL; l = l->next) {
                XfdesktopDraggedIcon *dragged_icon = l->data;
                g_signal_emit(icon_view, __signals[SIG_ICON_MOVED], 0,
                              icon_list->source_icon_view,
                              &dragged_icon->iter,
                              dragged_icon->dest_row,
                              dragged_icon->dest_col);
            }

            icon_view->drag_dropped = FALSE;
            gtk_drag_finish(context, TRUE, FALSE, time_);
        } else if (icon_view->drag_dropped) {
            // Dropp has happened but we're not in a valid, empty slot.
            gtk_drag_finish(context, FALSE, FALSE, time_);
        } else {
            gdk_drag_status(context, slot_valid && slot_empty ? GDK_ACTION_MOVE : 0, time_);
        }
    }
}

void
xfdesktop_icon_view_sort_icons(XfdesktopIconView *icon_view,
                               GtkSortType sort_type)
{
    GHashTable *priority_buckets;
    GList *sorted_buckets;

    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));
    g_return_if_fail(icon_view->model != NULL);

    priority_buckets = g_hash_table_new(g_direct_hash, g_direct_equal);

    for (GList *l = icon_view->items; l != NULL; l = l->next) {
        ViewItem *item = (ViewItem *)l->data;
        GtkTreeIter iter;

        if (item->placed) {
            xfdesktop_icon_view_unplace_item(icon_view, item);
        }
        item->row = -1;
        item->col = -1;

        if (view_item_get_iter(item, icon_view->model, &iter)) {
            SortItem *sort_item;
            gint priority = 0;
            GList *sort_items;

            sort_item = sort_item_new(item);

            if (icon_view->sort_priority_column != -1) {
                gtk_tree_model_get(icon_view->model, &iter,
                                   icon_view->text_column, &sort_item->label,
                                   icon_view->sort_priority_column, &priority,
                                   -1);
            } else {
                gtk_tree_model_get(icon_view->model, &iter,
                                   icon_view->text_column, &sort_item->label,
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
xfdesktop_icon_view_icon_theme_changed(GtkIconTheme *icon_theme, XfdesktopIconView *icon_view) {
    xfdesktop_icon_view_invalidate_pixbuf_cache(icon_view);
    xfdesktop_icon_view_invalidate_all(icon_view, TRUE);
}

static void
xfdesktop_icon_view_style_updated(GtkWidget *widget)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);

    DBG("entering");

    gint cell_spacing;
    gint slot_padding;
    gdouble cell_text_width_proportion;
    gtk_widget_style_get(widget,
                         "cell-spacing", &cell_spacing,
                         "cell-padding", &slot_padding,
                         "cell-text-width-proportion", &cell_text_width_proportion,
                         "ellipsize-icon-labels", &icon_view->ellipsize_icon_labels,
                         "label-radius", &icon_view->label_radius,
                         "tooltip-size", &icon_view->tooltip_icon_size_style,
                         NULL);

    gboolean need_grid_resize = cell_spacing != icon_view->cell_spacing
        || slot_padding != icon_view->slot_padding
        || cell_text_width_proportion != icon_view->cell_text_width_proportion;

    icon_view->cell_spacing = cell_spacing;
    icon_view->slot_padding = slot_padding;

    if (cell_text_width_proportion != icon_view->cell_text_width_proportion) {
        icon_view->cell_text_width_proportion = cell_text_width_proportion;
        g_object_notify(G_OBJECT(widget), "icon-width");
    }

    XF_DEBUG("cell spacing is %d", icon_view->cell_spacing);
    XF_DEBUG("cell padding is %d", icon_view->slot_padding);
    XF_DEBUG("cell text width proportion is %f", icon_view->cell_text_width_proportion);
    XF_DEBUG("ellipsize icon label is %s", icon_view->ellipsize_icon_labels ? "true" : "false");
    XF_DEBUG("label radius is %f", icon_view->label_radius);

    gint width = TEXT_WIDTH + icon_view->label_radius * 2;
    g_object_set(icon_view->text_renderer,
                 "alignment", icon_view->center_text
                 ? PANGO_ALIGN_CENTER
                 : (gtk_widget_get_direction(GTK_WIDGET(icon_view)) == GTK_TEXT_DIR_RTL
                    ? PANGO_ALIGN_RIGHT
                    : PANGO_ALIGN_LEFT),
                 "align-set", TRUE,
                 "ellipsize", icon_view->ellipsize_icon_labels ? PANGO_ELLIPSIZE_END : PANGO_ELLIPSIZE_NONE,
                 "ellipsize-set", TRUE,
                 "unselected-height", (gint)(TEXT_HEIGHT + icon_view->label_radius * 2),
                 "width", (gint)width,
                 "wrap-width", (gint)(width * PANGO_SCALE),
                 "xpad", (gint)icon_view->label_radius,
                 "ypad", (gint)icon_view->label_radius,
                 NULL);

    if (gtk_widget_get_realized(widget)) {
        if (need_grid_resize) {
            xfdesktop_icon_view_size_grid(icon_view);
        } else {
            for (GList *l = icon_view->selected_items; l != NULL; l = l->next) {
                xfdesktop_icon_view_invalidate_item_text(icon_view, (ViewItem *)l->data);
            }
        }
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

    GtkAllocation old_allocation;
    gtk_widget_get_allocation(widget, &old_allocation);

    GTK_WIDGET_CLASS(xfdesktop_icon_view_parent_class)->size_allocate(widget, allocation);

    if (gtk_widget_get_realized(widget) &&
        (old_allocation.x != allocation->x ||
         old_allocation.y != allocation->y ||
         old_allocation.width != allocation->width ||
         old_allocation.height != allocation->height))
    {
        xfdesktop_icon_view_size_grid(XFDESKTOP_ICON_VIEW(widget));
    } else {
        DBG("allocation did not change; ignoring");
    }
}

static void
xfdesktop_icon_view_realize(GtkWidget *widget)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);
    GdkScreen *gscreen;

    GTK_WIDGET_CLASS(xfdesktop_icon_view_parent_class)->realize(widget);

    gtk_widget_add_events(widget, GDK_EXPOSURE_MASK);

    g_signal_connect(icon_view, "notify::scale-factor",
                     G_CALLBACK(scale_factor_changed_cb), NULL);

    /* we need this call here to initalize some members of icon_view,
     * those depend on custom style properties */
    xfdesktop_icon_view_style_updated(widget);

    GtkWidget *toplevel = gtk_widget_get_toplevel(widget);
    gtk_window_set_accept_focus(GTK_WINDOW(toplevel), TRUE);
    gtk_window_set_focus_on_map(GTK_WINDOW(toplevel), FALSE);

    gscreen = gtk_widget_get_screen(widget);
    g_signal_connect_after(G_OBJECT(gtk_icon_theme_get_for_screen(gscreen)),
                           "changed",
                           G_CALLBACK(xfdesktop_icon_view_icon_theme_changed),
                           icon_view);

    xfdesktop_icon_view_size_grid(icon_view);
}

static void
xfdesktop_icon_view_unrealize(GtkWidget *widget)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);
    GdkScreen *gscreen;

    GtkWidget *toplevel = gtk_widget_get_toplevel(widget);
    gtk_window_set_accept_focus(GTK_WINDOW(toplevel), FALSE);

    gscreen = gtk_widget_get_screen(widget);

    g_signal_handlers_disconnect_by_func(G_OBJECT(gtk_icon_theme_get_for_screen(gscreen)),
                     G_CALLBACK(xfdesktop_icon_view_icon_theme_changed),
                     icon_view);

    g_signal_handlers_disconnect_by_func(G_OBJECT(icon_view),
                                         G_CALLBACK(scale_factor_changed_cb),
                                         NULL);

    if (icon_view->model != NULL) {
        xfdesktop_icon_view_disconnect_model_signals(icon_view);
    }
    xfdesktop_icon_view_items_free(icon_view);

    g_free(icon_view->grid_layout);
    icon_view->grid_layout = NULL;

    GTK_WIDGET_CLASS(xfdesktop_icon_view_parent_class)->unrealize(widget);
}

static gboolean
xfdesktop_icon_view_shift_to_slot_area(XfdesktopIconView *icon_view,
                                       ViewItem *item,
                                       GdkRectangle *rect,
                                       GdkRectangle *slot_rect)
{
    g_return_val_if_fail(item->row >= 0 && item->row < icon_view->nrows, FALSE);
    g_return_val_if_fail(item->col >= 0 && item->col < icon_view->ncols, FALSE);

    slot_rect->x = icon_view->xmargin + item->col * SLOT_SIZE + item->col * icon_view->xspacing + rect->x;
    slot_rect->y = icon_view->ymargin + item->row * SLOT_SIZE + item->row * icon_view->yspacing + rect->y;
    slot_rect->width = rect->width;
    slot_rect->height = rect->height;

    return TRUE;
}

static gboolean
xfdesktop_icon_view_place_item(XfdesktopIconView *icon_view,
                               ViewItem *item,
                               gboolean honor_model_position)
{
    if (icon_view->grid_layout != NULL) {
        gint row = -1, col = -1;

        if (honor_model_position && icon_view->row_column != -1 && icon_view->col_column != -1) {
            GtkTreeIter iter;

            if (view_item_get_iter(item, icon_view->model, &iter)) {
                gtk_tree_model_get(icon_view->model,
                                   &iter,
                                   icon_view->row_column, &row,
                                   icon_view->col_column, &col,
                                   -1);

                if (row >= 0 && row < icon_view->nrows && col >= 0 && col < icon_view->ncols) {
                    xfdesktop_icon_view_place_item_at(icon_view, item, row, col);
                }
            }
        }

        if (!item->placed && xfdesktop_icon_view_get_next_free_grid_position(icon_view, -1, -1, &row, &col)) {
            xfdesktop_icon_view_place_item_at(icon_view, item, row, col);
        }

        return item->placed;
    } else {
        return FALSE;
    }
}

static gboolean
xfdesktop_icon_view_place_item_in_grid_at(XfdesktopIconView *icon_view, ViewItem **grid_layout, ViewItem *item, gint row, gint col) {
    g_return_val_if_fail(grid_layout != NULL, FALSE);
    g_return_val_if_fail(item != NULL, FALSE);
    g_return_val_if_fail(row >= 0 && row < icon_view->nrows, FALSE);
    g_return_val_if_fail(col >= 0 && col < icon_view->ncols, FALSE);

    if (xfdesktop_icon_view_item_in_grid_slot(icon_view, grid_layout, row, col) == NULL) {
        grid_layout[col * icon_view->nrows + row] = item;
        return TRUE;
    } else {
        return FALSE;
    }
}

static gboolean
xfdesktop_icon_view_place_item_at(XfdesktopIconView *icon_view,
                                  ViewItem *item,
                                  gint row,
                                  gint col)
{
    g_return_val_if_fail(row >= 0 && row < icon_view->nrows, FALSE);
    g_return_val_if_fail(col >= 0 && col < icon_view->ncols, FALSE);
    g_return_val_if_fail(icon_view->grid_layout != NULL, FALSE);

    if (xfdesktop_icon_view_place_item_in_grid_at(icon_view, icon_view->grid_layout, item, row, col)) {
        item->row = row;
        item->col = col;
        item->placed = TRUE;

        xfdesktop_icon_view_invalidate_item(icon_view, item, TRUE);

        GtkTreeIter iter;
        if (view_item_get_iter(item, icon_view->model, &iter)) {
            g_signal_emit(icon_view, __signals[SIG_ICON_MOVED], 0, icon_view, &iter, item->row, item->col);
        }
        return TRUE;
    } else {
        return FALSE;
    }
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
    if (!cairo_region_is_empty(item->icon_slot_region)) {
        cairo_region_destroy(item->icon_slot_region);
        item->icon_slot_region = cairo_region_create();
    }

    if (icon_view->grid_layout != NULL
        && row >= 0 && row < icon_view->nrows
        && col >= 0 && row < icon_view->ncols
        && xfdesktop_icon_view_item_in_slot(icon_view, row, col) == item)
    {
        icon_view->grid_layout[col * icon_view->nrows + row] = NULL;
    }

    if (icon_view->item_under_pointer == item) {
        icon_view->item_under_pointer = NULL;
    }

    if (icon_view->cursor == item) {
        xfdesktop_icon_view_set_cursor(icon_view, NULL, TRUE);
    }

    if (icon_view->first_clicked_item == item) {
        icon_view->first_clicked_item = NULL;
    }

    if (item->selected) {
        item->selected = FALSE;
        icon_view->selected_items = g_list_remove(icon_view->selected_items, item);
        g_signal_emit(icon_view, __signals[SIG_ICON_SELECTION_CHANGED], 0);
    }
}

static void
xfdesktop_icon_view_set_cell_properties(XfdesktopIconView *icon_view,
                                        ViewItem *item)
{
    GtkTreeIter iter;

    g_return_if_fail(GTK_IS_TREE_MODEL(icon_view->model));

    if (icon_view->pixbuf_column != -1) {
        cairo_surface_t *surface = xfdesktop_icon_view_get_surface_for_item(icon_view, item);
        g_object_set(icon_view->icon_renderer,
                     "surface", surface,
                     NULL);
        if (surface != NULL) {
            cairo_surface_destroy(surface);
        }
    }

    if (icon_view->text_column != -1 && view_item_get_iter(item, icon_view->model, &iter)) {
        gchar *text = NULL;
        gtk_tree_model_get(icon_view->model,
                           &iter,
                           icon_view->text_column, &text,
                           -1);
        g_object_set(icon_view->text_renderer,
                     "text", text,
                     NULL);
        g_free(text);
    }
}

static void
xfdesktop_icon_view_unset_cell_properties(XfdesktopIconView *icon_view)
{
    g_object_set(icon_view->icon_renderer,
                 "surface", NULL,
                 NULL);
    g_object_set(icon_view->text_renderer,
                 "text", NULL,
                 NULL);
}

static void
xfdesktop_icon_view_update_item_extents(XfdesktopIconView *icon_view,
                                        ViewItem *item)
{
    GtkRequisition min_req, nat_req;
    GtkRequisition *req;

    if (!item->placed) {
        if (G_UNLIKELY(!xfdesktop_icon_view_place_item(icon_view, item, TRUE))) {
            return;
        }
    }

    xfdesktop_icon_view_set_cell_properties(icon_view, item);

    // Icon renderer
    gtk_cell_renderer_get_preferred_size(icon_view->icon_renderer, GTK_WIDGET(icon_view), &min_req, NULL);
    item->icon_extents.width = MIN(ICON_WIDTH, min_req.width);
    item->icon_extents.height = MIN(ICON_SIZE, min_req.height);
    item->icon_extents.x = MAX(0, (SLOT_SIZE - item->icon_extents.width) / 2);
    item->icon_extents.y = icon_view->slot_padding + MAX(0, (ICON_SIZE - min_req.height) / 2);

    // Text renderer
    gtk_cell_renderer_get_preferred_size(icon_view->text_renderer, GTK_WIDGET(icon_view), &min_req, &nat_req);
    req = item->selected ? &nat_req : &min_req;
    item->text_extents.width = MIN(SLOT_SIZE, req->width);
    item->text_extents.height = req->height;
    item->text_extents.x = MAX(0, (SLOT_SIZE - item->text_extents.width) / 2);
    item->text_extents.y = icon_view->slot_padding + ICON_SIZE + icon_view->slot_padding;

    if (!cairo_region_is_empty(item->icon_slot_region)) {
        cairo_region_destroy(item->icon_slot_region);
        item->icon_slot_region = cairo_region_create();
    }
    GdkRectangle slot_part_extents;
    xfdesktop_icon_view_shift_to_slot_area(icon_view, item, &item->icon_extents, &slot_part_extents);
    // Ther is a blank space (->slot_padding high) between the icon and the
    // label, which we want included in the region.
    slot_part_extents.height += icon_view->slot_padding;
    cairo_region_union_rectangle(item->icon_slot_region, &slot_part_extents);
    xfdesktop_icon_view_shift_to_slot_area(icon_view, item, &item->text_extents, &slot_part_extents);
    cairo_region_union_rectangle(item->icon_slot_region, &slot_part_extents);

#if 0
    DBG("new icon extents: %dx%d+%d+%d", item->icon_extents.width, item->icon_extents.height, item->icon_extents.x, item->icon_extents.y);
    DBG("new text extents: %dx%d+%d+%d", item->text_extents.width, item->text_extents.height, item->text_extents.x, item->text_extents.y);
    cairo_rectangle_int_t slot_extents;
    cairo_region_get_extents(item->icon_slot_region, &slot_extents);
    DBG("new slot extents: %dx%d+%d+%d", slot_extents.width, slot_extents.height, slot_extents.x, slot_extents.y);
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

    g_return_if_fail(item->row >= 0 && item->row < icon_view->nrows);
    g_return_if_fail(item->col >= 0 && item->col < icon_view->ncols);

    // TODO: check grid slot area against extents and bail early if possible

    xfdesktop_icon_view_set_cell_properties(icon_view, item);

    style_context = gtk_widget_get_style_context(GTK_WIDGET(icon_view));
    state = gtk_widget_get_state_flags(GTK_WIDGET(icon_view));

    gtk_style_context_save(style_context);
    gtk_style_context_add_class(style_context, GTK_STYLE_CLASS_CELL);

    state &= ~(GTK_STATE_FLAG_SELECTED | GTK_STATE_FLAG_PRELIGHT | GTK_STATE_FLAG_FOCUSED);
    has_focus = gtk_widget_has_focus(GTK_WIDGET(icon_view));

    if (G_UNLIKELY(has_focus && item == icon_view->cursor && icon_view->draw_focus)) {
        flags |= GTK_CELL_RENDERER_FOCUSED;
    }

    if (G_UNLIKELY(item->selected)) {
        state |= GTK_STATE_FLAG_SELECTED;
        flags |= GTK_CELL_RENDERER_SELECTED;
    }

    if (G_UNLIKELY(icon_view->item_under_pointer == item)) {
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
                                       icon_view->icon_renderer,
                                       flags,
                                       &item->icon_extents);
    xfdesktop_icon_view_draw_item_cell(icon_view,
                                       style_context,
                                       cr,
                                       area,
                                       item,
                                       icon_view->text_renderer,
                                       flags,
                                       &item->text_extents);

    gtk_style_context_restore(style_context);
}

static gboolean
xfdesktop_icon_view_draw(GtkWidget *widget,
                         cairo_t *cr)
{
    TRACE("entering");
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

    for (GList *l = icon_view->items; l != NULL; l = l->next) {
        ViewItem *item = l->data;
        if (item->placed && !item->selected) {
            xfdesktop_icon_view_draw_item(icon_view, cr, &clipbox, item);
        }
    }
    for (GList *l = icon_view->items; l != NULL; l = l->next) {
        ViewItem *item = l->data;
        if (item->placed && item->selected) {
            xfdesktop_icon_view_draw_item(icon_view, cr, &clipbox, item);
        }
    }

    xfdesktop_icon_view_unset_cell_properties(icon_view);

    if (icon_view->definitely_rubber_banding) {
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

            if (gdk_rectangle_intersect(&temp, &icon_view->band_rect, &intersect)) {
                cairo_save(cr);

                /* paint the rubber band area */
                gdk_cairo_rectangle(cr, &intersect);
                cairo_clip_preserve(cr);
                gtk_render_background(context, cr,
                                      icon_view->band_rect.x,
                                      icon_view->band_rect.y,
                                      icon_view->band_rect.width,
                                      icon_view->band_rect.height);
                gtk_render_frame(context, cr,
                                 icon_view->band_rect.x,
                                 icon_view->band_rect.y,
                                 icon_view->band_rect.width,
                                 icon_view->band_rect.height);

                cairo_restore(cr);
            }
        }

        gtk_style_context_remove_class(context, GTK_STYLE_CLASS_RUBBERBAND);
        gtk_style_context_restore(context);
    } else if (icon_view->highlight_row != -1 && icon_view->highlight_col != -1) {
        gint x, y;
        if (xfdesktop_icon_view_slot_coords_to_widget_coords(icon_view,
                                                             icon_view->highlight_row,
                                                             icon_view->highlight_col,
                                                             &x,
                                                             &y))
        {
            GtkStyleContext *style_context = gtk_widget_get_style_context(widget);
            gtk_render_focus(style_context, cr, x, y, SLOT_SIZE, SLOT_SIZE);
        }
    }

    cairo_rectangle_list_destroy(rects);

    return FALSE;
}

static void
xfdesktop_icon_view_select_between(XfdesktopIconView *icon_view,
                                   ViewItem *start_item,
                                   ViewItem *end_item)
{
    gint start_row, start_col, end_row, end_col;

    g_return_if_fail(start_item->row >= 0 && start_item->row < icon_view->nrows);
    g_return_if_fail(start_item->col >= 0 && start_item->col < icon_view->ncols);
    g_return_if_fail(end_item->row >= 0 && end_item->row < icon_view->nrows);
    g_return_if_fail(end_item->col >= 0 && end_item->col < icon_view->ncols);

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
            (i == end_row ? j <= end_col : j < icon_view->ncols);
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

    if (icon_view->items != NULL) {
        for (gint i = 0; i < icon_view->nrows && item == NULL; ++i) {
            for (gint j = 0; j < icon_view->ncols && item == NULL; ++j) {
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

    if (icon_view->items != NULL) {
        for (gint i = icon_view->nrows - 1; i >= 0 && item == NULL; --i) {
            for (gint j = icon_view->ncols - 1; j >= 0 && item == NULL; --j) {
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

    if (icon_view->cursor == NULL) {
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
            xfdesktop_icon_view_set_cursor(icon_view, item, TRUE);
            xfdesktop_icon_view_select_item_internal(icon_view, item, TRUE);
        }
    } else {
        guint remaining = count;
        gint step = direction == GTK_DIR_UP || direction == GTK_DIR_LEFT ? -1 : 1;
        gint row = icon_view->cursor->row;
        gint col = icon_view->cursor->col;
        gboolean row_major = direction == GTK_DIR_LEFT || direction == GTK_DIR_RIGHT;

        if (row < 0 || col < 0) {
            return;
        }

        if (!icon_view->cursor->selected) {
            xfdesktop_icon_view_invalidate_item(icon_view, icon_view->cursor, FALSE);
        }

        if ((modmask & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) == 0) {
            xfdesktop_icon_view_unselect_all(icon_view);
        }

        for(gint i = row_major ? row : col;
            remaining > 0 && (row_major
                              ? (step < 0 ? i >= 0 : i < icon_view->nrows)
                              : (step < 0 ? i >= 0 : i < icon_view->ncols));
            i += step)
        {
            for(gint j = row_major
                         ? (i == row ? col + step : (step < 0) ? icon_view->ncols - 1 : 0)
                         : (i == col ? row + step : (step < 0) ? icon_view->nrows - 1 : 0);
                remaining > 0 && (row_major
                                  ? (step < 0 ? j >= 0 : j < icon_view->ncols)
                                  : (step < 0 ? j >= 0 : j < icon_view->nrows));
                j += step)
            {
                gint slot_row = row_major ? i : j;
                gint slot_col = row_major ? j : i;
                ViewItem *item = xfdesktop_icon_view_item_in_slot(icon_view, slot_row, slot_col);

                if (item != NULL) {
                    xfdesktop_icon_view_set_cursor(icon_view, item, TRUE);
                    if ((modmask & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) != 0 || remaining == 1) {
                        xfdesktop_icon_view_select_item_internal(icon_view, item, TRUE);
                    }
                    remaining -= 1;
                }
            }
        }

        if (icon_view->selected_items == NULL) {
            ViewItem *item;
            if (step < 0) {
                item = xfdesktop_icon_view_find_first_icon(icon_view);
            } else {
                item = xfdesktop_icon_view_find_last_icon(icon_view);
            }

            if (item != NULL) {
                xfdesktop_icon_view_set_cursor(icon_view, item, TRUE);
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
        ViewItem *old_cursor = icon_view->cursor;
        xfdesktop_icon_view_set_cursor(icon_view, item, TRUE);

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
xfdesktop_icon_view_move_cursor(XfdesktopIconView *icon_view, GtkMovementStep step, gint count) {
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

static void
xfdesktop_icon_view_temp_unplace_items(XfdesktopIconView *icon_view)
{
    for (GList *l = icon_view->items; l != NULL; l = l->next) {
        ViewItem *item = l->data;
        if (item->placed) {
            xfdesktop_icon_view_unplace_item(icon_view, item);
        }
    }
}

static void
xfdesktop_icon_view_place_items(XfdesktopIconView *icon_view)
{
    if (icon_view->grid_layout != NULL) {
        // First try to place items that already had locations set
        for (GList *l = icon_view->items; l != NULL; l = l->next) {
            ViewItem *item = l->data;

            if (!item->placed) {
                if (item->row < 0 || item->row >= icon_view->nrows
                    || item->col < 0 || item->col >= icon_view->ncols)
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
        for (GList *l = icon_view->items; l != NULL; l = l->next) {
            ViewItem *item = l->data;
            if (!item->placed) {
                xfdesktop_icon_view_place_item(icon_view, item, TRUE);
            }
        }
    }
}

static gboolean
_xfdesktop_icon_view_build_grid_params(XfdesktopIconView *icon_view,
                                       GtkAllocation *allocation,
                                       GridParams *grid_params)
{
    DBG("icon view size: %dx%d", allocation->width, allocation->height);

    gint new_nrows = MAX((allocation->height - MIN_MARGIN * 2) / SLOT_SIZE, 0);
    gint new_ncols = MAX((allocation->width - MIN_MARGIN * 2) / SLOT_SIZE, 0);
    if (new_nrows <= 0 || new_ncols <= 0) {
        return FALSE;
    } else {
        DBG("new grid size: rows=%d, cols=%d", new_nrows, new_ncols);
        grid_params->nrows = new_nrows;
        grid_params->ncols = new_ncols;

        gint xrest = allocation->width - grid_params->ncols * SLOT_SIZE;
        grid_params->xspacing = new_ncols > 1 ? (xrest - MIN_MARGIN * 2) / (new_ncols - 1) : 1;
        grid_params->xmargin = (xrest - (grid_params->ncols - 1) * grid_params->xspacing) / 2;

        gint yrest = allocation->height - grid_params->nrows * SLOT_SIZE;
        grid_params->yspacing = new_nrows > 1 ? (yrest - MIN_MARGIN * 2) / (new_nrows - 1) : 1;
        grid_params->ymargin = (yrest - (grid_params->nrows - 1) * grid_params->yspacing) / 2;

        DBG("margin: (%d, %d), spacing: (%d, %d)",
            grid_params->xmargin, grid_params->ymargin,
            grid_params->xspacing, grid_params->yspacing);

        return TRUE;
    }
}

static void
xfdesktop_icon_view_size_grid(XfdesktopIconView *icon_view)
{
    if (!gtk_widget_get_realized(GTK_WIDGET(icon_view))) {
        return;
    }

    DBG("entering");

    gint old_nrows = icon_view->nrows;
    gint old_ncols = icon_view->ncols;
    gsize old_size = grid_layout_bytes(old_nrows, old_ncols);
    gint old_xmargin = icon_view->xmargin;
    gint old_ymargin = icon_view->ymargin;
    gint old_xspacing = icon_view->xspacing;
    gint old_yspacing = icon_view->yspacing;

    GtkAllocation allocation;
    gtk_widget_get_allocation(GTK_WIDGET(icon_view), &allocation);

    GridParams new_grid_params;
    if (!_xfdesktop_icon_view_build_grid_params(icon_view, &allocation, &new_grid_params)) {
        return;
    }

    gboolean grid_changed = old_nrows != new_grid_params.nrows || old_ncols != new_grid_params.ncols;
    gsize new_size = grid_layout_bytes(new_grid_params.nrows, new_grid_params.ncols);

    if (grid_changed
        || old_xmargin != new_grid_params.xmargin
        || old_ymargin != new_grid_params.ymargin
        || old_xspacing != new_grid_params.xspacing
        || old_yspacing != new_grid_params.yspacing)
    {
        xfdesktop_icon_view_invalidate_all(icon_view, FALSE);
    }

    if (grid_changed) {
        g_signal_emit(icon_view, __signals[SIG_START_GRID_RESIZE], 0, new_grid_params.nrows, new_grid_params.ncols);
        xfdesktop_icon_view_temp_unplace_items(icon_view);
    }

    icon_view->nrows = new_grid_params.nrows;
    icon_view->ncols = new_grid_params.ncols;
    icon_view->xmargin = new_grid_params.xmargin;
    icon_view->ymargin = new_grid_params.ymargin;
    icon_view->xspacing = new_grid_params.xspacing;
    icon_view->yspacing = new_grid_params.yspacing;

    if (icon_view->grid_layout == NULL) {
        icon_view->grid_layout = g_malloc0(new_size);
    } else if (old_size != new_size) {
        DBG("old_size != new_size; resizing grid");
        icon_view->grid_layout = g_realloc(icon_view->grid_layout, new_size);

        if (new_size > old_size) {
            memset(((guint8 *)icon_view->grid_layout) + old_size, 0, new_size - old_size);
        }
    }

    if (grid_changed) {
        g_signal_emit(icon_view, __signals[SIG_END_GRID_RESIZE], 0);
        xfdesktop_icon_view_place_items(icon_view);
    }
    g_signal_emit(G_OBJECT(icon_view), __signals[SIG_RESIZE_EVENT], 0, NULL);

    DBG("SLOT_SIZE=%0.3f, TEXT_WIDTH=%0.3f, ICON_SIZE=%u", SLOT_SIZE, TEXT_WIDTH, ICON_SIZE);
    DBG("grid size is %dx%d", icon_view->nrows, icon_view->ncols);

    XF_DEBUG("created grid_layout with %lu positions", (gulong)(new_size/sizeof(gpointer)));
    DUMP_GRID_LAYOUT(icon_view);
}

static gboolean
xfdesktop_icon_view_queue_draw_item(XfdesktopIconView *icon_view,
                                    ViewItem *item)
{
    if (!cairo_region_is_empty(item->icon_slot_region)) {
        GdkRectangle slot_rect = {
            .x = 0,
            .y = 0,
            .width = SLOT_SIZE,
            .height = SLOT_SIZE,
        };
        xfdesktop_icon_view_shift_to_slot_area(icon_view, item, &slot_rect, &slot_rect);
        cairo_region_t *draw_region = cairo_region_create_rectangle(&slot_rect);
        cairo_region_union(draw_region, item->icon_slot_region);

        gtk_widget_queue_draw_region(GTK_WIDGET(icon_view), draw_region);
        cairo_region_destroy(draw_region);

        return TRUE;
    } else {
        return FALSE;
    }
}

static void
xfdesktop_icon_view_invalidate_item(XfdesktopIconView *icon_view,
                                    ViewItem *item,
                                    gboolean recalc_extents)
{
    g_return_if_fail(item != NULL);

    if (item->row >= 0 && item->row < icon_view->nrows && item->col >= 0 && item->col < icon_view->ncols) {
        if (!xfdesktop_icon_view_queue_draw_item(icon_view, item)) {
            recalc_extents = TRUE;
        }

        if (recalc_extents) {
            xfdesktop_icon_view_update_item_extents(icon_view, item);
            xfdesktop_icon_view_queue_draw_item(icon_view, item);
        }
    }
}

static void
xfdesktop_icon_view_invalidate_item_text(XfdesktopIconView *icon_view, ViewItem *item) {
    g_return_if_fail(item != NULL);

    GdkRectangle text_slot_extents;
    if (item->text_extents.width > 0
        && item->text_extents.height > 0
        && xfdesktop_icon_view_shift_to_slot_area(icon_view, item, &item->text_extents, &text_slot_extents))
    {
        gtk_widget_queue_draw_area(GTK_WIDGET(icon_view),
                                   text_slot_extents.x, text_slot_extents.y,
                                   text_slot_extents.width, text_slot_extents.height);
    }
}

static void
xfdesktop_icon_view_select_item_internal(XfdesktopIconView *icon_view,
                                         ViewItem *item,
                                         gboolean emit_signal)
{
    if (!item->selected) {
        if (icon_view->sel_mode == GTK_SELECTION_SINGLE) {
            xfdesktop_icon_view_unselect_all(icon_view);
        }

        item->selected = TRUE;
        icon_view->selected_items = g_list_prepend(icon_view->selected_items, item);

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
        icon_view->selected_items = g_list_remove(icon_view->selected_items, item);

        xfdesktop_icon_view_invalidate_item(icon_view, item, TRUE);

        if (emit_signal) {
            g_signal_emit(icon_view, __signals[SIG_ICON_SELECTION_CHANGED], 0);
        }
    }
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
        if ((icon_view->gravity & XFDESKTOP_ICON_VIEW_GRAVITY_BOTTOM) != 0) {
            *next_row = icon_view->nrows - 1;
        } else {
            *next_row = 0;
        }

        if ((icon_view->gravity & XFDESKTOP_ICON_VIEW_GRAVITY_RIGHT) != 0) {
            *next_col = icon_view->ncols - 1;
        } else {
            *next_col = 0;
        }
    } else if ((icon_view->gravity & XFDESKTOP_ICON_VIEW_GRAVITY_HORIZONTAL) != 0) {
        if ((icon_view->gravity & XFDESKTOP_ICON_VIEW_GRAVITY_RIGHT) != 0) {
            col -= 1;
        } else {
            col += 1;
        }

        if (col < 0 || col >= icon_view->ncols) {
            if (col < 0) {
                col = icon_view->ncols - 1;
            } else {
                col = 0;
            }

            if ((icon_view->gravity & XFDESKTOP_ICON_VIEW_GRAVITY_BOTTOM) != 0) {
                row -= 1;
                if (row < 0) {
                    return FALSE;
                }
            } else {
                row += 1;
                if (row >= icon_view->nrows) {
                    return FALSE;
                }
            }
        }

        *next_row = row;
        *next_col = col;
    } else {
        if ((icon_view->gravity & XFDESKTOP_ICON_VIEW_GRAVITY_BOTTOM) != 0) {
            row -= 1;
        } else {
            row += 1;
        }

        if (row < 0 || row >= icon_view->nrows) {
            if (row < 0) {
                row = icon_view->nrows - 1;
            } else {
                row = 0;
            }

            if ((icon_view->gravity & XFDESKTOP_ICON_VIEW_GRAVITY_RIGHT) != 0) {
                col -= 1;
                if (col < 0) {
                    return FALSE;
                }
            } else {
                col += 1;
                if (col >= icon_view->ncols) {
                    return FALSE;
                }
            }
        }

        *next_row = row;
        *next_col = col;
    }

    return TRUE;
}

static gboolean
xfdesktop_icon_view_get_next_free_grid_position_for_grid(XfdesktopIconView *icon_view,
                                                         ViewItem **grid_layout,
                                                         gint row,
                                                         gint col,
                                                         gint *next_row,
                                                         gint *next_col)
{
    gint cur_row = row;
    gint cur_col = col;

    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view), FALSE);
    g_return_val_if_fail(grid_layout != NULL, FALSE);
    g_return_val_if_fail(row >= -1 && row < icon_view->nrows, FALSE);
    g_return_val_if_fail(col >= -1 && col < icon_view->ncols, FALSE);
    g_return_val_if_fail(next_row != NULL && next_col != NULL, FALSE);

    while (next_pos(icon_view, cur_row, cur_col, &cur_row, &cur_col)) {
        if (xfdesktop_icon_view_item_in_grid_slot(icon_view, grid_layout, cur_row, cur_col) == NULL) {
            *next_row = cur_row;
            *next_col = cur_col;
            return TRUE;
        }
    }

    return FALSE;
}

gboolean
xfdesktop_icon_view_get_next_free_grid_position(XfdesktopIconView *icon_view,
                                                gint row,
                                                gint col,
                                                gint *next_row,
                                                gint *next_col)
{
    return xfdesktop_icon_view_get_next_free_grid_position_for_grid(icon_view,
                                                                    icon_view->grid_layout,
                                                                    row,
                                                                    col,
                                                                    next_row,
                                                                    next_col);
}

static inline ViewItem *
xfdesktop_icon_view_item_in_grid_slot(XfdesktopIconView *icon_view, ViewItem **grid_layout, gint row, gint col) {
    g_return_val_if_fail(grid_layout != NULL, NULL);
    g_return_val_if_fail(row >= 0 && row < icon_view->nrows, NULL);
    g_return_val_if_fail(col >= 0 && col < icon_view->ncols, NULL);

    gsize idx = col * icon_view->nrows + row;
    return grid_layout[idx];
}

static inline ViewItem *
xfdesktop_icon_view_item_in_slot(XfdesktopIconView *icon_view, gint row, gint col) {
    return xfdesktop_icon_view_item_in_grid_slot(icon_view, icon_view->grid_layout, row, col);
}

static gint
xfdesktop_check_icon_clicked(gconstpointer data,
                             gconstpointer user_data)
{
    ViewItem *item = (ViewItem *)data;
    GdkEventButton *evt = (GdkEventButton *)user_data;

    return cairo_region_contains_point(item->icon_slot_region, evt->x, evt->y) ? 0 : 1;
}

static void
xfdesktop_icon_view_populate_items(XfdesktopIconView *icon_view)
{
    g_return_if_fail(icon_view->model != NULL);
    g_return_if_fail(icon_view->items == NULL);

    GtkTreeIter iter;
    if (gtk_tree_model_get_iter_first(icon_view->model, &iter)) {
        do {
            ViewItem *item = view_item_new(icon_view->model, &iter);
            GtkTreePath *path;
            gint index;

            path = gtk_tree_model_get_path(icon_view->model, &iter);
            index = gtk_tree_path_get_indices(path)[0];
            gtk_tree_path_free(path);

            icon_view->items = g_list_insert(icon_view->items, item, index);

            if (icon_view->row_column != -1 && icon_view->col_column != -1) {
                gint row, col;

                gtk_tree_model_get(icon_view->model, &iter,
                                   icon_view->row_column, &row,
                                   icon_view->col_column, &col,
                                   -1);
            }
        } while (gtk_tree_model_iter_next(icon_view->model, &iter));
    }

    xfdesktop_icon_view_place_items(icon_view);
}

static void
xfdesktop_icon_view_connect_model_signals(XfdesktopIconView *icon_view)
{
    g_signal_connect(icon_view->model, "row-inserted",
                     G_CALLBACK(xfdesktop_icon_view_model_row_inserted), icon_view);
    g_signal_connect(icon_view->model, "row-changed",
                     G_CALLBACK(xfdesktop_icon_view_model_row_changed), icon_view);
    g_signal_connect(icon_view->model, "row-deleted",
                     G_CALLBACK(xfdesktop_icon_view_model_row_deleted), icon_view);

}

static void
xfdesktop_icon_view_disconnect_model_signals(XfdesktopIconView *icon_view)
{
    g_return_if_fail(icon_view->model != NULL);

    g_signal_handlers_disconnect_by_func(icon_view->model,
                                         G_CALLBACK(xfdesktop_icon_view_model_row_inserted),
                                         icon_view);
    g_signal_handlers_disconnect_by_func(icon_view->model,
                                         G_CALLBACK(xfdesktop_icon_view_model_row_changed),
                                         icon_view);
    g_signal_handlers_disconnect_by_func(icon_view->model,
                                         G_CALLBACK(xfdesktop_icon_view_model_row_deleted),
                                         icon_view);
}

static void
xfdesktop_icon_view_model_row_inserted(GtkTreeModel *model,
                                       GtkTreePath *path,
                                       GtkTreeIter *iter,
                                       XfdesktopIconView *icon_view)
{
    ViewItem *item = view_item_new(icon_view->model, iter);
    gint idx = gtk_tree_path_get_indices(path)[0];

    DBG("entering, index=%d", gtk_tree_path_get_indices(path)[0]);

    icon_view->items = g_list_insert(icon_view->items, item, idx);

    if (xfdesktop_icon_view_place_item(icon_view, item, TRUE)) {
        DBG("placed new icon at (%d, %d)", item->row, item->col);
    } else {
        DBG("failed to place new icon");
    }
}

static void
xfdesktop_icon_view_model_row_changed(GtkTreeModel *model,
                                      GtkTreePath *path,
                                      GtkTreeIter *iter,
                                      XfdesktopIconView *icon_view)
{
    ViewItem *item = g_list_nth_data(icon_view->items, gtk_tree_path_get_indices(path)[0]);

    if (item != NULL) {
        if (item->pixbuf_surface != NULL) {
            cairo_surface_destroy(item->pixbuf_surface);
            item->pixbuf_surface = NULL;
        }
        xfdesktop_icon_view_invalidate_item(icon_view, item, TRUE);

        if (item->placed && icon_view->row_column != -1 && icon_view->col_column != -1) {
            gint row, col;
            gtk_tree_model_get(model, iter,
                               icon_view->row_column, &row,
                               icon_view->col_column, &col,
                               -1);

            if (row != item->row || col != item->col) {
                xfdesktop_icon_view_unplace_item(icon_view, item);
                xfdesktop_icon_view_place_item(icon_view, item, TRUE);
            }
        }
    }
}

static void
xfdesktop_icon_view_model_row_deleted(GtkTreeModel *model,
                                      GtkTreePath *path,
                                      XfdesktopIconView *icon_view)
{
    GList *item_l = g_list_nth(icon_view->items, gtk_tree_path_get_indices(path)[0]);

    if (item_l != NULL) {
        ViewItem *item = item_l->data;

        if (item->placed) {
            xfdesktop_icon_view_unplace_item(icon_view, item);
        }

        icon_view->items = g_list_delete_link(icon_view->items, item_l);

        view_item_free(item);
    }
}

static void
xfdesktop_icon_view_clear_grid_layout(XfdesktopIconView *icon_view)
{
    gsize size = grid_layout_bytes(icon_view->nrows, icon_view->ncols);
    if (size > 0 && icon_view->grid_layout != NULL) {
        memset(icon_view->grid_layout, 0, size);
    }
}

static void
xfdesktop_icon_view_items_free(XfdesktopIconView *icon_view)
{
    xfdesktop_icon_view_clear_grid_layout(icon_view);

    icon_view->item_under_pointer = NULL;
    icon_view->cursor = NULL;
    icon_view->first_clicked_item = NULL;

    g_list_free(icon_view->selected_items);
    icon_view->selected_items = NULL;

    g_list_free_full(icon_view->items, (GDestroyNotify)view_item_free);
    icon_view->items = NULL;
}


/* public api */

guint
xfdesktop_icon_view_get_icon_drag_info(void) {
    return TARGET_XFDESKTOP_ICON;
}

GdkAtom
xfdesktop_icon_view_get_icon_drag_target(void) {
    return gdk_atom_intern(XFDESKTOP_ICON_NAME, FALSE);
}

GtkWidget *
xfdesktop_icon_view_new(XfconfChannel *channel, XfwScreen *screen) {
    return g_object_new(XFDESKTOP_TYPE_ICON_VIEW,
                        "channel", channel,
                        "screen", screen,
                        NULL);
}

GtkWidget *
xfdesktop_icon_view_new_with_model(XfconfChannel *channel, XfwScreen *screen, GtkTreeModel *model) {
    g_return_val_if_fail(GTK_IS_TREE_MODEL(model), NULL);

    return g_object_new(XFDESKTOP_TYPE_ICON_VIEW,
                        "channel", channel,
                        "screen", screen,
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

    if (model == icon_view->model) {
        return;
    }

    if (model != NULL) {
        if (!xfdesktop_icon_view_validate_column_type(icon_view, model, icon_view->pixbuf_column, G_TYPE_ICON, "pixbuf-column")
            || !xfdesktop_icon_view_validate_column_type(icon_view, model, icon_view->text_column, G_TYPE_STRING, "text-column")
            || !xfdesktop_icon_view_validate_column_type(icon_view, model, icon_view->search_column, G_TYPE_STRING, "search-column")
            || !xfdesktop_icon_view_validate_column_type(icon_view, model, icon_view->tooltip_icon_column, G_TYPE_ICON, "tooltip-icon-column")
            || !xfdesktop_icon_view_validate_column_type(icon_view, model, icon_view->tooltip_text_column, G_TYPE_STRING, "tooltip-text-column")
            || !xfdesktop_icon_view_validate_column_type(icon_view, model, icon_view->row_column, G_TYPE_INT, "row-column")
            || !xfdesktop_icon_view_validate_column_type(icon_view, model, icon_view->col_column, G_TYPE_INT, "col-column"))
        {
            return;
        }
    }

    if (icon_view->model != NULL) {
        if (gtk_widget_get_realized(GTK_WIDGET(icon_view))) {
            xfdesktop_icon_view_invalidate_all(icon_view, FALSE);
        }
        xfdesktop_icon_view_disconnect_model_signals(icon_view);
        xfdesktop_icon_view_items_free(icon_view);
        g_clear_object(&icon_view->model);
    }

    if (model != NULL) {
        icon_view->model = g_object_ref(model);
        xfdesktop_icon_view_connect_model_signals(icon_view);
        xfdesktop_icon_view_populate_items(icon_view);
    }

    g_object_notify(G_OBJECT(icon_view), "model");
}

GtkTreeModel *
xfdesktop_icon_view_get_model(XfdesktopIconView *icon_view)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view), NULL);
    return icon_view->model;
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
    } else if (icon_view->model != NULL
               && !xfdesktop_icon_view_validate_column_type(icon_view,
                                                            icon_view->model,
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

    changed = xfdesktop_icon_view_set_column(icon_view, column, &icon_view->pixbuf_column, G_TYPE_ICON, "pixbuf-column");
    if (changed) {
        xfdesktop_icon_view_invalidate_all(icon_view, TRUE);
    }

    g_object_thaw_notify(G_OBJECT(icon_view));
}

void
xfdesktop_icon_view_set_icon_opacity_column(XfdesktopIconView *icon_view, gint column) {
    gboolean changed;

    // Delay notify for pixbuf-column until after we set up the column
    g_object_freeze_notify(G_OBJECT(icon_view));

    changed = xfdesktop_icon_view_set_column(icon_view, column, &icon_view->icon_opacity_column, G_TYPE_DOUBLE, "icon-opacity-column");
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

    changed = xfdesktop_icon_view_set_column(icon_view, column, &icon_view->text_column, G_TYPE_STRING, "text-column");
    if (changed) {
        xfdesktop_icon_view_invalidate_all(icon_view, TRUE);
    }

    g_object_thaw_notify(G_OBJECT(icon_view));
}

void
xfdesktop_icon_view_set_search_column(XfdesktopIconView *icon_view,
                                      gint column)
{
    xfdesktop_icon_view_set_column(icon_view, column, &icon_view->search_column, G_TYPE_STRING, "search-column");
}

void
xfdesktop_icon_view_set_sort_priority_column(XfdesktopIconView *icon_view,
                                             gint column)
{
    xfdesktop_icon_view_set_column(icon_view, column, &icon_view->sort_priority_column, G_TYPE_INT, "sort-priority-column");
}

void
xfdesktop_icon_view_set_tooltip_icon_column(XfdesktopIconView *icon_view,
                                               gint column)
{
    xfdesktop_icon_view_set_column(icon_view, column, &icon_view->tooltip_icon_column, G_TYPE_ICON, "tooltip-icon-column");
}

void
xfdesktop_icon_view_set_tooltip_text_column(XfdesktopIconView *icon_view,
                                            gint column)
{
    xfdesktop_icon_view_set_column(icon_view, column, &icon_view->tooltip_text_column, G_TYPE_STRING, "tooltip-text-column");
}

void
xfdesktop_icon_view_set_row_column(XfdesktopIconView *icon_view,
                                   gint column)
{
    xfdesktop_icon_view_set_column(icon_view, column, &icon_view->row_column, G_TYPE_INT, "row-column");
}

void
xfdesktop_icon_view_set_col_column(XfdesktopIconView *icon_view,
                                   gint column)
{
    xfdesktop_icon_view_set_column(icon_view, column, &icon_view->col_column, G_TYPE_INT, "col-column");
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
            icon_view->allow_rubber_banding = FALSE;

            if (icon_view->selected_items != NULL) {
                for(GList *l = icon_view->selected_items->next; l != NULL; l = l->next) {
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
            icon_view->allow_rubber_banding = TRUE;
            break;
    }

    if (new_mode != icon_view->sel_mode) {
        icon_view->sel_mode = mode;
    }
}

GtkSelectionMode
xfdesktop_icon_view_get_selection_mode(XfdesktopIconView *icon_view)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view),
                         GTK_SELECTION_NONE);

    return icon_view->sel_mode;
}

void
xfdesktop_icon_view_enable_drag_source(XfdesktopIconView *icon_view,
                                       GdkModifierType start_button_mask,
                                       const GtkTargetEntry *targets,
                                       gint n_targets,
                                       GdkDragAction actions)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));
    g_return_if_fail((targets != NULL && n_targets > 0) || n_targets == 0);

    gtk_target_list_unref(icon_view->source_targets);
    icon_view->source_targets = gtk_target_list_new(targets, n_targets);
    // Add our internal targets first because _add_table() prepends, and we
    // want ours to be found first.
    gtk_target_list_add_table(icon_view->source_targets,
                              icon_view_targets,
                              icon_view_n_targets);

    gtk_drag_source_set(GTK_WIDGET(icon_view), 0, NULL, 0, GDK_ACTION_MOVE | actions);
    icon_view->foreign_source_actions = actions;
    icon_view->foreign_source_mask = start_button_mask;

    icon_view->drag_source_set = TRUE;
}

void
xfdesktop_icon_view_enable_drag_dest(XfdesktopIconView *icon_view,
                                     const GtkTargetEntry *targets,
                                     gint n_targets,
                                     GdkDragAction actions)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));
    g_return_if_fail((targets != NULL && n_targets > 0) || n_targets == 0);

    gtk_target_list_unref(icon_view->dest_targets);
    icon_view->dest_targets = gtk_target_list_new(targets, n_targets);
    // Add our internal targets first because _add_table() prepends, and we
    // want ours to be found first.
    gtk_target_list_add_table(icon_view->dest_targets,
                              icon_view_targets,
                              icon_view_n_targets);

    gtk_drag_dest_set(GTK_WIDGET(icon_view), 0, NULL, 0, GDK_ACTION_MOVE | actions);
    gtk_drag_dest_set_target_list(GTK_WIDGET(icon_view), icon_view->dest_targets);
    icon_view->foreign_dest_actions = actions;

    icon_view->drag_dest_set = TRUE;
}

void
xfdesktop_icon_view_unset_drag_source(XfdesktopIconView *icon_view)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));

    if (icon_view->drag_source_set) {
        gtk_target_list_unref(icon_view->source_targets);
        icon_view->source_targets = gtk_target_list_new(icon_view_targets, icon_view_n_targets);

        gtk_drag_source_set(GTK_WIDGET(icon_view), 0, NULL, 0, GDK_ACTION_MOVE);

        icon_view->drag_source_set = FALSE;
    }
}

void
xfdesktop_icon_view_unset_drag_dest(XfdesktopIconView *icon_view)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));

    if (icon_view->drag_dest_set) {
        gtk_target_list_unref(icon_view->dest_targets);
        icon_view->dest_targets = gtk_target_list_new(icon_view_targets, icon_view_n_targets);

        gtk_drag_dest_set(GTK_WIDGET(icon_view), 0, NULL, 0, GDK_ACTION_MOVE);

        icon_view->drag_dest_set = FALSE;
    }
}

void
xfdesktop_icon_view_draw_highlight(XfdesktopIconView *icon_view, gint row, gint col) {
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));
    g_return_if_fail(row >= 0 && row < icon_view->nrows);
    g_return_if_fail(col >= 0 && col < icon_view->ncols);

    if (row != icon_view->highlight_row || col != icon_view->highlight_col) {
        xfdesktop_icon_view_unset_highlight(icon_view);

        gint x, y;
        if (xfdesktop_icon_view_slot_coords_to_widget_coords(icon_view, row, col, &x, &y)) {
            icon_view->highlight_row = row;
            icon_view->highlight_col = col;

            gtk_widget_queue_draw_area(GTK_WIDGET(icon_view), x, y, SLOT_SIZE, SLOT_SIZE);
        }
    }
}

void
xfdesktop_icon_view_unset_highlight(XfdesktopIconView *icon_view) {
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));

    if (icon_view->highlight_row != -1 && icon_view->highlight_col != -1) {
        gint row = icon_view->highlight_row;
        gint col = icon_view->highlight_col;

        icon_view->highlight_row = -1;
        icon_view->highlight_col = -1;

        gint x, y;
        if (xfdesktop_icon_view_slot_coords_to_widget_coords(icon_view, row, col, &x, &y)) {
            gtk_widget_queue_draw_area(GTK_WIDGET(icon_view), x, y, SLOT_SIZE, SLOT_SIZE);
        }
    }
}


GtkTargetList *
xfdesktop_icon_view_get_drag_dest_targets(XfdesktopIconView *icon_view) {
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view), NULL);
    return icon_view->dest_targets;
}

static ViewItem *
xfdesktop_icon_view_widget_coords_to_item_internal(XfdesktopIconView *icon_view,
                                                   gint wx,
                                                   gint wy)
{
    gint row, col;

    if (xfdesktop_icon_view_widget_coords_to_slot_coords(icon_view, wx, wy, &row, &col)) {
        return xfdesktop_icon_view_item_in_slot(icon_view, row, col);
    } else {
        return NULL;
    }

}

gboolean
xfdesktop_icon_view_widget_coords_to_item(XfdesktopIconView *icon_view,
                                          gint wx,
                                          gint wy,
                                          GtkTreeIter *iter)
{
    ViewItem *item;

    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view), FALSE);
    g_return_val_if_fail(icon_view->model != NULL, FALSE);

    item = xfdesktop_icon_view_widget_coords_to_item_internal(icon_view, wx, wy);
    if (item != NULL) {
        return iter != NULL ? view_item_get_iter(item, icon_view->model, iter) : TRUE;
    } else {
        return FALSE;
    }
}

gboolean
xfdesktop_icon_view_widget_coords_to_slot_coords(XfdesktopIconView *icon_view,
                                                 gint wx,
                                                 gint wy,
                                                 gint *row_out,
                                                 gint *col_out)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view), FALSE);

    gint row = (wy - icon_view->ymargin) / (SLOT_SIZE + icon_view->yspacing);
    gint col = (wx - icon_view->xmargin) / (SLOT_SIZE + icon_view->xspacing);
    if (row >= 0 && row < icon_view->nrows && col >= 0 && col < icon_view->ncols) {
        if (row_out != NULL) {
            *row_out = row;
        }
        if (col_out != NULL) {
            *col_out = col;
        }
        return TRUE;
    } else {
        return FALSE;
    }
}

gboolean
xfdesktop_icon_view_slot_coords_to_widget_coords(XfdesktopIconView *icon_view,
                                                 gint row,
                                                 gint col,
                                                 gint *wx_out,
                                                 gint *wy_out)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view), FALSE);
    g_return_val_if_fail(row >= 0, FALSE);
    g_return_val_if_fail(col >= 0, FALSE);

    if (row < icon_view->nrows && col < icon_view->ncols) {
        if (wx_out != NULL) {
            *wx_out = icon_view->xmargin + col * SLOT_SIZE + col * icon_view->xspacing;
        }
        if (wy_out != NULL) {
            *wy_out = icon_view->ymargin + row * SLOT_SIZE + row * icon_view->yspacing;
        }
        return TRUE;
    } else {
        return FALSE;
    }
}

gboolean
xfdesktop_icon_view_get_cursor(XfdesktopIconView *icon_view, GtkTreeIter *iter, gint *row, gint *col) {
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view), FALSE);

    if (icon_view->cursor != NULL) {
        if (iter != NULL) {
            if (!view_item_get_iter(icon_view->cursor, GTK_TREE_MODEL(icon_view->model), iter)) {
                return FALSE;
            }
        }

        if (row != NULL) {
            *row = icon_view->cursor->row;
        }
        if (col != NULL) {
            *col = icon_view->cursor->col;
        }

        return TRUE;
    } else {
        return FALSE;
    }
}

GList *
xfdesktop_icon_view_get_selected_items(XfdesktopIconView *icon_view)
{
    GList *paths = NULL;

    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view), NULL);

    for (GList *l = icon_view->selected_items; l != NULL; l = l->next) {
        ViewItem *item = l->data;
        GtkTreePath *path = view_item_get_path(item, icon_view->model);
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

    g_return_val_if_fail(icon_view->model != NULL, NULL);

    path = gtk_tree_model_get_path(icon_view->model, iter);
    if (G_LIKELY(path != NULL)) {
        item = g_list_nth_data(icon_view->items, gtk_tree_path_get_indices(path)[0]);
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
    g_return_if_fail(icon_view->model != NULL);
    g_return_if_fail(iter != NULL);

    item = xfdesktop_icon_view_find_item(icon_view, iter);
    if (item != NULL && !item->selected) {
        xfdesktop_icon_view_select_item_internal(icon_view, item, TRUE);
    }
}

void
xfdesktop_icon_view_toggle_cursor(XfdesktopIconView *icon_view) {
    DBG("entering");

    if (icon_view->cursor != NULL) {
        if (icon_view->cursor->selected) {
            xfdesktop_icon_view_unselect_item_internal(icon_view, icon_view->cursor, TRUE);
        } else {
            xfdesktop_icon_view_select_item_internal(icon_view, icon_view->cursor, TRUE);
        }
    }
}

void
xfdesktop_icon_view_select_all(XfdesktopIconView *icon_view)
{
    gboolean selected_something = FALSE;

    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));

    if (icon_view->sel_mode == GTK_SELECTION_MULTIPLE) {
        for (GList *l = icon_view->items; l != NULL; l = l->next) {
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
}

void
xfdesktop_icon_view_unselect_item(XfdesktopIconView *icon_view,
                                  GtkTreeIter *iter)
{
    ViewItem *item;

    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));
    g_return_if_fail(icon_view->model != NULL);
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

    for (GList *l = icon_view->items; l != NULL; l = l->next) {
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

    if (icon_size == icon_view->icon_size) {
        return;
    }

    icon_view->icon_size = icon_size;

    xfdesktop_icon_view_invalidate_pixbuf_cache(icon_view);
    xfdesktop_icon_view_size_grid(icon_view);

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
    return icon_view->icon_size;
}

void
xfdesktop_icon_view_set_font_size(XfdesktopIconView *icon_view,
                                  gdouble font_size_points)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));

    if (font_size_points == icon_view->font_size) {
        return;
    }

    icon_view->font_size = font_size_points;

    g_object_set(icon_view->text_renderer,
                 "size-points", font_size_points,
                 NULL);

    if (icon_view->font_size_set && gtk_widget_get_realized(GTK_WIDGET(icon_view))) {
        xfdesktop_icon_view_size_grid(icon_view);
        xfdesktop_icon_view_invalidate_all(icon_view, TRUE);
    }

    g_object_notify(G_OBJECT(icon_view), "icon-font-size");
}

gdouble
xfdesktop_icon_view_get_font_size(XfdesktopIconView *icon_view)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view), 0.0);
    return icon_view->font_size;
}

void
xfdesktop_icon_view_set_use_font_size(XfdesktopIconView *icon_view,
                                      gboolean use_font_size)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));

    if (use_font_size == icon_view->font_size_set) {
        return;
    }

    icon_view->font_size_set = use_font_size;

    g_object_set(icon_view->text_renderer,
                 "size-points-set", use_font_size,
                 NULL);

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

    if (center_text == icon_view->center_text) {
        return;
    }

    icon_view->center_text = center_text;

    g_object_set(icon_view->text_renderer,
                 "alignment", center_text
                 ? PANGO_ALIGN_CENTER
                 : (gtk_widget_get_direction(GTK_WIDGET(icon_view)) == GTK_TEXT_DIR_RTL
                    ? PANGO_ALIGN_RIGHT
                    : PANGO_ALIGN_LEFT),
                 "align-set", TRUE,
                 NULL);

    if (gtk_widget_get_realized(GTK_WIDGET(icon_view))) {
        xfdesktop_icon_view_invalidate_all(icon_view, TRUE);
    }

    g_object_notify(G_OBJECT(icon_view), "icon-center-text");
}

static void
insert_icon_label_fg_color_attrs(XfdesktopIconView *icon_view) {
    PangoAttribute *attr_fg = pango_attr_foreground_new(round(icon_view->label_fg_color.red * G_MAXUINT16),
                                                        round(icon_view->label_fg_color.green * G_MAXUINT16),
                                                        round(icon_view->label_fg_color.blue * G_MAXUINT16));
    attr_fg->start_index = 0;
    attr_fg->end_index = -1;
    PangoAttribute *attr_alpha = pango_attr_foreground_alpha_new(round(icon_view->label_fg_color.alpha * G_MAXUINT16));
    attr_alpha->start_index = 0;
    attr_alpha->end_index = -1;

    PangoAttrList *attrs = NULL;
    g_object_get(icon_view->text_renderer,
                 "attributes", &attrs,
                 NULL);

    if (attrs != NULL) {
        pango_attr_list_change(attrs, attr_fg);
        pango_attr_list_change(attrs, attr_alpha);
    } else {
        attrs = pango_attr_list_new();
        pango_attr_list_insert(attrs, attr_fg);
        pango_attr_list_insert(attrs, attr_alpha);
    }

    g_object_set(icon_view->text_renderer,
                 "attributes", attrs,
                 NULL);
    pango_attr_list_unref(attrs);
}

void
xfdesktop_icon_view_set_icon_label_fg_color(XfdesktopIconView *icon_view, GdkRGBA *color) {
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));
    g_return_if_fail(color != NULL);

    if (color->red != icon_view->label_fg_color.red
        || color->green != icon_view->label_fg_color.green
        || color->blue != icon_view->label_fg_color.blue
        || color->alpha != icon_view->label_fg_color.alpha)
    {
        icon_view->label_fg_color = *color;

        if (icon_view->label_fg_color_set) {
            insert_icon_label_fg_color_attrs(icon_view);

            for (GList *l = icon_view->items; l != NULL; l = l->next) {
                ViewItem *item = l->data;
                xfdesktop_icon_view_invalidate_item_text(icon_view, item);
            }
        }

        g_object_notify(G_OBJECT(icon_view), "icon-label-fg-color");
    }
}

static gboolean
remove_fg_color_attrs(PangoAttribute *attr, gpointer user_data) {
    return attr->klass->type == PANGO_ATTR_FOREGROUND
        || attr->klass->type == PANGO_ATTR_FOREGROUND_ALPHA;
}

void
xfdesktop_icon_view_set_use_icon_label_fg_color(XfdesktopIconView *icon_view, gboolean use) {
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));

    if (use != icon_view->label_fg_color_set) {
        icon_view->label_fg_color_set = !!use;

        if (icon_view->label_fg_color_set) {
            insert_icon_label_fg_color_attrs(icon_view);
        } else {
            PangoAttrList *attrs = NULL;
            g_object_get(icon_view->text_renderer,
                         "attributes", &attrs,
                         NULL);

            if (attrs != NULL) {
                PangoAttrList *removed_attrs = pango_attr_list_filter(attrs, remove_fg_color_attrs, NULL);
                g_object_set(icon_view->text_renderer,
                             "attributes", attrs,
                             NULL);
                pango_attr_list_unref(removed_attrs);
                pango_attr_list_unref(attrs);
            }
        }

        for (GList *l = icon_view->items; l != NULL; l = l->next) {
            ViewItem *item = l->data;
            xfdesktop_icon_view_invalidate_item_text(icon_view, item);
        }

        g_object_notify(G_OBJECT(icon_view), "icon-label-fg-color-set");
    }
}

static void
remove_label_bg_style_provider(XfdesktopIconView *icon_view) {
    if (icon_view->label_bg_color_provider != NULL) {
        gtk_style_context_remove_provider_for_screen(gdk_screen_get_default(),
                                                     GTK_STYLE_PROVIDER(icon_view->label_bg_color_provider));
        g_clear_object(&icon_view->label_bg_color_provider);
    }
}

static void
add_label_bg_style_provider(XfdesktopIconView *icon_view) {
    gchar alpha[G_ASCII_DTOSTR_BUF_SIZE];  // Avoid locale settings giving us a ','
    gchar *css_data = g_strdup_printf(LABEL_BG_COLOR_CSS_FMT,
                                      (int)round(icon_view->label_bg_color.red * G_MAXUINT8),
                                      (int)round(icon_view->label_bg_color.green * G_MAXUINT8),
                                      (int)round(icon_view->label_bg_color.blue * G_MAXUINT8),
                                      g_ascii_dtostr(alpha, sizeof(alpha), CLAMP(icon_view->label_bg_color.alpha, 0.0, 1.0)));
    DBG("adding CSS %s", css_data);

    icon_view->label_bg_color_provider = gtk_css_provider_new();
    if (gtk_css_provider_load_from_data(icon_view->label_bg_color_provider,
                                        css_data,
                                        -1,
                                        NULL))
    {
        gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                                  GTK_STYLE_PROVIDER(icon_view->label_bg_color_provider),
                                                  GTK_STYLE_PROVIDER_PRIORITY_USER);
    } else {
        g_clear_object(&icon_view->label_bg_color_provider);
    }

    g_free(css_data);
}

void
xfdesktop_icon_view_set_icon_label_bg_color(XfdesktopIconView *icon_view, GdkRGBA *color) {
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));
    g_return_if_fail(color != NULL);

    if (color->red != icon_view->label_bg_color.red
        || color->green != icon_view->label_bg_color.green
        || color->blue != icon_view->label_bg_color.blue
        || color->alpha != icon_view->label_bg_color.alpha)
    {
        icon_view->label_bg_color = *color;

        if (icon_view->label_bg_color_set) {
            remove_label_bg_style_provider(icon_view);
            add_label_bg_style_provider(icon_view);
        }

        for (GList *l = icon_view->items; l != NULL; l = l->next) {
            ViewItem *item = l->data;
            xfdesktop_icon_view_invalidate_item_text(icon_view, item);
        }

        g_object_notify(G_OBJECT(icon_view), "icon-label-bg-color");
    }
}

void
xfdesktop_icon_view_set_use_icon_label_bg_color(XfdesktopIconView *icon_view, gboolean use) {
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));

    if (use != icon_view->label_bg_color_set) {
        icon_view->label_bg_color_set = !!use;

        if (icon_view->label_bg_color_set) {
            add_label_bg_style_provider(icon_view);
        } else {
            remove_label_bg_style_provider(icon_view);
        }

        for (GList *l = icon_view->items; l != NULL; l = l->next) {
            ViewItem *item = l->data;
            xfdesktop_icon_view_invalidate_item_text(icon_view, item);
        }

        g_object_notify(G_OBJECT(icon_view), "icon-label-bg-color-set");
    }
}

gboolean
xfdesktop_icon_view_get_single_click(XfdesktopIconView *icon_view)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view), FALSE);

    return icon_view->single_click;
}

void
xfdesktop_icon_view_set_single_click(XfdesktopIconView *icon_view,
                                     gboolean single_click)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));

    if (single_click == icon_view->single_click) {
        return;
    }

    icon_view->single_click = single_click;

    g_object_set(icon_view->text_renderer,
                 "underline-when-prelit", icon_view->single_click && icon_view->single_click_underline_hover,
                 NULL);

    if(gtk_widget_get_realized(GTK_WIDGET(icon_view))) {
        for (GList *l = icon_view->selected_items; l != NULL; l = l->next) {
            xfdesktop_icon_view_invalidate_item(icon_view, l->data, TRUE);
        }
    }

    g_object_notify(G_OBJECT(icon_view), "single-click");
}

void
xfdesktop_icon_view_set_single_click_underline_hover(XfdesktopIconView *icon_view,
                                                     gboolean single_click_underline_hover)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));

    if (single_click_underline_hover != icon_view->single_click_underline_hover) {
        icon_view->single_click_underline_hover = single_click_underline_hover;

        g_object_set(icon_view->text_renderer,
                     "underline-when-prelit", icon_view->single_click && icon_view->single_click_underline_hover,
                     NULL);

        if (gtk_widget_get_realized(GTK_WIDGET(icon_view)) && icon_view->item_under_pointer != NULL) {
            ViewItem *item = icon_view->item_under_pointer;
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

    if (gravity == icon_view->gravity) {
        return;
    }

    icon_view->gravity = gravity;

    g_object_notify(G_OBJECT(icon_view), "gravity");
}

void
xfdesktop_icon_view_set_show_tooltips(XfdesktopIconView *icon_view,
                                      gboolean show_tooltips)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));

    if (show_tooltips == icon_view->show_tooltips) {
        return;
    }

    icon_view->show_tooltips = show_tooltips;
    g_object_notify(G_OBJECT(icon_view), "show-tooltips");
}

gint
xfdesktop_icon_view_get_tooltip_icon_size(XfdesktopIconView *icon_view)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view), DEFAULT_TOOLTIP_ICON_SIZE);

    if (icon_view->tooltip_icon_size_xfconf > 0) {
        return icon_view->tooltip_icon_size_xfconf;
    } else if (icon_view->tooltip_icon_size_style > 0) {
        return icon_view->tooltip_icon_size_style;
    } else {
        return DEFAULT_TOOLTIP_ICON_SIZE;
    }
}


// For config migration only
static void
find_slot_for_coords(XfdesktopIconView *icon_view, GridParams *grid_params, gint x, gint y, gint *row, gint *col) {
    // Algebraically rearranged from xfdesktop_icon_view_shift_to_slot_area()
    *row = (y - grid_params->ymargin) / (SLOT_SIZE + grid_params->yspacing);
    *col = (x - grid_params->xmargin) / (SLOT_SIZE + grid_params->xspacing);
    DBG("row=%d, col=%d", *row, *col);
}

// For config migration only
gboolean
xfdesktop_icon_view_grid_geometry_for_metrics(XfdesktopIconView *icon_view,
                                              GdkRectangle *total_workarea,
                                              GdkRectangle *monitor_workarea,
                                              gint *first_row,
                                              gint *first_col,
                                              gint *last_row,
                                              gint *last_col)
{
    xfdesktop_icon_view_style_updated(GTK_WIDGET(icon_view));

    GtkAllocation allocation = {
        .x = total_workarea->x,
        .y = total_workarea->y,
        .width = total_workarea->width,
        .height = total_workarea->height,
    };
    GridParams grid_params;
    if (!_xfdesktop_icon_view_build_grid_params(icon_view, &allocation, &grid_params)) {
        return FALSE;
    }

    DBG("SLOT_SIZE=%.02f", SLOT_SIZE);

    // If our monitor origin is all the way to the top or left, the margin will
    // come into play, so add that buffer so our coord will actually be in the
    // row/col.
    gint xleftoff = monitor_workarea->x == total_workarea->x ? grid_params.xmargin : 0;
    gint ytopoff = monitor_workarea->y == total_workarea->y ? grid_params.ymargin : 0;
    gint xorigin = monitor_workarea->x + xleftoff;
    gint yorigin = monitor_workarea->y + ytopoff;
    DBG("xorigin=%d, yorigin=%d", xorigin, yorigin);
    find_slot_for_coords(icon_view, &grid_params, xorigin, yorigin, first_row, first_col);

    // If our monitor is all the way to the bottom or left, the margin will
    // again come into play, so subtract that out.
    gint xrightoff = monitor_workarea->x + monitor_workarea->width == total_workarea->x + total_workarea->width
        ? grid_params.xmargin : 0;
    gint ybottomoff = monitor_workarea->y + monitor_workarea->height == total_workarea->y + total_workarea->height
        ? grid_params.ymargin : 0;
    // + 1 on these next two is because SLOT_SIZE is floating-point, and we
    // always want to round up to ensure we're actually inside the last slot.
    gint xlast = monitor_workarea->x + monitor_workarea->width - xrightoff - SLOT_SIZE + 1;
    gint ylast = monitor_workarea->y + monitor_workarea->height - ybottomoff - SLOT_SIZE + 1;
    DBG("xlast=%d, ylast=%d", xlast, ylast);
    find_slot_for_coords(icon_view, &grid_params, xlast, ylast, last_row, last_col);

    return *first_row >= 0
        && *first_row < grid_params.nrows
        && *first_col >= 0
        && *first_col < grid_params.ncols
        && *last_row >= 0
        && *last_row < grid_params.nrows
        && *last_col >= 0
        && *last_col < grid_params.ncols
        && *first_row <= *last_row
        && *first_col <= *last_col;
}
