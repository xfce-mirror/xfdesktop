/*   undo.h */

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

#ifndef __HAVE_UNDO_H
#define __HAVE_UNDO_H

#include "menueditor.h"

GSList* undo_list;

struct undo_list_element{
  enum {DELETE, ADD, MOVE} action;
  GtkTreeIter target;
  GtkTreeIter previous;
};

void undo_add_action(struct undo_list_element element);
void undo_cb(GtkWidget* widget, gpointer data);

#endif
