/* $Id: frap-menu-directory.h 24974 2007-02-14 10:49:23Z jannis $ */
/* vi:set expandtab sw=2 sts=2: */
/*-
 * Copyright (c) 2006-2007 Jannis Pohlmann <jannis@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if !defined(LIBFRAPMENU_INSIDE_LIBFRAPMENU_H) && !defined(LIBFRAPMENU_COMPILATION)
#error "Only <libfrapmenu/libfrapmenu.h> can be included directly. This file may disappear or change contents."
#endif

#ifndef __FRAP_MENU_DIRECTORY_H__
#define __FRAP_MENU_DIRECTORY_H__

#include <glib-object.h>

G_BEGIN_DECLS;

typedef struct _FrapMenuDirectoryPrivate FrapMenuDirectoryPrivate;
typedef struct _FrapMenuDirectoryClass   FrapMenuDirectoryClass;
typedef struct _FrapMenuDirectory        FrapMenuDirectory;

#define FRAP_TYPE_MENU_DIRECTORY            (frap_menu_directory_get_type ())
#define FRAP_MENU_DIRECTORY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FRAP_TYPE_MENU_DIRECTORY, FrapMenuDirectory))
#define FRAP_MENU_DIRECTORY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FRAP_TYPE_MENU_DIRECTORY, FrapMenuDirectoryClass))
#define FRAP_IS_MENU_DIRECTORY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FRAP_TYPE_MENU_DIRECTORY))
#define FRAP_IS_MENU_DIRECTORY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FRAP_TYPE_MENU_DIRECTORY))
#define FRAP_MENU_DIRECTORY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FRAP_TYPE_MENU_DIRECTORY, FrapMenuDirectoryClass))

GType                    frap_menu_directory_get_type            (void) G_GNUC_CONST;

const gchar             *frap_menu_directory_get_filename        (FrapMenuDirectory *directory);
void                     frap_menu_directory_set_filename        (FrapMenuDirectory *directory,
                                                                  const gchar       *name);
const gchar             *frap_menu_directory_get_name            (FrapMenuDirectory *directory);
void                     frap_menu_directory_set_name            (FrapMenuDirectory *directory,
                                                                  const gchar       *name);
const gchar             *frap_menu_directory_get_comment         (FrapMenuDirectory *directory);
void                     frap_menu_directory_set_comment         (FrapMenuDirectory *directory,
                                                                  const gchar       *comment);
const gchar             *frap_menu_directory_get_icon            (FrapMenuDirectory *directory);
void                     frap_menu_directory_set_icon            (FrapMenuDirectory *directory,
                                                                  const gchar       *icon);
gboolean                 frap_menu_directory_get_no_display      (FrapMenuDirectory *directory);
void                     frap_menu_directory_set_no_display      (FrapMenuDirectory *directory,
                                                                  gboolean           no_display);
gboolean                 frap_menu_directory_get_hidden          (FrapMenuDirectory *directory);
gboolean                 frap_menu_directory_show_in_environment (FrapMenuDirectory *directory);

#if defined(LIBFRAPMENU_COMPILATION)
void _frap_menu_directory_init     (void) G_GNUC_INTERNAL;
void _frap_menu_directory_shutdown (void) G_GNUC_INTERNAL;
#endif

G_END_DECLS;

#endif /* !__FRAP_MENU_DIRECTORY_H__ */
