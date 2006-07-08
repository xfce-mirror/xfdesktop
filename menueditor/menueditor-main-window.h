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

#ifndef __MENUEDITOR_MAIN_WINDOW_H__
#define __MENUEDITOR_MAIN_WINDOW_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

G_BEGIN_DECLS
#define MENUEDITOR_TYPE_MAIN_WINDOW            (menueditor_main_window_get_type ())
#define MENUEDITOR_MAIN_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MENUEDITOR_TYPE_MAIN_WINDOW, MenuEditorMainWindow))
#define MENUEDITOR_MAIN_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MENUEDITOR_TYPE_MAIN_WINDOW, MenuEditorMainWindowClass))
#define MENUEDITOR_IS_MAIN_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MENUEDITOR_TYPE_MAIN_WINDOW))
#define MENUEDITOR_IS_MAIN_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MENUEDITOR_TYPE_MAIN_WINDOW))
#define MENUEDITOR_MAIN_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MENUEDITOR_TYPE_MAIN_WINDOW, MenuEditorMainWindowClass))
typedef struct _MenuEditorMainWindow MenuEditorMainWindow;
typedef struct _MenuEditorMainWindowClass MenuEditorMainWindowClass;

struct _MenuEditorMainWindow
{
  GtkWindow window;
};

struct _MenuEditorMainWindowClass
{
  GtkWindowClass parent_class;
};

enum
{
  COLUMN_ICON, COLUMN_NAME, COLUMN_COMMAND, COLUMN_HIDDEN, COLUMN_TYPE,
  COLUMN_OPTION_1, COLUMN_OPTION_2, COLUMN_OPTION_3, COLUMNS
};

typedef enum
{
  TITLE, MENU, APP, SEPARATOR, BUILTIN, INCLUDE_FILE, INCLUDE_SYSTEM,
} EntryType;

typedef enum {
  SIMPLE,
  MULTI_LEVEL,
} ExternalEntryStyle;

GtkType menueditor_main_window_get_type (void);
GtkWidget *menueditor_main_window_new (void);

void menueditor_main_window_set_menu_modified (MenuEditorMainWindow *win);

G_END_DECLS
#endif
