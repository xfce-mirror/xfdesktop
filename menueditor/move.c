/*   move.c */

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

#include "move.h"

/******************************************/
/* Workaround for gtk_tree_store_swap bug */
/******************************************/
void my_tree_store_swap_down(GtkTreeStore *tree_store,
			     GtkTreeIter *a,
			     GtkTreeIter *b)
{
  GValue val_name = {0};
  GValue val_command = {0};
  GValue val_hidden = {0};
  GValue val_icon = {0};
  GValue val_pointer = {0};
  gchar *str_name;
  gchar *str_command;
  GdkPixbuf *icon;
  gboolean hidden;
  xmlNodePtr node;
  GtkTreeIter iter_new;
  GtkTreeModel *model=GTK_TREE_MODEL(menueditor_app.treestore);

  /* Create a new iter after b */
  gtk_tree_store_insert_after(menueditor_app.treestore, &iter_new,
			      NULL, b);

  /* Get the values of a */
  gtk_tree_model_get_value (GTK_TREE_MODEL(menueditor_app.treestore), a, NAME_COLUMN, &val_name);
  str_name = (gchar*) g_value_get_string(&val_name);
  gtk_tree_model_get_value (GTK_TREE_MODEL(menueditor_app.treestore), a, COMMAND_COLUMN, &val_command);
  str_command = (gchar*) g_value_get_string(&val_command);
  gtk_tree_model_get_value (GTK_TREE_MODEL(menueditor_app.treestore), a, HIDDEN_COLUMN, &val_hidden);
  hidden = g_value_get_boolean(&val_hidden);
  gtk_tree_model_get_value (GTK_TREE_MODEL(menueditor_app.treestore), a, ICON_COLUMN, &val_icon);
  icon = g_value_get_object(&val_icon);
  gtk_tree_model_get_value (GTK_TREE_MODEL(menueditor_app.treestore), a, POINTER_COLUMN, &val_pointer);
  node = g_value_get_pointer(&val_pointer);

  /* Copy values of a in iter_new */
  gtk_tree_store_set (menueditor_app.treestore, &iter_new, 
		      ICON_COLUMN, icon,
		      NAME_COLUMN, str_name, 
		      COMMAND_COLUMN, str_command,
		      HIDDEN_COLUMN, hidden,
		      POINTER_COLUMN, node, -1);

  /* Remove a */
  gtk_tree_store_remove (menueditor_app.treestore,a);
  *a = iter_new;
  gtk_tree_view_set_cursor(GTK_TREE_VIEW(menueditor_app.treeview),
			   gtk_tree_model_get_path(model, a),
			   NULL,FALSE);
  /* Free mem */
  g_free(str_name);
  g_free(str_command);
}

void my_tree_store_swap_up(GtkTreeStore *tree_store,
			   GtkTreeIter *a,
			   GtkTreeIter *b)
{
  GValue val_icon = {0};
  GValue val_name = {0};
  GValue val_command = {0};
  GValue val_hidden = {0};
  GValue val_pointer = {0};
  gchar *str_name;
  gchar *str_command;
  GdkPixbuf *icon;
  gboolean hidden;
  xmlNodePtr node;
  GtkTreeIter iter_new;
  GtkTreeModel *model=GTK_TREE_MODEL(menueditor_app.treestore);

  /* Create a new iter after b */
  gtk_tree_store_insert_before(menueditor_app.treestore, &iter_new,
			       NULL, b);

  /* Get the values of a */
  gtk_tree_model_get_value (GTK_TREE_MODEL(menueditor_app.treestore), a, ICON_COLUMN, &val_icon);
  icon = g_value_get_object(&val_icon);
  gtk_tree_model_get_value (GTK_TREE_MODEL(menueditor_app.treestore), a, NAME_COLUMN, &val_name);
  str_name = (gchar*) g_value_get_string(&val_name);
  gtk_tree_model_get_value (GTK_TREE_MODEL(menueditor_app.treestore), a, COMMAND_COLUMN, &val_command);
  str_command = (gchar*) g_value_get_string(&val_command);
  gtk_tree_model_get_value (GTK_TREE_MODEL(menueditor_app.treestore), a, HIDDEN_COLUMN, &val_hidden);
  hidden = g_value_get_boolean(&val_hidden);
  gtk_tree_model_get_value (GTK_TREE_MODEL(menueditor_app.treestore), a, POINTER_COLUMN, &val_pointer);
  node = g_value_get_pointer(&val_pointer);

  /* Copy values of a in iter_new */
  gtk_tree_store_set (menueditor_app.treestore, &iter_new, 
		      ICON_COLUMN, icon, 
		      NAME_COLUMN, str_name, 
		      COMMAND_COLUMN, str_command,
		      HIDDEN_COLUMN, hidden,
		      POINTER_COLUMN, node, -1);

  /* Remove a */
  gtk_tree_store_remove (menueditor_app.treestore,a);
  *a = iter_new;
  gtk_tree_view_set_cursor(GTK_TREE_VIEW(menueditor_app.treeview),
			   gtk_tree_model_get_path(model, a),
			   NULL,FALSE);
  /* Free mem */
  g_free(str_name);
  g_free(str_command);
}


