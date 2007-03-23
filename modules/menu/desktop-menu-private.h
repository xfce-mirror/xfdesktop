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

#include <gtk/gtk.h>

#include <libxfce4menu/libxfce4menu.h>

struct _XfceDesktopMenu {
	XfceMenu *xfce_menu;
	GtkWidget *menu;  /* the menu widget itself */
	
    gchar *filename;  /* file the menu is currently using */
    gboolean using_default_menu;
    
    gboolean use_menu_icons;  /* show menu icons? */
	
    gint idle_id;  /* source id for idled generation */
    
    gboolean modified;
    
#ifdef HAVE_THUNAR_VFS
    GList *monitors;
#endif
};

/*< private >*/
void _xfce_desktop_menu_free_menudata(struct _XfceDesktopMenu *desktop_menu);
void _desktop_menu_ensure_unknown_icon();
extern gint _xfce_desktop_menu_icon_size;

#endif  /* !def __DESKTOP_MENU_PRIVATE_H__ */
