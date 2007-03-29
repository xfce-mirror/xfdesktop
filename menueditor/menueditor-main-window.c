/* $Id$ */
/*
 * Copyright (c) 2006 Jean-François Wauthy (pollux@xfce.org)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_MMAP
#include <sys/mman.h>
#endif
#ifndef MAP_FILE
#define MAP_FILE (0)
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include <xfdesktop-common.h>

#include "menueditor-add-dialog.h"
#include "menueditor-add-external-dialog.h"
#include "menueditor-edit-dialog.h"
#include "menueditor-edit-external-dialog.h"
#include "menueditor-main-window.h"

#include "../modules/menu/dummy_icon.h"

#define MENUEDITOR_MAIN_WINDOW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), MENUEDITOR_TYPE_MAIN_WINDOW, MenuEditorMainWindowPrivate))

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

enum
{
  DND_TARGET_MENUEDITOR, DND_TARGET_TEXT_PLAIN, DND_TARGET_APP_DESKTOP, TARGETS
};

/* private struct */
typedef struct {
  GtkActionGroup *action_group;
  GtkUIManager *ui_manager;

  GtkWidget *menubar;
  GtkWidget *toolbar;
  GtkWidget *file_browser;
  GtkWidget *treeview;
  
  gchar *menu_file_name;
  gboolean menu_modified;
} MenuEditorMainWindowPrivate;

/* prototypes */
static void menueditor_main_window_class_init (MenuEditorMainWindowClass *);
static void menueditor_main_window_init (MenuEditorMainWindow *);
static void menueditor_main_window_finalize (GObject * object);

static void action_quit (GtkAction *, MenuEditorMainWindow *);
static void action_new_menu (GtkAction *, MenuEditorMainWindow *);
static void action_open_menu (GtkAction *, MenuEditorMainWindow *);
static void action_open_default_menu (GtkAction *, MenuEditorMainWindow *);
static void action_save_menu (GtkAction *, MenuEditorMainWindow *);
static void action_save_menu_as (GtkAction *, MenuEditorMainWindow *);
static void action_close_menu (GtkAction *, MenuEditorMainWindow *);

static void action_edit (GtkAction *, MenuEditorMainWindow *);
static void action_add (GtkAction *, MenuEditorMainWindow *);
static void action_add_external (GtkAction *, MenuEditorMainWindow *);
static void action_remove (GtkAction *, MenuEditorMainWindow *);
static void action_up (GtkAction *, MenuEditorMainWindow *);
static void action_down (GtkAction *, MenuEditorMainWindow *);

static void action_about (GtkAction *, MenuEditorMainWindow *);

static void action_collapse_all (GtkAction *, MenuEditorMainWindow *);
static void action_expand_all (GtkAction *, MenuEditorMainWindow *);

static gboolean cb_delete_main_window (GtkWidget *, GdkEvent *, MenuEditorMainWindow *);
static void cb_treeview_cursor_changed (GtkTreeView *, MenuEditorMainWindow *);
static void cb_column_visible_toggled (GtkCellRendererToggle *, gchar *, MenuEditorMainWindow *);
static gboolean cb_treeview_button_pressed (GtkTreeView *, GdkEventButton *, MenuEditorMainWindow *);
static void cb_treeview_row_activated (GtkWidget *, GtkTreePath *, GtkTreeViewColumn *, MenuEditorMainWindow *);

static gboolean load_menu_in_treeview (const gchar *, MenuEditorMainWindow *);
static void save_treeview_in_file (MenuEditorMainWindow *window);

static void copy_menuelement_to (MenuEditorMainWindowPrivate *priv, GtkTreeIter *src, GtkTreeIter *dest, 
								 GtkTreeViewDropPosition position);
static void cb_treeview_drag_data_get (GtkWidget * widget, GdkDragContext * dc,
                           GtkSelectionData * data, guint info, guint time, MenuEditorMainWindowPrivate *priv);
static void cb_treeview_drag_data_rcv (GtkWidget * widget, GdkDragContext * dc,
                           guint x, guint y, GtkSelectionData * sd, guint info, guint t, MenuEditorMainWindow *window);

static gchar *extract_text_from_markup (const gchar *markup);

/* globals */
static GtkWindowClass *parent_class = NULL;
static const GtkActionEntry action_entries[] = {
  {"file-menu", NULL, N_("_File"), NULL,},
  {"new-menu", GTK_STOCK_NEW, N_("_New"), NULL, N_("Create a new empty menu"), G_CALLBACK (action_new_menu),},
  {"open-menu", GTK_STOCK_OPEN, N_("_Open"), NULL, N_("Open existing menu"), G_CALLBACK (action_open_menu),},
  {"open-default-menu", NULL, N_("Open _default menu"), NULL, N_("Open default menu"), G_CALLBACK (action_open_default_menu),},
  {"save-menu", GTK_STOCK_SAVE, N_("_Save"), NULL, N_("Save modifications"), G_CALLBACK (action_save_menu),},
  {"save-menu-as", GTK_STOCK_SAVE_AS, N_("Save _as..."), NULL, N_("Save menu under a given name"), G_CALLBACK (action_save_menu_as),},
  {"close-menu", GTK_STOCK_CLOSE, N_("_Close"), NULL, N_("Close menu"), G_CALLBACK (action_close_menu),},
  {"quit", GTK_STOCK_QUIT, N_("_Quit"), NULL, N_("Quit Xfce4-Menueditor"), G_CALLBACK (action_quit),},
  
  {"edit-menu", NULL, N_("_Edit"), NULL,},
  {"edit", GTK_STOCK_EDIT, N_("_Edit entry"), NULL, N_("Edit selected entry"), G_CALLBACK (action_edit),},
  {"add", GTK_STOCK_ADD, N_("_Add entry"), NULL, N_("Add a new entry in the menu"), G_CALLBACK (action_add),},
  {"add-external", NULL, N_("Add _external"), NULL, N_("Add an external entry"), G_CALLBACK (action_add_external),},
  {"remove", GTK_STOCK_REMOVE, N_("_Remove entry"), NULL, N_("Remove entry"), G_CALLBACK (action_remove),},
  {"up", GTK_STOCK_GO_UP, N_("_Up"), NULL, N_("Move entry up"), G_CALLBACK (action_up),},
  {"down", GTK_STOCK_GO_DOWN, N_("_Down"), NULL, N_("Move entry down"), G_CALLBACK (action_down),},
  
  {"help-menu", NULL, N_("_Help"), NULL,},
  {"about", GTK_STOCK_ABOUT, N_("_About..."), NULL, N_("Show informations about xfce4-menueditor"), G_CALLBACK (action_about),},
  
  {"collapse-all", GTK_STOCK_ZOOM_OUT, N_("Collapse all"), NULL, N_("Collapse all menu entries"), G_CALLBACK (action_collapse_all),},
  {"expand-all", GTK_STOCK_ZOOM_IN, N_("Expand all"), NULL, N_("Expand all menu entries"), G_CALLBACK (action_expand_all),},
};

static GdkPixbuf *dummy_icon = NULL;

/******************************/
/* MenuEditorMainWindow class */
/******************************/
GtkType
menueditor_main_window_get_type (void)
{
  static GtkType main_window_type = 0;

  if (!main_window_type) {
    static const GTypeInfo main_window_info = {
      sizeof (MenuEditorMainWindowClass),
      NULL,
      NULL,
      (GClassInitFunc) menueditor_main_window_class_init,
      NULL,
      NULL,
      sizeof (MenuEditorMainWindow),
      0,
      (GInstanceInitFunc) menueditor_main_window_init
    };

    main_window_type = g_type_register_static (GTK_TYPE_WINDOW, "MenuEditorMainWindow", &main_window_info, 0);
  }

  return main_window_type;
}

static void
menueditor_main_window_class_init (MenuEditorMainWindowClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  
  g_type_class_add_private (klass, sizeof (MenuEditorMainWindowPrivate));

  parent_class = g_type_class_peek_parent (klass);
  
  object_class->finalize = menueditor_main_window_finalize;
}

static void
menueditor_main_window_finalize (GObject * object)
{
  MenuEditorMainWindow *cobj;
  MenuEditorMainWindowPrivate *priv;

  cobj = MENUEDITOR_MAIN_WINDOW (object);
  priv = MENUEDITOR_MAIN_WINDOW_GET_PRIVATE (cobj);

  g_free (priv->menu_file_name);
  
  if (G_LIKELY (dummy_icon != NULL)) {
	g_object_unref (dummy_icon);
	dummy_icon = NULL;
  }
}

