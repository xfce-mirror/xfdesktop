/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2004-2008 Brian J. Tarricone <brian@tarricone.org>
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
#include <libxfce4ui/libxfce4ui.h>

#include "xfdesktop-common.h"
#include "menu.h"
#ifdef ENABLE_DESKTOP_MENU
#include <garcon/garcon.h>
#include <garcon-gtk/garcon-gtk.h>
#endif

#ifdef ENABLE_DESKTOP_MENU
static gboolean inited = FALSE;
static gboolean show_desktop_menu = TRUE;
static gboolean show_desktop_menu_icons = TRUE;
static GarconMenu *garcon_menu = NULL;
#endif

GtkMenu *
menu_populate(GtkMenu *menu, gint scale_factor)
{
#ifdef ENABLE_DESKTOP_MENU
    GtkWidget *desktop_menu = NULL;

    TRACE("ENTERING");

    if(!show_desktop_menu)
        return menu;

    /* init garcon environment */
    garcon_set_environment_xdg(GARCON_ENVIRONMENT_XFCE);

    if(garcon_menu == NULL) {
        garcon_menu = garcon_menu_new_applications();
    }
    desktop_menu = g_object_new(GARCON_GTK_TYPE_MENU,
                                "show-menu-icons", show_desktop_menu_icons,
                                "menu", garcon_menu,
                                NULL);
    XF_DEBUG("show desktop menu icons %s", show_desktop_menu_icons ? "TRUE" : "FALSE");

    // If we were provided a menu to populate, add the apps menu to a submenu
    if (menu != NULL) {
        GtkIconTheme *itheme = gtk_icon_theme_get_default();
        GtkWidget *mi;

        mi = gtk_separator_menu_item_new();
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        mi = gtk_image_menu_item_new_with_mnemonic(_("_Applications"));
G_GNUC_END_IGNORE_DEPRECATIONS
        if (gtk_icon_theme_has_icon(itheme, "applications-other")) {
            GtkWidget *img = gtk_image_new_from_icon_name("applications-other",
                                                          GTK_ICON_SIZE_MENU);
            gtk_widget_show(img);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
            gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
G_GNUC_END_IGNORE_DEPRECATIONS
        }
        gtk_menu_item_set_submenu (GTK_MENU_ITEM(mi), desktop_menu);
        gtk_widget_show(mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);

        return menu;
    } else {
        return GTK_MENU(desktop_menu);
    }
#else  /* !ENABLE_DESKTOP_MENU */
    return menu;
#endif /* ENABLE_DESKTOP_MENU */
}

#ifdef ENABLE_DESKTOP_MENU
static void
menu_settings_changed(XfconfChannel *channel,
                      const gchar *property,
                      const GValue *value,
                      gpointer user_data)
{
    if(!strcmp(property, DESKTOP_MENU_SHOW_PROP)) {
        show_desktop_menu = G_VALUE_TYPE(value)
                            ? g_value_get_boolean(value)
                            : TRUE;
        if (!show_desktop_menu) {
            g_clear_object(&garcon_menu);
        }
    } else if(!strcmp(property, DESKTOP_MENU_SHOW_ICONS_PROP)) {
        show_desktop_menu_icons = G_VALUE_TYPE(value)
                                  ? g_value_get_boolean(value)
                                  : TRUE;
    }
}
#endif

void
menu_init(XfconfChannel *channel)
{
#ifdef ENABLE_DESKTOP_MENU
    g_return_if_fail(!inited);

    if(!channel || xfconf_channel_get_bool(channel, DESKTOP_MENU_SHOW_PROP, TRUE)) {
        show_desktop_menu = TRUE;
        if(channel) {
            show_desktop_menu_icons = xfconf_channel_get_bool(channel,
                                                              DESKTOP_MENU_SHOW_ICONS_PROP,
                                                              TRUE);
        }
    } else {
        show_desktop_menu = FALSE;
    }

    if(channel) {
        g_signal_connect(G_OBJECT(channel), "property-changed",
                         G_CALLBACK(menu_settings_changed), NULL);
    }

    inited = TRUE;
#endif
}

void
menu_cleanup(XfconfChannel *channel)
{
#ifdef ENABLE_DESKTOP_MENU
    if (inited) {
        g_signal_handlers_disconnect_by_func(channel,
                                             G_CALLBACK(menu_settings_changed),
                                             NULL);

        g_clear_object(&garcon_menu);

        inited = FALSE;
    }
#endif
}
