/*  xfce4
 *  
 *  Copyright (C) 2002 Jasper Huijsmans (huysmans@users.sourceforge.net)
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

#ifndef __XFDESKTOP_MENU_H__
#define __XFDESKTOP_MENU_H__

#include "main.h"

/* where to find the current panel icon theme (if any) */
#define CHANNEL "xfce"
#define DEFAULT_ICON_THEME "Curve"

typedef enum __MenuItemTypeEnum
{
    MI_APP,
    MI_SEPARATOR,
    MI_SUBMENU,
    MI_TITLE,
    MI_BUILTIN
} MenuItemType;

typedef struct __MenuItemStruct
{
    MenuItemType type;		/* Type of Menu Item    */
    char *path;				/* itemfactory path to item */
	char *cmd;				/* shell cmd to execute */
    gboolean term;			/* execute in terminal  */
    char *icon;				/* icon to display      */
	GdkPixbuf *pix_free;	/* pointer to pixbuf to free */
} MenuItem;

extern GHashTable *menu_entry_hash;
extern gboolean is_using_system_rc;
extern gboolean use_menu_icons;

void menu_init (XfceDesktop * xfdesktop);
void menu_load_settings (XfceDesktop * xfdesktop);
void popup_menu (int button, guint32 time);
void popup_windowlist (int button, guint32 time);

#endif /* !__XFDESKTOP_MENU_H__ */