static void
menueditor_main_window_init (MenuEditorMainWindow * mainwin)
{
  MenuEditorMainWindowPrivate *priv = MENUEDITOR_MAIN_WINDOW_GET_PRIVATE (mainwin);
  
  gchar *file;
  GtkAccelGroup *accel_group;
  GdkPixbuf *icon = NULL;
  GtkWidget *vbox;
 
  GtkWidget *scrolledwindow;
  GtkTreeStore *treestore;
  GtkCellRenderer *cell_name, *cell_command, *cell_hidden, *cell_icon;
  GtkTreeViewColumn *column_name, *column_command, *column_hidden;

  GtkAction *action;
  
  GtkTargetEntry gte_src[] = { {"MENUEDITOR_ENTRY", GTK_TARGET_SAME_WIDGET, DND_TARGET_MENUEDITOR},};
  GtkTargetEntry gte_dest[] = {
  {"MENUEDITOR_ENTRY", GTK_TARGET_SAME_WIDGET, DND_TARGET_MENUEDITOR},
  {"text/plain", 0, DND_TARGET_TEXT_PLAIN},
  {"application/x-desktop", 0, DND_TARGET_APP_DESKTOP} 
  };

  if (dummy_icon == NULL)
	dummy_icon = xfce_inline_icon_at_size (dummy_icon_data, ICON_SIZE, ICON_SIZE);
  
  /* Window */
  gtk_window_set_position (GTK_WINDOW (mainwin), GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_title (GTK_WINDOW (mainwin), "Xfce4-MenuEditor");
  gtk_window_set_default_size (GTK_WINDOW (mainwin), 600, 450);
  
  g_signal_connect (G_OBJECT (mainwin), "delete-event", G_CALLBACK (cb_delete_main_window), mainwin);
  
  /* Set default icon */
  icon = xfce_themed_icon_load ("xfce4-menueditor", 48);
  gtk_window_set_icon (GTK_WINDOW (mainwin), icon);
  g_object_unref (icon);

  /* create ui manager */
  priv->action_group = gtk_action_group_new ("menueditor-main-window");
  gtk_action_group_set_translation_domain (priv->action_group, GETTEXT_PACKAGE);
  gtk_action_group_add_actions (priv->action_group, action_entries, G_N_ELEMENTS (action_entries),
                                GTK_WIDGET (mainwin));

  priv->ui_manager = gtk_ui_manager_new ();
  gtk_ui_manager_insert_action_group (priv->ui_manager, priv->action_group, 0);

  xfce_resource_push_path (XFCE_RESOURCE_DATA, DATADIR);
  file = xfce_resource_lookup (XFCE_RESOURCE_DATA, "xfce4-menueditor/xfce4-menueditor.ui");

  if (G_LIKELY (file != NULL)) {
    GError *error = NULL;
    if (gtk_ui_manager_add_ui_from_file (priv->ui_manager, file, &error) == 0) {
      g_warning ("Unable to load %s: %s", file, error->message);
      g_error_free (error);
    }
    gtk_ui_manager_ensure_update (priv->ui_manager);
    g_free (file);
  }
  else {
    g_warning ("Unable to locate xfce4-menueditor/xfce4-menueditor.ui !");
  }
  xfce_resource_pop_path (XFCE_RESOURCE_DATA);
  
  /* accel group */
  accel_group = gtk_ui_manager_get_accel_group (priv->ui_manager);
  gtk_window_add_accel_group (GTK_WINDOW (mainwin), accel_group);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (mainwin), vbox);
  gtk_widget_show (vbox);

  /* menubar */
  priv->menubar = gtk_ui_manager_get_widget (priv->ui_manager, "/main-menu");
  if (G_LIKELY (priv->menubar != NULL)) {
    gtk_box_pack_start (GTK_BOX (vbox), priv->menubar, FALSE, FALSE, 0);
    gtk_widget_show (priv->menubar);
  }
  
  /* toolbar */
  priv->toolbar = gtk_ui_manager_get_widget (priv->ui_manager, "/main-toolbar");
  if (G_LIKELY (priv->toolbar != NULL)) {
    gtk_box_pack_start (GTK_BOX (vbox), priv->toolbar, FALSE, FALSE, 0);
    gtk_widget_show (priv->toolbar);
  }

  /* treeview */
  treestore =
    gtk_tree_store_new (COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_INT,
                        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
  scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (scrolledwindow);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (vbox), scrolledwindow, TRUE, TRUE, 0);

  priv->treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (treestore));
  g_object_unref (treestore);
  gtk_container_add (GTK_CONTAINER (scrolledwindow), priv->treeview);
  gtk_widget_show (priv->treeview);
  
  g_signal_connect (G_OBJECT (priv->treeview), "button-press-event", G_CALLBACK (cb_treeview_button_pressed), mainwin);
  g_signal_connect (G_OBJECT (priv->treeview), "row-activated", G_CALLBACK (cb_treeview_row_activated), mainwin);
  g_signal_connect (G_OBJECT (priv->treeview), "cursor-changed", G_CALLBACK (cb_treeview_cursor_changed), mainwin);

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
  g_signal_connect (G_OBJECT (cell_hidden), "toggled", G_CALLBACK (cb_column_visible_toggled), mainwin);

  gtk_tree_view_column_set_expand (column_name, TRUE);
  gtk_tree_view_column_set_expand (column_command, TRUE);
  gtk_tree_view_column_set_expand (column_hidden, FALSE);

  gtk_tree_view_append_column (GTK_TREE_VIEW (priv->treeview), column_name);
  gtk_tree_view_append_column (GTK_TREE_VIEW (priv->treeview), column_command);
  gtk_tree_view_append_column (GTK_TREE_VIEW (priv->treeview), column_hidden);

  gtk_tree_view_column_set_alignment (column_name, 0.5);
  gtk_tree_view_column_set_alignment (column_command, 0.5);
  gtk_tree_view_column_set_alignment (column_hidden, 0.5);

  gtk_tree_view_expand_all (GTK_TREE_VIEW (priv->treeview));

  /* Set up DnD */
  gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (priv->treeview), GDK_BUTTON1_MASK, gte_src,
					  1, GDK_ACTION_COPY | GDK_ACTION_MOVE);
  gtk_tree_view_enable_model_drag_dest (GTK_TREE_VIEW (priv->treeview), gte_dest, TARGETS, GDK_ACTION_COPY);
  
  g_signal_connect (G_OBJECT (priv->treeview), "drag-data-received", G_CALLBACK (cb_treeview_drag_data_rcv), mainwin);
  g_signal_connect (G_OBJECT (priv->treeview), "drag-data-get", G_CALLBACK (cb_treeview_drag_data_get), priv);
 
  /* Deactivate the widgets not usable */
  action = gtk_action_group_get_action (priv->action_group, "save-menu");
  gtk_action_set_sensitive (action, FALSE);
  action = gtk_action_group_get_action (priv->action_group, "save-menu-as");
  gtk_action_set_sensitive (action, FALSE);
  action = gtk_action_group_get_action (priv->action_group, "close-menu");
  gtk_action_set_sensitive (action, FALSE);
  
  action = gtk_action_group_get_action (priv->action_group, "edit");
  gtk_action_set_sensitive (action, FALSE);
  action = gtk_action_group_get_action (priv->action_group, "add");
  gtk_action_set_sensitive (action, FALSE);
  action = gtk_action_group_get_action (priv->action_group, "add-external");
  gtk_action_set_sensitive (action, FALSE);
  action = gtk_action_group_get_action (priv->action_group, "remove");
  gtk_action_set_sensitive (action, FALSE);
  action = gtk_action_group_get_action (priv->action_group, "up");
  gtk_action_set_sensitive (action, FALSE);
  action = gtk_action_group_get_action (priv->action_group, "down");
  gtk_action_set_sensitive (action, FALSE);
  action = gtk_action_group_get_action (priv->action_group, "edit-menu");
  gtk_action_set_sensitive (action, FALSE);
  
  action = gtk_action_group_get_action (priv->action_group, "collapse-all");
  gtk_action_set_sensitive (action, FALSE);
  action = gtk_action_group_get_action (priv->action_group, "expand-all");
  gtk_action_set_sensitive (action, FALSE);
 
  gtk_widget_set_sensitive (priv->treeview, FALSE);
}

/*************/
/* internals */
/*************/

/* actions */

static void
action_quit (GtkAction * action, MenuEditorMainWindow * window)
{
  cb_delete_main_window (GTK_WIDGET (window), NULL, window);
}

static void
action_new_menu (GtkAction *action, MenuEditorMainWindow *window)
{
  MenuEditorMainWindowPrivate *priv = MENUEDITOR_MAIN_WINDOW_GET_PRIVATE (window);
  GtkWidget *dialog;
  GtkWidget *filesel_dialog;

  /* is there any opened menu ? */
  if (priv->menu_file_name) {
    dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_QUESTION,
                                     GTK_BUTTONS_YES_NO, _("Are you sure you want to close the current menu?"));

    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_NO) {
      gtk_widget_destroy (dialog);
      return;
    }
    gtk_widget_destroy (dialog);
  }

  /* current menu has been modified, saving ? */
  if (priv->menu_modified) {
    dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_QUESTION,
                                     GTK_BUTTONS_YES_NO, _("Do you want to save before closing the file?"));

    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_YES)
      save_treeview_in_file (window);

    gtk_widget_destroy (dialog);
  }

  filesel_dialog =
    gtk_file_chooser_dialog_new (_("Select command"), GTK_WINDOW (window),
								 GTK_FILE_CHOOSER_ACTION_SAVE,
								 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
								 GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);

  if (gtk_dialog_run (GTK_DIALOG (filesel_dialog)) == GTK_RESPONSE_ACCEPT) {
    GtkTreeModel *model;
    gchar *filename = NULL;

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->treeview));
    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filesel_dialog));
    gtk_tree_store_clear (GTK_TREE_STORE (model));
    priv->menu_file_name = filename;
  }

  gtk_widget_destroy (filesel_dialog);
}

static void
action_open_menu (GtkAction *action, MenuEditorMainWindow *window)
{
  MenuEditorMainWindowPrivate *priv = MENUEDITOR_MAIN_WINDOW_GET_PRIVATE (window);
  GtkWidget *filesel_dialog;

  /* Check if there is no other file opened */
  if (priv->menu_file_name && priv->menu_modified) {
    gint response;

    response =
      xfce_message_dialog (GTK_WINDOW (window), _("Question"),
                           GTK_STOCK_DIALOG_QUESTION,
                           _("Do you want to save before opening an other menu ?"),
                           NULL, XFCE_CUSTOM_BUTTON,
                           _("Ignore modifications"), GTK_RESPONSE_NO,
                           GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE, GTK_RESPONSE_YES, NULL);
    switch (response) {
    case GTK_RESPONSE_CANCEL:
      return;
    case GTK_RESPONSE_YES:
	  save_treeview_in_file (window);
    }
  }

  filesel_dialog =
    gtk_file_chooser_dialog_new (_("Open menu file"), GTK_WINDOW (window),
				 GTK_FILE_CHOOSER_ACTION_OPEN,
				 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				 GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
  
  if (gtk_dialog_run (GTK_DIALOG (filesel_dialog)) == GTK_RESPONSE_ACCEPT) {
    gchar *filename = NULL;

    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filesel_dialog));

    if (priv->menu_file_name) {
      GtkTreeModel *model;

      model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->treeview));
      gtk_tree_store_clear (GTK_TREE_STORE (model));
      g_free (priv->menu_file_name);
      priv->menu_file_name = NULL;
    }

    load_menu_in_treeview (filename, window);

    g_free (filename);
  }

  gtk_widget_destroy (filesel_dialog);
}

static
void action_open_default_menu (GtkAction * action, MenuEditorMainWindow * window)
{
  MenuEditorMainWindowPrivate *priv = MENUEDITOR_MAIN_WINDOW_GET_PRIVATE (window);
  gchar *home_menu = NULL;

  /* Check if there is no other file opened */
  if (priv->menu_file_name != NULL && priv->menu_modified) {
    gint response;

    response =
      xfce_message_dialog (GTK_WINDOW (window), _("Question"),
                           GTK_STOCK_DIALOG_QUESTION,
                           _("Do you want to save before opening the default menu ?"),
                           NULL, XFCE_CUSTOM_BUTTON,
                           _("Ignore modifications"), GTK_RESPONSE_NO,
                           GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE, GTK_RESPONSE_YES, NULL);
    switch (response) {
    case GTK_RESPONSE_CANCEL:
      return;
    case GTK_RESPONSE_YES:
	  save_treeview_in_file (window);
	  break;
    }
  }

  if (priv->menu_file_name != NULL) {
    GtkTreeStore *treestore;

    treestore = GTK_TREE_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (priv->treeview)));
    gtk_tree_store_clear (treestore);
    g_free (priv->menu_file_name);
    priv->menu_file_name = NULL;
  }

  home_menu = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, "xfce4/desktop/menu.xml", TRUE);

  if (g_file_test (home_menu, G_FILE_TEST_EXISTS)) {
    load_menu_in_treeview (home_menu, window);
  }
  else {
    gchar *filename = xfce_desktop_get_menufile ();
    gchar *window_title = NULL;
    
    load_menu_in_treeview (filename, window);

    g_free (priv->menu_file_name);
    priv->menu_file_name = g_strdup (home_menu);
    save_treeview_in_file (window);
    
    window_title = g_strdup_printf ("Xfce4-MenuEditor - %s", home_menu);
    gtk_window_set_title (GTK_WINDOW (window), window_title);
    g_free (window_title);


    g_free (filename);
  }
  
  g_free (home_menu);
}

static void
action_save_menu (GtkAction *action, MenuEditorMainWindow *window)
{
  save_treeview_in_file (window);
}

