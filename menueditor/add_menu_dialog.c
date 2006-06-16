/*   add_menu_dialog.c */

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

#include "add_menu_dialog.h"

/* Definitions */
struct _AddMenuDialog
{
  GtkWidget *dialog;
  EntryType type;

  GtkWidget *label_source;
  GtkWidget *hbox_source;
  GtkWidget *entry_source;
  GtkWidget *label_style;
  GtkWidget *option_menu_style;
  GtkWidget *check_button_unique;
};

typedef struct _AddMenuDialog AddMenuDialog;

/*************/
/* Callbacks */
/*************/
static void
addmenu_option_file_cb (GtkWidget * widget, AddMenuDialog * add_menu_dialog)
{
  add_menu_dialog->type = INCLUDE_FILE;
  gtk_widget_set_sensitive (add_menu_dialog->hbox_source, TRUE);
  gtk_widget_set_sensitive (add_menu_dialog->label_source, TRUE);
  gtk_widget_set_sensitive (add_menu_dialog->label_style, FALSE);
  gtk_widget_set_sensitive (add_menu_dialog->option_menu_style, FALSE);
  gtk_widget_set_sensitive (add_menu_dialog->check_button_unique, FALSE);
}

static void
addmenu_option_system_cb (GtkWidget * widget, AddMenuDialog * add_menu_dialog)
{
  add_menu_dialog->type = INCLUDE_SYSTEM;
  gtk_widget_set_sensitive (add_menu_dialog->hbox_source, FALSE);
  gtk_widget_set_sensitive (add_menu_dialog->label_source, FALSE);
  gtk_widget_set_sensitive (add_menu_dialog->label_style, TRUE);
  gtk_widget_set_sensitive (add_menu_dialog->option_menu_style, TRUE);
  gtk_widget_set_sensitive (add_menu_dialog->check_button_unique, TRUE);
}

static void
browse_file_clicked_cb (GtkWidget * widget, AddMenuDialog * add_menu_dialog)
{
  browse_file (GTK_ENTRY (add_menu_dialog->entry_source), GTK_WINDOW (add_menu_dialog->dialog));
}

