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

#include "menueditor.h"

#include "add_dialog.h"
#include "add_menu_dialog.h"
#include "about_dialog.h"
#include "edit_dialog.h"
#include "move.h"
#include "new.h"
#include "dnd.h"

/* Pixmaps */
#include "me-icon16.xpm"
#include "me-icon32.xpm"
#include "me-icon24.xpm"
#include "me-icon48.xpm"

/* Search path for menu.xml file */
#define SEARCHPATH (SYSCONFDIR G_DIR_SEPARATOR_S "xfce4" G_DIR_SEPARATOR_S "%F.%L:"\
                    SYSCONFDIR G_DIR_SEPARATOR_S "xfce4" G_DIR_SEPARATOR_S "%F.%l:"\
                    SYSCONFDIR G_DIR_SEPARATOR_S "xfce4" G_DIR_SEPARATOR_S "%F")

/**************/
/* Prototypes */
/**************/

static gchar* get_default_menu_file ();
void open_menu_file(gchar *menu_file);

/* Callbacks */
void quit_cb(GtkWidget *widget, gpointer data);
void not_yet_cb(GtkWidget *widget, gpointer data);
gboolean confirm_quit_cb(GtkWidget *widget, gpointer data);
void filesel_ok(GtkWidget *widget, GtkFileSelection *filesel_dialog);
void menu_open_cb(GtkWidget *widget, gpointer data);
void menu_open_default_cb(GtkWidget *widget, gpointer data);
void menu_save_cb(GtkWidget *widget, gpointer data);
void menu_saveas_cb(GtkWidget *widget, gpointer data);
void close_menu_cb(GtkWidget *widget, gpointer data);
void treeview_cursor_changed_cb(GtkTreeView *treeview,gpointer user_data);
void visible_column_toggled_cb(GtkCellRendererToggle *toggle,
			       gchar *str_path,
			       gpointer data);
void filesel_saveas_ok(GtkWidget *widget, GtkFileSelection *filesel_dialog);
void delete_entry_cb(GtkWidget *widget, gpointer data);

/* Main window */
void create_main_window();

/* Load the menu in the tree */
void load_menu_in_tree(xmlNodePtr menu, GtkTreeIter *p);

/**********************************************/
/* mcs client code copy of Brian J. Tarricone */
/**********************************************/
static time_t last_theme_change = 0;
static McsClient *client = NULL;

static GdkFilterReturn
client_event_filter1(GdkXEvent * xevent, GdkEvent * event, gpointer data)
{
	if(mcs_client_process_event (client, (XEvent *)xevent))
		return GDK_FILTER_REMOVE;
	else
		return GDK_FILTER_CONTINUE;
}

static void
mcs_watch_cb(Window window, Bool is_start, long mask, void *cb_data)
{
	GdkWindow *gdkwin;

	gdkwin = gdk_window_lookup (window);

	if(is_start)
		gdk_window_add_filter (gdkwin, client_event_filter1, NULL);
	else
		gdk_window_remove_filter (gdkwin, client_event_filter1, NULL);
}

static void
mcs_notify_cb(const gchar *name, const gchar *channel_name, McsAction action,
              McsSetting *setting, void *cb_data)
{
  if(strcasecmp(channel_name, CHANNEL) || !setting)
    return;
  
  if((action==MCS_ACTION_NEW || action==MCS_ACTION_CHANGED) &&
     !strcmp(setting->name, "theme") && setting->type==MCS_TYPE_STRING)
    {
      gchar *origin = g_strdup_printf("%s:%d", __FILE__, __LINE__);
      gtk_settings_set_string_property(gtk_settings_get_default(),
				       "gtk-icon-theme-name", setting->data.v_string, origin);

      g_free(origin);
      last_theme_change = time(NULL);
    }
}

/* Load an icon from the theme and resize it */
GdkPixbuf* load_icon_from_theme(gchar* icon_name)
{
  GdkPixbuf *tmpicon;
  GdkPixbuf *icon;

  tmpicon = gtk_icon_theme_load_icon (menueditor_app.icon_theme, icon_name, 24, 0, NULL);
  icon = gdk_pixbuf_scale_simple(tmpicon, 24, 24, GDK_INTERP_BILINEAR);
  g_object_unref(G_OBJECT(tmpicon));

  return icon;
}

/****************************************************/
/* browse for a command and set it in entry_command */
/****************************************************/
void browse_command_cb(GtkWidget *widget, GtkEntry *entry_command){
  GtkWidget *filesel_dialog;

  filesel_dialog = gtk_file_selection_new(_("Select command"));

  if(gtk_dialog_run(GTK_DIALOG(filesel_dialog)) == GTK_RESPONSE_OK){
    gtk_entry_set_text (entry_command,
			gtk_file_selection_get_filename (GTK_FILE_SELECTION(filesel_dialog)));
  }

  gtk_widget_destroy(GTK_WIDGET(filesel_dialog));
}

void browse_icon_cb(GtkWidget *widget, GtkEntry *entry_icon){
  GtkWidget *filesel_dialog;

  filesel_dialog = preview_file_selection_new(_("Select icon"), TRUE);

  if(gtk_dialog_run(GTK_DIALOG(filesel_dialog)) == GTK_RESPONSE_OK){
    gtk_entry_set_text (entry_icon,
			gtk_file_selection_get_filename (GTK_FILE_SELECTION(filesel_dialog)));
  }

  gtk_widget_destroy(GTK_WIDGET(filesel_dialog));
}


/*************/
/* callbacks */
/*************/
void not_yet_cb(GtkWidget *widget, gpointer data)
{
  GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(menueditor_app.main_window),
					      GTK_DIALOG_DESTROY_WITH_PARENT,
					      GTK_MESSAGE_WARNING,
					      GTK_BUTTONS_OK,
					      _("Not yet implemented !!!"));
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
  
}

/* Ask confirmation when exiting program */
gboolean confirm_quit_cb(GtkWidget *widget,gpointer data)
{
  GtkWidget *dialog;

  if(menueditor_app.menu_modified==TRUE){
    dialog = gtk_message_dialog_new(GTK_WINDOW(menueditor_app.main_window),
				    GTK_DIALOG_DESTROY_WITH_PARENT,
				    GTK_MESSAGE_QUESTION,
				    GTK_BUTTONS_YES_NO,
				    _("Do you want to save before closing the file ?"));
    
   
    if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_YES)
      menu_save_cb(widget,data);
        
    gtk_widget_destroy(dialog);
  }

  xmlFreeDoc(menueditor_app.xml_menu_file); 
  menueditor_app.xml_menu_file=NULL;
  gtk_main_quit();

  return FALSE;
}

