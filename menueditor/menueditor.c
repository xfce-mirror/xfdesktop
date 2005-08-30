/*   menueditor.c */

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <libxfce4util/i18n.h>

#include "../common/xfdesktop-common.h"
#include "utils.h"
#include "menueditor.h"

#include "about_dialog.h"
#include "add_dialog.h"
#include "add_menu_dialog.h"
#include "dnd.h"
#include "edit_dialog.h"

/* Prototypes */
static void create_main_window (MenuEditor * me);
static void open_default_menu_cb (GtkWidget * widget, gpointer data);
static void save_menu_cb (GtkWidget * widget, gpointer data);

/* Global */
GdkPixbuf *dummy_icon = NULL;

/***************/
/* Entry point */
/***************/
int
main (int argc, char *argv[])
{
  MenuEditor *me;

  me = g_new0 (MenuEditor, 1);

  if (argc > 1 && (!strcmp (argv[1], "--version") || !strcmp (argv[1], "-V"))) {
    g_print ("\tThis is xfce4-menueditor version %s for Xfce %s\n", VERSION, xfce_version_string ());
    g_print ("\tbuilt with GTK+-%d.%d.%d, linked with GTK+-%d.%d.%d.\n",
             GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION, gtk_major_version, gtk_minor_version,
             gtk_micro_version);
    return 0;
  }

  xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

  gtk_init (&argc, &argv);

  dummy_icon = xfce_inline_icon_at_size (dummy_icon_data, ICON_SIZE, ICON_SIZE);
  create_main_window (me);
  gtk_widget_show_all (me->window);

  if (argc > 1) {
    if (g_file_test (argv[1], G_FILE_TEST_EXISTS))
      load_menu_in_treeview (argv[1], me);
    else {
      gchar *error_message;

      error_message = g_strdup_printf (_("File %s doesn't exist !"), argv[1]);

      xfce_err (error_message);

      g_free (error_message);
    }
  }
  else
    open_default_menu_cb (NULL, me);

  gtk_main ();

  g_object_unref (dummy_icon);
  g_free (me->menu_file_name);
  g_free (me);

  return 0;
}

/*************/
/* Callbacks */
/*************/

/* =========== */
/* Main window */
/* =========== */
static gboolean
delete_main_window_cb (GtkWidget * widget, GdkEvent * event, gpointer data)
{
  MenuEditor *me;

  me = (MenuEditor *) data;

  if (me->menu_modified) {
    gint response = GTK_RESPONSE_NONE;

    response =
      xfce_message_dialog (GTK_WINDOW (me->window), "Question",
                           GTK_STOCK_DIALOG_QUESTION,
                           _
                           ("Do you want to save before closing the menu ?"),
                           _
                           ("You have modified the menu, do you want to save it before quitting ?"),
                           XFCE_CUSTOM_STOCK_BUTTON,
                           _("Forget modifications"), GTK_STOCK_QUIT,
                           GTK_RESPONSE_NO, GTK_STOCK_CANCEL,
                           GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE, GTK_RESPONSE_YES, NULL);

    switch (response) {
    case GTK_RESPONSE_YES:
      save_menu_cb (widget, data);
      break;
    case GTK_RESPONSE_CANCEL:
      return TRUE;
    }
  }

  gtk_main_quit ();

  return FALSE;
}

static void
confirm_quit_cb (GtkWidget * widget, gpointer data)
{
  delete_main_window_cb (widget, NULL, data);
}

/* ========== */
/* Icon Theme */
/* ========== */
static gboolean
icon_theme_update_foreach_func (GtkTreeModel * model, GtkTreePath * path, GtkTreeIter * iter, gpointer data)
{
  MenuEditor *me;
  GdkPixbuf *icon;
  gchar *icon_name = NULL;
  ENTRY_TYPE type;

  me = (MenuEditor *) data;

  gtk_tree_model_get (model, iter, COLUMN_ICON, &icon, COLUMN_TYPE, &type, COLUMN_OPTION_1, &icon_name, -1);

  if (icon_name && strlen (icon_name) > 0 && type != SEPARATOR && type != INCLUDE_FILE && type != INCLUDE_SYSTEM) {
    if (icon)
      g_object_unref (icon);

    icon = xfce_icon_theme_load (me->icon_theme, icon_name, ICON_SIZE);
    if (icon) {
      gtk_tree_store_set (GTK_TREE_STORE (model), iter, COLUMN_ICON, icon, -1);
      g_object_unref (icon);
    }
    else
      gtk_tree_store_set (GTK_TREE_STORE (model), iter, COLUMN_ICON, dummy_icon, -1);
  }

  g_free (icon_name);

  return FALSE;
}

static void
icon_theme_changed_cb (XfceIconTheme * icon_theme, gpointer user_data)
{
  MenuEditor *me;
  GtkTreeModel *model;

  me = (MenuEditor *) user_data;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (me->treeview));

  if (model)
    gtk_tree_model_foreach (model, &icon_theme_update_foreach_func, me);
}