/****************/
/* Moving entry */
/****************/
void entry_up_cb(GtkWidget *widget, gpointer data)
{
  GtkTreeSelection *selection=gtk_tree_view_get_selection(GTK_TREE_VIEW(menueditor_app.treeview));
  GtkTreeModel *model=GTK_TREE_MODEL(menueditor_app.treestore);
  GtkTreeIter iter, iter_prev, iter_up;
  GtkTreePath *path = gtk_tree_path_new();
  GtkTreePath *path_prev = gtk_tree_path_new();
  GtkTreePath *path_up = gtk_tree_path_new();
  gboolean ret_path=FALSE, ret_iter=FALSE;

  /* Retrieve current iter */
  gtk_tree_selection_get_selected(selection,&model,&iter);
  path_prev = gtk_tree_model_get_path(model, &iter);
  path_up = gtk_tree_model_get_path(model, &iter);

  /* Retrieve previous iter */
  ret_path = gtk_tree_path_prev (path_prev);
  ret_iter = gtk_tree_model_get_iter (model,&iter_prev,path_prev);

  if(ret_path){

    if(ret_iter){
      GValue val1 = { 0, };
      GValue val2 = { 0, };
      xmlNodePtr node, node_prev;
      
      /* Modified ! */
      menueditor_app.menu_modified=TRUE;
      gtk_widget_set_sensitive(menueditor_app.file_menu.save,TRUE);
      gtk_widget_set_sensitive(menueditor_app.main_toolbar.save,TRUE);

      /* Swap entries */
      my_tree_store_swap_up(menueditor_app.treestore, &iter, &iter_prev);
      
      /* Swap in the xml tree */
      gtk_tree_model_get_value (GTK_TREE_MODEL(menueditor_app.treestore), &iter, POINTER_COLUMN, &val1);
      node = g_value_get_pointer(&val1);
      gtk_tree_model_get_value (GTK_TREE_MODEL(menueditor_app.treestore), &iter_prev, POINTER_COLUMN, &val2);
      node_prev = g_value_get_pointer(&val2);
      xmlAddPrevSibling(node_prev, node);

    }
  }else{
    gtk_tree_path_up(path_up);
    ret_iter = gtk_tree_model_get_iter (model,&iter_up,path_up);

    if(gtk_tree_path_get_depth(path_up) > 0 && ret_iter){
      /* Move into the parent menu ? */
      GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(menueditor_app.main_window),
				      GTK_DIALOG_DESTROY_WITH_PARENT,
				      GTK_MESSAGE_QUESTION,
				      GTK_BUTTONS_YES_NO,
				      _("Do you want to move the item into the parent menu?"));
      if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_YES){
	/* Move into the parent menu */
	GtkTreeIter iter_new;
	GValue val1 = { 0, };
	GValue val2 = { 0, };
	GValue val3 = { 0, };
	GValue val4 = { 0, };
	GValue val5 = { 0, };
	GdkPixbuf *icon;
	gchar *str_name;
	gchar *str_command;
	xmlNodePtr node, node_parent;

	/* Modified ! */
	menueditor_app.menu_modified=TRUE;
	gtk_widget_set_sensitive(menueditor_app.file_menu.save,TRUE);
	gtk_widget_set_sensitive(menueditor_app.main_toolbar.save,TRUE);

	
	/* gtk should implement a method to move a element across the tree ! */
	
	/* Create the new iter */
	gtk_tree_store_insert_before(menueditor_app.treestore,
				     &iter_new,
				     NULL,
				     &iter_up);

	/* Copy the values of the current iter */
	gtk_tree_model_get_value (GTK_TREE_MODEL(menueditor_app.treestore), &iter, ICON_COLUMN, &val1);
	icon = g_value_get_object(&val1);
	gtk_tree_model_get_value (GTK_TREE_MODEL(menueditor_app.treestore), &iter, NAME_COLUMN, &val2);
	str_name = (gchar*) g_value_get_string(&val2);
	gtk_tree_model_get_value (GTK_TREE_MODEL(menueditor_app.treestore), &iter, COMMAND_COLUMN, &val3);
	str_command = (gchar*) g_value_get_string(&val3);
	gtk_tree_model_get_value (GTK_TREE_MODEL(menueditor_app.treestore), &iter, POINTER_COLUMN, &val4);
	node = g_value_get_pointer(&val4);
	
	gtk_tree_store_set (menueditor_app.treestore, &iter_new,
			    ICON_COLUMN, icon, 
			    NAME_COLUMN, str_name,
			    COMMAND_COLUMN, str_command,
			    POINTER_COLUMN, node , -1);

	g_free(str_name);
	g_free(str_command);

	/* Remove the current iter */
	gtk_tree_store_remove (menueditor_app.treestore,&iter);

	/* Set selection on the new iter */
	gtk_tree_view_set_cursor(GTK_TREE_VIEW(menueditor_app.treeview),
				 gtk_tree_model_get_path(model, &iter_new),
				 NULL,FALSE);
	/* Move the element in the xml tree */
	gtk_tree_model_get_value (GTK_TREE_MODEL(menueditor_app.treestore), &iter_up, POINTER_COLUMN, &val5);
	node_parent = g_value_get_pointer(&val5);
	xmlAddPrevSibling(node_parent, node);
	
      }
      gtk_widget_destroy(dialog);
    }
  }

  gtk_tree_path_free(path);
  gtk_tree_path_free(path_prev);
  gtk_tree_path_free(path_up); 
}

