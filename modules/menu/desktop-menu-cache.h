/*
 *  desktop-menu-cache.[ch] - routines for caching a generated menu file
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
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _DESKTOP_MENU_CACHE_H_
#define _DESKTOP_MENU_CACHE_H_

#include <glib.h>
#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

typedef enum {
    DM_TYPE_ROOT = 0,
    DM_TYPE_MENU,
    DM_TYPE_APP,
    DM_TYPE_TITLE,
    DM_TYPE_BUILTIN,
    DM_TYPE_SEPARATOR,
    DM_N_TYPES
} DesktopMenuCacheType;

void desktop_menu_cache_init(GtkWidget *root_menu);
/* returns menu cache file if valid */
gchar *desktop_menu_cache_is_valid(const gchar *cache_file_suffix,
        GHashTable **menufile_mtimes, GHashTable **dentrydir_mtimes,
        gboolean *using_system_menu);
void desktop_menu_cache_add_entry(DesktopMenuCacheType type, const gchar *name,
        const gchar *cmd, const gchar *icon, gboolean needs_term,
        gboolean snotify, GtkWidget *parent_menu, gint position,
        GtkWidget *menu_widget);
void desktop_menu_cache_add_menufile(const gchar *menu_file);
void desktop_menu_cache_add_dentrydir(const gchar *dentry_dir);
void desktop_menu_cache_flush(const gchar *cache_file_suffix);
void desktop_menu_cache_cleanup();

G_END_DECLS

#endif