/* ======== */
/* Treeview */
/* ======== */
static void
treeview_cursor_changed_cb (GtkTreeView * treeview, gpointer data)
{
  MenuEditor *me;
  GtkTreeIter iter;
  GtkTreeModel *tree_model;

  me = (MenuEditor *) data;

  tree_model = gtk_tree_view_get_model (GTK_TREE_VIEW (me->treeview));
  if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (me->treeview)), &tree_model, &iter)) {
    gtk_widget_set_sensitive (me->toolbar_add, TRUE);
    gtk_widget_set_sensitive (me->toolbar_del, TRUE);
    gtk_widget_set_sensitive (me->toolbar_up, TRUE);
    gtk_widget_set_sensitive (me->toolbar_down, TRUE);

    gtk_widget_set_sensitive (me->menu_item_edit_add_menu, TRUE);
    gtk_widget_set_sensitive (me->menu_item_edit_add, TRUE);
    gtk_widget_set_sensitive (me->menu_item_edit_del, TRUE);
    gtk_widget_set_sensitive (me->menu_item_edit_up, TRUE);
    gtk_widget_set_sensitive (me->menu_item_edit_down, TRUE);
  }
}

static gboolean
treeview_button_pressed_cb (GtkTreeView * treeview, GdkEventButton * event, gpointer data)
{
  MenuEditor *me;

  me = (MenuEditor *) data;

  /* Right click draws the context menu */
  if ((event->button == 3) && (event->type == GDK_BUTTON_PRESS)) {
    GtkTreePath *path;

    if (gtk_tree_view_get_path_at_pos (treeview, event->x, event->y, &path, NULL, NULL, NULL)) {
      GtkTreeSelection *selection;
      GtkTreeModel *model;
      GtkTreeIter iter;
      ENTRY_TYPE type;

      selection = gtk_tree_view_get_selection (treeview);
      model = gtk_tree_view_get_model (treeview);

      gtk_tree_selection_unselect_all (selection);
      gtk_tree_selection_select_path (selection, path);
      gtk_tree_model_get_iter (model, &iter, path);
      gtk_tree_model_get (model, &iter, COLUMN_TYPE, &type, -1);

      if (type == SEPARATOR)
	gtk_widget_set_sensitive (me->menu_item_popup_edit, FALSE);
      else
	gtk_widget_set_sensitive (me->menu_item_popup_edit, TRUE);

      gtk_menu_popup (GTK_MENU (me->menu_popup), NULL, NULL, NULL, NULL, event->button,
		      gtk_get_current_event_time ());

      return TRUE;
    }

    gtk_tree_path_free (path);
  }
  return FALSE;
}

static void
visible_column_toggled_cb (GtkCellRendererToggle * toggle, gchar * str_path, gpointer data)
{
  MenuEditor *me;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreePath *path;

  me = (MenuEditor *) data;

  /* Retrieve current iter */
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (me->treeview));
  path = gtk_tree_path_new_from_string (str_path);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_path_free (path);

  if (gtk_cell_renderer_toggle_get_active (toggle))
    gtk_tree_store_set (GTK_TREE_STORE (model), &iter, COLUMN_HIDDEN, FALSE, -1);
  else
    gtk_tree_store_set (GTK_TREE_STORE (model), &iter, COLUMN_HIDDEN, TRUE, -1);

  /* Modified ! */
  me->menu_modified = TRUE;
  gtk_widget_set_sensitive (me->menu_item_file_save, TRUE);
  gtk_widget_set_sensitive (me->toolbar_save, TRUE);
}

void
treeview_activate_cb (GtkWidget * widget, GtkTreePath * path, GtkTreeViewColumn * col, gpointer data)
{
  edit_selection (data);
}

/* ========= */
/* File menu */
/* ========= */
static void
new_menu_cb (GtkWidget * widget, gpointer data)
{
  MenuEditor *me;
  GtkWidget *dialog;
  GtkWidget *filesel_dialog;

  me =(MenuEditor *) data;

  /* Is there any opened menu ? */
  if (me->menu_file_name) {
    dialog = gtk_message_dialog_new (GTK_WINDOW (me->window),
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_QUESTION,
                                     GTK_BUTTONS_YES_NO, _("Are you sure you want to close the current menu?"));

    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_NO) {
      gtk_widget_destroy (dialog);
      return;
    }

    gtk_widget_destroy (dialog);
  }

  /* Current menu has been modified, saving ? */
  if (me->menu_modified) {
    dialog = gtk_message_dialog_new (GTK_WINDOW (me->window),
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_QUESTION,
                                     GTK_BUTTONS_YES_NO, _("Do you want to save before closing the file?"));

    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_YES)
      save_treeview_in_file (me);

    gtk_widget_destroy (dialog);
  }

  filesel_dialog =
    gtk_file_chooser_dialog_new (_("Select command"), GTK_WINDOW (me->window),
				 GTK_FILE_CHOOSER_ACTION_SAVE,
				 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				 GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);

  if (gtk_dialog_run (GTK_DIALOG (filesel_dialog)) == GTK_RESPONSE_ACCEPT) {
    GtkTreeModel *model;
    gchar *filename = NULL;

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (me->treeview));
    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filesel_dialog));
    gtk_tree_store_clear (GTK_TREE_STORE (model));
    me->menu_file_name = g_strdup (filename);
    g_free (filename);
  }

  gtk_widget_destroy (filesel_dialog);
}