static void
action_save_menu_as (GtkAction *action, MenuEditorMainWindow *window)
{
  MenuEditorMainWindowPrivate *priv = MENUEDITOR_MAIN_WINDOW_GET_PRIVATE (window);
  GtkWidget *save_dialog;
  
  save_dialog = gtk_file_chooser_dialog_new (_("Save menu file as"), GTK_WINDOW (window),
                                             GTK_FILE_CHOOSER_ACTION_SAVE,
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                             GTK_STOCK_SAVE_AS, GTK_RESPONSE_ACCEPT, NULL);
  
  gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (save_dialog), TRUE);
  gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (save_dialog), "menu.xml");
  
  if (gtk_dialog_run (GTK_DIALOG (save_dialog)) == GTK_RESPONSE_ACCEPT) {
    gchar *filename = NULL;
    gchar *window_title = NULL;
    
    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (save_dialog));
    
    g_free (priv->menu_file_name);
    priv->menu_file_name = g_strdup (filename);
    save_treeview_in_file (window);
    
    window_title = g_strdup_printf ("Xfce4-MenuEditor - %s", filename);
    gtk_window_set_title (GTK_WINDOW (window), window_title);
    g_free (window_title);
  }
  
  gtk_widget_destroy (save_dialog);
}

static void
action_close_menu (GtkAction *action, MenuEditorMainWindow *window)
{
  MenuEditorMainWindowPrivate *priv = MENUEDITOR_MAIN_WINDOW_GET_PRIVATE (window);
  GtkTreeModel *model;

  if (priv->menu_modified) {
    gint response;

    response =
      xfce_message_dialog (GTK_WINDOW (window), _("Question"),
                           GTK_STOCK_DIALOG_QUESTION,
                           _("Do you want to save before closing the menu ?"),
                           NULL, XFCE_CUSTOM_BUTTON,
                           _("Ignore modifications"), GTK_RESPONSE_NO,
                           GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE, GTK_RESPONSE_YES, NULL);
    switch (response) {
    case GTK_RESPONSE_CANCEL:
      return;
    case GTK_RESPONSE_YES:
      save_treeview_in_file (window);
	  break;
    }
  }

  g_free (priv->menu_file_name);
  priv->menu_file_name = NULL;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->treeview));
  gtk_tree_store_clear (GTK_TREE_STORE (model));

  priv->menu_modified = FALSE;

  action = gtk_action_group_get_action (priv->action_group, "save-menu");
  gtk_action_set_sensitive (action, FALSE);
  action = gtk_action_group_get_action (priv->action_group, "save-menu-as");
  gtk_action_set_sensitive (action, FALSE);
  action = gtk_action_group_get_action (priv->action_group, "close-menu");
  gtk_action_set_sensitive (action, FALSE);
  
  action = gtk_action_group_get_action (priv->action_group, "edit");
  gtk_action_set_sensitive (action, FALSE);
  action = gtk_action_group_get_action (priv->action_group, "add");
  gtk_action_set_sensitive (action, FALSE);
  action = gtk_action_group_get_action (priv->action_group, "add-external");
  gtk_action_set_sensitive (action, FALSE);
  action = gtk_action_group_get_action (priv->action_group, "remove");
  gtk_action_set_sensitive (action, FALSE);
  action = gtk_action_group_get_action (priv->action_group, "up");
  gtk_action_set_sensitive (action, FALSE);
  action = gtk_action_group_get_action (priv->action_group, "down");
  gtk_action_set_sensitive (action, FALSE);
  action = gtk_action_group_get_action (priv->action_group, "edit-menu");
  gtk_action_set_sensitive (action, FALSE);
  
  action = gtk_action_group_get_action (priv->action_group, "collapse-all");
  gtk_action_set_sensitive (action, FALSE);
  action = gtk_action_group_get_action (priv->action_group, "expand-all");
  gtk_action_set_sensitive (action, FALSE);
 
  gtk_widget_set_sensitive (priv->treeview, FALSE);
  gtk_window_set_title (GTK_WINDOW (window), "Xfce4-MenuEditor");
}

static void
action_edit (GtkAction *action, MenuEditorMainWindow *window)
{
  MenuEditorMainWindowPrivate *priv = MENUEDITOR_MAIN_WINDOW_GET_PRIVATE (window);
  GtkTreeModel *model;
  GtkTreeSelection *selection;
  GtkTreeIter iter_selected;
  GtkWidget *edit_dialog;
  
  EntryType type;
  gchar *name = NULL, *command = NULL, *icon = NULL;
  gchar *option_1 = NULL, *option_2 = NULL, *option_3 = NULL;
  gchar *temp = NULL;
  
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->treeview));

  if (!gtk_tree_selection_get_selected (selection, &model, &iter_selected)) {
    g_warning ("no entry selected !");
    return;
  }
  
  gtk_tree_model_get (model, &iter_selected,
                      COLUMN_NAME, &name,
                      COLUMN_COMMAND, &command,
                      COLUMN_TYPE, &type,
                      COLUMN_OPTION_1, &option_1, COLUMN_OPTION_2, &option_2, COLUMN_OPTION_3, &option_3, -1);
  
  if (type == SEPARATOR)
    goto cleanup;
  else if  (type == INCLUDE_FILE || type == INCLUDE_SYSTEM) {
    edit_dialog = menueditor_edit_external_dialog_new (GTK_WINDOW (window));
    
    menueditor_edit_external_dialog_set_entry_type (MENUEDITOR_EDIT_EXTERNAL_DIALOG (edit_dialog), type);
    
    if (type == INCLUDE_FILE) {
      temp = extract_text_from_markup (command);
      g_free (command);
      command = temp;
      menueditor_edit_external_dialog_set_entry_source (MENUEDITOR_EDIT_EXTERNAL_DIALOG (edit_dialog), command);
    } else {
      if (strcmp (option_1, "simple") == 0)
        menueditor_edit_external_dialog_set_entry_style (MENUEDITOR_EDIT_EXTERNAL_DIALOG (edit_dialog), SIMPLE);
      else
        menueditor_edit_external_dialog_set_entry_style (MENUEDITOR_EDIT_EXTERNAL_DIALOG (edit_dialog), MULTI_LEVEL);

      menueditor_edit_external_dialog_set_entry_unique (MENUEDITOR_EDIT_EXTERNAL_DIALOG (edit_dialog), 
                                                        (strcmp (option_2, "true") == 0));
    }
      
    if (gtk_dialog_run (GTK_DIALOG (edit_dialog)) == GTK_RESPONSE_OK) {  
      gchar *new_command = NULL;
      gchar *new_option_1 = NULL, *new_option_2 = NULL, *new_option_3 = NULL;
      
      if (type == INCLUDE_FILE) {
        gchar *selected_source = NULL;
        
        selected_source = menueditor_edit_external_dialog_get_entry_source (MENUEDITOR_EDIT_EXTERNAL_DIALOG (edit_dialog));
        
        new_command = g_markup_printf_escaped (INCLUDE_PATH_FORMAT, selected_source);
        new_option_1 = g_strdup ("");
        new_option_2 = g_strdup ("");
        new_option_3 = g_strdup ("");
        
        g_free (selected_source);
      } else {
        ExternalEntryStyle style;
        gboolean unique;
        
        style = menueditor_edit_external_dialog_get_entry_style (MENUEDITOR_EDIT_EXTERNAL_DIALOG (edit_dialog));
        unique = menueditor_edit_external_dialog_get_entry_unique (MENUEDITOR_EDIT_EXTERNAL_DIALOG (edit_dialog));
        
        new_command = g_strdup (command);
        if (style == SIMPLE)
          new_option_1 = g_strdup ("simple");
        else
          new_option_1 = g_strdup ("multilevel");
        if (unique)
          new_option_2 = g_strdup ("true");
        else
          new_option_2 = g_strdup ("false");
        new_option_3 = g_strdup ("false");
      }
      
      gtk_tree_store_set (GTK_TREE_STORE (model), &iter_selected, 
                          COLUMN_COMMAND, new_command,
                          COLUMN_OPTION_1, new_option_1,
                          COLUMN_OPTION_2, new_option_2,
                          COLUMN_OPTION_3, new_option_3, -1);
      
      menueditor_main_window_set_menu_modified (window);
    
      g_free (new_command);
      g_free (new_option_1);
      g_free (new_option_2);
      g_free (new_option_3);
    }
    
    gtk_widget_destroy (edit_dialog);
  } else {
    temp = extract_text_from_markup (name);
    g_free (name);
    name = temp;
    temp = extract_text_from_markup (command);
    g_free (command);
    command = temp;
  
    edit_dialog = menueditor_edit_dialog_new (GTK_WINDOW (window));
    menueditor_edit_dialog_set_entry_type (MENUEDITOR_EDIT_DIALOG (edit_dialog), type);
    menueditor_edit_dialog_set_entry_name (MENUEDITOR_EDIT_DIALOG (edit_dialog), name);
    switch (type) {
      case APP:
        menueditor_edit_dialog_set_entry_command (MENUEDITOR_EDIT_DIALOG (edit_dialog), command);
        menueditor_edit_dialog_set_entry_icon (MENUEDITOR_EDIT_DIALOG (edit_dialog), option_1); 
        menueditor_edit_dialog_set_entry_run_in_terminal (MENUEDITOR_EDIT_DIALOG (edit_dialog), (strcmp (option_2, "true") == 0));
        menueditor_edit_dialog_set_entry_startup_notification (MENUEDITOR_EDIT_DIALOG (edit_dialog), (strcmp (option_3, "true") == 0));
        break;
      case MENU:
      case TITLE:
      case BUILTIN:
        menueditor_edit_dialog_set_entry_icon (MENUEDITOR_EDIT_DIALOG (edit_dialog), option_1); 
      default:
        break;
    }
          
    if (gtk_dialog_run (GTK_DIALOG (edit_dialog)) == GTK_RESPONSE_OK) {
      gchar *selected_name = NULL, *selected_command = NULL, *selected_icon = NULL;
      gboolean snotify, interm;
    
      GdkPixbuf *new_icon = NULL;
      gchar *new_name = NULL;
      gchar *new_command = NULL;
      gchar *new_option_1 = NULL, *new_option_2 = NULL, *new_option_3 = NULL;

      selected_name = menueditor_edit_dialog_get_entry_name (MENUEDITOR_EDIT_DIALOG (edit_dialog));
      selected_command = menueditor_edit_dialog_get_entry_command (MENUEDITOR_EDIT_DIALOG (edit_dialog));
      selected_icon = menueditor_edit_dialog_get_entry_icon (MENUEDITOR_EDIT_DIALOG (edit_dialog));
      snotify = menueditor_edit_dialog_get_entry_startup_notification (MENUEDITOR_EDIT_DIALOG (edit_dialog));
      interm = menueditor_edit_dialog_get_entry_run_in_terminal (MENUEDITOR_EDIT_DIALOG (edit_dialog));
    
      if (selected_icon) {
        new_icon = xfce_themed_icon_load (selected_icon, ICON_SIZE);    
      } else {
		new_icon = xfce_inline_icon_at_size (dummy_icon_data, ICON_SIZE, ICON_SIZE);
	  }
      new_option_1 = g_strdup (selected_icon);
    
      switch (type) {
        case APP:
          new_name = g_markup_printf_escaped (NAME_FORMAT, selected_name);
          new_command = g_markup_printf_escaped (COMMAND_FORMAT, selected_command);
      
          if (interm)
            new_option_2 = g_strdup ("true");
          else
            new_option_2 = g_strdup ("false");
          if (snotify)
            new_option_3 = g_strdup ("true");
          else
            new_option_3 = g_strdup ("false");
          break;
        case MENU:
          new_name = g_markup_printf_escaped (MENU_FORMAT, selected_name);
          new_command = g_strdup ("");
          new_option_2 = g_strdup ("");
          new_option_3 = g_strdup ("");
          break;
        case TITLE:
          new_name = g_markup_printf_escaped (TITLE_FORMAT, selected_name);
          new_command = g_strdup ("");
          new_option_2 = g_strdup ("");
          new_option_3 = g_strdup ("");
          break;
        case BUILTIN:
          new_name = g_markup_printf_escaped (BUILTIN_FORMAT, selected_name);
          new_command = g_markup_printf_escaped (COMMAND_FORMAT, _("quit"));
          new_option_2 = g_strdup ("");
          new_option_3 = g_strdup ("");
          break;
        default:
          break;
      }
    
      gtk_tree_store_set (GTK_TREE_STORE (model), &iter_selected, 
                          COLUMN_ICON, new_icon,
                          COLUMN_NAME, new_name,
                          COLUMN_COMMAND, new_command,
                          COLUMN_OPTION_1, new_option_1,
                          COLUMN_OPTION_2, new_option_2,
                          COLUMN_OPTION_3, new_option_3, -1);
      
      menueditor_main_window_set_menu_modified (window);
    
      if (G_LIKELY (G_IS_OBJECT (new_icon)))
        g_object_unref (new_icon);
      g_free (new_name);
      g_free (new_command);
      g_free (new_option_1);
      g_free (new_option_2);
      g_free (new_option_3);
    
      g_free (selected_name);
      g_free (selected_command);
      g_free (selected_icon);
    }
    
    gtk_widget_destroy (edit_dialog);
  }
  