/*****************************/
/* Load the menu in the tree */
/*****************************/
void load_menu_in_tree(xmlNodePtr menu, GtkTreeIter *p)
{
  GtkTreeIter c;

  while(menu != NULL){
      gboolean hidden=FALSE;
    
    /* separator */
    if(!xmlStrcmp(menu->name,(xmlChar*)"separator")){
      gchar *name=NULL;

      if(xmlGetProp(menu,"visible") && !xmlStrcmp(xmlGetProp(menu,"visible"),(xmlChar*)"no"))
	hidden=TRUE;

      name = g_strdup_printf(SEPARATOR_FORMAT,
			     _("--- separator ---"));

      gtk_tree_store_append (menueditor_app.treestore, &c, p);
      gtk_tree_store_set (menueditor_app.treestore, &c, 0, NULL,
			  NAME_COLUMN, name, 
			  COMMAND_COLUMN, "",
			  HIDDEN_COLUMN, hidden,
			  POINTER_COLUMN, menu, -1);

      g_free(name);
    }
    /* launcher */
    if(!xmlStrcmp(menu->name,(xmlChar*)"app")){
      GdkPixbuf *icon = NULL;
      gchar *name=NULL;
      gchar *command=NULL;

      if(xmlGetProp(menu,"visible") && !xmlStrcmp(xmlGetProp(menu,"visible"),(xmlChar*)"no"))
	hidden=TRUE;
      
      name = g_strdup_printf(NAME_FORMAT,
			     xmlGetProp(menu,"name"));
      command = g_strdup_printf(COMMAND_FORMAT,
				xmlGetProp(menu,"cmd"));

      gtk_tree_store_append (menueditor_app.treestore, &c, p);
      gtk_tree_store_set (menueditor_app.treestore, &c, 
			  NAME_COLUMN, name,
			  COMMAND_COLUMN, command,
			  HIDDEN_COLUMN, hidden,
			  POINTER_COLUMN, menu, -1);
      if(xmlGetProp(menu,"icon"))
	/* Load the icon */
	icon = load_icon_from_theme(xmlGetProp(menu,"icon"));

      gtk_tree_store_set (menueditor_app.treestore, &c,
			  ICON_COLUMN, icon, -1);
	
      g_free(command);
      g_free(name);
    }
  
    /* menu */
    if(!xmlStrcmp(menu->name,(xmlChar*)"menu")){
      gchar *name=NULL;

      if(xmlGetProp(menu,"visible") && !xmlStrcmp(xmlGetProp(menu,"visible"),(xmlChar*)"no"))
	hidden=TRUE;

      name = g_strdup_printf(MENU_FORMAT,
			      xmlGetProp(menu,"name"));
      
      gtk_tree_store_append (menueditor_app.treestore, &c, p);
      gtk_tree_store_set (menueditor_app.treestore, &c, 0, NULL,
			  NAME_COLUMN, name,
			  COMMAND_COLUMN, "",
			  HIDDEN_COLUMN, hidden,
			  POINTER_COLUMN, menu, -1);
      load_menu_in_tree(menu->xmlChildrenNode,&c);

      g_free(name);
    }

    /* include */
    if(!xmlStrcmp(menu->name,(xmlChar*)"include")){
      gchar *src=NULL;
      gchar *name=NULL;

      if(xmlGetProp(menu,"visible") && !xmlStrcmp(xmlGetProp(menu,"visible"),(xmlChar*)"no"))
	hidden=TRUE;

      name = g_strdup_printf(INCLUDE_FORMAT,_("--- include ---"));

      if(!xmlStrcmp(xmlGetProp(menu,"type"),(xmlChar*)"system"))
	src = g_strdup_printf(INCLUDE_PATH_FORMAT,_("system"));
      else
	src = g_strdup_printf(INCLUDE_PATH_FORMAT,xmlGetProp(menu,"src"));
      
      gtk_tree_store_append (menueditor_app.treestore, &c, p);
      gtk_tree_store_set (menueditor_app.treestore, &c, 0, NULL,
			  NAME_COLUMN, name,
			  COMMAND_COLUMN, src,
			  HIDDEN_COLUMN, hidden,
			  POINTER_COLUMN, menu, -1);

      g_free(src);
      g_free(name);
    }

    /* builtin */
    if(!xmlStrcmp(menu->name,(xmlChar*)"builtin")){
      gchar *name=NULL;
      gchar *cmd=NULL;

      if(xmlGetProp(menu,"visible") && !xmlStrcmp(xmlGetProp(menu,"visible"),(xmlChar*)"no"))
	hidden=TRUE;

      name = g_strdup_printf(NAME_FORMAT, xmlGetProp(menu,"name"));
      cmd = g_strdup_printf(COMMAND_FORMAT, xmlGetProp(menu,"cmd"));

      gtk_tree_store_append (menueditor_app.treestore, &c, p);
      gtk_tree_store_set (menueditor_app.treestore, &c, 0, NULL,
			  NAME_COLUMN, name,
			  COMMAND_COLUMN, cmd,
			  HIDDEN_COLUMN, hidden,
			  POINTER_COLUMN, menu, -1);

      if(!xmlStrcmp(xmlGetProp(menu,"cmd"),(xmlChar*)"quit")){
	GdkPixbuf *icon;

	icon = load_icon_from_theme ("minipower");
	gtk_tree_store_set (menueditor_app.treestore, &c,
			    ICON_COLUMN, icon, -1);
      }

      g_free(name);
      g_free(cmd);
    }
    /* title */
    if(!xmlStrcmp(menu->name,(xmlChar*)"title")){
      gchar *title=NULL;

      if(!xmlGetProp(menu,"visible") && !xmlStrcmp(xmlGetProp(menu,"visible"),(xmlChar*)"no"))
	hidden=TRUE;

      title = g_strdup_printf(TITLE_FORMAT,
			     xmlGetProp(menu,"name"));

      gtk_tree_store_append (menueditor_app.treestore, &c, p);
      gtk_tree_store_set (menueditor_app.treestore, &c, 0, NULL, 
			  NAME_COLUMN, title,
			  COMMAND_COLUMN, "", 
			  HIDDEN_COLUMN, hidden, 
			  POINTER_COLUMN, menu , -1);

      g_free(title);
    }
    menu = menu->next;
  }

}