static void
open_menu_cb (GtkWidget * widget, gpointer data)
{
  MenuEditor *me;
  GtkWidget *filesel_dialog;

  me = (MenuEditor *) data;

  /* Check if there is no other file opened */
  if (me->menu_file_name && me->menu_modified) {
    gint response;

    response =
      xfce_message_dialog (GTK_WINDOW (me->window), _("Question"),
                           GTK_STOCK_DIALOG_QUESTION,
                           _("Do you want to save before opening an other menu ?"),
                           NULL, XFCE_CUSTOM_BUTTON,
                           _("Ignore modifications"), GTK_RESPONSE_NO,
                           GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE, GTK_RESPONSE_YES, NULL);
    switch (response) {
    case GTK_RESPONSE_CANCEL:
      return;
    case GTK_RESPONSE_YES:
      save_menu_cb (widget, data);
    }
  }

  filesel_dialog =
    gtk_file_chooser_dialog_new (_("Open menu file"), GTK_WINDOW (me->window),
				 GTK_FILE_CHOOSER_ACTION_OPEN,
				 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				 GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
  
  gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (filesel_dialog), "menu.xml");

  if (gtk_dialog_run (GTK_DIALOG (filesel_dialog)) == GTK_RESPONSE_ACCEPT) {
    gchar *filename = NULL;

    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filesel_dialog));

    if (me->menu_file_name) {
      GtkTreeModel *model;

      model = gtk_tree_view_get_model (GTK_TREE_VIEW (me->treeview));
      gtk_tree_store_clear (GTK_TREE_STORE (model));
      g_free (me->menu_file_name);
      me->menu_file_name = NULL;
    }

    load_menu_in_treeview (filename, me);

    g_free (filename);
  }

  gtk_widget_destroy (filesel_dialog);
}

static void
open_default_menu_cb (GtkWidget * widget, gpointer data)
{
  MenuEditor *me;
  gchar *home_menu;

  me = (MenuEditor *) data;

  /* Check if there is no other file opened */
  if (me->menu_file_name != NULL && me->menu_modified) {
    gint response;

    response =
      xfce_message_dialog (GTK_WINDOW (me->window), _("Question"),
                           GTK_STOCK_DIALOG_QUESTION,
                           _("Do you want to save before opening the default menu ?"),
                           NULL, XFCE_CUSTOM_BUTTON,
                           _("Ignore modifications"), GTK_RESPONSE_NO,
                           GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE, GTK_RESPONSE_YES, NULL);
    switch (response) {
    case GTK_RESPONSE_CANCEL:
      return;
    case GTK_RESPONSE_YES:
      save_menu_cb (widget, data);
    }
  }

  if (me->menu_file_name != NULL) {
    GtkTreeStore *treestore;

    treestore = GTK_TREE_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (me->treeview)));
    gtk_tree_store_clear (treestore);
    g_free (me->menu_file_name);
    me->menu_file_name = NULL;
  }

  home_menu = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, "xfce4/desktop/menu.xml", TRUE);

  if (g_file_test (home_menu, G_FILE_TEST_EXISTS)) {
    me->menu_file_name = g_strdup (home_menu);
    load_menu_in_treeview (me->menu_file_name, me);
  }
  else {
    gchar *filename = xfce_desktop_get_menufile ();

    load_menu_in_treeview (filename, me);

    g_free (me->menu_file_name);
    me->menu_file_name = g_strdup (home_menu);
    save_treeview_in_file (me);

    g_free (filename);
  }
}

static void
save_menu_cb (GtkWidget * widget, gpointer data)
{
  MenuEditor *me;

  me = (MenuEditor *) data;
  save_treeview_in_file (me);
}

static void
saveas_menu_cb (GtkWidget * widget, gpointer data)
{
  MenuEditor *me;
  GtkWidget *filesel_dialog;

  me = (MenuEditor *) data;

  filesel_dialog =
    gtk_file_chooser_dialog_new (_("Save as..."), GTK_WINDOW (me->window),
				 GTK_FILE_CHOOSER_ACTION_SAVE,
				 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				 GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);

  gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (filesel_dialog), "menu.xml");

  if (gtk_dialog_run (GTK_DIALOG (filesel_dialog)) == GTK_RESPONSE_ACCEPT) {
    gchar *filename = NULL;

    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filesel_dialog));

    if (strcmp (filename, me->menu_file_name) == 0)
      save_treeview_in_file (me);
    else {
      gchar *window_title;

      g_free (me->menu_file_name);
      me->menu_file_name = g_strdup (filename);

      save_treeview_in_file (me);

      /* Set window's title */
      window_title = g_strdup_printf ("Xfce4-MenuEditor - %s", me->menu_file_name);
      gtk_window_set_title (GTK_WINDOW (me->window), window_title);

      g_free (window_title);
    }

    g_free (filename);

  }

  gtk_widget_destroy (filesel_dialog);
}