cleanup:
  g_free (name);
  g_free (command);
  g_free (icon);
  g_free (option_1);
  g_free (option_2);
  g_free (option_3);
}

static void
action_add (GtkAction *action, MenuEditorMainWindow *window)
{
  MenuEditorMainWindowPrivate *priv = MENUEDITOR_MAIN_WINDOW_GET_PRIVATE (window);
  GtkWidget *add_dialog;

  add_dialog = menueditor_add_dialog_new (GTK_WINDOW (window));
  
  if (gtk_dialog_run (GTK_DIALOG (add_dialog)) == GTK_RESPONSE_OK) {
    EntryType entry_type;
    gchar *selected_name = NULL, *selected_command = NULL, *selected_icon = NULL;
    gboolean snotify, interm;
    
    GdkPixbuf *icon = NULL;
    gchar *name = NULL;
    gchar *command = NULL;
    gchar *option_1 = NULL;
    gchar *option_2 = NULL;
    gchar *option_3 = NULL;
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter_new, iter_selected;
    
    entry_type = menueditor_add_dialog_get_entry_type (MENUEDITOR_ADD_DIALOG (add_dialog));
    selected_name = menueditor_add_dialog_get_entry_name (MENUEDITOR_ADD_DIALOG (add_dialog));
    selected_command = menueditor_add_dialog_get_entry_command (MENUEDITOR_ADD_DIALOG (add_dialog));
    selected_icon = menueditor_add_dialog_get_entry_icon (MENUEDITOR_ADD_DIALOG (add_dialog));
    snotify = menueditor_add_dialog_get_entry_startup_notification (MENUEDITOR_ADD_DIALOG (add_dialog));
    interm = menueditor_add_dialog_get_entry_run_in_terminal (MENUEDITOR_ADD_DIALOG (add_dialog));
    
    switch (entry_type) {
      case APP:
        /* Set icon if needed */
        if (selected_icon)
          icon =
            xfce_themed_icon_load (selected_icon, ICON_SIZE);

        name = g_markup_printf_escaped (NAME_FORMAT, selected_name);
        command = g_markup_printf_escaped (COMMAND_FORMAT, selected_command);
        option_1 = g_strdup (selected_icon);
        if (interm)
          option_2 = g_strdup ("true");
        else
          option_2 = g_strdup ("false");
        if (snotify)
          option_3 = g_strdup ("true");
        else
          option_3 = g_strdup ("false");
        break;
      case MENU:
        /* Set icon if needed */
        if (selected_icon)
          icon =
            xfce_themed_icon_load (selected_icon, ICON_SIZE);

        name = g_markup_printf_escaped (MENU_FORMAT, selected_name);
        command = g_strdup ("");
        option_1 = g_strdup (selected_icon);
        option_2 = g_strdup ("");
        option_3 = g_strdup ("");
        break;
      case SEPARATOR:
        name = g_markup_printf_escaped (SEPARATOR_FORMAT, _("--- separator ---"));
        command = g_strdup ("");
        option_1 = g_strdup ("");
        option_2 = g_strdup ("");
        option_3 = g_strdup ("");
        break;
      case TITLE:
        /* Set icon if needed */
        if (selected_icon)
          icon =
            xfce_themed_icon_load (selected_icon, ICON_SIZE);

        name = g_markup_printf_escaped (TITLE_FORMAT, selected_name);
        command = g_strdup ("");
        option_1 = g_strdup (selected_icon);
        option_2 = g_strdup ("");
        option_3 = g_strdup ("");
        break;
      case BUILTIN:
        /* Set icon if needed */
        if (selected_icon)
          icon =
            xfce_themed_icon_load (selected_icon, ICON_SIZE);

        name = g_markup_printf_escaped (BUILTIN_FORMAT, selected_name);
        command = g_markup_printf_escaped (COMMAND_FORMAT, _("quit"));
        option_1 = g_strdup (selected_icon);
        option_2 = g_strdup ("");
        option_3 = g_strdup ("");
        break;
      default:
        break;
    }
      
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->treeview));
    if (gtk_tree_selection_get_selected (selection, &model, &iter_selected)) {
      EntryType type_selected = SEPARATOR;
      GtkTreePath *path = NULL;
      
      gtk_tree_model_get (model, &iter_selected, COLUMN_TYPE, &type_selected, -1);
      
      if (type_selected == MENU) {
        GtkTreePath *path_selected = NULL;
        
        /* Insert in the submenu */
        gtk_tree_store_prepend (GTK_TREE_STORE (model), &iter_new, &iter_selected);
        
        path_selected = gtk_tree_model_get_path (model, &iter_selected);
        gtk_tree_view_expand_row (GTK_TREE_VIEW (priv->treeview), path_selected, FALSE);
        gtk_tree_path_free (path_selected);
      } else
        /* Insert after the selected entry */
        gtk_tree_store_insert_after (GTK_TREE_STORE (model), &iter_new, NULL, &iter_selected);
      
      path = gtk_tree_model_get_path (model, &iter_new);
      gtk_tree_view_set_cursor (GTK_TREE_VIEW (priv->treeview), path, NULL, FALSE);
      gtk_tree_path_free (path);
    }
    else
      /* Insert at the beginning of the tree */
      gtk_tree_store_prepend (GTK_TREE_STORE (model), &iter_new, NULL);
    
    gtk_tree_store_set (GTK_TREE_STORE (model), &iter_new,
                        COLUMN_ICON, G_IS_OBJECT (icon) ? icon : dummy_icon,
                        COLUMN_NAME, name,
                        COLUMN_COMMAND, command,
                        COLUMN_HIDDEN, FALSE,
                        COLUMN_TYPE, entry_type,
                        COLUMN_OPTION_1, option_1, COLUMN_OPTION_2, option_2, COLUMN_OPTION_3, option_3, -1);
    
    menueditor_main_window_set_menu_modified (window);
    
    if (G_IS_OBJECT (icon))
      g_object_unref (icon);
    g_free (name);
    g_free (command);
    g_free (option_1);
    g_free (option_2);
    g_free (option_3);
      
    g_free (selected_name);
    g_free (selected_command);
    g_free (selected_icon);
  }

  gtk_widget_destroy (add_dialog);
}

static void
action_add_external (GtkAction *action, MenuEditorMainWindow *window)
{
  MenuEditorMainWindowPrivate *priv = MENUEDITOR_MAIN_WINDOW_GET_PRIVATE (window);
  GtkWidget *add_dialog;

  add_dialog = menueditor_add_external_dialog_new (GTK_WINDOW (window));
  
  if (gtk_dialog_run (GTK_DIALOG (add_dialog)) == GTK_RESPONSE_OK) {
    EntryType entry_type;
    ExternalEntryStyle external_type;
    gchar *selected_src = NULL;
    gboolean unique;
    
    gchar *name = NULL;
    gchar *command = NULL;
    gchar *option_1 = NULL;
    gchar *option_2 = NULL;
    gchar *option_3 = NULL;
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter_new, iter_selected;
    
    entry_type = menueditor_add_external_dialog_get_entry_type (MENUEDITOR_ADD_EXTERNAL_DIALOG (add_dialog));
    selected_src = menueditor_add_external_dialog_get_entry_source (MENUEDITOR_ADD_EXTERNAL_DIALOG (add_dialog));
    external_type = menueditor_add_external_dialog_get_entry_style (MENUEDITOR_ADD_EXTERNAL_DIALOG (add_dialog));
    unique = menueditor_add_external_dialog_get_entry_unique (MENUEDITOR_ADD_EXTERNAL_DIALOG (add_dialog));
    
    switch (entry_type) {
      case INCLUDE_FILE:
        name = g_markup_printf_escaped (INCLUDE_FORMAT, _("--- include ---"));
        command = g_markup_printf_escaped (INCLUDE_PATH_FORMAT, selected_src);
        option_1 = g_strdup ("");
        option_2 = g_strdup ("");
        option_3 = g_strdup ("");
        break;
      case INCLUDE_SYSTEM:
        name = g_markup_printf_escaped (INCLUDE_FORMAT, _("--- include ---"));
        command = g_markup_printf_escaped (INCLUDE_PATH_FORMAT, _("system"));
        if (external_type == SIMPLE)
          option_1 = g_strdup ("simple");
        else
          option_1 = g_strdup ("multilevel");
        if (unique)
          option_2 = g_strdup ("true");
        else
          option_2 = g_strdup ("false");
        option_3 = g_strdup ("false");
        break;
      default:
        break;
    }
    
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->treeview));
    if (gtk_tree_selection_get_selected (selection, &model, &iter_selected)) {
      EntryType type_selected = SEPARATOR;
      GtkTreePath *path = NULL;
      
      gtk_tree_model_get (model, &iter_selected, COLUMN_TYPE, &type_selected, -1);
      
      if (type_selected == MENU) {
        GtkTreePath *path_selected = NULL;
        
        /* Insert in the submenu */
        gtk_tree_store_prepend (GTK_TREE_STORE (model), &iter_new, &iter_selected);
        
        path_selected = gtk_tree_model_get_path (model, &iter_selected);
        gtk_tree_view_expand_row (GTK_TREE_VIEW (priv->treeview), path_selected, FALSE);
        gtk_tree_path_free (path_selected);
      } else
        /* Insert after the selected entry */
        gtk_tree_store_insert_after (GTK_TREE_STORE (model), &iter_new, NULL, &iter_selected);
      
      path = gtk_tree_model_get_path (model, &iter_new);
      gtk_tree_view_set_cursor (GTK_TREE_VIEW (priv->treeview), path, NULL, FALSE);
      gtk_tree_path_free (path);
    }
    else
      /* Insert at the beginning of the tree */
      gtk_tree_store_prepend (GTK_TREE_STORE (model), &iter_new, NULL);
    
    gtk_tree_store_set (GTK_TREE_STORE (model), &iter_new,
                        COLUMN_ICON, dummy_icon,
                        COLUMN_NAME, name,
                        COLUMN_COMMAND, command,
                        COLUMN_HIDDEN, FALSE,
                        COLUMN_TYPE, entry_type,
                        COLUMN_OPTION_1, option_1, COLUMN_OPTION_2, option_2, COLUMN_OPTION_3, option_3, -1);
    
    menueditor_main_window_set_menu_modified (window);
    
    g_free (name);
    g_free (command);
    g_free (option_1);
    g_free (option_2);
    g_free (option_3);
      
    g_free (selected_src);
  }
  
  gtk_widget_destroy (add_dialog);
}

