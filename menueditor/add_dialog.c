/*   add_dialog.c */

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

#include "add_dialog.h"

/* Definitions */
struct _AddDialog
{
  GtkWidget *dialog;
  ENTRY_TYPE type;

  GtkWidget *label_name;
  GtkWidget *entry_name;
  GtkWidget *label_command;
  GtkWidget *hbox_command;
  GtkWidget *entry_command;
  GtkWidget *label_icon;
  GtkWidget *hbox_icon;
  GtkWidget *entry_icon;
  GtkWidget *check_button_snotify;
  GtkWidget *check_button_interm;
};

typedef struct _AddDialog AddDialog;

/*************/
/* Callbacks */
/*************/
static void
addentry_option_launcher_cb (GtkWidget * widget, AddDialog * add_dialog)
{
  add_dialog->type = APP;
  gtk_widget_set_sensitive (add_dialog->label_name, TRUE);
  gtk_widget_set_sensitive (add_dialog->entry_name, TRUE);
  gtk_widget_set_sensitive (add_dialog->label_command, TRUE);
  gtk_widget_set_sensitive (add_dialog->hbox_command, TRUE);
  gtk_widget_set_sensitive (add_dialog->label_icon, TRUE);
  gtk_widget_set_sensitive (add_dialog->hbox_icon, TRUE);
  gtk_widget_set_sensitive (add_dialog->check_button_snotify, TRUE);
  gtk_widget_set_sensitive (add_dialog->check_button_interm, TRUE);
}

static void
addentry_option_menu_cb (GtkWidget * widget, AddDialog * add_dialog)
{
  add_dialog->type = MENU;
  gtk_widget_set_sensitive (add_dialog->label_name, TRUE);
  gtk_widget_set_sensitive (add_dialog->entry_name, TRUE);
  gtk_widget_set_sensitive (add_dialog->label_command, FALSE);
  gtk_widget_set_sensitive (add_dialog->hbox_command, FALSE);
  gtk_widget_set_sensitive (add_dialog->label_icon, TRUE);
  gtk_widget_set_sensitive (add_dialog->hbox_icon, TRUE);
  gtk_widget_set_sensitive (add_dialog->check_button_snotify, FALSE);
  gtk_widget_set_sensitive (add_dialog->check_button_interm, FALSE);
}

static void
addentry_option_separator_cb (GtkWidget * widget, AddDialog * add_dialog)
{
  add_dialog->type = SEPARATOR;
  gtk_widget_set_sensitive (add_dialog->label_name, FALSE);
  gtk_widget_set_sensitive (add_dialog->entry_name, FALSE);
  gtk_widget_set_sensitive (add_dialog->label_command, FALSE);
  gtk_widget_set_sensitive (add_dialog->hbox_command, FALSE);
  gtk_widget_set_sensitive (add_dialog->label_icon, FALSE);
  gtk_widget_set_sensitive (add_dialog->hbox_icon, FALSE);
  gtk_widget_set_sensitive (add_dialog->check_button_snotify, FALSE);
  gtk_widget_set_sensitive (add_dialog->check_button_interm, FALSE);
}

static void
addentry_option_title_cb (GtkWidget * widget, AddDialog * add_dialog)
{
  add_dialog->type = TITLE;
  gtk_widget_set_sensitive (add_dialog->label_name, TRUE);
  gtk_widget_set_sensitive (add_dialog->entry_name, TRUE);
  gtk_widget_set_sensitive (add_dialog->label_command, FALSE);
  gtk_widget_set_sensitive (add_dialog->hbox_command, FALSE);
  gtk_widget_set_sensitive (add_dialog->label_icon, TRUE);
  gtk_widget_set_sensitive (add_dialog->hbox_icon, TRUE);
  gtk_widget_set_sensitive (add_dialog->check_button_snotify, FALSE);
  gtk_widget_set_sensitive (add_dialog->check_button_interm, FALSE);
}

static void
addentry_option_builtin_cb (GtkWidget * widget, AddDialog * add_dialog)
{
  add_dialog->type = BUILTIN;
  gtk_widget_set_sensitive (add_dialog->label_name, TRUE);
  gtk_widget_set_sensitive (add_dialog->entry_name, TRUE);
  gtk_widget_set_sensitive (add_dialog->label_command, FALSE);
  gtk_widget_set_sensitive (add_dialog->hbox_command, FALSE);
  gtk_widget_set_sensitive (add_dialog->label_icon, TRUE);
  gtk_widget_set_sensitive (add_dialog->hbox_icon, TRUE);
  gtk_widget_set_sensitive (add_dialog->check_button_snotify, FALSE);
  gtk_widget_set_sensitive (add_dialog->check_button_interm, FALSE);
}