static void
close_menu_cb (GtkWidget * widget, gpointer data)
{
  MenuEditor *me;
  GtkTreeModel *model;

  me = (MenuEditor *) data;

  if (me->menu_modified) {
    gint response;

    response =
      xfce_message_dialog (GTK_WINDOW (me->window), _("Question"),
                           GTK_STOCK_DIALOG_QUESTION,
                           _("Do you want to save before closing the menu ?"),
                           NULL, XFCE_CUSTOM_BUTTON,
                           _("Ignore modifications"), GTK_RESPONSE_NO,
                           GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE, GTK_RESPONSE_YES, NULL);
    switch (response) {
    case GTK_RESPONSE_CANCEL:
      return;
    case GTK_RESPONSE_YES:
      save_treeview_in_file (me);
    }
  }

  g_free (me->menu_file_name);
  me->menu_file_name = NULL;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (me->treeview));
  gtk_tree_store_clear (GTK_TREE_STORE (model));

  me->menu_modified = FALSE;

  gtk_widget_set_sensitive (me->toolbar_close, FALSE);
  gtk_widget_set_sensitive (me->toolbar_save, FALSE);
  gtk_widget_set_sensitive (me->toolbar_add, FALSE);
  gtk_widget_set_sensitive (me->toolbar_del, FALSE);
  gtk_widget_set_sensitive (me->toolbar_up, FALSE);
  gtk_widget_set_sensitive (me->toolbar_down, FALSE);
  gtk_widget_set_sensitive (me->toolbar_expand, FALSE);
  gtk_widget_set_sensitive (me->toolbar_collapse, FALSE);

  gtk_widget_set_sensitive (me->menu_item_file_close, FALSE);
  gtk_widget_set_sensitive (me->menu_item_file_save, FALSE);
  gtk_widget_set_sensitive (me->menu_item_file_saveas, FALSE);

  gtk_widget_set_sensitive (me->menu_item_edit, FALSE);

  gtk_widget_set_sensitive (me->treeview, FALSE);
}

/* ========= */
/* Edit menu */
/* ========= */
static void 
popup_menu_edit_cb (GtkWidget * widget, gpointer data)
{
  edit_selection (data);
}

static void
delete_entry_cb (GtkWidget * widget, gpointer data)
{
  MenuEditor *me;
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;

  me = (MenuEditor *) data;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (me->treeview));
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (me->treeview));
  if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
    gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);

    /* Modified ! */
    me->menu_modified = TRUE;
    gtk_widget_set_sensitive (me->menu_item_file_save, TRUE);
    gtk_widget_set_sensitive (me->toolbar_save, TRUE);
  }

  gtk_widget_set_sensitive (me->toolbar_del, FALSE);
  gtk_widget_set_sensitive (me->toolbar_up, FALSE);
  gtk_widget_set_sensitive (me->toolbar_down, FALSE);

  gtk_widget_set_sensitive (me->menu_item_edit_del, FALSE);
  gtk_widget_set_sensitive (me->menu_item_edit_up, FALSE);
  gtk_widget_set_sensitive (me->menu_item_edit_down, FALSE);
}

static void
entry_up_cb (GtkWidget * widget, gpointer data)
{
  MenuEditor *me;
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreeIter iter_prev;
  GtkTreePath *path_prev = NULL;

  me = (MenuEditor *) data;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (me->treeview));
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (me->treeview));

  /* Retrieve current iter */
  if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    return;

  path_prev = gtk_tree_model_get_path (model, &iter);

  /* Retrieve previous iter */
  if (gtk_tree_path_prev (path_prev)) {
    gtk_tree_model_get_iter (model, &iter_prev, path_prev);
    menueditor_tree_store_swap_up (GTK_TREE_STORE (model), &iter, &iter_prev, me);

    menueditor_menu_modified (me);
  }
  else if (gtk_tree_path_up (path_prev)) {
    if (gtk_tree_path_get_depth (path_prev) > 0 && gtk_tree_model_get_iter (model, &iter_prev, path_prev)) {
      /* Move into the parent menu ? */
      GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW (me->window),
                                                  GTK_DIALOG_DESTROY_WITH_PARENT,
                                                  GTK_MESSAGE_QUESTION,
                                                  GTK_BUTTONS_YES_NO,
                                                  _("Do you want to move the item into the parent menu?"));
      if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_YES) {
        menueditor_tree_store_swap_up (GTK_TREE_STORE (model), &iter, &iter_prev, me);
        menueditor_menu_modified (me);
      }
      gtk_widget_destroy (dialog);
    }
  }

  gtk_tree_path_free (path_prev);
}

