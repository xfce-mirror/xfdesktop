/*  xfce4
 *  
 *  Copyright (C) 2004 Brian Tarricone <bjt23@cornell.edu>
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

#include <gmodule.h>

#include "desktop-menu-stub.h"

static GModule *menu_gmod = NULL;
static gint refcnt = 0;

XfceDesktopMenu *(*xfce_desktop_menu_new)(const gchar *menu_file, gboolean deferred) = NULL;
GtkWidget *(*xfce_desktop_menu_get_widget)(XfceDesktopMenu *desktop_menu) = NULL;
G_CONST_RETURN gchar *(*xfce_desktop_menu_get_menu_file)(XfceDesktopMenu *desktop_menu) = NULL;
gboolean (*xfce_desktop_menu_need_update)(XfceDesktopMenu *desktop_menu) = NULL;
void (*xfce_desktop_menu_start_autoregen)(XfceDesktopMenu *desktop_menu, guint delay) = NULL;
void (*xfce_desktop_menu_stop_autoregen)(XfceDesktopMenu *desktop_menu) = NULL;
void (*xfce_desktop_menu_force_regen)(XfceDesktopMenu *desktop_menu) = NULL;
void (*xfce_desktop_menu_set_show_icons)(XfceDesktopMenu *desktop_menu, gboolean show_icons) = NULL;
void (*xfce_desktop_menu_destroy)(XfceDesktopMenu *desktop_menu) = NULL;

static gboolean
_setup_functions(GModule *menu_gmod)
{
	if(!g_module_symbol(menu_gmod, "xfce_desktop_menu_new_impl",
			(gpointer)&xfce_desktop_menu_new))
		return FALSE;
	if(!g_module_symbol(menu_gmod, "xfce_desktop_menu_get_widget_impl",
			(gpointer)&xfce_desktop_menu_get_widget))
		return FALSE;
	if(!g_module_symbol(menu_gmod, "xfce_desktop_menu_get_menu_file",
			(gpointer)&xfce_desktop_menu_get_menu_file))
		return FALSE;
	if(!g_module_symbol(menu_gmod, "xfce_desktop_menu_need_update_impl",
			(gpointer)&xfce_desktop_menu_need_update))
		return FALSE;
	if(!g_module_symbol(menu_gmod, "xfce_desktop_menu_start_autoregen_impl",
			(gpointer)&xfce_desktop_menu_start_autoregen))
		return FALSE;
	if(!g_module_symbol(menu_gmod, "xfce_desktop_menu_stop_autoregen_impl",
			(gpointer)&xfce_desktop_menu_stop_autoregen))
		return FALSE;
	if(!g_module_symbol(menu_gmod, "xfce_desktop_menu_force_regen_impl",
			(gpointer)&xfce_desktop_menu_force_regen))
		return FALSE;
	if(!g_module_symbol(menu_gmod, "xfce_desktop_menu_set_show_icons_impl",
			(gpointer)&xfce_desktop_menu_set_show_icons))
		return FALSE;
	if(!g_module_symbol(menu_gmod, "xfce_desktop_menu_destroy_impl",
			(gpointer)&xfce_desktop_menu_destroy))
		return FALSE;
	
	return TRUE;
}

GModule *
xfce_desktop_menu_stub_init()
{
	gchar *menu_module = NULL;
	
	if(menu_gmod && refcnt > 0) {
		refcnt++;
		return menu_gmod;
	}

	if(!g_module_supported()) {
		g_warning("%s: no GModule support, menu is disabled\n", PACKAGE);
		return NULL;
	} else {
		menu_module = g_module_build_path(XFCEMODDIR, "xfce4_desktop_menu");
		menu_gmod = g_module_open(menu_module, 0);
	}
	
	if(!menu_gmod) {
		g_warning("%s: failed to load xfce4_desktop_menu module (%s)\n",
				PACKAGE, menu_module);
	} else if(!_setup_functions(menu_gmod)) {
		g_module_close(menu_gmod);
		menu_gmod = NULL;
	}
	
	if(menu_module)
		g_free(menu_module);
	
	if(menu_gmod)
		refcnt = 1;
	
	return menu_gmod;
}

void
xfce_desktop_menu_stub_cleanup(GModule *menu_gmod)
{
	g_return_if_fail(menu_gmod != NULL && refcnt > 0);
	
	if(--refcnt <= 0) {
		g_module_close(menu_gmod);
		menu_gmod = NULL;
		refcnt = 0;
	}
		
}
