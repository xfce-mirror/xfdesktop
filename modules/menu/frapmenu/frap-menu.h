/* $Id: frap-menu.h 25219 2007-03-19 12:52:26Z jannis $ */
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

#ifndef __FRAP_MENU_H__
#define __FRAP_MENU_H__

#include <glib-object.h>

G_BEGIN_DECLS;

typedef struct _FrapMenuPrivate FrapMenuPrivate;
typedef struct _FrapMenuClass   FrapMenuClass;
typedef struct _FrapMenu        FrapMenu;

#define FRAP_TYPE_MENU            (frap_menu_get_type ())
#define FRAP_MENU(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FRAP_TYPE_MENU, FrapMenu))
#define FRAP_MENU_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FRAP_TYPE_MENU, FrapMenuClass))
#define FRAP_IS_MENU(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FRAP_TYPE_MENU))
#define FRAP_IS_MENU_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FRAP_TYPE_MENU))
#define FRAP_MENU_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FRAP_TYPE_MENU, FrapMenuClass))

void               frap_menu_init                  (const gchar *env);
void               frap_menu_shutdown              (void);

GType              frap_menu_get_type              (void) G_GNUC_CONST;

FrapMenu          *frap_menu_get_root              (GError           **error) G_GNUC_CONST;

FrapMenu          *frap_menu_new                   (const gchar       *filename,
                                                    GError           **error) G_GNUC_MALLOC;

const gchar       *frap_menu_get_filename          (FrapMenu          *menu);
void               frap_menu_set_filename          (FrapMenu          *menu,
                                                    const gchar       *filename);
const gchar       *frap_menu_get_name              (FrapMenu          *menu);
void               frap_menu_set_name              (FrapMenu          *menu,
                                                    const gchar       *name);
FrapMenuDirectory *frap_menu_get_directory         (FrapMenu          *menu);
void               frap_menu_set_directory         (FrapMenu          *menu,
                                                    FrapMenuDirectory *directory);
GSList            *frap_menu_get_directory_dirs    (FrapMenu          *menu);
GSList            *frap_menu_get_legacy_dirs       (FrapMenu          *menu);
GSList            *frap_menu_get_app_dirs          (FrapMenu          *menu);
gboolean           frap_menu_get_only_unallocated  (FrapMenu          *menu);
void               frap_menu_set_only_unallocated  (FrapMenu          *menu,
                                                    gboolean           only_unallocated);
gboolean           frap_menu_get_deleted           (FrapMenu          *menu);
void               frap_menu_set_deleted           (FrapMenu          *menu,
                                                    gboolean           deleted);
GSList            *frap_menu_get_menus             (FrapMenu          *menu);
void               frap_menu_add_menu              (FrapMenu          *menu,
                                                    FrapMenu          *submenu);
FrapMenu          *frap_menu_get_menu_with_name    (FrapMenu          *menu,
                                                    const gchar       *name);
FrapMenu          *frap_menu_get_parent            (FrapMenu          *menu);
FrapMenuItemPool  *frap_menu_get_item_pool         (FrapMenu          *menu);
GSList            *frap_menu_get_items             (FrapMenu          *menu);
gboolean           frap_menu_has_layout            (FrapMenu          *menu);
GSList            *frap_menu_get_layout_items      (FrapMenu          *menu);

G_END_DECLS;

#endif /* !__FRAP_MENU_H__ */
