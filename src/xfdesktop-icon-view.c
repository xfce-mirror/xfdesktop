/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2006 Brian Tarricone, <bjt23@cornell.edu>
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <glib-object.h>

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <libxfcegui4/libxfcegui4.h>

#include "xfdesktop-icon-view.h"

#define ICON_SIZE         32
#define CELL_SIZE         112
#define TEXT_WIDTH        100
#define CELL_PADDING      6
#define SPACING           8
#define SCREEN_MARGIN     8
#define CORNER_ROUNDNESS  4

#if defined(DEBUG) && DEBUG > 0
#define DUMP_GRID_LAYOUT(icon_view) \
{\
    gint my_i, my_maxi;\
    \
    g_printerr("\nDBG[%s:%d] %s\n", __FILE__, __LINE__, __FUNCTION__);\
    my_maxi = icon_view->priv->nrows * icon_view->priv->ncols;\
    for(my_i = 0; my_i < my_maxi; my_i++)\
        g_printerr("%c ", icon_view->priv->grid_layout[my_i] ? '1' : '0');\
    g_printerr("\n\n");\
}
#else
#define DUMP_GRID_LAYOUT(icon_view)
#endif

typedef enum
{
    XFDESKTOP_DIRECTION_UP = 0,
    XFDESKTOP_DIRECTION_DOWN,
    XFDESKTOP_DIRECTION_LEFT,
    XFDESKTOP_DIRECTION_RIGHT,
} XfdesktopDirection;

typedef enum
{
    XFDESKTOP_WORKAREA_OK = 0,
    XFDESKTOP_WORKAREA_FAILED,
    XFDESKTOP_WORKAREA_ABORTED,
} XfdesktopWorkareaStatus;

enum
{
    SIG_ICON_SELECTED = 0,
    SIG_ICON_ACTIVATED,
    SIG_N_SIGNALS,
};

struct _XfdesktopIconViewPrivate
{
    XfdesktopIconViewManager *manager;
    
    GtkWidget *parent_window;
    
    NetkScreen *netk_screen;
    PangoLayout *playout;
    
    GList *pending_icons;
    GList *icons;
    GList *selected_icons;
    
    gint xorigin;
    gint yorigin;
    gint width;
    gint height;
    
    guint16 nrows;
    guint16 ncols;
    XfdesktopIcon **grid_layout;
    
    guint grid_resize_timeout;
    guint icon_repaint_id;
    
    GtkSelectionMode sel_mode;
    gboolean allow_overlapping_drops;
    gboolean maybe_begin_drag;
    gboolean definitely_dragging;
    gint press_start_x;
    gint press_start_y;
    XfdesktopIcon *last_clicked_item;
    GtkTargetList *source_targets;
    
    GtkTargetList *foreign_source_targets;
    GdkDragAction foreign_source_actions;
    GdkModifierType foreign_source_mask;
    
    GtkTargetList *foreign_dest_targets;
    GdkDragAction foreign_dest_actions;
    
    GdkPixbuf *rounded_frame;
};

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
static void xfdesktop_icon_view_realize(GtkWidget *widget);
static void xfdesktop_icon_view_unrealize(GtkWidget *widget);
static gboolean xfdesktop_icon_view_expose(GtkWidget *widget,
                                           GdkEventExpose *evt,
                                           gpointer user_data);
static void xfdesktop_icon_view_drag_begin(GtkWidget *widget,
                                           GdkDragContext *contest);
static gboolean xfdesktop_icon_view_drag_motion(GtkWidget *widget,
                                                GdkDragContext *context,
                                                gint x,
                                                gint y,
                                                guint time);
static void xfdesktop_icon_view_drag_leave(GtkWidget *widget,
                                           GdkDragContext *context,
                                           guint time);
static gboolean xfdesktop_icon_view_drag_drop(GtkWidget *widget,
                                              GdkDragContext *context,
                                              gint x,
                                              gint y,
                                              guint time);
                                                      
static void xfdesktop_icon_view_finalize(GObject *obj);


static void xfdesktop_icon_view_clear_icon_extents(XfdesktopIconView *icon_view,
                                                   XfdesktopIcon *icon);
static void xfdesktop_icon_view_paint_icon(XfdesktopIconView *icon_view,
                                           XfdesktopIcon *icon);
static void xfdesktop_icon_view_paint_icons(XfdesktopIconView *icon_view,
                                            GdkRectangle *area);

static void xfdesktop_setup_grids(XfdesktopIconView *icon_view);
static gboolean xfdesktop_grid_get_next_free_position(XfdesktopIconView *icon_view,
                                                      guint16 *row,
                                                      guint16 *col);
static inline gboolean xfdesktop_grid_is_free_position(XfdesktopIconView *icon_view,
                                                       guint16 row,
                                                       guint16 col);
static inline void xfdesktop_grid_set_position_free(XfdesktopIconView *icon_view,
                                                    guint16 row,
                                                    guint16 col);
static inline void xfdesktop_grid_unset_position_free(XfdesktopIconView *icon_view,
                                                      XfdesktopIcon *icon);
static XfdesktopIcon *xfdesktop_find_icon_below(XfdesktopIconView *icon_view,
                                                XfdesktopIcon *icon);
static gint xfdesktop_check_icon_clicked(gconstpointer data,
                                         gconstpointer user_data);
static void xfdesktop_list_foreach_repaint(gpointer data,
                                           gpointer user_data);
static void xfdesktop_grid_find_nearest(XfdesktopIconView *icon_view,
                                        XfdesktopIcon *icon,
                                        XfdesktopDirection dir,
                                        gboolean allow_multiple);
static inline void xfdesktop_xy_to_rowcol(XfdesktopIconView *icon_view,
                                          gint x,
                                          gint y,
                                          guint16 *row,
                                          guint16 *col);
static gboolean xfdesktop_grid_resize_timeout(gpointer user_data);
static void xfdesktop_screen_size_changed_cb(GdkScreen *gscreen,
                                             gpointer user_data);
static GdkFilterReturn xfdesktop_rootwin_watch_workarea(GdkXEvent *gxevent,
                                                        GdkEvent *event,
                                                        gpointer user_data);
static gboolean xfdesktop_icon_view_paint_icon_idled(gpointer user_data);
static gboolean xfdesktop_get_workarea_single(XfdesktopIconView *icon_view,
                                              guint ws_num,
                                              gint *xorigin,
                                              gint *yorigin,
                                              gint *width,
                                              gint *height);
static void xfdesktop_grid_do_resize(XfdesktopIconView *icon_view);

enum
{
    TARGET_XFDESKTOP_ICON = 9999,
};

static const GtkTargetEntry icon_view_targets[] = {
    { "XFDESKTOP_ICON", GTK_TARGET_SAME_APP, TARGET_XFDESKTOP_ICON }
};
static const gint icon_view_n_targets = 1;

static guint __signals[SIG_N_SIGNALS] = { 0, };

G_DEFINE_TYPE(XfdesktopIconView, xfdesktop_icon_view, GTK_TYPE_WIDGET)


static void
xfdesktop_icon_view_class_init(XfdesktopIconViewClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;
    GtkWidgetClass *widget_class = (GtkWidgetClass *)klass;
    
    gobject_class->finalize = xfdesktop_icon_view_finalize;
    
    widget_class->realize = xfdesktop_icon_view_realize;
    widget_class->unrealize = xfdesktop_icon_view_unrealize;
    widget_class->drag_begin = xfdesktop_icon_view_drag_begin;
    widget_class->drag_motion = xfdesktop_icon_view_drag_motion;
    widget_class->drag_leave = xfdesktop_icon_view_drag_leave;
    widget_class->drag_drop = xfdesktop_icon_view_drag_drop;
    
    __signals[SIG_ICON_SELECTED] = g_signal_new("icon-selected",
                                                XFDESKTOP_TYPE_ICON_VIEW,
                                                G_SIGNAL_RUN_LAST,
                                                G_STRUCT_OFFSET(XfdesktopIconViewClass,
                                                                icon_selected),
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
}

static void
xfdesktop_icon_view_init(XfdesktopIconView *icon_view)
{
    icon_view->priv = g_new0(XfdesktopIconViewPrivate, 1);
    
    icon_view->priv->source_targets = gtk_target_list_new(icon_view_targets,
                                                          icon_view_n_targets);
    gtk_drag_dest_set(GTK_WIDGET(icon_view), 0, NULL, 0, GDK_ACTION_MOVE);
    
    GTK_WIDGET_SET_FLAGS(GTK_WIDGET(icon_view), GTK_NO_WINDOW);
}

static void
xfdesktop_icon_view_finalize(GObject *obj)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(obj);
    
    if(icon_view->priv->manager) {
        xfdesktop_icon_view_manager_fini(icon_view->priv->manager);
        g_object_unref(G_OBJECT(icon_view->priv->manager));
    }
    
    gtk_target_list_unref(icon_view->priv->source_targets);
    
    g_list_free(icon_view->priv->icons);
    g_list_free(icon_view->priv->pending_icons);
    
    g_free(icon_view->priv);
    
    G_OBJECT_CLASS(xfdesktop_icon_view_parent_class)->finalize(obj);
}

