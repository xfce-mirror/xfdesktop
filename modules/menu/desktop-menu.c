/*  xfce4
 *  
 *  Copyright (C) 2002-2003 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *                     2003 Biju Chacko (botsie@users.sourceforge.net)
 *                     2004 Danny Milosavljevic <danny.milo@gmx.net>
 *                     2004 Brian Tarricone <bjt23@cornell.edu>
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

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#include <errno.h>

#include <X11/X.h>
#include <X11/Xlib.h>

#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <gmodule.h>
#include <glib.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include "desktop-menu-private.h"
#include "desktop-menu.h"
#include "desktop-menu-cache.h"
#include "desktop-menu-file.h"
#include "desktop-menu-dentry.h"
#include "dummy_icon.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define DEFAULT_ICON_THEME "Curve"

#define EVENTMASK (ButtonPressMask|SubstructureNotifyMask|PropertyChangeMask)

/*< private >*/
GdkPixbuf *dummy_icon = NULL;
gint _xfce_desktop_menu_icon_size = 24;
XfceIconTheme *_deskmenu_icon_theme = NULL;
static GList *timeout_handles = NULL;
static time_t last_settings_change = 0;
static gchar *cur_icon_theme = NULL;

static void
itheme_changed_cb(XfceIconTheme *itheme, gpointer user_data)
{
	last_settings_change = time(NULL);
}

static gboolean
_generate_menu(XfceDesktopMenu *desktop_menu, gboolean force)
{
	gboolean ret = TRUE;
	gchar *menu_cache_file = NULL;
	XfceKiosk *kiosk;
	gboolean user_menu = TRUE;
	
	_xfce_desktop_menu_free_menudata(desktop_menu);
	desktop_menu->menu = gtk_menu_new();
	desktop_menu->menu_entry_hash = g_hash_table_new_full(g_str_hash,
			g_str_equal, (GDestroyNotify)g_free, NULL);
	desktop_menu->menu_branches = g_hash_table_new_full(g_str_hash, g_str_equal,
			(GDestroyNotify)g_free, NULL);
	g_hash_table_insert(desktop_menu->menu_branches, g_strdup("/"),
			desktop_menu->menu);
	
	desktop_menu->menufile_mtimes = g_hash_table_new_full(g_str_hash,
			g_str_equal, (GDestroyNotify)g_free, NULL);
	desktop_menu->using_system_menu = FALSE;
	
	kiosk = xfce_kiosk_new("xfdesktop");
	user_menu = xfce_kiosk_query(kiosk, "UserMenu");
	xfce_kiosk_free(kiosk);
	
	if(!force && user_menu) {
		/* don't use the menu cache if we're in kiosk mode, as the user could
		 * potentially modify the cache file directly */
		menu_cache_file = desktop_menu_cache_is_valid(desktop_menu->cache_file_suffix,
				&desktop_menu->menufile_mtimes, &desktop_menu->dentrydir_mtimes,
				&desktop_menu->using_system_menu);
	}
	if(menu_cache_file) {
		if(!desktop_menu_file_parse(desktop_menu, menu_cache_file,
			desktop_menu->menu, "/", TRUE, TRUE))
		{
			_xfce_desktop_menu_free_menudata(desktop_menu);
			ret = FALSE;
		}
		g_free(menu_cache_file);
	} else {
		desktop_menu_cache_init(desktop_menu->menu);
		
		if(!desktop_menu_file_parse(desktop_menu, desktop_menu->filename,
				desktop_menu->menu, "/", TRUE, FALSE))
		{
			_xfce_desktop_menu_free_menudata(desktop_menu);
			ret = FALSE;
		}
		
		desktop_menu_cache_flush(desktop_menu->cache_file_suffix);
		desktop_menu_cache_cleanup();
	}
	
	desktop_menu->last_menu_gen = time(NULL);
	
	if(desktop_menu->menu_entry_hash) {
		g_hash_table_destroy(desktop_menu->menu_entry_hash);
		desktop_menu->menu_entry_hash = NULL;
	}
	if(desktop_menu->menu_branches) {
		g_hash_table_destroy(desktop_menu->menu_branches);
		desktop_menu->menu_branches = NULL;
	}
	
	return ret;
}

