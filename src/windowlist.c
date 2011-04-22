/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2004-2008 Brian Tarricone <bjt23@cornell.edu>
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

#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <libwnck/libwnck.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include "windowlist.h"
#include "xfdesktop-common.h"

#define WLIST_MAXLEN 30

static gboolean show_windowlist = TRUE;
static gboolean wl_show_icons = TRUE;
static gboolean wl_show_ws_names = TRUE;
static gboolean wl_submenus = FALSE;
static gboolean wl_sticky_once = FALSE;

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
    WnckWindow *wnck_window = user_data;
    
    if(!wnck_window_is_sticky(wnck_window)) {
        wnck_workspace_activate(wnck_window_get_workspace(wnck_window),
                                gtk_get_current_event_time());
    }
    wnck_window_activate(wnck_window, gtk_get_current_event_time());
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

static void
menulist_set_label_flags(GtkWidget *widget, gpointer data)
{
    gboolean *done = data;
    if(*done)
        return;
    if(GTK_IS_LABEL(widget)) {
        GtkLabel *label = GTK_LABEL(widget);
        gtk_label_set_use_markup(label, TRUE);
        gtk_label_set_ellipsize(label, PANGO_ELLIPSIZE_MIDDLE);
        gtk_label_set_max_width_chars(label, 24);
        *done = TRUE;
    }
    else if(GTK_IS_CONTAINER (widget))
        gtk_container_forall(GTK_CONTAINER (widget), menulist_set_label_flags,
                              &done);
}

static GtkWidget *
menu_item_from_wnck_window(WnckWindow *wnck_window, gint icon_width,
        gint icon_height)
{
    GtkWidget *mi, *img = NULL;
    gchar *title;
    GString *label;
    GdkPixbuf *icon, *tmp;
    gint w, h;
    gboolean truncated = FALSE;

    title = g_markup_escape_text(wnck_window_get_name(wnck_window), -1);
    if(!title)
        return NULL;

    label = g_string_new(title);
    g_free(title);

    if(wnck_window_is_active(wnck_window)) {
        g_string_prepend(label, "<i>");
        g_string_append(label, "</i>");
    }
    if(wnck_window_is_minimized(wnck_window)) {
        g_string_prepend(label, "[");
        g_string_append(label, "]");
    }
    
    if(wl_show_icons) {
        icon = wnck_window_get_icon(wnck_window);
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
        mi = gtk_image_menu_item_new_with_label(label->str);
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
    } else
        mi = gtk_menu_item_new_with_label(label->str);

    g_string_free(label, TRUE);

    gtk_container_forall(GTK_CONTAINER(mi), menulist_set_label_flags,
                         &truncated);
    
    return mi;
}

