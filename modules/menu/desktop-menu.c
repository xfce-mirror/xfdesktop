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

#include "xfdesktop-common.h"

#include "desktop-menu-private.h"
#include "desktop-menu.h"
#include "dummy_icon.h"

/*< private >*/
GdkPixbuf *dummy_icon = NULL;
GdkPixbuf *unknown_icon = NULL;
gint _xfce_desktop_menu_icon_size = 24;
static GtkIconTheme *_deskmenu_icon_theme = NULL;
static GList *timeout_handles = NULL;
static time_t last_settings_change = 0;

static void desktop_menu_add_items(XfceDesktopMenu *desktop_menu,
                                   FrapMenu *frap_menu,
                                   GtkWidget *menu);

static void
itheme_changed_cb(GtkIconTheme *itheme, gpointer user_data)
{
    last_settings_change = time(NULL);
}

#if 0
static gint
compare_items(gconstpointer a,
              gconstpointer b)
{
  return g_utf8_collate(frap_menu_item_get_name((FrapMenuItem *)a),
                        frap_menu_item_get_name((FrapMenuItem *)b));
}
#endif

static void
desktop_menu_add_items(XfceDesktopMenu *desktop_menu,
                       FrapMenu *frap_menu,
                       GtkWidget *menu)
{
    GSList *layout_items, *l;
    GtkWidget *submenu, *mi;
    FrapMenu *frap_submenu;
    FrapMenuItem *frap_item;
    
    layout_items = frap_menu_get_layout_items(frap_menu);
    for(l = layout_items; l; l = l->next) {
        if(FRAP_IS_MENU(l->data)) {
            frap_submenu = l->data;
            
            submenu = gtk_menu_new();
            gtk_widget_show(submenu);
            
            mi = gtk_menu_item_new_with_label(frap_menu_get_name(frap_submenu));
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
            gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi), submenu);
            
            desktop_menu_add_items(desktop_menu, frap_submenu, submenu);
            
            /* we have to check emptiness down here instead of at the top of the
             * loop because there may be further submenus that are empty */
            if(!gtk_container_get_children(GTK_CONTAINER(submenu)))
                gtk_widget_destroy(mi);
        } else if(FRAP_IS_MENU_SEPARATOR(l->data)) {
            mi = gtk_separator_menu_item_new();
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        } else if(FRAP_IS_MENU_ITEM(l->data)) {
            frap_item = l->data;
            
            if(frap_menu_item_get_no_display(frap_item)
               || !frap_menu_item_show_in_environment(frap_item))
            {
                continue;
            }
            
            mi = xfce_app_menu_item_new_full(frap_menu_item_get_name(frap_item),
                                             frap_menu_item_get_command(frap_item),
                                             frap_menu_item_get_icon_name(frap_item),
                                             frap_menu_item_requires_terminal(frap_item),
                                             frap_menu_item_supports_startup_notification(frap_item));
            gtk_widget_show(mi);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        }
    }
    g_slist_free(layout_items);
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
    
    /* FIXME: do something with this */
    kiosk = xfce_kiosk_new("xfdesktop");
    user_menu = xfce_kiosk_query(kiosk, "UserMenu");
    xfce_kiosk_free(kiosk);
    
    if(!desktop_menu->filename)
        desktop_menu->filename = xfce_desktop_get_menufile();
    
    DBG("menu file name is %s", desktop_menu->filename);
    desktop_menu->frap_menu = frap_menu_new(desktop_menu->filename, &error);
    if(!desktop_menu->frap_menu) {
        g_critical("Unable to create FrapMenu from file '%s': %s",
                   desktop_menu->filename, error->message);
        g_error_free(error);
        return FALSE;
    }
    
    desktop_menu->menu = gtk_menu_new();
	gtk_widget_show(desktop_menu->menu);
    
    desktop_menu_add_items(desktop_menu, desktop_menu->frap_menu,
                           desktop_menu->menu);
    
    desktop_menu->last_menu_gen = time(NULL);
    
    return ret;
}

