/*
 *  menu-dentry.[ch] - routines for gathering .desktop entry data
 *
 *  Copyright (C) 2004 Danny Milosavljevic <danny.milo@gmx.net>
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
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __MENU_DENTRY_H_
#define __MENU_DENTRY_H_

#include <glib.h>

typedef enum {
	MPATH_SIMPLE = 0,
	MPATH_SIMPLE_UNIQUE,
	MPATH_MULTI,
	MPATH_MULTI_UNIQUE
} MenuPathType;

GList *menu_dentry_parse_files(const char *basepath, MenuPathType pathtype,
		gboolean do_legacy);
gboolean menu_dentry_need_update(void);

#endif
