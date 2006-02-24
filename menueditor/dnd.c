/*   dnd.c */

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

#include "menueditor.h"
#include "utils.h"

#include "dnd.h"

/* Get DnD */
void
treeview_drag_data_get_cb (GtkWidget * widget, GdkDragContext * dc,
                           GtkSelectionData * data, guint info, guint time, gpointer user_data)
{
  if (info == DND_TARGET_MENUEDITOR) {
    MenuEditor *me;

    me = (MenuEditor *) user_data;
    
    GtkTreeRowReference *ref = g_object_get_data (G_OBJECT (dc), "gtk-tree-view-source-row");
    GtkTreePath *path_source = gtk_tree_row_reference_get_path (ref);

    gtk_selection_data_set (data, gdk_atom_intern ("MENUEDITOR_ENTRY", FALSE), 8,  /* bits */
			    (gpointer) &path_source, sizeof (path_source));
  }
}

/* Receive DnD */
void
treeview_drag_data_rcv_cb (GtkWidget * widget, GdkDragContext * dc,
                           guint x, guint y, GtkSelectionData * sd, guint info, guint t, gpointer user_data)
{
  MenuEditor *me;
  GtkTreePath *path_where_insert = NULL;
  GtkTreeViewDropPosition position;
  GtkTreeModel *model;

  GdkPixbuf *icon = NULL;
  gchar *name = NULL;
  gchar *command = NULL;
  gboolean hidden = FALSE;
  ENTRY_TYPE type = SEPARATOR;
  gchar *option_1 = NULL;
  gchar *option_2 = NULL;
  gchar *option_3 = NULL;

  GtkTreeIter iter_where_insert;
  GtkTreeIter iter_new;

  /* insertion */
  ENTRY_TYPE type_where_insert;
  gboolean inserted = FALSE;

  me = (MenuEditor *) user_data;

  g_return_if_fail (sd->data);
  gtk_tree_view_get_dest_row_at_pos (GTK_TREE_VIEW (widget), x, y, &path_where_insert, &position);

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
  if (!model) {
    g_warning ("unable to get the GtkTreeModel");
    goto cleanup;
  }
  
  if (sd->target == gdk_atom_intern ("MENUEDITOR_ENTRY", FALSE)) {
    GtkTreePath *path_source;
    GtkTreeIter iter_source;
    memcpy (&path_source, sd->data, sizeof (path_source));
    
    if (!path_source) {
      g_warning ("wrong path_source");
      goto cleanup;
    }
    
    gtk_tree_model_get_iter (model, &iter_source, path_source);
    
    gtk_tree_model_get (model, &iter_source, COLUMN_ICON, &icon, COLUMN_NAME, &name,
			COLUMN_COMMAND, &command, COLUMN_HIDDEN, &hidden,
			COLUMN_TYPE, &type, COLUMN_OPTION_1, &option_1,
			COLUMN_OPTION_2, &option_2, COLUMN_OPTION_3, &option_3, -1);
    gtk_tree_path_free (path_source);
  } else if (sd->target == gdk_atom_intern ("text/plain", FALSE)) {
    /* text/plain */
    gchar *filename = NULL;
    gchar *temp = NULL;
    gchar *buf = NULL;

    XfceDesktopEntry *de = NULL;
    const char *cat[] = { "Name", "Exec", "Icon" };

    if (g_str_has_prefix (sd->data, "file://"))
      buf = g_build_filename (&(sd->data)[7], NULL);
    else if (g_str_has_prefix (sd->data, "file:"))
      buf = g_build_filename (&(sd->data)[5], NULL);
    else
      buf = g_strdup (sd->data);

    /* Remove \n at the end of filename (if present) */
    temp = strtok (buf, "\n");
    if (!temp)
      filename = g_strdup (buf);
    else if (!g_file_test (temp, G_FILE_TEST_EXISTS))
      filename = g_strndup (temp, strlen (temp) - 1);
    else
      filename = g_strdup (temp);
    g_free (buf);

    de = xfce_desktop_entry_new (filename, cat, 3);
    g_free (filename);

    if (!de) {
      g_warning ("not valid desktop data");
      goto cleanup;
    }

    xfce_desktop_entry_get_string (de, "Name", TRUE, &temp);
    name = g_markup_printf_escaped (NAME_FORMAT, temp);
    g_free (temp);

    xfce_desktop_entry_get_string (de, "Exec", TRUE, &temp);
    command = g_markup_printf_escaped (COMMAND_FORMAT, temp);
    g_free (temp);

    if (xfce_desktop_entry_get_string (de, "Icon", TRUE, &temp)) {
      icon = xfce_themed_icon_load (temp, ICON_SIZE);
      option_1 = g_strdup (temp);
    } else
      option_1 = g_strdup ("");
    g_free (temp);

    type = APP;

    option_2 = g_strdup ("false");
    option_3 = g_strdup ("false");
  } else if (sd->target == gdk_atom_intern ("application/x-desktop", FALSE)) {
    /* application/x-desktop */
    XfceDesktopEntry *de = NULL;
    const char *cat[] = { "Name", "Exec", "Icon" };
    gchar *temp = NULL;

    de = xfce_desktop_entry_new_from_data (sd->data, cat, 3);
    if (!de) {
      g_warning ("not valid desktop data");
      goto cleanup;
    }

    xfce_desktop_entry_get_string (de, "Name", TRUE, &temp);
    name = g_markup_printf_escaped (NAME_FORMAT, temp);
    g_free (temp);

    xfce_desktop_entry_get_string (de, "Exec", TRUE, &temp);
    command = g_markup_printf_escaped (COMMAND_FORMAT, temp);
    g_free (temp);

    if (xfce_desktop_entry_get_string (de, "Icon", TRUE, &temp)) {
      icon = xfce_themed_icon_load (temp, ICON_SIZE);
      option_1 = g_strdup (temp);
    } else
      option_1 = g_strdup ("");
    g_free (temp);

    type = APP;

    option_2 = g_strdup ("false");
    option_3 = g_strdup ("false");
  } else
    goto cleanup;

  /* Insert in the tree */
  gtk_tree_model_get_iter (model, &iter_where_insert, path_where_insert);
  switch (position){
  case GTK_TREE_VIEW_DROP_BEFORE:
    gtk_tree_store_insert_before (GTK_TREE_STORE (model), &iter_new, NULL, &iter_where_insert);
    inserted = TRUE;
    break;
  case GTK_TREE_VIEW_DROP_AFTER:
    gtk_tree_store_insert_after (GTK_TREE_STORE (model), &iter_new, NULL, &iter_where_insert);
    inserted = TRUE;
    break;
  case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
  case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
    gtk_tree_model_get (model, &iter_where_insert, COLUMN_TYPE, &type_where_insert, -1);
    if (type_where_insert == MENU) {
      gtk_tree_store_prepend (GTK_TREE_STORE (model), &iter_new, &iter_where_insert);
      inserted = TRUE;
    }
    break;
  }

  if (inserted) {
    gtk_tree_store_set (GTK_TREE_STORE (model), &iter_new,
			COLUMN_ICON, G_IS_OBJECT (icon) ? icon : dummy_icon,
			COLUMN_NAME, name,
			COLUMN_COMMAND, command, COLUMN_HIDDEN, hidden,
			COLUMN_TYPE, type, COLUMN_OPTION_1, option_1,
			COLUMN_OPTION_2, option_2, COLUMN_OPTION_3, option_3, -1);

    menueditor_menu_modified (me);
  }

 cleanup:
  gtk_drag_finish (dc, inserted, (dc->action == GDK_ACTION_MOVE), t);
  if (G_IS_OBJECT (icon))
    g_object_unref (icon);
  g_free (name);
  g_free (command);
  g_free (option_1);
  g_free (option_2);
  g_free (option_3);

  gtk_tree_path_free (path_where_insert);
}