static void
entry_down_cb (GtkWidget * widget, gpointer data)
{
  MenuEditor *me;
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreeIter iter_next;
  GtkTreePath *path_next = NULL;

  me = (MenuEditor *) data;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (me->treeview));
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (me->treeview));

  /* Retrieve current iter */
  if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    return;

  path_next = gtk_tree_model_get_path (model, &iter);
  gtk_tree_path_next (path_next);

  /* Retrieve previous iter */
  if (gtk_tree_model_get_iter (model, &iter_next, path_next)) {
    ENTRY_TYPE type;

    gtk_tree_model_get_iter (model, &iter_next, path_next);
    gtk_tree_model_get (model, &iter_next, COLUMN_TYPE, &type, -1);

    if (type == MENU) {
      GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW (me->window),
                                                  GTK_DIALOG_DESTROY_WITH_PARENT,
                                                  GTK_MESSAGE_QUESTION,
                                                  GTK_BUTTONS_YES_NO,
                                                  _("Do you want to move the item into the submenu?"));
      if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_YES) {
        /* move into the submenu */
        /* gtk should implement a method to move a element across the tree ! */
        GtkTreeIter iter_new;
        GtkTreePath *path_new;

        GdkPixbuf *icon = NULL;
        gchar *name = NULL;
        gchar *command = NULL;
        gboolean hidden = FALSE;
        ENTRY_TYPE type = SEPARATOR;
        gchar *option_1 = NULL;
        gchar *option_2 = NULL;
        gchar *option_3 = NULL;


        /* Move the element in the tree */
        gtk_tree_model_get (model, &iter, COLUMN_ICON, &icon, COLUMN_NAME, &name,
                            COLUMN_COMMAND, &command, COLUMN_HIDDEN, &hidden,
                            COLUMN_TYPE, &type, COLUMN_OPTION_1, &option_1,
                            COLUMN_OPTION_2, &option_2, COLUMN_OPTION_3, &option_3, -1);
        gtk_tree_store_prepend (GTK_TREE_STORE (model), &iter_new, &iter_next);
        gtk_tree_store_set (GTK_TREE_STORE (model), &iter_new,
                            COLUMN_ICON, icon, COLUMN_NAME, name,
                            COLUMN_COMMAND, command, COLUMN_HIDDEN, hidden,
                            COLUMN_TYPE, type, COLUMN_OPTION_1, option_1,
                            COLUMN_OPTION_2, option_2, COLUMN_OPTION_3, option_3, -1);

        gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);

        if (G_IS_OBJECT (icon))
          g_object_unref (icon);

        g_free (name);
        g_free (command);
        g_free (option_1);
        g_free (option_2);
        g_free (option_3);

        /* Expand the whole tree */
        gtk_tree_view_expand_all (GTK_TREE_VIEW (me->treeview));

        /* Set selection on the new iter */
        path_new = gtk_tree_model_get_path (model, &iter_new);

        gtk_tree_view_set_cursor (GTK_TREE_VIEW (me->treeview), path_new, NULL, FALSE);
        gtk_tree_path_free (path_new);

        menueditor_menu_modified (me);
      }
      else {
        /* move after the menu */
        menueditor_tree_store_swap_down (GTK_TREE_STORE (model), &iter, &iter_next, me);
        menueditor_menu_modified (me);
      }
      gtk_widget_destroy (dialog);
    }
    else {
      /* next isn't a menu */
      menueditor_tree_store_swap_down (GTK_TREE_STORE (model), &iter, &iter_next, me);
      menueditor_menu_modified (me);
    }
  }

  gtk_tree_path_free (path_next);
}

static void
collapse_tree_cb (GtkWidget * widget, gpointer data)
{
  MenuEditor *me;

  me = (MenuEditor *) data;

  gtk_tree_view_collapse_all (GTK_TREE_VIEW (me->treeview));
}

static void
expand_tree_cb (GtkWidget * widget, gpointer data)
{
  MenuEditor *me;

  me = (MenuEditor *) data;

  gtk_tree_view_expand_all (GTK_TREE_VIEW (me->treeview));
}