static void 
action_remove (GtkAction *action, MenuEditorMainWindow *window)
{
  MenuEditorMainWindowPrivate *priv = MENUEDITOR_MAIN_WINDOW_GET_PRIVATE (window);
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->treeview));
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->treeview));
  if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
	GtkAction *action;
	
    gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);

    /* Modified ! */
    menueditor_main_window_set_menu_modified (window);
	
	action = gtk_action_group_get_action (priv->action_group, "edit");
	gtk_action_set_sensitive (action, FALSE);
	action = gtk_action_group_get_action (priv->action_group, "remove");
	gtk_action_set_sensitive (action, FALSE);
	action = gtk_action_group_get_action (priv->action_group, "up");
	gtk_action_set_sensitive (action, FALSE);
	action = gtk_action_group_get_action (priv->action_group, "down");
	gtk_action_set_sensitive (action, FALSE);
  }
}

static void 
action_up (GtkAction *action, MenuEditorMainWindow *window)
{
  MenuEditorMainWindowPrivate *priv = MENUEDITOR_MAIN_WINDOW_GET_PRIVATE (window);
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreePath *path_prev;
  GtkTreeIter iter, iter_prev;
  EntryType type;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->treeview));
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->treeview));

  /* Retrieve current iter */
  if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    return;

  path_prev = gtk_tree_model_get_path (model, &iter);
  if (!gtk_tree_path_prev (path_prev)) {
	gtk_tree_path_free (path_prev);
	return;
  }
  gtk_tree_model_get_iter (model, &iter_prev, path_prev);
  gtk_tree_path_free (path_prev);
   
  gtk_tree_model_get (model, &iter_prev, COLUMN_TYPE, &type, -1);

  if (type == MENU) {
	GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW (window),
												GTK_DIALOG_DESTROY_WITH_PARENT,
												GTK_MESSAGE_QUESTION,
												GTK_BUTTONS_YES_NO,
												_("Do you want to move the item into the submenu?"));
    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_YES) {
	  /* move into the submenu */
	  /* gtk should implement a method to move a element across the tree ! */
	  
	  copy_menuelement_to (priv, &iter, &iter_prev, GTK_TREE_VIEW_DROP_INTO_OR_BEFORE);
	  gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);
	  
	  menueditor_main_window_set_menu_modified (window);
	} else {
	  gtk_tree_store_swap (GTK_TREE_STORE (model), &iter, &iter_prev);
	  menueditor_main_window_set_menu_modified (window);
	}
	
	gtk_widget_destroy (dialog);
  } else {
	gtk_tree_store_swap (GTK_TREE_STORE (model), &iter, &iter_prev);
	menueditor_main_window_set_menu_modified (window);
  }
}

static void 
action_down (GtkAction *action, MenuEditorMainWindow *window)
{
  MenuEditorMainWindowPrivate *priv = MENUEDITOR_MAIN_WINDOW_GET_PRIVATE (window);
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter, iter_next;
  EntryType type;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->treeview));
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->treeview));

  /* Retrieve current iter */
  if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    return;

  iter_next = iter;
  if (!gtk_tree_model_iter_next (model, &iter_next))
	return;
   
  gtk_tree_model_get (model, &iter_next, COLUMN_TYPE, &type, -1);

  if (type == MENU) {
	GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW (window),
												GTK_DIALOG_DESTROY_WITH_PARENT,
												GTK_MESSAGE_QUESTION,
												GTK_BUTTONS_YES_NO,
												_("Do you want to move the item into the submenu?"));
    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_YES) {
	  /* move into the submenu */
	  /* gtk should implement a method to move a element across the tree ! */
	  
	  copy_menuelement_to (priv, &iter, &iter_next, GTK_TREE_VIEW_DROP_INTO_OR_AFTER);
	  gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);
	  
	  menueditor_main_window_set_menu_modified (window);
	} else {
	  gtk_tree_store_swap (GTK_TREE_STORE (model), &iter, &iter_next);
	  menueditor_main_window_set_menu_modified (window);
	}
	
	gtk_widget_destroy (dialog);
  } else {
	gtk_tree_store_swap (GTK_TREE_STORE (model), &iter, &iter_next);
	menueditor_main_window_set_menu_modified (window);
  }
}

static void
action_about (GtkAction *action, MenuEditorMainWindow *window)
{
  XfceAboutInfo *info;
  GtkWidget *dialog;
  GdkPixbuf *icon;

  icon = xfce_themed_icon_load ("xfce4-menueditor", 48);
  
  info = xfce_about_info_new ("Xfce4-MenuEditor", VERSION, _("A menu editor for Xfce4"), 
							  XFCE_COPYRIGHT_TEXT ("2004-2006", "The Xfce Development Team"), XFCE_LICENSE_GPL);
  xfce_about_info_set_homepage (info, "http://www.xfce.org/");

  /* Credits */
  xfce_about_info_add_credit (info, "Jean-François Wauthy", "pollux@xfce.org", _("Author/Maintainer"));
  xfce_about_info_add_credit (info, "Brian Tarricone", "bjt23@cornell.edu", _("Contributor"));
  xfce_about_info_add_credit (info, "Danny Milosavljevic", "danny.milo@gmx.net", _("Contributor"));
  xfce_about_info_add_credit (info, "Jens Luedicke", "perldude@lunar-linux.org", _("Contributor"));
  xfce_about_info_add_credit (info, "Francois Le Clainche", "fleclainche@xfce.org", _("Icon designer"));

  dialog = xfce_about_dialog_new_with_values (GTK_WINDOW (window), info, icon);
  gtk_widget_set_size_request (GTK_WIDGET (dialog), 400, 300);
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  xfce_about_info_free (info);
  if (G_LIKELY (icon != NULL))
    g_object_unref (G_OBJECT (icon));
}

static void 
action_collapse_all (GtkAction *action, MenuEditorMainWindow *window)
{
  MenuEditorMainWindowPrivate *priv = MENUEDITOR_MAIN_WINDOW_GET_PRIVATE (window);
  
  gtk_tree_view_collapse_all (GTK_TREE_VIEW (priv->treeview));
}

static void
action_expand_all (GtkAction *action, MenuEditorMainWindow *window)
{
  MenuEditorMainWindowPrivate *priv = MENUEDITOR_MAIN_WINDOW_GET_PRIVATE (window);
  
  gtk_tree_view_expand_all (GTK_TREE_VIEW (priv->treeview));
}

/* callbacks */
static gboolean
cb_delete_main_window (GtkWidget * widget, GdkEvent * event, MenuEditorMainWindow *window)
{
  MenuEditorMainWindowPrivate *priv = MENUEDITOR_MAIN_WINDOW_GET_PRIVATE (window);
  
  if (priv->menu_modified) {
    gint response = GTK_RESPONSE_NONE;

    response =
      xfce_message_dialog (GTK_WINDOW (window), "Question",
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
        save_treeview_in_file (window);
        break;
      case GTK_RESPONSE_CANCEL:
        return TRUE;
    }
  }

  gtk_main_quit ();

  return FALSE;
}

static void
cb_treeview_cursor_changed (GtkTreeView *treeview, MenuEditorMainWindow *window)
{
  MenuEditorMainWindowPrivate *priv = MENUEDITOR_MAIN_WINDOW_GET_PRIVATE (window);
  GtkAction *action;
  gboolean selection;
  
  selection = gtk_tree_selection_get_selected (gtk_tree_view_get_selection (treeview), NULL, NULL);
  
  action = gtk_action_group_get_action (priv->action_group, "edit");
  gtk_action_set_sensitive (action, selection);
  action = gtk_action_group_get_action (priv->action_group, "add");
  gtk_action_set_sensitive (action, selection);
  action = gtk_action_group_get_action (priv->action_group, "add-external");
  gtk_action_set_sensitive (action, selection);
  action = gtk_action_group_get_action (priv->action_group, "remove");
  gtk_action_set_sensitive (action, selection);
  action = gtk_action_group_get_action (priv->action_group, "up");
  gtk_action_set_sensitive (action, selection);
  action = gtk_action_group_get_action (priv->action_group, "down");
  gtk_action_set_sensitive (action, selection);
}

static void
cb_column_visible_toggled (GtkCellRendererToggle * toggle, gchar * str_path, MenuEditorMainWindow *window)
{
  MenuEditorMainWindowPrivate *priv = MENUEDITOR_MAIN_WINDOW_GET_PRIVATE (window);
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreePath *path;

  /* retrieve current iter */
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->treeview));
  path = gtk_tree_path_new_from_string (str_path);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_path_free (path);

  if (gtk_cell_renderer_toggle_get_active (toggle))
    gtk_tree_store_set (GTK_TREE_STORE (model), &iter, COLUMN_HIDDEN, FALSE, -1);
  else
    gtk_tree_store_set (GTK_TREE_STORE (model), &iter, COLUMN_HIDDEN, TRUE, -1);

  menueditor_main_window_set_menu_modified (window);
}

static gboolean
cb_treeview_button_pressed (GtkTreeView * treeview, GdkEventButton * event, MenuEditorMainWindow *window)
{
  MenuEditorMainWindowPrivate *priv = MENUEDITOR_MAIN_WINDOW_GET_PRIVATE (window);
  gboolean ret = FALSE;
  
  /* Right click draws the context menu */
  if ((event->button == 3) && (event->type == GDK_BUTTON_PRESS)) {
    GtkTreePath *path;

    if (gtk_tree_view_get_path_at_pos (treeview, event->x, event->y, &path, NULL, NULL, NULL)) {
      GtkTreeSelection *selection;
      GtkTreeModel *model;
      GtkTreeIter iter;
      EntryType type;
	  GtkAction *action;
	  GtkWidget *menu_popup;
	  
      selection = gtk_tree_view_get_selection (treeview);
      model = gtk_tree_view_get_model (treeview);

      gtk_tree_selection_unselect_all (selection);
      gtk_tree_selection_select_path (selection, path);
      gtk_tree_model_get_iter (model, &iter, path);
      gtk_tree_model_get (model, &iter, COLUMN_TYPE, &type, -1);

	  action = gtk_action_group_get_action (priv->action_group, "edit");
	  gtk_action_set_sensitive (action, type != SEPARATOR);
      action = gtk_action_group_get_action (priv->action_group, "remove");
	  gtk_action_set_sensitive (action, TRUE);
	  action = gtk_action_group_get_action (priv->action_group, "up");
	  gtk_action_set_sensitive (action, TRUE);
	  action = gtk_action_group_get_action (priv->action_group, "down");
	  gtk_action_set_sensitive (action, TRUE);
  
	  menu_popup = gtk_ui_manager_get_widget (priv->ui_manager, "/main-popup");
      if (G_LIKELY (priv->menubar != NULL)) {
		gtk_widget_show (menu_popup);
		gtk_menu_popup (GTK_MENU (menu_popup), NULL, NULL, NULL, NULL, event->button,
						gtk_get_current_event_time ());
	  }
	  
      ret = TRUE;
    }

    gtk_tree_path_free (path);
  }
  
  return ret;
}