static gboolean
xfdesktop_icon_view_button_press(GtkWidget *widget,
                                 GdkEventButton *evt,
                                 gpointer user_data)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(user_data);
    XfdesktopIcon *icon;
    
    TRACE("entering, type is %s", evt->type == GDK_BUTTON_PRESS ? "GDK_BUTTON_PRESS" : (evt->type == GDK_2BUTTON_PRESS ? "GDK_2BUTTON_PRESS" : "i dunno"));
    
    if(evt->type == GDK_BUTTON_PRESS) {
        GList *icon_l = g_list_find_custom(icon_view->priv->icons, evt,
                                           (GCompareFunc)xfdesktop_check_icon_clicked);
        if(icon_l && (icon = icon_l->data)) {
            /* if selection mode is not multiple, or if it is, but shift/ctrl
            * is not held down, unselect the others */
            if(icon_view->priv->sel_mode != GTK_SELECTION_MULTIPLE
               || !(evt->state & (GDK_CONTROL_MASK|GDK_SHIFT_MASK)))
            {
                GList *repaint_icons = icon_view->priv->selected_icons;
                icon_view->priv->selected_icons = NULL;
                g_list_foreach(repaint_icons, xfdesktop_list_foreach_repaint,
                               icon_view);
                g_list_free(repaint_icons);
            }
            
            icon_view->priv->selected_icons = g_list_prepend(icon_view->priv->selected_icons,
                                                             icon);
            xfdesktop_icon_view_clear_icon_extents(icon_view, icon);
            xfdesktop_icon_view_paint_icon(icon_view, icon);
            icon_view->priv->last_clicked_item = icon;
            
            g_signal_emit(G_OBJECT(icon_view), __signals[SIG_ICON_SELECTED], 0);
            
            if(evt->button == 1) {
                /* we might be the start of a drag */
                DBG("setting stuff");
                icon_view->priv->maybe_begin_drag = TRUE;
                icon_view->priv->definitely_dragging = FALSE;
                icon_view->priv->press_start_x = evt->x;
                icon_view->priv->press_start_y = evt->y;
                
                return TRUE;
            } else if(evt->button == 3) {
                /* if we're a right click, emit signal on the icon */
                xfdesktop_icon_menu_popup(icon);
                return TRUE;
            }
        } else {
            /* unselect previously selected icons if we didn't click one */
            if(icon_view->priv->sel_mode != GTK_SELECTION_MULTIPLE
               || !(evt->state & (GDK_CONTROL_MASK|GDK_SHIFT_MASK))) {
                GList *repaint_icons = icon_view->priv->selected_icons;
                icon_view->priv->selected_icons = NULL;
                g_list_foreach(repaint_icons, xfdesktop_list_foreach_repaint,
                               icon_view);
                g_list_free(repaint_icons);
            }
        }
    } else if(evt->type == GDK_2BUTTON_PRESS) {
        GList *icon_l = g_list_find_custom(icon_view->priv->icons, evt,
                                           (GCompareFunc)xfdesktop_check_icon_clicked);
        if(icon_l && (icon = icon_l->data)) {
            xfdesktop_icon_activated(icon);
            g_signal_emit(G_OBJECT(icon_view), __signals[SIG_ICON_ACTIVATED], 0);
        }
    }
    
    return FALSE;
}

static gboolean
xfdesktop_icon_view_button_release(GtkWidget *widget,
                                   GdkEventButton *evt,
                                   gpointer user_data)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(user_data);
    
    TRACE("entering btn=%d", evt->button);
    
    if(evt->button == 1) {
        DBG("unsetting stuff");
        icon_view->priv->definitely_dragging = FALSE;
        icon_view->priv->maybe_begin_drag = FALSE;
    }
    
    return FALSE;
}

static gboolean
xfdesktop_icon_view_key_press(GtkWidget *widget,
                              GdkEventKey *evt,
                              gpointer user_data)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(user_data);
    XfdesktopIcon *icon = NULL;
    gboolean allow_multiple = (evt->state & GDK_CONTROL_MASK)
                              || (evt->state & GDK_SHIFT_MASK);
    
    if(icon_view->priv->selected_icons)
        icon = icon_view->priv->selected_icons->data;
    
    switch(evt->keyval) {
        case GDK_Up:
        case GDK_KP_Up:
            xfdesktop_grid_find_nearest(icon_view, icon,
                                        XFDESKTOP_DIRECTION_UP,
                                        allow_multiple);
            break;
        
        case GDK_Down:
        case GDK_KP_Down:
            xfdesktop_grid_find_nearest(icon_view, icon,
                                        XFDESKTOP_DIRECTION_DOWN,
                                        allow_multiple);
            break;
        
        case GDK_Left:
        case GDK_KP_Left:
            xfdesktop_grid_find_nearest(icon_view, icon,
                                        XFDESKTOP_DIRECTION_LEFT,
                                        allow_multiple);
            break;
        
        case GDK_Right:
        case GDK_KP_Right:
            xfdesktop_grid_find_nearest(icon_view, icon,
                                        XFDESKTOP_DIRECTION_RIGHT,
                                        allow_multiple);
            break;
        
        case GDK_Return:
        case GDK_KP_Enter:
            if(icon) {
                g_list_foreach(icon_view->priv->selected_icons,
                               (GFunc)xfdesktop_icon_activated, NULL);
                /* FIXME: really unselect? */
                xfdesktop_icon_view_unselect_all(icon_view);
            }
            break;
        
        default:
            return FALSE;
    }
    
    return TRUE;
}

static gboolean
xfdesktop_icon_view_focus_in(GtkWidget *widget,
                             GdkEventFocus *evt,
                             gpointer user_data)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(user_data);
    
    GTK_WIDGET_SET_FLAGS(GTK_WIDGET(icon_view), GTK_HAS_FOCUS);
    DBG("GOT FOCUS");
    
    g_list_foreach(icon_view->priv->selected_icons,
                   xfdesktop_list_foreach_repaint, icon_view);
    
    return FALSE;
}

static gboolean
xfdesktop_icon_view_focus_out(GtkWidget *widget,
                              GdkEventFocus *evt,
                              gpointer user_data)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(user_data);
    
    GTK_WIDGET_UNSET_FLAGS(GTK_WIDGET(icon_view), GTK_HAS_FOCUS);
    DBG("LOST FOCUS");
    
    g_list_foreach(icon_view->priv->selected_icons,
                   xfdesktop_list_foreach_repaint, icon_view);
    
    return FALSE;
}

static gboolean
xfdesktop_icon_view_maybe_begin_drag(XfdesktopIconView *icon_view,
                                     GdkEventMotion *evt)
{
    GdkDragContext *context;
    GdkDragAction actions;
    
    /* sanity check */
    g_return_val_if_fail(icon_view->priv->last_clicked_item, FALSE);
    
    if(!gtk_drag_check_threshold(GTK_WIDGET(icon_view),
                                 icon_view->priv->press_start_x,
                                 icon_view->priv->press_start_y,
                                 evt->x, evt->y))
    {
        return FALSE;
    }
    
    actions = GDK_ACTION_MOVE | (icon_view->priv->foreign_source_targets ?
                                 icon_view->priv->foreign_source_actions : 0);
    
    context = gtk_drag_begin(GTK_WIDGET(icon_view),
                             icon_view->priv->source_targets,
                             actions,
                             1, (GdkEvent *)evt);
    
    DBG("DRAG BEGIN!");
    
    return TRUE;
}


static gboolean
xfdesktop_icon_view_motion_notify(GtkWidget *widget,
                                  GdkEventMotion *evt,
                                  gpointer user_data)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(user_data);
    gboolean ret = FALSE;
    
    if(icon_view->priv->maybe_begin_drag
       && !icon_view->priv->definitely_dragging)
    {
        icon_view->priv->definitely_dragging = xfdesktop_icon_view_maybe_begin_drag(icon_view,
                                                                                    evt);
        if(icon_view->priv->definitely_dragging)
            ret = TRUE;
    }
    
    return ret;
}

static void
xfdesktop_icon_view_drag_begin(GtkWidget *widget,
                               GdkDragContext *context)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);
    XfdesktopIcon *icon;
    GdkRectangle extents;
    
    icon = icon_view->priv->last_clicked_item;
    g_return_if_fail(icon);
    
    if(xfdesktop_icon_get_extents(icon, &extents)) {
        GdkPixbuf *pix;
        gint x, y;
        
        x = icon_view->priv->press_start_x - extents.x + 1;
        y = icon_view->priv->press_start_y - extents.y + 1;
    
        pix = xfdesktop_icon_peek_pixbuf(icon, ICON_SIZE);
        if(pix)
            gtk_drag_set_icon_pixbuf(context, pix, x, y);
    }
}

static inline void
xfdesktop_xy_to_rowcol(XfdesktopIconView *icon_view,
                       gint x,
                       gint y,
                       guint16 *row,
                       guint16 *col)
{
    g_return_if_fail(row && col);
    
    *row = (y - icon_view->priv->xorigin - CELL_PADDING) / CELL_SIZE;
    *col = (x - icon_view->priv->yorigin - CELL_PADDING) / CELL_SIZE;
}

static gboolean
xfdesktop_icon_view_drag_motion(GtkWidget *widget,
                               GdkDragContext *context,
                               gint x,
                               gint y,
                               guint time)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);
    GdkAtom target = GDK_NONE;
    guint16 row, col;
    GdkRectangle *cell_highlight;
    
    /*TRACE("entering: (%d,%d)", x, y);*/
    
    target = gtk_drag_dest_find_target(widget, context,
                                       icon_view->priv->source_targets);
    if(target == GDK_NONE && icon_view->priv->foreign_source_targets) {
        target = gtk_drag_dest_find_target(widget, context,
                                           icon_view->priv->foreign_source_targets);
        if(target == GDK_NONE)
            return FALSE;
    }
    
    gdk_drag_status(context, GDK_ACTION_MOVE, time);
    
    cell_highlight = g_object_get_data(G_OBJECT(context),
                                       "xfce-desktop-cell-highlight");
    
    xfdesktop_xy_to_rowcol(icon_view, x, y, &row, &col);
    if(row < icon_view->priv->nrows && col < icon_view->priv->ncols
       && (icon_view->priv->allow_overlapping_drops
           || xfdesktop_grid_is_free_position(icon_view, row, col)))
    {
        gint newx, newy;
        
        newx = icon_view->priv->xorigin + col * CELL_SIZE + CELL_PADDING;
        newy = icon_view->priv->yorigin + row * CELL_SIZE + CELL_PADDING;
        
        if(cell_highlight) {
            DBG("have old cell higlight: (%d,%d)", cell_highlight->x,
                cell_highlight->y);
            if(cell_highlight->x != newx || cell_highlight->y != newy) {
                gdk_window_clear_area(widget->window,
                                      cell_highlight->x,
                                      cell_highlight->y,
                                      cell_highlight->width + 1,
                                      cell_highlight->height + 1);
            }
        } else {
            cell_highlight = g_new0(GdkRectangle, 1);
            g_object_set_data_full(G_OBJECT(context),
                                   "xfce-desktop-cell-highlight",
                                   cell_highlight, (GDestroyNotify)g_free);
        }
        
        cell_highlight->x = newx;
        cell_highlight->y = newy;
        cell_highlight->width = cell_highlight->height = CELL_SIZE;
        
        DBG("painting highlight: (%d,%d)", newx, newy);
        
        gdk_draw_rectangle(GDK_DRAWABLE(widget->window),
                           widget->style->bg_gc[GTK_STATE_SELECTED], FALSE,
                           newx, newy, CELL_SIZE, CELL_SIZE);
        
        return TRUE;
    } else {
        if(cell_highlight) {
            gdk_window_clear_area(widget->window,
                                  cell_highlight->x,
                                  cell_highlight->y,
                                  cell_highlight->width + 1,
                                  cell_highlight->height + 1);
        }
        return FALSE;
    }
}

