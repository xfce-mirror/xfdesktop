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

#ifdef GTK_DISABLE_DEPRECATED
#undef GTK_DISABLE_DEPRECATED
#endif
#include <gtk/gtk.h>

/* includes for the libxml */
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

/* includes for xfce4 */
#include <libxfcegui4/libxfcegui4.h>
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

#define ICON_SIZE 24

/***********/
/* Globals */
/***********/
#if !GLIB_CHECK_VERSION (2, 4, 0)
char *g_markup_printf_escaped (const char *format, ...);
#endif

void browse_command_cb (GtkWidget * widget, gpointer data);
void browse_icon_cb (GtkWidget * widget, gpointer data);
gboolean command_exists (const gchar * command);
void menu_save_cb (GtkWidget * widget, gpointer data);
/* Load the menu in the tree */
void load_menu_in_tree (xmlNodePtr menu, GtkTreeIter * p, gpointer data);

enum
{ ICON_COLUMN, NAME_COLUMN, COMMAND_COLUMN, HIDDEN_COLUMN,
  POINTER_COLUMN, NUM_COLUMNS
};

typedef struct _menueditor_app MenuEditor;

struct _menueditor_app
{
  gboolean menu_modified;
  gchar menu_file_name[255];
  xmlDocPtr xml_menu_file;

  XfceIconTheme *icon_theme;
  GtkWidget *main_window;

  /* AccelGroup */
  GtkAccelGroup *accel_group;

  /* Tree */
  GtkWidget *treeview;
  GtkTreeStore *treestore;

  /* Menus */
  GtkWidget *file_menu_item;
  GtkWidget *file_menu_new;
  GtkWidget *file_menu_open;
  GtkWidget *file_menu_default;
  GtkWidget *file_menu_save;
  GtkWidget *file_menu_saveas;
  GtkWidget *file_menu_close;
  GtkWidget *file_menu_exit;

  GtkWidget *edit_menu_item;
  GtkWidget *edit_menu_add;
  GtkWidget *edit_menu_add_menu;
  GtkWidget *edit_menu_del;
  GtkWidget *edit_menu_up;
  GtkWidget *edit_menu_down;

  GtkWidget *help_menu_item;
  GtkWidget *help_menu_about;

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

  GtkWidget *entry_command;
  GtkWidget *entry_icon;
};


#endif
