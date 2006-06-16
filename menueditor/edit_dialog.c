/*   edit_dialog.c */

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

#include "edit_dialog.h"

struct _EditDialog {
  GtkWidget *dialog;
  EntryType type;

  GtkWidget *entry_icon;
  GtkWidget *entry_command;
};

typedef struct _EditDialog EditDialog;

/*************/
/* Callbacks */
/*************/
static void
browse_command_clicked_cb (GtkWidget * widget, EditDialog * edit_dialog)
{
  browse_file (GTK_ENTRY (edit_dialog->entry_command), GTK_WINDOW (edit_dialog->dialog));
}

static void
browse_icon_clicked_cb (GtkWidget * widget, EditDialog * edit_dialog)
{
  browse_icon (GTK_ENTRY (edit_dialog->entry_icon), GTK_WINDOW (edit_dialog->dialog));
}

/***************/
/* Show dialog */
/***************/

void
edit_selection (MenuEditor *me)
{  
  EditDialog *edit_dialog;

  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter_selected;

  GdkPixbuf *icon = NULL;
  gchar *name = NULL;
  gchar *command = NULL;
  EntryType type = SEPARATOR;
  gchar *option_1 = NULL;
  gchar *option_2 = NULL;
  gchar *option_3 = NULL;
  gchar *temp = NULL;
  
  GtkWidget *header_image;
  GtkWidget *header;

  GtkWidget *table;
  GtkWidget *label_name;
  GtkWidget *entry_name = NULL;
  GtkWidget *label_command;
  GtkWidget *hbox_command;
  GtkWidget *hbox_icon;
  GtkWidget *label_icon;
  GtkWidget *button_browse;
  GtkWidget *check_button_snotify = NULL;
  GtkWidget *check_button_interm = NULL;

  gint response;

  edit_dialog = g_new0 (EditDialog, 1);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (me->treeview));

  if (!gtk_tree_selection_get_selected (selection, &model, &iter_selected)) {
    g_warning ("no entry selected !");
    return;
  }

  gtk_tree_model_get (model, &iter_selected, COLUMN_ICON, &icon,
		      COLUMN_NAME, &name,
		      COLUMN_COMMAND, &command,
		      COLUMN_TYPE, &type,
		      COLUMN_OPTION_1, &option_1,
		      COLUMN_OPTION_2, &option_2,
		      COLUMN_OPTION_3, &option_3, -1);

  temp = extract_text_from_markup (name);
  g_free (name);
  name = temp;
  temp = extract_text_from_markup (command);
  g_free (command);
  command = temp;

  /* Create dialog for editing */
  edit_dialog->dialog = gtk_dialog_new_with_buttons (_("Edit menu entry"),
						     GTK_WINDOW (me->window),
						     GTK_DIALOG_DESTROY_WITH_PARENT,
						     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);

  /* Header */
  header_image = gtk_image_new_from_stock (GTK_STOCK_JUSTIFY_FILL, GTK_ICON_SIZE_LARGE_TOOLBAR);
  header = xfce_create_header_with_image (header_image, _("Edit menu entry"));
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (edit_dialog->dialog)->vbox), header, FALSE, FALSE, 0);

  switch (type) {
  case SEPARATOR:
    xfce_info (_("Separators cannot be edited"));
    gtk_widget_destroy (edit_dialog->dialog);
    return;
  case INCLUDE_FILE:
  case INCLUDE_SYSTEM:
    break;
  case APP:
    table = gtk_table_new (4, 2, FALSE);

    /* Icon */
    hbox_icon = gtk_hbox_new (FALSE, 0);
    label_icon = gtk_label_new (_("Icon:"));
    edit_dialog->entry_icon = gtk_entry_new ();
    button_browse = gtk_button_new_with_label ("...");
    gtk_box_pack_start (GTK_BOX (hbox_icon), edit_dialog->entry_icon, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox_icon), button_browse, FALSE, FALSE, 0);
    g_signal_connect (G_OBJECT (button_browse), "clicked", G_CALLBACK (browse_icon_clicked_cb), edit_dialog);

    /* Name */
    label_name = gtk_label_new (_("Name:"));
    entry_name = gtk_entry_new ();

    /* Command */
    hbox_command = gtk_hbox_new (FALSE, 0);
    label_command = gtk_label_new (_("Command:"));
    edit_dialog->entry_command = gtk_entry_new ();
    button_browse = gtk_button_new_with_label ("...");
    gtk_box_pack_start (GTK_BOX (hbox_command), edit_dialog->entry_command, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox_command), button_browse, FALSE, FALSE, 0);
    g_signal_connect (G_OBJECT (button_browse), "clicked", G_CALLBACK (browse_command_clicked_cb), edit_dialog);

    /* Startup notification */
    check_button_snotify = gtk_check_button_new_with_mnemonic (_("Use startup _notification"));

    /* Run in terminal */
    check_button_interm = gtk_check_button_new_with_mnemonic (_("Run in _terminal"));

    gtk_table_set_row_spacings (GTK_TABLE (table), 5);
    gtk_table_set_col_spacings (GTK_TABLE (table), 5);
    gtk_container_set_border_width (GTK_CONTAINER (table), 10);

    gtk_table_attach (GTK_TABLE (table), label_name, 0, 1, 0, 1, GTK_FILL, GTK_SHRINK, 0, 0);
    gtk_table_attach (GTK_TABLE (table), entry_name, 1, 2, 0, 1, GTK_FILL, GTK_SHRINK, 0, 0);
    gtk_table_attach (GTK_TABLE (table), label_command, 0, 1, 1, 2, GTK_FILL, GTK_SHRINK, 0, 0);
    gtk_table_attach (GTK_TABLE (table), hbox_command, 1, 2, 1, 2, GTK_FILL, GTK_SHRINK, 0, 0);
    gtk_table_attach (GTK_TABLE (table), label_icon, 0, 1, 2, 3, GTK_FILL, GTK_SHRINK, 0, 0);
    gtk_table_attach (GTK_TABLE (table), hbox_icon, 1, 2, 2, 3, GTK_FILL, GTK_SHRINK, 0, 0);
    gtk_table_attach (GTK_TABLE (table), check_button_interm, 0, 1, 3, 4, GTK_FILL, GTK_SHRINK, 0, 0);
    gtk_table_attach (GTK_TABLE (table), check_button_snotify, 1, 2, 3, 4, GTK_FILL, GTK_SHRINK, 0, 0);

    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (edit_dialog->dialog)->vbox), table, FALSE, FALSE, 0);

    /* Get the current values */
    if (name)
      gtk_entry_set_text (GTK_ENTRY (entry_name), name);
    if (command)
      gtk_entry_set_text (GTK_ENTRY (edit_dialog->entry_command), command);
    if (option_1)
      gtk_entry_set_text (GTK_ENTRY (edit_dialog->entry_icon), option_1);
    if (option_2 && strcmp (option_2, "true") == 0)
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button_interm), TRUE);
    if (option_3 && strcmp (option_3, "true") == 0)
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button_snotify), TRUE);
    break;
  case TITLE:
  case MENU:
  case BUILTIN:
    table = gtk_table_new (3, 2, FALSE);

    /* Icon */
    hbox_icon = gtk_hbox_new (FALSE, 0);
    label_icon = gtk_label_new (_("Icon:"));
    edit_dialog->entry_icon = gtk_entry_new ();
    button_browse = gtk_button_new_with_label ("...");
    gtk_box_pack_start (GTK_BOX (hbox_icon), edit_dialog->entry_icon, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox_icon), button_browse, FALSE, FALSE, 0);
    g_signal_connect (G_OBJECT (button_browse), "clicked", G_CALLBACK (browse_icon_clicked_cb), edit_dialog);

    /* Name */
    label_name = gtk_label_new (_("Name:"));
    entry_name = gtk_entry_new ();

    gtk_table_set_row_spacings (GTK_TABLE (table), 5);
    gtk_table_set_col_spacings (GTK_TABLE (table), 5);
    gtk_container_set_border_width (GTK_CONTAINER (table), 10);

    gtk_table_attach (GTK_TABLE (table), label_name, 0, 1, 0, 1, GTK_FILL, GTK_SHRINK, 0, 0);
    gtk_table_attach (GTK_TABLE (table), entry_name, 1, 2, 0, 1, GTK_FILL, GTK_SHRINK, 0, 0);
    gtk_table_attach (GTK_TABLE (table), label_icon, 0, 1, 1, 2, GTK_FILL, GTK_SHRINK, 0, 0);
    gtk_table_attach (GTK_TABLE (table), hbox_icon, 1, 2, 1, 2, GTK_FILL, GTK_SHRINK, 0, 0);

    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (edit_dialog->dialog)->vbox), table, FALSE, FALSE, 0);

    /* Get the current values */
    if (name)
      gtk_entry_set_text (GTK_ENTRY (entry_name), name);
    if (option_1)
      gtk_entry_set_text (GTK_ENTRY (edit_dialog->entry_icon), option_1);

    gtk_window_set_default_size (GTK_WINDOW (edit_dialog->dialog), 200, 100);
    break;
  }

  gtk_widget_show_all (edit_dialog->dialog);

  /* Commit changes */
  while ((response = gtk_dialog_run (GTK_DIALOG (edit_dialog->dialog)))) {
    if (response == GTK_RESPONSE_OK) {
      const gchar *str_icon = NULL;

      /* Check if all required fields are filled correctly */
      switch (type) {
      case APP:
        if (!command_exists (gtk_entry_get_text (GTK_ENTRY (edit_dialog->entry_command)))) {
          xfce_warn (_("The command doesn't exist !"));
          continue;
        }

        if ((strlen (gtk_entry_get_text (GTK_ENTRY (entry_name))) == 0) ||
            (strlen (gtk_entry_get_text (GTK_ENTRY (edit_dialog->entry_command))) == 0)) {
          xfce_warn (_("All fields must be filled to add an item."));
          continue;
        }
        break;
      case MENU:
      case TITLE:
      case BUILTIN:
        if (strlen (gtk_entry_get_text (GTK_ENTRY (entry_name)))
            == 0) {
          xfce_warn (_("The 'Name' field is required."));
          continue;
        }
        break;
      default:
        break;
      }

      if (G_IS_OBJECT (icon)) {
        g_object_unref (icon);
        icon = NULL;
      }

      str_icon = gtk_entry_get_text (GTK_ENTRY (edit_dialog->entry_icon));
      /* set the new icon if there is one and it exists otherwise use the dummy one */
      if ((edit_dialog->entry_icon && strlen (str_icon) != 0)) {
        icon = xfce_themed_icon_load (str_icon, ICON_SIZE);
        if (!icon)
          icon = dummy_icon;
	g_free (option_1);
	option_1 = g_strdup (str_icon);
      }
      else {
        icon = dummy_icon;

	g_free (option_1);
	option_1 = g_strdup ("");
      }

      switch (type) {
      case APP:
	g_free (name);
	name = g_markup_printf_escaped (NAME_FORMAT, gtk_entry_get_text (GTK_ENTRY (entry_name)));
	g_free (command);
        command = g_markup_printf_escaped (COMMAND_FORMAT, gtk_entry_get_text (GTK_ENTRY (edit_dialog->entry_command)));

	g_free (option_2);
	g_free (option_3);
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check_button_interm)))
          option_2 = g_strdup ("true");
        else
          option_2 = g_strdup ("false");
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check_button_snotify)))
          option_3 = g_strdup ("true");
        else
          option_3 = g_strdup ("false");
	break;
      case MENU:
	g_free (name);
	name = g_markup_printf_escaped (MENU_FORMAT, gtk_entry_get_text (GTK_ENTRY (entry_name)));
	break;
      case TITLE:
	g_free (name);
	name = g_markup_printf_escaped (MENU_FORMAT, gtk_entry_get_text (GTK_ENTRY (entry_name)));
	break;
      case BUILTIN:
	g_free (name);
	name = g_markup_printf_escaped (BUILTIN_FORMAT, gtk_entry_get_text (GTK_ENTRY (entry_name)));
	g_free (command);
        command = g_markup_printf_escaped (COMMAND_FORMAT, _("quit")); 
	break;
      default:
	break;
      }


      gtk_tree_store_set (GTK_TREE_STORE (model), &iter_selected, COLUMN_ICON, icon,
			  COLUMN_NAME, name,
			  COLUMN_COMMAND, command,
			  COLUMN_OPTION_1, option_1,
			  COLUMN_OPTION_2, option_2,
			  COLUMN_OPTION_3, option_3, -1);

      menueditor_menu_modified (me);
    }
    break;
  }

  gtk_widget_destroy (edit_dialog->dialog);

  if (G_IS_OBJECT (icon))
    g_object_unref (icon);
  g_free (name);
  g_free (command);
  g_free (option_1);
  g_free (option_2);
  g_free (option_3);

  g_free (edit_dialog);
}