static void
xfdesktop_icon_view_drag_leave(GtkWidget *widget,
                               GdkDragContext *context,
                               guint time)
{
    GdkRectangle *cell_highlight = g_object_get_data(G_OBJECT(context),
                                                     "xfce-desktop-cell-highlight");
    if(cell_highlight) {
        gdk_window_clear_area(widget->window,
                              cell_highlight->x,
                              cell_highlight->y,
                              cell_highlight->width + 1,
                              cell_highlight->height + 1);
    }
}


static gboolean
xfdesktop_icon_view_drag_drop(GtkWidget *widget,
                              GdkDragContext *context,
                              gint x,
                              gint y,
                              guint time)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);
    GdkAtom target = GDK_NONE;
    XfdesktopIcon *icon, *icon_below;
    guint16 old_row, old_col, row, col;
    gint cell_x, cell_y;
    GdkRectangle extents;
    
    TRACE("entering: (%d,%d)", x, y);
    
    target = gtk_drag_dest_find_target(widget, context,
                                       icon_view->priv->source_targets);
    if(target == GDK_NONE) {
        if(icon_view->priv->foreign_dest_targets) {
            target = gtk_drag_dest_find_target(widget, context,
                                               icon_view->priv->foreign_dest_targets);
        }
        if(target == GDK_NONE)
            return FALSE;
    }
    
    xfdesktop_xy_to_rowcol(icon_view, x, y, &row, &col);
    if(row >= icon_view->priv->nrows || col >= icon_view->priv->ncols
       || (!icon_view->priv->allow_overlapping_drops
           && !xfdesktop_grid_is_free_position(icon_view, row, col)))
    {
        gtk_drag_finish(context, FALSE, FALSE, time);
        return FALSE;
    }
    
    /* clear highlight box */
    cell_x = icon_view->priv->xorigin + col * CELL_SIZE + CELL_PADDING;
    cell_y = icon_view->priv->yorigin + row * CELL_SIZE + CELL_PADDING;
    gdk_window_clear_area(widget->window, cell_x, cell_y,
                          CELL_SIZE + 1, CELL_SIZE + 1);
    
    if(target == gdk_atom_intern("XFDESKTOP_ICON", FALSE)) {
        icon = icon_view->priv->last_clicked_item;
        g_return_val_if_fail(icon, FALSE);
        
        icon_below = xfdesktop_find_icon_below(icon_view, icon);
        if(xfdesktop_icon_get_position(icon, &old_row, &old_col))
            xfdesktop_grid_set_position_free(icon_view, old_row, old_col);
        xfdesktop_icon_set_position(icon, row, col);
        xfdesktop_grid_unset_position_free(icon_view, icon);
    
        /* clear out old position */
        if(xfdesktop_icon_get_extents(icon, &extents)) {
            gdk_window_clear_area(widget->window, extents.x, extents.y,
                                  extents.width, extents.height);
            extents.width = extents.height = 0;
            xfdesktop_icon_set_extents(icon, &extents);
        }
        
        xfdesktop_icon_view_paint_icon(icon_view, icon);
        if(icon_below)
            xfdesktop_icon_view_paint_icon(icon_view, icon_below);
        
        DBG("drag succeeded");
        
        gtk_drag_finish(context, TRUE, FALSE, time);
    } else {
        /* other handler will take care of stuff */
    }
    
    return TRUE;
}

static void
xfdesktop_icon_view_realize(GtkWidget *widget)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);
    PangoContext *pctx;
    GdkScreen *gscreen;
    GdkWindow *groot;
    GList *l;
    
    icon_view->priv->parent_window = gtk_widget_get_toplevel(widget);
    g_return_if_fail(icon_view->priv->parent_window);
    widget->window = icon_view->priv->parent_window->window;
    GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);
    
    /* there's no reason to start up the manager before we're realized,
     * but we do NOT shut it down if we unrealize, since there may not be
     * a reason to do so.  shutdown occurs in finalize. */
    xfdesktop_icon_view_manager_init(icon_view->priv->manager, icon_view);
    
    gtk_window_set_accept_focus(GTK_WINDOW(icon_view->priv->parent_window),
                                TRUE);
    
    xfdesktop_setup_grids(icon_view);
    
    pctx = gtk_widget_get_pango_context(GTK_WIDGET(icon_view));
    icon_view->priv->playout = pango_layout_new(pctx);
    
    /* unfortunately GTK_NO_WINDOW widgets don't receive events, with the
     * exception of expose events.  however, even expose events don't seem
     * to work for some reason. */
    gtk_widget_add_events(icon_view->priv->parent_window,
                          GDK_POINTER_MOTION_MASK | GDK_KEY_PRESS_MASK
                          | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
                          | GDK_FOCUS_CHANGE_MASK | GDK_EXPOSURE_MASK);
    g_signal_connect(G_OBJECT(icon_view->priv->parent_window),
                     "motion-notify-event",
                     G_CALLBACK(xfdesktop_icon_view_motion_notify), icon_view);
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
    g_signal_connect_after(G_OBJECT(icon_view->priv->parent_window),
                           "expose-event",
                           G_CALLBACK(xfdesktop_icon_view_expose), icon_view);
    
    /* watch for _NET_WORKAREA changes */
    gscreen = gtk_widget_get_screen(widget);
    groot = gdk_screen_get_root_window(gscreen);
    gdk_window_set_events(groot, gdk_window_get_events(groot)
                                 | GDK_PROPERTY_CHANGE_MASK);
    gdk_window_add_filter(groot, xfdesktop_rootwin_watch_workarea, icon_view);
    
    g_signal_connect(G_OBJECT(gscreen), "size-changed",
                     G_CALLBACK(xfdesktop_screen_size_changed_cb), icon_view);
    
    for(l = icon_view->priv->pending_icons; l; l = l->next)
        xfdesktop_icon_view_add_item(icon_view, XFDESKTOP_ICON(l->data));
    g_list_free(icon_view->priv->pending_icons);
    icon_view->priv->pending_icons = NULL;
}

static void
xfdesktop_icon_view_unrealize(GtkWidget *widget)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(widget);
    GdkScreen *gscreen;
    GdkWindow *groot;
    
    gtk_window_set_accept_focus(GTK_WINDOW(icon_view->priv->parent_window), FALSE);
    
    gscreen = gtk_widget_get_screen(widget);
    groot = gdk_screen_get_root_window(gscreen);
    gdk_window_remove_filter(groot, xfdesktop_rootwin_watch_workarea, icon_view);
    
    g_signal_handlers_disconnect_by_func(G_OBJECT(icon_view->priv->parent_window),
                     G_CALLBACK(xfdesktop_icon_view_motion_notify), icon_view);
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
    g_signal_handlers_disconnect_by_func(G_OBJECT(icon_view->priv->parent_window),
                     G_CALLBACK(xfdesktop_icon_view_expose), icon_view);
    
    if(icon_view->priv->grid_resize_timeout) {
        g_source_remove(icon_view->priv->grid_resize_timeout);
        icon_view->priv->grid_resize_timeout = 0;
    }
    
    if(icon_view->priv->icon_repaint_id) {
        g_source_remove(icon_view->priv->icon_repaint_id);
        icon_view->priv->icon_repaint_id = 0;
    }
    
    g_signal_handlers_disconnect_by_func(G_OBJECT(gscreen),
                                         G_CALLBACK(xfdesktop_screen_size_changed_cb),
                                         icon_view);
    
    /* FIXME: really clear these? */
    g_list_free(icon_view->priv->selected_icons);
    icon_view->priv->selected_icons = NULL;
    
    g_free(icon_view->priv->grid_layout);
    icon_view->priv->grid_layout = NULL;
    
    g_object_unref(G_OBJECT(icon_view->priv->playout));
    icon_view->priv->playout = NULL;
    
    if(icon_view->priv->rounded_frame) {
        g_object_unref(G_OBJECT(icon_view->priv->rounded_frame));
        icon_view->priv->rounded_frame = NULL;
    }
    
    widget->window = NULL;
    GTK_WIDGET_UNSET_FLAGS(widget, GTK_REALIZED);
}

static gboolean
xfdesktop_icon_view_expose(GtkWidget *widget,
                           GdkEventExpose *evt,
                           gpointer user_data)
{
    if(evt->count != 0)
        return FALSE;
    
    xfdesktop_icon_view_paint_icons(XFDESKTOP_ICON_VIEW(user_data), &evt->area);
    
    return FALSE;
}

static void
xfdesktop_screen_size_changed_cb(GdkScreen *gscreen,
                                 gpointer user_data)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(user_data);
    
   /* this is kinda icky.  we want to use _NET_WORKAREA to reset the size of
     * the grid, but we can never be sure it'll actually change.  so let's
     * give it 7 seconds, and then fix it manually */
    if(icon_view->priv->grid_resize_timeout)
        g_source_remove(icon_view->priv->grid_resize_timeout);
    icon_view->priv->grid_resize_timeout = g_timeout_add(7000,
                                                         xfdesktop_grid_resize_timeout,
                                                         icon_view);   
}

