/*  xfce4
 *  
 *  Copyright (C) 2002-2003 Jasper Huijsmans (jasper@xfce.org)
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
#include <libxfcegui4/xfce-appmenuitem.h>

#if GTK_CHECK_VERSION(2, 4, 0)
#include <gtk/gtkicontheme.h>
#else
#include <libxfce4mcs/mcs-client.h>
#endif

#include "desktop-menu-private.h"
#include "desktop-menu.h"
#include "desktop-menu-file.h"
#include "dummy_icon.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define DEFAULT_ICON_THEME "Curve"

#define EVENTMASK (ButtonPressMask|SubstructureNotifyMask|PropertyChangeMask)

/*< private >*/
GdkPixbuf *dummy_icon = NULL;
gint _xfce_desktop_menu_icon_size = 24;
static GList *timeout_handles = NULL;
static time_t last_settings_change = 0;
static gchar *cur_icon_theme = NULL;

#if GTK_CHECK_VERSION(2, 4, 0)
static GtkIconTheme *notify_itheme = NULL;

static void
itheme_changed_cb(GtkIconTheme *itheme, gpointer user_data)
{
	last_settings_change = time(NULL);
}

#else

static McsClient *notify_client = NULL;

static void
mcs_notify_cb(const char *name, const char *channel_name, McsAction action,
		McsSetting *setting, void *data)
{
	if(g_ascii_strcasecmp(channel_name, "xfce"))
		return;

	switch(action) {
		case MCS_ACTION_NEW:
		case MCS_ACTION_CHANGED:
			if(!g_ascii_strcasecmp(setting->name, "theme")) {
				xfce_set_icon_theme(setting->data.v_string);
				last_settings_change = time(NULL);
			}
			break;
		case MCS_ACTION_DELETED:
			/* We don't use this now. Perhaps revert to default? */
			break;
	}
}

static GdkFilterReturn
mcs_client_event_filter(GdkXEvent *xevent, GdkEvent *event, gpointer data)
{
	if(mcs_client_process_event(notify_client, (XEvent *)xevent))
		return GDK_FILTER_REMOVE;
	else
		return GDK_FILTER_CONTINUE;
}

static void
mcs_watch_cb(Window window, Bool is_start, long mask, void *cb_data)
{
	GdkWindow *gdkwin;

	gdkwin = gdk_window_lookup(window);

	if(is_start)
		gdk_window_add_filter(gdkwin, mcs_client_event_filter, NULL);
	else
		gdk_window_remove_filter(gdkwin, mcs_client_event_filter, NULL);
}

#endif

static gboolean
_generate_menu(XfceDesktopMenu *desktop_menu)
{
	gboolean ret = TRUE;
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
		ret = FALSE;
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
	
	newfilename = desktop_menu_file_get_menufile();
	if(!desktop_menu->filename || strcmp(desktop_menu->filename, newfilename)) {
		if(desktop_menu->filename)
			g_free(desktop_menu->filename);
		desktop_menu->filename = newfilename;
		modified = TRUE;
	} else
		g_free(newfilename);
	
	if(modified)
		_generate_menu(desktop_menu);
	
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
		g_hash_table_destroy(desktop_menu->dentrydir_mtimes);
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
	
	desktop_menu->use_menu_icons = TRUE;
	
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
	g_return_val_if_fail(desktop_menu != NULL, FALSE);
	
	TRACE("desktop_menu: %p", desktop_menu);
	
	if(desktop_menu_file_need_update(desktop_menu)
			|| (desktop_menu->using_system_menu
				&& desktop_menu_dentry_need_update(desktop_menu))
			|| last_settings_change > desktop_menu->last_menu_gen
			|| !desktop_menu->menu)
	{
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
	
	_generate_menu(desktop_menu);
}

G_MODULE_EXPORT void
xfce_desktop_menu_set_show_icons_impl(XfceDesktopMenu *desktop_menu,
		gboolean show_icons)
{
	g_return_if_fail(desktop_menu != NULL);
	
	if(desktop_menu->use_menu_icons != show_icons) {
		desktop_menu->use_menu_icons = show_icons;
		_generate_menu(desktop_menu);
	}
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
	
	if(dummy_icon)
		g_object_unref(G_OBJECT(dummy_icon));
	dummy_icon = xfce_inline_icon_at_size(dummy_icon_data,
			_xfce_desktop_menu_icon_size, _xfce_desktop_menu_icon_size);
	
#if GTK_CHECK_VERSION(2, 4, 0)
	notify_itheme = gtk_icon_theme_get_default();
	g_signal_connect(G_OBJECT(notify_itheme), "changed",
			G_CALLBACK(itheme_changed_cb), NULL);
#else
	notify_client = mcs_client_new(GDK_DISPLAY(), DefaultScreen(GDK_DISPLAY()),
			mcs_notify_cb, mcs_watch_cb, NULL);
	if(notify_client) {
		if(MCS_SUCCESS == mcs_client_add_channel(notify_client, "xfce")) {
			McsSetting *setting;
			if(MCS_SUCCESS == mcs_client_get_setting(notify_client, "theme",
					"xfce", &setting))
			{
				xfce_set_icon_theme(setting->data.v_string);
				last_settings_change = time(NULL);
				mcs_setting_free(setting);
			}
		}
	}
#endif
	
	return NULL;
}

G_MODULE_EXPORT void
g_module_unload(GModule *module)
{
	GList *l;
	
#if GTK_CHECK_VERSION(2, 4, 0)
	g_signal_handlers_disconnect_by_func(G_OBJECT(notify_itheme),
			itheme_changed_cb, NULL);
	if(cur_icon_theme)
		g_free(cur_icon_theme);
	cur_icon_theme = NULL;
#else
	if(notify_client)
		mcs_client_destroy(notify_client);
	notify_client = NULL;
#endif
	
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
