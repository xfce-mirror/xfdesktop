/*   menueditor.c */

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

#include <time.h>
#include <errno.h>

#include <libxfce4util/i18n.h>

#include "menueditor.h"

#include "add_dialog.h"
#include "add_menu_dialog.h"
#include "about_dialog.h"
#include "edit_dialog.h"
#include "new_dialog.h"
#include "move.h"
#include "dnd.h"

#include "../modules/menu/dummy_icon.h"

/* Search path for menu.xml file */
#define SEARCHPATH (SYSCONFDIR G_DIR_SEPARATOR_S "xfce4" G_DIR_SEPARATOR_S "%F.%L:"\
                    SYSCONFDIR G_DIR_SEPARATOR_S "xfce4" G_DIR_SEPARATOR_S "%F.%l:"\
                    SYSCONFDIR G_DIR_SEPARATOR_S "xfce4" G_DIR_SEPARATOR_S "%F")

/**************/
/* Prototypes */
/**************/

static gchar* get_default_menu_file ();
static void open_menu_file (gchar *menu_file, MenuEditor *me);

/* Callbacks */
static void confirm_quit_cb (GtkWidget *widget, gpointer data);
static gboolean delete_main_window_cb (GtkWidget *widget, GdkEvent *event, gpointer data);
static void menu_open_cb (GtkWidget *widget, gpointer data);
static void menu_open_default_cb (GtkWidget *widget, gpointer data);
void menu_save_cb (GtkWidget *widget, gpointer data);
static void menu_saveas_cb (GtkWidget *widget, gpointer data);
static void close_menu_cb (GtkWidget *widget, gpointer data);
static void treeview_cursor_changed_cb (GtkTreeView *treeview,gpointer user_data);
static void visible_column_toggled_cb (GtkCellRendererToggle *toggle, gchar *str_path, gpointer data);
static void delete_entry_cb (GtkWidget *widget, gpointer data);

/* Main window */
static void create_main_window(MenuEditor *me);

/* Load the menu in the tree */
void load_menu_in_tree (xmlNodePtr menu, GtkTreeIter *p, gpointer data);

/*****************************/
/* Manage icon theme changes */
/*****************************/
static gboolean icon_theme_update_foreach_func (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
  MenuEditor *me;
  GdkPixbuf *icon;
  xmlNodePtr node;
  xmlChar *prop_icon = NULL;

  me = (MenuEditor *) data;

  gtk_tree_model_get (model, iter, ICON_COLUMN, &icon, POINTER_COLUMN, &node, -1);
  prop_icon = xmlGetProp (node, "icon");

  if (prop_icon) {
    if (icon)
      g_object_unref (icon);
    
    icon = xfce_icon_theme_load (me->icon_theme, prop_icon, ICON_SIZE);
    gtk_tree_store_set (me->treestore, iter, ICON_COLUMN, icon, -1);
  }

  xmlFree (prop_icon);

  return FALSE;
}

static void icon_theme_changed_cb (XfceIconTheme *icon_theme, gpointer user_data)
{
  MenuEditor *me;
  GtkTreeModel *model;

  me = (MenuEditor *) user_data;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (me->treeview));

  if (model)
    gtk_tree_model_foreach (model, &icon_theme_update_foreach_func, me);
}

/*******************************/
/* Check if the command exists */
/*******************************/
gboolean command_exists(const gchar *command)
{
  gchar *cmd_buf = NULL;
  gchar *cmd_tok = NULL;
  gchar *program = NULL;
  gboolean result = FALSE;

  cmd_buf = g_strdup (command);
  cmd_tok = strtok (cmd_buf, " ");

  program = g_find_program_in_path (cmd_buf);

  if (program)
    result = TRUE;

  g_free (program);
  g_free (cmd_buf);

  return result;
}

/****************************************************/
/* browse for a command and set it in entry_command */
/****************************************************/
void browse_command_cb(GtkWidget *widget, gpointer data)
{
  MenuEditor *me;
  GtkWidget *filesel_dialog;

  me = (MenuEditor *) data;

  filesel_dialog = xfce_file_chooser_new (_("Select command"), GTK_WINDOW (me->main_window),
					  XFCE_FILE_CHOOSER_ACTION_OPEN,
					  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					  GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);

  if (strlen (gtk_entry_get_text (GTK_ENTRY (me->entry_command))) != 0) {
    gchar *cmd_buf = NULL;
    gchar *cmd_tok = NULL;
    gchar *programpath = NULL;

    cmd_buf = g_strdup (gtk_entry_get_text (GTK_ENTRY (me->entry_command)));
    cmd_tok = strtok (cmd_buf, " ");
    programpath = g_find_program_in_path (cmd_buf);
    xfce_file_chooser_set_filename (XFCE_FILE_CHOOSER (filesel_dialog), programpath);

    g_free (cmd_buf);
    g_free (programpath);
  }

  if(gtk_dialog_run (GTK_DIALOG (filesel_dialog)) == GTK_RESPONSE_ACCEPT){
    gchar *filename = NULL;

    filename = xfce_file_chooser_get_filename (XFCE_FILE_CHOOSER (filesel_dialog));
    gtk_entry_set_text (GTK_ENTRY (me->entry_command), filename);
    g_free (filename);
  }

  gtk_widget_hide (GTK_WIDGET (filesel_dialog));
}

/**********************************************/
/* browse for a icon and set it in entry_icon */
/**********************************************/
static void browse_icon_update_preview_cb (XfceFileChooser *chooser, gpointer data)
{
  GtkImage *preview;
  char *filename;
  GdkPixbuf *pix = NULL;
  
  preview = GTK_IMAGE (data);
  filename = xfce_file_chooser_get_filename (chooser);
  
  if (g_file_test (filename, G_FILE_TEST_IS_REGULAR))
    pix = xfce_pixbuf_new_from_file_at_size (filename, 250, 250, NULL);
  g_free (filename);
  
  if(pix) {
    gtk_image_set_from_pixbuf (preview, pix);
    g_object_unref (G_OBJECT (pix));
  }
  xfce_file_chooser_set_preview_widget_active (chooser, (pix != NULL));
}