static void
xfdesktop_check_icon_needs_repaint(gpointer data,
                                   gpointer user_data)
{
    XfdesktopIcon *icon = XFDESKTOP_ICON(data);
    GdkRectangle *area = user_data, extents, dummy;
    
    if(!xfdesktop_icon_get_extents(icon, &extents)
       || gdk_rectangle_intersect(area, &extents, &dummy))
    {
        XfdesktopIconView *icon_view = g_object_get_data(G_OBJECT(icon),
                                                         "--xfdesktop-icon-view");
        g_return_if_fail(icon_view);
        if(g_list_find(icon_view->priv->selected_icons, icon)) {
            /* save it for last to avoid painting another icon over top of
             * part of this one if it has an overly-large label */
            if(icon_view->priv->icon_repaint_id)
                g_source_remove(icon_view->priv->icon_repaint_id);
            icon_view->priv->icon_repaint_id = g_idle_add(xfdesktop_icon_view_paint_icon_idled,
                                                          icon);
        } else
            xfdesktop_icon_view_paint_icon(icon_view, icon);
    }
}

static void
xfdesktop_icon_view_paint_icons(XfdesktopIconView *icon_view,
                                GdkRectangle *area)
{
    if(icon_view->priv->icons) {    
        g_list_foreach(icon_view->priv->icons,
                       xfdesktop_check_icon_needs_repaint,
                       area);
    }
}


static void
xfdesktop_setup_grids(XfdesktopIconView *icon_view)
{
    gint xorigin, yorigin, width, height, tmp;
    
    if(xfdesktop_get_workarea_single(icon_view, 0,
                                     &xorigin, &yorigin,
                                     &width, &height))
    {
            
        icon_view->priv->xorigin = xorigin;
        icon_view->priv->yorigin = yorigin;
        icon_view->priv->width = width;
        icon_view->priv->height = height;
        
        icon_view->priv->nrows = (height - SCREEN_MARGIN * 2) / CELL_SIZE;
        icon_view->priv->ncols = (width - SCREEN_MARGIN * 2) / CELL_SIZE;
        
        tmp = icon_view->priv->nrows * icon_view->priv->ncols
              * sizeof(XfdesktopIcon *);
        
        if(G_UNLIKELY(icon_view->priv->grid_layout)) {
            icon_view->priv->grid_layout = g_realloc(
                icon_view->priv->grid_layout,
                tmp
            );
        } else
            icon_view->priv->grid_layout = g_malloc0(tmp);
        
        DBG("created grid_layout with %d positions", tmp);
        DUMP_GRID_LAYOUT(icon_view);
    } else {
        GdkScreen *gscreen = gtk_widget_get_screen(GTK_WIDGET(icon_view));
        gint w = gdk_screen_get_width(gscreen);
        gint h = gdk_screen_get_height(gscreen);
        
        icon_view->priv->xorigin = 0;
        icon_view->priv->yorigin = 0;
        icon_view->priv->width = w;
        icon_view->priv->height = h;
        
        icon_view->priv->nrows = (h - SCREEN_MARGIN * 2) / CELL_SIZE;
        icon_view->priv->ncols = (w - SCREEN_MARGIN * 2) / CELL_SIZE;
            
        tmp = icon_view->priv->nrows * icon_view->priv->ncols
              * sizeof(XfdesktopIcon *);
        
        if(G_UNLIKELY(icon_view->priv->grid_layout)) {
            icon_view->priv->grid_layout = g_realloc(
                icon_view->priv->grid_layout,
                tmp
            );
        } else
            icon_view->priv->grid_layout = g_malloc0(tmp);
        
        DBG("created grid_layout with %d positions", tmp);
        DUMP_GRID_LAYOUT(icon_view);
    }
}


static GdkFilterReturn
xfdesktop_rootwin_watch_workarea(GdkXEvent *gxevent,
                                 GdkEvent *event,
                                 gpointer user_data)
{
    XfdesktopIconView *icon_view = user_data;
    XPropertyEvent *xevt = (XPropertyEvent *)gxevent;
    
    if(xevt->type == PropertyNotify
       && XInternAtom(GDK_DISPLAY(), "_NET_WORKAREA", False) == xevt->atom)
    {
        DBG("got _NET_WORKAREA change on rootwin!");
        if(icon_view->priv->grid_resize_timeout) {
            g_source_remove(icon_view->priv->grid_resize_timeout);
            icon_view->priv->grid_resize_timeout = 0;
        }
        xfdesktop_grid_do_resize(icon_view);
    }
    
    return GDK_FILTER_CONTINUE;
}



static gint
xfdesktop_find_icon_below_from_hash(gconstpointer data,
                                    gconstpointer user_data)
{
    XfdesktopIcon *icon_maybe_below = XFDESKTOP_ICON(data);
    XfdesktopIcon *icon = XFDESKTOP_ICON(user_data);
    guint16 row, row1, dummy;
    
    if(!xfdesktop_icon_get_position(icon, &row, &dummy)
       || !xfdesktop_icon_get_position(icon_maybe_below, &row1, &dummy)
       || row1 != row + 1)
    {
        return 1;
    } else
        return 0;
}

static XfdesktopIcon *
xfdesktop_find_icon_below(XfdesktopIconView *icon_view,
                          XfdesktopIcon *icon)
{
    GList *icon_below_l = NULL;
    guint16 row, col;
    
    if(!xfdesktop_icon_get_position(icon, &row, &col)
       || row == icon_view->priv->nrows - 1)
    {
        return NULL;
    }
    
    icon_below_l = g_list_find_custom(icon_view->priv->icons, icon,
                                      xfdesktop_find_icon_below_from_hash);
    
    return icon_below_l ? icon_below_l->data : NULL;
}




static void
xfdesktop_icon_view_clear_icon_extents(XfdesktopIconView *icon_view,
                                       XfdesktopIcon *icon)
{
    GdkRectangle extents;
    
    g_return_if_fail(icon);
    
    if(xfdesktop_icon_get_extents(icon, &extents)) {
        gdk_window_clear_area(GTK_WIDGET(icon_view)->window,
                              extents.x, extents.y,
                              extents.width, extents.height);
        
        /* check and make sure we didn't used to be too large for the cell.
         * if so, repaint the one below it. */
        if(extents.height + 3 * CELL_PADDING > CELL_SIZE) {
            XfdesktopIcon *icon_below = xfdesktop_find_icon_below(icon_view,
                                                                  icon);
            if(icon_below)
                xfdesktop_icon_view_paint_icon(icon_view, icon_below);
        }
    }
}


/* Copied from Nautilus, Copyright (C) 2000 Eazel, Inc. */
static void
xfdesktop_clear_rounded_corners(GdkPixbuf *pix,
                                GdkPixbuf *corners_pix)
{
    gint dest_width, dest_height, src_width, src_height;
    
    g_return_if_fail(pix && corners_pix);
    
    dest_width = gdk_pixbuf_get_width(pix);
    dest_height = gdk_pixbuf_get_height(pix);
    
    src_width = gdk_pixbuf_get_width(corners_pix);
    src_height = gdk_pixbuf_get_height(corners_pix);
    
    /* draw top left corner */
    gdk_pixbuf_copy_area(corners_pix,
                         0, 0,
                         CORNER_ROUNDNESS, CORNER_ROUNDNESS,
                         pix,
                         0, 0);
    
    /* draw top right corner */
    gdk_pixbuf_copy_area(corners_pix,
                         src_width - CORNER_ROUNDNESS, 0,
                         CORNER_ROUNDNESS, CORNER_ROUNDNESS,
                         pix,
                         dest_width - CORNER_ROUNDNESS, 0);

    /* draw bottom left corner */
    gdk_pixbuf_copy_area(corners_pix,
                         0, src_height - CORNER_ROUNDNESS,
                         CORNER_ROUNDNESS, CORNER_ROUNDNESS,
                         pix,
                         0, dest_height - CORNER_ROUNDNESS);
    
    /* draw bottom right corner */
    gdk_pixbuf_copy_area(corners_pix,
                         src_width - CORNER_ROUNDNESS,
                         src_height - CORNER_ROUNDNESS,
                         CORNER_ROUNDNESS, CORNER_ROUNDNESS,
                         pix,
                         dest_width - CORNER_ROUNDNESS,
                         dest_height - CORNER_ROUNDNESS);
}

#define EEL_RGBA_COLOR_GET_R(color) (((color) >> 16) & 0xff)
#define EEL_RGBA_COLOR_GET_G(color) (((color) >> 8) & 0xff)
#define EEL_RGBA_COLOR_GET_B(color) (((color) >> 0) & 0xff)
#define EEL_RGBA_COLOR_GET_A(color) (((color) >> 24) & 0xff)
#define EEL_RGBA_COLOR_PACK(r, g, b, a)         \
( (((guint32)a) << 24) |                        \
  (((guint32)r) << 16) |                        \
  (((guint32)g) <<  8) |                        \
  (((guint32)b) <<  0) )
/* Copied from Nautilus, Copyright (C) 2000 Eazel, Inc. */
/* Multiplies each pixel in a pixbuf by the specified color */
static void
xfdesktop_multiply_pixbuf_rgba(GdkPixbuf *pixbuf,
                               guint rgba)
{
    guchar *pixels;
    int r, g, b, a;
    int width, height, rowstride;
    gboolean has_alpha;
    int x, y;
    guchar *p;

    g_return_if_fail (gdk_pixbuf_get_colorspace (pixbuf) == GDK_COLORSPACE_RGB);
    g_return_if_fail (gdk_pixbuf_get_n_channels (pixbuf) == 3
              || gdk_pixbuf_get_n_channels (pixbuf) == 4);

    r = EEL_RGBA_COLOR_GET_R (rgba);
    g = EEL_RGBA_COLOR_GET_G (rgba);
    b = EEL_RGBA_COLOR_GET_B (rgba);
    a = EEL_RGBA_COLOR_GET_A (rgba);

    width = gdk_pixbuf_get_width (pixbuf);
    height = gdk_pixbuf_get_height (pixbuf);
    rowstride = gdk_pixbuf_get_rowstride (pixbuf);
    has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);

    pixels = gdk_pixbuf_get_pixels (pixbuf);

    for (y = 0; y < height; y++) {
        p = pixels;

        for (x = 0; x < width; x++) {
            p[0] = p[0] * r / 255;
            p[1] = p[1] * g / 255;
            p[2] = p[2] * b / 255;

            if (has_alpha) {
                p[3] = p[3] * a / 255;
                p += 4;
            } else
                p += 3;
        }

        pixels += rowstride;
    }
}




