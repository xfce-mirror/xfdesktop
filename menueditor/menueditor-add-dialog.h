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

#ifndef __MENUEDITOR_ADD_DIALOG_H__
#define __MENUEDITOR_ADD_DIALOG_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include "menueditor-main-window.h"

G_BEGIN_DECLS
#define MENUEDITOR_TYPE_ADD_DIALOG            (menueditor_add_dialog_get_type ())
#define MENUEDITOR_ADD_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MENUEDITOR_TYPE_ADD_DIALOG, MenuEditorAddDialog))
#define MENUEDITOR_ADD_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MENUEDITOR_TYPE_ADD_DIALOG, MenuEditorAddDialogClass))
#define MENUEDITOR_IS_ADD_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MENUEDITOR_TYPE_ADD_DIALOG))
#define MENUEDITOR_IS_ADD_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MENUEDITOR_TYPE_ADD_DIALOG))
#define MENUEDITOR_ADD_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MENUEDITOR_TYPE_ADD_DIALOG, MenuEditorAddDialogClass))
typedef struct _MenuEditorAddDialog MenuEditorAddDialog;
typedef struct _MenuEditorAddDialogClass MenuEditorAddDialogClass;

struct _MenuEditorAddDialog
{
  XfceTitledDialog window;
};

struct _MenuEditorAddDialogClass
{
  XfceTitledDialogClass parent_class;
};

GtkType menueditor_add_dialog_get_type (void);

GtkWidget *menueditor_add_dialog_new (GtkWindow *parent);

EntryType menueditor_add_dialog_get_entry_type (MenuEditorAddDialog *dialog);
gchar *menueditor_add_dialog_get_entry_name (MenuEditorAddDialog *dialog);
gchar *menueditor_add_dialog_get_entry_command (MenuEditorAddDialog *dialog);
gchar *menueditor_add_dialog_get_entry_icon (MenuEditorAddDialog *dialog);
gboolean menueditor_add_dialog_get_entry_startup_notification (MenuEditorAddDialog *dialog);
gboolean menueditor_add_dialog_get_entry_run_in_terminal (MenuEditorAddDialog *dialog);

G_END_DECLS
#endif