static void
cb_treeview_row_activated (GtkWidget * treeview, GtkTreePath * path, GtkTreeViewColumn * col, MenuEditorMainWindow *window)
{
  action_edit (NULL, window);
}

/* utils */
typedef struct
{
  gboolean started;
  GtkTreeView *treeview;
  GQueue *parents;
}  MenuFileParserState;

static gint
_find_attribute (const gchar ** attribute_names, const gchar * attr)
{
  gint i;

  for (i = 0; attribute_names[i]; i++) {
    if (!strcmp (attribute_names[i], attr))
      return i;
  }

  return -1;
}

static void
menu_file_xml_start (GMarkupParseContext * context, const gchar * element_name,
                     const gchar ** attribute_names, const gchar ** attribute_values,
                     gpointer user_data, GError ** error)
{
  gint i, j, k, l, m;
  GtkTreeIter iter;
  GtkTreeIter *iter_parent;
  GtkTreeStore *treestore;

  gchar *name = NULL;
  gchar *command = NULL;
  gboolean hidden = FALSE;
  GdkPixbuf *icon = NULL;

  MenuFileParserState *state = user_data;

  if (!state->started && !strcmp (element_name, "xfdesktop-menu"))
    state->started = TRUE;
  else if (!state->started)
    return;

  treestore = GTK_TREE_STORE (gtk_tree_view_get_model (state->treeview));

  iter_parent = g_queue_peek_tail (state->parents);
  
  if ((i = _find_attribute (attribute_names, "visible")) != -1 &&
      (!strcmp (attribute_values[i], "false") || !strcmp (attribute_values[i], "no")))
    hidden = TRUE;

  if (!strcmp (element_name, "app")) {
    gboolean in_terminal = FALSE;
    gboolean start_notify = FALSE;

    i = _find_attribute (attribute_names, "name");
    if (i == -1)
      return;
    name = g_markup_printf_escaped (NAME_FORMAT, attribute_values[i]);

    j = _find_attribute (attribute_names, "cmd");
    if (j == -1)
      return;
    command = g_markup_printf_escaped (COMMAND_FORMAT, attribute_values[j]);

    k = _find_attribute (attribute_names, "term");
    l = _find_attribute (attribute_names, "snotify");

    if (k != -1 && (!strcmp (attribute_values[k], "true") || !strcmp (attribute_values[k], "yes")))
      in_terminal = TRUE;

    if (l != -1 && !strcmp (attribute_values[l], "true"))
      start_notify = TRUE;

    m = _find_attribute (attribute_names, "icon");
    if (m != -1 && *attribute_values[m])
      icon = xfce_themed_icon_load (attribute_values[m], ICON_SIZE);

    gtk_tree_store_append (treestore, &iter, iter_parent);
    gtk_tree_store_set (treestore, &iter,
                        COLUMN_ICON, icon ? icon : dummy_icon,
                        COLUMN_NAME, name,
                        COLUMN_COMMAND, command,
                        COLUMN_HIDDEN, hidden,
                        COLUMN_TYPE, APP,
                        COLUMN_OPTION_1, icon ? attribute_values[m] : "",
                        COLUMN_OPTION_2, in_terminal ? "true" : "false",
                        COLUMN_OPTION_3, start_notify ? "true" : "false", -1);
    if (icon)
      g_object_unref (icon);
  }
  else if (!strcmp (element_name, "menu")) {
	GtkTreeIter *parent;
	
    i = _find_attribute (attribute_names, "name");
    if (i == -1)
      return;
    name = g_markup_printf_escaped (MENU_FORMAT, attribute_values[i]);

    j = _find_attribute (attribute_names, "icon");
    if (j != -1 && *attribute_values[j])
      icon = xfce_themed_icon_load (attribute_values[j], ICON_SIZE);

    gtk_tree_store_append (treestore, &iter, iter_parent);
    gtk_tree_store_set (treestore, &iter,
                        COLUMN_ICON, icon ? icon : dummy_icon,
                        COLUMN_NAME, name,
                        COLUMN_COMMAND, "",
                        COLUMN_HIDDEN, hidden, COLUMN_OPTION_1, icon ? attribute_values[j] : "", COLUMN_TYPE, MENU, -1);
    if (icon)
      g_object_unref (icon);

	parent = g_new0 (GtkTreeIter, 1);
	*parent = iter;
    g_queue_push_tail (state->parents, parent);
  }
  else if (!strcmp (element_name, "separator")) {
    name = g_markup_printf_escaped (SEPARATOR_FORMAT, _("--- separator ---"));

    gtk_tree_store_append (treestore, &iter, iter_parent);
    gtk_tree_store_set (treestore, &iter,
                        COLUMN_ICON, dummy_icon, COLUMN_NAME, name, COLUMN_HIDDEN, hidden, COLUMN_TYPE, SEPARATOR, -1);
  }
  else if (!strcmp (element_name, "builtin")) {
    i = _find_attribute (attribute_names, "name");
    if (i == -1)
      return;
    name = g_markup_printf_escaped (NAME_FORMAT, attribute_values[i]);

    j = _find_attribute (attribute_names, "cmd");
    if (j == -1)
      return;
    command = g_markup_printf_escaped (COMMAND_FORMAT, attribute_values[j]);

    k = _find_attribute (attribute_names, "icon");
    if (k != -1 && *attribute_values[k])
      icon = xfce_themed_icon_load (attribute_values[k], ICON_SIZE);

    gtk_tree_store_append (treestore, &iter, iter_parent);
    gtk_tree_store_set (treestore, &iter,
                        COLUMN_ICON, icon ? icon : dummy_icon,
                        COLUMN_NAME, name,
                        COLUMN_COMMAND, command,
                        COLUMN_HIDDEN, hidden,
                        COLUMN_TYPE, BUILTIN,
                        COLUMN_OPTION_1, icon ? attribute_values[k] : "", COLUMN_OPTION_2, "builtin", -1);
    if (icon)
      g_object_unref (icon);
  }
  else if (!strcmp (element_name, "title")) {
    i = _find_attribute (attribute_names, "name");
    if (i == -1)
      return;
    name = g_markup_printf_escaped (TITLE_FORMAT, attribute_values[i]);

    j = _find_attribute (attribute_names, "icon");
    if (j != -1 && *attribute_values[j])
      icon = xfce_themed_icon_load (attribute_values[j], ICON_SIZE);

    gtk_tree_store_append (treestore, &iter, iter_parent);
    gtk_tree_store_set (treestore, &iter,
                        COLUMN_ICON, icon ? icon : dummy_icon,
                        COLUMN_NAME, name, COLUMN_HIDDEN, hidden, COLUMN_TYPE, TITLE, COLUMN_OPTION_1,
                        icon ? attribute_values[j] : "", -1);
    if (icon)
      g_object_unref (icon);
  }
  else if (!strcmp (element_name, "include")) {
    i = _find_attribute (attribute_names, "type");
    if (i == -1)
      return;
    name = g_markup_printf_escaped (INCLUDE_FORMAT, _("--- include ---"));

    if (!strcmp (attribute_values[i], "file")) {
      j = _find_attribute (attribute_names, "src");
      if (j != -1) {
        command = g_markup_printf_escaped (INCLUDE_PATH_FORMAT, attribute_values[j]);

        gtk_tree_store_append (treestore, &iter, iter_parent);
        gtk_tree_store_set (treestore, &iter,
                            COLUMN_ICON, dummy_icon,
                            COLUMN_NAME, name, COLUMN_COMMAND, command, COLUMN_HIDDEN, hidden, COLUMN_TYPE,
                            INCLUDE_FILE, -1);
      }
    }
    else if (!strcmp (attribute_values[i], "system")) {
      gboolean do_legacy = TRUE, only_unique = TRUE;

      command = g_markup_printf_escaped (INCLUDE_FORMAT, _("system"));

      j = _find_attribute (attribute_names, "style");
      k = _find_attribute (attribute_names, "unique");
      l = _find_attribute (attribute_names, "legacy");

      if (k != -1 && !strcmp (attribute_values[k], "false"))
        only_unique = FALSE;
      if (l != -1 && !strcmp (attribute_values[l], "false"))
        do_legacy = FALSE;

      gtk_tree_store_append (treestore, &iter, iter_parent);
      gtk_tree_store_set (treestore, &iter,
                          COLUMN_ICON, dummy_icon,
                          COLUMN_NAME, name,
                          COLUMN_COMMAND, command,
                          COLUMN_HIDDEN, hidden,
                          COLUMN_TYPE, INCLUDE_SYSTEM,
                          COLUMN_OPTION_1, j != -1 ? attribute_values[j] : "",
                          COLUMN_OPTION_2, only_unique ? "true" : "false",
                          COLUMN_OPTION_3, do_legacy ? "true" : "false", -1);
    }
  }

  g_free (name);
  g_free (command);
}

static void
menu_file_xml_end (GMarkupParseContext * context, const gchar * element_name, gpointer user_data, GError ** error)
{
  MenuFileParserState *state = user_data;

  if (!strcmp (element_name, "menu")) {
    GtkTreeIter *parent;

    parent = g_queue_pop_tail (state->parents);
    g_free (parent);
  }
  else if (!strcmp (element_name, "xfdesktop-menu"))
    state->started = FALSE;
}

