/*
 *  menu-file.[ch] - routines for parsing an xfdeskop menu xml file
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
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __MENU_FILE_H__
#define __MENU_FILE_H__

#include "desktop-menu.h"
#include <gtk/gtkwidget.h>

gboolean desktop_menu_file_need_update(XfceDesktopMenu *desktop_menu);
gboolean desktop_menu_file_parse(XfceDesktopMenu *desktop_menu,
		const gchar *filename, GtkWidget *menu, const gchar *path,
		gboolean is_root);

#endif
