/*   dnd.c */

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

#include <libxfce4util/xfce-desktopentry.h>

#include "menueditor.h"

#include "dnd.h"
#include "../modules/menu/dummy_icon.h"

extern void load_menu_in_tree(xmlNodePtr menu, GtkTreeIter *p);

inline void show_str(char *str)
{
  int i=0;

  while(i<strlen(str))
    printf("%d;",str[i++]);

  printf("\n");
}

/*****************/
/* Drag and drop */
/*****************/

void treeview_drag_data_get_cb(GtkWidget *widget, GdkDragContext *dc, 
			       GtkSelectionData *data, guint info,
			       guint time, gpointer *null)
{
  if (data->target == gdk_atom_intern("XFCE_MENU_ENTRY", FALSE)) {
    GtkTreeRowReference *ref = g_object_get_data(G_OBJECT(dc), "gtk-tree-view-source-row");
    GtkTreePath *sourcerow = gtk_tree_row_reference_get_path(ref);
    GtkTreeIter *iter = (GtkTreeIter*) g_malloc(sizeof(GtkTreeIter));

    if(!sourcerow)
      return;

    gtk_tree_model_get_iter(GTK_TREE_MODEL(menueditor_app.treestore), iter, sourcerow);

    gtk_selection_data_set (data,
			    gdk_atom_intern ("XFCE_MENU_ENTRY", FALSE),
			    8, /* bits */
			    (void*)&iter,
			    sizeof (iter));

    gtk_tree_path_free(sourcerow);
  }
}

