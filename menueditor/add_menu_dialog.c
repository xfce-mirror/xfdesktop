/* add_menu_dialog.c */

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

#include "add_menu_dialog.h"

#include "../modules/menu/dummy_icon.h"

/*************************************/
/* Support for adding an include tag */
/*************************************/
void
addmenu_option_file_cb (GtkWidget * widget, struct _controls_menu *controls)
{
  controls->menu_type = MENUFILE;
  gtk_widget_set_sensitive (controls->hbox_source, TRUE);
  gtk_widget_set_sensitive (controls->label_source, TRUE);
  gtk_widget_set_sensitive (controls->label_style, FALSE);
  gtk_widget_set_sensitive (controls->optionmenu_style, FALSE);
  gtk_widget_set_sensitive (controls->checkbutton_unique, FALSE);
}

void
addmenu_option_system_cb (GtkWidget * widget, struct _controls_menu *controls)
{
  controls->menu_type = SYSTEM;
  gtk_widget_set_sensitive (controls->hbox_source, FALSE);
  gtk_widget_set_sensitive (controls->label_source, FALSE);
  gtk_widget_set_sensitive (controls->label_style, TRUE);
  gtk_widget_set_sensitive (controls->optionmenu_style, TRUE);
  gtk_widget_set_sensitive (controls->checkbutton_unique, TRUE);
}

