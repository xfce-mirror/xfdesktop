/*  xfce4
 *  
 *  Copyright (C) 2002 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *                2004 Brian Tarricone <bjt23@cornell.edu>
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

#ifndef __DESKTOP_MENU_PRIVATE_H__
#define __DESKTOP_MENU_PRIVATE_H__

#include <time.h>

struct _XfceDesktopMenu {
	gchar *filename;
	GtkWidget *menu;  /* the menu widget itself */
	gboolean use_menu_icons;  /* show menu icons? */
	gboolean using_system_menu;   /* is there an autogenned menu in this DM? */
	gint tim;
	time_t last_menu_gen;  /* last time this menu was generated */
	GHashTable *menu_entry_hash;  /* list of entries in the menu */
	GHashTable *menu_branches;
	GList *pix_free;  /* a list of GdkPixbufs that need to be freed */
	
	GList *menufiles_watch;  /* a list of files to watch */
	time_t *menufile_mtimes;
	time_t *dentrydir_mtimes;
	time_t *legacydir_mtimes;
	
	gchar *dentry_basepath;
	GtkWidget *dentry_basemenu;
};

extern GdkPixbuf *dummy_icon;

/*< private >*/
void _xfce_desktop_menu_free_menudata(struct _XfceDesktopMenu *desktop_menu);

#endif  /* !def __DESKTOP_MENU_PRIVATE_H__ */
