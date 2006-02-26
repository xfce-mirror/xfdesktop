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

#include <libxfcegui4/libxfcegui4.h>
#include <libxfcegui4/netk-window-action-menu.h>

#include "xfdesktop-icon.h"
#include "xfdesktop-window-icon.h"

struct _XfdesktopWindowIconPrivate
{
    gint workspace;
    gint16 row;
    gint16 col;
    GdkPixbuf *pix;
    gint cur_pix_size;
    gchar *label;
    GdkRectangle extents;
    NetkWindow *window;
};

static void xfdesktop_window_icon_icon_init(XfdesktopIconIface *iface);
static void xfdesktop_window_icon_finalize(GObject *obj);

static GdkPixbuf *xfdesktop_window_icon_peek_pixbuf(XfdesktopIcon *icon,
                                                   gint size);
static G_CONST_RETURN gchar *xfdesktop_window_icon_peek_label(XfdesktopIcon *icon);

static void xfdesktop_window_icon_set_position(XfdesktopIcon *icon,
                                               gint16 row,
                                               gint16 col);
static gboolean xfdesktop_window_icon_get_position(XfdesktopIcon *icon,
                                                   gint16 *row,
                                                   gint16 *col);

static void xfdesktop_window_icon_set_extents(XfdesktopIcon *icon,
                                              const GdkRectangle *extents);
static gboolean xfdesktop_window_icon_get_extents(XfdesktopIcon *icon,
                                                  GdkRectangle *extents);

static gboolean xfdesktop_window_icon_is_drop_dest(XfdesktopIcon *icon);

static void xfdesktop_window_icon_selected(XfdesktopIcon *icon);
static void xfdesktop_window_icon_activated(XfdesktopIcon *icon);
static void xfdesktop_window_icon_menu_popup(XfdesktopIcon *icon);

static void xfdesktop_window_name_changed_cb(NetkWindow *window,
                                             gpointer user_data);
static void xfdesktop_window_icon_changed_cb(NetkWindow *window,
                                             gpointer user_data);


G_DEFINE_TYPE_EXTENDED(XfdesktopWindowIcon, xfdesktop_window_icon,
                       G_TYPE_OBJECT, 0,
                       G_IMPLEMENT_INTERFACE(XFDESKTOP_TYPE_ICON,
                                             xfdesktop_window_icon_icon_init))

static void
xfdesktop_window_icon_class_init(XfdesktopWindowIconClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;
    
    gobject_class->finalize = xfdesktop_window_icon_finalize;
}

static void
xfdesktop_window_icon_init(XfdesktopWindowIcon *icon)
{
    icon->priv = g_new0(XfdesktopWindowIconPrivate, 1);
}

static void
xfdesktop_window_icon_finalize(GObject *obj)
{
    XfdesktopWindowIcon *icon = XFDESKTOP_WINDOW_ICON(obj);
    gchar data_name[256];
    
    if(icon->priv->pix)
        g_object_unref(G_OBJECT(icon->priv->pix));
    
    g_free(icon->priv->label);
    
    /* save previous position */
    if(icon->priv->row >= 0 && icon->priv->col >= 0) {
        g_snprintf(data_name, 256, "--xfdesktop-last-row-%d",
                   icon->priv->workspace);
        g_object_set_data(G_OBJECT(icon->priv->window),
                          data_name,
                          GUINT_TO_POINTER(icon->priv->row + 1));
        g_snprintf(data_name, 256, "--xfdesktop-last-col-%d",
                   icon->priv->workspace);
        g_object_set_data(G_OBJECT(icon->priv->window),
                          data_name,
                          GUINT_TO_POINTER(icon->priv->col + 1));
    }
    
    g_signal_handlers_disconnect_by_func(G_OBJECT(icon->priv->window),
                                         G_CALLBACK(xfdesktop_window_name_changed_cb),
                                         icon);
    g_signal_handlers_disconnect_by_func(G_OBJECT(icon->priv->window),
                                         G_CALLBACK(xfdesktop_window_icon_changed_cb),
                                         icon);
    
    G_OBJECT_CLASS(xfdesktop_window_icon_parent_class)->finalize(obj);
}



static void
xfdesktop_window_name_changed_cb(NetkWindow *window,
                                 gpointer user_data)
{
    XfdesktopWindowIcon *icon = user_data;
    
    g_free(icon->priv->label);
    icon->priv->label = NULL;
    
    xfdesktop_icon_label_changed(XFDESKTOP_ICON(icon));
}

static void
xfdesktop_window_icon_changed_cb(NetkWindow *window,
                                 gpointer user_data)
{
    XfdesktopWindowIcon *icon = user_data;
    
    if(icon->priv->pix) {
        g_object_unref(G_OBJECT(icon->priv->pix));
        icon->priv->pix = NULL;
    }
    
    xfdesktop_icon_label_changed(XFDESKTOP_ICON(icon));
}



static void
xfdesktop_window_icon_icon_init(XfdesktopIconIface *iface)
{
    iface->peek_pixbuf = xfdesktop_window_icon_peek_pixbuf;
    iface->peek_label = xfdesktop_window_icon_peek_label;
    iface->set_position = xfdesktop_window_icon_set_position;
    iface->get_position = xfdesktop_window_icon_get_position;
    iface->set_extents = xfdesktop_window_icon_set_extents;
    iface->get_extents = xfdesktop_window_icon_get_extents;
    iface->is_drop_dest = xfdesktop_window_icon_is_drop_dest;
    iface->selected = xfdesktop_window_icon_selected;
    iface->activated = xfdesktop_window_icon_activated;
    iface->menu_popup = xfdesktop_window_icon_menu_popup;
}