static void
xfdesktop_paint_rounded_box(XfdesktopIconView *icon_view,
                            GtkStateType state,
                            GdkRectangle *area)
{
    GdkPixbuf *box_pix;
    GtkStyle *style = GTK_WIDGET(icon_view)->style;
    
    box_pix = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8,
                             area->width + CORNER_ROUNDNESS * 2,
                             area->height + CORNER_ROUNDNESS * 2);
    gdk_pixbuf_fill(box_pix, 0xffffffff);
    
    if(!icon_view->priv->rounded_frame)
        icon_view->priv->rounded_frame = gdk_pixbuf_new_from_file(DATADIR \
                                                                  "/pixmaps/xfce4/xfdesktop/text-selection-frame.png",
                                                                  NULL);
    
    xfdesktop_clear_rounded_corners(box_pix, icon_view->priv->rounded_frame);
    xfdesktop_multiply_pixbuf_rgba(box_pix,
                                   EEL_RGBA_COLOR_PACK(style->base[state].red >> 8, 
                                                       style->base[state].green >> 8, 
                                                       style->base[state].blue >> 8,
                                                       0xff));
    
    gdk_draw_pixbuf(GDK_DRAWABLE(GTK_WIDGET(icon_view)->window), NULL,
                    box_pix, 0, 0,
                    area->x - CORNER_ROUNDNESS, area->y - CORNER_ROUNDNESS,
                    area->width + CORNER_ROUNDNESS * 2,
                    area->height + CORNER_ROUNDNESS * 2,
                    GDK_RGB_DITHER_NORMAL, 0, 0);
    
    g_object_unref(G_OBJECT(box_pix));
}



static void
xfdesktop_icon_view_paint_icon(XfdesktopIconView *icon_view,
                               XfdesktopIcon *icon)
{
    GtkWidget *widget = GTK_WIDGET(icon_view);
    GdkPixbuf *pix = NULL;
    gint pix_w, pix_h, pix_x, pix_y, text_x, text_y, text_w, text_h,
         cell_x, cell_y, state;
    PangoLayout *playout;
    GdkRectangle area;
    const gchar *label;
    guint16 row, col;
    
    TRACE("entering");
    
    g_return_if_fail(xfdesktop_icon_get_position(icon, &row, &col));
    
    if(g_list_find(icon_view->priv->selected_icons, icon)) {
        if(GTK_WIDGET_FLAGS(widget) & GTK_HAS_FOCUS)
            state = GTK_STATE_SELECTED;
        else
            state = GTK_STATE_ACTIVE;
    } else
        state = GTK_STATE_NORMAL;
    
    pix = xfdesktop_icon_peek_pixbuf(icon, ICON_SIZE);
    pix_w = gdk_pixbuf_get_width(pix);
    pix_h = gdk_pixbuf_get_height(pix);
    
    playout = icon_view->priv->playout;
    pango_layout_set_alignment(playout, PANGO_ALIGN_LEFT);
    pango_layout_set_width(playout, -1);
    label = xfdesktop_icon_peek_label(icon);
    pango_layout_set_text(playout, label, -1);
    pango_layout_get_size(playout, &text_w, &text_h);
    DBG("unadjusted size: %dx%d", text_w/PANGO_SCALE, text_h/PANGO_SCALE);
    if(text_w > TEXT_WIDTH * PANGO_SCALE) {
        if(state == GTK_STATE_NORMAL) {
#if GTK_CHECK_VERSION(2, 6, 0)  /* can't find a way to get pango version info */
            pango_layout_set_ellipsize(playout, PANGO_ELLIPSIZE_END);
#endif
        } else {
            pango_layout_set_wrap(playout, PANGO_WRAP_WORD_CHAR);
#if GTK_CHECK_VERSION(2, 6, 0)  /* can't find a way to get pango version info */
            pango_layout_set_ellipsize(playout, PANGO_ELLIPSIZE_NONE);
#endif
        }
        pango_layout_set_width(playout, TEXT_WIDTH * PANGO_SCALE);
        pango_layout_get_size(playout, &text_w, &text_h);
        DBG("adjusted size: %dx%d.  Tried to set width to %d.",
            text_w, text_h, TEXT_WIDTH*PANGO_SCALE);
    }
    pango_layout_get_pixel_size(playout, &text_w, &text_h);
    
    cell_x = icon_view->priv->xorigin + col * CELL_SIZE + CELL_PADDING;
    cell_y = icon_view->priv->yorigin + row * CELL_SIZE + CELL_PADDING;
    
    pix_x = cell_x + ((CELL_SIZE - 2 * CELL_PADDING) - pix_w) / 2;
    pix_y = cell_y + 2 * CELL_PADDING;
    
    /*
    DBG("computing text_x:\n\tcell_x=%d\n\tcell width: %d\n\ttext_w: %d\n\tnon-text space: %d\n\tdiv 2: %d",
        cell_x,
        CELL_SIZE - 2 * CELL_PADDING,
        text_w,
        ((CELL_SIZE - 2 * CELL_PADDING) - text_w),
        ((CELL_SIZE - 2 * CELL_PADDING) - text_w) / 2);
    */
    
    text_x = cell_x + ((CELL_SIZE - 2 * CELL_PADDING) - text_w) / 2;
    text_y = cell_y + 2 * CELL_PADDING + pix_h + SPACING + 2;
    
    DBG("drawing pixbuf at (%d,%d)", pix_x, pix_y);
    
    gdk_draw_pixbuf(GDK_DRAWABLE(widget->window), widget->style->black_gc,
                    pix, 0, 0, pix_x, pix_y, pix_w, pix_h,
                    GDK_RGB_DITHER_NORMAL, 0, 0);
    
    DBG("painting layout: area: %dx%d+%d+%d", text_w, text_h, text_x, text_y);
    
    area.x = text_x;
    area.y = text_y;
    area.width = text_w;
    area.height = text_h;
    
    xfdesktop_paint_rounded_box(icon_view, state, &area);
    
    gtk_paint_layout(widget->style, widget->window, state, FALSE,
                     &area, widget, "label", text_x, text_y, playout);
    
    area.x = (pix_w > text_w + CORNER_ROUNDNESS * 2 ? pix_x : text_x - CORNER_ROUNDNESS);
    area.y = cell_y + (2 * CELL_PADDING);
    area.width = (pix_w > text_w + CORNER_ROUNDNESS * 2 ? pix_w : text_w + CORNER_ROUNDNESS * 2);
    area.height = pix_h + SPACING + text_h + CORNER_ROUNDNESS  + 2;
    xfdesktop_icon_set_extents(icon, &area);
    
#if 0 /* debug */
    gdk_draw_rectangle(GDK_DRAWABLE(widget->window),
                       widget->style->white_gc,
                       FALSE,
                       area.x,
                       area.y,
                       area.width,
                       area.height);
#endif
}

static gboolean
xfdesktop_icon_view_paint_icon_idled(gpointer user_data)
{
    XfdesktopIcon *icon = XFDESKTOP_ICON(user_data);
    XfdesktopIconView *icon_view;
    
    g_return_val_if_fail(XFDESKTOP_IS_ICON(icon), FALSE);
    
    icon_view = g_object_get_data(G_OBJECT(icon), "--xfdesktop-icon-view");
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view), FALSE);
    
    icon_view->priv->icon_repaint_id = 0;
    
    xfdesktop_icon_view_paint_icon(icon_view, icon);
    
    return FALSE;
}

static void
xfdesktop_icons_set_map(gpointer data,
                        gpointer user_data)
{
    XfdesktopIcon *icon = data;
    XfdesktopIconView *icon_view = user_data;
    guint16 row, col;
    
    g_return_if_fail(xfdesktop_icon_get_position(icon, &row, &col));
    
    if(row < icon_view->priv->nrows && col < icon_view->priv->ncols)
        xfdesktop_grid_unset_position_free(icon_view, icon);
}

static void
xfdesktop_icon_view_constrain_icons(XfdesktopIconView *icon_view)
{
    XfdesktopIcon *icon;
    guint16 row, col;
    GList *l, *cur_l;
    
    l = icon_view->priv->icons;
    while(l) {
        cur_l = l;
        l = l->next;
        icon = cur_l->data;
        
        if(!xfdesktop_icon_get_position(icon, &row, &col)
           || (row >= icon_view->priv->nrows || col >= icon_view->priv->ncols))
        {
            if(xfdesktop_grid_get_next_free_position(icon_view, &row, &col)) {
                xfdesktop_icon_set_position(icon, row, col);
                xfdesktop_grid_unset_position_free(icon_view, icon);
                DBG("icon moved to (%d,%d)", row, col);
            } else {
                icon_view->priv->icons = g_list_delete_link(icon_view->priv->icons,
                                                            cur_l);
            }
        }
    }
}

static void
xfdesktop_grid_do_resize(XfdesktopIconView *icon_view)
{
    gint old_rows, old_cols;
    
    old_rows = icon_view->priv->nrows;
    old_cols = icon_view->priv->ncols;
    
    xfdesktop_setup_grids(icon_view);
    
    memset(icon_view->priv->grid_layout, 0,
           icon_view->priv->nrows * icon_view->priv->ncols
           * sizeof(XfdesktopIcon *));
    g_list_foreach(icon_view->priv->icons,
                   xfdesktop_icons_set_map,
                   icon_view);
    DUMP_GRID_LAYOUT(icon_view);
                         
    if(icon_view->priv->icons
       && (old_rows > icon_view->priv->nrows
           || old_cols> icon_view->priv->ncols))
    {
        xfdesktop_icon_view_constrain_icons(icon_view);
    }
    
    gtk_widget_queue_draw(GTK_WIDGET(icon_view));
}

