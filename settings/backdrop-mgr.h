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

#ifndef XFDESKTOP_BACKDROP_MGR_H
#define XFDESKTOP_BACKDROP_MGR_H

typedef void (*ListMgrCb)(char *path, gpointer data);

gboolean is_backdrop_list(const char *path);
    
void create_list_file(GtkWidget *parent, ListMgrCb callback, gpointer data);

void edit_list_file(const char *path, GtkWidget *parent, ListMgrCb callback, gpointer data);

#endif /* XFDESKTOP_BACKDROP_MGR_H */

