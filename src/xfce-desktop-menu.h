/*
 *  xfdesktop - xfce4's desktop manager
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

#ifndef _XFCE_DESKTOP_MENU_H_
#define _XFCE_DESKTOP_MENU_H_

#include "xfce-desktop.h"
#include <xfconf/xfconf.h>

G_BEGIN_DECLS

typedef struct _XfceDesktopMenu XfceDesktopMenu;

XfceDesktopMenu *xfce_desktop_menu_new(gboolean deferred);
void xfce_desktop_menu_populate_menu(XfceDesktopMenu *desktop_menu,
                                          GtkWidget *menu);
GtkWidget *xfce_desktop_menu_get_widget(XfceDesktopMenu *desktop_menu);
void xfce_desktop_menu_force_regen(XfceDesktopMenu *desktop_menu);
void xfce_desktop_menu_set_show_icons(XfceDesktopMenu *desktop_menu,
                                           gboolean show_icons);
void xfce_desktop_menu_destroy(XfceDesktopMenu *desktop_menu);


G_END_DECLS

#endif
