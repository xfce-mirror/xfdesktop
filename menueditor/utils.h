/*   utils.h */

/*  Copyright (C) 2005 Jean-François Wauthy under GNU GPL
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

#ifndef __HAVE_UTILS_H
#define __HAVE_UTILS_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_MMAP
#include <sys/mman.h>
#endif
#ifndef MAP_FILE
#define MAP_FILE (0)
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gtk/gtk.h>

#include "menueditor.h"

gchar *extract_text_from_markup (const gchar *markup);
gboolean command_exists (const gchar * command);
void browse_file (GtkEntry * entry, GtkWindow * parent);
void browse_icon (GtkEntry * entry, GtkWindow * parent);
gboolean load_menu_in_treeview (const gchar * filename, MenuEditor * me);
gboolean save_treeview_in_file ();
void menueditor_tree_store_swap_up (GtkTreeStore * tree_store, GtkTreeIter * a, GtkTreeIter * b, gpointer data);
void menueditor_tree_store_swap_down (GtkTreeStore * tree_store, GtkTreeIter * a, GtkTreeIter * b, gpointer data);
void menueditor_menu_modified (MenuEditor * me);
#endif
