/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2004 Brian Tarricone, <bjt23@cornell.edu>
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
 *
 *  Random portions taken from or inspired by the original xfdesktop for xfce4:
 *     Copyright (C) 2002-2003 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *     Copyright (C) 2003 Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include "windowlist.h"
#include "xfdesktop-common.h"

#define WLIST_MAXLEN 30

static gboolean show_windowlist = TRUE;
static gboolean show_windowlist_icons = TRUE;

static void
set_num_workspaces(GtkWidget *w, gpointer num)
{
    static Atom xa_NET_NUMBER_OF_DESKTOPS = 0;
    XClientMessageEvent sev;
    gint n;
    GdkScreen *gscreen = gtk_widget_get_screen(w);
    GdkWindow *groot = gdk_screen_get_root_window(gscreen);

    if(!xa_NET_NUMBER_OF_DESKTOPS) {
        xa_NET_NUMBER_OF_DESKTOPS = XInternAtom(GDK_DISPLAY(),
                "_NET_NUMBER_OF_DESKTOPS", False);
    }

    n = GPOINTER_TO_INT(num);

    sev.type = ClientMessage;
    sev.display = GDK_DISPLAY();
    sev.format = 32;
    sev.window = GDK_WINDOW_XID(groot);
    sev.message_type = xa_NET_NUMBER_OF_DESKTOPS;
    sev.data.l[0] = n;

    gdk_error_trap_push();

    XSendEvent(GDK_DISPLAY(), GDK_WINDOW_XID(groot), False,
            SubstructureNotifyMask | SubstructureRedirectMask,
            (XEvent *)&sev);

    gdk_flush();
    gdk_error_trap_pop();
}

static void
activate_window(GtkWidget *w, gpointer user_data)
{
    NetkWindow *netk_window = user_data;
    
    netk_workspace_activate(netk_window_get_workspace(netk_window));
    netk_window_activate(netk_window);
}

static void
window_destroyed_cb(gpointer data, GObject *where_the_object_was)
{
    GtkWidget *mi = data;
    GtkWidget *menu = gtk_widget_get_parent(mi);
    
    if(mi && menu)
        gtk_container_remove(GTK_CONTAINER(menu), mi);
}

static void
mi_destroyed_cb(GtkObject *object, gpointer user_data)
{
    g_object_weak_unref(G_OBJECT(user_data),
            (GWeakNotify)window_destroyed_cb, object);
}

static GtkWidget *
menu_item_from_netk_window(NetkWindow *netk_window, gint icon_width,
        gint icon_height)
{
    GtkWidget *mi, *img = NULL;
    const gchar *title;
    NetkScreen *netk_screen;
    NetkWorkspace *netk_workspace, *active_workspace;
    gchar *label;
    GdkPixbuf *icon, *tmp;
    gint w, h;
    
    title = netk_window_get_name(netk_window);
    if(!title)
        return NULL;
    
    netk_screen = netk_window_get_screen(netk_window);
    active_workspace = netk_screen_get_active_workspace(netk_screen);
    netk_workspace = netk_window_get_workspace(netk_window);
    
    label = g_malloc0(WLIST_MAXLEN+13);
    
    if(netk_window_is_minimized(netk_window))
        g_strlcat(label, "[", WLIST_MAXLEN+13);
    g_strlcat(label, title, strlen(label)+WLIST_MAXLEN);
    if(strlen(title) > WLIST_MAXLEN)
        g_strlcat(label, "...", WLIST_MAXLEN+13);
    if(netk_window_is_minimized(netk_window))
        g_strlcat(label, "]", WLIST_MAXLEN+13);
    
    if(show_windowlist_icons) {
        icon = netk_window_get_icon(netk_window);
        w = gdk_pixbuf_get_width(icon);
        h = gdk_pixbuf_get_height(icon);
        if(w != icon_width || h != icon_height) {
            tmp = gdk_pixbuf_scale_simple(icon, icon_width, icon_height,
                    GDK_INTERP_BILINEAR);
            img = gtk_image_new_from_pixbuf(tmp);
            g_object_unref(G_OBJECT(tmp));
        } else
            img = gtk_image_new_from_pixbuf(icon);
    }
    
    if(img) {
        mi = gtk_image_menu_item_new_with_label(label);
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
    } else
        mi = gtk_menu_item_new_with_label(label);
    g_free(label);
    
    return mi;
}