/* File Selection ok button callback */
void filesel_ok(GtkWidget *widget, GtkFileSelection *filesel_dialog)
{
  gtk_widget_hide(GTK_WIDGET(filesel_dialog));

  if(menueditor_app.xml_menu_file != NULL){
    gtk_tree_store_clear(GTK_TREE_STORE(menueditor_app.treestore));
    xmlFreeDoc(menueditor_app.xml_menu_file);
    menueditor_app.xml_menu_file=NULL;
  }

#ifdef DEBUG
  g_print("%s\n",gtk_file_selection_get_filename (filesel_dialog));
#endif

  open_menu_file((gchar*)gtk_file_selection_get_filename (filesel_dialog));
}

/*****************************/
/* Open the menu file in use */
/*****************************/
void menu_open_default_cb(GtkWidget *widget, gpointer data)
{
  gchar *filename = get_default_menu_file();
  
  /* Check if there is no other file opened */
  if(menueditor_app.xml_menu_file != NULL){
    if(menueditor_app.menu_modified==TRUE){
      GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(menueditor_app.main_window),
				      GTK_DIALOG_DESTROY_WITH_PARENT,
				      GTK_MESSAGE_QUESTION,
				      GTK_BUTTONS_YES_NO,
				      _("Do you want to save before closing the file ?"));
      
      if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_YES)
	menu_save_cb(widget,NULL);

      gtk_widget_destroy(dialog);
    }

  }

  if(menueditor_app.xml_menu_file != NULL){
    gtk_tree_store_clear(GTK_TREE_STORE(menueditor_app.treestore));
    xmlFreeDoc(menueditor_app.xml_menu_file);
    menueditor_app.xml_menu_file=NULL;
  }

  open_menu_file(filename);

  g_free(filename);
}

/********************************************/
/* Browse for menu file to open and open it */
/********************************************/
void menu_open_cb(GtkWidget *widget, gpointer data)
{
  GtkWidget *filesel_dialog;

  /* Check if there is no other file opened */
  if(menueditor_app.xml_menu_file != NULL){
    if(menueditor_app.menu_modified==TRUE){
      GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(menueditor_app.main_window),
				      GTK_DIALOG_DESTROY_WITH_PARENT,
				      GTK_MESSAGE_QUESTION,
				      GTK_BUTTONS_YES_NO,
				      _("Do you want to save before closing the file ?"));
      
      if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_YES)
	menu_save_cb(widget,NULL);

      gtk_widget_destroy(dialog);
    }

  }


  filesel_dialog = gtk_file_selection_new(_("Menu file selection"));
  gtk_file_selection_set_filename (GTK_FILE_SELECTION(filesel_dialog), 
				   "menu.xml");

  g_signal_connect (G_OBJECT (GTK_FILE_SELECTION (filesel_dialog)->ok_button),
		    "clicked", G_CALLBACK (filesel_ok), (gpointer) filesel_dialog);
  
  g_signal_connect_swapped (G_OBJECT (GTK_FILE_SELECTION (filesel_dialog)->cancel_button),
			    "clicked", G_CALLBACK (gtk_widget_destroy),
			    G_OBJECT (filesel_dialog));

  gtk_widget_show (filesel_dialog);
}

void quit_cb(GtkWidget *widget, gpointer data)
{
  gtk_main_quit();
}

/* Save the menu file */
void menu_save_cb(GtkWidget *widget, gpointer data)
{
  menueditor_app.menu_modified=FALSE;
  gtk_widget_set_sensitive(menueditor_app.file_menu.save,FALSE);
  gtk_widget_set_sensitive(menueditor_app.main_toolbar.save,FALSE);
  xmlSaveFormatFile(menueditor_app.menu_file_name,menueditor_app.xml_menu_file,1);
}

/* File Selection ok button callback */
void filesel_saveas_ok(GtkWidget *widget, GtkFileSelection *filesel_dialog)
{
  gtk_widget_hide(GTK_WIDGET(filesel_dialog));

  menueditor_app.menu_modified=FALSE;
  gtk_widget_set_sensitive(menueditor_app.file_menu.save,FALSE);
  gtk_widget_set_sensitive(menueditor_app.main_toolbar.save,FALSE);
  g_stpcpy(menueditor_app.menu_file_name,gtk_file_selection_get_filename(filesel_dialog)); 
  xmlSaveFormatFile(menueditor_app.menu_file_name,menueditor_app.xml_menu_file,1);
}

/* Ask the filename and save the menu into */
void menu_saveas_cb(GtkWidget *widget, gpointer data)
{
  GtkWidget *filesel_dialog;

  filesel_dialog = gtk_file_selection_new(_("Menu file selection"));
  gtk_file_selection_set_filename (GTK_FILE_SELECTION(filesel_dialog), 
				   "menu.xml");

  g_signal_connect (G_OBJECT (GTK_FILE_SELECTION (filesel_dialog)->ok_button),
		    "clicked", G_CALLBACK (filesel_saveas_ok), (gpointer) filesel_dialog);
  
  g_signal_connect_swapped (G_OBJECT (GTK_FILE_SELECTION (filesel_dialog)->cancel_button),
			    "clicked", G_CALLBACK (gtk_widget_destroy),
			    G_OBJECT (filesel_dialog));

  gtk_widget_show (filesel_dialog);
}

/* Close the menu file and prompt to save if it was modified */
void close_menu_cb(GtkWidget *widget, gpointer data)
{
  if(menueditor_app.menu_modified==TRUE){
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(menueditor_app.main_window),
					     GTK_DIALOG_DESTROY_WITH_PARENT,
					     GTK_MESSAGE_QUESTION,
					     GTK_BUTTONS_YES_NO,
					     _("Do you want to save before closing the file ?"));

    if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_YES)
      menu_save_cb(widget,data);

    gtk_widget_destroy(dialog);
  }

  xmlFreeDoc(menueditor_app.xml_menu_file);
  menueditor_app.xml_menu_file=NULL;

  gtk_tree_store_clear(GTK_TREE_STORE(menueditor_app.treestore));

  menueditor_app.menu_modified=FALSE;
  
  gtk_widget_set_sensitive(menueditor_app.main_toolbar.close,FALSE);
  gtk_widget_set_sensitive(menueditor_app.main_toolbar.save,FALSE);
  gtk_widget_set_sensitive(menueditor_app.main_toolbar.add,FALSE);
  gtk_widget_set_sensitive(menueditor_app.main_toolbar.del,FALSE);
  gtk_widget_set_sensitive(menueditor_app.main_toolbar.up,FALSE);
  gtk_widget_set_sensitive(menueditor_app.main_toolbar.down,FALSE);

  gtk_widget_set_sensitive(menueditor_app.file_menu.close,FALSE);
  gtk_widget_set_sensitive(menueditor_app.file_menu.save,FALSE);
  gtk_widget_set_sensitive(menueditor_app.file_menu.saveas,FALSE);

  gtk_widget_set_sensitive(menueditor_app.edit_menu.menu_item,FALSE);

  gtk_widget_set_sensitive(menueditor_app.treeview,FALSE);
}

