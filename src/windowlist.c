/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2004-2008 Brian Tarricone <brian@tarricone.org>
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
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4windowing/libxfce4windowing.h>
#include <libxfce4windowingui/libxfce4windowingui.h>

#include "common/xfdesktop-common.h"
#include "windowlist.h"

static XfconfChannel *wl_channel = NULL;
static gboolean show_windowlist = TRUE;

static const struct {
    const gchar *setting;
    GType setting_type;
    const gchar *property;
} setting_bindings[] = {
    { WINLIST_SHOW_APP_ICONS_PROP, G_TYPE_BOOLEAN, "show-icons" },
    { WINLIST_SHOW_WS_NAMES_PROP, G_TYPE_BOOLEAN, "show-workspace-names" },
    { WINLIST_SHOW_WS_SUBMENUS_PROP, G_TYPE_BOOLEAN, "show-workspace-submenus" },
    { WINLIST_SHOW_STICKY_WIN_ONCE_PROP, G_TYPE_BOOLEAN, "show-sticky-windows-once" },
    { WINLIST_SHOW_ADD_REMOVE_WORKSPACES_PROP, G_TYPE_BOOLEAN, "show-workspace-actions" },
    { WINLIST_SHOW_URGENT_WINDOWS_SECTION_PROP, G_TYPE_BOOLEAN, "show-urgent-windows-section" },
    { WINLIST_SHOW_ALL_WORKSPACES_PROP, G_TYPE_BOOLEAN, "show-all-workspaces" },
};

GtkMenu *
windowlist_populate(GtkMenu *menu, gint scale_factor)
{
    if (show_windowlist) {
        XfwScreen *screen = xfw_screen_get_default();

        // If we were given a menu to populate, add the windowlist to a submenu
        GtkWidget *top_menu;
        GtkWidget *list_menu;
        if (menu != NULL) {
            GtkWidget *mi;

            list_menu = xfw_window_list_menu_new(screen);
            gtk_menu_set_screen(GTK_MENU(list_menu), gtk_widget_get_screen(GTK_WIDGET(menu)));

            mi = gtk_separator_menu_item_new();
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);

            mi = gtk_menu_item_new_with_label(_("Window List"));
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);

            gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi), list_menu);

            top_menu = GTK_WIDGET(menu);
        } else {
            top_menu = list_menu = xfw_window_list_menu_new(screen);
            gtk_menu_set_screen(GTK_MENU(list_menu), gdk_screen_get_default());
        }

        if (wl_channel != NULL) {
            for (gsize i = 0; i < G_N_ELEMENTS(setting_bindings); ++i) {
                xfconf_g_property_bind(wl_channel,
                                       setting_bindings[i].setting,
                                       setting_bindings[i].setting_type,
                                       list_menu,
                                       setting_bindings[i].property);
            }
        }

        g_object_unref(screen);

        return GTK_MENU(top_menu);
    } else {
        return menu;
    }
}

static void
windowlist_show_setting_changed(XfconfChannel *channel, const gchar *property, const GValue *value) {
    show_windowlist = G_VALUE_HOLDS_BOOLEAN(value) ? g_value_get_boolean(value) : TRUE;
}

void
windowlist_init(XfconfChannel *channel)
{
    if (channel != NULL) {
        wl_channel = g_object_ref(channel);
        show_windowlist = xfconf_channel_get_bool(channel, WINLIST_SHOW_WINDOWS_MENU_PROP, TRUE);
        g_signal_connect(G_OBJECT(channel), "property-changed::" WINLIST_SHOW_WINDOWS_MENU_PROP,
                         G_CALLBACK(windowlist_show_setting_changed), NULL);
    }
}

void
windowlist_cleanup(XfconfChannel *channel)
{
    if (wl_channel != NULL && wl_channel == channel) {
        g_signal_handlers_disconnect_by_func(channel,
                                             G_CALLBACK(windowlist_show_setting_changed),
                                             NULL);
        g_clear_object(&wl_channel);
    }
}