static gboolean
xfdesktop_grid_resize_timeout(gpointer user_data)
{
    XfdesktopIconView *icon_view = user_data;
    
    xfdesktop_grid_do_resize(icon_view);
    
    icon_view->priv->grid_resize_timeout = 0;
    return FALSE;
}


static gboolean
xfdesktop_get_workarea_single(XfdesktopIconView *icon_view,
                              guint ws_num,
                              gint *xorigin,
                              gint *yorigin,
                              gint *width,
                              gint *height)
{
    gboolean ret = FALSE;
    GdkScreen *gscreen;
    Display *dpy;
    Window root;
    Atom property, actual_type = None;
    gint actual_format = 0, first_id;
    gulong nitems = 0, bytes_after = 0, offset = 0;
    unsigned char *data_p = NULL;
    
    g_return_val_if_fail(xorigin && yorigin
                         && width && height, FALSE);
    
    gscreen = gtk_widget_get_screen(GTK_WIDGET(icon_view));
    dpy = GDK_DISPLAY_XDISPLAY(gdk_screen_get_display(gscreen));
    root = GDK_WINDOW_XID(gdk_screen_get_root_window(gscreen));
    property = XInternAtom(dpy, "_NET_WORKAREA", False);
    
    first_id = ws_num * 4;
    
    gdk_error_trap_push();
    
    do {
        if(Success == XGetWindowProperty(dpy, root, property, offset,
                                         G_MAXULONG, False, XA_CARDINAL,
                                         &actual_type, &actual_format, &nitems,
                                         &bytes_after, &data_p))
        {
            gint i;
            gulong *data = (gulong *)data_p;
            
            if(actual_format != 32 || actual_type != XA_CARDINAL) {
                XFree(data);
                break;
            }
            
            i = offset / 32;  /* first element id in this batch */
            
            /* there's probably a better way to do this. */
            if(i + nitems >= first_id && first_id - offset >= 0)
                *xorigin = data[first_id - offset] + SCREEN_MARGIN;
            if(i + nitems >= first_id + 1 && first_id - offset + 1 >= 0)
                *yorigin = data[first_id - offset + 1] + SCREEN_MARGIN;
            if(i + nitems >= first_id + 2 && first_id - offset + 2 >= 0)
                *width = data[first_id - offset + 2] - 2 * SCREEN_MARGIN;
            if(i + nitems >= first_id + 3 && first_id - offset + 3 >= 0) {
                *height = data[first_id - offset + 3] - 2 * SCREEN_MARGIN;
                ret = TRUE;
                break;
            }
            
            offset += actual_format * nitems;
        } else
            break;
    } while(bytes_after > 0);
    
    gdk_error_trap_pop();
    
    return ret;
}


#if 0
/* i don't think i'm going to use these.  xfdesktop will just support a single
 * value for workarea, derived from workspace 0.  that's questionable, but
 * is a lot easier.
*/
static void
desktop_setup_grids(XfceDesktop *desktop,
                    gint nws)
{
    gint i, *xorigins, *yorigins, *widths, *heights, tmp;
    XfceDesktopWorkareaStatus ret;
    
    xorigins = g_new(gint, nws);
    yorigins = g_new(gint, nws);
    widths = g_new(gint, nws);
    heights = g_new(gint, nws);
    
    ret = desktop_get_workarea(desktop, nws, xorigins, yorigins, widths, heights);
    switch(ret) {
        case XFCE_DESKTOP_WORKAREA_OK:
            for(i = 0; i < nws; i++) {
                if(G_LIKELY(!desktop->priv->icon_workspaces[i]))
                    desktop->priv->icon_workspaces[i] = g_new0(XfceDesktopIconWorkspace, 1);
                
                desktop->priv->icon_workspaces[i]->xorigin = xorigins[i];
                desktop->priv->icon_workspaces[i]->yorigin = yorigins[i];
                desktop->priv->icon_workspaces[i]->width = widths[i];
                desktop->priv->icon_workspaces[i]->height = heights[i];
                
                desktop->priv->icon_workspaces[i]->nrows = heights[i] / CELL_SIZE;
                desktop->priv->icon_workspaces[i]->ncols = widths[i] / CELL_SIZE;
                
                tmp = desktop->priv->icon_workspaces[i]->nrows
                      * desktop->priv->icon_workspaces[i]->ncols
                      * sizeof(XfceDesktopIcon *);
                
                if(G_UNLIKELY(desktop->priv->icon_workspaces[i]->grid_layout)) {
                    desktop->priv->icon_workspaces[i]->grid_layout = g_realloc(
                        desktop->priv->icon_workspaces[i]->grid_layout,
                        tmp
                    );
                } else
                    desktop->priv->icon_workspaces[i]->grid_layout = g_malloc0(tmp);
                
                DBG("created grid_layout with %d positions", tmp);
                DUMP_GRID_LAYOUT(desktop->priv->icon_workspaces[i]);
            }
            break;
        
        case XFCE_DESKTOP_WORKAREA_FAILED:
            {
                gint w = gdk_screen_get_width(desktop->priv->gscreen);
                gint h = gdk_screen_get_height(desktop->priv->gscreen);
                for(i = 0; i < nws; i++) {
                    if(G_LIKELY(!desktop->priv->icon_workspaces[i]))
                        desktop->priv->icon_workspaces[i] = g_new0(XfceDesktopIconWorkspace, 1);
                    
                    desktop->priv->icon_workspaces[i]->xorigin = 0;
                    desktop->priv->icon_workspaces[i]->yorigin = 0;
                    desktop->priv->icon_workspaces[i]->width = w;
                    desktop->priv->icon_workspaces[i]->height = h;
                    
                    desktop->priv->icon_workspaces[i]->nrows = h / CELL_SIZE;
                    desktop->priv->icon_workspaces[i]->ncols = w / CELL_SIZE;
                    
                    tmp = desktop->priv->icon_workspaces[i]->nrows
                          * desktop->priv->icon_workspaces[i]->ncols
                          * sizeof(XfceDesktopIcon *);
                    
                    if(G_UNLIKELY(desktop->priv->icon_workspaces[i]->grid_layout)) {
                        desktop->priv->icon_workspaces[i]->grid_layout = g_realloc(
                            desktop->priv->icon_workspaces[i]->grid_layout,
                            tmp
                        );
                    } else
                        desktop->priv->icon_workspaces[i]->grid_layout = g_malloc0(tmp);
                    
                    DBG("created grid_layout with %d positions", tmp);
                    DUMP_GRID_LAYOUT(desktop->priv->icon_workspaces[i]);
                }
            }
            break;
        
        case XFCE_DESKTOP_WORKAREA_ABORTED:
            /* do nothing */
            DBG("got XFCE_DESKTOP_WORKAREA_ABORTED");
            break;
    }
    
    g_free(xorigins);
    g_free(yorigins);
    g_free(widths);
    g_free(heights);
}

static void
desktop_grid_do_resize(XfceDesktop *desktop)
{
    gint i, *old_rows, *old_cols;
    
    /* remember the old sizes */
    old_rows = g_new(gint, desktop->priv->nworkspaces);
    old_cols = g_new(gint, desktop->priv->nworkspaces);
    for(i = 0; i < desktop->priv->nworkspaces; i++) {
        old_rows[i] = desktop->priv->icon_workspaces[i]->nrows;
        old_cols[i] = desktop->priv->icon_workspaces[i]->ncols;
    }
    
    DBG("old geom: %dx%d", old_rows[0], old_cols[0]);
    
    desktop_setup_grids(desktop, desktop->priv->nworkspaces);
    
    DBG("new geom: %dx%d", desktop->priv->icon_workspaces[0]->nrows,
        desktop->priv->icon_workspaces[0]->ncols);
    
    /* first redo each grid_layout map, and then make sure we don't lose any
     * icons off the screen (if the screen got smaller */
    for(i = 0; i < desktop->priv->nworkspaces; i++) {
        if(!desktop->priv->icon_workspaces[i]->icons)
            continue;
        
        memset(desktop->priv->icon_workspaces[i]->grid_layout, 0,
               desktop->priv->icon_workspaces[i]->nrows
               * desktop->priv->icon_workspaces[i]->ncols
               * sizeof(XfceDesktopIcon *));
        g_hash_table_foreach(desktop->priv->icon_workspaces[i]->icons,
                             desktop_icons_set_map,
                             desktop->priv->icon_workspaces[i]);
        DUMP_GRID_LAYOUT(desktop->priv->icon_workspaces[i]);
                             
        if(old_rows[i] > desktop->priv->icon_workspaces[i]->nrows
           || old_cols[i] > desktop->priv->icon_workspaces[i]->ncols)
        {
            g_hash_table_foreach_remove(desktop->priv->icon_workspaces[i]->icons,
                                        desktop_icons_constrain,
                                        desktop->priv->icon_workspaces[i]);
        }
    }
    
    g_free(old_rows);
    g_free(old_cols);
    
    gtk_widget_queue_draw(GTK_WIDGET(desktop));
}

