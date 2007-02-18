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

#ifndef __DESKTOP_MENU_H__
#define __DESKTOP_MENU_H__

#include <glib.h>
#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

typedef struct _XfceDesktopMenu XfceDesktopMenu;
    
XfceDesktopMenu *xfce_desktop_menu_new_impl(const gchar *menu_file, gboolean deferred);
GtkWidget *xfce_desktop_menu_get_widget_impl(XfceDesktopMenu *desktop_menu);
G_CONST_RETURN gchar *xfce_desktop_menu_get_menu_file_impl(XfceDesktopMenu *desktop_menu);
gboolean xfce_desktop_menu_need_update_impl(XfceDesktopMenu *desktop_menu);
void xfce_desktop_menu_start_autoregen_impl(XfceDesktopMenu *desktop_menu, guint delay);
void xfce_desktop_menu_stop_autoregen_impl(XfceDesktopMenu *desktop_menu);
void xfce_desktop_menu_force_regen_impl(XfceDesktopMenu *desktop_menu);
void xfce_desktop_menu_set_show_icons_impl(XfceDesktopMenu *desktop_menu, gboolean show_icons);
void xfce_desktop_menu_destroy_impl(XfceDesktopMenu *desktop_menu);

extern GdkPixbuf *dummy_icon;
extern GdkPixbuf *unknown_icon;

G_END_DECLS

#endif /* !def __DESKTOP_MENU_H__ */