void browse_icon_cb(GtkWidget *widget, gpointer data)
{
  MenuEditor *me;
  GtkWidget *filesel_dialog, *preview;
  XfceFileFilter *filter;

  me = (MenuEditor *) data;

  filesel_dialog = xfce_file_chooser_new (_("Select icon"), GTK_WINDOW (me->main_window),
					  XFCE_FILE_CHOOSER_ACTION_OPEN,
					  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					  GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
  
  filter = xfce_file_filter_new ();
  xfce_file_filter_set_name (filter, _("All Files"));
  xfce_file_filter_add_pattern (filter, "*");
  xfce_file_chooser_add_filter (XFCE_FILE_CHOOSER (filesel_dialog), filter);
  filter = xfce_file_filter_new ();
  xfce_file_filter_set_name (filter, _("Image Files"));
  xfce_file_filter_add_pattern (filter, "*.png");
  xfce_file_filter_add_pattern (filter, "*.jpg");
  xfce_file_filter_add_pattern (filter, "*.bmp");
  xfce_file_filter_add_pattern (filter, "*.svg");
  xfce_file_filter_add_pattern (filter, "*.xpm");
  xfce_file_filter_add_pattern (filter, "*.gif");
  xfce_file_chooser_add_filter (XFCE_FILE_CHOOSER (filesel_dialog), filter);
  xfce_file_chooser_set_filter (XFCE_FILE_CHOOSER (filesel_dialog), filter);

  preview = gtk_image_new ();
  gtk_widget_show (preview);
  xfce_file_chooser_set_preview_widget (XFCE_FILE_CHOOSER (filesel_dialog), preview);
  xfce_file_chooser_set_preview_widget_active (XFCE_FILE_CHOOSER (filesel_dialog), FALSE);
  xfce_file_chooser_set_preview_callback (XFCE_FILE_CHOOSER (filesel_dialog),
					  (PreviewUpdateFunc)browse_icon_update_preview_cb, preview);

  if (strlen (gtk_entry_get_text (GTK_ENTRY (me->entry_icon))) != 0){
    gchar *iconpath = NULL;

    iconpath = xfce_icon_theme_lookup (me->icon_theme, gtk_entry_get_text (GTK_ENTRY (me->entry_icon)), ICON_SIZE);
    xfce_file_chooser_set_filename (XFCE_FILE_CHOOSER (filesel_dialog), iconpath);

    g_free (iconpath);
  }

  if(gtk_dialog_run (GTK_DIALOG (filesel_dialog)) == GTK_RESPONSE_ACCEPT){
    gchar *filename = NULL;

    filename = xfce_file_chooser_get_filename (XFCE_FILE_CHOOSER (filesel_dialog));  
    gtk_entry_set_text (GTK_ENTRY (me->entry_icon), filename);
    g_free (filename);
  }

  gtk_widget_hide (GTK_WIDGET (filesel_dialog));
}


/*************/
/* callbacks */
/*************/
/* Ask confirmation when exiting program */
static gboolean delete_main_window_cb (GtkWidget *widget, GdkEvent *event, gpointer data)
{
  MenuEditor *me;

  me = (MenuEditor *) data;

  if(me->menu_modified){
    gint response = GTK_RESPONSE_NONE;

    response = xfce_message_dialog (GTK_WINDOW (me->main_window), "Question",
				    GTK_STOCK_DIALOG_QUESTION,
				    _("Do you want to save before closing the menu ?"),
				    _("You have modified the menu, do you want to save it before quitting ?"),
				    XFCE_CUSTOM_STOCK_BUTTON, _("Forget modifications"), GTK_STOCK_QUIT, GTK_RESPONSE_NO,
				    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				    GTK_STOCK_SAVE, GTK_RESPONSE_YES, NULL);

    switch(response){
    case GTK_RESPONSE_YES:
      menu_save_cb (widget, data);
      break;
    case GTK_RESPONSE_CANCEL:
      return TRUE;
    }
  }

  xmlFreeDoc (me->xml_menu_file); 
  me->xml_menu_file = NULL;
  gtk_main_quit();

  return FALSE;
}

static void confirm_quit_cb (GtkWidget *widget, gpointer data)
{
  delete_main_window_cb (widget, NULL, data);
}

/*****************************/
/* Load the menu in the tree */
/*****************************/
void load_menu_in_tree(xmlNodePtr menu, GtkTreeIter *p, gpointer data)
{
  MenuEditor *me;
  GtkTreeIter c;

  me = (MenuEditor *) data;

  while(menu != NULL){
    gboolean hidden = FALSE;

    GdkPixbuf *icon = NULL;

    xmlChar *prop_visible = NULL;
    xmlChar *prop_icon = NULL;
    xmlChar* prop_name = NULL;
    xmlChar* prop_cmd = NULL;
    xmlChar *prop_type = NULL;
    xmlChar *prop_src = NULL;

    gchar *name = NULL;
    gchar *cmd = NULL;
    gchar *src = NULL;
    gchar *title = NULL;

    prop_visible = xmlGetProp(menu, "visible");
    prop_icon = xmlGetProp(menu, "icon");

    /* Visible */
    if(prop_visible && (!xmlStrcmp(prop_visible,(xmlChar*)"false") || !xmlStrcmp(prop_visible,(xmlChar*)"no")))
      hidden=TRUE;

    /* Load the icon */
    if(prop_icon){
      XfceIconTheme *icontheme = xfce_icon_theme_get_for_screen (NULL);
      icon = xfce_icon_theme_load (icontheme, prop_icon, ICON_SIZE);

      if (!icon)
	icon = xfce_inline_icon_at_size(dummy_icon_data, ICON_SIZE, ICON_SIZE);
    } else
      icon = xfce_inline_icon_at_size(dummy_icon_data, ICON_SIZE, ICON_SIZE);

    /* separator */
    if(!xmlStrcmp(menu->name,(xmlChar*)"separator")){
      name = g_strdup_printf(SEPARATOR_FORMAT,
			     _("--- separator ---"));

      gtk_tree_store_append (me->treestore, &c, p);
      gtk_tree_store_set (me->treestore, &c,
			  ICON_COLUMN, icon,
			  NAME_COLUMN, name, 
			  COMMAND_COLUMN, "",
			  HIDDEN_COLUMN, hidden,
			  POINTER_COLUMN, menu, -1);
    }
    /* launcher */
    if(!xmlStrcmp(menu->name,(xmlChar*)"app")){
      prop_name = xmlGetProp(menu, "name");
      prop_cmd = xmlGetProp(menu, "cmd");

      name = g_strdup_printf(NAME_FORMAT, prop_name);
      cmd = g_strdup_printf(COMMAND_FORMAT, prop_cmd);

      gtk_tree_store_append (me->treestore, &c, p);
      gtk_tree_store_set (me->treestore, &c, 
			  ICON_COLUMN, icon,
			  NAME_COLUMN, name,
			  COMMAND_COLUMN, cmd,
			  HIDDEN_COLUMN, hidden,
			  POINTER_COLUMN, menu, -1);
    }
  
    /* menu */
    if(!xmlStrcmp(menu->name,(xmlChar*)"menu")){
      prop_name = xmlGetProp(menu, "name");

      name = g_strdup_printf(MENU_FORMAT, prop_name);
      
      gtk_tree_store_append (me->treestore, &c, p);
      gtk_tree_store_set (me->treestore, &c, 
			  ICON_COLUMN, icon,
			  NAME_COLUMN, name,
			  COMMAND_COLUMN, "",
			  HIDDEN_COLUMN, hidden,
			  POINTER_COLUMN, menu, -1);
      load_menu_in_tree (menu->xmlChildrenNode, &c, me);
    }

    /* include */
    if(!xmlStrcmp(menu->name,(xmlChar*)"include")){
      prop_type = xmlGetProp(menu, "type");
      prop_src = xmlGetProp(menu, "src");

      name = g_strdup_printf(INCLUDE_FORMAT,_("--- include ---"));

      if(!xmlStrcmp(prop_type, (xmlChar*)"system"))
	src = g_strdup_printf(INCLUDE_PATH_FORMAT,_("system"));
      else
	src = g_strdup_printf(INCLUDE_PATH_FORMAT, prop_src);
      
      gtk_tree_store_append (me->treestore, &c, p);
      gtk_tree_store_set (me->treestore, &c, 
			  ICON_COLUMN, icon,
			  NAME_COLUMN, name,
			  COMMAND_COLUMN, src,
			  HIDDEN_COLUMN, hidden,
			  POINTER_COLUMN, menu, -1);
    }

    /* builtin */
    if(!xmlStrcmp(menu->name,(xmlChar*)"builtin")){
      prop_name = xmlGetProp(menu, "name");
      prop_cmd = xmlGetProp(menu , "cmd");

      name = g_strdup_printf(NAME_FORMAT, prop_name);
      cmd = g_strdup_printf(COMMAND_FORMAT, prop_cmd);

      gtk_tree_store_append (me->treestore, &c, p);
      gtk_tree_store_set (me->treestore, &c, 
			  ICON_COLUMN, icon,
			  NAME_COLUMN, name,
			  COMMAND_COLUMN, cmd,
			  HIDDEN_COLUMN, hidden,
			  POINTER_COLUMN, menu, -1);
    }
    /* title */
    if(!xmlStrcmp(menu->name,(xmlChar*)"title")){
      prop_name = xmlGetProp(menu, "name");

      title = g_strdup_printf(TITLE_FORMAT, prop_name);

      gtk_tree_store_append (me->treestore, &c, p);
      gtk_tree_store_set (me->treestore, &c, 
			  ICON_COLUMN, icon, 
			  NAME_COLUMN, title,
			  COMMAND_COLUMN, "", 
			  HIDDEN_COLUMN, hidden, 
			  POINTER_COLUMN, menu , -1);
    }

    g_free(name);
    g_free(cmd);
    g_free(src);
    g_free(title);

    xmlFree(prop_type);
    xmlFree(prop_src);
    xmlFree(prop_name);
    xmlFree(prop_cmd);
    xmlFree(prop_visible);
    xmlFree(prop_icon);

    menu = menu->next;
  }

}

/*****************************/
/* Open the menu file in use */
/*****************************/
static void menu_open_default_cb(GtkWidget *widget, gpointer data)
{
  MenuEditor *me;
  gchar *window_title;
  gchar *home_menu;

  me = (MenuEditor *) data;

  /* Check if there is no other file opened */
  if(me->xml_menu_file != NULL && me->menu_modified){
    gint response;

    response = xfce_message_dialog (GTK_WINDOW (me->main_window), _("Question"),
				    GTK_STOCK_DIALOG_QUESTION,
				    _("Do you want to save before opening the default menu ?"),
				    NULL,
				    XFCE_CUSTOM_BUTTON, _("Ignore modifications"), GTK_RESPONSE_NO,
				    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				    GTK_STOCK_SAVE, GTK_RESPONSE_YES, NULL);
    switch(response){
    case GTK_RESPONSE_CANCEL:
      return;
    case GTK_RESPONSE_YES:
      menu_save_cb (widget, data);
    }
  }

  if(me->xml_menu_file != NULL){
    gtk_tree_store_clear (GTK_TREE_STORE (me->treestore));
    xmlFreeDoc (me->xml_menu_file);
    me->xml_menu_file = NULL;
  }

  home_menu = xfce_resource_save_location(XFCE_RESOURCE_CONFIG,"xfce4/desktop/menu.xml", TRUE);

  if(g_file_test(home_menu, G_FILE_TEST_EXISTS))
    open_menu_file (home_menu, me);
  else{
    gchar *filename = get_default_menu_file();

    open_menu_file (filename, me);
    g_stpcpy(me->menu_file_name, home_menu);

    xmlSaveFormatFile(home_menu, me->xml_menu_file, 1);

    g_free(filename);
  }

  /* Set window's title */
  window_title = g_strdup_printf ("Xfce4-MenuEditor - %s", me->menu_file_name);
  gtk_window_set_title (GTK_WINDOW (me->main_window), window_title);

  g_free (home_menu);
  g_free (window_title);
}

/************************************/
/* Browse for menu file and open it */
/************************************/
static void menu_open_cb(GtkWidget *widget, gpointer data)
{
  MenuEditor *me;
  GtkWidget *filesel_dialog;

  me = (MenuEditor *) data;

  /* Check if there is no other file opened */
  if(me->xml_menu_file != NULL && me->menu_modified){
    gint response;

    response = xfce_message_dialog (GTK_WINDOW (me->main_window), _("Question"),
				    GTK_STOCK_DIALOG_QUESTION,
				    _("Do you want to save before opening an other menu ?"),
				    NULL,
				    XFCE_CUSTOM_BUTTON, _("Ignore modifications"), GTK_RESPONSE_NO,
				    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				    GTK_STOCK_SAVE, GTK_RESPONSE_YES, NULL);
    switch(response){
    case GTK_RESPONSE_CANCEL:
      return;
    case GTK_RESPONSE_YES:
      menu_save_cb (widget, data);
    }
  }

  filesel_dialog = xfce_file_chooser_new (_("Open menu file"), GTK_WINDOW (me->main_window),
					  XFCE_FILE_CHOOSER_ACTION_OPEN,
					  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					  GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);

  if( gtk_dialog_run (GTK_DIALOG (filesel_dialog)) == GTK_RESPONSE_ACCEPT){
    gchar *filename = NULL;

    filename = xfce_file_chooser_get_filename (XFCE_FILE_CHOOSER (filesel_dialog));

    if(me->xml_menu_file != NULL){
      gtk_tree_store_clear (GTK_TREE_STORE (me->treestore));
      xmlFreeDoc (me->xml_menu_file);
      me->xml_menu_file = NULL;
    }

    open_menu_file (filename, me);
    
    g_free(filename);
  }
    
  gtk_widget_hide (filesel_dialog);
}

static gboolean menufile_save(MenuEditor *me)
{
  gchar *tmp_filename = NULL;
  
  /* Save the menu file with atomicity */
  tmp_filename = g_strdup_printf ("%s.tmp", me->menu_file_name);

  xmlSaveFormatFile(tmp_filename, me->xml_menu_file, 1);
  if (unlink (me->menu_file_name)){
    perror ("unlink(me->menu_file_name)");
    xfce_err (_("Cannot write in %s : \n%s"), me->menu_file_name, strerror (errno));
    g_free (tmp_filename);
    return FALSE;
  }
  if (link (tmp_filename, me->menu_file_name)){
    perror ("link(tmp_filename, me->menu_file_name)");
    g_free (tmp_filename);
    return FALSE;
  }
  if (unlink (tmp_filename)){
    perror ("unlink(tmp_filename)");
    xfce_err (_("Cannot write in %s : \n%s"), tmp_filename, strerror(errno));
    g_free (tmp_filename);
    return FALSE;
  }
  g_free (tmp_filename);

  return TRUE;
}

/* Save the menu file */
void menu_save_cb (GtkWidget *widget, gpointer data)
{
  MenuEditor *me;

  me = (MenuEditor *) data;

  if (menufile_save (me)){
    me->menu_modified = FALSE;
    gtk_widget_set_sensitive (me->file_menu_save, FALSE);
    gtk_widget_set_sensitive (me->toolbar_save, FALSE);
  }
}

/* Ask the filename and save the menu into */
static void menu_saveas_cb (GtkWidget *widget, gpointer data)
{
  MenuEditor *me;
  GtkWidget *filesel_dialog;

  me = (MenuEditor *) data;

  filesel_dialog = xfce_file_chooser_new (_("Save as..."), GTK_WINDOW (me->main_window),
					  XFCE_FILE_CHOOSER_ACTION_SAVE,
					  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					  GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);

  xfce_file_chooser_set_current_name (XFCE_FILE_CHOOSER (filesel_dialog), "menu.xml");

  if( gtk_dialog_run (GTK_DIALOG (filesel_dialog)) == GTK_RESPONSE_ACCEPT){
    gchar *filename = NULL;

    filename = xfce_file_chooser_get_filename (XFCE_FILE_CHOOSER (filesel_dialog));

    if (strcmp (filename, me->menu_file_name) == 0){
      g_stpcpy (me->menu_file_name, filename); 

      if (menufile_save (me)){
	me->menu_modified = FALSE;
	gtk_widget_set_sensitive (me->file_menu_save, FALSE);
	gtk_widget_set_sensitive (me->toolbar_save, FALSE);
      }
    }else{
      gchar *window_title;

      xmlSaveFormatFile(filename, me->xml_menu_file, 1);

      g_stpcpy(me->menu_file_name, filename); 

      me->menu_modified = FALSE;
      gtk_widget_set_sensitive (me->file_menu_save, FALSE);
      gtk_widget_set_sensitive (me->toolbar_save, FALSE);

      /* Set window's title */
      window_title = g_strdup_printf ("Xfce4-MenuEditor - %s", me->menu_file_name);
      gtk_window_set_title (GTK_WINDOW (me->main_window), window_title);

      g_free (window_title);
    }
    
    g_free (filename);

  }

  gtk_widget_destroy (filesel_dialog);
}

/* Close the menu file and prompt to save if it was modified */
static void close_menu_cb (GtkWidget *widget, gpointer data)
{
  MenuEditor *me;

  me = (MenuEditor *) data;

  if (me->menu_modified){
    gint response;

    response = xfce_message_dialog (GTK_WINDOW (me->main_window), _("Question"),
				    GTK_STOCK_DIALOG_QUESTION,
				    _("Do you want to save before closing the menu ?"),
				    NULL,
				    XFCE_CUSTOM_BUTTON, _("Ignore modifications"), GTK_RESPONSE_NO,
				    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				    GTK_STOCK_SAVE, GTK_RESPONSE_YES, NULL);
    switch(response){
    case GTK_RESPONSE_CANCEL:
      return;
    case GTK_RESPONSE_YES:
      menu_save_cb(widget,NULL);
    }
  }

  xmlFreeDoc (me->xml_menu_file);
  me->xml_menu_file = NULL;

  gtk_tree_store_clear (GTK_TREE_STORE (me->treestore));

  me->menu_modified = FALSE;
  
  gtk_widget_set_sensitive (me->toolbar_close, FALSE);
  gtk_widget_set_sensitive (me->toolbar_save, FALSE);
  gtk_widget_set_sensitive (me->toolbar_add, FALSE);
  gtk_widget_set_sensitive (me->toolbar_del, FALSE);
  gtk_widget_set_sensitive (me->toolbar_up, FALSE);
  gtk_widget_set_sensitive (me->toolbar_down, FALSE);

  gtk_widget_set_sensitive (me->file_menu_close, FALSE);
  gtk_widget_set_sensitive (me->file_menu_save, FALSE);
  gtk_widget_set_sensitive (me->file_menu_saveas, FALSE);

  gtk_widget_set_sensitive (me->edit_menu_item, FALSE);

  gtk_widget_set_sensitive (me->treeview, FALSE);
}

/* Collapse the treeview */
static void collapse_tree_cb (GtkWidget *widget, gpointer data)
{
  MenuEditor *me;

  me = (MenuEditor *) data;

  gtk_tree_view_collapse_all (GTK_TREE_VIEW (me->treeview));
}

/* Expand the treeview */
static void expand_tree_cb (GtkWidget *widget, gpointer data)
{
  MenuEditor *me;

  me = (MenuEditor *) data;

  gtk_tree_view_expand_all (GTK_TREE_VIEW (me->treeview));
}

static void treeview_cursor_changed_cb (GtkTreeView *treeview, gpointer data)
{
  MenuEditor *me;
  GtkTreeIter iter;
  GtkTreeModel *tree_model;

  me = (MenuEditor *) data;

  tree_model = GTK_TREE_MODEL (me->treestore);
  if(gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (me->treeview)),
				      &tree_model, &iter)){
    gtk_widget_set_sensitive (me->toolbar_add, TRUE);
    gtk_widget_set_sensitive (me->toolbar_del, TRUE);
    gtk_widget_set_sensitive (me->toolbar_up, TRUE);
    gtk_widget_set_sensitive (me->toolbar_down, TRUE);

    gtk_widget_set_sensitive (me->edit_menu_add_menu, TRUE);
    gtk_widget_set_sensitive (me->edit_menu_add, TRUE);
    gtk_widget_set_sensitive (me->edit_menu_del, TRUE);
    gtk_widget_set_sensitive (me->edit_menu_up, TRUE);
    gtk_widget_set_sensitive (me->edit_menu_down, TRUE);
  }

}