void entry_down_cb(GtkWidget *widget, gpointer data)
{
  GtkTreeSelection *selection=gtk_tree_view_get_selection(GTK_TREE_VIEW(menueditor_app.treeview));
  GtkTreeModel *model=GTK_TREE_MODEL(menueditor_app.treestore);
  GtkTreeIter iter, iter_next;
  GtkTreePath *path = gtk_tree_path_new();
  GtkTreePath *path_next = gtk_tree_path_new();
  gboolean ret_iter=FALSE;

  /* Retrieve current iter */
  gtk_tree_selection_get_selected (selection,&model,&iter);
  path_next = gtk_tree_model_get_path(model, &iter);

  /* Retrieve next iter */
  gtk_tree_path_next (path_next);
  ret_iter = gtk_tree_model_get_iter (model,&iter_next,path_next);

  if(ret_iter){
    xmlNodePtr node_temp;
    GValue val_type = { 0, };

    gtk_tree_model_get_value (GTK_TREE_MODEL(menueditor_app.treestore), &iter_next, POINTER_COLUMN, &val_type);
    node_temp = g_value_get_pointer(&val_type);

    /* Insert in the submenu ? */
    if(!xmlStrcmp(node_temp->name,(xmlChar*)"menu")){
      GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(menueditor_app.main_window),
				      GTK_DIALOG_DESTROY_WITH_PARENT,
				      GTK_MESSAGE_QUESTION,
				      GTK_BUTTONS_YES_NO,
				      _("Do you want to move the item into the submenu?"));
      if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_YES){
	/* Insert in the submenu ! */
	GtkTreeIter iter_new, iter_child;
	GValue val1 = { 0, };
	GValue val2 = { 0, };
	GValue val3 = { 0, };
	GValue val4 = { 0, };
	GValue val5 = { 0, };
	gchar *str_name;
	gchar *str_command;
	xmlNodePtr node, node_next;
	GdkPixbuf *icon;
	gint children = 0;

	/* Modified ! */
	menueditor_app.menu_modified=TRUE;
	gtk_widget_set_sensitive(menueditor_app.file_menu.save,TRUE);
	gtk_widget_set_sensitive(menueditor_app.main_toolbar.save,TRUE);
    
	/* gtk should implement a method to move a element across the tree ! */
	
	/* Create the new iter */
	gtk_tree_store_prepend (menueditor_app.treestore, &iter_new, &iter_next);

	/* Copy the values of the current iter */
	gtk_tree_model_get_value (GTK_TREE_MODEL(menueditor_app.treestore), &iter, ICON_COLUMN, &val1);
	icon = g_value_get_object(&val1);
	gtk_tree_model_get_value (GTK_TREE_MODEL(menueditor_app.treestore), &iter, NAME_COLUMN, &val2);
	str_name = (gchar*) g_value_get_string(&val2);
	gtk_tree_model_get_value (GTK_TREE_MODEL(menueditor_app.treestore), &iter, COMMAND_COLUMN, &val3);
	str_command = (gchar*) g_value_get_string(&val3);
	gtk_tree_model_get_value (GTK_TREE_MODEL(menueditor_app.treestore), &iter, POINTER_COLUMN, &val4);
	node = g_value_get_pointer(&val4);
	
	
	gtk_tree_store_set (menueditor_app.treestore, &iter_new, 
			    ICON_COLUMN, icon, 
			    NAME_COLUMN, str_name, 
			    COMMAND_COLUMN, str_command, 
			    POINTER_COLUMN, node , -1);

	g_free(str_name);
	g_free(str_command);

	/* Remove the current iter */
	gtk_tree_store_remove (menueditor_app.treestore,&iter);

	/* Expand the whole tree */
	gtk_tree_view_expand_all (GTK_TREE_VIEW(menueditor_app.treeview));

	/* Set selection on the new iter */
	gtk_tree_view_set_cursor(GTK_TREE_VIEW(menueditor_app.treeview),
				 gtk_tree_model_get_path(model, &iter_new),
				 NULL,FALSE);

	/* Move the element in the xml tree */
	gtk_tree_model_iter_children(GTK_TREE_MODEL(menueditor_app.treestore),
				     &iter_child,&iter_next);
	children = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(menueditor_app.treestore),
						  &iter_next);
	gtk_tree_model_get_value (GTK_TREE_MODEL(menueditor_app.treestore), &iter_next, POINTER_COLUMN, &val5);
	node_next = g_value_get_pointer(&val5);

	if(children > 1)
	  xmlAddPrevSibling(node_next->xmlChildrenNode, node);
	else{
	  xmlUnlinkNode(node);
	  xmlAddChild(node_next, node);
	}
      }else{
	GValue val1 = { 0, };
	GValue val2 = { 0, };
	xmlNodePtr node, node_next;
	
	/* Swap entries */
	my_tree_store_swap_down(menueditor_app.treestore, &iter, &iter_next);
      
	/* Swap in the xml tree */
	gtk_tree_model_get_value (GTK_TREE_MODEL(menueditor_app.treestore), &iter, POINTER_COLUMN, &val1);
	node = g_value_get_pointer(&val1);
	gtk_tree_model_get_value (GTK_TREE_MODEL(menueditor_app.treestore), &iter_next, POINTER_COLUMN, &val2);
	node_next = g_value_get_pointer(&val2);
	xmlAddNextSibling(node_next, node);

	/* Modified ! */
	menueditor_app.menu_modified=TRUE;
	gtk_widget_set_sensitive(menueditor_app.file_menu.save,TRUE);
	gtk_widget_set_sensitive(menueditor_app.main_toolbar.save,TRUE);
      }
      gtk_widget_destroy(dialog);
    }else{
      GValue val1 = { 0, };
      GValue val2 = { 0, };
      xmlNodePtr node, node_next;

      /* Swap entries */
      my_tree_store_swap_down(menueditor_app.treestore, &iter, &iter_next);
      
      /* Swap in the xml tree */
      gtk_tree_model_get_value (GTK_TREE_MODEL(menueditor_app.treestore), &iter, POINTER_COLUMN, &val1);
      node = g_value_get_pointer(&val1);
      gtk_tree_model_get_value (GTK_TREE_MODEL(menueditor_app.treestore), &iter_next, POINTER_COLUMN, &val2);
      node_next = g_value_get_pointer(&val2);

      xmlAddNextSibling(node_next, node);

      /* Modified ! */
      menueditor_app.menu_modified=TRUE;
      gtk_widget_set_sensitive(menueditor_app.file_menu.save,TRUE);
      gtk_widget_set_sensitive(menueditor_app.main_toolbar.save,TRUE);
    }
  }

  gtk_tree_path_free(path);
  gtk_tree_path_free(path_next);

}
