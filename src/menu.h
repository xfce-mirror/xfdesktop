/*  xfce4
 *  
 *  Copyright (C) 2002 Jasper Huijsmans (huysmans@users.sourceforge.net)
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

#ifndef __XFDESKTOP_MENU_H__
#define __XFDESKTOP_MENU_H__

#include <libxfce4mcs/mcs-client.h>
#include "main.h"

void menu_init_global(McsClient *client);
void menu_cleanup_global();
void menu_init (XfceDesktop * xfdesktop);
void menu_cleanup(XfceDesktop *xfdesktop);
void menu_load_settings (XfceDesktop * xfdesktop);
void menu_force_regen (void);
void popup_menu (int button, guint32 time, XfceDesktop *xfdesktop);
void popup_windowlist (int button, guint32 time, XfceDesktop *xfdesktop);
void menu_settings_changed(const char *channel_name, McsClient *client,	McsAction action, McsSetting *setting);

#endif /* !__XFDESKTOP_MENU_H__ */