void
add_menu_cb (GtkWidget * widget, gpointer data)
{
  MenuEditor *me;
  GtkWidget *dialog;
  GtkWidget *header;
  GtkWidget *mitem;

  GtkWidget *table;
  GtkWidget *label_type;
  GtkWidget *menu;
  GtkWidget *optionmenu_type;

  struct _controls_menu controls;
  GtkWidget *button_browse;

  gint response;

  gchar *header_text;

  me = (MenuEditor *) data;

  dialog = gtk_dialog_new_with_buttons (_("Add an external menu"),
                                        GTK_WINDOW (me->main_window),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);

  /* set ok button as default */
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  header_text = g_strdup_printf ("%s", _("Add an external menu"));
  header = xfce_create_header (NULL, header_text);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), header, FALSE, FALSE, 0);
  g_free (header_text);

  /* Table */
  table = gtk_table_new (4, 2, TRUE);

  /* Type */
  label_type = gtk_label_new (_("Type:"));

  controls.menu_type = MENUFILE;

  menu = gtk_menu_new ();
  mitem = gtk_menu_item_new_with_mnemonic (_("File"));
  g_signal_connect ((gpointer) mitem, "activate", G_CALLBACK (addmenu_option_file_cb), &controls);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);
  gtk_widget_show (mitem);

  mitem = gtk_menu_item_new_with_mnemonic (_("System"));
  g_signal_connect ((gpointer) mitem, "activate", G_CALLBACK (addmenu_option_system_cb), &controls);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);
  gtk_widget_show (mitem);

  optionmenu_type = gtk_option_menu_new ();

  gtk_option_menu_set_menu (GTK_OPTION_MENU (optionmenu_type), menu);
  gtk_option_menu_set_history (GTK_OPTION_MENU (optionmenu_type), 0);

  gtk_table_attach (GTK_TABLE (table), label_type, 0, 1, 0, 1, GTK_FILL, GTK_SHRINK, 0, 0);
  gtk_table_attach (GTK_TABLE (table), optionmenu_type, 1, 2, 0, 1, GTK_FILL, GTK_SHRINK, 0, 0);

  /* Source */
  controls.hbox_source = gtk_hbox_new (FALSE, 0);
  controls.label_source = gtk_label_new (_("Source:"));
  me->entry_command = gtk_entry_new ();
  button_browse = gtk_button_new_with_label ("...");

  g_signal_connect ((gpointer) button_browse, "clicked", G_CALLBACK (browse_command_cb), me);

  gtk_box_pack_start (GTK_BOX (controls.hbox_source), me->entry_command, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (controls.hbox_source), button_browse, FALSE, FALSE, 0);

  gtk_table_attach (GTK_TABLE (table), controls.label_source, 0, 1, 1, 2, GTK_FILL, GTK_SHRINK, 0, 0);
  gtk_table_attach (GTK_TABLE (table), controls.hbox_source, 1, 2, 1, 2, GTK_FILL, GTK_SHRINK, 0, 0);

  /* Style */
  controls.label_style = gtk_label_new (_("Style:"));

  menu = gtk_menu_new ();
  mitem = gtk_menu_item_new_with_mnemonic (_("Simple"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);
  gtk_widget_show (mitem);
  mitem = gtk_menu_item_new_with_mnemonic (_("Multilevel"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);
  gtk_widget_show (mitem);

  controls.optionmenu_style = gtk_option_menu_new ();

  gtk_option_menu_set_menu (GTK_OPTION_MENU (controls.optionmenu_style), menu);
  gtk_option_menu_set_history (GTK_OPTION_MENU (controls.optionmenu_style), 0);

  gtk_table_attach (GTK_TABLE (table), controls.label_style, 0, 1, 2, 3, GTK_FILL, GTK_SHRINK, 0, 0);
  gtk_table_attach (GTK_TABLE (table), controls.optionmenu_style, 1, 2, 2, 3, GTK_FILL, GTK_SHRINK, 0, 0);

  /* Unique */
  controls.checkbutton_unique = gtk_check_button_new_with_mnemonic (_("_Unique entries only"));

  gtk_table_attach (GTK_TABLE (table), controls.checkbutton_unique, 1, 2, 3, 4, GTK_FILL, GTK_SHRINK, 0, 0);

  /* Table properties */
  gtk_table_set_row_spacings (GTK_TABLE (table), 5);
  gtk_table_set_col_spacings (GTK_TABLE (table), 5);
  gtk_container_set_border_width (GTK_CONTAINER (table), 10);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), table, FALSE, FALSE, 0);

  /* Show dialog */
  gtk_widget_set_sensitive (controls.label_style, FALSE);
  gtk_widget_set_sensitive (controls.optionmenu_style, FALSE);
  gtk_widget_set_sensitive (controls.checkbutton_unique, FALSE);

  gtk_window_set_default_size (GTK_WINDOW (dialog), 350, 100);

  gtk_widget_show_all (dialog);

  while ((response = gtk_dialog_run (GTK_DIALOG (dialog)))) {
    if (response == GTK_RESPONSE_OK) {
      GdkPixbuf *icon = NULL;
      xmlNodePtr node = NULL, root_node = NULL, selection_node = NULL;
      GtkTreeIter iter, selection_iter, parent;
      GtkTreeModel *tree_model = GTK_TREE_MODEL (me->treestore);
      GValue val = { 0, };
      gboolean ret_selection, is_menu = FALSE;
      gchar *name = NULL;
      gchar *source = NULL;

      /* use the dummy icon for same height as the others entries */
      icon = xfce_inline_icon_at_size (dummy_icon_data, ICON_SIZE, ICON_SIZE);

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

      if (controls.menu_type == MENUFILE) {
        /* Test if all field are filled */
        if (strlen (gtk_entry_get_text (GTK_ENTRY (me->entry_command))) == 0) {
          xfce_warn (_("The 'Source' field is required."));
          continue;
        }

        /* Create node */
        node = xmlNewNode (NULL, "include");

        xmlSetProp (node, "type", "file");
        xmlSetProp (node, "src", gtk_entry_get_text (GTK_ENTRY (me->entry_command)));

        name = menueditor_markup_printf_escaped (INCLUDE_FORMAT, _("--- include ---"));
        source = menueditor_markup_printf_escaped (INCLUDE_PATH_FORMAT, gtk_entry_get_text (GTK_ENTRY (me->entry_command)));
      }

      if (controls.menu_type == SYSTEM) {
        /* Create node */
        node = xmlNewNode (NULL, "include");

        xmlSetProp (node, "type", "system");

        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (controls.checkbutton_unique)))
          xmlSetProp (node, "unique", "true");

        if (gtk_option_menu_get_history (GTK_OPTION_MENU (controls.optionmenu_style)) != 0)
          xmlSetProp (node, "style", "multilevel");

        name = menueditor_markup_printf_escaped (INCLUDE_FORMAT, _("--- include ---"));
        source = menueditor_markup_printf_escaped (INCLUDE_PATH_FORMAT, _("system"));
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
                          ICON_COLUMN, icon, NAME_COLUMN, name, COMMAND_COLUMN, source, POINTER_COLUMN, node, -1);

      me->menu_modified = TRUE;
      gtk_widget_set_sensitive (me->file_menu_save, TRUE);
      gtk_widget_set_sensitive (me->file_menu_saveas, TRUE);
      gtk_widget_set_sensitive (me->toolbar_save, TRUE);

      g_free (name);
      g_free (source);

      break;
    }
    else {
      break;
    }
  }

  gtk_widget_hide (dialog);
}
