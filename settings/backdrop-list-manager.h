/*  xfce4
 *  
 *  Copyright (C) 2003 Jasper Huijsmans (huysmans@users.sourceforge.net)
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

#ifndef __XFDESKTOP_BACKDROP_MGR_H__
#define __XFDESKTOP_BACKDROP_MGR_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef void (*BackdropListMgrCb)(gchar *filename, gpointer data);

void backdrop_list_manager_create_list_file(GtkWidget *parent,
                                            BackdropListMgrCb callback,
                                            gpointer data);

void backdrop_list_manager_edit_list_file(const gchar *path,
                                          GtkWidget *parent,
                                          BackdropListMgrCb callback,
                                          gpointer data);

G_END_DECLS

#endif /* !__XFDESKTOP_BACKDROP_MGR_H__ */