void treeview_cursor_changed_cb(GtkTreeView *treeview,gpointer user_data)
{
  GtkTreeIter iter;
  GtkTreeModel *tree_model=GTK_TREE_MODEL(menueditor_app.treestore);

  if(gtk_tree_selection_get_selected (gtk_tree_view_get_selection(GTK_TREE_VIEW(menueditor_app.treeview)),
					 &tree_model,
					 &iter)){
    gtk_widget_set_sensitive(menueditor_app.main_toolbar.add,TRUE);
    gtk_widget_set_sensitive(menueditor_app.main_toolbar.del,TRUE);
    gtk_widget_set_sensitive(menueditor_app.main_toolbar.up,TRUE);
    gtk_widget_set_sensitive(menueditor_app.main_toolbar.down,TRUE);

    gtk_widget_set_sensitive(menueditor_app.edit_menu.add_menu,TRUE);
    gtk_widget_set_sensitive(menueditor_app.edit_menu.add,TRUE);
    gtk_widget_set_sensitive(menueditor_app.edit_menu.del,TRUE);
    gtk_widget_set_sensitive(menueditor_app.edit_menu.up,TRUE);
    gtk_widget_set_sensitive(menueditor_app.edit_menu.down,TRUE);
  }

}


void delete_entry_cb(GtkWidget *widget, gpointer data)
{
  GtkTreeSelection *selection=gtk_tree_view_get_selection(GTK_TREE_VIEW(menueditor_app.treeview));
  GtkTreeModel *model=GTK_TREE_MODEL(menueditor_app.treestore);
  GtkTreeIter iter;

  if(gtk_tree_selection_get_selected (selection,&model,&iter)){
    xmlNodePtr node;
    GdkPixbuf *icon=NULL;

    GValue val_node = { 0, };
    GValue val_icon = { 0, };

    /* Retrieve the xmlNodePtr of the menu entry and free it */
    gtk_tree_model_get_value (GTK_TREE_MODEL(menueditor_app.treestore), &iter, POINTER_COLUMN, &val_node);
    node = g_value_get_pointer(&val_node);
    xmlUnlinkNode(node);
    xmlFreeNode(node);
    /* Retrieve the GdkPixbuf pointer on the icon and free it */
    gtk_tree_model_get_value (GTK_TREE_MODEL(menueditor_app.treestore), &iter, ICON_COLUMN, &val_icon);
    icon = g_value_get_object(&val_icon);
    if(G_IS_OBJECT (icon))
      g_object_unref(icon);

    gtk_tree_store_remove (GTK_TREE_STORE(menueditor_app.treestore),&iter);

    /* Modified ! */
    menueditor_app.menu_modified=TRUE;
    gtk_widget_set_sensitive(menueditor_app.file_menu.save,TRUE);
    gtk_widget_set_sensitive(menueditor_app.main_toolbar.save,TRUE);
  }

  gtk_widget_set_sensitive(menueditor_app.main_toolbar.del,FALSE);
  gtk_widget_set_sensitive(menueditor_app.main_toolbar.up,FALSE);
  gtk_widget_set_sensitive(menueditor_app.main_toolbar.down,FALSE);

  gtk_widget_set_sensitive(menueditor_app.edit_menu.del,FALSE);
  gtk_widget_set_sensitive(menueditor_app.edit_menu.up,FALSE);
  gtk_widget_set_sensitive(menueditor_app.edit_menu.down,FALSE);
}



/*******************************/
/* Click on the visible toggle */
/*******************************/
void visible_column_toggled_cb(GtkCellRendererToggle *toggle,
			       gchar *str_path,
			       gpointer data)
{
  GtkTreePath *path = gtk_tree_path_new_from_string(str_path);
  GtkTreeModel *model=GTK_TREE_MODEL(menueditor_app.treestore);
  GtkTreeIter iter;
  GValue val = {0};
  GValue val2 = {0};
  xmlNodePtr node;
  gboolean hidden;

  /* Retrieve current iter */
  gtk_tree_model_get_iter (model,&iter,path);

  /* Retrieve state */
  gtk_tree_model_get_value (GTK_TREE_MODEL(menueditor_app.treestore), &iter, HIDDEN_COLUMN, &val);
  hidden = g_value_get_boolean(&val);

  gtk_tree_model_get_value (GTK_TREE_MODEL(menueditor_app.treestore), &iter, POINTER_COLUMN, &val2);
  node = g_value_get_pointer(&val2);

  /* Change state */
  if(hidden){
    gtk_tree_store_set (menueditor_app.treestore, &iter, HIDDEN_COLUMN, FALSE, -1);
    xmlSetProp(node,"visible","yes");
  }else{
    gtk_tree_store_set (menueditor_app.treestore, &iter, HIDDEN_COLUMN, TRUE, -1);
    xmlSetProp(node,"visible","no");
  }

  /* Modified ! */
  menueditor_app.menu_modified=TRUE;
  gtk_widget_set_sensitive(menueditor_app.file_menu.save,TRUE);
  gtk_widget_set_sensitive(menueditor_app.main_toolbar.save,TRUE);

  gtk_tree_path_free(path);
}

/***************/
/* Main window */
/***************/