void treeview_drag_data_rcv_cb(GtkWidget *widget, GdkDragContext *dc,
			       guint x, guint y, GtkSelectionData *sd,
			       guint info, guint t)
{
  if (sd->target == gdk_atom_intern("XFCE_MENU_ENTRY", FALSE) && sd->data) {
    GtkTreeIter *iter_to_insert = NULL;
    GtkTreePath *path = NULL;
    GtkTreeViewDropPosition position;
    memcpy(&iter_to_insert, sd->data, sizeof(iter_to_insert));

    if(gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(widget), x, y, &path, &position)) {
      /* if we're here, I think it means the drop is ok */	
      GtkTreeIter iter, iter_new;
      GtkTreePath *path_to_insert = NULL;
      gchar *str_name = NULL, *str_command = NULL;
      gboolean hidden = FALSE;
      GValue val1 = {0};
      GValue val2 = {0};
      GValue val3 = {0};
      xmlNodePtr node_sibling;
      xmlNodePtr node_to_insert;
      GdkPixbuf *icon = NULL;
      xmlChar *prop_visible = NULL;
      xmlChar *prop_name = NULL;
      xmlChar *prop_cmd = NULL;
      xmlChar *prop_src = NULL;

      path_to_insert = gtk_tree_model_get_path(GTK_TREE_MODEL(menueditor_app.treestore), iter_to_insert);

      gtk_tree_model_get_iter(GTK_TREE_MODEL(menueditor_app.treestore),
                              &iter, path);

      gtk_tree_model_get_value (GTK_TREE_MODEL(menueditor_app.treestore),
                                &iter, POINTER_COLUMN, &val1);
      node_sibling = g_value_get_pointer(&val1);
      if(!node_sibling){
	gtk_drag_finish(dc, FALSE, (dc->action == GDK_ACTION_MOVE), t);
	gtk_tree_path_free(path);
	g_free(iter_to_insert);
	return;
      }

      gtk_tree_model_get_value (GTK_TREE_MODEL(menueditor_app.treestore),
                                iter_to_insert, POINTER_COLUMN, &val2);
      node_to_insert = g_value_get_pointer(&val2);

      gtk_tree_model_get_value (GTK_TREE_MODEL(menueditor_app.treestore),
                                iter_to_insert, ICON_COLUMN, &val3);
      icon = g_value_get_object(&val3);

      prop_name = xmlGetProp(node_to_insert, "name");
      prop_cmd = xmlGetProp(node_to_insert, "cmd");
      prop_visible = xmlGetProp(node_to_insert, "visible");
      prop_src = xmlGetProp(node_to_insert, "src");

      if(!xmlStrcmp(prop_visible, "no"))
        hidden=TRUE;

      if(!xmlStrcmp(node_to_insert->name,"title")){
	str_name = g_strdup_printf(TITLE_FORMAT, prop_name);
      }else if(!xmlStrcmp(node_to_insert->name,"app")){
        str_name = g_strdup_printf(NAME_FORMAT, prop_name);
        str_command = g_strdup_printf(COMMAND_FORMAT, prop_cmd);
      }else if(!xmlStrcmp(node_to_insert->name,"menu")){
	if(gtk_tree_path_is_descendant(path, path_to_insert)){
	  gtk_tree_path_free(path);
	  g_free(iter_to_insert);
	  gtk_drag_finish(dc, FALSE, (dc->action == GDK_ACTION_MOVE), t);
	  gtk_tree_path_free(path_to_insert);
	  return;
	}

	str_name = g_strdup_printf(MENU_FORMAT, prop_name);
      }else if(!xmlStrcmp(node_to_insert->name,"separator")){
        str_name = g_strdup_printf(SEPARATOR_FORMAT, _("--- separator ---"));
      }else if(!xmlStrcmp(node_to_insert->name,"include")){
        str_name = g_strdup_printf(INCLUDE_FORMAT, _("--- include ---"));

	if(xmlHasProp(node_to_insert, "src"))
	  str_command = g_strdup_printf(INCLUDE_PATH_FORMAT, prop_src);
	else
	  str_command = g_strdup_printf(INCLUDE_PATH_FORMAT, _("system"));
      }else if(!xmlStrcmp(node_to_insert->name,"builtin")){
        str_name = g_strdup_printf(NAME_FORMAT, prop_name);
        str_command = g_strdup_printf(COMMAND_FORMAT, prop_cmd);
      }

      /* move in the gtk and xml tree */
      switch(gtk_tree_path_compare(path, path_to_insert)){
      case -1:
	gtk_tree_store_insert_before(menueditor_app.treestore, &iter_new, NULL,
				     &iter);
	xmlAddPrevSibling(node_sibling, node_to_insert);
	break;
      case 0:
	gtk_tree_path_free(path);
	gtk_tree_path_free(path_to_insert);
	g_free(iter_to_insert);
	gtk_drag_finish(dc, FALSE, (dc->action == GDK_ACTION_MOVE), t);
	return;
      case 1:
	gtk_tree_store_insert_after(menueditor_app.treestore, &iter_new, NULL,
				    &iter);
	xmlAddNextSibling(node_sibling, node_to_insert);
      }


      gtk_tree_store_set (menueditor_app.treestore, &iter_new,
			  ICON_COLUMN, icon,
			  NAME_COLUMN, str_name,
			  COMMAND_COLUMN, str_command,
			  HIDDEN_COLUMN, hidden,
			  POINTER_COLUMN, node_to_insert, -1);

      if(!xmlStrcmp(node_to_insert->name,"menu")){
	load_menu_in_tree(node_to_insert->xmlChildrenNode, &iter_new);
	gtk_tree_view_expand_all (GTK_TREE_VIEW(menueditor_app.treeview));      
      }

      /* Modified ! */
      menueditor_app.menu_modified=TRUE;
      gtk_widget_set_sensitive(menueditor_app.file_menu.save,TRUE);
      gtk_widget_set_sensitive(menueditor_app.main_toolbar.save,TRUE);

      xmlFree(prop_name);
      xmlFree(prop_cmd);
      xmlFree(prop_visible);
      xmlFree(prop_src);
      g_free(str_name);
      g_free(str_command);
      gtk_tree_path_free(path);
      gtk_tree_path_free(path_to_insert);
      gtk_drag_finish(dc, TRUE, (dc->action == GDK_ACTION_MOVE), t);
    }

    g_free(iter_to_insert);
  }else if (sd->target == gdk_atom_intern("text/plain", FALSE) && sd->data) {
    gchar *filename=NULL;
    GtkTreePath *path = NULL;
    GtkTreeViewDropPosition position;    

    if(g_str_has_prefix(sd->data,"file://")){
      gchar *source=&((sd->data)[7]);

      filename = g_strndup(source, strlen(sd->data)-8);
      filename[strlen(filename)-1]='\0';
    }else if(g_str_has_prefix(sd->data,"file:")){
      gchar *source=&((sd->data)[5]);

      filename = g_strndup(source, strlen(sd->data)-6);
      filename[strlen(filename)-1]='\0';
      }

    if(gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(widget), x, y, &path, &position)) {
      XfceDesktopEntry *de=NULL;
      const char *cat[]={"Name","Exec","Icon"};

      xmlNodePtr node, node_target;
      GtkTreeIter iter, iter_target;
      GValue val = {0};
      gchar *value_name = NULL;
      gchar *value_command = NULL;
      gchar *value_icon = NULL;
      gchar* name = NULL;
      gchar *command = NULL;
      gboolean icon_found = FALSE;
      GdkPixbuf *icon = NULL;

      gtk_tree_model_get_iter(GTK_TREE_MODEL(menueditor_app.treestore),
                              &iter_target, path);

      gtk_tree_model_get_value (GTK_TREE_MODEL(menueditor_app.treestore),
                                &iter_target, POINTER_COLUMN, &val);
      node_target = g_value_get_pointer(&val);

      de = xfce_desktop_entry_new (filename, cat, 3);
      g_return_if_fail (xfce_desktop_entry_parse(de));
      g_return_if_fail (xfce_desktop_entry_get_string (de,
						       "Name",
						       TRUE,
						       &value_name));
      g_return_if_fail (xfce_desktop_entry_get_string (de,
						       "Exec",
						       TRUE,
						       &value_command));
      icon_found = xfce_desktop_entry_get_string (de,
						  "Icon",
						  TRUE,
						  &value_icon);

      /* Create node */
      node = xmlNewNode(NULL, "app");
     
      xmlSetProp(node, "name", value_name);
      xmlSetProp(node, "cmd", value_command);
      if(icon_found){
	xmlSetProp(node, "icon", value_icon);
	icon = xfce_load_themed_icon(value_icon, ICON_SIZE);
      }else
	icon = xfce_inline_icon_at_size(dummy_icon_data, ICON_SIZE, ICON_SIZE);

      if(xmlAddNextSibling(node_target, node) == NULL){
	perror("xmlAddNextSibling");
	xmlFreeNode(node);
	gtk_drag_finish(dc, TRUE, (dc->action == GDK_ACTION_COPY), t);
	return;
      }

      gtk_tree_store_insert_after (menueditor_app.treestore,
				   &iter, NULL, &iter_target);
          
      name = g_strdup_printf(NAME_FORMAT, value_name);
      command = g_strdup_printf(COMMAND_FORMAT, value_command);

      gtk_tree_store_set (menueditor_app.treestore, &iter, 
			  ICON_COLUMN, icon, 
			  NAME_COLUMN, name, 
			  COMMAND_COLUMN, command,
			  POINTER_COLUMN, node, -1);

      g_free(value_name);
      g_free(value_command);
      g_free(value_icon);
      g_free(name);
      g_free(command);
      g_object_unref (G_OBJECT (de));

      /* Modified ! */
      menueditor_app.menu_modified=TRUE;
      gtk_widget_set_sensitive(menueditor_app.file_menu.save,TRUE);
      gtk_widget_set_sensitive(menueditor_app.main_toolbar.save,TRUE);
    }

    g_free(filename);

    gtk_drag_finish(dc, TRUE, (dc->action == GDK_ACTION_COPY), t);
  }else if (sd->target == gdk_atom_intern("application/x-desktop", FALSE) && sd->data) {
    printf("Not yet supported !!!\n");
    printf("%s\n",sd->data);
    gtk_drag_finish(dc, TRUE, (dc->action == GDK_ACTION_COPY), t);
  }

}
