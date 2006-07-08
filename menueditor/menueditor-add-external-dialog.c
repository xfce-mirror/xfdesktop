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

#include <gtk/gtk.h>
#include <libxfcegui4/libxfcegui4.h>

#include "menueditor-add-external-dialog.h"

#define MENUEDITOR_ADD_EXTERNAL_DIALOG_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), MENUEDITOR_TYPE_ADD_EXTERNAL_DIALOG, MenuEditorAddExternalDialogPrivate))

/* private struct */
typedef struct {
  GtkWidget *combo_type;
  GtkWidget *label_src;
  GtkWidget *chooser_src;
  GtkWidget *label_style;
  GtkWidget *combo_style;
  GtkWidget *check_button_unique;
} MenuEditorAddExternalDialogPrivate;

/* prototypes */
static void menueditor_add_external_dialog_class_init (MenuEditorAddExternalDialogClass *);
static void menueditor_add_external_dialog_init (MenuEditorAddExternalDialog *);

static void cb_combo_type_changed (GtkComboBox *, MenuEditorAddExternalDialog *);

/* globals */
static XfceTitledDialogClass *parent_class = NULL;

/******************************/
/* MenuEditorAddExternalDialog class */
/******************************/
GtkType
menueditor_add_external_dialog_get_type (void)
{
  static GtkType add_external_dialog_type = 0;

  if (!add_external_dialog_type) {
    static const GTypeInfo add_external_dialog_info = {
      sizeof (MenuEditorAddExternalDialogClass),
      NULL,
      NULL,
      (GClassInitFunc) menueditor_add_external_dialog_class_init,
      NULL,
      NULL,
      sizeof (MenuEditorAddExternalDialog),
      0,
      (GInstanceInitFunc) menueditor_add_external_dialog_init
    };

    add_external_dialog_type = g_type_register_static (XFCE_TYPE_TITLED_DIALOG, "MenuEditorAddExternalDialog", &add_external_dialog_info, 0);
  }

  return add_external_dialog_type;
}

static void
menueditor_add_external_dialog_class_init (MenuEditorAddExternalDialogClass * klass)
{  
  g_type_class_add_private (klass, sizeof (MenuEditorAddExternalDialogPrivate));

  parent_class = g_type_class_peek_parent (klass);
}