void create_main_window()
{
  GtkWidget *main_vbox;
  GtkAccelGroup *accel_group;
  GtkWidget* image_file_menu;

  GtkWidget* tmp_toolbar_icon;
  GtkWidget* main_toolbar_save_button;
  /* Treeview */
  GtkWidget* scrolledwindow;
  GtkCellRenderer *name_cell, *command_cell, *hidden_cell, *pointer_cell, *icon_cell;
  GtkTreeViewColumn *name_column, *command_column, *hidden_column, *pointer_column;
  GtkTreeIter p, p2, c;

  GtkWidget *menu_separator;

  /* Status bar */
  GtkWidget* statusbar;

  /* Icons */
  GList *icons = NULL;
  GdkPixbuf *icon = NULL;

  /* DND */
  GtkTargetEntry gte[] = {{"XFCE_MENU_ENTRY", GTK_TARGET_SAME_WIDGET, 0},
			  {"text/plain",0, 1}};

  accel_group = gtk_accel_group_new ();
  
  /* Window */
  menueditor_app.main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(menueditor_app.main_window),
		       "XFCE4 - MenuEditor");
  gtk_signal_connect(GTK_OBJECT(menueditor_app.main_window), "delete_event",
		     GTK_SIGNAL_FUNC(confirm_quit_cb), NULL);
  gtk_window_set_default_size(GTK_WINDOW(menueditor_app.main_window),600,450);

  /* Set default icon */
  icon = gdk_pixbuf_new_from_xpm_data(icon16_xpm);
  if (icon)
    icons = g_list_append(icons,icon);
  icon = gdk_pixbuf_new_from_xpm_data(icon32_xpm);
  if (icon)
    icons = g_list_append(icons,icon);
  icon = gdk_pixbuf_new_from_xpm_data(icon48_xpm);
  if (icon)
    icons = g_list_append(icons,icon);

  gtk_window_set_default_icon_list(icons);
  g_list_free(icons);
  
  /* Main vbox */
  main_vbox = gtk_vbox_new(FALSE,0);
  gtk_container_add (GTK_CONTAINER (menueditor_app.main_window), main_vbox);

  /* Menu bar */
  menueditor_app.main_menubar = gtk_menu_bar_new();
  gtk_box_pack_start (GTK_BOX (main_vbox), menueditor_app.main_menubar, FALSE, FALSE, 0);

  /* File menu */
  menueditor_app.file_menu.menu_item = gtk_image_menu_item_new_with_mnemonic (_("_File"));
  gtk_container_add (GTK_CONTAINER (menueditor_app.main_menubar), menueditor_app.file_menu.menu_item);
  image_file_menu = gtk_image_new_from_stock ("gtk-open", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menueditor_app.file_menu.menu_item), image_file_menu);
  menueditor_app.file_menu.menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menueditor_app.file_menu.menu_item), menueditor_app.file_menu.menu);

  menueditor_app.file_menu.new = gtk_image_menu_item_new_from_stock("gtk-new", accel_group);
  gtk_container_add (GTK_CONTAINER (menueditor_app.file_menu.menu),menueditor_app.file_menu.new);
  menueditor_app.file_menu.open = gtk_image_menu_item_new_from_stock("gtk-open", accel_group);
  gtk_container_add (GTK_CONTAINER (menueditor_app.file_menu.menu),menueditor_app.file_menu.open);
  menueditor_app.file_menu.open_default = gtk_image_menu_item_new_with_mnemonic(_("Open default menu"));
  gtk_container_add (GTK_CONTAINER (menueditor_app.file_menu.menu),menueditor_app.file_menu.open_default);
  menueditor_app.file_menu.save = gtk_image_menu_item_new_from_stock("gtk-save", accel_group);
  gtk_container_add (GTK_CONTAINER (menueditor_app.file_menu.menu),menueditor_app.file_menu.save);
  menueditor_app.file_menu.saveas = gtk_image_menu_item_new_from_stock("gtk-save-as", accel_group);
  gtk_container_add (GTK_CONTAINER (menueditor_app.file_menu.menu),menueditor_app.file_menu.saveas);
  menueditor_app.file_menu.close = gtk_image_menu_item_new_from_stock("gtk-close", accel_group);
  gtk_container_add (GTK_CONTAINER (menueditor_app.file_menu.menu),menueditor_app.file_menu.close);
  menu_separator = gtk_separator_menu_item_new();
  gtk_container_add (GTK_CONTAINER (menueditor_app.file_menu.menu),menu_separator);
  menueditor_app.file_menu.exit = gtk_image_menu_item_new_from_stock ("gtk-quit", accel_group);
  gtk_container_add (GTK_CONTAINER (menueditor_app.file_menu.menu),menueditor_app.file_menu.exit);

  /* Edit menu */
  menueditor_app.edit_menu.menu_item = gtk_image_menu_item_new_with_mnemonic(_("_Edit"));
  gtk_container_add (GTK_CONTAINER (menueditor_app.main_menubar), menueditor_app.edit_menu.menu_item);
  image_file_menu = gtk_image_new_from_stock ("gtk-dnd", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menueditor_app.edit_menu.menu_item), image_file_menu);
  menueditor_app.edit_menu.menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menueditor_app.edit_menu.menu_item), menueditor_app.edit_menu.menu);
  menueditor_app.edit_menu.add = gtk_image_menu_item_new_from_stock ("gtk-add", accel_group);
  gtk_container_add (GTK_CONTAINER (menueditor_app.edit_menu.menu),menueditor_app.edit_menu.add);
  menueditor_app.edit_menu.add_menu = gtk_image_menu_item_new_with_mnemonic (_("Add an external menu..."));
  gtk_container_add (GTK_CONTAINER (menueditor_app.edit_menu.menu),menueditor_app.edit_menu.add_menu);
  menueditor_app.edit_menu.del = gtk_image_menu_item_new_from_stock ("gtk-remove", accel_group);
  gtk_container_add (GTK_CONTAINER (menueditor_app.edit_menu.menu),menueditor_app.edit_menu.del);
  menu_separator = gtk_separator_menu_item_new();
  gtk_container_add (GTK_CONTAINER (menueditor_app.edit_menu.menu),menu_separator);
  menueditor_app.edit_menu.up = gtk_image_menu_item_new_from_stock ("gtk-go-up", accel_group);
  gtk_container_add (GTK_CONTAINER (menueditor_app.edit_menu.menu),menueditor_app.edit_menu.up);
  menueditor_app.edit_menu.down = gtk_image_menu_item_new_from_stock ("gtk-go-down", accel_group);
  gtk_container_add (GTK_CONTAINER (menueditor_app.edit_menu.menu),menueditor_app.edit_menu.down);

  /* Help menu */
  menueditor_app.help_menu.menu_item = gtk_image_menu_item_new_with_mnemonic(_("_Help"));
  gtk_container_add (GTK_CONTAINER (menueditor_app.main_menubar), menueditor_app.help_menu.menu_item);
  image_file_menu = gtk_image_new_from_stock ("gtk-help", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menueditor_app.help_menu.menu_item), image_file_menu);
  menueditor_app.help_menu.menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menueditor_app.help_menu.menu_item), menueditor_app.help_menu.menu);
  
  menueditor_app.help_menu.about = gtk_menu_item_new_with_mnemonic(_("About ..."));
  gtk_container_add (GTK_CONTAINER (menueditor_app.help_menu.menu),menueditor_app.help_menu.about);

  /* Toolbar */
  menueditor_app.main_toolbar.toolbar = gtk_toolbar_new ();
  gtk_widget_show (menueditor_app.main_toolbar.toolbar);
  gtk_box_pack_start (GTK_BOX (main_vbox), menueditor_app.main_toolbar.toolbar, FALSE, FALSE, 0);
  gtk_toolbar_set_style (GTK_TOOLBAR (menueditor_app.main_toolbar.toolbar), GTK_TOOLBAR_ICONS);

  tmp_toolbar_icon = gtk_image_new_from_stock("gtk-new",GTK_ICON_SIZE_LARGE_TOOLBAR);
  menueditor_app.main_toolbar.new = gtk_toolbar_append_element (GTK_TOOLBAR (menueditor_app.main_toolbar.toolbar),
							 GTK_TOOLBAR_CHILD_BUTTON,
							 NULL,
							 "",
							 _("Create a new XFCE4 menu file"), NULL,
							 tmp_toolbar_icon, GTK_SIGNAL_FUNC(new_menu_cb), NULL);
  tmp_toolbar_icon = gtk_image_new_from_stock("gtk-open",GTK_ICON_SIZE_LARGE_TOOLBAR);
  menueditor_app.main_toolbar.open = gtk_toolbar_append_element (GTK_TOOLBAR (menueditor_app.main_toolbar.toolbar),
							 GTK_TOOLBAR_CHILD_BUTTON,
							 NULL,
							 "",
							 _("Open XFCE4 menu file"), NULL,
							 tmp_toolbar_icon, GTK_SIGNAL_FUNC(menu_open_cb), NULL);
  tmp_toolbar_icon = gtk_image_new_from_stock("gtk-save",GTK_ICON_SIZE_LARGE_TOOLBAR);
  menueditor_app.main_toolbar.save = gtk_toolbar_append_element (GTK_TOOLBAR (menueditor_app.main_toolbar.toolbar),
							 GTK_TOOLBAR_CHILD_BUTTON,
							 NULL,
							 "",
							 _("Save current menu"), NULL,
							 tmp_toolbar_icon, GTK_SIGNAL_FUNC(menu_save_cb), NULL);
  tmp_toolbar_icon = gtk_image_new_from_stock("gtk-close",GTK_ICON_SIZE_LARGE_TOOLBAR);
  menueditor_app.main_toolbar.close = gtk_toolbar_append_element (GTK_TOOLBAR (menueditor_app.main_toolbar.toolbar),
							 GTK_TOOLBAR_CHILD_BUTTON,
							 NULL,
							 "",
							 _("Close current menu"), NULL,
							 tmp_toolbar_icon, GTK_SIGNAL_FUNC(close_menu_cb), NULL);
  gtk_toolbar_append_space (GTK_TOOLBAR (menueditor_app.main_toolbar.toolbar));
  tmp_toolbar_icon = gtk_image_new_from_stock("gtk-add",GTK_ICON_SIZE_LARGE_TOOLBAR);
  menueditor_app.main_toolbar.add = gtk_toolbar_append_element (GTK_TOOLBAR (menueditor_app.main_toolbar.toolbar),
							 GTK_TOOLBAR_CHILD_BUTTON,
							 NULL,
							 "",
							 _("Add an entry to the menu"), NULL,
							 tmp_toolbar_icon, GTK_SIGNAL_FUNC(add_entry_cb), NULL);
  tmp_toolbar_icon = gtk_image_new_from_stock("gtk-remove",GTK_ICON_SIZE_LARGE_TOOLBAR);
  menueditor_app.main_toolbar.del = gtk_toolbar_append_element (GTK_TOOLBAR (menueditor_app.main_toolbar.toolbar),
							 GTK_TOOLBAR_CHILD_BUTTON,
							 NULL,
							 "",
							 _("Delete the current entry"), NULL,
							 tmp_toolbar_icon, GTK_SIGNAL_FUNC(delete_entry_cb), NULL);
  tmp_toolbar_icon = gtk_image_new_from_stock("gtk-go-up",GTK_ICON_SIZE_LARGE_TOOLBAR);
  menueditor_app.main_toolbar.up = gtk_toolbar_append_element (GTK_TOOLBAR (menueditor_app.main_toolbar.toolbar),
							 GTK_TOOLBAR_CHILD_BUTTON,
							 NULL,
							 "",
							 _("Move up the current entry"), NULL,
							 tmp_toolbar_icon, GTK_SIGNAL_FUNC(entry_up_cb), NULL);
  tmp_toolbar_icon = gtk_image_new_from_stock("gtk-go-down",GTK_ICON_SIZE_LARGE_TOOLBAR);
  menueditor_app.main_toolbar.down = gtk_toolbar_append_element (GTK_TOOLBAR (menueditor_app.main_toolbar.toolbar),
							 GTK_TOOLBAR_CHILD_BUTTON,
							 NULL,
							 "",
							 _("Move down the current entry"), NULL,
							 tmp_toolbar_icon, GTK_SIGNAL_FUNC(entry_down_cb), NULL);

  /* Tree View inspirated from Gaim */
  menueditor_app.treestore = gtk_tree_store_new (5, GDK_TYPE_PIXBUF, G_TYPE_STRING,
						 G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_POINTER);
  scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (main_vbox), scrolledwindow, TRUE, TRUE, 0);

  menueditor_app.treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL(menueditor_app.treestore));
  gtk_container_add (GTK_CONTAINER (scrolledwindow), menueditor_app.treeview);

  /* Columns */
  icon_cell = gtk_cell_renderer_pixbuf_new();
  name_cell = gtk_cell_renderer_text_new ();
  command_cell = gtk_cell_renderer_text_new();
  hidden_cell = gtk_cell_renderer_toggle_new();
  pointer_cell = gtk_cell_renderer_text_new();

  name_column = gtk_tree_view_column_new ();

  icon_cell = gtk_cell_renderer_pixbuf_new();
  gtk_tree_view_column_pack_start (name_column, icon_cell, FALSE);
  gtk_tree_view_column_set_attributes (name_column, icon_cell,
				       "pixbuf", ICON_COLUMN,
				       NULL);
  g_object_set(icon_cell, "xalign", 0.0, "ypad", 0, NULL);

  gtk_tree_view_column_pack_start (name_column, name_cell, TRUE);
  gtk_tree_view_column_set_attributes (name_column, name_cell,
				       "markup", NAME_COLUMN,
				       NULL);
  g_object_set(name_cell, "ypad", 0, "yalign", 0.5, NULL);
  gtk_tree_view_column_set_title  (name_column,_("Name"));

  command_column = gtk_tree_view_column_new_with_attributes (_("Command"), command_cell, "markup",
							     COMMAND_COLUMN, NULL);
  hidden_column = gtk_tree_view_column_new_with_attributes (_("Hidden"), hidden_cell, "active",
							     HIDDEN_COLUMN, NULL);
  pointer_column = gtk_tree_view_column_new_with_attributes (_("Pointer"), pointer_cell, "text",
							     POINTER_COLUMN, NULL);

  gtk_tree_view_append_column (GTK_TREE_VIEW (menueditor_app.treeview), name_column);
  gtk_tree_view_append_column (GTK_TREE_VIEW (menueditor_app.treeview), command_column);
  gtk_tree_view_append_column (GTK_TREE_VIEW (menueditor_app.treeview), hidden_column);
  gtk_tree_view_append_column (GTK_TREE_VIEW (menueditor_app.treeview), pointer_column);

  gtk_tree_view_column_set_alignment (name_column, 0.5);
  gtk_tree_view_column_set_alignment (command_column, 0.5);
  gtk_tree_view_column_set_alignment (hidden_column, 0.5);

  gtk_tree_view_column_set_max_width (command_column,200);
  gtk_tree_view_column_set_max_width (hidden_column,50);
  