/* gtk_timeout handler */
static gboolean
_menu_check_update(gpointer data)
{
	XfceDesktopMenu *desktop_menu = data;
	gboolean modified = FALSE;
	gchar *newfilename = NULL;
	
	TRACE("desktop_menu: %p", desktop_menu);
	
	g_return_val_if_fail(desktop_menu != NULL, FALSE);
	
	modified = xfce_desktop_menu_need_update_impl(desktop_menu);
	
	if(desktop_menu->using_default_menu) {
		newfilename = desktop_menu_file_get_menufile();
		if(!desktop_menu->menufile_mtimes || 
                   !g_hash_table_lookup(desktop_menu->menufile_mtimes, newfilename)) {
			g_free(desktop_menu->filename);
			desktop_menu->filename = newfilename;
			modified = TRUE;
		} else
			g_free(newfilename);
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
	if(desktop_menu->menu_entry_hash)
		g_hash_table_destroy(desktop_menu->menu_entry_hash);
	if(desktop_menu->menufile_mtimes)
		g_hash_table_destroy(desktop_menu->menufile_mtimes);
	if(desktop_menu->dentrydir_mtimes)
		g_hash_table_destroy(desktop_menu->dentrydir_mtimes);
	
	desktop_menu->menu = NULL;
	desktop_menu->menu_entry_hash = NULL;
	desktop_menu->menufile_mtimes = NULL;
	desktop_menu->dentrydir_mtimes = NULL;
}

static gboolean
_generate_menu_initial(gpointer data) {
	g_return_val_if_fail(data != NULL, FALSE);
	
	_generate_menu((XfceDesktopMenu *)data, FALSE);
	
	return FALSE;
}

G_MODULE_EXPORT XfceDesktopMenu *
xfce_desktop_menu_new_impl(const gchar *menu_file, gboolean deferred)
{
	XfceDesktopMenu *desktop_menu = g_new0(XfceDesktopMenu, 1);
	gchar *p;
	
	desktop_menu->use_menu_icons = TRUE;
	
	if(menu_file)
		desktop_menu->filename = g_strdup(menu_file);
	else {
		desktop_menu->filename = desktop_menu_file_get_menufile();
		desktop_menu->using_default_menu = TRUE;
	}
	
	desktop_menu->cache_file_suffix = g_strdup(desktop_menu->filename);
	p = desktop_menu->cache_file_suffix;
	while(*p) {
		if(*p == G_DIR_SEPARATOR)
			*p = '-';
		p++;
	}
	
	if(deferred)
		g_idle_add(_generate_menu_initial, desktop_menu);
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

G_CONST_RETURN gchar *
xfce_desktop_menu_get_menu_file_impl(XfceDesktopMenu *desktop_menu)
{
	g_return_val_if_fail(desktop_menu != NULL, NULL);
	
	return desktop_menu->filename;
}

G_MODULE_EXPORT gboolean
xfce_desktop_menu_need_update_impl(XfceDesktopMenu *desktop_menu)
{
	g_return_val_if_fail(desktop_menu != NULL, FALSE);
	
	TRACE("desktop_menu: %p", desktop_menu);
	
	if(desktop_menu_file_need_update(desktop_menu)
			|| (desktop_menu->using_system_menu
				&& desktop_menu_dentry_need_update(desktop_menu))
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
xfce_desktop_menu_start_autoregen_impl(XfceDesktopMenu *desktop_menu, guint delay)
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
	xfce_desktop_menu_stop_autoregen_impl(desktop_menu);
	
	_xfce_desktop_menu_free_menudata(desktop_menu);
	if(desktop_menu->filename) {
		g_free(desktop_menu->filename);
		desktop_menu->filename = NULL;
	}
	if(desktop_menu->cache_file_suffix) {
		g_free(desktop_menu->cache_file_suffix);
		desktop_menu->cache_file_suffix = NULL;
	}
	
	g_free(desktop_menu);
}

G_MODULE_EXPORT gchar *
g_module_check_init(GModule *module)
{
	gint w, h;
	
	gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &w, &h);
	_xfce_desktop_menu_icon_size = w;
	xfce_app_menu_item_set_icon_size(_xfce_desktop_menu_icon_size);
	
	if(dummy_icon)
		g_object_unref(G_OBJECT(dummy_icon));
	dummy_icon = xfce_inline_icon_at_size(dummy_icon_data,
			_xfce_desktop_menu_icon_size, _xfce_desktop_menu_icon_size);
	
	_deskmenu_icon_theme = xfce_icon_theme_get_for_screen(NULL);
	g_signal_connect(G_OBJECT(_deskmenu_icon_theme), "changed",
			G_CALLBACK(itheme_changed_cb), NULL);
	
	return NULL;
}

G_MODULE_EXPORT void
g_module_unload(GModule *module)
{
	GList *l;
	
	if(_deskmenu_icon_theme) {
		g_object_unref(G_OBJECT(_deskmenu_icon_theme));
		_deskmenu_icon_theme = NULL;
	}
	
	if(timeout_handles) {
		for(l=timeout_handles; l; l=l->next)
			g_source_remove(GPOINTER_TO_UINT(l->data));
		g_list_free(timeout_handles);
	}
	timeout_handles = NULL;
	
	if(dummy_icon)
		g_object_unref(G_OBJECT(dummy_icon));
	dummy_icon = NULL;
}
