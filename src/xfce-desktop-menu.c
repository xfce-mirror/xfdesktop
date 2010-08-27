/*  xfce4
 *  
 *  Copyright (C) 2002-2003 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *                     2003 Biju Chacko (botsie@users.sourceforge.net)
 *                     2004 Danny Milosavljevic <danny.milo@gmx.net>
 *                2004-2008 Brian Tarricone <bjt23@cornell.edu>
 *                     2009 Jannis Pohlmann <jannis@xfce.org>
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <glib.h>

#include <garcon/garcon.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include "xfce-desktop-menu.h"
#include "xfdesktop-app-menu-item.h"

struct _XfceDesktopMenu
{
    GarconMenu *garcon_menu;

    gboolean cache_menu_items;
    GList *menu_item_cache;
	
    gchar *filename;  /* file the menu is currently using */
    
    gboolean use_menu_icons;  /* show menu icons? */
	
    gint idle_id;  /* source id for idled generation */
};


static void _xfce_desktop_menu_free_menudata(XfceDesktopMenu *desktop_menu);
static GtkIconTheme *_deskmenu_icon_theme = NULL;

static gboolean _generate_menu_idled(gpointer data);
static gboolean _generate_menu(XfceDesktopMenu *desktop_menu);
static void desktop_menu_add_items(XfceDesktopMenu *desktop_menu,
                                   GarconMenu *garcon_menu,
                                   GtkWidget *menu,
                                   GList **menu_items_return);

static void
itheme_changed_cb(GtkIconTheme *itheme, gpointer user_data)
{
    XfceDesktopMenu *desktop_menu = user_data;

    /* this fixes bugs 3615 and 4342.  if both the .desktop files
     * and icon theme change very quickly after each other, we'll
     * get a crash when the icon theme gets regenerated when calling
     * gtk_icon_theme_lookup_icon(), which triggers a recursive regen.
     * so we'll idle the regen, and make sure we don't enter it
     * recursively.  same deal for _something_changed(). */

    if(!desktop_menu->idle_id)
        desktop_menu->idle_id = g_idle_add(_generate_menu_idled, desktop_menu);
}

/*
 * this is a bit of a kludge.  in order to support the cache and be a bit
 * faster, we either want to build a GtkMenu, or we want to just build
 * the GtkMenuItems and store them in a GList for later.  only one
 * of |menu| or |menu_items_return| should be non-NULL
 */
static void
desktop_menu_add_items(XfceDesktopMenu *desktop_menu,
                       GarconMenu *garcon_menu,
                       GtkWidget *menu,
                       GList **menu_items_return)
{
    GList *items, *l;
    GtkWidget *submenu, *mi, *img;
    GarconMenu *garcon_submenu;
    GarconMenuDirectory *garcon_directory;
    GarconMenuItem *garcon_item;
    const gchar *name, *icon_name;

    g_return_if_fail((menu && !menu_items_return)
                     || (!menu && menu_items_return));
    
    items = garcon_menu_get_elements(garcon_menu);
    for(l = items; l; l = l->next) {
        if(!garcon_menu_element_get_visible(l->data))
            continue;

        if(GARCON_IS_MENU(l->data)) {
            GList *tmpl;
            garcon_submenu = l->data;
            garcon_directory = garcon_menu_get_directory(garcon_submenu);
            icon_name = NULL;
            
            submenu = gtk_menu_new();
            gtk_widget_show(submenu);
            
            if(garcon_directory) {
                if(desktop_menu->use_menu_icons)
                    icon_name = garcon_menu_directory_get_icon_name(garcon_directory);
            }
            
            name = garcon_menu_element_get_name(GARCON_MENU_ELEMENT(garcon_submenu));

            mi = gtk_image_menu_item_new_with_label(name);
            if(icon_name) {
                img = gtk_image_new_from_icon_name(icon_name,
                                                   GTK_ICON_SIZE_MENU);
                gtk_widget_show(img);
                gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
            }
            gtk_widget_show(mi);
            gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi), submenu);

            if(menu)
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            else
                *menu_items_return = g_list_prepend(*menu_items_return, mi);
            
            desktop_menu_add_items(desktop_menu, garcon_submenu,
                                   submenu, NULL);
            
            /* we have to check emptiness down here instead of at the top of the
             * loop because there may be further submenus that are empty */
            if(!(tmpl = gtk_container_get_children(GTK_CONTAINER(submenu))))
                gtk_widget_destroy(mi);
            else
                g_list_free(tmpl);
        } else if(GARCON_IS_MENU_SEPARATOR(l->data)) {
            mi = gtk_separator_menu_item_new();
            gtk_widget_show(mi);

            if(menu)
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            else
                *menu_items_return = g_list_prepend(*menu_items_return, mi);
        } else if(GARCON_IS_MENU_ITEM(l->data)) {
            garcon_item = l->data;
            
            mi = xfdesktop_app_menu_item_new_full(garcon_menu_element_get_name(GARCON_MENU_ELEMENT(garcon_item)),
                                                  garcon_menu_item_get_command(garcon_item),
                                                  desktop_menu->use_menu_icons
                                                    ? garcon_menu_item_get_icon_name(garcon_item)
                                                    : NULL,
                                                  garcon_menu_item_requires_terminal(garcon_item),
                                                  garcon_menu_item_supports_startup_notification(garcon_item));
            gtk_widget_show(mi);

            if(menu)
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            else
                *menu_items_return = g_list_prepend(*menu_items_return, mi);
        }
    }
    g_list_free(items);

    if(menu_items_return)
        *menu_items_return = g_list_reverse(*menu_items_return);
}