#ifndef DEBUG  
  gtk_tree_view_column_set_visible(GTK_TREE_VIEW_COLUMN(pointer_column), FALSE);
#endif

  /* Status bar */
  statusbar = gtk_statusbar_new ();
  gtk_box_pack_start (GTK_BOX (main_vbox), statusbar, FALSE, FALSE, 0);

  /* Connect signals */
  g_signal_connect ((gpointer) menueditor_app.file_menu.new, "activate",
                    G_CALLBACK (new_menu_cb),
                    NULL);
  g_signal_connect ((gpointer) menueditor_app.file_menu.open, "activate",
                    G_CALLBACK (menu_open_cb),
                    NULL);
  g_signal_connect ((gpointer) menueditor_app.file_menu.open_default, "activate",
                    G_CALLBACK (menu_open_default_cb),
                    NULL);
  g_signal_connect ((gpointer) menueditor_app.file_menu.save, "activate",
                    G_CALLBACK (menu_save_cb),
                    NULL);
  g_signal_connect ((gpointer) menueditor_app.file_menu.saveas, "activate",
                    G_CALLBACK (menu_saveas_cb),
                    NULL);
  g_signal_connect ((gpointer) menueditor_app.file_menu.close, "activate",
                    G_CALLBACK (close_menu_cb),
                    NULL);
  g_signal_connect ((gpointer) menueditor_app.file_menu.exit, "activate",
                    G_CALLBACK (confirm_quit_cb),
                    NULL);

  g_signal_connect ((gpointer) menueditor_app.edit_menu.add_menu, "activate",
                    G_CALLBACK (add_menu_cb),
                    NULL);
  g_signal_connect ((gpointer) menueditor_app.edit_menu.add, "activate",
                    G_CALLBACK (add_entry_cb),
                    NULL);
  g_signal_connect ((gpointer) menueditor_app.edit_menu.del, "activate",
                    G_CALLBACK (delete_entry_cb),
                    NULL);
  g_signal_connect ((gpointer) menueditor_app.edit_menu.up, "activate",
                    G_CALLBACK (entry_up_cb),
                    NULL);
  g_signal_connect ((gpointer) menueditor_app.edit_menu.down, "activate",
                    G_CALLBACK (entry_down_cb),
                    NULL);

  g_signal_connect ((gpointer) menueditor_app.help_menu.about, "activate",
                    G_CALLBACK (about_cb),
                    NULL);

  /* Set up dnd */
  gtk_tree_view_enable_model_drag_source(GTK_TREE_VIEW(menueditor_app.treeview),
					 GDK_BUTTON1_MASK, gte, 2, 
					 GDK_ACTION_MOVE);
  gtk_tree_view_enable_model_drag_dest(GTK_TREE_VIEW(menueditor_app.treeview),
				       gte, 2,
				       GDK_ACTION_COPY);

  g_signal_connect(G_OBJECT(menueditor_app.treeview), "drag-data-received",
		   G_CALLBACK(treeview_drag_data_rcv_cb),
		   NULL);
  g_signal_connect(G_OBJECT(menueditor_app.treeview), "drag-data-get",
		   G_CALLBACK(treeview_drag_data_get_cb),
		   NULL);

  /* Selection in the treeview */
  g_signal_connect (G_OBJECT(menueditor_app.treeview), "row-activated",
                    G_CALLBACK (treeview_activate_cb),
		    NULL);
  g_signal_connect (G_OBJECT(menueditor_app.treeview), "cursor-changed",
                    G_CALLBACK (treeview_cursor_changed_cb),
		    NULL);
  /* Change the hidden property */
  g_signal_connect (G_OBJECT(hidden_cell), "toggled",
                    G_CALLBACK (visible_column_toggled_cb),
		    NULL);

  /* Add accelerators */
  gtk_window_add_accel_group (GTK_WINDOW (menueditor_app.main_window), accel_group);

  /* Deactivate the widgets not usable */
  gtk_widget_set_sensitive(menueditor_app.file_menu.save,FALSE);
  gtk_widget_set_sensitive(menueditor_app.file_menu.saveas,FALSE);
  gtk_widget_set_sensitive(menueditor_app.file_menu.close,FALSE);

  gtk_widget_set_sensitive(menueditor_app.edit_menu.add,FALSE);
  gtk_widget_set_sensitive(menueditor_app.edit_menu.add_menu,FALSE);
  gtk_widget_set_sensitive(menueditor_app.edit_menu.del,FALSE);
  gtk_widget_set_sensitive(menueditor_app.edit_menu.up,FALSE);
  gtk_widget_set_sensitive(menueditor_app.edit_menu.down,FALSE);
  
  gtk_widget_set_sensitive(menueditor_app.main_toolbar.save,FALSE);
  gtk_widget_set_sensitive(menueditor_app.main_toolbar.close,FALSE);
  gtk_widget_set_sensitive(menueditor_app.main_toolbar.add,FALSE);
  gtk_widget_set_sensitive(menueditor_app.main_toolbar.del,FALSE);
  gtk_widget_set_sensitive(menueditor_app.main_toolbar.up,FALSE);
  gtk_widget_set_sensitive(menueditor_app.main_toolbar.down,FALSE);

  gtk_widget_set_sensitive(menueditor_app.treeview,FALSE);

  gtk_widget_set_sensitive(menueditor_app.edit_menu.menu_item,FALSE);
  /* Show all */
  //  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (menueditor_app.treeview), FALSE);
  gtk_tree_view_expand_all (GTK_TREE_VIEW(menueditor_app.treeview));


  gtk_widget_show_all(menueditor_app.main_window);

}