XfdesktopWindowIcon *
xfdesktop_window_icon_new(NetkWindow *window,
                          gint workspace)
{
    XfdesktopWindowIcon *icon = g_object_new(XFDESKTOP_TYPE_WINDOW_ICON, NULL);
    gchar data_name[256];
    gint row, col;
    
    icon->priv->window = window;
    icon->priv->workspace = workspace;
    
    /* check for availability of old position (if any) */
    g_snprintf(data_name, 256, "--xfdesktop-last-row-%d", workspace);
    row = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(window),
                                             data_name));
    if(row > 0)
        icon->priv->row = row - 1;
    else
        icon->priv->row = -1;
    
    g_snprintf(data_name, 256, "--xfdesktop-last-col-%d", workspace);
    col = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(window),
                                             data_name));
    if(col > 0)
        icon->priv->col = col - 1;
    else
        icon->priv->col = -1;
    
    g_signal_connect(G_OBJECT(window), "name-changed",
                     G_CALLBACK(xfdesktop_window_name_changed_cb),
                     icon);
    g_signal_connect(G_OBJECT(window), "icon-changed",
                     G_CALLBACK(xfdesktop_window_icon_changed_cb),
                     icon);
    
    return icon;
}

gint
xfdesktop_window_icon_get_workspace(XfdesktopWindowIcon *window_icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_WINDOW_ICON(window_icon), -1);
    return window_icon->priv->workspace;
}

static GdkPixbuf *
xfdesktop_window_icon_peek_pixbuf(XfdesktopIcon *icon,
                                 gint size)
{
    XfdesktopWindowIcon *window_icon = XFDESKTOP_WINDOW_ICON(icon);
    
    if(!window_icon->priv->pix || window_icon->priv->cur_pix_size != size) {
        if(window_icon->priv->pix)
            g_object_unref(G_OBJECT(window_icon->priv->pix));
        
        window_icon->priv->pix = netk_window_get_icon(window_icon->priv->window);
        if(window_icon->priv->pix) {
            if(gdk_pixbuf_get_width(window_icon->priv->pix) != size) {
                window_icon->priv->pix = gdk_pixbuf_scale_simple(window_icon->priv->pix,
                                                                 size,
                                                                 size,
                                                                 GDK_INTERP_BILINEAR);
            } else
                g_object_ref(G_OBJECT(window_icon->priv->pix));
            window_icon->priv->cur_pix_size = size;
        }
    }
    
    return window_icon->priv->pix;
}

static G_CONST_RETURN gchar *
xfdesktop_window_icon_peek_label(XfdesktopIcon *icon)
{
    XfdesktopWindowIcon *window_icon = XFDESKTOP_WINDOW_ICON(icon);
    
    if(!window_icon->priv->label)
        window_icon->priv->label = g_strdup(netk_window_get_name(window_icon->priv->window));
    
    return window_icon->priv->label;
}

static void
xfdesktop_window_icon_set_position(XfdesktopIcon *icon,
                                   gint16 row,
                                   gint16 col)
{
    XfdesktopWindowIcon *window_icon = XFDESKTOP_WINDOW_ICON(icon);
    
    window_icon->priv->row = row;
    window_icon->priv->col = col;
}
    
static gboolean
xfdesktop_window_icon_get_position(XfdesktopIcon *icon,
                                   gint16 *row,
                                   gint16 *col)
{
    XfdesktopWindowIcon *window_icon = XFDESKTOP_WINDOW_ICON(icon);
    
    *row = window_icon->priv->row;
    *col = window_icon->priv->col;
    
    return TRUE;
}

static void
xfdesktop_window_icon_set_extents(XfdesktopIcon *icon,
                                  const GdkRectangle *extents)
{
    XfdesktopWindowIcon *window_icon = XFDESKTOP_WINDOW_ICON(icon);
    
    memcpy(&window_icon->priv->extents, extents, sizeof(GdkRectangle));
}

static gboolean
xfdesktop_window_icon_get_extents(XfdesktopIcon *icon,
                                  GdkRectangle *extents)
{
    XfdesktopWindowIcon *window_icon = XFDESKTOP_WINDOW_ICON(icon);
    
    if(window_icon->priv->extents.width > 0
       && window_icon->priv->extents.height > 0)
    {
        memcpy(extents, &window_icon->priv->extents, sizeof(GdkRectangle));
        return TRUE;
    }
    
    return FALSE;
}

static gboolean
xfdesktop_window_icon_is_drop_dest(XfdesktopIcon *icon)
{
    return FALSE;
}

static void
xfdesktop_window_icon_selected(XfdesktopIcon *icon)
{
    /* nada */
}

static void
xfdesktop_window_icon_activated(XfdesktopIcon *icon)
{
    XfdesktopWindowIcon *window_icon = XFDESKTOP_WINDOW_ICON(icon);
    
    netk_window_activate(window_icon->priv->window);
}


static gboolean
xfdesktop_action_menu_destroy_idled(gpointer data)
{
    gtk_widget_destroy(GTK_WIDGET(data));
    return FALSE;
}

static void
xfdesktop_action_menu_deactivate_cb(GtkMenu *menu,
                          gpointer user_data)
{
    g_idle_add(xfdesktop_action_menu_destroy_idled, menu);
}

static void
xfdesktop_window_icon_menu_popup(XfdesktopIcon *icon)
{
    XfdesktopWindowIcon *window_icon = XFDESKTOP_WINDOW_ICON(icon);
    GtkWidget *menu = netk_create_window_action_menu(window_icon->priv->window);
    
    gtk_widget_show(menu);
    g_signal_connect(G_OBJECT(menu), "deactivate",
                     G_CALLBACK(xfdesktop_action_menu_deactivate_cb), NULL);
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
                   3, gtk_get_current_event_time());
}