static void
browse_command_clicked_cb (GtkWidget * widget, AddDialog * add_dialog)
{
  browse_file (GTK_ENTRY (add_dialog->entry_command), GTK_WINDOW (add_dialog->dialog));
}

static void
browse_icon_clicked_cb (GtkWidget * widget, AddDialog * add_dialog)
{
  browse_icon (GTK_ENTRY (add_dialog->entry_icon), GTK_WINDOW (add_dialog->dialog));
}

/*******************/
/* Show the dialog */
/*******************/
void
add_entry_cb (GtkWidget * widget, gpointer data)
{
  MenuEditor *me;
  AddDialog *add_dialog;

  GtkWidget *header_image;
  GtkWidget *header;

  GtkWidget *table;
  GtkWidget *label_type;
  GtkWidget *menu;
  GtkWidget *menu_item;
  GtkWidget *option_menu_type;

  GtkWidget *button_browse;

  gint response;

  me = (MenuEditor *) data;

  add_dialog = g_new0 (AddDialog, 1);

  add_dialog->dialog = gtk_dialog_new_with_buttons (_("Add menu entry"),
                                                    GTK_WINDOW (me->window),
                                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                                    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK,
                                                    GTK_RESPONSE_OK, NULL);

  table = gtk_table_new (5, 2, TRUE);

  /* Header */
  header_image = gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_LARGE_TOOLBAR);
  header = xfce_create_header_with_image (header_image, _("Add menu entry"));
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (add_dialog->dialog)->vbox), header, FALSE, FALSE, 0);

  /* Type */
  label_type = gtk_label_new (_("Type:"));

  menu = gtk_menu_new ();
  menu_item = gtk_menu_item_new_with_mnemonic (_("Launcher"));
  g_signal_connect (G_OBJECT (menu_item), "activate", G_CALLBACK (addentry_option_launcher_cb), add_dialog);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

  menu_item = gtk_menu_item_new_with_mnemonic (_("Submenu"));
  g_signal_connect (G_OBJECT (menu_item), "activate", G_CALLBACK (addentry_option_menu_cb), add_dialog);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

  menu_item = gtk_menu_item_new_with_mnemonic (_("Separator"));
  g_signal_connect (G_OBJECT (menu_item), "activate", G_CALLBACK (addentry_option_separator_cb), add_dialog);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

  menu_item = gtk_menu_item_new_with_mnemonic (_("Title"));
  g_signal_connect (G_OBJECT (menu_item), "activate", G_CALLBACK (addentry_option_title_cb), add_dialog);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

  menu_item = gtk_menu_item_new_with_mnemonic (_("Quit"));
  g_signal_connect (G_OBJECT (menu_item), "activate", G_CALLBACK (addentry_option_builtin_cb), add_dialog);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

  option_menu_type = gtk_option_menu_new ();

  gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu_type), menu);
  gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu_type), 0);
  add_dialog->type = APP;

  gtk_table_attach (GTK_TABLE (table), label_type, 0, 1, 0, 1, GTK_FILL, GTK_SHRINK, 0, 0);
  gtk_table_attach (GTK_TABLE (table), option_menu_type, 1, 2, 0, 1, GTK_FILL, GTK_SHRINK, 0, 0);

  /* Name */
  add_dialog->label_name = gtk_label_new (_("Name:"));
  add_dialog->entry_name = gtk_entry_new ();

  gtk_table_attach (GTK_TABLE (table), add_dialog->label_name, 0, 1, 1, 2, GTK_FILL, GTK_SHRINK, 0, 0);
  gtk_table_attach (GTK_TABLE (table), add_dialog->entry_name, 1, 2, 1, 2, GTK_FILL, GTK_SHRINK, 0, 0);

  /* Command */
  add_dialog->hbox_command = gtk_hbox_new (FALSE, 0);
  add_dialog->label_command = gtk_label_new (_("Command:"));
  add_dialog->entry_command = gtk_entry_new ();
  button_browse = gtk_button_new_with_label ("...");
  g_signal_connect (G_OBJECT (button_browse), "clicked", G_CALLBACK (browse_command_clicked_cb), add_dialog);
  gtk_box_pack_start (GTK_BOX (add_dialog->hbox_command), add_dialog->entry_command, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (add_dialog->hbox_command), button_browse, FALSE, FALSE, 0);

  gtk_table_attach (GTK_TABLE (table), add_dialog->label_command, 0, 1, 2, 3, GTK_FILL, GTK_SHRINK, 0, 0);
  gtk_table_attach (GTK_TABLE (table), add_dialog->hbox_command, 1, 2, 2, 3, GTK_FILL, GTK_SHRINK, 0, 0);

  /* Icon */
  add_dialog->hbox_icon = gtk_hbox_new (FALSE, 0);
  add_dialog->label_icon = gtk_label_new (_("Icon:"));
  add_dialog->entry_icon = gtk_entry_new ();
  button_browse = gtk_button_new_with_label ("...");
  g_signal_connect (G_OBJECT (button_browse), "clicked", G_CALLBACK (browse_icon_clicked_cb), add_dialog);
  gtk_box_pack_start (GTK_BOX (add_dialog->hbox_icon), add_dialog->entry_icon, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (add_dialog->hbox_icon), button_browse, FALSE, FALSE, 0);

  gtk_table_attach (GTK_TABLE (table), add_dialog->label_icon, 0, 1, 3, 4, GTK_FILL, GTK_SHRINK, 0, 0);
  gtk_table_attach (GTK_TABLE (table), add_dialog->hbox_icon, 1, 2, 3, 4, GTK_FILL, GTK_SHRINK, 0, 0);

  /* Start Notify check button */
  add_dialog->check_button_snotify = gtk_check_button_new_with_mnemonic (_("Use startup _notification"));
  gtk_table_attach (GTK_TABLE (table), add_dialog->check_button_snotify, 1, 2, 4, 5, GTK_FILL, GTK_SHRINK, 0, 0);

  /* Run in terminal check button */
  add_dialog->check_button_interm = gtk_check_button_new_with_mnemonic (_("Run in _terminal"));
  gtk_table_attach (GTK_TABLE (table), add_dialog->check_button_interm, 0, 1, 4, 5, GTK_FILL, GTK_SHRINK, 0, 0);

  gtk_table_set_row_spacings (GTK_TABLE (table), 5);
  gtk_table_set_col_spacings (GTK_TABLE (table), 5);
  gtk_container_set_border_width (GTK_CONTAINER (table), 10);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (add_dialog->dialog)->vbox), table, FALSE, FALSE, 0);

  /* Show dialog */
  gtk_window_set_default_size (GTK_WINDOW (add_dialog->dialog), 350, 100);
  gtk_widget_show_all (add_dialog->dialog);

  while ((response = gtk_dialog_run (GTK_DIALOG (add_dialog->dialog)))) {
    if (response == GTK_RESPONSE_OK) {
      GdkPixbuf *icon = NULL;
      gchar *name = NULL;
      gchar *command = NULL;
      ENTRY_TYPE type = SEPARATOR;
      gchar *option_1 = NULL;
      gchar *option_2 = NULL;
      gchar *option_3 = NULL;

      GtkTreeSelection *selection;
      GtkTreeModel *model;
      GtkTreeIter iter_new, iter_selected;
      GtkTreePath *path = NULL;

      /* Check if all required fields are filled correctly */
      switch (add_dialog->type) {
      case APP:
        if (!command_exists (gtk_entry_get_text (GTK_ENTRY (add_dialog->entry_command)))) {
          xfce_warn (_("The command doesn't exist !"));
          continue;
        }

        if ((strlen (gtk_entry_get_text (GTK_ENTRY (add_dialog->entry_name))) == 0) ||
            (strlen (gtk_entry_get_text (GTK_ENTRY (add_dialog->entry_command))) == 0)) {
          xfce_warn (_("All fields must be filled to add an item."));
          continue;
        }
        break;
      case MENU:
      case TITLE:
      case BUILTIN:
        if (strlen (gtk_entry_get_text (GTK_ENTRY (add_dialog->entry_name)))
            == 0) {
          xfce_warn (_("The 'Name' field is required."));
          continue;
        }
        break;
      default:
        break;
      }

      switch (add_dialog->type) {
      case APP:
        /* Set icon if needed */
        if (strlen (gtk_entry_get_text (GTK_ENTRY (add_dialog->entry_icon))) != 0)
          icon =
            xfce_themed_icon_load (gtk_entry_get_text (GTK_ENTRY (add_dialog->entry_icon)), ICON_SIZE);

        name = g_markup_printf_escaped (NAME_FORMAT, gtk_entry_get_text (GTK_ENTRY (add_dialog->entry_name)));
        command = g_markup_printf_escaped (COMMAND_FORMAT, gtk_entry_get_text (GTK_ENTRY (add_dialog->entry_command)));
        type = APP;
        if (G_IS_OBJECT (icon))
          option_1 = g_strdup (gtk_entry_get_text (GTK_ENTRY (add_dialog->entry_icon)));
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (add_dialog->check_button_interm)))
          option_2 = g_strdup ("true");
        else
          option_2 = g_strdup ("false");
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (add_dialog->check_button_snotify)))
          option_3 = g_strdup ("true");
        else
          option_3 = g_strdup ("false");
        break;
      case MENU:
        /* Set icon if needed */
        if (strlen (gtk_entry_get_text (GTK_ENTRY (add_dialog->entry_icon))) != 0)
          icon =
            xfce_themed_icon_load (gtk_entry_get_text (GTK_ENTRY (add_dialog->entry_icon)), ICON_SIZE);

        name = g_markup_printf_escaped (MENU_FORMAT, gtk_entry_get_text (GTK_ENTRY (add_dialog->entry_name)));
        command = g_strdup ("");
        type = MENU;
        if (G_IS_OBJECT (icon))
          option_1 = g_strdup (gtk_entry_get_text (GTK_ENTRY (add_dialog->entry_icon)));
        option_2 = g_strdup ("");
        option_3 = g_strdup ("");
        break;
      case SEPARATOR:
        name = g_markup_printf_escaped (SEPARATOR_FORMAT, _("--- separator ---"));
        command = g_strdup ("");
        option_1 = g_strdup ("");
        option_2 = g_strdup ("");
        option_3 = g_strdup ("");
        break;
      case TITLE:
        /* Set icon if needed */
        if (strlen (gtk_entry_get_text (GTK_ENTRY (add_dialog->entry_icon))) != 0)
          icon =
            xfce_themed_icon_load (gtk_entry_get_text (GTK_ENTRY (add_dialog->entry_icon)), ICON_SIZE);

        name = g_markup_printf_escaped (MENU_FORMAT, gtk_entry_get_text (GTK_ENTRY (add_dialog->entry_name)));
        command = g_strdup ("");
        if (G_IS_OBJECT (icon))
          option_1 = g_strdup (gtk_entry_get_text (GTK_ENTRY (add_dialog->entry_icon)));
        option_2 = g_strdup ("");
        option_3 = g_strdup ("");
        break;
      case BUILTIN:
        /* Set icon if needed */
        if (strlen (gtk_entry_get_text (GTK_ENTRY (add_dialog->entry_icon))) != 0)
          icon =
            xfce_themed_icon_load (gtk_entry_get_text (GTK_ENTRY (add_dialog->entry_icon)), ICON_SIZE);

        name = g_markup_printf_escaped (BUILTIN_FORMAT, gtk_entry_get_text (GTK_ENTRY (add_dialog->entry_name)));
        command = g_markup_printf_escaped (COMMAND_FORMAT, _("quit"));
        if (G_IS_OBJECT (icon))
          option_1 = g_strdup (gtk_entry_get_text (GTK_ENTRY (add_dialog->entry_icon)));
        option_2 = g_strdup ("");
        option_3 = g_strdup ("");
        break;
      default:
        break;
      }

      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (me->treeview));
      if (gtk_tree_selection_get_selected (selection, &model, &iter_selected)) {
        ENTRY_TYPE type_selected = SEPARATOR;

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
                          COLUMN_ICON, G_IS_OBJECT (icon) ? icon : dummy_icon,
                          COLUMN_NAME, name,
                          COLUMN_COMMAND, command,
                          COLUMN_HIDDEN, FALSE,
                          COLUMN_TYPE, add_dialog->type,
                          COLUMN_OPTION_1, option_1, COLUMN_OPTION_2, option_2, COLUMN_OPTION_3, option_3, -1);

      path = gtk_tree_model_get_path (model, &iter_new);
      gtk_tree_view_set_cursor (GTK_TREE_VIEW (me->treeview), path, NULL, FALSE);

      menueditor_menu_modified (me);

      if (G_IS_OBJECT (icon))
        g_object_unref (icon);
      g_free (name);
      g_free (command);
      g_free (option_1);
      g_free (option_2);
      g_free (option_3);
      gtk_tree_path_free (path);
    }
    break;
  }

  gtk_widget_destroy (add_dialog->dialog);
  g_free (add_dialog);
}
