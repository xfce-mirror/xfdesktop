/*
 *  xfdesktop
 *
 *  Copyright (c) 2008 Stephan Arts <stephan@xfce.org>
 *  Copyright (c) 2008 Brian Tarricone <brian@tarricone.org>
 *  Copyright (c) 2008 Jérôme Guelfucci <jerome.guelfucci@gmail.com>
 *  Copyright (c) 2011 Jannis Pohlmann <jannis@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>
#include <gtk/gtk.h>

#include "common/xfdesktop-common.h"
#include "xfdesktop-settings.h"

void
xfdesktop_menu_settings_init(XfdesktopSettings *settings) {
    GtkWidget *chk_show_desktop_menu = GTK_WIDGET(gtk_builder_get_object(settings->main_gxml, "chk_show_desktop_menu"));
    xfconf_g_property_bind(settings->channel, DESKTOP_MENU_SHOW_PROP, G_TYPE_BOOLEAN,
                           G_OBJECT(chk_show_desktop_menu), "active");
    GtkWidget *box_menu_subopts = GTK_WIDGET(gtk_builder_get_object(settings->main_gxml, "box_menu_subopts"));
    g_object_bind_property(chk_show_desktop_menu, "active", box_menu_subopts, "sensitive", G_BINDING_SYNC_CREATE);

    xfconf_g_property_bind(settings->channel, DESKTOP_MENU_SHOW_ICONS_PROP,
                           G_TYPE_BOOLEAN,
                           G_OBJECT(GTK_WIDGET(gtk_builder_get_object(settings->main_gxml,
                                                                      "chk_menu_show_app_icons"))),
                           "active");

    GtkWidget *chk_show_winlist_menu = GTK_WIDGET(gtk_builder_get_object(settings->main_gxml, "chk_show_winlist_menu"));
    xfconf_g_property_bind(settings->channel, WINLIST_SHOW_WINDOWS_MENU_PROP,
                           G_TYPE_BOOLEAN, G_OBJECT(chk_show_winlist_menu), "active");
    GtkWidget *box_winlist_subopts = GTK_WIDGET(gtk_builder_get_object(settings->main_gxml, "box_winlist_subopts"));
    g_object_bind_property(chk_show_winlist_menu, "active", box_winlist_subopts, "sensitive", G_BINDING_SYNC_CREATE);

    xfconf_g_property_bind(settings->channel, WINLIST_SHOW_APP_ICONS_PROP, G_TYPE_BOOLEAN,
                           gtk_builder_get_object(settings->main_gxml, "chk_winlist_show_app_icons"),
                           "active");

    xfconf_g_property_bind(settings->channel,
                           WINLIST_SHOW_ALL_WORKSPACES_PROP,
                           G_TYPE_BOOLEAN,
                           gtk_builder_get_object(settings->main_gxml, "chk_show_all_workspaces"),
                           "active");

    xfconf_g_property_bind(settings->channel, WINLIST_SHOW_STICKY_WIN_ONCE_PROP,
                           G_TYPE_BOOLEAN,
                           gtk_builder_get_object(settings->main_gxml, "chk_show_winlist_sticky_once"),
                           "active");

    xfconf_g_property_bind(settings->channel,
                           WINLIST_SHOW_URGENT_WINDOWS_SECTION_PROP,
                           G_TYPE_BOOLEAN,
                           gtk_builder_get_object(settings->main_gxml, "chk_show_urgent_windows_section"),
                           "active");

    xfconf_g_property_bind(settings->channel, WINLIST_SHOW_ADD_REMOVE_WORKSPACES_PROP,
                           G_TYPE_BOOLEAN,
                           gtk_builder_get_object(settings->main_gxml, "chk_show_add_remove_workspaces"),
                           "active");

    GtkWidget *chk_show_winlist_names = GTK_WIDGET(gtk_builder_get_object(settings->main_gxml, "chk_show_winlist_ws_names"));
    xfconf_g_property_bind(settings->channel, WINLIST_SHOW_WS_NAMES_PROP, G_TYPE_BOOLEAN,
                           G_OBJECT(chk_show_winlist_names), "active");
    GtkWidget *box_winlist_names_subopts = GTK_WIDGET(gtk_builder_get_object(settings->main_gxml, "box_winlist_names_subopts"));
    g_object_bind_property(chk_show_winlist_names,
                           "active",
                           box_winlist_names_subopts,
                           "sensitive",
                           G_BINDING_SYNC_CREATE);

    xfconf_g_property_bind(settings->channel, WINLIST_SHOW_WS_SUBMENUS_PROP,
                           G_TYPE_BOOLEAN,
                           gtk_builder_get_object(settings->main_gxml, "chk_show_winlist_ws_submenus"),
                           "active");
}