static void
windowlist_populate(XfceDesktop *desktop,
                    GtkMenuShell *menu,
                    gpointer user_data)
{
    GtkWidget *submenu, *mi, *label, *img;
    GdkScreen *gscreen;
    GList *menu_children;
    GtkStyle *style;
    WnckScreen *wnck_screen;
    gint nworkspaces, i;
    WnckWorkspace *active_workspace, *wnck_workspace;
    gchar *ws_label, *rm_label;
    const gchar *ws_name = NULL;
    GList *windows, *l;
    WnckWindow *wnck_window;
    gint w, h;
    PangoFontDescription *italic_font_desc;
    gboolean is_empty_workspace;
    
    if(!show_windowlist)
        return;
    
    italic_font_desc = pango_font_description_from_string("italic");
    
    if(gtk_widget_has_screen(GTK_WIDGET(menu)))
        gscreen = gtk_widget_get_screen(GTK_WIDGET(menu));
    else
        gscreen = gdk_display_get_default_screen(gdk_display_get_default());
    
    /* check to see if the menu is empty.  if not, add the windowlist to a
     * submenu */
    menu_children = gtk_container_get_children(GTK_CONTAINER(menu));
    if(menu_children) {
        GtkWidget *tmpmenu = gtk_menu_new();
        gtk_menu_set_screen(GTK_MENU(tmpmenu), gscreen);
        
        mi = gtk_separator_menu_item_new();
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        
        mi = gtk_menu_item_new_with_label(_("Window List"));
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi), tmpmenu);
        menu = (GtkMenuShell *)tmpmenu;
        g_list_free(menu_children);
    }
    
    gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &w, &h);
    style = gtk_widget_get_style(GTK_WIDGET(menu));
    
    wnck_screen = wnck_screen_get(gdk_screen_get_number(gscreen));
    wnck_screen_force_update (wnck_screen);
    nworkspaces = wnck_screen_get_workspace_count(wnck_screen);
    active_workspace = wnck_screen_get_active_workspace(wnck_screen);
    
    for(i = 0; i < nworkspaces; i++) {
        wnck_workspace = wnck_screen_get_workspace(wnck_screen, i);
        submenu = (GtkWidget *)menu;
        
        if(wl_show_ws_names || wl_submenus) {
            ws_name = wnck_workspace_get_name(wnck_workspace);
            
            if(wnck_workspace == active_workspace) {
                if(ws_name == NULL || *ws_name == '\0')
                    ws_label = g_strdup_printf(_("<b>Workspace %d</b>"), i+1);
                else {
                    gchar *ws_name_esc = g_markup_escape_text(ws_name, strlen(ws_name));
                    ws_label = g_strdup_printf("<b>%s</b>", ws_name_esc);
                    g_free(ws_name_esc);
                }
            } else {  /* don't italicise if we're showing stuff in submenus */
                if(ws_name == NULL || *ws_name == '\0') {
                    if(wl_submenus)
                        ws_label = g_strdup_printf(_("Workspace %d"), i+1);
                    else
                        ws_label = g_strdup_printf(_("<i>Workspace %d</i>"), i+1);
                } else {
                    gchar *ws_name_esc = g_markup_escape_text(ws_name, strlen(ws_name));
                    if(wl_submenus)
                        ws_label = ws_name_esc;
                    else {
                        ws_label = g_strdup_printf("<i>%s</i>", ws_name_esc);
                        g_free(ws_name_esc);
                    }
                }
            }
            
            mi = gtk_menu_item_new_with_label(ws_label);
            g_free(ws_label);
            label = gtk_bin_get_child(GTK_BIN(mi));
            gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            if(!wl_submenus) {
                g_signal_connect_swapped(G_OBJECT(mi), "activate",
                                         G_CALLBACK(wnck_workspace_activate),
                                         wnck_workspace);
            }
            
            if(wl_submenus) {
                submenu = gtk_menu_new();
                gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi), submenu);
            } else {
                mi = gtk_separator_menu_item_new();
                gtk_widget_show(mi);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            }
        }
        
        windows = wnck_screen_get_windows_stacked(wnck_screen);
        is_empty_workspace = TRUE;
        for(l = windows; l; l = l->next) {
            wnck_window = l->data;
            
            if((wnck_window_get_workspace(wnck_window) != wnck_workspace
                        && (!wnck_window_is_sticky(wnck_window)
                            || (wl_sticky_once
                                && wnck_workspace != active_workspace)))
                    || wnck_window_is_skip_pager(wnck_window)
                    || wnck_window_is_skip_tasklist(wnck_window))
            {
                /* the window isn't on the current WS AND isn't sticky,
                 * OR,
                 * the window is set to skip the pager,
                 * OR,
                 * the window is set to skip the tasklist
                 */
                continue;
            }
            
            mi = menu_item_from_wnck_window(wnck_window, w, h);
            if(!mi)
                continue;
            is_empty_workspace = FALSE;
            if(wnck_workspace != active_workspace
                    && (!wnck_window_is_sticky(wnck_window)
                        || wnck_workspace != active_workspace))
            {
                GtkWidget *lbl = gtk_bin_get_child(GTK_BIN(mi));
                gtk_widget_modify_fg(lbl, GTK_STATE_NORMAL,
                        &(style->fg[GTK_STATE_INSENSITIVE]));
                gtk_widget_modify_font(lbl, italic_font_desc);
            }
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(submenu), mi);
            g_object_weak_ref(G_OBJECT(wnck_window),
                    (GWeakNotify)window_destroyed_cb, mi);
            g_signal_connect(G_OBJECT(mi), "activate",
                    G_CALLBACK(activate_window), wnck_window);
            g_signal_connect(G_OBJECT(mi), "destroy",
                    G_CALLBACK(mi_destroyed_cb), wnck_window);
        }
        
        if(!wl_submenus && !is_empty_workspace) {
            mi = gtk_separator_menu_item_new();
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(submenu), mi);
        }
    }
    
    pango_font_description_free(italic_font_desc);
    
    if(wl_submenus) {
        mi = gtk_separator_menu_item_new();
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    }
    
    /* 'add workspace' item */
    if(wl_show_icons) {
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
    if(wl_show_icons) {
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
}

static void
windowlist_settings_changed(XfconfChannel *channel,
                            const gchar *property,
                            const GValue *value,
                            gpointer user_data)
{
    if(!strcmp(property, "/windowlist-menu/show"))
        show_windowlist = G_VALUE_TYPE(value) ? g_value_get_boolean(value) : TRUE;
    else if(!strcmp(property, "/windowlist-menu/show-icons"))
        wl_show_icons = G_VALUE_TYPE(value) ? g_value_get_boolean(value) : TRUE;
    else if(!strcmp(property, "/windowlist-menu/show-workspace-names"))
        wl_show_ws_names = G_VALUE_TYPE(value) ? g_value_get_boolean(value) : TRUE;
    else if(!strcmp(property, "/windowlist-menu/show-submenus"))
        wl_submenus = G_VALUE_TYPE(value) ? g_value_get_boolean(value) : FALSE;
    else if(!strcmp(property, "/windowlist-menu/show-sticky-once"))
        wl_sticky_once = G_VALUE_TYPE(value) ? g_value_get_boolean(value) : FALSE;
}

void
windowlist_init(XfconfChannel *channel)
{
    if(channel) {
        show_windowlist = xfconf_channel_get_bool(channel,
                                                  "/windowlist-menu/show",
                                                  TRUE);
        
        wl_show_icons = xfconf_channel_get_bool(channel,
                                                "/windowlist-menu/show-icons",
                                                TRUE);

        wl_show_ws_names = xfconf_channel_get_bool(channel,
                                                   "/windowlist-menu/show-workspace-names",
                                                   TRUE);
        
        wl_submenus = xfconf_channel_get_bool(channel,
                                              "/windowlist-menu/show-submenus",
                                              FALSE);

        wl_sticky_once = xfconf_channel_get_bool(channel,
                                                 "/windowlist-menu/show-sticky-once",
                                                 FALSE);

        g_signal_connect(G_OBJECT(channel), "property-changed",
                         G_CALLBACK(windowlist_settings_changed), NULL);
    }
}

void
windowlist_attach(XfceDesktop *desktop)
{
    g_signal_connect_after(G_OBJECT(desktop), "populate-secondary-root-menu",
                           G_CALLBACK(windowlist_populate), NULL);
}

void
windowlist_cleanup(void)
{
    /* notused */
}