static gboolean
_generate_menu(XfceDesktopMenu *desktop_menu)
{
    gboolean ret = TRUE;
    XfceKiosk *kiosk;
    gboolean user_menu;
    GError *error = NULL;
    
    _xfce_desktop_menu_free_menudata(desktop_menu);

    if(!desktop_menu->filename) {
        g_critical("%s: can't load menu: no menu file found", PACKAGE);
        return FALSE;
    }

    kiosk = xfce_kiosk_new("xfdesktop");
    user_menu = xfce_kiosk_query(kiosk, "UserMenu");
    xfce_kiosk_free(kiosk);
    
    DBG("menu file name is %s", desktop_menu->filename);

    desktop_menu->garcon_menu = garcon_menu_new_for_path(desktop_menu->filename);

    if(!garcon_menu_load (desktop_menu->garcon_menu, NULL, &error)) {
        g_critical("Unable to create GarconMenu from file '%s': %s",
                   desktop_menu->filename, error->message);
        g_error_free(error);
        _xfce_desktop_menu_free_menudata(desktop_menu);
        return FALSE;
    }

    if(desktop_menu->cache_menu_items) {
        desktop_menu_add_items(desktop_menu, desktop_menu->garcon_menu,
                               NULL, &desktop_menu->menu_item_cache);
    }

    return ret;
}

static void
_xfce_desktop_menu_free_menudata(XfceDesktopMenu *desktop_menu)
{
    if(desktop_menu->menu_item_cache) {
        g_list_foreach(desktop_menu->menu_item_cache,
                       (GFunc)gtk_widget_destroy, NULL);
        g_list_free(desktop_menu->menu_item_cache);
        desktop_menu->menu_item_cache = NULL;
    }

    if(desktop_menu->garcon_menu) {
        g_object_unref(G_OBJECT(desktop_menu->garcon_menu));
        desktop_menu->garcon_menu = NULL;
    }
    
    desktop_menu->garcon_menu = NULL;
}

static gboolean
_generate_menu_idled(gpointer data)
{
    XfceDesktopMenu *desktop_menu = data;
    
    g_return_val_if_fail(data != NULL, FALSE);
    
    _generate_menu(desktop_menu);
    desktop_menu->idle_id = 0;
    
    return FALSE;
}

static void
desktop_menu_recache(gpointer data,
                     GObject *where_the_object_was)
{
    XfceDesktopMenu *desktop_menu = data;
    if(!desktop_menu->menu_item_cache) {
        desktop_menu_add_items(desktop_menu, desktop_menu->garcon_menu,
                               NULL, &desktop_menu->menu_item_cache);
    }
}

static gchar *
xfce_desktop_get_menufile(void)
{
    XfceKiosk *kiosk;
    gboolean user_menu;
    gchar *menu_file = NULL;
    gchar **all_dirs;
    const gchar *userhome = xfce_get_homedir();
    gint i;

    kiosk = xfce_kiosk_new("xfdesktop");
    user_menu = xfce_kiosk_query(kiosk, "UserMenu");
    xfce_kiosk_free(kiosk);
    
    if(user_menu) {
        gchar *file = xfce_resource_save_location(XFCE_RESOURCE_CONFIG,
                                                  "menus/xfce-applications.menu",
                                                  FALSE);
        if(file) {
            DBG("checking %s", file);
            if(g_file_test(file, G_FILE_TEST_IS_REGULAR))
                return file;
            else
                g_free(file);
        }
    }
    
    all_dirs = xfce_resource_lookup_all(XFCE_RESOURCE_CONFIG,
                                        "menus/xfce-applications.menu");
    for(i = 0; all_dirs[i]; i++) {
        DBG("checking %s", all_dirs[i]);
        if(user_menu || strstr(all_dirs[i], userhome) != all_dirs[i]) {
            if(g_file_test(all_dirs[i], G_FILE_TEST_IS_REGULAR)) {
                menu_file = g_strdup(all_dirs[i]);
                break;
            }
        }
    }
    g_strfreev(all_dirs);

    if(!menu_file)
        g_warning("%s: Could not locate a menu definition file", PACKAGE);

    return menu_file;
}


