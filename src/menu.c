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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <glib.h>
#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>

#include "xfdesktop-common.h"
#include "menu.h"
#ifdef USE_DESKTOP_MENU
#include <garcon/garcon.h>
#include <garcon-gtk/garcon-gtk.h>
#endif

#ifdef USE_DESKTOP_MENU
static gboolean show_desktop_menu = TRUE;
static gboolean show_desktop_menu_icons = TRUE;
static GarconMenu *garcon_menu = NULL;
#endif

#ifdef USE_DESKTOP_MENU
static void
menu_populate(XfceDesktop *desktop,
              GtkMenuShell *menu,
              gpointer user_data)
{
    GtkWidget *mi, *img = NULL;
    GtkIconTheme *itheme = gtk_icon_theme_get_default();
    GtkWidget *desktop_menu = NULL;

    TRACE("ENTERING");

    if(!show_desktop_menu)
        return;

    if(garcon_menu == NULL) {
        garcon_menu = garcon_menu_new_applications();
    }
    desktop_menu = garcon_gtk_menu_new (garcon_menu);

    mi = gtk_separator_menu_item_new();
    gtk_widget_show(mi);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    
    if(gtk_icon_theme_has_icon(itheme, "applications-other")) {
        img = gtk_image_new_from_icon_name("applications-other",
                                           GTK_ICON_SIZE_MENU);
        gtk_widget_show(img);
    }

    mi = xfdesktop_menu_create_menu_item_with_mnemonic(_("_Applications"), img);
    gtk_widget_show(mi);

    XF_DEBUG("show desktop menu icons %s", show_desktop_menu_icons ? "TRUE" : "FALSE");
    garcon_gtk_menu_set_show_menu_icons(GARCON_GTK_MENU(desktop_menu), show_desktop_menu_icons);

    gtk_menu_item_set_submenu (GTK_MENU_ITEM(mi), desktop_menu);

    gtk_menu_shell_append(menu, mi);
}
#endif /* USE_DESKTOP_MENU */

#ifdef USE_DESKTOP_MENU
static void
menu_settings_changed(XfconfChannel *channel,
                      const gchar *property,
                      const GValue *value,
                      gpointer user_data)
{
    if(!strcmp(property, "/desktop-menu/show")) {
        show_desktop_menu = G_VALUE_TYPE(value)
                            ? g_value_get_boolean(value)
                            : TRUE;
    } else if(!strcmp(property, "/desktop-menu/show-icons")) {
        show_desktop_menu_icons = G_VALUE_TYPE(value)
                                  ? g_value_get_boolean(value)
                                  : TRUE;
    }
}
#endif

void
menu_init(XfconfChannel *channel)
{    
#ifdef USE_DESKTOP_MENU
    if(!channel || xfconf_channel_get_bool(channel, "/desktop-menu/show", TRUE))
    {
        show_desktop_menu = TRUE;
        if(channel) {
            show_desktop_menu_icons = xfconf_channel_get_bool(channel,
                                                              "/desktop-menu/show-icons",
                                                              TRUE);
        }
    } else {
        show_desktop_menu = FALSE;
    }

    if(channel) {
        g_signal_connect(G_OBJECT(channel), "property-changed",
                         G_CALLBACK(menu_settings_changed), NULL);
    }
#endif
}

void
menu_attach(XfceDesktop *desktop)
{
#ifdef USE_DESKTOP_MENU
    DBG("attached default menu");
    g_signal_connect_after(G_OBJECT(desktop), "populate-root-menu",
                           G_CALLBACK(menu_populate), NULL);
#endif /* USE_DESKTOP_MENU */
}

void
menu_cleanup(void)
{
}