static GtkWidget *
windowlist_create(GdkScreen *gscreen)
{
    GtkWidget *menu, *mi, *label, *img;
    GtkStyle *style;
    NetkScreen *netk_screen;
    gint nworkspaces, i;
    NetkWorkspace *active_workspace, *netk_workspace;
    gchar *ws_label, *rm_label;
    const gchar *ws_name = NULL;
    GList *windows, *l;
    NetkWindow *netk_window;
    gint w, h;
    PangoFontDescription *italic_font_desc = pango_font_description_from_string("italic");
    
    gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &w, &h);
    
    menu = gtk_menu_new();
    gtk_widget_show(menu);
    style = gtk_widget_get_style(menu);
    
    netk_screen = netk_screen_get(gdk_screen_get_number(gscreen));
    nworkspaces = netk_screen_get_workspace_count(netk_screen);
    active_workspace = netk_screen_get_active_workspace(netk_screen);
    
    for(i = 0; i < nworkspaces; i++) {
        netk_workspace = netk_screen_get_workspace(netk_screen, i);
        ws_name = netk_workspace_get_name(netk_workspace);
        
        if(netk_workspace == active_workspace) {
            if(!ws_name || atoi(ws_name) == i+1)
                ws_label = g_strdup_printf(_("<b>Workspace %d</b>"), i+1);
            else {
                gchar *ws_name_esc = g_markup_escape_text(ws_name, strlen(ws_name));
                ws_label = g_strdup_printf("<b>%s</b>", ws_name_esc);
                g_free(ws_name_esc);
            }
        } else {
            if(!ws_name || atoi(ws_name) == i+1)
                ws_label = g_strdup_printf(_("<i>Workspace %d</i>"), i+1);
            else {
                gchar *ws_name_esc = g_markup_escape_text(ws_name, strlen(ws_name));
                ws_label = g_strdup_printf("<i>%s</i>", ws_name_esc);
                g_free(ws_name_esc);
            }
        }
        mi = gtk_menu_item_new_with_label(ws_label);
        g_free(ws_label);
        label = gtk_bin_get_child(GTK_BIN(mi));
        gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        g_signal_connect_swapped(G_OBJECT(mi), "activate",
                G_CALLBACK(netk_workspace_activate), netk_workspace);
        
        mi = gtk_separator_menu_item_new();
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        
        windows = netk_screen_get_windows_stacked(netk_screen);
        for(l = windows; l; l = l->next) {
            netk_window = l->data;
            
            if((netk_window_get_workspace(netk_window) != netk_workspace
                        && !netk_window_is_sticky(netk_window))
                    || netk_window_is_skip_pager(netk_window)
                    || netk_window_is_skip_tasklist(netk_window))
            {
                /* the window isn't on the current WS AND isn't sticky,
                 * OR,
                 * the window is set to skip the pager,
                 * OR,
                 * the window is set to skip the tasklist
                 */
                continue;
            }
            
            mi = menu_item_from_netk_window(netk_window, w, h);
            if(!mi)
                continue;
            if(netk_workspace != active_workspace) {
                GtkWidget *lbl = gtk_bin_get_child(GTK_BIN(mi));
                gtk_widget_modify_fg(lbl, GTK_STATE_NORMAL,
                        &(style->fg[GTK_STATE_INSENSITIVE]));
                gtk_widget_modify_font(lbl, italic_font_desc);
            }
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            g_object_weak_ref(G_OBJECT(netk_window),
                    (GWeakNotify)window_destroyed_cb, mi);
            g_signal_connect(G_OBJECT(mi), "activate",
                    G_CALLBACK(activate_window), netk_window);
            g_signal_connect(G_OBJECT(mi), "destroy",
                    G_CALLBACK(mi_destroyed_cb), netk_window);
        }
        
        mi = gtk_separator_menu_item_new();
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    }
    
    pango_font_description_free(italic_font_desc);
    
    /* 'add workspace' item */
    if(show_windowlist_icons) {
        img = gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_MENU);
        mi = gtk_image_menu_item_new_with_mnemonic(_("_Add Workspace"));
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
    } else
        mi = gtk_menu_item_new_with_mnemonic(_("_Add Workspace"));
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    g_signal_connect(G_OBJECT(mi), "activate",
            G_CALLBACK(set_num_workspaces), GINT_TO_POINTER(nworkspaces+1));
    
    /* 'remove workspace' item */
    if(!ws_name || atoi(ws_name) == nworkspaces)
        rm_label = g_strdup_printf(_("_Remove Workspace %d"), nworkspaces);
    else {
        gchar *ws_name_esc = g_markup_escape_text(ws_name, strlen(ws_name));
        rm_label = g_strdup_printf(_("_Remove Workspace '%s'"), ws_name_esc);
        g_free(ws_name_esc);
    }
    if(show_windowlist_icons) {
        img = gtk_image_new_from_stock(GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU);
        mi = gtk_image_menu_item_new_with_mnemonic(rm_label);
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
    } else
        mi = gtk_menu_item_new_with_mnemonic(rm_label);
    g_free(rm_label);
    if(nworkspaces == 1)
        gtk_widget_set_sensitive(mi, FALSE);
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    g_signal_connect(G_OBJECT(mi), "activate",
            G_CALLBACK(set_num_workspaces), GINT_TO_POINTER(nworkspaces-1));
    
    return menu;
}

