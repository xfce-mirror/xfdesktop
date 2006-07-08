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

#ifndef __MENUEDITOR_EDIT_DIALOG_H__
#define __MENUEDITOR_EDIT_DIALOG_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include "menueditor-main-window.h"

G_BEGIN_DECLS
#define MENUEDITOR_TYPE_EDIT_DIALOG            (menueditor_edit_dialog_get_type ())
#define MENUEDITOR_EDIT_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MENUEDITOR_TYPE_EDIT_DIALOG, MenuEditorEditDialog))
#define MENUEDITOR_EDIT_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MENUEDITOR_TYPE_EDIT_DIALOG, MenuEditorEditDialogClass))
#define MENUEDITOR_IS_EDIT_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MENUEDITOR_TYPE_EDIT_DIALOG))
#define MENUEDITOR_IS_EDIT_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MENUEDITOR_TYPE_EDIT_DIALOG))
#define MENUEDITOR_EDIT_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MENUEDITOR_TYPE_EDIT_DIALOG, MenuEditorEditDialogClass))
typedef struct _MenuEditorEditDialog MenuEditorEditDialog;
typedef struct _MenuEditorEditDialogClass MenuEditorEditDialogClass;

struct _MenuEditorEditDialog
{
  XfceTitledDialog window;
};

struct _MenuEditorEditDialogClass
{
  XfceTitledDialogClass parent_class;
};

GtkType menueditor_edit_dialog_get_type (void);

GtkWidget *menueditor_edit_dialog_new (GtkWindow *parent);

void menueditor_edit_dialog_set_entry_type (MenuEditorEditDialog *dialog, EntryType type);
void menueditor_edit_dialog_set_entry_name (MenuEditorEditDialog *dialog, const gchar *name);
void menueditor_edit_dialog_set_entry_command (MenuEditorEditDialog *dialog, const gchar *command);
void menueditor_edit_dialog_set_entry_icon (MenuEditorEditDialog *dialog, const gchar *icon);
void menueditor_edit_dialog_set_entry_startup_notification (MenuEditorEditDialog *dialog, gboolean snotify);
void menueditor_edit_dialog_set_entry_run_in_terminal (MenuEditorEditDialog *dialog, gboolean interm);

gchar *menueditor_edit_dialog_get_entry_name (MenuEditorEditDialog *dialog);
gchar *menueditor_edit_dialog_get_entry_command (MenuEditorEditDialog *dialog);
gchar *menueditor_edit_dialog_get_entry_icon (MenuEditorEditDialog *dialog);
gboolean menueditor_edit_dialog_get_entry_startup_notification (MenuEditorEditDialog *dialog);
gboolean menueditor_edit_dialog_get_entry_run_in_terminal (MenuEditorEditDialog *dialog);

G_END_DECLS
#endif
