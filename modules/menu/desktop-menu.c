/*  xfce4
 *  
 *  Copyright (C) 2002-2003 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *                     2003 Biju Chacko (botsie@users.sourceforge.net)
 *                     2004 Danny Milosavljevic <danny.milo@gmx.net>
 *                2004-2007 Brian Tarricone <bjt23@cornell.edu>
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
#include <gmodule.h>
#include <glib.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>
#include <libxfce4menu/libxfce4menu.h>

#ifdef HAVE_THUNAR_VFS
#include <thunar-vfs/thunar-vfs.h>
#endif

#include "desktop-menu-utils.h"

typedef struct
{
	XfceMenu *xfce_menu;

    gboolean cache_menu_items;
    GList *menu_item_cache;
	
    gchar *filename;  /* file the menu is currently using */
    gboolean using_default_menu;
    
    gboolean use_menu_icons;  /* show menu icons? */
	
    gint idle_id;  /* source id for idled generation */
    
    gboolean modified;
    
#ifdef HAVE_THUNAR_VFS
    GList *monitors;
#endif
} XfceDesktopMenu;

static void _xfce_desktop_menu_free_menudata(XfceDesktopMenu *desktop_menu);

static gint _xfce_desktop_menu_icon_size = 24;
static GtkIconTheme *_deskmenu_icon_theme = NULL;

static gboolean _generate_menu_idled(gpointer data);
static gboolean _generate_menu(XfceDesktopMenu *desktop_menu,
                               gboolean deferred);