static XfceDesktopWorkareaStatus
xfdesktop_get_workarea(XfdesktopIconView *icon_view,
                     guint nworkspaces,
                     gint *xorigins,
                     gint *yorigins,
                     gint *widths,
                     gint *heights)
{
    gboolean ret = XFCE_DESKTOP_WORKAREA_FAILED;
    Display *dpy;
    Window root;
    Atom property, actual_type = None;
    gint actual_format = 0, new_nworkspaces = -1;
    gulong nitems = 0, bytes_after = 0, *data = NULL, offset = 0;
    gint *full_data, i = 0, j;
    unsigned char *data_p = NULL;
    
    g_return_val_if_fail(xorigins && yorigins
                         && widths && heights, FALSE);
    
    full_data = g_new0(gint, nworkspaces * 4);
    
    dpy = GDK_DISPLAY_XDISPLAY(gtk_widget_get_display(GTK_WIDGET(desktop)));
    root = GDK_WINDOW_XID(gdk_screen_get_root_window(desktop->priv->gscreen));
    property = XInternAtom(dpy, "_NET_WORKAREA", False);
    
    gdk_error_trap_push();
    
    do {
        if(Success == XGetWindowProperty(dpy, root, property, offset,
                                         G_MAXULONG, False, XA_CARDINAL,
                                         &actual_type, &actual_format, &nitems,
                                         &bytes_after, &data_p))
        {
            if(!data_p)
                break;
            
            if(actual_format != 32 || actual_type != XA_CARDINAL) {
                XFree(data_p);
                break;
            }
            
            if(new_nworkspaces == -1) {
                gint tot_bytes = nitems * sizeof(gulong) + bytes_after;
                new_nworkspaces = tot_bytes / sizeof(gulong) / 4;
                DBG("WORKAREA says we have %d workspaces now (nitems=%ld, tot_bytes=%d, bytes_after=%ld", new_nworkspaces, nitems, tot_bytes, bytes_after);
                
                if(new_nworkspaces != nworkspaces) {
                    DBG("bailing on getting workarea: workspaces count doesn't match");
                    XFree(data_p);
                    ret = XFCE_DESKTOP_WORKAREA_ABORTED;
                    break;
                }
            }
            
            data = (gulong *)data_p;
            for(j = 0; j < nitems; j++, i++)
                full_data[i] = data[j];
            XFree(data_p);
            data_p = NULL;
            
            if(i == nworkspaces * 4)
                ret = XFCE_DESKTOP_WORKAREA_OK;
            
            offset += actual_format * nitems;
            
        } else
            break;
    } while(bytes_after > 0);
    
    gdk_error_trap_pop();
    
    if(ret == XFCE_DESKTOP_WORKAREA_OK) {
        for(i = 0; i < nworkspaces*4; i += 4) {
            xorigins[i/4] = full_data[i] + SCREEN_MARGIN;
            yorigins[i/4] = full_data[i+1] + SCREEN_MARGIN;
            widths[i/4] = full_data[i+2] - 2 * SCREEN_MARGIN;
            heights[i/4] = full_data[i+3] - 2 * SCREEN_MARGIN;
        }
    }
    
    g_free(full_data);
    
    return ret;
}
#endif


static inline gboolean
xfdesktop_grid_is_free_position(XfdesktopIconView *icon_view,
                                guint16 row,
                                guint16 col)
{
    g_return_val_if_fail(row < icon_view->priv->nrows
                         && col < icon_view->priv->ncols, FALSE);
    
    return !icon_view->priv->grid_layout[col * icon_view->priv->nrows + row];
}


static gboolean
xfdesktop_grid_get_next_free_position(XfdesktopIconView *icon_view,
                                      guint16 *row,
                                      guint16 *col)
{
    gint i, maxi;
    
    g_return_val_if_fail(row && col, FALSE);
    
    maxi = icon_view->priv->nrows * icon_view->priv->ncols;
    for(i = 0; i < maxi; ++i) {
        if(!icon_view->priv->grid_layout[i]) {
            *row = i % icon_view->priv->nrows;
            *col = i / icon_view->priv->nrows;
            return TRUE;
        }
    }
    
    return FALSE;
}


static inline void
xfdesktop_grid_set_position_free(XfdesktopIconView *icon_view,
                                 guint16 row,
                                 guint16 col)
{
    g_return_if_fail(row < icon_view->priv->nrows
                     && col < icon_view->priv->ncols);
    
    DUMP_GRID_LAYOUT(icon_view);
    icon_view->priv->grid_layout[col * icon_view->priv->nrows + row] = NULL;
    DUMP_GRID_LAYOUT(icon_view);
}


static inline void
xfdesktop_grid_unset_position_free(XfdesktopIconView *icon_view,
                                   XfdesktopIcon *icon)
{
    guint16 row, col;
    
    g_return_if_fail(xfdesktop_icon_get_position(icon, &row, &col));
    g_return_if_fail(row < icon_view->priv->nrows
                     && col < icon_view->priv->ncols);
    
    DUMP_GRID_LAYOUT(icon_view);
    icon_view->priv->grid_layout[col * icon_view->priv->nrows + row] = icon;
    DUMP_GRID_LAYOUT(icon_view);
}

static void
xfdesktop_grid_find_nearest(XfdesktopIconView *icon_view,
                            XfdesktopIcon *icon,
                            XfdesktopDirection dir,
                            gboolean allow_multiple)
{
    XfdesktopIcon **grid_layout = icon_view->priv->grid_layout;
    gint i, maxi;
    guint16 row, col;
    
    if(icon && !xfdesktop_icon_get_position(icon, &row, &col))
        return;
    
    if((icon_view->priv->sel_mode != GTK_SELECTION_MULTIPLE
        || !allow_multiple)
       && icon_view->priv->selected_icons)
    {
        xfdesktop_icon_view_unselect_all(icon_view);
    }
    
    if(!icon) {
        maxi = icon_view->priv->nrows * icon_view->priv->ncols;
        for(i = 0; i < maxi; ++i) {
            if(grid_layout[i]) {
                icon_view->priv->selected_icons = g_list_prepend(icon_view->priv->selected_icons,
                                                                 grid_layout[i]);
                xfdesktop_icon_view_clear_icon_extents(icon_view,
                                                       grid_layout[i]);
                xfdesktop_icon_view_paint_icon(icon_view, grid_layout[i]);
                return;
            }
        }
    } else {
        gint cur_i = col * icon_view->priv->nrows + row;
        XfdesktopIcon *new_sel_icon = NULL;
        
        switch(dir) {
            case XFDESKTOP_DIRECTION_UP:
                for(i = cur_i - 1; i >= 0; --i) {
                    if(grid_layout[i]) {
                        new_sel_icon = grid_layout[i];
                        break;
                    }
                }
                break;
            
            case XFDESKTOP_DIRECTION_DOWN:
                maxi = icon_view->priv->nrows * icon_view->priv->ncols;
                for(i = cur_i + 1; i < maxi; ++i) {
                    if(grid_layout[i]) {
                        new_sel_icon = grid_layout[i];
                        break;
                    }
                }
                break;
            
            case XFDESKTOP_DIRECTION_LEFT:
                if(cur_i == 0)
                    return;
                
                for(i = cur_i >= icon_view->priv->nrows
                        ? cur_i - icon_view->priv->nrows
                        : icon_view->priv->nrows * icon_view->priv->ncols - 1
                          - (icon_view->priv->nrows - cur_i);
                    i >= 0;
                    i = i >= icon_view->priv->nrows
                        ? i - icon_view->priv->nrows
                        : icon_view->priv->nrows * icon_view->priv->ncols - 1
                          - (icon_view->priv->nrows - i))
                {
                    if(grid_layout[i]) {
                        new_sel_icon = grid_layout[i];
                        break;
                    }
                    
                    if(i == 0)
                        break;
                }
                break;
            
            case XFDESKTOP_DIRECTION_RIGHT:
                maxi = icon_view->priv->nrows * icon_view->priv->ncols;
                if(cur_i == maxi - 1)
                    return;
                
                for(i = cur_i < icon_view->priv->nrows * (icon_view->priv->ncols - 1)
                        ? cur_i + icon_view->priv->nrows
                        : cur_i % icon_view->priv->nrows + 1;
                    i < maxi;
                    i = i < icon_view->priv->nrows * (icon_view->priv->ncols - 1)
                        ? i + icon_view->priv->nrows
                        : i % icon_view->priv->nrows + 1)
                {
                    if(grid_layout[i]) {
                        new_sel_icon = grid_layout[i];
                        break;
                    }
                    
                    if(i == maxi - 1)
                        return;
                }
                break;
            
            default:
                break;
        }
        
        if(new_sel_icon) {
            icon_view->priv->selected_icons = g_list_prepend(icon_view->priv->selected_icons,
                                                             grid_layout[i]);
            xfdesktop_icon_view_clear_icon_extents(icon_view, icon);
            xfdesktop_icon_view_clear_icon_extents(icon_view, grid_layout[i]);
            xfdesktop_icon_view_paint_icon(icon_view, icon);
            xfdesktop_icon_view_paint_icon(icon_view, grid_layout[i]);
        }
    }
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
    XfdesktopIcon *icon = XFDESKTOP_ICON(data);
    GdkEventButton *evt = (GdkEventButton *)user_data;
    GdkRectangle extents;
    
    if(xfdesktop_icon_get_extents(icon, &extents)
       && xfdesktop_rectangle_contains_point(&extents, evt->x, evt->y))
    {
        return 0;
    } else
        return 1;
}

static void
xfdesktop_list_foreach_repaint(gpointer data,
                               gpointer user_data)
{
    XfdesktopIconView *icon_view = XFDESKTOP_ICON_VIEW(user_data);
    xfdesktop_icon_view_clear_icon_extents(icon_view, (XfdesktopIcon *)data);
    xfdesktop_icon_view_paint_icon(icon_view, (XfdesktopIcon *)data);
}





/* public api */


GtkWidget *
xfdesktop_icon_view_new(XfdesktopIconViewManager *manager)
{
    XfdesktopIconView *icon_view;
    
    g_return_val_if_fail(manager, NULL);
    
    icon_view = g_object_new(XFDESKTOP_TYPE_ICON_VIEW, NULL);
    icon_view->priv->manager = manager;
    
    return GTK_WIDGET(icon_view);
}

