/*
 *  Copyright (c) 2004-2007 Brian Tarricone <bjt23@cornell.edu>
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <libxfce4util/libxfce4util.h>

#include "desktop-menu-utils.h"

gchar *
xfce_desktop_get_menufile()
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