/********************/
/* Open a menu file */
/********************/
void open_menu_file(gchar *menu_file)
{
  GtkTreeIter p;
  xmlNodePtr root;
  xmlNodePtr menu_entry;

  if(menu_file==NULL){
    g_warning ("%s: open_menu_file(): menu_file==NULL", PACKAGE);
    return;
  }


  /* Open the menu file */
  g_stpcpy(menueditor_app.menu_file_name,menu_file);
  menueditor_app.xml_menu_file = xmlParseFile(menu_file);

  /* Check if the format is correct */
  if(menueditor_app.xml_menu_file==NULL){
    GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(menueditor_app.main_window),
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_OK,
						_("Corrupted file or incorrect file format !"));
#ifdef DEBUG
    g_printerr(_("Corrupted file or incorrect file format !\n"));;
#endif
    
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    
    xmlFreeDoc(menueditor_app.xml_menu_file);
    menueditor_app.xml_menu_file=NULL;
    return;
  }

  root=xmlDocGetRootElement(menueditor_app.xml_menu_file);

  if(root==NULL){
    GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(menueditor_app.main_window),
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_OK,
						_("No root element in file !"));
#ifdef DEBUG
    g_printerr(_("No root element in file !\n"));;
#endif
    
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    
    xmlFreeDoc(menueditor_app.xml_menu_file);
    menueditor_app.xml_menu_file=NULL;
    return;
  }
  
  if(xmlStrcmp(root->name,(xmlChar*)"xfdesktop-menu")){
    GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(menueditor_app.main_window),
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_OK,
						_("Bad datafile format !"));
