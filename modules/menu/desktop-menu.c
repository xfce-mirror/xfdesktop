/*  xfce4
 *  
 *  Copyright (C) 2002-2003 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *                     2003 Biju Chacko (botsie@users.sourceforge.net)
 *                     2004 Danny Milosavljevic <danny.milo@gmx.net>
 *                     2004-2007 Brian Tarricone <bjt23@cornell.edu>
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

#ifdef HAVE_THUNAR_VFS
#include <thunar-vfs/thunar-vfs.h>
#endif

#include "xfdesktop-common.h"

#include "desktop-menu-private.h"
#include "desktop-menu.h"
#include "dummy_icon.h"

/*< private >*/
GdkPixbuf *dummy_icon = NULL;
GdkPixbuf *unknown_icon = NULL;
gint _xfce_desktop_menu_icon_size = 24;
static GtkIconTheme *_deskmenu_icon_theme = NULL;

static gboolean _generate_menu(XfceDesktopMenu *desktop_menu,
                               gboolean deferred);
static void desktop_menu_add_items(XfceDesktopMenu *desktop_menu,
                                   XfceMenu *xfce_menu,
                                   GtkWidget *menu,
                                   GHashTable *watch_dirs);

static void
itheme_changed_cb(GtkIconTheme *itheme, gpointer user_data)
{
    _generate_menu((XfceDesktopMenu *)user_data, FALSE);
}

#if 0
static gint
compare_items(gconstpointer a,
              gconstpointer b)
{
  return g_utf8_collate(xfce_menu_item_get_name((XfceMenuItem *)a),
                        xfce_menu_item_get_name((XfceMenuItem *)b));
}
#endif

static void
desktop_menu_monitors_destroy(XfceDesktopMenu *desktop_menu)
{
#ifdef HAVE_THUNAR_VFS
    ThunarVfsMonitor *monitor = thunar_vfs_monitor_get_default();
    GList *l;
    
    for(l = desktop_menu->monitors; l; l = l->next) {
        ThunarVfsMonitorHandle *mhandle = l->data;
        thunar_vfs_monitor_remove(monitor, mhandle);
    }
    g_list_free(desktop_menu->monitors);
    desktop_menu->monitors = NULL;
    
    g_object_unref(G_OBJECT(monitor));
#endif
}

#ifdef HAVE_THUNAR_VFS
static void
desktop_menu_directory_changed(ThunarVfsMonitor *monitor,
                               ThunarVfsMonitorHandle *handle,
                               ThunarVfsMonitorEvent event,
                               ThunarVfsPath *handle_path,
                               ThunarVfsPath *event_path,
                               gpointer user_data)
{
    XfceDesktopMenu *desktop_menu = user_data;
    _generate_menu(desktop_menu, FALSE);
}

static void
desktop_menu_monitor_add(gpointer key,
                         gpointer value,
                         gpointer user_data)
{
    XfceDesktopMenu *desktop_menu = user_data;
    const gchar *pathstr = key;
    ThunarVfsPath *path;
    ThunarVfsMonitor *monitor = thunar_vfs_monitor_get_default();
    ThunarVfsMonitorHandle *mhandle;
    
    path = thunar_vfs_path_new(pathstr, NULL);
    if(path) {
        mhandle = thunar_vfs_monitor_add_directory(monitor, path,
                                                   desktop_menu_directory_changed,
                                                   desktop_menu);
        desktop_menu->monitors = g_list_prepend(desktop_menu->monitors,
                                                mhandle);
        thunar_vfs_path_unref(path);
    }
    
    g_object_unref(G_OBJECT(monitor));
}
#endif

static void
desktop_menu_monitors_create(XfceDesktopMenu *desktop_menu,
                            GHashTable *watch_dirs)
{
#ifdef HAVE_THUNAR_VFS
    if(desktop_menu->monitors) {
        g_warning("Attempt to create new monitors without destroying the old.");
        desktop_menu_monitors_destroy(desktop_menu);
    }
    
    g_hash_table_foreach(watch_dirs, desktop_menu_monitor_add, desktop_menu);
#endif
}

static void
desktop_menu_watch_dirs_add(GHashTable *watch_dirs,
                            const gchar *filename)
{
    gchar *dirname = g_strdup(filename), *p;
    
    p = g_strrstr(dirname, G_DIR_SEPARATOR_S);
    if(!p) {
        g_warning("Can't add \"%s\" to watch dirs: no dir separator", dirname);
        return;
    }
    
    *p = 0;    
    g_hash_table_replace(watch_dirs, dirname, GINT_TO_POINTER(1));
}