/* g_timeout handler */
static gboolean
_menu_check_update(gpointer data)
{
    XfceDesktopMenu *desktop_menu = data;
    gboolean modified = FALSE;
    
    TRACE("desktop_menu: %p", desktop_menu);
    
    g_return_val_if_fail(desktop_menu != NULL, FALSE);
    
    modified = xfce_desktop_menu_need_update_impl(desktop_menu);
    
    if(desktop_menu->using_default_menu) {
		g_free(desktop_menu->filename);
        desktop_menu->filename = xfce_desktop_get_menufile();
    }
    
    if(modified)
        _generate_menu(desktop_menu, TRUE);
    
    return TRUE;
}

/*< private >*/
void
_xfce_desktop_menu_free_menudata(XfceDesktopMenu *desktop_menu)
{
    if(desktop_menu->menu)
        gtk_widget_destroy(desktop_menu->menu);
    
    desktop_menu->menu = NULL;
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

static gboolean
desktop_menu_file_need_update(XfceDesktopMenu *desktop_menu)
{
    return FALSE;
}

G_MODULE_EXPORT gboolean
xfce_desktop_menu_need_update_impl(XfceDesktopMenu *desktop_menu)
{
    g_return_val_if_fail(desktop_menu != NULL, FALSE);
    
    TRACE("desktop_menu: %p", desktop_menu);
    
    if(desktop_menu_file_need_update(desktop_menu)
       || last_settings_change > desktop_menu->last_menu_gen
       || !desktop_menu->menu)
    {
        TRACE("\n\nreturning TRUE, last_settings_change=%d, last_menu_gen=%d, desktop_menu->menu=%p",
              (gint)last_settings_change, (gint)desktop_menu->last_menu_gen, desktop_menu->menu);
        return TRUE;
    }
    
    return FALSE;
}

G_MODULE_EXPORT void
xfce_desktop_menu_start_autoregen_impl(XfceDesktopMenu *desktop_menu,
                                       guint delay)
{
    g_return_if_fail(desktop_menu != NULL && desktop_menu->tim == 0);
    
    desktop_menu_file_need_update(desktop_menu);
    desktop_menu->tim = g_timeout_add(delay*1000, _menu_check_update,
                                      desktop_menu);
    timeout_handles = g_list_prepend(timeout_handles,
                                     GUINT_TO_POINTER(desktop_menu->tim));
}

G_MODULE_EXPORT void
xfce_desktop_menu_stop_autoregen_impl(XfceDesktopMenu *desktop_menu)
{
    g_return_if_fail(desktop_menu != NULL);
    
    if(desktop_menu->tim) {
        g_source_remove(desktop_menu->tim);
        timeout_handles = g_list_remove(timeout_handles,
                                        GUINT_TO_POINTER(desktop_menu->tim));
    }
    desktop_menu->tim = 0;
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
    
    xfce_desktop_menu_stop_autoregen_impl(desktop_menu);
    
    _xfce_desktop_menu_free_menudata(desktop_menu);
    g_free(desktop_menu->filename);
    g_free(desktop_menu);
}

G_MODULE_EXPORT gchar *
g_module_check_init(GModule *module)
{
    gint w, h;
    
	/* libfrapmenu registers gobject types, so we can't be removed */
	g_module_make_resident(module);
	
    gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &w, &h);
    _xfce_desktop_menu_icon_size = w;
    xfce_app_menu_item_set_icon_size(_xfce_desktop_menu_icon_size);
    
    if(dummy_icon)
        g_object_unref(G_OBJECT(dummy_icon));
    dummy_icon = xfce_inline_icon_at_size(dummy_icon_data,
                                           _xfce_desktop_menu_icon_size,
                                          _xfce_desktop_menu_icon_size);
    
    _deskmenu_icon_theme = gtk_icon_theme_get_default();
    g_signal_connect(G_OBJECT(_deskmenu_icon_theme), "changed",
                     G_CALLBACK(itheme_changed_cb), NULL);
    
    return NULL;
}
