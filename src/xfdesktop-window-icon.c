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

#include "xfdesktop-window-icon.h"

struct _XfdesktopWindowIconPrivate
{
    gint workspace;
    GdkPixbuf *pix;
    gint cur_pix_size;
    gchar *label;
    NetkWindow *window;
};

static void xfdesktop_window_icon_class_init(XfdesktopWindowIconClass *klass);
static void xfdesktop_window_icon_init(XfdesktopWindowIcon *icon);
static void xfdesktop_window_icon_finalize(GObject *obj);

static GdkPixbuf *xfdesktop_window_icon_peek_pixbuf(XfdesktopIcon *icon,
                                                   gint size);
static G_CONST_RETURN gchar *xfdesktop_window_icon_peek_label(XfdesktopIcon *icon);

static gboolean xfdesktop_window_icon_is_drop_dest(XfdesktopIcon *icon);
static XfdesktopIconDragResult xfdesktop_window_icon_do_drop_dest(XfdesktopIcon *icon,
                                                                  XfdesktopIcon *src_icon,
                                                                  GdkDragAction action);

static void xfdesktop_window_icon_selected(XfdesktopIcon *icon);
static gboolean xfdesktop_window_icon_activated(XfdesktopIcon *icon);
static void xfdesktop_window_icon_menu_popup(XfdesktopIcon *icon);

static void xfdesktop_window_name_changed_cb(NetkWindow *window,
                                             gpointer user_data);
static void xfdesktop_window_icon_changed_cb(NetkWindow *window,
                                             gpointer user_data);


G_DEFINE_TYPE(XfdesktopWindowIcon, xfdesktop_window_icon, XFDESKTOP_TYPE_ICON)


static void
xfdesktop_window_icon_class_init(XfdesktopWindowIconClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;
    XfdesktopIconClass *icon_class = (XfdesktopIconClass *)klass;
    
    g_type_class_add_private(klass, sizeof(XfdesktopWindowIconPrivate));
    
    gobject_class->finalize = xfdesktop_window_icon_finalize;
    
    icon_class->peek_pixbuf = xfdesktop_window_icon_peek_pixbuf;
    icon_class->peek_label = xfdesktop_window_icon_peek_label;
    icon_class->is_drop_dest = xfdesktop_window_icon_is_drop_dest;
    icon_class->do_drop_dest = xfdesktop_window_icon_do_drop_dest;
    icon_class->selected = xfdesktop_window_icon_selected;
    icon_class->activated = xfdesktop_window_icon_activated;
    icon_class->menu_popup = xfdesktop_window_icon_menu_popup;
}

static void
xfdesktop_window_icon_init(XfdesktopWindowIcon *icon)
{
    icon->priv = G_TYPE_INSTANCE_GET_PRIVATE(icon, XFDESKTOP_TYPE_WINDOW_ICON,
                                             XfdesktopWindowIconPrivate);
}

static void
xfdesktop_window_icon_finalize(GObject *obj)
{
    XfdesktopWindowIcon *icon = XFDESKTOP_WINDOW_ICON(obj);
    gchar data_name[256];
    guint16 row, col;
    
    if(icon->priv->pix)
        g_object_unref(G_OBJECT(icon->priv->pix));
    
    g_free(icon->priv->label);
    
    /* save previous position */
    if(xfdesktop_icon_get_position(XFDESKTOP_ICON(icon), &row, &col)) {
        g_snprintf(data_name, 256, "--xfdesktop-last-row-%d",
                   icon->priv->workspace);
        g_object_set_data(G_OBJECT(icon->priv->window),
                          data_name, GUINT_TO_POINTER(row + 1));
        g_snprintf(data_name, 256, "--xfdesktop-last-col-%d",
                   icon->priv->workspace);
        g_object_set_data(G_OBJECT(icon->priv->window),
                          data_name, GUINT_TO_POINTER(col + 1));
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
    g_snprintf(data_name, 256, "--xfdesktop-last-col-%d", workspace);
    col = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(window),
                                             data_name));
    if(row > 0 && col > 0)
        xfdesktop_icon_set_position(XFDESKTOP_ICON(icon), row - 1, col - 1);
    
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

static gboolean
xfdesktop_window_icon_is_drop_dest(XfdesktopIcon *icon)
{
    return FALSE;
}

static XfdesktopIconDragResult
xfdesktop_window_icon_do_drop_dest(XfdesktopIcon *icon,
                                   XfdesktopIcon *src_icon,
                                   GdkDragAction action)
{
    return XFDESKTOP_ICON_DRAG_FAILED;
}

static void
xfdesktop_window_icon_selected(XfdesktopIcon *icon)
{
    /* nada */
}

static gboolean
xfdesktop_window_icon_activated(XfdesktopIcon *icon)
{
    XfdesktopWindowIcon *window_icon = XFDESKTOP_WINDOW_ICON(icon);
    
    netk_window_activate(window_icon->priv->window);
    
    return TRUE;
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
    NetkScreen *netk_screen;
    gint screen_num;
    GdkScreen *gscreen;
    
    netk_screen = netk_window_get_screen(window_icon->priv->window);
    screen_num = netk_screen_get_number(netk_screen);
    gscreen = gdk_display_get_screen(gdk_display_get_default(), screen_num);
    gtk_menu_set_screen(GTK_MENU(menu), gscreen);
    gtk_widget_show(menu);
    g_signal_connect(G_OBJECT(menu), "deactivate",
                     G_CALLBACK(xfdesktop_action_menu_deactivate_cb), NULL);
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
                   3, gtk_get_current_event_time());
}