static void
desktop_menu_add_items(XfceDesktopMenu *desktop_menu,
                       XfceMenu *xfce_menu,
                       GtkWidget *menu,
                       GHashTable *watch_dirs)
{
    GSList *items, *l;
    GtkWidget *submenu, *mi, *img;
    XfceMenu *xfce_submenu;
    XfceMenuDirectory *xfce_directory;
    XfceMenuItem *xfce_item;
    const gchar *name, *icon_name;
    
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
                icon_name = xfce_menu_directory_get_icon(xfce_directory);
                desktop_menu_watch_dirs_add(watch_dirs,
                                            xfce_menu_directory_get_filename(xfce_directory));
            } else {
                name = xfce_menu_get_name(xfce_submenu);
                icon_name = NULL;
            }
            
            mi = gtk_image_menu_item_new_with_label(name);
            if(icon_name) {
                img = gtk_image_new_from_icon_name(icon_name,
                                                   GTK_ICON_SIZE_MENU);
                gtk_widget_show(img);
                gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
            }
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi), submenu);
            
            desktop_menu_add_items(desktop_menu, xfce_submenu, submenu,
                                   watch_dirs);
            
            /* we have to check emptiness down here instead of at the top of the
             * loop because there may be further submenus that are empty */
            if(!gtk_container_get_children(GTK_CONTAINER(submenu)))
                gtk_widget_destroy(mi);
        } else if(XFCE_IS_MENU_SEPARATOR(l->data)) {
            mi = gtk_separator_menu_item_new();
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        } else if(XFCE_IS_MENU_ITEM(l->data)) {
            xfce_item = l->data;
            
            if(xfce_menu_item_get_no_display(xfce_item)
               || !xfce_menu_item_show_in_environment(xfce_item))
            {
                continue;
            }
            
            mi = xfce_app_menu_item_new_full(xfce_menu_item_get_name(xfce_item),
                                             xfce_menu_item_get_command(xfce_item),
                                             xfce_menu_item_get_icon_name(xfce_item),
                                             xfce_menu_item_requires_terminal(xfce_item),
                                             xfce_menu_item_supports_startup_notification(xfce_item));
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            
            desktop_menu_watch_dirs_add(watch_dirs,
                                        xfce_menu_item_get_filename(xfce_item));
        }
    }
    g_slist_free(items);
}

static gboolean
_generate_menu(XfceDesktopMenu *desktop_menu,
               gboolean force)
{
    gboolean ret = TRUE;
    XfceKiosk *kiosk;
    gboolean user_menu;
    GError *error = NULL;
    GHashTable *watch_dirs;
    gchar *local_menus_dir;
    
    _xfce_desktop_menu_free_menudata(desktop_menu);
    
    watch_dirs = g_hash_table_new_full(g_str_hash, g_str_equal,
                                       (GDestroyNotify)g_free, NULL);
    
    /* FIXME: do something with this */
    kiosk = xfce_kiosk_new("xfdesktop");
    user_menu = xfce_kiosk_query(kiosk, "UserMenu");
    xfce_kiosk_free(kiosk);
    
    DBG("menu file name is %s", desktop_menu->filename);
    
    /* this is kinda lame, but xfcemenu currently caches too much */
    xfce_menu_shutdown();
    xfce_menu_init("XFCE");
    
    if(g_file_test(desktop_menu->filename, G_FILE_TEST_EXISTS)) {
        desktop_menu->xfce_menu = xfce_menu_new(desktop_menu->filename, &error);
        if(!desktop_menu->xfce_menu) {
            g_critical("Unable to create XfceMenu from file '%s': %s",
                       desktop_menu->filename, error->message);
            g_error_free(error);
            return FALSE;
        }
    
        desktop_menu->menu = gtk_menu_new();
        gtk_widget_show(desktop_menu->menu);
    
        desktop_menu_watch_dirs_add(watch_dirs,
                                    xfce_menu_get_filename(desktop_menu->xfce_menu));
    
    
        desktop_menu_add_items(desktop_menu, desktop_menu->xfce_menu,
                               desktop_menu->menu, watch_dirs);
        
        /* really don't need to keep this around */
        g_object_unref(G_OBJECT(desktop_menu->xfce_menu));
        desktop_menu->xfce_menu = NULL;
    }
    
    if(user_menu && desktop_menu->using_default_menu) {
        local_menus_dir = xfce_resource_save_location(XFCE_RESOURCE_CONFIG,
                                                     "menus/", TRUE);
        if(local_menus_dir) {
            desktop_menu_watch_dirs_add(watch_dirs, local_menus_dir);
            g_free(local_menus_dir);
        }
    }
    
    desktop_menu_monitors_create(desktop_menu, watch_dirs);
    
    g_hash_table_destroy(watch_dirs);
    
    return ret;
}

