/*   menueditor.h */

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

#ifndef __HAVE_MENUEDITOR_H
#define __HAVE_MENUEDITOR_H

#include "config.h"

#include <gtk/gtk.h>

/* includes for the libxml */
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

/* includes for xfce4 */
#include <libxfce4mcs/mcs-client.h>
#include <libxfcegui4/icons.h>
#include <libxfcegui4/libxfcegui4.h>
#include <libxfcegui4/xgtkicontheme.h>
#include <libxfce4util/libxfce4util.h>

/* definitions of fonts in the tree */
#define TITLE_FORMAT "<span weight='bold' color='dim grey' style='italic'>%s</span>"
#define MENU_FORMAT "<span weight='bold'>%s</span>"
#define SEPARATOR_FORMAT "<span color='grey'>%s</span>"
#define BUILTIN_FORMAT "%s"
#define NAME_FORMAT "%s"
#define COMMAND_FORMAT "<span style='italic'>%s</span>"
#define INCLUDE_FORMAT "<span style='italic' color='dark green'>%s</span>"
#define INCLUDE_PATH_FORMAT "<span style='italic' color='dark green'>%s</span>"

/* definitions for mcsclient */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#define CHANNEL "xfce"

#define ICON_SIZE 24

/***********/
/* Globals */
/***********/
/*static guchar icon_size = 24; uuugh... this is a h file */

void browse_command_cb(GtkWidget *widget, GtkEntry *entry_command);
void browse_icon_cb(GtkWidget *widget, GtkEntry *entry_icon);

enum {ICON_COLUMN, NAME_COLUMN, COMMAND_COLUMN, HIDDEN_COLUMN, POINTER_COLUMN};

struct _controls_menu{
  enum {MENUFILE, SYSTEM} menu_type;
  GtkWidget *hbox_source;
  GtkWidget *label_source;
};

struct _menueditor_app{
  gboolean menu_modified;
  gchar menu_file_name[255];
  xmlDocPtr xml_menu_file;
  GtkWidget *main_window;
  GtkIconTheme *icon_theme;
  /* Tree */
  GtkWidget *treeview;
  GtkTreeStore *treestore;
  /* Menus */
  GtkWidget* main_menubar;
     struct _file_menu{
       GtkWidget* menu;
       GtkWidget* menu_item;
       GtkWidget* new;
       GtkWidget* open;
       GtkWidget* open_default;
       GtkWidget* save;
       GtkWidget* saveas;
       GtkWidget* close;
       GtkWidget* exit;
     } file_menu;

     struct _edit_menu{
       GtkWidget* menu;
       GtkWidget* menu_item;
       GtkWidget* add;
       GtkWidget* add_menu;
       GtkWidget* del;
       GtkWidget* up;
       GtkWidget* down;
     } edit_menu;
  
     struct _help_menu{
       GtkWidget* menu;
       GtkWidget* menu_item;
       GtkWidget* about;
     } help_menu;
  /* Toolbar */
     struct _main_toolbar{
       GtkWidget* toolbar;
       GtkWidget* new;
       GtkWidget* open;
       GtkWidget* save;
       GtkWidget* close;
       GtkWidget* add;
       GtkWidget* del;
       GtkWidget* up;
       GtkWidget* down;
     } main_toolbar;
  
} menueditor_app;


#endif