static gboolean treeview_button_pressed_cb (GtkTreeView *treeview, GdkEventButton *event, gpointer data)
{
  MenuEditor *me;

  me = (MenuEditor *) data;

  /* Right click draws the context menu */
  if ((event->button == 3) && (event->type == GDK_BUTTON_PRESS)){
    GtkTreePath *path;

    if (gtk_tree_view_get_path_at_pos (treeview, event->x, event->y, &path, NULL, NULL, NULL)) {
      GtkTreeSelection *selection;

      GtkTreeIter iter;
      GValue val = { 0, };
      xmlNodePtr node;
  
      GtkWidget* popup_menu;
      GtkWidget* edit_menuitem;
      GtkWidget* add_menuitem;
      GtkWidget* addmenu_menuitem;
      GtkWidget* del_menuitem;
      GtkWidget* moveup_menuitem;
      GtkWidget* movedown_menuitem;
      GtkWidget* separator_menuitem;

      selection = gtk_tree_view_get_selection (treeview);
      gtk_tree_selection_unselect_all (selection);
      gtk_tree_selection_select_path (selection, path);
      gtk_tree_model_get_iter (GTK_TREE_MODEL (me->treestore), &iter, path);
      gtk_tree_model_get_value (GTK_TREE_MODEL (me->treestore), &iter, POINTER_COLUMN, &val);
      node = g_value_get_pointer(&val);
    
      if(!xmlStrcmp (node->name, "separator"))
	return TRUE;

      /* Create the popup menu */
      popup_menu = gtk_menu_new ();

      edit_menuitem = gtk_image_menu_item_new_with_mnemonic (_("Edit"));
      gtk_container_add (GTK_CONTAINER (popup_menu), edit_menuitem);
      separator_menuitem = gtk_separator_menu_item_new();
      gtk_container_add (GTK_CONTAINER (popup_menu), separator_menuitem);
      add_menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_ADD, me->accel_group);
      gtk_container_add (GTK_CONTAINER (popup_menu), add_menuitem);
      addmenu_menuitem = gtk_image_menu_item_new_with_mnemonic (_("Add an external menu"));
      gtk_container_add (GTK_CONTAINER (popup_menu), addmenu_menuitem);
      del_menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_REMOVE, me->accel_group);
      gtk_container_add (GTK_CONTAINER (popup_menu), del_menuitem);
      separator_menuitem = gtk_separator_menu_item_new();
      gtk_container_add (GTK_CONTAINER (popup_menu), separator_menuitem);
      moveup_menuitem = gtk_image_menu_item_new_from_stock (GTK_STOCK_GO_UP, me->accel_group);
      gtk_container_add (GTK_CONTAINER (popup_menu), moveup_menuitem);
      movedown_menuitem = gtk_image_menu_item_new_from_stock (GTK_STOCK_GO_DOWN, me->accel_group);
      gtk_container_add (GTK_CONTAINER (popup_menu), movedown_menuitem);

      g_signal_connect ((gpointer) edit_menuitem, "activate",
			G_CALLBACK (popup_edit_cb), me);
      g_signal_connect ((gpointer) add_menuitem, "activate",
			G_CALLBACK (add_entry_cb), me);
      g_signal_connect ((gpointer) addmenu_menuitem, "activate",
			G_CALLBACK (add_menu_cb), me);
      g_signal_connect ((gpointer) del_menuitem, "activate",
			G_CALLBACK (delete_entry_cb), me);
      g_signal_connect ((gpointer) moveup_menuitem, "activate",
			G_CALLBACK (entry_up_cb), me);
      g_signal_connect ((gpointer) movedown_menuitem, "activate",
			G_CALLBACK (entry_down_cb), me);

      gtk_widget_show_all(popup_menu);
      gtk_menu_popup(GTK_MENU(popup_menu), NULL, NULL, NULL, NULL,
		     event->button, gtk_get_current_event_time());
      return TRUE;
    }
  }
  
  return FALSE;
}


