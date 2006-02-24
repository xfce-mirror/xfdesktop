/*   menueditor.h */

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

#ifndef __HAVE_MENUEDITOR_H
#define __HAVE_MENUEDITOR_H

#include "config.h"

#include <gtk/gtk.h>

/* includes for xfce4 */
#include <libxfcegui4/libxfcegui4.h>
#include <libxfce4util/libxfce4util.h>

#include "../modules/menu/dummy_icon.h"

/* definitions of fonts in the tree */
#define TITLE_FORMAT "<span weight='bold' color='dim grey' style='italic'>%s</span>"
#define MENU_FORMAT "<span weight='bold'>%s</span>"
#define SEPARATOR_FORMAT "<span color='grey'>%s</span>"
#define BUILTIN_FORMAT "%s"
#define NAME_FORMAT "%s"
#define COMMAND_FORMAT "<span style='italic'>%s</span>"
#define INCLUDE_FORMAT "<span style='italic' color='dark green'>%s</span>"
#define INCLUDE_PATH_FORMAT "<span style='italic' color='dark green'>%s</span>"

#define ICON_SIZE 24

/***********/
/* Globals */
/***********/
enum
{
  COLUMN_ICON, COLUMN_NAME, COLUMN_COMMAND, COLUMN_HIDDEN, COLUMN_TYPE,
  COLUMN_OPTION_1, COLUMN_OPTION_2, COLUMN_OPTION_3, COLUMNS
};

enum _ENTRY_TYPE
{
  TITLE, MENU, APP, SEPARATOR, BUILTIN, INCLUDE_FILE, INCLUDE_SYSTEM
};

enum
{
  DND_TARGET_MENUEDITOR, DND_TARGET_TEXT_PLAIN, DND_TARGET_APP_DESKTOP, TARGETS
};

typedef struct _menueditor_app MenuEditor;
typedef enum _ENTRY_TYPE ENTRY_TYPE;

struct _menueditor_app
{
  gboolean menu_modified;
  gchar *menu_file_name;

  GtkWidget *window;

  /* Tree */
  GtkWidget *treeview;

  /* Menus */
  GtkWidget *menu_item_file;
  GtkWidget *menu_item_file_new;
  GtkWidget *menu_item_file_open;
  GtkWidget *menu_item_file_default;
  GtkWidget *menu_item_file_save;
  GtkWidget *menu_item_file_saveas;
  GtkWidget *menu_item_file_close;
  GtkWidget *menu_item_file_exit;

  GtkWidget *menu_item_edit;
  GtkWidget *menu_item_edit_add;
  GtkWidget *menu_item_edit_add_menu;
  GtkWidget *menu_item_edit_del;
  GtkWidget *menu_item_edit_up;
  GtkWidget *menu_item_edit_down;

  GtkWidget *menu_item_help;
  GtkWidget *menu_item_help_about;

  /* Popup menu */
  GtkWidget *menu_popup;
  GtkWidget *menu_item_popup_edit;
  GtkWidget *menu_item_popup_add;
  GtkWidget *menu_item_popup_addmenu;
  GtkWidget *menu_item_popup_del;
  GtkWidget *menu_item_popup_up;
  GtkWidget *menu_item_popup_down;

  /* Toolbar */
  GtkWidget *toolbar_new;
  GtkWidget *toolbar_open;
  GtkWidget *toolbar_save;
  GtkWidget *toolbar_close;
  GtkWidget *toolbar_collapse;
  GtkWidget *toolbar_expand;
  GtkWidget *toolbar_add;
  GtkWidget *toolbar_del;
  GtkWidget *toolbar_up;
  GtkWidget *toolbar_down;
};

extern GdkPixbuf *dummy_icon;
#endif