static gboolean
load_menu_in_treeview (const gchar * filename, MenuEditorMainWindow *win)
{
  MenuEditorMainWindowPrivate *priv = MENUEDITOR_MAIN_WINDOW_GET_PRIVATE (win);
  
  gchar *window_title;
  gchar *file_contents = NULL;
  GMarkupParseContext *gpcontext = NULL;
  struct stat st;
  GMarkupParser gmparser = {
    menu_file_xml_start,
    menu_file_xml_end,
    NULL,
    NULL,
    NULL
  };
  MenuFileParserState state = { };
  gboolean ret = FALSE;
  GError *err = NULL;
#ifdef HAVE_MMAP
  gint fd = -1;
  void *maddr = NULL;
#endif

  GtkAction *action;
  
  g_return_val_if_fail (filename != NULL, FALSE);

  if (stat (filename, &st) < 0) {
    g_warning ("XfceDesktopMenu: unable to find a usable menu file\n");
    goto cleanup;
  }

#ifdef HAVE_MMAP
  fd = open (filename, O_RDONLY, 0);
  if (fd < 0)
    goto cleanup;

  maddr = mmap (NULL, st.st_size, PROT_READ, MAP_FILE | MAP_SHARED, fd, 0);
  if (maddr)
    file_contents = maddr;
#endif

  if (!file_contents && !g_file_get_contents (filename, &file_contents, NULL, &err)) {
    if (err) {
      g_warning ("Unable to read menu file '%s' (%d): %s\n", filename, err->code, err->message);
      g_error_free (err);
    }
    goto cleanup;
  }

  state.started = FALSE;
  state.parents = g_queue_new ();
  state.treeview = GTK_TREE_VIEW (priv->treeview);
  gpcontext = g_markup_parse_context_new (&gmparser, 0, &state, NULL);
  if (!g_markup_parse_context_parse (gpcontext, file_contents, st.st_size, &err)) {
    g_warning ("Error parsing xfdesktop menu file (%d): %s\n", err->code, err->message);
    g_error_free (err);
    goto cleanup;
  }

  if (g_markup_parse_context_end_parse (gpcontext, NULL))
    ret = TRUE;

  /* Activate the widgets */
  gtk_tree_view_expand_all (GTK_TREE_VIEW (priv->treeview));
  gtk_widget_set_sensitive (priv->treeview, TRUE);
  
  action = gtk_action_group_get_action (priv->action_group, "save-menu");
  gtk_action_set_sensitive (action, FALSE);
  action = gtk_action_group_get_action (priv->action_group, "save-menu-as");
  gtk_action_set_sensitive (action, TRUE);
  action = gtk_action_group_get_action (priv->action_group, "close-menu");
  gtk_action_set_sensitive (action, TRUE);
  
  action = gtk_action_group_get_action (priv->action_group, "add");
  gtk_action_set_sensitive (action, TRUE);
  action = gtk_action_group_get_action (priv->action_group, "add-external");
  gtk_action_set_sensitive (action, TRUE);
  action = gtk_action_group_get_action (priv->action_group, "remove");
  gtk_action_set_sensitive (action, FALSE);
  action = gtk_action_group_get_action (priv->action_group, "up");
  gtk_action_set_sensitive (action, FALSE);
  action = gtk_action_group_get_action (priv->action_group, "down");
  gtk_action_set_sensitive (action, FALSE);
  action = gtk_action_group_get_action (priv->action_group, "edit-menu");
  gtk_action_set_sensitive (action, TRUE);
  
  action = gtk_action_group_get_action (priv->action_group, "collapse-all");
  gtk_action_set_sensitive (action, TRUE);
  action = gtk_action_group_get_action (priv->action_group, "expand-all");
  gtk_action_set_sensitive (action, TRUE);
 
  /* set title */
  window_title = g_strdup_printf ("Xfce4-MenuEditor - %s", filename);
  gtk_window_set_title (GTK_WINDOW (win), window_title);
  g_free (window_title);

  if (priv->menu_file_name) {
	g_free (priv->menu_file_name);
	priv->menu_file_name = NULL;
  }
  priv->menu_file_name = g_strdup (filename);
  priv->menu_modified = FALSE;
  
cleanup:
  if (gpcontext)
    g_markup_parse_context_free (gpcontext);
#ifdef HAVE_MMAP
  if (maddr) {
    munmap (maddr, st.st_size);
    file_contents = NULL;
  }
  if (fd > -1)
    close (fd);
#endif
  if (file_contents)
    free (file_contents);
  if (state.parents) {
    g_queue_foreach (state.parents, (GFunc) g_free, NULL);
    g_queue_free (state.parents);
  }
  return ret;
}

/******************************/
/* Save treeview in menu file */
/******************************/
typedef struct
{
  FILE *file_menu;
  gint last_depth;
} MenuFileSaveState;

static gchar *
extract_text_from_markup (const gchar *markup)
{
  gchar **temp_set = NULL;
  gchar **set = NULL;
  gchar *text = NULL;

  if (markup == NULL)
    return text;

  set = g_strsplit_set (markup, "<>", 0);
  temp_set = set;
  while (*temp_set) {
    if (strlen (*temp_set) > 0 && !g_strrstr (*temp_set, "span")) {
      text = *temp_set; /* already escaped */
    }
    temp_set++;
  }
  text = strdup (text ? text : "");
  g_strfreev (set);

  return text;
}

static gboolean
save_treeview_foreach_func (GtkTreeModel * model, GtkTreePath * path, GtkTreeIter * iter, gpointer data)
{
  MenuFileSaveState *state = data;
  gchar *space = NULL;
  gint i;
  gchar *name = NULL;
  gchar *command = NULL;
  gboolean hidden = FALSE;
  gchar *option_1 = NULL;
  gchar *option_2 = NULL;
  gchar *option_3 = NULL;
  EntryType type = SEPARATOR;
  gchar *temp;

  space = g_strnfill (gtk_tree_path_get_depth (path), '\t');

  for (i = state->last_depth; i > gtk_tree_path_get_depth(path); i--) {
    gchar *space2 = NULL;

    space2 = g_strnfill(i - 1,'\t');
    fprintf (state->file_menu, "%s</menu>\n", space2);
    g_free (space2);
  }

  gtk_tree_model_get (model, iter,
                      COLUMN_NAME, &name,
                      COLUMN_COMMAND, &command,
                      COLUMN_HIDDEN, &hidden,
                      COLUMN_OPTION_1, &option_1,
                      COLUMN_OPTION_2, &option_2, COLUMN_OPTION_3, &option_3, COLUMN_TYPE, &type, -1);

  temp = extract_text_from_markup (name);
  g_free (name);
  name = temp;
  temp = extract_text_from_markup (command);
  g_free (command);
  command = temp;

  switch (type) {
  case TITLE:
    fprintf (state->file_menu, "%s<title name=\"%s\"", space, name);
    if (hidden)
      fprintf (state->file_menu, " visible=\"no\"");
    if (option_1 && *option_1)
    {
      temp = g_markup_escape_text (option_1, -1);
      fprintf (state->file_menu, " icon=\"%s\"", temp);
      free (temp);
    }
    fprintf (state->file_menu, "/>\n");
    break;
  case MENU:
    fprintf (state->file_menu, "%s<menu name=\"%s\"", space, name);
    if (hidden)
      fprintf (state->file_menu, " visible=\"no\"");
    if (option_1 && *option_1)
    {
      temp = g_markup_escape_text (option_1, -1);
      fprintf (state->file_menu, " icon=\"%s\"", temp);
      free (temp);
    }
    if (gtk_tree_model_iter_has_child (model, iter))
      fprintf (state->file_menu, ">\n");
    else
      fprintf (state->file_menu, "/>\n");
    break;
  case APP:
    fprintf (state->file_menu, "%s<app name=\"%s\" cmd=\"%s\"", space, name, command);
    if (hidden)
      fprintf (state->file_menu, " visible=\"no\"");
    if (option_1 && *option_1)
    {
      temp = g_markup_escape_text (option_1, -1);
      fprintf (state->file_menu, " icon=\"%s\"", temp);
      free (temp);
    }
    if (option_2 && (strcmp (option_2, "true") == 0))
      fprintf (state->file_menu, " term=\"%s\"", option_2);
    if (option_3 && (strcmp (option_3, "true") == 0))
      fprintf (state->file_menu, " snotify=\"%s\"", option_3);
    fprintf (state->file_menu, "/>\n");
    break;
  case BUILTIN:
    fprintf (state->file_menu, "%s<builtin name=\"%s\" cmd=\"%s\"", space, name, command);
    if (hidden)
      fprintf (state->file_menu, " visible=\"no\"");
    if (option_1 && *option_1)
    {
      temp = g_markup_escape_text (option_1, -1);
      fprintf (state->file_menu, " icon=\"%s\"", temp);
      free (temp);
    }
    fprintf (state->file_menu, "/>\n");
    break;
  case INCLUDE_FILE:
    fprintf (state->file_menu, "%s<include type=\"file\" src=\"%s\"", space, command);
    if (hidden)
      fprintf (state->file_menu, " visible=\"no\"");
    fprintf (state->file_menu, "/>\n");
    break;
  case INCLUDE_SYSTEM:
    fprintf (state->file_menu, "%s<include type=\"system\"", space);
    if (option_1 && *option_1)
    {
      temp = g_markup_escape_text (option_1, -1);
      fprintf (state->file_menu, " style=\"%s\"", temp);
      free (temp);
    }
    if (option_2 && (strcmp (option_2, "true") == 0))
      fprintf (state->file_menu, " unique=\"%s\"", option_2);
    if (option_3 && (strcmp (option_3, "true") == 0))
      fprintf (state->file_menu, " legacy=\"%s\"", option_3);
    if (hidden)
      fprintf (state->file_menu, " visible=\"no\"");
    fprintf (state->file_menu, "/>\n");
    break;
  case SEPARATOR:
    fprintf (state->file_menu, "%s<separator", space);
    if (hidden)
        fprintf (state->file_menu, " visible=\"no\"");
    fprintf (state->file_menu, "/>\n");
    break;
  }

  state->last_depth = gtk_tree_path_get_depth (path);
  g_free (space);
  g_free (name);
  g_free (command);
  g_free (option_1);
  g_free (option_2);
  g_free (option_3);

  return FALSE;
}

static void
save_treeview_in_file (MenuEditorMainWindow *window)
{
  MenuEditorMainWindowPrivate *priv = MENUEDITOR_MAIN_WINDOW_GET_PRIVATE (window);
  FILE *file_menu;
  GtkAction *action;
  
  g_return_if_fail (window != NULL);

  file_menu = fopen (priv->menu_file_name, "w+");

  if (file_menu) {
    MenuFileSaveState state;
    GtkTreeModel *model;

    state.file_menu = file_menu;
    state.last_depth = 0;

    fprintf (file_menu, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf (file_menu, "<xfdesktop-menu>\n");

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->treeview));
    if (model) {
      gint i;

      gtk_tree_model_foreach (model, &save_treeview_foreach_func, &state);
      for (i = state.last_depth; i > 1; i--) {
        gchar *space = NULL;
 
        space = g_strnfill (i - 1,'\t');
        fprintf (file_menu, "%s</menu>\n", space);
	g_free (space);
      }
    }

    fprintf (file_menu, "</xfdesktop-menu>\n");
    
    action = gtk_action_group_get_action (priv->action_group, "save-menu");
    gtk_action_set_sensitive (action, FALSE);
    priv->menu_modified = FALSE;

    fclose (file_menu);
  } else {
    xfce_err (_("Unable to open the menu file %s in write mode"), priv->menu_file_name);
  }
}