static void delete_entry_cb (GtkWidget *widget, gpointer data)
{
  MenuEditor *me;
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;

  me = (MenuEditor *) data;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (me->treeview));
  model = GTK_TREE_MODEL (me->treestore);
  if (gtk_tree_selection_get_selected (selection,&model,&iter)) {
    xmlNodePtr node;
    GdkPixbuf *icon=NULL;

    GValue val_node = { 0, };
    GValue val_icon = { 0, };

    /* Retrieve the xmlNodePtr of the menu entry and free it */
    gtk_tree_model_get_value (GTK_TREE_MODEL (me->treestore), &iter, POINTER_COLUMN, &val_node);
    node = g_value_get_pointer (&val_node);
    xmlUnlinkNode (node);
    xmlFreeNode (node);
    /* Retrieve the GdkPixbuf pointer on the icon and free it */
    gtk_tree_model_get_value (GTK_TREE_MODEL (me->treestore), &iter, ICON_COLUMN, &val_icon);
    icon = g_value_get_object (&val_icon);
    if(G_IS_OBJECT (icon))
      g_object_unref (icon);

    gtk_tree_store_remove (GTK_TREE_STORE (me->treestore),&iter);

    /* Modified ! */
    me->menu_modified = TRUE;
    gtk_widget_set_sensitive (me->file_menu_save, TRUE);
    gtk_widget_set_sensitive (me->toolbar_save, TRUE);
  }

  gtk_widget_set_sensitive (me->toolbar_del, FALSE);
  gtk_widget_set_sensitive (me->toolbar_up, FALSE);
  gtk_widget_set_sensitive (me->toolbar_down, FALSE);

  gtk_widget_set_sensitive (me->edit_menu_del, FALSE);
  gtk_widget_set_sensitive (me->edit_menu_up, FALSE);
  gtk_widget_set_sensitive (me->edit_menu_down, FALSE);
}