static void desktop_menu_add_items(XfceDesktopMenu *desktop_menu,
                                   XfceMenu *xfce_menu,
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

#ifdef HAVE_THUNAR_VFS

static void
desktop_menu_something_changed(ThunarVfsMonitor *monitor,
                               ThunarVfsMonitorHandle *handle,
                               ThunarVfsMonitorEvent event,
                               ThunarVfsPath *handle_path,
                               ThunarVfsPath *event_path,
                               gpointer user_data)
{
    XfceDesktopMenu *desktop_menu = user_data;
    XfceMenuItemCache *cache = xfce_menu_item_cache_get_default();
    
#ifdef DEBUG
    {
        gchar buf[1024];
        thunar_vfs_path_to_string(event_path, buf, sizeof(buf), NULL);
        TRACE("entering (%d,%s)", event, buf);
    }
#endif
    
    xfce_menu_item_cache_invalidate(cache);
    if(!desktop_menu->idle_id)
        desktop_menu->idle_id = g_idle_add(_generate_menu_idled, desktop_menu);
}

#if 0
static gpointer
desktop_menu_xfce_menu_monitor_file(XfceMenu *menu,
                                    const gchar *filename,
                                    gpointer user_data)
{
    XfceDesktopMenu *desktop_menu = user_data;
    ThunarVfsPath *path;
    ThunarVfsMonitor *monitor = thunar_vfs_monitor_get_default();
    ThunarVfsMonitorHandle *mhandle = NULL;
    
    path = thunar_vfs_path_new(filename, NULL);
    if(path) {
        mhandle = thunar_vfs_monitor_add_file(monitor, path,
                                              desktop_menu_something_changed,
                                              desktop_menu);
        thunar_vfs_path_unref(path);
    }
    
    g_object_unref(G_OBJECT(monitor));
    
    TRACE("exiting (%s), returning 0x%p\n", filename, mhandle);
    
    return mhandle;
}
#endif

static gpointer
desktop_menu_xfce_menu_monitor_directory(XfceMenu *menu,
                                         const gchar *filename,
                                         gpointer user_data)
{
    XfceDesktopMenu *desktop_menu = user_data;
    ThunarVfsPath *path;
    ThunarVfsMonitorHandle *mhandle = NULL;
    
    if(!g_file_test(filename, G_FILE_TEST_IS_DIR))
        return NULL;
    
    path = thunar_vfs_path_new(filename, NULL);
    if(path) {
        ThunarVfsMonitor *monitor = thunar_vfs_monitor_get_default();
        mhandle = thunar_vfs_monitor_add_directory(monitor, path,
                                                   desktop_menu_something_changed,
                                                   desktop_menu);
        thunar_vfs_path_unref(path);
        g_object_unref(G_OBJECT(monitor));
    }
    
    TRACE("exiting (%s), returning 0x%p", filename, mhandle);
    
    return mhandle;
}

static void
desktop_menu_xfce_menu_remove_monitor(XfceMenu *menu,
                                      gpointer monitor_handle)
{
    ThunarVfsMonitor *monitor = thunar_vfs_monitor_get_default();
    ThunarVfsMonitorHandle *mhandle = monitor_handle;
    
    TRACE("entering (0x%p)", mhandle); 
    
    thunar_vfs_monitor_remove(monitor, mhandle);
    g_object_unref(G_OBJECT(monitor));
}

#endif

/*
 * this is a bit of a kludge.  in order to support the cache and be a bit
 * faster, we either want to build a GtkMenu, or we want to just build
 * the GtkMenuItems and store them in a GList for later.  only one
 * of |menu| or |menu_items_return| should be non-NULL
 */
static void
desktop_menu_add_items(XfceDesktopMenu *desktop_menu,
                       XfceMenu *xfce_menu,
                       GtkWidget *menu,
                       GList **menu_items_return)
{
    GSList *items, *l;
    GtkWidget *submenu, *mi, *img;
    XfceMenu *xfce_submenu;
    XfceMenuDirectory *xfce_directory;
    XfceMenuItem *xfce_item;
    const gchar *name, *icon_name;

    g_return_if_fail((menu && !menu_items_return)
                     || (!menu && menu_items_return));
    
    if(xfce_menu_has_layout(xfce_menu))
        items = xfce_menu_get_layout_elements(xfce_menu);
    else {
        items = xfce_menu_get_menus(xfce_menu);
        items = g_slist_concat(items, xfce_menu_get_items(xfce_menu));
    }
    for(l = items; l; l = l->next) {
        if(XFCE_IS_MENU(l->data)) {
            xfce_submenu = l->data;
            xfce_directory = xfce_menu_get_directory(xfce_submenu);
            icon_name = NULL;
            
            if(xfce_directory
               && (xfce_menu_directory_get_no_display(xfce_directory)
               || !xfce_menu_directory_show_in_environment(xfce_directory)))
            {
                continue;
            }
            
            submenu = gtk_menu_new();
            gtk_widget_show(submenu);
            
            if(xfce_directory) {
                name = xfce_menu_directory_get_name(xfce_directory);
                if(desktop_menu->use_menu_icons)
                    icon_name = xfce_menu_directory_get_icon(xfce_directory);
            } else
                name = xfce_menu_get_name(xfce_submenu);
            
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
            
            desktop_menu_add_items(desktop_menu, xfce_submenu,
                                   submenu, NULL);
            
            /* we have to check emptiness down here instead of at the top of the
             * loop because there may be further submenus that are empty */
            if(!gtk_container_get_children(GTK_CONTAINER(submenu)))
                gtk_widget_destroy(mi);
        } else if(XFCE_IS_MENU_SEPARATOR(l->data)) {
            mi = gtk_separator_menu_item_new();
            gtk_widget_show(mi);

            if(menu)
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            else
                *menu_items_return = g_list_prepend(*menu_items_return, mi);
        } else if(XFCE_IS_MENU_ITEM(l->data)) {
            const gchar *name = NULL;
            
            xfce_item = l->data;
            
            if(xfce_menu_item_get_no_display(xfce_item)
               || !xfce_menu_item_show_in_environment(xfce_item))
            {
                continue;
            }
            
            if(xfce_menu_item_has_category(xfce_item, "X-XFCE"))
                name = xfce_menu_item_get_generic_name(xfce_item);
            if(!name)
                name = xfce_menu_item_get_name(xfce_item);
            
            mi = xfce_app_menu_item_new_full(name,
                                             xfce_menu_item_get_command(xfce_item),
                                             desktop_menu->use_menu_icons
                                             ? xfce_menu_item_get_icon_name(xfce_item)
                                             : NULL,
                                             xfce_menu_item_requires_terminal(xfce_item),
                                             xfce_menu_item_supports_startup_notification(xfce_item));
            gtk_widget_show(mi);

            if(menu)
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            else
                *menu_items_return = g_list_prepend(*menu_items_return, mi);
        }
    }
    g_slist_free(items);

    if(menu_items_return)
        *menu_items_return = g_list_reverse(*menu_items_return);
}

static gboolean
_generate_menu(XfceDesktopMenu *desktop_menu,
               gboolean force)
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

#if 0
    /* this is kinda lame, but xfcemenu currently caches too much */
    xfce_menu_shutdown();
    xfce_menu_init("XFCE");
#endif
    
    desktop_menu->xfce_menu = xfce_menu_new(desktop_menu->filename, &error);
    if(!desktop_menu->xfce_menu) {
        g_critical("Unable to create XfceMenu from file '%s': %s",
                   desktop_menu->filename, error->message);
        g_error_free(error);
        return FALSE;
    }

    if(desktop_menu->cache_menu_items) {
        desktop_menu_add_items(desktop_menu, desktop_menu->xfce_menu,
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

    if(desktop_menu->xfce_menu) {
        g_object_unref(G_OBJECT(desktop_menu->xfce_menu));
        desktop_menu->xfce_menu = NULL;
    }
    
    desktop_menu->xfce_menu = NULL;
}

static gboolean
_generate_menu_idled(gpointer data) {
    XfceDesktopMenu *desktop_menu = data;
    
    g_return_val_if_fail(data != NULL, FALSE);
    
    _generate_menu(desktop_menu, FALSE);
    desktop_menu->idle_id = 0;
    
    return FALSE;
}

static void
desktop_menu_recache(gpointer data,
                     GObject *where_the_object_was)
{
    XfceDesktopMenu *desktop_menu = data;
    if(!desktop_menu->menu_item_cache) {
        desktop_menu_add_items(desktop_menu, desktop_menu->xfce_menu,
                               NULL, &desktop_menu->menu_item_cache);
    }
}

G_MODULE_EXPORT XfceDesktopMenu *
xfce_desktop_menu_new_impl(const gchar *menu_file,
                           gboolean deferred)
{
#ifdef HAVE_THUNAR_VFS
    static XfceMenuMonitorVTable monitor_vtable = {
        NULL, /*desktop_menu_xfce_menu_monitor_file,*/
        desktop_menu_xfce_menu_monitor_directory,
        desktop_menu_xfce_menu_remove_monitor
    };
#endif
    XfceDesktopMenu *desktop_menu = g_new0(XfceDesktopMenu, 1);
    
    desktop_menu->use_menu_icons = TRUE;
    desktop_menu->cache_menu_items = TRUE;  /* FIXME: hidden pref? */
    
    if(menu_file)
        desktop_menu->filename = g_strdup(menu_file);
    else {
        desktop_menu->filename = xfce_desktop_get_menufile();
        desktop_menu->using_default_menu = TRUE;
    }
    
#ifdef HAVE_THUNAR_VFS
    thunar_vfs_init();
    xfce_menu_monitor_set_vtable(&monitor_vtable, desktop_menu);
#endif
    
    if(deferred)
        desktop_menu->idle_id = g_idle_add(_generate_menu_idled, desktop_menu);
    else {
        if(!_generate_menu(desktop_menu, FALSE)) {
#ifdef HAVE_THUNAR_VFS
            xfce_menu_monitor_set_vtable(NULL, NULL);
#endif
            g_free(desktop_menu);
            desktop_menu = NULL;
        }
    }
    
    g_signal_connect(G_OBJECT(_deskmenu_icon_theme), "changed",
                     G_CALLBACK(itheme_changed_cb), desktop_menu);
    
    return desktop_menu;
}

G_MODULE_EXPORT void
xfce_desktop_menu_populate_menu_impl(XfceDesktopMenu *desktop_menu,
                                     GtkWidget *menu)
{
    g_return_if_fail(desktop_menu && menu);
    
    if(!desktop_menu->xfce_menu) {
        if(desktop_menu->idle_id) {
            g_source_remove(desktop_menu->idle_id);
            desktop_menu->idle_id = 0;
        }
        _generate_menu(desktop_menu, FALSE);
        if(!desktop_menu->xfce_menu)
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
        desktop_menu_add_items(desktop_menu, desktop_menu->xfce_menu,
                               GTK_WIDGET(menu), NULL);
    }
}

G_MODULE_EXPORT GtkWidget *
xfce_desktop_menu_get_widget_impl(XfceDesktopMenu *desktop_menu)
{
    GtkWidget *menu;
    
    g_return_val_if_fail(desktop_menu != NULL, NULL);
    
    menu = gtk_menu_new();
    
    xfce_desktop_menu_populate_menu_impl(desktop_menu, menu);
    
    if(!desktop_menu->xfce_menu) {
        gtk_widget_destroy(menu);
        return NULL;
    }
    
    return menu;
}

G_MODULE_EXPORT G_CONST_RETURN gchar *
xfce_desktop_menu_get_menu_file_impl(XfceDesktopMenu *desktop_menu)
{
    g_return_val_if_fail(desktop_menu != NULL, NULL);
    
    return desktop_menu->filename;
}

G_MODULE_EXPORT gboolean
xfce_desktop_menu_need_update_impl(XfceDesktopMenu *desktop_menu)
{
    return FALSE;
}

G_MODULE_EXPORT void
xfce_desktop_menu_start_autoregen_impl(XfceDesktopMenu *desktop_menu,
                                       guint delay)
{
    /* noop */
}

G_MODULE_EXPORT void
xfce_desktop_menu_stop_autoregen_impl(XfceDesktopMenu *desktop_menu)
{
    /* noop */
}

G_MODULE_EXPORT void
xfce_desktop_menu_force_regen_impl(XfceDesktopMenu *desktop_menu)
{
    TRACE("dummy");
    g_return_if_fail(desktop_menu != NULL);
    
    if(desktop_menu->idle_id) {
        g_source_remove(desktop_menu->idle_id);
        desktop_menu->idle_id = 0;
    }

    _generate_menu(desktop_menu, TRUE);
}

G_MODULE_EXPORT void
xfce_desktop_menu_set_show_icons_impl(XfceDesktopMenu *desktop_menu,
                                      gboolean show_icons)
{
    g_return_if_fail(desktop_menu != NULL);
    
    if(desktop_menu->use_menu_icons != show_icons) {
        desktop_menu->use_menu_icons = show_icons;
        if(desktop_menu->idle_id) {
            g_source_remove(desktop_menu->idle_id);
            desktop_menu->idle_id = 0;
        }
        _generate_menu(desktop_menu, TRUE);
    }
}

G_MODULE_EXPORT void
xfce_desktop_menu_destroy_impl(XfceDesktopMenu *desktop_menu)
{
    g_return_if_fail(desktop_menu != NULL);
    TRACE("dummy");
    
#ifdef HAVE_THUNAR_VFS
    xfce_menu_monitor_set_vtable(NULL, NULL);
#endif
    
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

G_MODULE_EXPORT gchar *
g_module_check_init(GModule *module)
{
    gint w, h;
    
	/* libxfcemenu registers gobject types, so we can't be removed */
	g_module_make_resident(module);
	
	xfce_menu_init("XFCE");
	
    gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &w, &h);
    _xfce_desktop_menu_icon_size = w;
    
    _deskmenu_icon_theme = gtk_icon_theme_get_default();
    
    return NULL;
}
