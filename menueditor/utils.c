/*   utils.c */

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

#include "menueditor.h"

#include "utils.h"

/******************************/
/* Save treeview in menu file */
/******************************/
struct MenuFileSaveState
{
  FILE *file_menu;
  gint last_depth;
};

static gboolean
save_treeview_foreach_func (GtkTreeModel * model, GtkTreePath * path, GtkTreeIter * iter, gpointer data)
{
  struct MenuFileSaveState *state = data;
  gchar *space = NULL;
  gchar *name = NULL;
  gchar *command = NULL;
  gboolean hidden = FALSE;
  gchar *option_1 = NULL;
  gchar *option_2 = NULL;
  gchar *option_3 = NULL;
  ENTRY_TYPE type = SEPARATOR;
  gchar *temp;

  space = g_strnfill (gtk_tree_path_get_depth (path), '\t');

  if (state->last_depth > gtk_tree_path_get_depth (path)) {
    fprintf (state->file_menu, "%s</menu>\n", space);
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
  temp = extract_text_from_markup (option_1);
  g_free (option_1);
  option_1 = temp;
  temp = extract_text_from_markup (option_2);
  g_free (option_2);
  option_2 = temp;
  temp = extract_text_from_markup (option_3);
  g_free (option_3);
  option_3 = temp;


  switch (type) {
  case TITLE:
    fprintf (state->file_menu, "%s<title name=\"%s\"", space, name);
    if (hidden)
      fprintf (state->file_menu, " visible=\"no\"");
    if (option_1 && strlen (option_1) > 0)
      fprintf (state->file_menu, " icon=\"%s\"", option_1);

    fprintf (state->file_menu, "/>\n");
    break;
  case MENU:
    fprintf (state->file_menu, "%s<menu name=\"%s\"", space, name);
    if (hidden)
      fprintf (state->file_menu, " visible=\"no\"");
    if (option_1 && strlen (option_1) > 0)
      fprintf (state->file_menu, " icon=\"%s\"", option_1);
    fprintf (state->file_menu, ">\n");
    break;
  case APP:
    fprintf (state->file_menu, "%s<app name=\"%s\" cmd=\"%s\"", space, name, command);
    if (hidden)
      fprintf (state->file_menu, " visible=\"no\"");
    if (option_1 && strlen (option_1) > 0)
      fprintf (state->file_menu, " icon=\"%s\"", option_1);
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
    if (strlen (option_1) > 0)
      fprintf (state->file_menu, " icon=\"%s\"", option_1);

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
    if (option_1 && strlen (option_1) > 0)
      fprintf (state->file_menu, " style=\"%s\"", option_1);
    if (option_2 && (strcmp (option_2, "true") == 0))
      fprintf (state->file_menu, " unique=\"%s\"", option_2);
    if (option_3 && (strcmp (option_3, "true") == 0))
      fprintf (state->file_menu, " legacy=\"%s\"", option_3);
    if (hidden)
      fprintf (state->file_menu, " visible=\"no\"");
    fprintf (state->file_menu, "/>\n");
    break;
  case SEPARATOR:
    fprintf (state->file_menu, "%s<separator/>\n", space);
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

gboolean
save_treeview_in_file (MenuEditor * me)
{
  FILE *file_menu;

  file_menu = fopen (me->menu_file_name, "w+");

  g_return_val_if_fail (me != NULL, FALSE);

  if (file_menu) {
    struct MenuFileSaveState state;
    GtkTreeModel *model;

    state.file_menu = file_menu;
    state.last_depth = 0;

    fprintf (file_menu, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf (file_menu, "<xfdesktop-menu>\n");

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (me->treeview));
    if (model)
      gtk_tree_model_foreach (model, &save_treeview_foreach_func, &state);

    fprintf (file_menu, "</xfdesktop-menu>\n");
    fclose (file_menu);
  }

  gtk_widget_set_sensitive (me->menu_item_file_save, FALSE);
  gtk_widget_set_sensitive (me->toolbar_save, FALSE);
  me->menu_modified = FALSE;


  return TRUE;
}

/******************************/
/* Load menu file in treeview */
/******************************/
struct MenuFileParserState
{
  gboolean started;
  GtkTreeView *treeview;
  XfceIconTheme *icontheme;
  GQueue *parents;
  gchar *cur_parent;
};

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
  GtkTreeIter iter, iter_temp;
  GtkTreeIter *iter_parent;
  GtkTreeStore *treestore;

  gchar *name = NULL;
  gchar *command = NULL;
  gboolean hidden = FALSE;
  GdkPixbuf *icon = NULL;

  GtkTreePath *path = NULL;
  struct MenuFileParserState *state = user_data;

  if (!state->started && !strcmp (element_name, "xfdesktop-menu"))
    state->started = TRUE;
  else if (!state->started)
    return;

  treestore = GTK_TREE_STORE (gtk_tree_view_get_model (state->treeview));

  iter_parent = NULL;
  if (state->cur_parent) {
    path = gtk_tree_path_new_from_string (state->cur_parent);
    if (path && gtk_tree_model_get_iter (GTK_TREE_MODEL (treestore), &iter_temp, path)) {
      iter_parent = &iter_temp;
      gtk_tree_path_free (path);
    }
  }

  if ((i = _find_attribute (attribute_names, "visible")) != -1 &&
      (!strcmp (attribute_values[i], "false") || !strcmp (attribute_values[i], "no")))
    hidden = TRUE;

  if (!strcmp (element_name, "app")) {
    gboolean in_terminal = FALSE;
    gboolean start_notify = FALSE;

    i = _find_attribute (attribute_names, "name");
    if (i == -1)
      return;
    name = g_strdup_printf (NAME_FORMAT, attribute_values[i]);

    j = _find_attribute (attribute_names, "cmd");
    if (j == -1)
      return;
    command = g_strdup_printf (COMMAND_FORMAT, attribute_values[j]);

    k = _find_attribute (attribute_names, "term");
    l = _find_attribute (attribute_names, "snotify");

    if (k != -1 && (!strcmp (attribute_values[k], "true") || !strcmp (attribute_values[k], "yes")))
      in_terminal = TRUE;

    if (l != -1 && !strcmp (attribute_values[l], "true"))
      start_notify = TRUE;

    m = _find_attribute (attribute_names, "icon");
    if (m != -1 && *attribute_values[m])
      icon = xfce_icon_theme_load (state->icontheme, attribute_values[m], ICON_SIZE);

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
    GtkTreePath *path_current;

    i = _find_attribute (attribute_names, "name");
    if (i == -1)
      return;
    name = g_strdup_printf (MENU_FORMAT, attribute_values[i]);

    j = _find_attribute (attribute_names, "icon");
    if (j != -1 && *attribute_values[j])
      icon = xfce_icon_theme_load (state->icontheme, attribute_values[j], ICON_SIZE);

    gtk_tree_store_append (treestore, &iter, iter_parent);
    gtk_tree_store_set (treestore, &iter,
                        COLUMN_ICON, icon ? icon : dummy_icon,
                        COLUMN_NAME, name,
                        COLUMN_COMMAND, "",
                        COLUMN_HIDDEN, hidden, COLUMN_OPTION_1, icon ? attribute_values[j] : "", COLUMN_TYPE, MENU, -1);
    if (icon)
      g_object_unref (icon);

    path_current = gtk_tree_model_get_path (GTK_TREE_MODEL (treestore), &iter);
    state->cur_parent = gtk_tree_path_to_string (path_current);
    gtk_tree_path_free (path_current);
    g_queue_push_tail (state->parents, state->cur_parent);
  }
  else if (!strcmp (element_name, "separator")) {
    name = g_strdup_printf (SEPARATOR_FORMAT, _("--- separator ---"));

    gtk_tree_store_append (treestore, &iter, iter_parent);
    gtk_tree_store_set (treestore, &iter,
                        COLUMN_ICON, dummy_icon, COLUMN_NAME, name, COLUMN_HIDDEN, hidden, COLUMN_TYPE, SEPARATOR, -1);
  }
  else if (!strcmp (element_name, "builtin")) {
    i = _find_attribute (attribute_names, "name");
    if (i == -1)
      return;
    name = g_strdup_printf (NAME_FORMAT, attribute_values[i]);

    j = _find_attribute (attribute_names, "cmd");
    if (j == -1)
      return;
    command = g_strdup_printf (COMMAND_FORMAT, attribute_values[j]);

    k = _find_attribute (attribute_names, "icon");
    if (k != -1 && *attribute_values[k])
      icon = xfce_icon_theme_load (state->icontheme, attribute_values[k], ICON_SIZE);

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
    name = g_strdup_printf (TITLE_FORMAT, attribute_values[i]);

    j = _find_attribute (attribute_names, "icon");
    if (j != -1 && *attribute_values[j])
      icon = xfce_icon_theme_load (state->icontheme, attribute_values[j], ICON_SIZE);

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
    name = g_strdup_printf (INCLUDE_FORMAT, _("--- include ---"));

    if (!strcmp (attribute_values[i], "file")) {
      j = _find_attribute (attribute_names, "src");
      if (j != -1) {
        command = g_strdup_printf (INCLUDE_PATH_FORMAT, attribute_values[j]);

        gtk_tree_store_append (treestore, &iter, iter_parent);
        gtk_tree_store_set (treestore, &iter,
                            COLUMN_ICON, dummy_icon,
                            COLUMN_NAME, name, COLUMN_COMMAND, command, COLUMN_HIDDEN, hidden, COLUMN_TYPE,
                            INCLUDE_FILE, -1);
      }
    }
    else if (!strcmp (attribute_values[i], "system")) {
      gboolean do_legacy = TRUE, only_unique = TRUE;

      command = g_strdup_printf (INCLUDE_FORMAT, _("system"));

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
  struct MenuFileParserState *state = user_data;

  if (!strcmp (element_name, "menu")) {
    gchar *parent;

    parent = g_queue_pop_tail (state->parents);
    g_free (parent);
    state->cur_parent = g_queue_peek_tail (state->parents);
  }
  else if (!strcmp (element_name, "xfdesktop-menu"))
    state->started = FALSE;
}

gboolean
load_menu_in_treeview (const gchar * filename, MenuEditor * me)
{
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
  struct MenuFileParserState state = { FALSE, NULL, NULL, NULL, NULL };
  gboolean ret = FALSE;
  GError *err = NULL;
#ifdef HAVE_MMAP
  gint fd = -1;
  void *maddr = NULL;
#endif

  g_return_val_if_fail (me != NULL && filename != NULL, FALSE);

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
  state.treeview = GTK_TREE_VIEW (me->treeview);
  state.icontheme = me->icon_theme;

  gpcontext = g_markup_parse_context_new (&gmparser, 0, &state, NULL);

  if (!g_markup_parse_context_parse (gpcontext, file_contents, st.st_size, &err)) {
    g_warning ("Error parsing xfdesktop menu file (%d): %s\n", err->code, err->message);
    g_error_free (err);
    goto cleanup;
  }

  if (g_markup_parse_context_end_parse (gpcontext, NULL))
    ret = TRUE;

  /* Activate the widgets */
  gtk_tree_view_expand_all (GTK_TREE_VIEW (me->treeview));


  gtk_widget_set_sensitive (me->menu_item_edit, TRUE);
  gtk_widget_set_sensitive (me->menu_item_edit_add, TRUE);
  gtk_widget_set_sensitive (me->menu_item_edit_add_menu, TRUE);
  gtk_widget_set_sensitive (me->menu_item_edit_del, FALSE);
  gtk_widget_set_sensitive (me->menu_item_edit_up, FALSE);
  gtk_widget_set_sensitive (me->menu_item_edit_down, FALSE);


  gtk_widget_set_sensitive (me->menu_item_file_saveas, TRUE);
  gtk_widget_set_sensitive (me->menu_item_file_close, TRUE);
  gtk_widget_set_sensitive (me->treeview, TRUE);

  gtk_widget_set_sensitive (me->toolbar_add, TRUE);
  gtk_widget_set_sensitive (me->toolbar_del, FALSE);
  gtk_widget_set_sensitive (me->toolbar_close, TRUE);
  gtk_widget_set_sensitive (me->toolbar_up, FALSE);
  gtk_widget_set_sensitive (me->toolbar_down, FALSE);
  gtk_widget_set_sensitive (me->toolbar_expand, TRUE);
  gtk_widget_set_sensitive (me->toolbar_collapse, TRUE);

  me->menu_file_name = g_strdup (filename);
  me->menu_modified = FALSE;

  /* Set window's title */
  window_title = g_strdup_printf ("Xfce4-MenuEditor - %s", filename);
  gtk_window_set_title (GTK_WINDOW (me->window), window_title);
  g_free (window_title);

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

/*******************************/
/* Check if the command exists */
/*******************************/
gboolean
command_exists (const gchar * command)
{
  gchar *cmd_buf = NULL;
  gchar *cmd_tok = NULL;
  gchar *program = NULL;
  gboolean result = FALSE;

  cmd_buf = g_strdup (command);
  cmd_tok = (gchar *) strtok (cmd_buf, " ");

  program = g_find_program_in_path (cmd_buf);

  if (program)
    result = TRUE;

  g_free (program);
  g_free (cmd_buf);

  return result;
}

/*****************************************/
/* browse for a file and set it in entry */
/*****************************************/
void
browse_file (GtkEntry * entry, GtkWindow * parent)
{
  GtkWidget *filesel_dialog;
  const gchar *text;

  text = gtk_entry_get_text (entry);

  filesel_dialog =
    xfce_file_chooser_new (_("Select command"),
                           parent,
                           XFCE_FILE_CHOOSER_ACTION_OPEN,
                           GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);

  if (strlen (text) != 0) {
    gchar *cmd_buf = NULL;
    gchar *cmd_tok = NULL;
    gchar *programpath = NULL;

    cmd_buf = g_strdup (text);
    cmd_tok = strtok (cmd_buf, " ");
    programpath = g_find_program_in_path (cmd_buf);
    xfce_file_chooser_set_filename (XFCE_FILE_CHOOSER (filesel_dialog), programpath);

    g_free (cmd_buf);
    g_free (programpath);
  }

  if (gtk_dialog_run (GTK_DIALOG (filesel_dialog)) == GTK_RESPONSE_ACCEPT) {
    gchar *filename = NULL;

    filename = xfce_file_chooser_get_filename (XFCE_FILE_CHOOSER (filesel_dialog));
    gtk_entry_set_text (entry, filename);
    g_free (filename);
  }

  gtk_widget_destroy (filesel_dialog);
}

/*****************************************/
/* browse for a icon and set it in entry */
/*****************************************/
static void
browse_icon_update_preview_cb (XfceFileChooser * chooser, gpointer data)
{
  GtkImage *preview;
  char *filename;
  GdkPixbuf *pix = NULL;

  preview = GTK_IMAGE (data);
  filename = xfce_file_chooser_get_filename (chooser);

  if (g_file_test (filename, G_FILE_TEST_IS_REGULAR))
    pix = xfce_pixbuf_new_from_file_at_size (filename, 250, 250, NULL);
  g_free (filename);

  if (pix) {
    gtk_image_set_from_pixbuf (preview, pix);
    g_object_unref (G_OBJECT (pix));
  }
  xfce_file_chooser_set_preview_widget_active (chooser, (pix != NULL));
}

void
browse_icon (GtkEntry * entry, GtkWindow * parent, XfceIconTheme * icon_theme)
{
  GtkWidget *filesel_dialog, *preview;
  XfceFileFilter *filter;
  const gchar *text;

  filesel_dialog =
    xfce_file_chooser_new (_("Select icon"), parent,
                           XFCE_FILE_CHOOSER_ACTION_OPEN,
                           GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);

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
  xfce_file_chooser_set_preview_callback (XFCE_FILE_CHOOSER (filesel_dialog), (PreviewUpdateFunc)
                                          browse_icon_update_preview_cb, preview);

  text = gtk_entry_get_text (entry);
  if (strlen (text) != 0) {
    gchar *iconpath = NULL;

    iconpath = xfce_icon_theme_lookup (icon_theme, text, ICON_SIZE);
    xfce_file_chooser_set_filename (XFCE_FILE_CHOOSER (filesel_dialog), iconpath);

    g_free (iconpath);
  }

  if (gtk_dialog_run (GTK_DIALOG (filesel_dialog)) == GTK_RESPONSE_ACCEPT) {
    gchar *filename = NULL;

    filename = xfce_file_chooser_get_filename (XFCE_FILE_CHOOSER (filesel_dialog));
    gtk_entry_set_text (entry, filename);
    g_free (filename);
  }

  gtk_widget_destroy (filesel_dialog);
}

/******************************************/
/* Workaround for gtk_tree_store_swap bug */
/******************************************/
void
menueditor_tree_store_swap_down (GtkTreeStore * tree_store, GtkTreeIter * a, GtkTreeIter * b, gpointer data)
{
  MenuEditor *me;
  GtkTreeIter iter_new;
  GtkTreeModel *model = GTK_TREE_MODEL (tree_store);

  GdkPixbuf *icon = NULL;
  gchar *name = NULL;
  gchar *command = NULL;
  gboolean hidden = FALSE;
  ENTRY_TYPE type = SEPARATOR;
  gchar *option_1 = NULL;
  gchar *option_2 = NULL;
  gchar *option_3 = NULL;

  GtkTreePath *path = NULL;

  me = (MenuEditor *) data;

  gtk_tree_model_get (model, a, COLUMN_ICON, &icon, COLUMN_NAME, &name,
                      COLUMN_COMMAND, &command, COLUMN_HIDDEN, &hidden,
                      COLUMN_TYPE, &type, COLUMN_OPTION_1, &option_1,
                      COLUMN_OPTION_2, &option_2, COLUMN_OPTION_3, &option_3, -1);

  gtk_tree_store_insert_after (tree_store, &iter_new, NULL, b);
  gtk_tree_store_set (tree_store, &iter_new,
                      COLUMN_ICON, icon, COLUMN_NAME, name,
                      COLUMN_COMMAND, command, COLUMN_HIDDEN, hidden,
                      COLUMN_TYPE, type, COLUMN_OPTION_1, option_1,
                      COLUMN_OPTION_2, option_2, COLUMN_OPTION_3, option_3, -1);

  /* Remove a */
  gtk_tree_store_remove (tree_store, a);

  /* a is now iter_new */
  *a = iter_new;
  path = gtk_tree_model_get_path (model, a);
  gtk_tree_view_set_cursor (GTK_TREE_VIEW (me->treeview), path, NULL, FALSE);

  if (G_IS_OBJECT (icon))
    g_object_unref (icon);

  g_free (name);
  g_free (command);
  g_free (option_1);
  g_free (option_2);
  g_free (option_3);

  gtk_tree_path_free (path);
}

void
menueditor_tree_store_swap_up (GtkTreeStore * tree_store, GtkTreeIter * a, GtkTreeIter * b, gpointer data)
{
  MenuEditor *me;
  GtkTreeIter iter_new;
  GtkTreeModel *model = GTK_TREE_MODEL (tree_store);

  GdkPixbuf *icon = NULL;
  gchar *name = NULL;
  gchar *command = NULL;
  gboolean hidden = FALSE;
  ENTRY_TYPE type = SEPARATOR;
  gchar *option_1 = NULL;
  gchar *option_2 = NULL;
  gchar *option_3 = NULL;

  GtkTreePath *path = NULL;

  me = (MenuEditor *) data;

  gtk_tree_model_get (model, a, COLUMN_ICON, &icon, COLUMN_NAME, &name,
                      COLUMN_COMMAND, &command, COLUMN_HIDDEN, &hidden,
                      COLUMN_TYPE, &type, COLUMN_OPTION_1, &option_1,
                      COLUMN_OPTION_2, &option_2, COLUMN_OPTION_3, &option_3, -1);

  gtk_tree_store_insert_before (tree_store, &iter_new, NULL, b);
  gtk_tree_store_set (tree_store, &iter_new,
                      COLUMN_ICON, icon, COLUMN_NAME, name,
                      COLUMN_COMMAND, command, COLUMN_HIDDEN, hidden,
                      COLUMN_TYPE, type, COLUMN_OPTION_1, option_1,
                      COLUMN_OPTION_2, option_2, COLUMN_OPTION_3, option_3, -1);

  /* Remove a */
  gtk_tree_store_remove (tree_store, a);

  /* a is now iter_new */
  *a = iter_new;
  path = gtk_tree_model_get_path (model, a);
  gtk_tree_view_set_cursor (GTK_TREE_VIEW (me->treeview), path, NULL, FALSE);

  if (G_IS_OBJECT (icon))
    g_object_unref (icon);

  g_free (name);
  g_free (command);
  g_free (option_1);
  g_free (option_2);
  g_free (option_3);

  gtk_tree_path_free (path);
}

/**************************/
/* Menu has been modified */
/**************************/
inline void
menueditor_menu_modified (MenuEditor * me)
{
  me->menu_modified = TRUE;
  gtk_widget_set_sensitive (me->menu_item_file_save, TRUE);
  gtk_widget_set_sensitive (me->toolbar_save, TRUE);
}

/****************************/
/* Extract text from markup */
/****************************/
gchar *
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
      text = g_markup_escape_text (*temp_set, strlen (*temp_set) * sizeof (gchar));
    }
    *temp_set++;
  }
  g_strfreev (set);

  return text;
}
