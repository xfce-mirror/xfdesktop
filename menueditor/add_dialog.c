/* add_dialog.c */

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "menueditor.h"

#include "../modules/menu/dummy_icon.h"

#include "add_dialog.h"

/*************************/
/* Manage activated item */
/*************************/
void
addentry_option_launcher_cb (GtkWidget * widget, struct _controls_add *controls)
{
  controls->entry_type = LAUNCHER;
  gtk_widget_set_sensitive (controls->label_name, TRUE);
  gtk_widget_set_sensitive (controls->entry_name, TRUE);
  gtk_widget_set_sensitive (controls->label_command, TRUE);
  gtk_widget_set_sensitive (controls->entry_command, TRUE);
  gtk_widget_set_sensitive (controls->label_icon, TRUE);
  gtk_widget_set_sensitive (controls->entry_icon, TRUE);
  gtk_widget_set_sensitive (controls->snotify_checkbutton, TRUE);
  gtk_widget_set_sensitive (controls->term_checkbutton, TRUE);
}

void
addentry_option_submenu_cb (GtkWidget * widget, struct _controls_add *controls)
{
  controls->entry_type = SUBMENU;
  gtk_widget_set_sensitive (controls->label_name, TRUE);
  gtk_widget_set_sensitive (controls->entry_name, TRUE);
  gtk_widget_set_sensitive (controls->label_command, FALSE);
  gtk_widget_set_sensitive (controls->entry_command, FALSE);
  gtk_widget_set_sensitive (controls->label_icon, TRUE);
  gtk_widget_set_sensitive (controls->entry_icon, TRUE);
  gtk_widget_set_sensitive (controls->snotify_checkbutton, FALSE);
  gtk_widget_set_sensitive (controls->term_checkbutton, FALSE);
}

void
addentry_option_separator_cb (GtkWidget * widget, struct _controls_add *controls)
{
  controls->entry_type = SEPARATOR;
  gtk_widget_set_sensitive (controls->label_name, FALSE);
  gtk_widget_set_sensitive (controls->entry_name, FALSE);
  gtk_widget_set_sensitive (controls->label_command, FALSE);
  gtk_widget_set_sensitive (controls->entry_command, FALSE);
  gtk_widget_set_sensitive (controls->label_icon, FALSE);
  gtk_widget_set_sensitive (controls->entry_icon, FALSE);
  gtk_widget_set_sensitive (controls->snotify_checkbutton, FALSE);
  gtk_widget_set_sensitive (controls->term_checkbutton, FALSE);
}

void
addentry_option_title_cb (GtkWidget * widget, struct _controls_add *controls)
{
  controls->entry_type = TITLE;
  gtk_widget_set_sensitive (controls->label_name, TRUE);
  gtk_widget_set_sensitive (controls->entry_name, TRUE);
  gtk_widget_set_sensitive (controls->label_command, FALSE);
  gtk_widget_set_sensitive (controls->entry_command, FALSE);
  gtk_widget_set_sensitive (controls->label_icon, TRUE);
  gtk_widget_set_sensitive (controls->entry_icon, TRUE);
  gtk_widget_set_sensitive (controls->snotify_checkbutton, FALSE);
  gtk_widget_set_sensitive (controls->term_checkbutton, FALSE);
}

void
addentry_option_quit_cb (GtkWidget * widget, struct _controls_add *controls)
{
  controls->entry_type = QUIT;
  gtk_widget_set_sensitive (controls->label_name, TRUE);
  gtk_widget_set_sensitive (controls->entry_name, TRUE);
  gtk_widget_set_sensitive (controls->label_command, FALSE);
  gtk_widget_set_sensitive (controls->entry_command, FALSE);
  gtk_widget_set_sensitive (controls->label_icon, TRUE);
  gtk_widget_set_sensitive (controls->entry_icon, TRUE);
  gtk_widget_set_sensitive (controls->snotify_checkbutton, FALSE);
  gtk_widget_set_sensitive (controls->term_checkbutton, FALSE);
}

