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

#include <libxfce4mcs/mcs-client.h>
#include <libxfce4util/debug.h>
#include <libxfce4util/i18n.h>
#include <libxfce4util/util.h>
#include <libxfcegui4/libxfcegui4.h>
#include <libxfcegui4/xgtkicontheme.h>
#include <libxfcegui4/xfce-appmenuitem.h>

//#include "main.h"
#include "desktop-menu-private.h"
#include "desktop-menu.h"
#include "desktop-menu-file.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* where to find the current panel icon theme (if any) */
#define CHANNEL "xfce"
#define DEFAULT_ICON_THEME "Curve"

#define EVENTMASK (ButtonPressMask|SubstructureNotifyMask|PropertyChangeMask)

static GList *timeout_handles = NULL;
static McsClient *client = NULL;
static time_t last_theme_change = 0;

static gint
_calc_icon_size()
{
	static guchar icon_sizes[] = {
			12, 16, 22, 24, 32, 36, 48, 64, 72, 96, 128, 192, 0
	};
	gint i, icon_size = -1;
	GtkWidget *tmp;
	GtkStyle *style;
	PangoFontDescription *pfdesc;
	gint totheight;
	
	/* determine widget height */
	tmp = gtk_label_new("foo");
	gtk_widget_set_name(tmp, "xfdesktopmenu");
	gtk_widget_show(tmp);
	style = gtk_rc_get_style(tmp);
	pfdesc = style->font_desc;
	totheight = PANGO_PIXELS(pango_font_description_get_size(pfdesc));
	totheight += 12;  /* FIXME: fudge factor */
	gtk_widget_destroy(tmp);

	/* figure out an ideal icon size */
	for(i=0; icon_sizes[i]; i++) {
		if(icon_sizes[i] < totheight)
			icon_size = icon_sizes[i];
		else
			break;
	}
	
	return icon_size;
}

static GdkFilterReturn
client_event_filter1(GdkXEvent * xevent, GdkEvent * event, gpointer data)
{
	if(mcs_client_process_event (client, (XEvent *)xevent))
		return GDK_FILTER_REMOVE;
	else
		return GDK_FILTER_CONTINUE;
}

static void
mcs_watch_cb(Window window, Bool is_start, long mask, void *cb_data)
{
	GdkWindow *gdkwin;

	gdkwin = gdk_window_lookup (window);

	if(is_start)
		gdk_window_add_filter (gdkwin, client_event_filter1, NULL);
	else
		gdk_window_remove_filter (gdkwin, client_event_filter1, NULL);
}


static void
mcs_notify_cb(const gchar *name, const gchar *channel_name, McsAction action,
              McsSetting *setting, void *cb_data)
{
	if(strcasecmp(channel_name, CHANNEL) || !setting)
		return;
			
	if((action==MCS_ACTION_NEW || action==MCS_ACTION_CHANGED) &&
			!strcmp(setting->name, "theme") && setting->type==MCS_TYPE_STRING)
	{
		gchar *origin = g_strdup_printf("%s:%d", __FILE__, __LINE__);
		gtk_settings_set_string_property(gtk_settings_get_default(),
				"gtk-icon-theme-name", setting->data.v_string, origin);
		g_free(origin);
		last_theme_change = time(NULL);
	}
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
	if(desktop_menu->pix_free) {
		for(l=desktop_menu->pix_free; l; l=l->next)
			g_object_unref(G_OBJECT(l->data));
		g_list_free(desktop_menu->pix_free);
	}
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
	desktop_menu->pix_free = NULL;
	desktop_menu->menufiles_watch = NULL;
	desktop_menu->menufile_mtimes = NULL;
	desktop_menu->dentrydir_mtimes = NULL;
	desktop_menu->legacydir_mtimes = NULL;
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
	g_return_val_if_fail(desktop_menu != NULL, FALSE);
	
	TRACE("desktop_menu: %p", desktop_menu);
	if(desktop_menu_file_need_update(desktop_menu) ||
			last_theme_change > desktop_menu->last_menu_gen ||
			!desktop_menu->menu)
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
	/* track icon theme changes (from the panel) */
    if(!client) {
        Display *dpy = GDK_DISPLAY();
        int screen = XDefaultScreen(dpy);
        
        if(!mcs_client_check_manager(dpy, screen, "xfce-mcs-manager"))
            g_warning("%s: mcs manager not running\n", PACKAGE);
        client = mcs_client_new(dpy, screen, mcs_notify_cb, mcs_watch_cb, NULL);
        if(client)
            mcs_client_add_channel(client, CHANNEL);
    }
	
	return NULL;
}

G_MODULE_EXPORT void
g_module_unload(GModule *module)
{
	GList *l;
	
	if(timeout_handles) {
		for(l=timeout_handles; l; l=l->next)
			g_source_remove(GPOINTER_TO_UINT(l->data));
		g_list_free(timeout_handles);
	}
	timeout_handles = NULL;
	
	if(client)
		mcs_client_destroy(client);
	client = NULL;
}