/***************/
/* Main window */
/***************/
static void
create_main_window (MenuEditor * me)
{
  GtkAccelGroup *accel_group;
  GdkPixbuf *icon;

  /* Widgets */
  GtkWidget *vbox;
  GtkWidget *menubar;
  GtkWidget *separator;
  GtkWidget *menu_file;
  GtkWidget *menu_edit;
  GtkWidget *menu_help;

  GtkWidget *toolbar;
  GtkWidget *image;

  /* Treeview */
  GtkWidget *scrolledwindow;
  GtkTreeStore *treestore;
  GtkCellRenderer *cell_name, *cell_command, *cell_hidden, *cell_icon;
  GtkTreeViewColumn *column_name, *column_command, *column_hidden;

  /* Status bar */
  GtkWidget *statusbar;

  /* DnD */
  GtkTargetEntry gte[] = { {"MENUEDITOR_ENTRY", GTK_TARGET_SAME_WIDGET, DND_TARGET_MENUEDITOR},
			   {"text/plain", 0, DND_TARGET_TEXT_PLAIN},
			   {"application/x-desktop", 0, DND_TARGET_APP_DESKTOP} };

  accel_group = gtk_accel_group_new ();
  me->icon_theme = xfce_icon_theme_get_for_screen (NULL);

  /* Window */
  me->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (me->window), "Xfce4-MenuEditor");
  gtk_window_set_default_size (GTK_WINDOW (me->window), 600, 450);

  /* Set default icon */
  icon = xfce_icon_theme_load (me->icon_theme, "xfce4-menueditor", 48);
  gtk_window_set_icon (GTK_WINDOW (me->window), icon);
  g_object_unref (icon);

  /* Main vbox */
  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (me->window), vbox);

  /* Popup menu */
  me->menu_popup = gtk_menu_new ();
  me->menu_item_popup_edit = gtk_image_menu_item_new_with_mnemonic (_("Edit"));
  gtk_container_add (GTK_CONTAINER (me->menu_popup), me->menu_item_popup_edit);
  separator = gtk_separator_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (me->menu_popup), separator);
  me->menu_item_popup_add = gtk_image_menu_item_new_from_stock (GTK_STOCK_ADD, accel_group);
  gtk_container_add (GTK_CONTAINER (me->menu_popup), me->menu_item_popup_add);
  me->menu_item_popup_addmenu = gtk_image_menu_item_new_with_mnemonic (_("Add an external menu"));
  gtk_container_add (GTK_CONTAINER (me->menu_popup), me->menu_item_popup_addmenu);
  me->menu_item_popup_del = gtk_image_menu_item_new_from_stock (GTK_STOCK_REMOVE, accel_group);
  gtk_container_add (GTK_CONTAINER (me->menu_popup), me->menu_item_popup_del);
  separator = gtk_separator_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (me->menu_popup), separator);
  me->menu_item_popup_up = gtk_image_menu_item_new_from_stock (GTK_STOCK_GO_UP, accel_group);
  gtk_container_add (GTK_CONTAINER (me->menu_popup), me->menu_item_popup_up);
  me->menu_item_popup_down = gtk_image_menu_item_new_from_stock (GTK_STOCK_GO_DOWN, accel_group);
  gtk_container_add (GTK_CONTAINER (me->menu_popup), me->menu_item_popup_down);
  gtk_widget_show_all (me->menu_popup);

  /* Menu bar */
  /* ======== */
  menubar = gtk_menu_bar_new ();
  gtk_box_pack_start (GTK_BOX (vbox), menubar, FALSE, FALSE, 0);

  /* File menu */
  me->menu_item_file = gtk_image_menu_item_new_with_mnemonic (_("_File"));
  gtk_container_add (GTK_CONTAINER (menubar), me->menu_item_file);
  menu_file = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (me->menu_item_file), menu_file);

  me->menu_item_file_new = gtk_image_menu_item_new_from_stock (GTK_STOCK_NEW, accel_group);
  gtk_container_add (GTK_CONTAINER (menu_file), me->menu_item_file_new);
  me->menu_item_file_open = gtk_image_menu_item_new_from_stock (GTK_STOCK_OPEN, accel_group);
  gtk_container_add (GTK_CONTAINER (menu_file), me->menu_item_file_open);
  me->menu_item_file_default = gtk_image_menu_item_new_with_mnemonic (_("Open default menu"));
  gtk_container_add (GTK_CONTAINER (menu_file), me->menu_item_file_default);
  me->menu_item_file_save = gtk_image_menu_item_new_from_stock (GTK_STOCK_SAVE, accel_group);
  gtk_container_add (GTK_CONTAINER (menu_file), me->menu_item_file_save);
  me->menu_item_file_saveas = gtk_image_menu_item_new_from_stock (GTK_STOCK_SAVE_AS, accel_group);
  gtk_container_add (GTK_CONTAINER (menu_file), me->menu_item_file_saveas);
  me->menu_item_file_close = gtk_image_menu_item_new_from_stock (GTK_STOCK_CLOSE, accel_group);
  gtk_container_add (GTK_CONTAINER (menu_file), me->menu_item_file_close);
  separator = gtk_separator_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menu_file), separator);
  me->menu_item_file_exit = gtk_image_menu_item_new_from_stock (GTK_STOCK_QUIT, accel_group);
  gtk_container_add (GTK_CONTAINER (menu_file), me->menu_item_file_exit);

  /* Edit menu */
  me->menu_item_edit = gtk_image_menu_item_new_with_mnemonic (_("_Edit"));
  gtk_container_add (GTK_CONTAINER (menubar), me->menu_item_edit);
  menu_edit = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (me->menu_item_edit), menu_edit);
  me->menu_item_edit_add = gtk_image_menu_item_new_from_stock (GTK_STOCK_ADD, accel_group);
  gtk_container_add (GTK_CONTAINER (menu_edit), me->menu_item_edit_add);
  me->menu_item_edit_add_menu = gtk_image_menu_item_new_with_mnemonic (_("Add an external menu..."));
  gtk_container_add (GTK_CONTAINER (menu_edit), me->menu_item_edit_add_menu);
  me->menu_item_edit_del = gtk_image_menu_item_new_from_stock (GTK_STOCK_REMOVE, accel_group);
  gtk_container_add (GTK_CONTAINER (menu_edit), me->menu_item_edit_del);
  separator = gtk_separator_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menu_edit), separator);
  me->menu_item_edit_up = gtk_image_menu_item_new_from_stock (GTK_STOCK_GO_UP, accel_group);
  gtk_container_add (GTK_CONTAINER (menu_edit), me->menu_item_edit_up);
  me->menu_item_edit_down = gtk_image_menu_item_new_from_stock (GTK_STOCK_GO_DOWN, accel_group);
  gtk_container_add (GTK_CONTAINER (menu_edit), me->menu_item_edit_down);

  /* Help menu */
  me->menu_item_help = gtk_image_menu_item_new_with_mnemonic (_("_Help"));
  gtk_container_add (GTK_CONTAINER (menubar), me->menu_item_help);
  menu_help = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (me->menu_item_help), menu_help);
  me->menu_item_help_about = gtk_menu_item_new_with_mnemonic (_("_About..."));
  gtk_container_add (GTK_CONTAINER (menu_help), me->menu_item_help_about);

  /* Toolbar */
  toolbar = gtk_toolbar_new ();
  gtk_box_pack_start (GTK_BOX (vbox), toolbar, FALSE, FALSE, 0);
  gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_ICONS);

  image = gtk_image_new_from_stock (GTK_STOCK_NEW, GTK_ICON_SIZE_LARGE_TOOLBAR);
  me->toolbar_new =
    gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON, NULL, "",
                                _("Create a new Xfce4 menu file"), NULL, image, GTK_SIGNAL_FUNC (new_menu_cb), me);
  image = gtk_image_new_from_stock (GTK_STOCK_OPEN, GTK_ICON_SIZE_LARGE_TOOLBAR);
  me->toolbar_open =
    gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON, NULL, "",
                                _("Open an Xfce4 menu file"), NULL, image, GTK_SIGNAL_FUNC (open_menu_cb), me);
  image = gtk_image_new_from_stock (GTK_STOCK_SAVE, GTK_ICON_SIZE_LARGE_TOOLBAR);
  me->toolbar_save =
    gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON, NULL, "",
                                _("Save current menu"), NULL, image, GTK_SIGNAL_FUNC (save_menu_cb), me);
  image = gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_LARGE_TOOLBAR);
  me->toolbar_close =
    gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON, NULL, "",
                                _("Close current menu"), NULL, image, GTK_SIGNAL_FUNC (close_menu_cb), me);
  gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));
  image = gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_LARGE_TOOLBAR);
  me->toolbar_add =
    gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON, NULL, "",
                                _("Add an entry to the menu"), NULL, image, GTK_SIGNAL_FUNC (add_entry_cb), me);
  image = gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_LARGE_TOOLBAR);
  me->toolbar_del = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                                GTK_TOOLBAR_CHILD_BUTTON, NULL, "",
                                                _("Delete the current entry"), NULL, image,
                                                GTK_SIGNAL_FUNC (delete_entry_cb), me);
  image = gtk_image_new_from_stock (GTK_STOCK_GO_UP, GTK_ICON_SIZE_LARGE_TOOLBAR);
  me->toolbar_up =
    gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON, NULL, "",
                                _("Move the current entry up"), NULL, image, GTK_SIGNAL_FUNC (entry_up_cb), me);
  image = gtk_image_new_from_stock (GTK_STOCK_GO_DOWN, GTK_ICON_SIZE_LARGE_TOOLBAR);
  me->toolbar_down =
    gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON, NULL, "",
                                _("Move the current entry down"), NULL, image, GTK_SIGNAL_FUNC (entry_down_cb), me);
  gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));
  image = gtk_image_new_from_stock (GTK_STOCK_ZOOM_OUT, GTK_ICON_SIZE_LARGE_TOOLBAR);
  me->toolbar_collapse =
    gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON, NULL, "",
                                _("Collapse the tree"), NULL, image, GTK_SIGNAL_FUNC (collapse_tree_cb), me);
  image = gtk_image_new_from_stock (GTK_STOCK_ZOOM_IN, GTK_ICON_SIZE_LARGE_TOOLBAR);
  me->toolbar_expand =
    gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON, NULL, "",
                                _("Expand the tree"), NULL, image, GTK_SIGNAL_FUNC (expand_tree_cb), me);

  /* Treeview */
  treestore =
    gtk_tree_store_new (COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_INT,
                        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
  scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (vbox), scrolledwindow, TRUE, TRUE, 0);

  me->treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (treestore));
  gtk_container_add (GTK_CONTAINER (scrolledwindow), me->treeview);

  /* Columns */
  cell_icon = gtk_cell_renderer_pixbuf_new ();
  cell_name = gtk_cell_renderer_text_new ();
  cell_command = gtk_cell_renderer_text_new ();
  cell_hidden = gtk_cell_renderer_toggle_new ();

  column_name = gtk_tree_view_column_new ();

  gtk_tree_view_column_pack_start (column_name, cell_icon, FALSE);
  gtk_tree_view_column_set_attributes (column_name, cell_icon, "pixbuf", COLUMN_ICON, NULL);
  g_object_set (cell_icon, "xalign", 0.0, "ypad", 0, NULL);

  gtk_tree_view_column_pack_start (column_name, cell_name, TRUE);
  gtk_tree_view_column_set_attributes (column_name, cell_name, "markup", COLUMN_NAME, NULL);
  g_object_set (cell_name, "ypad", 0, "yalign", 0.5, NULL);
  gtk_tree_view_column_set_title (column_name, _("Name"));

  column_command =
    gtk_tree_view_column_new_with_attributes (_("Command"), cell_command, "markup", COLUMN_COMMAND, NULL);
  column_hidden = gtk_tree_view_column_new_with_attributes (_("Hidden"), cell_hidden, "active", COLUMN_HIDDEN, NULL);

  gtk_tree_view_column_set_expand (column_name, TRUE);
  gtk_tree_view_column_set_expand (column_command, TRUE);
  gtk_tree_view_column_set_expand (column_hidden, FALSE);

  gtk_tree_view_append_column (GTK_TREE_VIEW (me->treeview), column_name);
  gtk_tree_view_append_column (GTK_TREE_VIEW (me->treeview), column_command);
  gtk_tree_view_append_column (GTK_TREE_VIEW (me->treeview), column_hidden);

  gtk_tree_view_column_set_alignment (column_name, 0.5);
  gtk_tree_view_column_set_alignment (column_command, 0.5);
  gtk_tree_view_column_set_alignment (column_hidden, 0.5);

  /* Status bar */
  statusbar = gtk_statusbar_new ();
  gtk_box_pack_start (GTK_BOX (vbox), statusbar, FALSE, FALSE, 0);

  /* Set up DnD */
  gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (me->treeview), GDK_BUTTON1_MASK, gte,
					  TARGETS, GDK_ACTION_COPY | GDK_ACTION_MOVE);
  gtk_tree_view_enable_model_drag_dest (GTK_TREE_VIEW (me->treeview), gte, TARGETS, GDK_ACTION_COPY);

  /* Connect signals */
  /* =============== */
  g_signal_connect (G_OBJECT (me->window), "delete-event", G_CALLBACK (delete_main_window_cb), me);
  g_signal_connect (G_OBJECT (me->icon_theme), "changed", G_CALLBACK (icon_theme_changed_cb), me);

  /* Menu */
  g_signal_connect (G_OBJECT (me->menu_item_file_new), "activate", G_CALLBACK (new_menu_cb), me);
  g_signal_connect (G_OBJECT (me->menu_item_file_open), "activate", G_CALLBACK (open_menu_cb), me);
  g_signal_connect (G_OBJECT (me->menu_item_file_default), "activate", G_CALLBACK (open_default_menu_cb), me);
  g_signal_connect (G_OBJECT (me->menu_item_file_save), "activate", G_CALLBACK (save_menu_cb), me);
  g_signal_connect (G_OBJECT (me->menu_item_file_saveas), "activate", G_CALLBACK (saveas_menu_cb), me);
  g_signal_connect (G_OBJECT (me->menu_item_file_close), "activate", G_CALLBACK (close_menu_cb), me);
  g_signal_connect (G_OBJECT (me->menu_item_file_exit), "activate", G_CALLBACK (confirm_quit_cb), me);

  g_signal_connect (G_OBJECT (me->menu_item_edit_add), "activate", G_CALLBACK (add_entry_cb), me);
  g_signal_connect (G_OBJECT (me->menu_item_edit_add_menu), "activate", G_CALLBACK (add_menu_cb), me);
  g_signal_connect (G_OBJECT (me->menu_item_edit_del), "activate", G_CALLBACK (delete_entry_cb), me);
  g_signal_connect (G_OBJECT (me->menu_item_edit_up), "activate", G_CALLBACK (entry_up_cb), me);
  g_signal_connect (G_OBJECT (me->menu_item_edit_down), "activate", G_CALLBACK (entry_down_cb), me);

  g_signal_connect (G_OBJECT (me->menu_item_help_about), "activate", G_CALLBACK (about_cb), me);

  /* Popup menu */
  g_signal_connect (G_OBJECT (me->menu_item_popup_edit), "activate", G_CALLBACK (popup_menu_edit_cb), me);
  g_signal_connect (G_OBJECT (me->menu_item_popup_add), "activate", G_CALLBACK (add_entry_cb), me);
  g_signal_connect (G_OBJECT (me->menu_item_popup_addmenu), "activate", G_CALLBACK (add_menu_cb), me);
  g_signal_connect (G_OBJECT (me->menu_item_popup_del), "activate", G_CALLBACK (delete_entry_cb), me);
  g_signal_connect (G_OBJECT (me->menu_item_popup_up), "activate", G_CALLBACK (entry_up_cb), me);
  g_signal_connect (G_OBJECT (me->menu_item_popup_down), "activate", G_CALLBACK (entry_down_cb), me);

  /* Treeview */
  g_signal_connect (G_OBJECT (me->treeview), "button-press-event", G_CALLBACK (treeview_button_pressed_cb), me);
  g_signal_connect (G_OBJECT (me->treeview), "row-activated", G_CALLBACK (treeview_activate_cb), me);
  g_signal_connect (G_OBJECT (me->treeview), "cursor-changed", G_CALLBACK (treeview_cursor_changed_cb), me);
  g_signal_connect (G_OBJECT (cell_hidden), "toggled", G_CALLBACK (visible_column_toggled_cb), me);

  /* DnD */
  g_signal_connect (G_OBJECT (me->treeview), "drag-data-received", G_CALLBACK (treeview_drag_data_rcv_cb), me);
  g_signal_connect (G_OBJECT (me->treeview), "drag-data-get", G_CALLBACK (treeview_drag_data_get_cb), me);

  /* Add accelerators */
  gtk_window_add_accel_group (GTK_WINDOW (me->window), accel_group);

  /* Deactivate the widgets not usable */
  gtk_widget_set_sensitive (me->menu_item_file_save, FALSE);
  gtk_widget_set_sensitive (me->menu_item_file_saveas, FALSE);
  gtk_widget_set_sensitive (me->menu_item_file_close, FALSE);

  gtk_widget_set_sensitive (me->menu_item_edit_add, FALSE);
  gtk_widget_set_sensitive (me->menu_item_edit_add_menu, FALSE);
  gtk_widget_set_sensitive (me->menu_item_edit_del, FALSE);
  gtk_widget_set_sensitive (me->menu_item_edit_up, FALSE);
  gtk_widget_set_sensitive (me->menu_item_edit_down, FALSE);
  gtk_widget_set_sensitive (me->menu_item_edit, FALSE);

  gtk_widget_set_sensitive (me->toolbar_save, FALSE);
  gtk_widget_set_sensitive (me->toolbar_close, FALSE);
  gtk_widget_set_sensitive (me->toolbar_add, FALSE);
  gtk_widget_set_sensitive (me->toolbar_del, FALSE);
  gtk_widget_set_sensitive (me->toolbar_up, FALSE);
  gtk_widget_set_sensitive (me->toolbar_down, FALSE);
  gtk_widget_set_sensitive (me->toolbar_collapse, FALSE);
  gtk_widget_set_sensitive (me->toolbar_expand, FALSE);

  gtk_widget_set_sensitive (me->treeview, FALSE);

  /* Show all */
  gtk_tree_view_expand_all (GTK_TREE_VIEW (me->treeview));
}
