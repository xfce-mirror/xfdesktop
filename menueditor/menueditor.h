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

#include <gtk/gtk.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include <libxfcegui4/libxfcegui4.h>
#include <libxfce4util/util.h>

#include <libintl.h>
#undef _
#define _(String) dgettext (PACKAGE, String)
#ifdef gettext_noop
 #define N_(String) gettext_noop (String)
#else
 #define N_(String) (String)
#endif

#include "config.h"

#include "me-icon16.xpm"
#include "me-icon32.xpm"
#include "me-icon24.xpm"
#include "me-icon48.xpm"

/* Search path for menu.xml file */
#define SEARCHPATH (SYSCONFDIR G_DIR_SEPARATOR_S "xfce4" G_DIR_SEPARATOR_S "%F.%L:"\
                    SYSCONFDIR G_DIR_SEPARATOR_S "xfce4" G_DIR_SEPARATOR_S "%F.%l:"\
                    SYSCONFDIR G_DIR_SEPARATOR_S "xfce4" G_DIR_SEPARATOR_S "%F")


#define TITLE_FORMAT "<span weight='bold' color='dim grey' style='italic'>%s</span>"
#define MENU_FORMAT "<span weight='bold'>%s</span>"
#define SEPARATOR_FORMAT "<span color='grey'>%s</span>"
#define BUILTIN_FORMAT "%s"
#define NAME_FORMAT "%s"
#define COMMAND_FORMAT "<span style='italic'>%s</span>"
#define INCLUDE_FORMAT "<span style='italic' color='dark green'>%s</span>"
#define INCLUDE_PATH_FORMAT "<span style='italic' color='dark green'>%s</span>"

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
void browse_command_cb(GtkWidget *widget, GtkEntry *entry_command);

/* Main window */
void create_main_window();

void load_menu_in_tree(xmlNodePtr menu, GtkTreeIter *p);
static GdkPixbuf * find_icon (gchar const *ifile);

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static guchar icon_size = 24;
static gchar icon_theme[]="Curve"; /* Set Curve as theme (it really needs to be in a library) */
static gchar const *pix_ext[] = {
  ".svgz",
  ".svg",
  ".png",
  ".xpm",
  NULL
};

#ifdef HAVE_GETENV
static gchar *kdefmts[] = {
  "%s/share/icons/default.kde/scalable/apps/%s",
  "%s/share/icons/default.kde/48x48/apps/%s",
  "%s/share/icons/default.kde/32x32/apps/%s",
  "%s/share/icons/hicolor/scalable/apps/%s",
  "%s/share/icons/hicolor/48x48/apps/%s",
  "%s/share/icons/hicolor/32x32/apps/%s",
  NULL
};
#endif

static gchar const *pix_paths[] = {
  "/usr/share/xfce4/themes/%s/",  /* for xfce4 theme-specific path */
  "/usr/share/icons/%s/scalable/apps/",  /* ditto */
  "/usr/share/icons/%s/48x48/apps/",  /* ditto */
  "/usr/share/icons/%s/32x32/apps/",  /* ditto */
  "/usr/share/pixmaps/",
  "/usr/share/icons/hicolor/scalable/apps/",
  "/usr/share/icons/hicolor/48x48/apps/",
  "/usr/share/icons/hicolor/32x32/apps/",
  "/usr/share/icons/gnome/scalable/apps/",  /* gnome's default */
  "/usr/share/icons/gnome/48x48/apps/",  /* ditto */
  "/usr/share/icons/gnome/32x32/apps/",  /* ditto */
  "/usr/share/icons/default.kde/scalable/apps/",  /* kde's default */
  "/usr/share/icons/default.kde/48x48/apps/",  /* ditto */
  "/usr/share/icons/default.kde/32x32/apps/",  /* ditto */
  "/usr/share/icons/locolor/scalable/apps/",  /* fallbacks */
  "/usr/share/icons/locolor/48x48/apps/",
  "/usr/share/icons/locolor/32x32/apps/",
  NULL
};


/***********/
/* Globals */
/***********/

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
       GtkWidget* undo;
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
       GtkWidget* undo;
       GtkWidget* add;
       GtkWidget* del;
       GtkWidget* up;
       GtkWidget* down;
     } main_toolbar;
  
} menueditor_app;


#endif