/*******************************/
/* Click on the visible toggle */
/*******************************/
static void visible_column_toggled_cb (GtkCellRendererToggle *toggle, gchar *str_path,
				       gpointer data)
{
  MenuEditor *me;
  GtkTreePath *path = gtk_tree_path_new_from_string(str_path);
  GtkTreeModel *model;
  GtkTreeIter iter;
  GValue val = {0};
  GValue val2 = {0};
  xmlNodePtr node;
  gboolean hidden;

  me = (MenuEditor *) data;
  model = GTK_TREE_MODEL(me->treestore);

  /* Retrieve current iter */
  gtk_tree_model_get_iter (model,&iter,path);

  /* Retrieve state */
  gtk_tree_model_get_value (GTK_TREE_MODEL (me->treestore), &iter, HIDDEN_COLUMN, &val);
  hidden = g_value_get_boolean(&val);

  gtk_tree_model_get_value (GTK_TREE_MODEL (me->treestore), &iter, POINTER_COLUMN, &val2);
  node = g_value_get_pointer(&val2);

  /* Change state */
  if(hidden){
    gtk_tree_store_set (me->treestore, &iter, HIDDEN_COLUMN, FALSE, -1);
    xmlSetProp(node,"visible","true");
  }else{
    gtk_tree_store_set (me->treestore, &iter, HIDDEN_COLUMN, TRUE, -1);
    xmlSetProp(node,"visible","false");
  }

  /* Modified ! */
  me->menu_modified = TRUE;
  gtk_widget_set_sensitive (me->file_menu_save, TRUE);
  gtk_widget_set_sensitive (me->toolbar_save, TRUE);

  gtk_tree_path_free (path);
}

/***************/
/* Main window */
/***************/