#ifdef DEBUG
    g_printerr(_("Bad datafile format !\n"));;
#endif
    
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    
    xmlFreeDoc(menueditor_app.xml_menu_file);
    menueditor_app.xml_menu_file=NULL;
    return;
  }

  menu_entry = root->xmlChildrenNode;
  load_menu_in_tree(menu_entry,NULL);

  /* Terminate operations */
  gtk_tree_view_expand_all (GTK_TREE_VIEW(menueditor_app.treeview));

  gtk_widget_set_sensitive(menueditor_app.main_toolbar.close,TRUE);

  gtk_widget_set_sensitive(menueditor_app.edit_menu.menu_item,TRUE);

  gtk_widget_set_sensitive(menueditor_app.file_menu.saveas,TRUE);
  gtk_widget_set_sensitive(menueditor_app.file_menu.close,TRUE);
  gtk_widget_set_sensitive(menueditor_app.treeview,TRUE);

  gtk_widget_set_sensitive(menueditor_app.main_toolbar.add,FALSE);
  gtk_widget_set_sensitive(menueditor_app.main_toolbar.del,FALSE);
  gtk_widget_set_sensitive(menueditor_app.main_toolbar.up,FALSE);
  gtk_widget_set_sensitive(menueditor_app.main_toolbar.down,FALSE);

  gtk_widget_set_sensitive(menueditor_app.edit_menu.add,FALSE);
  gtk_widget_set_sensitive(menueditor_app.edit_menu.add_menu,FALSE);
  gtk_widget_set_sensitive(menueditor_app.edit_menu.del,FALSE);
  gtk_widget_set_sensitive(menueditor_app.edit_menu.up,FALSE);
  gtk_widget_set_sensitive(menueditor_app.edit_menu.down,FALSE);

  menueditor_app.menu_modified=FALSE;
}

/***********************************************************/
/* Get the menu file in use (inspired from xfdesktop code) */
/***********************************************************/
static gchar* get_default_menu_file ()
{
  gchar *filename = NULL;
  gchar *path = NULL;
  const char *env;

  env = g_getenv ("XFCE_DISABLE_USER_CONFIG");
  
  if (!env || strcmp (env, "0")){
    filename = xfce_get_userfile ("menu.xml", NULL);
    if (g_file_test (filename, G_FILE_TEST_EXISTS))
      return filename;
  }

  path = g_build_filename (SYSCONFDIR, "xfce4", "menu.xml", NULL);
  filename = (gchar*)xfce_get_file_localized (path);
  g_free (path);
  
  if(filename)
    return filename;

  g_warning ("%s: Could not locate a menu definition file", PACKAGE);
  
  return NULL;

}

/***************/
/* Entry point */
/***************/
int main (int argc, char *argv[]) 
{

  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  menueditor_app.xml_menu_file=NULL;

  gtk_init(&argc, &argv);
  
  create_main_window();

  /* track icon theme changes (from the panel) */
  if(!client) {
    Display *dpy = GDK_DISPLAY();
    int screen = XDefaultScreen(dpy);
    
    if(!mcs_client_check_manager(dpy, screen, "xfce-mcs-manager"))
      g_warning("%s: mcs manager not running\n", PACKAGE);
    client = mcs_client_new(dpy, screen, mcs_notify_cb, mcs_watch_cb, NULL);
    if(client)
      mcs_client_add_channel(client, CHANNEL);
  }

  /* Initialize icon theme */
  menueditor_app.icon_theme = gtk_icon_theme_get_default();
  gtk_icon_theme_prepend_search_path(menueditor_app.icon_theme, "/usr/share/xfce4/themes");

  if(argc>1){
    if (g_file_test (argv[1], G_FILE_TEST_EXISTS))
      open_menu_file(argv[1]);
    else{
      GtkWidget *dialog;
      gchar *error_message;

      error_message= g_strdup_printf(_("File %s doesn't exist !"),argv[1]);
      
      dialog = gtk_message_dialog_new (GTK_WINDOW(menueditor_app.main_window),
				       GTK_DIALOG_DESTROY_WITH_PARENT,
				       GTK_MESSAGE_ERROR,
				       GTK_BUTTONS_OK,
				       error_message);
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      g_free(error_message);
    }
  }else
    menu_open_default_cb(NULL, NULL);

  gtk_main();

  return 0;
}
