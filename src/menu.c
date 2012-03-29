/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2004-2008 Brian J. Tarricone <bjt23@cornell.edu>
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <glib.h>
#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>

#include "menu.h"
#ifdef USE_DESKTOP_MENU
#include "xfce-desktop-menu.h"
#endif

#ifdef USE_DESKTOP_MENU
static XfceDesktopMenu *desktop_menu = NULL;
static gboolean show_desktop_menu_icons = TRUE;
#endif

#ifdef USE_DESKTOP_MENU
static void
_stop_menu_module(void) {
    if(desktop_menu) {
        xfce_desktop_menu_destroy(desktop_menu);
        desktop_menu = NULL;
    }
}

static gboolean
_start_menu_module(void)
{
    desktop_menu = xfce_desktop_menu_new(TRUE);
    if(desktop_menu) {
        xfce_desktop_menu_set_show_icons(desktop_menu, show_desktop_menu_icons);
        return TRUE;
    } else {
        g_warning("%s: Unable to initialise menu module. Right-click menu will be unavailable.\n", PACKAGE);
        return FALSE;
    }
}
#endif

#if USE_DESKTOP_MENU
static void
menu_populate(XfceDesktop *desktop,
              GtkMenuShell *menu,
              gpointer user_data)
{
    GtkWidget *desktop_menu_widget;
    GList *menu_children;
    
    TRACE("ENTERING");
    
    if(!desktop_menu)
        return;
    
    /* check to see if the menu is empty.  if not, add the desktop menu
     * to a submenu */
    menu_children = gtk_container_get_children(GTK_CONTAINER(menu));
    if(menu_children) {
        g_list_free(menu_children);
        
        desktop_menu_widget = xfce_desktop_menu_get_widget(desktop_menu);
        if(desktop_menu_widget) {
            GtkWidget *mi, *img = NULL;
            GtkIconTheme *itheme = gtk_icon_theme_get_default();
            
            mi = gtk_separator_menu_item_new();
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            
            if(gtk_icon_theme_has_icon(itheme, "applications-other")) {
                img = gtk_image_new_from_icon_name("applications-other",
                                                   GTK_ICON_SIZE_MENU);
                gtk_widget_show(img);
            }
            
            mi = gtk_image_menu_item_new_with_mnemonic(_("_Applications"));
            gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            
            gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi), desktop_menu_widget);
        }
    } else {
        /* just get the menu as a list of toplevel GtkMenuItems instead of
         * a toplevel menu */
        xfce_desktop_menu_populate_menu(desktop_menu, GTK_WIDGET(menu));
    }
}
#endif

#ifdef USE_DESKTOP_MENU
static void
menu_settings_changed(XfconfChannel *channel,
                      const gchar *property,
                      const GValue *value,
                      gpointer user_data)
{
    if(!strcmp(property, "/desktop-menu/show")) {
        if(!G_VALUE_TYPE(value) || g_value_get_boolean(value)) {
            if(!desktop_menu) {
                _start_menu_module();
                if(desktop_menu && !show_desktop_menu_icons)
                    xfce_desktop_menu_set_show_icons(desktop_menu, FALSE);
            }
        } else {
            if(desktop_menu)
                _stop_menu_module();
        }
    } else if(!strcmp(property, "/desktop-menu/show-icons")) {
        show_desktop_menu_icons = G_VALUE_TYPE(value)
                                  ? g_value_get_boolean(value)
                                  : TRUE;
        if(desktop_menu) {
            xfce_desktop_menu_set_show_icons(desktop_menu,
                                             show_desktop_menu_icons);
        }
    }
}
#endif

void
menu_init(XfconfChannel *channel)
{    
#ifdef USE_DESKTOP_MENU
    if(!channel
       || xfconf_channel_get_bool(channel, "/desktop-menu/show", TRUE))
    {
        if(channel) {
            show_desktop_menu_icons = xfconf_channel_get_bool(channel,
                                                              "/desktop-menu/show-icons",
                                                              TRUE);
        }
        _start_menu_module();
    } else
        _stop_menu_module();

    if(channel) {
        g_signal_connect(G_OBJECT(channel), "property-changed",
                         G_CALLBACK(menu_settings_changed), NULL);
    }
#endif
}

void
menu_attach(XfceDesktop *desktop)
{
#if USE_DESKTOP_MENU
    DBG("attached default menu");
    g_signal_connect_after(G_OBJECT(desktop), "populate-root-menu",
                           G_CALLBACK(menu_populate), NULL);
#endif
}

void
menu_reload(void)
{
#ifdef USE_DESKTOP_MENU
    if(desktop_menu)
        xfce_desktop_menu_force_regen(desktop_menu);
#endif
}

void
menu_cleanup(void)
{
#ifdef USE_DESKTOP_MENU
    _stop_menu_module();
#endif
}