static void create_main_window(MenuEditor *me)
{
  GtkWidget *main_vbox;

  GtkWidget *tmp_toolbar_icon;
  /* Widgets */
  GtkWidget *main_menubar;
  GtkWidget *menu_separator;
  GtkWidget *file_menu;
  GtkWidget *edit_menu;
  GtkWidget *help_menu;

  GtkWidget *toolbar;

  /* Treeview */
  GtkWidget *scrolledwindow;
  GtkCellRenderer *name_cell, *command_cell, *hidden_cell, *pointer_cell, *icon_cell;
  GtkTreeViewColumn *name_column, *command_column, *hidden_column, *pointer_column;


  /* Status bar */
  GtkWidget *statusbar;

  /* Icons */
  GList *icons = NULL;
  GdkPixbuf *icon = NULL;

  /* DND */
  GtkTargetEntry gte[] = {{"XFCE_MENU_ENTRY", GTK_TARGET_SAME_WIDGET, 0},
			  {"text/plain",0, 1},
			  {"application/x-desktop", 0, 2}};

  me->accel_group = gtk_accel_group_new ();
  me->icon_theme = xfce_icon_theme_get_for_screen (NULL);

  /* Window */
  me->main_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (me->main_window),
			"Xfce4-MenuEditor");
  g_signal_connect (G_OBJECT (me->main_window), "delete-event",
		    G_CALLBACK (delete_main_window_cb), me);
  gtk_window_set_default_size (GTK_WINDOW (me->main_window),600,450);

  /* Set default icon */
  g_signal_connect (G_OBJECT (me->icon_theme), "changed",
		    G_CALLBACK(icon_theme_changed_cb), me);
  icon = xfce_icon_theme_load (me->icon_theme, "xfce4-menueditor", 16);
  if (icon)
    icons = g_list_append (icons,icon);
  icon = xfce_icon_theme_load (me->icon_theme, "xfce4-menueditor", 32);
  if (icon)
    icons = g_list_append (icons,icon);
  icon = xfce_icon_theme_load (me->icon_theme, "xfce4-menueditor", 48);
  if (icon)
    icons = g_list_append (icons,icon);

  gtk_window_set_default_icon_list (icons);
  g_list_free (icons);
  
  /* Main vbox */
  main_vbox = gtk_vbox_new (FALSE,0);
  gtk_container_add (GTK_CONTAINER (me->main_window), main_vbox);

  /* Menu bar */
  main_menubar = gtk_menu_bar_new ();
  gtk_box_pack_start (GTK_BOX (main_vbox), main_menubar, FALSE, FALSE, 0);

  /* File menu */
  me->file_menu_item = gtk_image_menu_item_new_with_mnemonic (_("_File"));
  gtk_container_add (GTK_CONTAINER (main_menubar), me->file_menu_item);
  file_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (me->file_menu_item), file_menu);

  me->file_menu_new = gtk_image_menu_item_new_from_stock (GTK_STOCK_NEW, me->accel_group);
  gtk_container_add (GTK_CONTAINER (file_menu), me->file_menu_new);
  me->file_menu_open = gtk_image_menu_item_new_from_stock (GTK_STOCK_OPEN, me->accel_group);
  gtk_container_add (GTK_CONTAINER (file_menu), me->file_menu_open);
  me->file_menu_default = gtk_image_menu_item_new_with_mnemonic (_("Open default menu"));
  gtk_container_add (GTK_CONTAINER (file_menu), me->file_menu_default);
  me->file_menu_save = gtk_image_menu_item_new_from_stock (GTK_STOCK_SAVE, me->accel_group);
  gtk_container_add (GTK_CONTAINER (file_menu), me->file_menu_save);
  me->file_menu_saveas = gtk_image_menu_item_new_from_stock (GTK_STOCK_SAVE_AS, me->accel_group);
  gtk_container_add (GTK_CONTAINER (file_menu), me->file_menu_saveas);
  me->file_menu_close = gtk_image_menu_item_new_from_stock (GTK_STOCK_CLOSE, me->accel_group);
  gtk_container_add (GTK_CONTAINER (file_menu), me->file_menu_close);
  menu_separator = gtk_separator_menu_item_new();
  gtk_container_add (GTK_CONTAINER (file_menu), menu_separator);
  me->file_menu_exit = gtk_image_menu_item_new_from_stock (GTK_STOCK_QUIT, me->accel_group);
  gtk_container_add (GTK_CONTAINER (file_menu), me->file_menu_exit);

  /* Edit menu */
  me->edit_menu_item = gtk_image_menu_item_new_with_mnemonic (_("_Edit"));
  gtk_container_add (GTK_CONTAINER (main_menubar), me->edit_menu_item);
  edit_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (me->edit_menu_item), edit_menu);
  me->edit_menu_add = gtk_image_menu_item_new_from_stock (GTK_STOCK_ADD, me->accel_group);
  gtk_container_add (GTK_CONTAINER (edit_menu), me->edit_menu_add);
  me->edit_menu_add_menu = gtk_image_menu_item_new_with_mnemonic (_("Add an external menu..."));
  gtk_container_add (GTK_CONTAINER (edit_menu), me->edit_menu_add_menu);
  me->edit_menu_del = gtk_image_menu_item_new_from_stock (GTK_STOCK_REMOVE, me->accel_group);
  gtk_container_add (GTK_CONTAINER (edit_menu), me->edit_menu_del);
  menu_separator = gtk_separator_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (edit_menu), menu_separator);
  me->edit_menu_up = gtk_image_menu_item_new_from_stock (GTK_STOCK_GO_UP, me->accel_group);
  gtk_container_add (GTK_CONTAINER (edit_menu), me->edit_menu_up);
  me->edit_menu_down = gtk_image_menu_item_new_from_stock (GTK_STOCK_GO_DOWN, me->accel_group);
  gtk_container_add (GTK_CONTAINER (edit_menu), me->edit_menu_down);

  /* Help menu */
  me->help_menu_item = gtk_image_menu_item_new_with_mnemonic (_("_Help"));
  gtk_container_add (GTK_CONTAINER (main_menubar), me->help_menu_item);
  help_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (me->help_menu_item), help_menu);
  
  me->help_menu_about = gtk_menu_item_new_with_mnemonic (_("_About..."));
  gtk_container_add (GTK_CONTAINER (help_menu), me->help_menu_about);

  /* Toolbar */
  toolbar = gtk_toolbar_new ();
  gtk_box_pack_start (GTK_BOX (main_vbox), toolbar, FALSE, FALSE, 0);
  gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_ICONS);

  tmp_toolbar_icon = gtk_image_new_from_stock (GTK_STOCK_NEW, GTK_ICON_SIZE_LARGE_TOOLBAR);
  me->toolbar_new = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_CHILD_BUTTON, NULL,
						"", _("Create a new Xfce4 menu file"), NULL,
						tmp_toolbar_icon, GTK_SIGNAL_FUNC (new_menu_cb), me);
  tmp_toolbar_icon = gtk_image_new_from_stock (GTK_STOCK_OPEN, GTK_ICON_SIZE_LARGE_TOOLBAR);
  me->toolbar_open = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_CHILD_BUTTON, NULL,
						 "", _("Open an Xfce4 menu file"), NULL,
						 tmp_toolbar_icon, GTK_SIGNAL_FUNC (menu_open_cb), me);
  tmp_toolbar_icon = gtk_image_new_from_stock (GTK_STOCK_SAVE, GTK_ICON_SIZE_LARGE_TOOLBAR);
  me->toolbar_save = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_CHILD_BUTTON, NULL,
						 "", _("Save current menu"), NULL,
						 tmp_toolbar_icon, GTK_SIGNAL_FUNC (menu_save_cb), me);
  tmp_toolbar_icon = gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_LARGE_TOOLBAR);
  me->toolbar_close = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_CHILD_BUTTON, NULL,
						  "", _("Close current menu"), NULL,
						  tmp_toolbar_icon, GTK_SIGNAL_FUNC (close_menu_cb), me);
  gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));
  tmp_toolbar_icon = gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_LARGE_TOOLBAR);
  me->toolbar_add = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_CHILD_BUTTON, NULL,
						"", _("Add an entry to the menu"), NULL,
						tmp_toolbar_icon, GTK_SIGNAL_FUNC (add_entry_cb), me);
  tmp_toolbar_icon = gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_LARGE_TOOLBAR);
  me->toolbar_del = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_CHILD_BUTTON, NULL,
						"", _("Delete the current entry"), NULL,
						tmp_toolbar_icon, GTK_SIGNAL_FUNC (delete_entry_cb), me);
  tmp_toolbar_icon = gtk_image_new_from_stock (GTK_STOCK_GO_UP, GTK_ICON_SIZE_LARGE_TOOLBAR);
  me->toolbar_up = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_CHILD_BUTTON, NULL,
					       "", _("Move the current entry up"), NULL,
					       tmp_toolbar_icon, GTK_SIGNAL_FUNC (entry_up_cb), me);
  tmp_toolbar_icon = gtk_image_new_from_stock (GTK_STOCK_GO_DOWN, GTK_ICON_SIZE_LARGE_TOOLBAR);
  me->toolbar_down = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_CHILD_BUTTON, NULL,
						 "", _("Move the current entry down"), NULL,
						 tmp_toolbar_icon, GTK_SIGNAL_FUNC (entry_down_cb), me);
  gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));
  tmp_toolbar_icon = gtk_image_new_from_stock (GTK_STOCK_ZOOM_OUT, GTK_ICON_SIZE_LARGE_TOOLBAR);
  me->toolbar_collapse = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_CHILD_BUTTON, NULL,
						     "", _("Collapse the tree"), NULL,
						     tmp_toolbar_icon, GTK_SIGNAL_FUNC (collapse_tree_cb), me);
  tmp_toolbar_icon = gtk_image_new_from_stock (GTK_STOCK_ZOOM_IN, GTK_ICON_SIZE_LARGE_TOOLBAR);
  me->toolbar_expand = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_CHILD_BUTTON, NULL,
						   "", _("Expand the tree"), NULL,
						   tmp_toolbar_icon, GTK_SIGNAL_FUNC (expand_tree_cb), me);


  /* Tree View inspirated from Gaim */
  me->treestore = gtk_tree_store_new (NUM_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING,
				      G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_POINTER);
  scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
				  GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (main_vbox), scrolledwindow, TRUE, TRUE, 0);

  me->treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL(me->treestore));
  gtk_container_add (GTK_CONTAINER (scrolledwindow), me->treeview);

  /* Columns */
  icon_cell = gtk_cell_renderer_pixbuf_new();
  name_cell = gtk_cell_renderer_text_new ();
  command_cell = gtk_cell_renderer_text_new();
  hidden_cell = gtk_cell_renderer_toggle_new();
  pointer_cell = gtk_cell_renderer_text_new();

  name_column = gtk_tree_view_column_new ();

  gtk_tree_view_column_pack_start (name_column, icon_cell, FALSE);
  gtk_tree_view_column_set_attributes (name_column, icon_cell, "pixbuf", ICON_COLUMN, NULL);
  g_object_set (icon_cell, "xalign", 0.0, "ypad", 0, NULL);

  gtk_tree_view_column_pack_start (name_column, name_cell, TRUE);
  gtk_tree_view_column_set_attributes (name_column, name_cell, "markup", NAME_COLUMN, NULL);
  g_object_set (name_cell, "ypad", 0, "yalign", 0.5, NULL);
  gtk_tree_view_column_set_title  (name_column, _("Name"));

  command_column = gtk_tree_view_column_new_with_attributes (_("Command"), command_cell, "markup",
							     COMMAND_COLUMN, NULL);
  hidden_column = gtk_tree_view_column_new_with_attributes (_("Hidden"), hidden_cell, "active",
							    HIDDEN_COLUMN, NULL);
  pointer_column = gtk_tree_view_column_new_with_attributes (_("Pointer"), pointer_cell, "text",
							     POINTER_COLUMN, NULL);

  gtk_tree_view_append_column (GTK_TREE_VIEW (me->treeview), name_column);
  gtk_tree_view_append_column (GTK_TREE_VIEW (me->treeview), command_column);
  gtk_tree_view_append_column (GTK_TREE_VIEW (me->treeview), hidden_column);
  gtk_tree_view_append_column (GTK_TREE_VIEW (me->treeview), pointer_column);

  gtk_tree_view_column_set_alignment (name_column, 0.5);
  gtk_tree_view_column_set_alignment (command_column, 0.5);
  gtk_tree_view_column_set_alignment (hidden_column, 0.5);

  gtk_tree_view_column_set_max_width (command_column, 200);
  gtk_tree_view_column_set_max_width (hidden_column, 50);

  gtk_tree_view_column_set_visible (GTK_TREE_VIEW_COLUMN (pointer_column), FALSE);

  /* Status bar */
  statusbar = gtk_statusbar_new ();
  gtk_box_pack_start (GTK_BOX (main_vbox), statusbar, FALSE, FALSE, 0);

  /* Connect signals */

  /* For the menu */
  g_signal_connect (G_OBJECT (me->file_menu_new), "activate",
                    G_CALLBACK (new_menu_cb), me);
  g_signal_connect (G_OBJECT (me->file_menu_open), "activate",
                    G_CALLBACK (menu_open_cb), me);
  g_signal_connect (G_OBJECT (me->file_menu_default), "activate",
                    G_CALLBACK (menu_open_default_cb), me);
  g_signal_connect (G_OBJECT (me->file_menu_save), "activate",
                    G_CALLBACK (menu_save_cb), me);
  g_signal_connect (G_OBJECT (me->file_menu_saveas), "activate",
                    G_CALLBACK (menu_saveas_cb), me);
  g_signal_connect (G_OBJECT (me->file_menu_close), "activate",
                    G_CALLBACK (close_menu_cb), me);
  g_signal_connect (G_OBJECT (me->file_menu_exit), "activate",
                    G_CALLBACK (confirm_quit_cb), me);

  g_signal_connect (G_OBJECT (me->edit_menu_add), "activate",
		    G_CALLBACK (add_entry_cb), me);
  g_signal_connect (G_OBJECT (me->edit_menu_add_menu), "activate",
		    G_CALLBACK (add_menu_cb), me);
  g_signal_connect (G_OBJECT (me->edit_menu_del), "activate",
		    G_CALLBACK (delete_entry_cb), me);
  g_signal_connect (G_OBJECT (me->edit_menu_up), "activate",
		    G_CALLBACK (entry_up_cb), me);
  g_signal_connect (G_OBJECT (me->edit_menu_down), "activate",
		    G_CALLBACK (entry_down_cb), me);

  g_signal_connect (G_OBJECT (me->help_menu_about), "activate",
		    G_CALLBACK (about_cb), me);

  /* For the treeview */
  g_signal_connect (G_OBJECT (me->treeview), "button-press-event",
		    G_CALLBACK (treeview_button_pressed_cb), me);
  g_signal_connect (G_OBJECT (me->treeview), "row-activated",
		    G_CALLBACK (treeview_activate_cb), me);
  g_signal_connect (G_OBJECT (me->treeview), "cursor-changed",
		    G_CALLBACK (treeview_cursor_changed_cb), me);
  g_signal_connect (G_OBJECT (hidden_cell), "toggled",
		    G_CALLBACK (visible_column_toggled_cb), me);

  /* Set up dnd */
  gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (me->treeview),
					  GDK_BUTTON1_MASK, gte, 3, 
					  GDK_ACTION_MOVE);
  gtk_tree_view_enable_model_drag_dest (GTK_TREE_VIEW (me->treeview),
					gte, 3,	GDK_ACTION_COPY);

  g_signal_connect (G_OBJECT (me->treeview), "drag-data-received",
		    G_CALLBACK(treeview_drag_data_rcv_cb), me);
  g_signal_connect (G_OBJECT (me->treeview), "drag-data-get",
		    G_CALLBACK (treeview_drag_data_get_cb), me);

  /* Add accelerators */
  gtk_window_add_accel_group (GTK_WINDOW (me->main_window), me->accel_group);

  /* Deactivate the widgets not usable */
  gtk_widget_set_sensitive (me->file_menu_save, FALSE);
  gtk_widget_set_sensitive (me->file_menu_saveas, FALSE);
  gtk_widget_set_sensitive (me->file_menu_close, FALSE);

  gtk_widget_set_sensitive (me->edit_menu_add, FALSE);
  gtk_widget_set_sensitive (me->edit_menu_add_menu, FALSE);
  gtk_widget_set_sensitive (me->edit_menu_del, FALSE);
  gtk_widget_set_sensitive (me->edit_menu_up, FALSE);
  gtk_widget_set_sensitive (me->edit_menu_down, FALSE);
  
  gtk_widget_set_sensitive (me->toolbar_save, FALSE);
  gtk_widget_set_sensitive (me->toolbar_close, FALSE);
  gtk_widget_set_sensitive (me->toolbar_add, FALSE);
  gtk_widget_set_sensitive (me->toolbar_del, FALSE);
  gtk_widget_set_sensitive (me->toolbar_up, FALSE);
  gtk_widget_set_sensitive (me->toolbar_down, FALSE);

  gtk_widget_set_sensitive (me->treeview, FALSE);

  gtk_widget_set_sensitive (me->edit_menu_item, FALSE);

  /* Show all */
  gtk_tree_view_expand_all (GTK_TREE_VIEW (me->treeview));
}

