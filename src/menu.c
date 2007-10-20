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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <glib.h>
#include <gmodule.h>
#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>

#include "menu.h"
#ifdef USE_DESKTOP_MENU
#include "desktop-menu-stub.h"
#include "xfdesktop-common.h"
#endif

#ifdef USE_DESKTOP_MENU
static XfceDesktopMenu *desktop_menu = NULL;
static gboolean show_desktop_menu_icons = TRUE;
#endif

#ifdef USE_DESKTOP_MENU
static void
_stop_menu_module() {
    if(desktop_menu) {
        xfce_desktop_menu_stop_autoregen(desktop_menu);
        xfce_desktop_menu_destroy(desktop_menu);
        desktop_menu = NULL;
    }
}

static gboolean
_start_menu_module()
{
    desktop_menu = xfce_desktop_menu_new(NULL, TRUE);
    if(desktop_menu) {
        xfce_desktop_menu_set_show_icons(desktop_menu, show_desktop_menu_icons);
        xfce_desktop_menu_start_autoregen(desktop_menu, 10);
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
    
    if(!desktop_menu)
        return;
    
    if(xfce_desktop_menu_need_update(desktop_menu))
        xfce_desktop_menu_force_regen(desktop_menu);
    
    /* check to see if the menu is empty.  if not, add the desktop menu
     * to a submenu */
    menu_children = gtk_container_get_children(GTK_CONTAINER(menu));
    if(menu_children) {
        GtkWidget *mi;
        
        g_list_free(menu_children);
        
        desktop_menu_widget = xfce_desktop_menu_get_widget(desktop_menu);
        if(desktop_menu_widget) {
            mi = gtk_separator_menu_item_new();
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            
            mi = gtk_menu_item_new_with_label(_("Applications"));
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            
            gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi), desktop_menu_widget);
            gtk_widget_show(desktop_menu_widget);
        }
    } else {
        /* just get the menu as a list of toplevel GtkMenuItems instead of
         * a toplevel menu */
        xfce_desktop_menu_populate_menu(desktop_menu, GTK_WIDGET(menu));
    }
}
#endif

void
menu_init(McsClient *mcs_client)
{    
#ifdef USE_DESKTOP_MENU
    McsSetting *setting = NULL;
    
    if(!mcs_client)
        _start_menu_module();
    else {
        if(MCS_SUCCESS == mcs_client_get_setting(mcs_client, "showdm",
                BACKDROP_CHANNEL, &setting))
        {
            if(setting->data.v_int)
                _start_menu_module();
            else
                _stop_menu_module();
            mcs_setting_free(setting);
            setting = NULL;
        } else
            _start_menu_module();
    }
#endif
}

void
menu_attach(XfceDesktop *desktop)
{
#if USE_DESKTOP_MENU
    g_signal_connect_after(G_OBJECT(desktop), "populate-root-menu",
                           G_CALLBACK(menu_populate), NULL);
#endif
}

gboolean
menu_settings_changed(McsClient *client, McsAction action, McsSetting *setting,
        gpointer user_data)
{
#ifdef USE_DESKTOP_MENU
    McsSetting *setting1 = NULL;
    
    switch(action) {
        case MCS_ACTION_NEW:
        case MCS_ACTION_CHANGED:
            if(!strcmp(setting->name, "showdm")) {
                if(setting->data.v_int && !desktop_menu) {
                    _start_menu_module();
                    if(desktop_menu
                            && MCS_SUCCESS == mcs_client_get_setting(client,
                                "showdmi", BACKDROP_CHANNEL, &setting1))
                    {
                        if(!setting1->data.v_int)
                            xfce_desktop_menu_set_show_icons(desktop_menu, FALSE);
                        mcs_setting_free(setting1);
                        setting1 = NULL;
                    }
                } else if(!setting->data.v_int && desktop_menu)
                    _stop_menu_module();
                return TRUE;
            }
            break;
        
        case MCS_ACTION_DELETED:
            break;
    }
#endif
    
    return FALSE;    
}

void
menu_set_show_icons(gboolean show_icons)
{
#ifdef USE_DESKTOP_MENU
    show_desktop_menu_icons = show_icons;
    if(desktop_menu)
        xfce_desktop_menu_set_show_icons(desktop_menu, show_icons);
#endif
}

void
menu_reload()
{
#ifdef USE_DESKTOP_MENU
    if(desktop_menu)
        xfce_desktop_menu_force_regen(desktop_menu);
#endif
}

void
menu_cleanup()
{
#ifdef USE_DESKTOP_MENU
    _stop_menu_module();
#endif
}
