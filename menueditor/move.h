/*   move.h */

/*  Copyright (C)  Jean-François Wauthy under GNU GPL
 *
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

#ifndef __HAVE_MOVE_HEADER
#define __HAVE_MOVE_HEADER

#include "menueditor.h"


void entry_down_cb(GtkWidget *widget, gpointer data);
void entry_up_cb(GtkWidget *widget, gpointer data);

/* Workaround for gtk_tree_store_swap bug */
inline void my_tree_store_swap_down(GtkTreeStore *tree_store,
				    GtkTreeIter *a,
				    GtkTreeIter *b);
inline void my_tree_store_swap_up(GtkTreeStore *tree_store,
				  GtkTreeIter *a,
				  GtkTreeIter *b);

#endif