void
xfdesktop_icon_view_add_item(XfdesktopIconView *icon_view,
                             XfdesktopIcon *icon)
{
    gboolean icon_has_pos;
    guint16 row, col;
    
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view)
                     && XFDESKTOP_IS_ICON(icon));
    
    icon_has_pos = xfdesktop_icon_get_position(icon, &row, &col);
    
    if(!GTK_WIDGET_REALIZED(GTK_WIDGET(icon_view))) {
        if(icon_has_pos) {
            icon_view->priv->pending_icons = g_list_prepend(icon_view->priv->pending_icons,
                                                            icon);
        } else {
            icon_view->priv->pending_icons = g_list_append(icon_view->priv->pending_icons,
                                                           icon);
        }
        return;
    }
    
    if(!icon_has_pos || !xfdesktop_grid_is_free_position(icon_view, row, col)) {
        if(xfdesktop_grid_get_next_free_position(icon_view, &row, &col)) {
            DBG("old position didn't exist or isn't free, got (%d,%d) instead",
                row, col);
            xfdesktop_icon_set_position(icon, row, col);
        } else {
            DBG("can't fit icon on screen");
            return;
        }
    } 
    
    xfdesktop_grid_unset_position_free(icon_view, icon);
    
    icon_view->priv->icons = g_list_prepend(icon_view->priv->icons, icon);
    
    g_object_set_data(G_OBJECT(icon), "--xfdesktop-icon-view", icon_view);
    
    xfdesktop_icon_view_paint_icon(icon_view, icon);
}

void
xfdesktop_icon_view_remove_item(XfdesktopIconView *icon_view,
                                XfdesktopIcon *icon)
{
    gint16 row, col;
    
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view)
                     && XFDESKTOP_IS_ICON(icon));
    
    if(!GTK_WIDGET_REALIZED(GTK_WIDGET(icon_view))) {
        icon_view->priv->pending_icons = g_list_remove(icon_view->priv->pending_icons,
                                                       icon);
        return;
    }
    
    if(g_list_find(icon_view->priv->icons, icon)) {
        XfdesktopIcon *icon_below = NULL;
        GdkRectangle extents;
        
        if(xfdesktop_icon_get_extents(icon, &extents)
           && extents.height + 3 * CELL_PADDING > CELL_SIZE)
        {
            icon_below = xfdesktop_find_icon_below(icon_view, icon);
        }
        
        if(xfdesktop_icon_get_position(icon, &row, &col)) {
            xfdesktop_icon_view_clear_icon_extents(icon_view, icon);
            xfdesktop_grid_set_position_free(icon_view, row, col);
        }
        icon_view->priv->icons = g_list_remove(icon_view->priv->icons, icon);
        icon_view->priv->selected_icons = g_list_remove(icon_view->priv->selected_icons,
                                                        icon);
        
        if(icon_below)
            xfdesktop_icon_view_paint_icon(icon_view, icon_below);
    }
    
    g_object_set_data(G_OBJECT(icon), "--xfdesktop-icon-view", NULL);
}

void
xfdesktop_icon_view_remove_all(XfdesktopIconView *icon_view)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));
    
    g_list_free(icon_view->priv->pending_icons);
    icon_view->priv->pending_icons = NULL;
    
    g_list_free(icon_view->priv->icons);
    icon_view->priv->icons = NULL;
    
    g_list_free(icon_view->priv->selected_icons);
    icon_view->priv->selected_icons = NULL;
    
    if(GTK_WIDGET_REALIZED(GTK_WIDGET(icon_view))) {
        GdkWindow *window;
        gint w, h;
        
        memset(icon_view->priv->grid_layout, 0, icon_view->priv->nrows
                                                * icon_view->priv->ncols
                                                * sizeof(gpointer));
        
        window = GTK_WIDGET(icon_view)->window;
        gdk_drawable_get_size(GDK_DRAWABLE(window), &w, &h);
        gdk_window_clear_area(window, 0, 0, w, h);
        gtk_widget_queue_draw(GTK_WIDGET(icon_view));
    }
}

void
xfdesktop_icon_view_set_selection_mode(XfdesktopIconView *icon_view,
                                       GtkSelectionMode mode)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));
    g_return_if_fail(mode <= GTK_SELECTION_MULTIPLE);
    
    if(mode == icon_view->priv->sel_mode)
        return;
    
    icon_view->priv->sel_mode = mode;
    
    switch(mode) {
        case GTK_SELECTION_NONE:
            g_warning("GTK_SELECTION_NONE is not implemented for " \
                      "XfdesktopIconView.  Falling back to " \
                      "GTK_SELECTION_SINGLE.");
            icon_view->priv->sel_mode = GTK_SELECTION_SINGLE;
            /* fall through */
        case GTK_SELECTION_SINGLE:
            if(g_list_length(icon_view->priv->selected_icons) > 1) {
                GList *l;
                /* TODO: enable later and make sure it works */
                /*gdk_window_freeze_updates(GTK_WIDGET(icon_view)->window);*/
                for(l = icon_view->priv->selected_icons->next; l; l = l->next) {
                    xfdesktop_icon_view_unselect_item(icon_view,
                                                      XFDESKTOP_ICON(l->data));
                }
                /*gdk_window_thaw_updates(GTK_WIDGET(icon_view)->window);*/
            }
            break;
        
        case GTK_SELECTION_BROWSE:
            g_warning("GTK_SELECTION_BROWSE is not implemented for " \
                  "XfdesktopIconView.  Falling back to " \
                  "GTK_SELECTION_MULTIPLE.");
            icon_view->priv->sel_mode = GTK_SELECTION_MULTIPLE;
            break;
        
        default:
            break;
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
xfdesktop_icon_view_set_allow_overlapping_drops(XfdesktopIconView *icon_view,
                                                gboolean allow_overlap)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));
    
    icon_view->priv->allow_overlapping_drops = allow_overlap;
}

gboolean
xfdesktop_icon_view_get_allow_overlapping_drops(XfdesktopIconView *icon_view)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view), FALSE);
    
    return icon_view->priv->allow_overlapping_drops;
}

void
xfdesktop_icon_view_enable_drag_source(XfdesktopIconView *icon_view,
                                       GdkModifierType start_button_mask,
                                       const GtkTargetEntry *targets,
                                       gint n_targets,
                                       GdkDragAction actions)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));
    
    if(icon_view->priv->foreign_source_targets)
        gtk_target_list_unref(icon_view->priv->foreign_source_targets);
    
    gtk_drag_source_set(GTK_WIDGET(icon_view), start_button_mask, NULL, 0,
                        GDK_ACTION_MOVE | actions);
    
    icon_view->priv->foreign_source_targets = gtk_target_list_new(targets,
                                                                  n_targets);
    icon_view->priv->foreign_source_actions = actions;
    icon_view->priv->foreign_source_mask = start_button_mask;
}

void
xfdesktop_icon_view_enable_drag_dest(XfdesktopIconView *icon_view,
                                     const GtkTargetEntry *targets,
                                     gint n_targets,
                                     GdkDragAction actions)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));
    
    if(icon_view->priv->foreign_dest_targets)
        gtk_target_list_unref(icon_view->priv->foreign_dest_targets);
    
    gtk_drag_dest_set(GTK_WIDGET(icon_view), 0, NULL, 0,
                      GDK_ACTION_MOVE | actions);
    
    icon_view->priv->foreign_dest_targets = gtk_target_list_new(targets,
                                                                n_targets);
    icon_view->priv->foreign_dest_actions = actions;
}

void
xfdesktop_icon_view_unset_drag_source(XfdesktopIconView *icon_view)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));
    
    if(icon_view->priv->foreign_source_targets) {
        gtk_target_list_unref(icon_view->priv->foreign_source_targets);
        icon_view->priv->foreign_source_targets = NULL;
    }
}

void
xfdesktop_icon_view_unset_drag_dest(XfdesktopIconView *icon_view)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));
    
    if(icon_view->priv->foreign_dest_targets) {
        gtk_target_list_unref(icon_view->priv->foreign_dest_targets);
        icon_view->priv->foreign_dest_targets = NULL;
    }
}

XfdesktopIcon *
xfdesktop_icon_view_widget_coords_to_item(XfdesktopIconView *icon_view,
                                          gint wx,
                                          gint wy)
{
    XfdesktopIcon *icon = NULL;
    guint16 row, col;
    
    xfdesktop_xy_to_rowcol(icon_view, wx, wy, &row, &col);
    icon = icon_view->priv->grid_layout[col * icon_view->priv->nrows + row];
    
    return icon;
}

GList *
xfdesktop_icon_view_get_selected_items(XfdesktopIconView *icon_view)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view), NULL);
    
    return g_list_copy(icon_view->priv->selected_icons);
}

void
xfdesktop_icon_view_select_item(XfdesktopIconView *icon_view,
                                XfdesktopIcon *icon)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));
    
    if(g_list_find(icon_view->priv->selected_icons, icon))
        return;
    
    if(icon_view->priv->sel_mode == GTK_SELECTION_SINGLE)
        xfdesktop_icon_view_unselect_all(icon_view);
    
    icon_view->priv->selected_icons = g_list_prepend(icon_view->priv->selected_icons,
                                                     icon);
    xfdesktop_icon_view_clear_icon_extents(icon_view, icon);
    xfdesktop_icon_view_paint_icon(icon_view, icon);
}

void
xfdesktop_icon_view_unselect_item(XfdesktopIconView *icon_view,
                                  XfdesktopIcon *icon)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view)
                     && XFDESKTOP_IS_ICON(icon));
    
    if(g_list_find(icon_view->priv->selected_icons, icon)) {
        icon_view->priv->selected_icons = g_list_remove(icon_view->priv->selected_icons,
                                                        icon);
        xfdesktop_icon_view_clear_icon_extents(icon_view, icon);
        xfdesktop_icon_view_paint_icon(icon_view, icon);
    }
}

void
xfdesktop_icon_view_unselect_all(XfdesktopIconView *icon_view)
{
    g_return_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view));
    
    if(icon_view->priv->selected_icons) {
        GList *repaint_icons = icon_view->priv->selected_icons;
        icon_view->priv->selected_icons = NULL;
        g_list_foreach(repaint_icons, xfdesktop_list_foreach_repaint,
                       icon_view);
        g_list_free(repaint_icons);
    }
}

GtkWidget *
xfdesktop_icon_view_get_window_widget(XfdesktopIconView *icon_view)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view), NULL);
    
    return icon_view->priv->parent_window;
}