/********************/
/* Open a menu file */
/********************/
static void open_menu_file(gchar *menu_file, MenuEditor *me)
{
  gchar *window_title = NULL;
  xmlNodePtr root;
  xmlNodePtr menu_entry;

  if(menu_file == NULL){
    g_warning ("%s: open_menu_file(): menu_file == NULL", PACKAGE);
    return;
  }

  /* Open the menu file */
  g_stpcpy (me->menu_file_name, menu_file);
  me->xml_menu_file = xmlParseFile (menu_file);

  /* Check if the format is correct */
  if (me->xml_menu_file == NULL){
    xfce_err ( _("Corrupted file or incorrect file format !"));
#ifdef DEBUG
    g_warning ( "%s\n", "Corrupted file or incorrect file format !");
#endif
    
    xmlFreeDoc (me->xml_menu_file);
    me->xml_menu_file = NULL;
    return;
  }

  root = xmlDocGetRootElement (me->xml_menu_file);

  if(root == NULL){
    xfce_err ( _("No root element in file !"));
#ifdef DEBUG
    g_warning ( "%s\n", "No root element in file !");
#endif
    
    xmlFreeDoc (me->xml_menu_file);
    me->xml_menu_file = NULL;
    return;
  }
  
  if(xmlStrcmp(root->name,(xmlChar*)"xfdesktop-menu")){
    xfce_err ( _("Bad datafile format !"));
#ifdef DEBUG
    g_warning ( "%s\n", "Bad datafile format !");
#endif
    
    xmlFreeDoc (me->xml_menu_file);
    me->xml_menu_file = NULL;
    return;
  }

  menu_entry = root->xmlChildrenNode;
  load_menu_in_tree (menu_entry, NULL, me);

  /* Terminate operations */
  gtk_tree_view_expand_all (GTK_TREE_VIEW (me->treeview));

  gtk_widget_set_sensitive (me->toolbar_close, TRUE);

  gtk_widget_set_sensitive (me->edit_menu_item, TRUE);

  gtk_widget_set_sensitive (me->file_menu_saveas, TRUE);
  gtk_widget_set_sensitive (me->file_menu_close, TRUE);
  gtk_widget_set_sensitive (me->treeview, TRUE);

  gtk_widget_set_sensitive (me->toolbar_add, FALSE);
  gtk_widget_set_sensitive (me->toolbar_del, FALSE);
  gtk_widget_set_sensitive (me->toolbar_up, FALSE);
  gtk_widget_set_sensitive (me->toolbar_down, FALSE);

  gtk_widget_set_sensitive (me->edit_menu_add, FALSE);
  gtk_widget_set_sensitive (me->edit_menu_add_menu, FALSE);
  gtk_widget_set_sensitive (me->edit_menu_del, FALSE);
  gtk_widget_set_sensitive (me->edit_menu_up, FALSE);
  gtk_widget_set_sensitive (me->edit_menu_down, FALSE);

  me->menu_modified = FALSE;

  /* Set window's title */
  window_title = g_strdup_printf ("Xfce4-MenuEditor - %s", menu_file);
  gtk_window_set_title (GTK_WINDOW (me->main_window), window_title);
  g_free (window_title);
}