/* DnD */
static void
copy_menuelement_to (MenuEditorMainWindowPrivate *priv, GtkTreeIter *src, GtkTreeIter *dest, GtkTreeViewDropPosition position)
{
  GtkTreeModel *model;
  GtkTreeIter iter_new;

  GdkPixbuf *icon = NULL;
  gchar *name = NULL;
  gchar *command = NULL;
  gboolean hidden = FALSE;
  EntryType type = SEPARATOR;
  gchar *option_1 = NULL;
  gchar *option_2 = NULL;
  gchar *option_3 = NULL;

  gint n_children = 0;
  gint i;
  GtkTreePath *path_src = NULL;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->treeview));

  gtk_tree_model_get (model, src, COLUMN_ICON, &icon, COLUMN_NAME, &name,
		      COLUMN_COMMAND, &command, COLUMN_HIDDEN, &hidden,
		      COLUMN_TYPE, &type, COLUMN_OPTION_1, &option_1,
		      COLUMN_OPTION_2, &option_2, COLUMN_OPTION_3, &option_3, -1);

  switch (position) {
  case GTK_TREE_VIEW_DROP_BEFORE:
    gtk_tree_store_insert_before (GTK_TREE_STORE (model), &iter_new, NULL, dest);
    break;
  case GTK_TREE_VIEW_DROP_AFTER:
    gtk_tree_store_insert_after (GTK_TREE_STORE (model), &iter_new, NULL, dest);
    break;
  case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
	gtk_tree_store_append (GTK_TREE_STORE (model), &iter_new, dest);
    break;
  case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
    gtk_tree_store_prepend (GTK_TREE_STORE (model), &iter_new, dest);
    break;
  }

  gtk_tree_store_set (GTK_TREE_STORE (model), &iter_new,
		      COLUMN_ICON, G_IS_OBJECT (icon) ? icon : dummy_icon,
		      COLUMN_NAME, name,
		      COLUMN_COMMAND, command, COLUMN_HIDDEN, hidden,
		      COLUMN_TYPE, type, COLUMN_OPTION_1, option_1,
		      COLUMN_OPTION_2, option_2, COLUMN_OPTION_3, option_3, -1);

  n_children = gtk_tree_model_iter_n_children (model, src);

  for (i = 0; i < n_children; i++) {
    GtkTreeIter iter_child;

    if (gtk_tree_model_iter_nth_child (model, &iter_child, src, i))
      copy_menuelement_to (priv, &iter_child, &iter_new, GTK_TREE_VIEW_DROP_INTO_OR_AFTER);
  }

  path_src = gtk_tree_model_get_path (model, src);
  if (n_children > 0 && gtk_tree_view_row_expanded (GTK_TREE_VIEW (priv->treeview), path_src)) {
    GtkTreePath *path_new = NULL;

    path_new = gtk_tree_model_get_path (model, &iter_new);
    gtk_tree_view_expand_row (GTK_TREE_VIEW (priv->treeview), path_new, FALSE);
    gtk_tree_path_free (path_new);
  }
  gtk_tree_path_free (path_src);

  if (G_IS_OBJECT (icon))
    g_object_unref (icon);
  g_free (name);
  g_free (command);
  g_free (option_1);
  g_free (option_2);
  g_free (option_3);
}

static void
cb_treeview_drag_data_get (GtkWidget * widget, GdkDragContext * dc,
                           GtkSelectionData * data, guint info, guint time, MenuEditorMainWindowPrivate *priv)
{
  GtkTreeRowReference *ref;
  GtkTreePath *path_source;

  if (info == DND_TARGET_MENUEDITOR) {
    ref = g_object_get_data (G_OBJECT (dc), "gtk-tree-view-source-row");
    path_source = gtk_tree_row_reference_get_path (ref);

    gtk_selection_data_set (data, gdk_atom_intern ("MENUEDITOR_ENTRY", FALSE), 8,  /* bits */
			    (gpointer) &path_source, sizeof (path_source));
  }
}

static void
cb_treeview_drag_data_rcv (GtkWidget * widget, GdkDragContext * dc,
                           guint x, guint y, GtkSelectionData * sd, guint info, guint t, MenuEditorMainWindow *window)
{
  MenuEditorMainWindowPrivate *priv = MENUEDITOR_MAIN_WINDOW_GET_PRIVATE (window);
  GtkTreePath *path_where_insert = NULL;
  GtkTreeViewDropPosition position;
  GtkTreeModel *model;

  GdkPixbuf *icon = NULL;
  gchar *name = NULL;
  gchar *command = NULL;
  gboolean hidden = FALSE;
  EntryType type = SEPARATOR;
  gchar *option_1 = NULL;
  gchar *option_2 = NULL;
  gchar *option_3 = NULL;

  GtkTreeIter iter_where_insert;
  GtkTreeIter iter_new;

  /* insertion */
  EntryType type_where_insert;
  gboolean inserted = FALSE;

  g_return_if_fail (sd->data);
  gtk_tree_view_get_dest_row_at_pos (GTK_TREE_VIEW (widget), x, y, &path_where_insert, &position);

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
  if (!model) {
    g_warning ("unable to get the GtkTreeModel");
    goto cleanup;
  }
  
  if (sd->target == gdk_atom_intern ("MENUEDITOR_ENTRY", FALSE)) {
    GtkTreePath *path_source;
    GtkTreeIter iter_source;
    memcpy (&path_source, sd->data, sizeof (path_source));
    
    if (!path_source) {
      g_warning ("wrong path_source");
      goto cleanup;
    }
    
    gtk_tree_model_get_iter (model, &iter_source, path_source);
    gtk_tree_model_get_iter (model, &iter_where_insert, path_where_insert);
    
    if (gtk_tree_path_is_descendant (path_where_insert, path_source) 
        || gtk_tree_path_compare (path_where_insert, path_source) == 0) {
      gtk_tree_path_free (path_source);
      goto cleanup;
    }
    
    gtk_tree_path_free (path_source);
    
    
    gtk_tree_model_get (model, &iter_where_insert, COLUMN_TYPE, &type, -1);

    if ((type == MENU) || (position == GTK_TREE_VIEW_DROP_BEFORE) || (position == GTK_TREE_VIEW_DROP_AFTER)) {
      copy_menuelement_to (priv, &iter_source, &iter_where_insert, position);
      menueditor_main_window_set_menu_modified (window);
      inserted = TRUE;
    }

    goto cleanup;
  } else if (sd->target == gdk_atom_intern ("text/plain", FALSE)) {
    /* text/plain */
    gchar *filename = NULL;
    gchar *temp = NULL;
    gchar *buf = NULL;

    XfceDesktopEntry *de = NULL;
    const char *cat[] = { "Name", "Exec", "Icon" };

    if (g_str_has_prefix ((gchar *) sd->data, "file://"))
      buf = g_build_filename ((gchar *) &(sd->data)[7], NULL);
    else if (g_str_has_prefix ((gchar *) sd->data, "file:"))
      buf = g_build_filename ((gchar *) &(sd->data)[5], NULL);
    else
      buf = g_strdup ((gchar *) sd->data);

    /* Remove \n at the end of filename (if present) */
    temp = strtok (buf, "\n");
    if (!temp)
      filename = g_strdup (buf);
    else if (!g_file_test (temp, G_FILE_TEST_EXISTS))
      filename = g_strndup (temp, strlen (temp) - 1);
    else
      filename = g_strdup (temp);
    g_free (buf);

    de = xfce_desktop_entry_new (filename, cat, 3);
    g_free (filename);

    if (!de) {
      g_warning ("not valid desktop data");
      goto cleanup;
    }

    xfce_desktop_entry_get_string (de, "Name", TRUE, &temp);
    name = g_markup_printf_escaped (NAME_FORMAT, temp);
    g_free (temp);

    xfce_desktop_entry_get_string (de, "Exec", TRUE, &temp);
    command = g_markup_printf_escaped (COMMAND_FORMAT, temp);
    g_free (temp);

    if (xfce_desktop_entry_get_string (de, "Icon", TRUE, &temp)) {
      icon = xfce_themed_icon_load (temp, ICON_SIZE);
      option_1 = g_strdup (temp);
      g_free (temp);
    } else
      option_1 = g_strdup ("");

    type = APP;

    option_2 = g_strdup ("false");
    option_3 = g_strdup ("false");
  } else if (sd->target == gdk_atom_intern ("application/x-desktop", FALSE)) {
    /* application/x-desktop */
    XfceDesktopEntry *de = NULL;
    const char *cat[] = { "Name", "Exec", "Icon" };
    gchar *temp = NULL;

    de = xfce_desktop_entry_new_from_data ((gchar *) sd->data, cat, 3);
    if (!de) {
      g_warning ("not valid desktop data");
      goto cleanup;
    }

    xfce_desktop_entry_get_string (de, "Name", TRUE, &temp);
    name = g_markup_printf_escaped (NAME_FORMAT, temp);
    g_free (temp);

    xfce_desktop_entry_get_string (de, "Exec", TRUE, &temp);
    command = g_markup_printf_escaped (COMMAND_FORMAT, temp);
    g_free (temp);

    if (xfce_desktop_entry_get_string (de, "Icon", TRUE, &temp)) {
      icon = xfce_themed_icon_load (temp, ICON_SIZE);
      option_1 = g_strdup (temp);
    } else
      option_1 = g_strdup ("");
    g_free (temp);

    type = APP;

    option_2 = g_strdup ("false");
    option_3 = g_strdup ("false");
  } else
    goto cleanup;

  /* Insert in the tree */
  gtk_tree_model_get_iter (model, &iter_where_insert, path_where_insert);
  switch (position){
  case GTK_TREE_VIEW_DROP_BEFORE:
    gtk_tree_store_insert_before (GTK_TREE_STORE (model), &iter_new, NULL, &iter_where_insert);
    inserted = TRUE;
    break;
  case GTK_TREE_VIEW_DROP_AFTER:
    gtk_tree_store_insert_after (GTK_TREE_STORE (model), &iter_new, NULL, &iter_where_insert);
    inserted = TRUE;
    break;
  case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
  case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
    gtk_tree_model_get (model, &iter_where_insert, COLUMN_TYPE, &type_where_insert, -1);
    if (type_where_insert == MENU) {
      gtk_tree_store_prepend (GTK_TREE_STORE (model), &iter_new, &iter_where_insert);
      inserted = TRUE;
    }
    break;
  }

  if (inserted) {
    gtk_tree_store_set (GTK_TREE_STORE (model), &iter_new,
			COLUMN_ICON, G_IS_OBJECT (icon) ? icon : dummy_icon,
			COLUMN_NAME, name,
			COLUMN_COMMAND, command, COLUMN_HIDDEN, hidden,
			COLUMN_TYPE, type, COLUMN_OPTION_1, option_1,
			COLUMN_OPTION_2, option_2, COLUMN_OPTION_3, option_3, -1);

    menueditor_main_window_set_menu_modified (window);
  }

 cleanup:
  gtk_drag_finish (dc, inserted, (dc->action == GDK_ACTION_MOVE), t);
  if (G_IS_OBJECT (icon))
    g_object_unref (icon);
  g_free (name);
  g_free (command);
  g_free (option_1);
  g_free (option_2);
  g_free (option_3);

  gtk_tree_path_free (path_where_insert);
}

/******************/
/* public methods */
/******************/
GtkWidget *
menueditor_main_window_new (void)
{
  GtkWidget *obj = NULL;
  
  obj = g_object_new (menueditor_main_window_get_type (), NULL);
  
  if (obj)
    action_open_default_menu (NULL, MENUEDITOR_MAIN_WINDOW (obj));
  
  return obj;
}

GtkWidget *
menueditor_main_window_new_with_menufile (const gchar *menufile)
{
  GtkWidget *obj = NULL;

  g_return_val_if_fail (menufile != NULL, NULL);

  obj = g_object_new (menueditor_main_window_get_type (), NULL);

  if (obj)
    load_menu_in_treeview (menufile, MENUEDITOR_MAIN_WINDOW (obj));

  return obj;
}

void
menueditor_main_window_set_menu_modified (MenuEditorMainWindow *win)
{
  MenuEditorMainWindowPrivate *priv = MENUEDITOR_MAIN_WINDOW_GET_PRIVATE (win);
  GtkAction *action;
  
  priv->menu_modified = TRUE;
  
  action = gtk_action_group_get_action (priv->action_group, "save-menu");
  gtk_action_set_sensitive (action, TRUE);
}
