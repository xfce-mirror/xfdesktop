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

static GModule *_menu_module = NULL;
static gint _menu_module_refcnt = 0;

static XfceDesktopMenu *(*xfce_desktop_menu_new_p)(const gchar *menu_file, gboolean deferred) = NULL;
GtkWidget *(*xfce_desktop_menu_get_widget)(XfceDesktopMenu *desktop_menu) = NULL;
void (*xfce_desktop_menu_populate_menu)(XfceDesktopMenu *desktop_menu, GtkWidget *menu) = NULL;
G_CONST_RETURN gchar *(*xfce_desktop_menu_get_menu_file)(XfceDesktopMenu *desktop_menu) = NULL;
gboolean (*xfce_desktop_menu_need_update)(XfceDesktopMenu *desktop_menu) = NULL;
void (*xfce_desktop_menu_start_autoregen)(XfceDesktopMenu *desktop_menu, guint delay) = NULL;
void (*xfce_desktop_menu_stop_autoregen)(XfceDesktopMenu *desktop_menu) = NULL;
void (*xfce_desktop_menu_force_regen)(XfceDesktopMenu *desktop_menu) = NULL;
void (*xfce_desktop_menu_set_show_icons)(XfceDesktopMenu *desktop_menu, gboolean show_icons) = NULL;
static void (*xfce_desktop_menu_destroy_p)(XfceDesktopMenu *desktop_menu) = NULL;

static GQuark
desktop_menu_error_quark(void)
{
    static GQuark q = 0;
    
    if(!q)
        q = g_quark_from_static_string("xfce-desktop-menu-error-quark");
    
    return q;
}

static gboolean
_setup_functions(GModule *module)
{
    if(!g_module_symbol(module, "xfce_desktop_menu_new_impl",
            (gpointer)&xfce_desktop_menu_new_p))
        return FALSE;
    if(!g_module_symbol(module, "xfce_desktop_menu_get_widget_impl",
            (gpointer)&xfce_desktop_menu_get_widget))
        return FALSE;
    if(!g_module_symbol(module, "xfce_desktop_menu_populate_menu_impl",
            (gpointer)&xfce_desktop_menu_populate_menu))
        return FALSE;
    if(!g_module_symbol(module, "xfce_desktop_menu_get_menu_file_impl",
            (gpointer)&xfce_desktop_menu_get_menu_file))
        return FALSE;
    if(!g_module_symbol(module, "xfce_desktop_menu_need_update_impl",
            (gpointer)&xfce_desktop_menu_need_update))
        return FALSE;
    if(!g_module_symbol(module, "xfce_desktop_menu_start_autoregen_impl",
            (gpointer)&xfce_desktop_menu_start_autoregen))
        return FALSE;
    if(!g_module_symbol(module, "xfce_desktop_menu_stop_autoregen_impl",
            (gpointer)&xfce_desktop_menu_stop_autoregen))
        return FALSE;
    if(!g_module_symbol(module, "xfce_desktop_menu_force_regen_impl",
            (gpointer)&xfce_desktop_menu_force_regen))
        return FALSE;
    if(!g_module_symbol(module, "xfce_desktop_menu_set_show_icons_impl",
            (gpointer)&xfce_desktop_menu_set_show_icons))
        return FALSE;
    if(!g_module_symbol(module, "xfce_desktop_menu_destroy_impl",
            (gpointer)&xfce_desktop_menu_destroy_p))
        return FALSE;
    
    return TRUE;
}

static GModule *
desktop_menu_stub_init(GError **err)
{
    GModule *module;
    gchar *filename;

    if(!g_module_supported()) {
        if(err)
            g_set_error(err, desktop_menu_error_quark(), 0,
                    "Glib was not compiled with GModule support.");
        return NULL;
    }
    
/*    filename = g_module_build_path(XFCEMODDIR, "xfce4_desktop_menu");*/
    filename = g_build_filename(XFCEMODDIR, "xfce4_desktop_menu." G_MODULE_SUFFIX, NULL);
    module = g_module_open(filename, 0);
    g_free(filename);
    
    if(!module) {
        if(err) {
            g_set_error(err, desktop_menu_error_quark(), 0,
                    "The XfceDesktopMenu module could not be loaded: %s",
                    g_module_error());
        }
        return NULL;
    }
    
    if(!_setup_functions(module)) {
        if(err) {
            g_set_error(err, desktop_menu_error_quark(), 0,
                    "The XfceDesktopMenu module is not valid: %s",
                    g_module_error());
        }
        g_module_close(module);
        return NULL;
    }
    
    return module;
}

static void
desktop_menu_stub_cleanup(GModule *module)
{
    g_module_close(module);
}

XfceDesktopMenu *
xfce_desktop_menu_new(const gchar *menu_file, gboolean deferred)
{
    GError *err = NULL;
    
    if(_menu_module_refcnt == 0)
        _menu_module = desktop_menu_stub_init(&err);
    if(!_menu_module) {
        g_critical("XfceDesktopMenu init failed (%s)",
                err ? err->message : "Unknown error");
        return NULL;
    }
    
    _menu_module_refcnt++;
    
    return xfce_desktop_menu_new_p(menu_file, deferred);
}

void
xfce_desktop_menu_destroy(XfceDesktopMenu *desktop_menu)
{
    xfce_desktop_menu_destroy_p(desktop_menu);
    
    if(--_menu_module_refcnt == 0) {
        desktop_menu_stub_cleanup(_menu_module);
        _menu_module = NULL;
    }
}