/***********************************************************/
/* Get the menu file in use (inspired from xfdesktop code) */
/***********************************************************/
/* FIXME: move to common/ */
static gchar* get_default_menu_file ()
{
  gchar filename[PATH_MAX];
  const gchar *env = g_getenv("XFCE_DISABLE_USER_CONFIG");

  if(!env || !strcmp(env, "0")) {
    gchar *usermenu = xfce_resource_lookup(XFCE_RESOURCE_CONFIG,"xfce4/desktop/menu.xml");
    if(g_file_test(usermenu, G_FILE_TEST_IS_REGULAR))
      return usermenu;
    g_free(usermenu);
  }

  if(xfce_get_path_localized(filename, PATH_MAX, SEARCHPATH,
			     "menu.xml", G_FILE_TEST_IS_REGULAR))
    {
      return g_strdup(filename);
    }

  g_warning("%s: Could not locate a menu definition file", PACKAGE);

  return NULL;
}

/***************/
/* Entry point */
/***************/
int main (int argc, char *argv[]) 
{
  MenuEditor *me;

  me = g_new0 (MenuEditor, 1);

  if(argc > 1 && (!strcmp(argv[1], "--version") || !strcmp(argv[1], "-V"))) {
    g_print("\tThis is xfce4-menueditor version %s for Xfce %s\n", VERSION,
	    xfce_version_string());
    g_print("\tbuilt with GTK+-%d.%d.%d and LIBXML2 v%s,\n", GTK_MAJOR_VERSION,
	    GTK_MINOR_VERSION, GTK_MICRO_VERSION, LIBXML_DOTTED_VERSION);
    g_print("\tlinked with GTK+-%d.%d.%d.\n", gtk_major_version,
	    gtk_minor_version, gtk_micro_version);
    exit(0);
  }

  xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

  me->xml_menu_file=NULL;

  gtk_init (&argc, &argv);

  create_main_window (me);
  gtk_widget_show_all (me->main_window);

  if(argc>1){
    if(g_file_test (argv[1], G_FILE_TEST_EXISTS))
      open_menu_file (argv[1], me);
    else{
      gchar *error_message;

      error_message = g_strdup_printf (_("File %s doesn't exist !"), argv[1]);
      
      xfce_err (error_message);
 
      g_free (error_message);
    }
  }else
    menu_open_default_cb (NULL, me);

  gtk_main ();

  return 0;
}