static gboolean
windowlist_deactivate_idled(gpointer user_data)
{
    gtk_widget_destroy(GTK_WIDGET(user_data));
    
    return FALSE;
}

void
popup_windowlist(GdkScreen *gscreen, gint button, guint32 time)
{
    GdkWindow *root;
    
    if(!show_windowlist)
        return;
    
    root = gdk_screen_get_root_window(gscreen);
    if (xfdesktop_popup_grab_available(root, time)) {
        GtkWidget *windowlist;

        windowlist = windowlist_create(gscreen);
        gtk_menu_set_screen(GTK_MENU(windowlist), gscreen);
        g_signal_connect_swapped(G_OBJECT(windowlist), "deactivate",
                G_CALLBACK(g_idle_add), (gpointer)windowlist_deactivate_idled);
        gtk_menu_popup(GTK_MENU(windowlist), NULL, NULL, NULL, NULL, button, time);
    }
    else
        g_critical("Unable to get keyboard/mouse grab. Unable to popup windowlist");
}

void
windowlist_init(McsClient *mcs_client)
{
    McsSetting *setting = NULL;
    
    if(mcs_client) {
        if(MCS_SUCCESS == mcs_client_get_setting(mcs_client, "showwl",
                BACKDROP_CHANNEL, &setting))
        {
            show_windowlist = setting->data.v_int;
            mcs_setting_free(setting);
            setting = NULL;
        }
        
        if(MCS_SUCCESS == mcs_client_get_setting(mcs_client, "showwli",
                BACKDROP_CHANNEL, &setting))
        {
            show_windowlist_icons = setting->data.v_int;
            mcs_setting_free(setting);
            setting = NULL;
        }
    }
}

gboolean
windowlist_settings_changed(McsClient *client, McsAction action,
        McsSetting *setting, gpointer user_data)
{
    switch(action) {
        case MCS_ACTION_NEW:
        case MCS_ACTION_CHANGED:
            if(!strcmp(setting->name, "showwl")) {
                show_windowlist = setting->data.v_int;
                return TRUE;
            } else if(!strcmp(setting->name, "showwli")) {
                show_windowlist_icons = setting->data.v_int;
                return TRUE;
            }
            break;
        
        case MCS_ACTION_DELETED:
            break;
    }
    
    return FALSE;
}

void
windowlist_cleanup()
{
    /* notused */
}