/*******************/
/* Show the dialog */
/*******************/
void
add_entry_cb (GtkWidget * widget, gpointer data)
{
  MenuEditor *me;
  GtkWidget *dialog;
  GtkWidget *table;
  GtkWidget *header;
  GtkWidget *header_image;
  gchar *header_text;
  GtkWidget *label_type;
  GtkWidget *optionmenu_type;
  GtkWidget *mitem;
  GtkWidget *menu;
  GtkWidget *label_name;
  GtkWidget *entry_name;
  GtkWidget *label_command;
  GtkWidget *button_browse;
  GtkWidget *hbox_command;
  GtkWidget *label_icon;
  GtkWidget *button_browse2;
  GtkWidget *hbox_icon;

  struct _controls_add controls;

  gint response;

  me = (MenuEditor *) data;

  dialog = gtk_dialog_new_with_buttons (_("Add menu entry"),
                                        GTK_WINDOW (me->main_window),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);

  /* set ok button as default */
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  table = gtk_table_new (5, 2, TRUE);

  /* Header */
  header_image = gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_LARGE_TOOLBAR);
  header_text = g_strdup_printf ("%s", _("Add menu entry"));
  header = xfce_create_header_with_image (header_image, header_text);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), header, FALSE, FALSE, 0);
  g_free (header_text);

  /* Type */
  label_type = gtk_label_new (_("Type:"));

  menu = gtk_menu_new ();
  mitem = gtk_menu_item_new_with_mnemonic (_("Launcher"));
  g_signal_connect ((gpointer) mitem, "activate", G_CALLBACK (addentry_option_launcher_cb), &controls);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);
  gtk_widget_show (mitem);

  mitem = gtk_menu_item_new_with_mnemonic (_("Submenu"));
  g_signal_connect ((gpointer) mitem, "activate", G_CALLBACK (addentry_option_submenu_cb), &controls);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);
  gtk_widget_show (mitem);

  mitem = gtk_menu_item_new_with_mnemonic (_("Separator"));
  g_signal_connect ((gpointer) mitem, "activate", G_CALLBACK (addentry_option_separator_cb), &controls);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);
  gtk_widget_show (mitem);

  mitem = gtk_menu_item_new_with_mnemonic (_("Title"));
  g_signal_connect ((gpointer) mitem, "activate", G_CALLBACK (addentry_option_title_cb), &controls);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);
  gtk_widget_show (mitem);

  mitem = gtk_menu_item_new_with_mnemonic (_("Quit"));
  g_signal_connect ((gpointer) mitem, "activate", G_CALLBACK (addentry_option_quit_cb), &controls);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);
  gtk_widget_show (mitem);

  optionmenu_type = gtk_option_menu_new ();

  gtk_option_menu_set_menu (GTK_OPTION_MENU (optionmenu_type), menu);
  gtk_option_menu_set_history (GTK_OPTION_MENU (optionmenu_type), 0);
  controls.entry_type = LAUNCHER;

  gtk_table_attach (GTK_TABLE (table), label_type, 0, 1, 0, 1, GTK_FILL, GTK_SHRINK, 0, 0);
  gtk_table_attach (GTK_TABLE (table), optionmenu_type, 1, 2, 0, 1, GTK_FILL, GTK_SHRINK, 0, 0);

  /* Name */
  label_name = gtk_label_new (_("Name:"));
  entry_name = gtk_entry_new ();

  gtk_table_attach (GTK_TABLE (table), label_name, 0, 1, 1, 2, GTK_FILL, GTK_SHRINK, 0, 0);
  gtk_table_attach (GTK_TABLE (table), entry_name, 1, 2, 1, 2, GTK_FILL, GTK_SHRINK, 0, 0);
  controls.label_name = label_name;
  controls.entry_name = entry_name;

  /* Command */
  hbox_command = gtk_hbox_new (FALSE, 0);
  label_command = gtk_label_new (_("Command:"));
  me->entry_command = gtk_entry_new ();
  button_browse = gtk_button_new_with_label ("...");

  g_signal_connect ((gpointer) button_browse, "clicked", G_CALLBACK (browse_command_cb), me);

  gtk_box_pack_start (GTK_BOX (hbox_command), me->entry_command, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox_command), button_browse, FALSE, FALSE, 0);

  gtk_table_attach (GTK_TABLE (table), label_command, 0, 1, 2, 3, GTK_FILL, GTK_SHRINK, 0, 0);
  gtk_table_attach (GTK_TABLE (table), hbox_command, 1, 2, 2, 3, GTK_FILL, GTK_SHRINK, 0, 0);
  controls.label_command = label_command;
  controls.entry_command = hbox_command;

  /* Icon */
  hbox_icon = gtk_hbox_new (FALSE, 0);
  label_icon = gtk_label_new (_("Icon:"));
  me->entry_icon = gtk_entry_new ();
  button_browse2 = gtk_button_new_with_label ("...");

  g_signal_connect ((gpointer) button_browse2, "clicked", G_CALLBACK (browse_icon_cb), me);

  gtk_box_pack_start (GTK_BOX (hbox_icon), me->entry_icon, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox_icon), button_browse2, FALSE, FALSE, 0);

  gtk_table_attach (GTK_TABLE (table), label_icon, 0, 1, 3, 4, GTK_FILL, GTK_SHRINK, 0, 0);
  gtk_table_attach (GTK_TABLE (table), hbox_icon, 1, 2, 3, 4, GTK_FILL, GTK_SHRINK, 0, 0);

  controls.label_icon = label_icon;
  controls.entry_icon = hbox_icon;

  /* Start Notify check button */
  controls.snotify_checkbutton = gtk_check_button_new_with_mnemonic (_("Use startup _notification"));
  gtk_table_attach (GTK_TABLE (table), controls.snotify_checkbutton, 1, 2, 4, 5, GTK_FILL, GTK_SHRINK, 0, 0);

  /* Run in terminal check button */
  controls.term_checkbutton = gtk_check_button_new_with_mnemonic (_("Run in _terminal"));
  gtk_table_attach (GTK_TABLE (table), controls.term_checkbutton, 0, 1, 4, 5, GTK_FILL, GTK_SHRINK, 0, 0);

  gtk_table_set_row_spacings (GTK_TABLE (table), 5);
  gtk_table_set_col_spacings (GTK_TABLE (table), 5);
  gtk_container_set_border_width (GTK_CONTAINER (table), 10);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), table, FALSE, FALSE, 0);

  /* Show dialog */
  gtk_window_set_default_size (GTK_WINDOW (dialog), 350, 100);

  gtk_widget_show_all (dialog);

  while ((response = gtk_dialog_run (GTK_DIALOG (dialog)))) {
    if (response == GTK_RESPONSE_OK) {
      xmlNodePtr node = NULL, root_node = NULL, selection_node = NULL;
      GtkTreeIter iter, selection_iter, parent;
      GtkTreeModel *tree_model = GTK_TREE_MODEL (me->treestore);
      GValue val = { 0, };
      gboolean ret_selection, is_menu = FALSE;
      gchar *name = NULL;
      gchar *command = NULL;
      GdkPixbuf *icon = NULL;

      /* Retrieve the root node of the xml tree */
      root_node = xmlDocGetRootElement (me->xml_menu_file);

      /* Retrieve the selected item */
      ret_selection =
        gtk_tree_selection_get_selected (gtk_tree_view_get_selection
                                         (GTK_TREE_VIEW (me->treeview)), &tree_model, &selection_iter);
      /* Check if the entry is a submenu */
      if (ret_selection) {
        gtk_tree_model_get_value (GTK_TREE_MODEL (me->treestore), &selection_iter, POINTER_COLUMN, &val);
        selection_node = g_value_get_pointer (&val);
        if (!xmlStrcmp (selection_node->name, (xmlChar *) "menu")) {
          parent = selection_iter;
          is_menu = TRUE;
        }
      }

      /* Check if all required fields are filled correctly */
      switch (controls.entry_type) {
      case LAUNCHER:
        if (!command_exists (gtk_entry_get_text (GTK_ENTRY (me->entry_command)))) {
          xfce_warn (_("The command doesn't exist !"));
          continue;
        }

        if ((strlen (gtk_entry_get_text (GTK_ENTRY (entry_name))) == 0) ||
            (strlen (gtk_entry_get_text (GTK_ENTRY (me->entry_command))) == 0)) {
          xfce_warn (_("All fields must be filled to add an item."));
          continue;
        }
        break;
      case SUBMENU:
      case TITLE:
      case QUIT:
        if (strlen (gtk_entry_get_text (GTK_ENTRY (entry_name)))
            == 0) {
          xfce_warn (_("The 'Name' field is required."));
          continue;
        }
        break;
      case SEPARATOR:
      case INCLUDE:
        break;
      }

      /* Create node */
      node = xmlNewNode (NULL, "new");

      /* Set icon if needed */
      if (me->entry_icon && (strlen (gtk_entry_get_text (GTK_ENTRY (me->entry_icon))) != 0)) {
        icon = xfce_icon_theme_load (me->icon_theme, (gchar *)
                                     gtk_entry_get_text (GTK_ENTRY (me->entry_icon)), ICON_SIZE);
        if (!icon)
          icon = xfce_inline_icon_at_size (dummy_icon_data, ICON_SIZE, ICON_SIZE);
        xmlSetProp (node, "icon", gtk_entry_get_text (GTK_ENTRY (me->entry_icon)));
      }
      else {
        icon = xfce_inline_icon_at_size (dummy_icon_data, ICON_SIZE, ICON_SIZE);
      }

      switch (controls.entry_type) {
      case LAUNCHER:
        /* Set node name */
        xmlNodeSetName (node, "app");

        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (controls.snotify_checkbutton)))
          xmlSetProp (node, "snotify", "true");

        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (controls.term_checkbutton)))
          xmlSetProp (node, "term", "yes");

        xmlSetProp (node, "name", gtk_entry_get_text (GTK_ENTRY (entry_name)));
        xmlSetProp (node, "cmd", gtk_entry_get_text (GTK_ENTRY (me->entry_command)));

        name = g_markup_printf_escaped (NAME_FORMAT, gtk_entry_get_text (GTK_ENTRY (entry_name)));
        command = g_markup_printf_escaped (COMMAND_FORMAT, gtk_entry_get_text (GTK_ENTRY (me->entry_command)));
        break;
      case SUBMENU:
        /* Set node name */
        xmlNodeSetName (node, "menu");

        xmlSetProp (node, "name", gtk_entry_get_text (GTK_ENTRY (entry_name)));
        xmlSetProp (node, "visible", "yes");


        name = g_markup_printf_escaped (MENU_FORMAT, gtk_entry_get_text (GTK_ENTRY (entry_name)));
        command = g_strdup ("");
        break;
      case SEPARATOR:
        /* Set node name */
        xmlNodeSetName (node, "separator");

        name = g_markup_printf_escaped (SEPARATOR_FORMAT, _("--- separator ---"));
        command = g_strdup ("");
        break;
      case TITLE:
        /* Set node name */
        xmlNodeSetName (node, "title");

        xmlSetProp (node, "name", gtk_entry_get_text (GTK_ENTRY (entry_name)));
        xmlSetProp (node, "visible", "yes");


        name = g_markup_printf_escaped (TITLE_FORMAT, gtk_entry_get_text (GTK_ENTRY (entry_name)));
        command = g_strdup ("");
        break;
      case INCLUDE:
        break;
      case QUIT:
        /* Set node name */
        xmlNodeSetName (node, "builtin");

        xmlSetProp (node, "name", gtk_entry_get_text (GTK_ENTRY (entry_name)));
        xmlSetProp (node, "visible", "yes");
        xmlSetProp (node, "cmd", "quit");

        name = g_markup_printf_escaped (BUILTIN_FORMAT, gtk_entry_get_text (GTK_ENTRY (entry_name)));
        command = g_markup_printf_escaped (COMMAND_FORMAT, _("quit"));
      }

      /* Append entry in the tree */
      if (!ret_selection) {
        /* Add the node to the tree */
        if (xmlAddChild (root_node, node) == NULL) {
          perror ("xmlAddChild");
          xmlFreeNode (node);
          continue;
        }
        gtk_tree_store_append (me->treestore, &iter, NULL);
      }
      else {
        if (is_menu) {
          if (xmlAddChild (selection_node, node) == NULL) {
            perror ("xmlAddChild");
            xmlFreeNode (node);
            continue;
          }
          gtk_tree_store_append (me->treestore, &iter, &parent);
          gtk_tree_view_expand_all (GTK_TREE_VIEW (me->treeview));
        }
        else {

          if (xmlAddNextSibling (selection_node, node) == NULL) {
            perror ("xmlAddNextSibling");
            xmlFreeNode (node);
            continue;
          }
          gtk_tree_store_insert_after (me->treestore, &iter, NULL, &selection_iter);
        }
      }

      gtk_tree_store_set (me->treestore, &iter,
                          ICON_COLUMN, icon, NAME_COLUMN, name, COMMAND_COLUMN, command, POINTER_COLUMN, node, -1);

      g_free (name);
      g_free (command);

      me->menu_modified = TRUE;
      gtk_widget_set_sensitive (me->file_menu_save, TRUE);
      gtk_widget_set_sensitive (me->file_menu_saveas, TRUE);
      gtk_widget_set_sensitive (me->toolbar_save, TRUE);
      break;
    }
    else {
      break;
    }
  }

  gtk_widget_hide (dialog);
}