/*******************/
/* Show the dialog */
/*******************/
void
add_menu_cb (GtkWidget * widget, gpointer data)
{
  MenuEditor *me;
  AddMenuDialog *add_menu_dialog;

  GtkWidget *header;
  GtkWidget *table;
  GtkWidget *label_type;
  GtkWidget *menu;
  GtkWidget *menu_item;
  GtkWidget *option_menu_type;
  GtkWidget *button;

  gint response;

  me = (MenuEditor *) data;

  add_menu_dialog = g_new0 (AddMenuDialog, 1);


  add_menu_dialog->dialog = gtk_dialog_new_with_buttons (_("Add an external menu"),
                                                         GTK_WINDOW (me->window),
                                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK,
                                                         GTK_RESPONSE_OK, NULL);

  header = xfce_create_header (NULL, _("Add an external menu"));
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (add_menu_dialog->dialog)->vbox), header, FALSE, FALSE, 0);

  /* Table */
  table = gtk_table_new (4, 2, TRUE);

  /* Type */
  label_type = gtk_label_new (_("Type:"));

  add_menu_dialog->type = INCLUDE_FILE;

  menu = gtk_menu_new ();
  menu_item = gtk_menu_item_new_with_mnemonic (_("File"));
  g_signal_connect (G_OBJECT (menu_item), "activate", G_CALLBACK (addmenu_option_file_cb), add_menu_dialog);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

  menu_item = gtk_menu_item_new_with_mnemonic (_("System"));
  g_signal_connect (G_OBJECT (menu_item), "activate", G_CALLBACK (addmenu_option_system_cb), add_menu_dialog);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

  option_menu_type = gtk_option_menu_new ();

  gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu_type), menu);
  gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu_type), 0);

  gtk_table_attach (GTK_TABLE (table), label_type, 0, 1, 0, 1, GTK_FILL, GTK_SHRINK, 0, 0);
  gtk_table_attach (GTK_TABLE (table), option_menu_type, 1, 2, 0, 1, GTK_FILL, GTK_SHRINK, 0, 0);

  /* Source */
  add_menu_dialog->hbox_source = gtk_hbox_new (FALSE, 0);
  add_menu_dialog->label_source = gtk_label_new (_("Source:"));
  add_menu_dialog->entry_source = gtk_entry_new ();
  button = gtk_button_new_with_label ("...");
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (browse_file_clicked_cb), add_menu_dialog);

  gtk_box_pack_start (GTK_BOX (add_menu_dialog->hbox_source), add_menu_dialog->entry_source, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (add_menu_dialog->hbox_source), button, FALSE, FALSE, 0);

  gtk_table_attach (GTK_TABLE (table), add_menu_dialog->label_source, 0, 1, 1, 2, GTK_FILL, GTK_SHRINK, 0, 0);
  gtk_table_attach (GTK_TABLE (table), add_menu_dialog->hbox_source, 1, 2, 1, 2, GTK_FILL, GTK_SHRINK, 0, 0);

  /* Style */
  add_menu_dialog->label_style = gtk_label_new (_("Style:"));

  menu = gtk_menu_new ();
  menu_item = gtk_menu_item_new_with_mnemonic (_("Simple"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
  menu_item = gtk_menu_item_new_with_mnemonic (_("Multilevel"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

  add_menu_dialog->option_menu_style = gtk_option_menu_new ();

  gtk_option_menu_set_menu (GTK_OPTION_MENU (add_menu_dialog->option_menu_style), menu);
  gtk_option_menu_set_history (GTK_OPTION_MENU (add_menu_dialog->option_menu_style), 0);

  gtk_table_attach (GTK_TABLE (table), add_menu_dialog->label_style, 0, 1, 2, 3, GTK_FILL, GTK_SHRINK, 0, 0);
  gtk_table_attach (GTK_TABLE (table), add_menu_dialog->option_menu_style, 1, 2, 2, 3, GTK_FILL, GTK_SHRINK, 0, 0);

  /* Unique */
  add_menu_dialog->check_button_unique = gtk_check_button_new_with_mnemonic (_("_Unique entries only"));

  gtk_table_attach (GTK_TABLE (table), add_menu_dialog->check_button_unique, 1, 2, 3, 4, GTK_FILL, GTK_SHRINK, 0, 0);

  /* Table properties */
  gtk_table_set_row_spacings (GTK_TABLE (table), 5);
  gtk_table_set_col_spacings (GTK_TABLE (table), 5);
  gtk_container_set_border_width (GTK_CONTAINER (table), 10);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (add_menu_dialog->dialog)->vbox), table, FALSE, FALSE, 0);

  /* Show dialog */
  gtk_widget_set_sensitive (add_menu_dialog->label_style, FALSE);
  gtk_widget_set_sensitive (add_menu_dialog->option_menu_style, FALSE);
  gtk_widget_set_sensitive (add_menu_dialog->check_button_unique, FALSE);
  gtk_window_set_default_size (GTK_WINDOW (add_menu_dialog->dialog), 350, 100);
  gtk_widget_show_all (add_menu_dialog->dialog);

  while ((response = gtk_dialog_run (GTK_DIALOG (add_menu_dialog->dialog)))) {
    if (response == GTK_RESPONSE_OK) {
      gchar *name = NULL;
      gchar *command = NULL;
      EntryType type = INCLUDE_SYSTEM;
      gchar *option_1 = NULL;
      gchar *option_2 = NULL;
      gchar *option_3 = NULL;

      GtkTreeSelection *selection;
      GtkTreeModel *model;
      GtkTreeIter iter_new, iter_selected;
      GtkTreePath *path = NULL;


      switch (add_menu_dialog->type) {
      case INCLUDE_FILE:
        /* Test if all field are filled */
        if (strlen (gtk_entry_get_text (GTK_ENTRY (add_menu_dialog->entry_source))) == 0) {
          xfce_warn (_("The 'Source' field is required."));
          continue;
        }

        name = g_markup_printf_escaped (INCLUDE_FORMAT, _("--- include ---"));
        command =
          g_markup_printf_escaped (INCLUDE_PATH_FORMAT, gtk_entry_get_text (GTK_ENTRY (add_menu_dialog->entry_source)));
        type = INCLUDE_FILE;
        option_1 = g_strdup ("");
        option_2 = g_strdup ("");
        option_3 = g_strdup ("");

        break;
      case INCLUDE_SYSTEM:
        name = g_markup_printf_escaped (INCLUDE_FORMAT, _("--- include ---"));
        command = g_markup_printf_escaped (INCLUDE_PATH_FORMAT, _("system"));
        if (gtk_option_menu_get_history (GTK_OPTION_MENU (add_menu_dialog->option_menu_style)) == 0)
          option_1 = g_strdup ("simple");
        else
          option_1 = g_strdup ("multilevel");
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (add_menu_dialog->check_button_unique)))
          option_2 = g_strdup ("true");
        option_3 = g_strdup ("false");

        break;
      default:
        break;
      }

      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (me->treeview));
      if (gtk_tree_selection_get_selected (selection, &model, &iter_selected)) {
        EntryType type_selected = SEPARATOR;

        gtk_tree_model_get (model, &iter_selected, COLUMN_TYPE, &type_selected, -1);

        if (type_selected == MENU)
          /* Insert in the submenu */
          gtk_tree_store_prepend (GTK_TREE_STORE (model), &iter_new, &iter_selected);
        else
          /* Insert after the selected entry */
          gtk_tree_store_insert_after (GTK_TREE_STORE (model), &iter_new, NULL, &iter_selected);

      }
      else
        /* Insert at the beginning of the tree */
        gtk_tree_store_prepend (GTK_TREE_STORE (model), &iter_new, NULL);


      gtk_tree_store_set (GTK_TREE_STORE (model), &iter_new,
                          COLUMN_ICON, dummy_icon,
                          COLUMN_NAME, name,
                          COLUMN_COMMAND, command,
                          COLUMN_HIDDEN, FALSE,
                          COLUMN_TYPE, add_menu_dialog->type,
                          COLUMN_OPTION_1, option_1, COLUMN_OPTION_2, option_2, COLUMN_OPTION_3, option_3, -1);

      path = gtk_tree_model_get_path (model, &iter_new);
      gtk_tree_view_set_cursor (GTK_TREE_VIEW (me->treeview), path, NULL, FALSE);

      menueditor_menu_modified (me);

      g_free (name);
      g_free (command);
      g_free (option_1);
      g_free (option_2);
      g_free (option_3);
      gtk_tree_path_free (path);

      break;
    }
    else
      break;
  }

  gtk_widget_destroy (add_menu_dialog->dialog);
  g_free (add_menu_dialog);
}