/*< private >*/
void
_xfce_desktop_menu_free_menudata(XfceDesktopMenu *desktop_menu)
{
    desktop_menu_monitors_destroy(desktop_menu);
    
    if(desktop_menu->menu)
        gtk_widget_destroy(desktop_menu->menu);
    if(desktop_menu->xfce_menu)
        g_object_unref(G_OBJECT(desktop_menu->xfce_menu));
    
    desktop_menu->menu = NULL;
    desktop_menu->xfce_menu = NULL;
}

void
_desktop_menu_ensure_unknown_icon(void)
{
    if(!unknown_icon)
        unknown_icon = gdk_pixbuf_new_from_inline(-1, xfce_unknown, TRUE, NULL);
}

static gboolean
_generate_menu_initial(gpointer data) {
    XfceDesktopMenu *desktop_menu = data;
    
    g_return_val_if_fail(data != NULL, FALSE);
    
    _generate_menu(desktop_menu, FALSE);
    desktop_menu->idle_id = 0;
    
    return FALSE;
}

G_MODULE_EXPORT XfceDesktopMenu *
xfce_desktop_menu_new_impl(const gchar *menu_file,
                           gboolean deferred)
{
    XfceDesktopMenu *desktop_menu = g_new0(XfceDesktopMenu, 1);
    
    desktop_menu->use_menu_icons = TRUE;
    
    if(menu_file)
        desktop_menu->filename = g_strdup(menu_file);
    else {
        desktop_menu->filename = xfce_desktop_get_menufile();
        desktop_menu->using_default_menu = TRUE;
    }
    
    if(deferred)
        desktop_menu->idle_id = g_idle_add(_generate_menu_initial, desktop_menu);
    else {
        if(!_generate_menu(desktop_menu, FALSE)) {
            g_free(desktop_menu);
            desktop_menu = NULL;
        }
    }
    
    g_signal_connect(G_OBJECT(_deskmenu_icon_theme), "changed",
                     G_CALLBACK(itheme_changed_cb), desktop_menu);
    
    return desktop_menu;
}

G_MODULE_EXPORT GtkWidget *
xfce_desktop_menu_get_widget_impl(XfceDesktopMenu *desktop_menu)
{
    g_return_val_if_fail(desktop_menu != NULL, NULL);
    
    return desktop_menu->menu;
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
    
    _generate_menu(desktop_menu, TRUE);
}

G_MODULE_EXPORT void
xfce_desktop_menu_set_show_icons_impl(XfceDesktopMenu *desktop_menu,
                                      gboolean show_icons)
{
    g_return_if_fail(desktop_menu != NULL);
    
    if(desktop_menu->use_menu_icons != show_icons) {
        desktop_menu->use_menu_icons = show_icons;
        _generate_menu(desktop_menu, TRUE);
    }
}

G_MODULE_EXPORT void
xfce_desktop_menu_destroy_impl(XfceDesktopMenu *desktop_menu)
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

G_MODULE_EXPORT gchar *
g_module_check_init(GModule *module)
{
    gint w, h;
    
	/* libxfcemenu registers gobject types, so we can't be removed */
	g_module_make_resident(module);
	
	xfce_menu_init("XFCE");
	
    gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &w, &h);
    _xfce_desktop_menu_icon_size = w;
    
    if(dummy_icon)
        g_object_unref(G_OBJECT(dummy_icon));
    dummy_icon = xfce_inline_icon_at_size(dummy_icon_data,
                                           _xfce_desktop_menu_icon_size,
                                          _xfce_desktop_menu_icon_size);
    
    _deskmenu_icon_theme = gtk_icon_theme_get_default();
    
    return NULL;
}