static void
menueditor_add_external_dialog_init (MenuEditorAddExternalDialog * dialog)
{
  MenuEditorAddExternalDialogPrivate *priv = MENUEDITOR_ADD_EXTERNAL_DIALOG_GET_PRIVATE (dialog);

  GtkWidget *table;
  GtkWidget *label;
  gchar *label_text = NULL;
  GtkFileFilter *filter;
  
  gtk_window_set_title (GTK_WINDOW (dialog), _("Add external menu entry"));
  gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_icon_name (GTK_WINDOW (dialog), GTK_STOCK_ADD);
  
  /* table */
  table = gtk_table_new (4, 2, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 5);
  gtk_table_set_col_spacings (GTK_TABLE (table), 5);
  gtk_container_set_border_width (GTK_CONTAINER (table), 10);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), table, TRUE, TRUE, 0);
  gtk_widget_show (table);
  
  /* Type */
  label = gtk_label_new (_("Type:"));
  label_text = g_strdup_printf ("<span weight='bold'>%s</span>", _("Type:"));
  gtk_label_set_markup (GTK_LABEL (label), label_text);
  g_free (label_text);
  gtk_misc_set_alignment (GTK_MISC (label), 1.0f, 0.5f);
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
	
  priv->combo_type = gtk_combo_box_new_text ();
  gtk_combo_box_append_text (GTK_COMBO_BOX (priv->combo_type), _("File"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (priv->combo_type), _("System"));
  g_signal_connect (G_OBJECT (priv->combo_type), "changed", G_CALLBACK (cb_combo_type_changed), dialog);
  gtk_widget_show (priv->combo_type);
  gtk_table_attach (GTK_TABLE (table), priv->combo_type, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

  /* Source */
  priv->label_src = gtk_label_new (_("Source:"));
  label_text = g_strdup_printf ("<span weight='bold'>%s</span>", _("Source:"));
  gtk_label_set_markup (GTK_LABEL (priv->label_src), label_text);
  g_free (label_text);
  gtk_misc_set_alignment (GTK_MISC (priv->label_src), 1.0f, 0.5f);
  gtk_widget_show (priv->label_src);
  gtk_table_attach (GTK_TABLE (table), priv->label_src, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
  
  priv->chooser_src = gtk_file_chooser_button_new (_("Select external menu"), GTK_FILE_CHOOSER_ACTION_OPEN);
  gtk_widget_show (priv->chooser_src);
  gtk_table_attach (GTK_TABLE (table), priv->chooser_src, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 6);
  
  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("All Files"));
  gtk_file_filter_add_pattern (filter, "*");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(priv->chooser_src), filter);
  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Menu Files"));
  gtk_file_filter_add_pattern (filter, "*.xml");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (priv->chooser_src), filter);
  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (priv->chooser_src), filter);
  
  gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (priv->chooser_src), TRUE);  
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (priv->chooser_src), xfce_get_homedir ());
  
  /* Style */
  priv->label_style = gtk_label_new (_("Style:"));
  label_text = g_strdup_printf ("<span weight='bold'>%s</span>", _("Style:"));
  gtk_label_set_markup (GTK_LABEL (priv->label_style), label_text);
  g_free (label_text);
  gtk_misc_set_alignment (GTK_MISC (priv->label_style), 1.0f, 0.5f);
  gtk_widget_show (priv->label_style);
  gtk_table_attach (GTK_TABLE (table), priv->label_style, 0, 1, 2, 3, GTK_FILL, GTK_FILL, 0, 0);
  
  priv->combo_style = gtk_combo_box_new_text ();
  gtk_widget_show (priv->combo_style);
  gtk_combo_box_append_text (GTK_COMBO_BOX (priv->combo_style), _("Simple"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (priv->combo_style), _("Multilevel"));
  gtk_combo_box_set_active (GTK_COMBO_BOX (priv->combo_style), 0);
  gtk_table_attach (GTK_TABLE (table), priv->combo_style, 1, 2, 2, 3, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 6);
  
  /* Unique check button */
  priv->check_button_unique = gtk_check_button_new_with_mnemonic (_("_Unique entries only"));
  gtk_widget_show (priv->check_button_unique);
  gtk_table_attach (GTK_TABLE (table), priv->check_button_unique, 1, 2, 3, 4, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

  gtk_dialog_add_buttons (GTK_DIALOG (dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
  
  gtk_combo_box_set_active (GTK_COMBO_BOX (priv->combo_type), 0);
}

/*************/
/* internals */
/*************/
static void
cb_combo_type_changed (GtkComboBox * combo, MenuEditorAddExternalDialog * dialog)
{
  MenuEditorAddExternalDialogPrivate *priv = MENUEDITOR_ADD_EXTERNAL_DIALOG_GET_PRIVATE (dialog);
  
  switch (gtk_combo_box_get_active (GTK_COMBO_BOX (priv->combo_type))) {
	case 0:
	  gtk_widget_set_sensitive (priv->label_src, TRUE);
	  gtk_widget_set_sensitive (priv->chooser_src, TRUE);
	  gtk_widget_set_sensitive (priv->label_style, FALSE);
	  gtk_widget_set_sensitive (priv->combo_style, FALSE);
	  gtk_widget_set_sensitive (priv->check_button_unique, FALSE);
	  break;
	case 1:
      gtk_widget_set_sensitive (priv->label_src, FALSE);
	  gtk_widget_set_sensitive (priv->chooser_src, FALSE);
	  gtk_widget_set_sensitive (priv->label_style, TRUE);
	  gtk_widget_set_sensitive (priv->combo_style, TRUE);
	  gtk_widget_set_sensitive (priv->check_button_unique, TRUE);
	  break;
  }
}

/******************/
/* public methods */
/******************/
GtkWidget *
menueditor_add_external_dialog_new (GtkWindow *parent)
{
  GtkWidget *obj = NULL;
  MenuEditorAddExternalDialogPrivate *priv;
  
  obj = g_object_new (menueditor_add_external_dialog_get_type (), NULL);
  priv = MENUEDITOR_ADD_EXTERNAL_DIALOG_GET_PRIVATE (obj);
  
  if (parent)
	gtk_window_set_transient_for (GTK_WINDOW (obj), parent);
	
  return obj;
}

EntryType
menueditor_add_external_dialog_get_entry_type (MenuEditorAddExternalDialog *dialog)
{
  MenuEditorAddExternalDialogPrivate *priv = MENUEDITOR_ADD_EXTERNAL_DIALOG_GET_PRIVATE (dialog);
  
  return (gtk_combo_box_get_active (GTK_COMBO_BOX (priv->combo_type)) + INCLUDE_FILE);
}

gchar *
menueditor_add_external_dialog_get_entry_source (MenuEditorAddExternalDialog *dialog)
{
  MenuEditorAddExternalDialogPrivate *priv = MENUEDITOR_ADD_EXTERNAL_DIALOG_GET_PRIVATE (dialog);
  
  return gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (priv->chooser_src));
}

ExternalEntryStyle
menueditor_add_external_dialog_get_entry_style (MenuEditorAddExternalDialog *dialog)
{
  MenuEditorAddExternalDialogPrivate *priv = MENUEDITOR_ADD_EXTERNAL_DIALOG_GET_PRIVATE (dialog);
  
  return gtk_combo_box_get_active (GTK_COMBO_BOX (priv->combo_style));
}

gboolean
menueditor_add_external_dialog_get_entry_unique (MenuEditorAddExternalDialog *dialog)
{
  MenuEditorAddExternalDialogPrivate *priv = MENUEDITOR_ADD_EXTERNAL_DIALOG_GET_PRIVATE (dialog);
  
  return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->check_button_unique));
}
