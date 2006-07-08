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

#ifndef __MENUEDITOR_ADD_EXTERNAL_DIALOG_H__
#define __MENUEDITOR_ADD_EXTERNAL_DIALOG_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include "menueditor-main-window.h"

G_BEGIN_DECLS
#define MENUEDITOR_TYPE_ADD_EXTERNAL_DIALOG            (menueditor_add_external_dialog_get_type ())
#define MENUEDITOR_ADD_EXTERNAL_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MENUEDITOR_TYPE_ADD_EXTERNAL_DIALOG, MenuEditorAddExternalDialog))
#define MENUEDITOR_ADD_EXTERNAL_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MENUEDITOR_TYPE_ADD_EXTERNAL_DIALOG, MenuEditorAddExternalDialogClass))
#define MENUEDITOR_IS_ADD_EXTERNAL_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MENUEDITOR_TYPE_ADD_EXTERNAL_DIALOG))
#define MENUEDITOR_IS_ADD_EXTERNAL_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MENUEDITOR_TYPE_ADD_EXTERNAL_DIALOG))
#define MENUEDITOR_ADD_EXTERNAL_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MENUEDITOR_TYPE_ADD_EXTERNAL_DIALOG, MenuEditorAddExternalDialogClass))
typedef struct _MenuEditorAddExternalDialog MenuEditorAddExternalDialog;
typedef struct _MenuEditorAddExternalDialogClass MenuEditorAddExternalDialogClass;

struct _MenuEditorAddExternalDialog
{
  XfceTitledDialog window;
};

struct _MenuEditorAddExternalDialogClass
{
  XfceTitledDialogClass parent_class;
};

GtkType menueditor_add_external_dialog_get_type (void);

GtkWidget *menueditor_add_external_dialog_new (GtkWindow *parent);

EntryType menueditor_add_external_dialog_get_entry_type (MenuEditorAddExternalDialog *dialog);
gchar *menueditor_add_external_dialog_get_entry_source (MenuEditorAddExternalDialog *dialog);
ExternalEntryStyle menueditor_add_external_dialog_get_entry_style (MenuEditorAddExternalDialog *dialog);
gboolean menueditor_add_external_dialog_get_entry_unique (MenuEditorAddExternalDialog *dialog);

G_END_DECLS
#endif