XfceDesktopMenu *
xfce_desktop_menu_new(const gchar *menu_file,
                           gboolean deferred)
{
    XfceDesktopMenu *desktop_menu = g_new0(XfceDesktopMenu, 1);
    
    desktop_menu->use_menu_icons = TRUE;
    desktop_menu->cache_menu_items = TRUE;  /* FIXME: hidden pref? */
    
    if(menu_file)
        desktop_menu->filename = g_strdup(menu_file);
    else
        desktop_menu->filename = xfce_desktop_get_menufile();
    
    if(deferred)
        desktop_menu->idle_id = g_idle_add(_generate_menu_idled, desktop_menu);
    else {
        if(!_generate_menu(desktop_menu)) {
            g_free(desktop_menu);
            desktop_menu = NULL;
        }
    }
    
    g_signal_connect(G_OBJECT(_deskmenu_icon_theme), "changed",
                     G_CALLBACK(itheme_changed_cb), desktop_menu);
    
    return desktop_menu;
}

void
xfce_desktop_menu_populate_menu(XfceDesktopMenu *desktop_menu,
                                     GtkWidget *menu)
{
    g_return_if_fail(desktop_menu && menu);
    
    if(!desktop_menu->garcon_menu) {
        if(desktop_menu->idle_id) {
            g_source_remove(desktop_menu->idle_id);
            desktop_menu->idle_id = 0;
        }
        _generate_menu(desktop_menu);
        if(!desktop_menu->garcon_menu)
            return;
    }

    if(desktop_menu->menu_item_cache) {
        GList *l;
        for(l = desktop_menu->menu_item_cache; l; l = l->next)
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), l->data);
        g_list_free(desktop_menu->menu_item_cache);
        desktop_menu->menu_item_cache = NULL;
        g_object_weak_ref(G_OBJECT(menu), desktop_menu_recache,
                          desktop_menu);
    } else {
        desktop_menu_add_items(desktop_menu, desktop_menu->garcon_menu,
                               GTK_WIDGET(menu), NULL);
    }
}

GtkWidget *
xfce_desktop_menu_get_widget(XfceDesktopMenu *desktop_menu)
{
    GtkWidget *menu;
    
    g_return_val_if_fail(desktop_menu != NULL, NULL);
    
    menu = gtk_menu_new();
    
    xfce_desktop_menu_populate_menu(desktop_menu, menu);
    
    if(!desktop_menu->garcon_menu) {
        gtk_widget_destroy(menu);
        return NULL;
    }
    
    return menu;
}

G_CONST_RETURN gchar *
xfce_desktop_menu_get_menu_file(XfceDesktopMenu *desktop_menu)
{
    g_return_val_if_fail(desktop_menu != NULL, NULL);
    
    return desktop_menu->filename;
}

gboolean
xfce_desktop_menu_need_update(XfceDesktopMenu *desktop_menu)
{
    return FALSE;
}

void
xfce_desktop_menu_start_autoregen(XfceDesktopMenu *desktop_menu,
                                       guint delay)
{
    /* noop */
}

void
xfce_desktop_menu_stop_autoregen(XfceDesktopMenu *desktop_menu)
{
    /* noop */
}

void
xfce_desktop_menu_force_regen(XfceDesktopMenu *desktop_menu)
{
    TRACE("dummy");
    g_return_if_fail(desktop_menu != NULL);
    
    if(desktop_menu->idle_id) {
        g_source_remove(desktop_menu->idle_id);
        desktop_menu->idle_id = 0;
    }

    _generate_menu(desktop_menu);
}

void
xfce_desktop_menu_set_show_icons(XfceDesktopMenu *desktop_menu,
                                      gboolean show_icons)
{
    g_return_if_fail(desktop_menu != NULL);
    
    if(desktop_menu->use_menu_icons != show_icons) {
        desktop_menu->use_menu_icons = show_icons;
        if(desktop_menu->idle_id) {
            g_source_remove(desktop_menu->idle_id);
            desktop_menu->idle_id = 0;
        }
        _generate_menu(desktop_menu);
    }
}

void
xfce_desktop_menu_destroy(XfceDesktopMenu *desktop_menu)
{
    g_return_if_fail(desktop_menu != NULL);
    TRACE("dummy");
    
    if(desktop_menu->idle_id) {
        g_source_remove(desktop_menu->idle_id);
        desktop_menu->idle_id = 0;
    }
    
    g_signal_handlers_disconnect_by_func(_deskmenu_icon_theme,
                                         G_CALLBACK(itheme_changed_cb),
                                         desktop_menu);
    
    _xfce_desktop_menu_free_menudata(desktop_menu);
    g_free(desktop_menu->filename);
    g_free(desktop_menu);
}
