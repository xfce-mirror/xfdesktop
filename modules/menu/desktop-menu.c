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

#include <libxfce4util/debug.h>
#include <libxfce4util/i18n.h>
#include <libxfce4util/util.h>
#include <libxfcegui4/libxfcegui4.h>
#include <libxfcegui4/xgtkicontheme.h>
#include <libxfcegui4/xfce-appmenuitem.h>

#include "desktop-menu-private.h"
#include "desktop-menu.h"
#include "desktop-menu-file.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define DEFAULT_ICON_THEME "Curve"

#define EVENTMASK (ButtonPressMask|SubstructureNotifyMask|PropertyChangeMask)

/*< private >*/
gint _xfce_desktop_menu_icon_size = 24;
static GList *timeout_handles = NULL;
static GtkIconTheme *notify_itheme = NULL;
static time_t last_theme_change = 0;
static gchar *cur_icon_theme = NULL;

static void
itheme_changed_cb(GtkIconTheme *itheme, gpointer user_data)
{
	last_theme_change = time(NULL);
}

static gboolean
_generate_menu(XfceDesktopMenu *desktop_menu)
{
	TRACE("dummy");

	_xfce_desktop_menu_free_menudata(desktop_menu);
	desktop_menu->menu = gtk_menu_new();
	desktop_menu->menu_entry_hash = g_hash_table_new_full(g_str_hash,
			g_str_equal, (GDestroyNotify)g_free, NULL);
	desktop_menu->menu_branches = g_hash_table_new_full(g_str_hash, g_str_equal,
			(GDestroyNotify)g_free, NULL);
	g_hash_table_insert(desktop_menu->menu_branches, g_strdup("/"),
			desktop_menu->menu);
	if(!desktop_menu_file_parse(desktop_menu, desktop_menu->filename,
			desktop_menu->menu, "/", TRUE))
	{
		_xfce_desktop_menu_free_menudata(desktop_menu);
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
	
    return TRUE;
}

/* gtk_timeout handler */
static gboolean
_menu_check_update(gpointer data)
{
	XfceDesktopMenu *desktop_menu = data;
	
	TRACE("desktop_menu: %p", desktop_menu);
	
	g_return_val_if_fail(desktop_menu != NULL, FALSE);
	
	if(xfce_desktop_menu_need_update_impl(desktop_menu))
		return _generate_menu(desktop_menu);
	
	return TRUE;
}

/*< private >*/
void
_xfce_desktop_menu_free_menudata(XfceDesktopMenu *desktop_menu)
{
	GList *l;
	TRACE("dummy");
	if(desktop_menu->menu)
		gtk_widget_destroy(desktop_menu->menu);
	if(desktop_menu->menu_entry_hash)
		g_hash_table_destroy(desktop_menu->menu_entry_hash);
	if(desktop_menu->menufiles_watch) {
		for(l=desktop_menu->menufiles_watch; l; l=l->next)
			g_free(l->data);
		g_list_free(desktop_menu->menufiles_watch);
	}
	if(desktop_menu->menufile_mtimes)
		g_free(desktop_menu->menufile_mtimes);
	if(desktop_menu->dentrydir_mtimes)
		g_free(desktop_menu->dentrydir_mtimes);
	if(desktop_menu->legacydir_mtimes)
		g_free(desktop_menu->legacydir_mtimes);
	
	desktop_menu->menu = NULL;
	desktop_menu->menu_entry_hash = NULL;
	desktop_menu->menufiles_watch = NULL;
	desktop_menu->menufile_mtimes = NULL;
	desktop_menu->dentrydir_mtimes = NULL;
	desktop_menu->legacydir_mtimes = NULL;
}

static gint
_calc_icon_size()
{
	static guchar icon_sizes[] = { 128, 96, 72, 64, 48, 36, 32, 24, 22, 16, 12, 0 };
	gint i, icon_size = 128;
	GtkWidget *w;
	GtkStyle *style;
	gint width, height;
	PangoContext *pctx;
	PangoLayout *playout;
	PangoFontDescription *pfdesc;
	
	/* determine widget height */
	w = gtk_label_new("foo");
	gtk_widget_set_name(w, "xfdesktopmenu");
	gtk_widget_show(w);
	style = gtk_rc_get_style(w);
	pfdesc = style->font_desc;
	
	pctx = gtk_widget_get_pango_context(w);
	pango_context_set_font_description(pctx, pfdesc);
	playout = pango_layout_new(pctx);
	pango_layout_get_pixel_size(playout, &width, &height);
	g_object_unref(G_OBJECT(pctx));
	g_object_unref(G_OBJECT(playout));
	gtk_widget_destroy(w);
	
	/* warning: lame hack alert */
	height += 3;

	/* figure out an ideal icon size */
	for(i=0; icon_sizes[i]; i++) {
		if(icon_sizes[i] >= height)
			icon_size = icon_sizes[i];
		else
			break;
	}
	
	return icon_size;
}

static gboolean
_generate_menu_initial(gpointer data) {
	g_return_val_if_fail(data != NULL, FALSE);
	
	_generate_menu((XfceDesktopMenu *)data);
	
	return FALSE;
}

G_MODULE_EXPORT XfceDesktopMenu *
xfce_desktop_menu_new_impl(const gchar *menu_file, gboolean deferred)
{
	XfceDesktopMenu *desktop_menu = g_new0(XfceDesktopMenu, 1);
	TRACE("dummy");
	if(menu_file)
		desktop_menu->filename = g_strdup(menu_file);
	
	if(deferred)
		g_idle_add(_generate_menu_initial, desktop_menu);
	else {
		if(!_generate_menu(desktop_menu)) {
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

G_MODULE_EXPORT gboolean
xfce_desktop_menu_need_update_impl(XfceDesktopMenu *desktop_menu)
{
	gboolean modified = FALSE;
	
	g_return_val_if_fail(desktop_menu != NULL, FALSE);
	
	TRACE("desktop_menu: %p", desktop_menu);
	
	if(gtk_major_version == 2 && gtk_minor_version < 4) {
		GtkSettings *settings;
		gchar *theme = NULL;
		settings = gtk_settings_get_default();
		g_object_get(G_OBJECT(settings), "gtk-icon-theme-name", &theme, NULL);
		if(theme && *theme && !cur_icon_theme) {
			cur_icon_theme = theme;
			last_theme_change = time(NULL);
		} else if(theme && *theme && strcmp(theme, cur_icon_theme)) {
			g_free(cur_icon_theme);
			cur_icon_theme = theme;
			last_theme_change = time(NULL);
		}
	}
	
	if(desktop_menu_file_need_update(desktop_menu) ||
			last_theme_change > desktop_menu->last_menu_gen ||
			!desktop_menu->menu)
	{
		modified = TRUE;
	}
	
	return modified;
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
	
	_generate_menu(desktop_menu);
}

G_MODULE_EXPORT void
xfce_desktop_menu_destroy_impl(XfceDesktopMenu *desktop_menu)
{
	g_return_if_fail(desktop_menu != NULL);
	TRACE("dummy");
	xfce_desktop_menu_stop_autoregen_impl(desktop_menu);
	
	_xfce_desktop_menu_free_menudata(desktop_menu);
	if(desktop_menu->filename)
		g_free(desktop_menu->filename);
	desktop_menu->filename = NULL;
	
	g_free(desktop_menu);
}

G_MODULE_EXPORT gchar *
g_module_check_init(GModule *module)
{
	_xfce_desktop_menu_icon_size = _calc_icon_size();
	xfce_app_menu_item_set_icon_size(_xfce_desktop_menu_icon_size);
	
	if(gtk_major_version >= 2
			|| (gtk_major_version == 2 && gtk_minor_version >= 4))
	{
		notify_itheme = gtk_icon_theme_get_default();
		g_signal_connect(G_OBJECT(notify_itheme), "changed",
				G_CALLBACK(itheme_changed_cb), NULL);
	}
	
	return NULL;
}

G_MODULE_EXPORT void
g_module_unload(GModule *module)
{
	GList *l;
	
	if(notify_itheme && (gtk_major_version >= 2
			|| (gtk_major_version == 2 && gtk_minor_version >= 4)))
	{
		g_signal_handlers_disconnect_by_func(G_OBJECT(notify_itheme),
				itheme_changed_cb, NULL);
		if(cur_icon_theme)
			g_free(cur_icon_theme);
		cur_icon_theme = NULL;
	}
	
	if(timeout_handles) {
		for(l=timeout_handles; l; l=l->next)
			g_source_remove(GPOINTER_TO_UINT(l->data));
		g_list_free(timeout_handles);
	}
	timeout_handles = NULL;
}
