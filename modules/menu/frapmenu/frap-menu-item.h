/* $Id: frap-menu-item.h 24974 2007-02-14 10:49:23Z jannis $ */
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

#ifndef __FRAP_MENU_ITEM_H__
#define __FRAP_MENU_ITEM_H__

#include <glib-object.h>

G_BEGIN_DECLS;

typedef struct _FrapMenuItemPrivate FrapMenuItemPrivate;
typedef struct _FrapMenuItemClass   FrapMenuItemClass;
typedef struct _FrapMenuItem        FrapMenuItem;

#define FRAP_TYPE_MENU_ITEM            (frap_menu_item_get_type())
#define FRAP_MENU_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FRAP_TYPE_MENU_ITEM, FrapMenuItem))
#define FRAP_MENU_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FRAP_TYPE_MENU_ITEM, FrapMenuItemClass))
#define FRAP_IS_MENU_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FRAP_TYPE_MENU_ITEM))
#define FRAP_IS_MENU_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FRAP_TYPE_MENU_ITEM))
#define FRAP_MENU_ITEM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FRAP_TYPE_MENU_ITEM, FrapMenuItemClass))

GType         frap_menu_item_get_type                          (void) G_GNUC_CONST;

FrapMenuItem *frap_menu_item_new                               (const gchar  *filename);

const gchar  *frap_menu_item_get_desktop_id                    (FrapMenuItem *item);
void          frap_menu_item_set_desktop_id                    (FrapMenuItem *item,
                                                                const gchar  *desktop_id);

const gchar  *frap_menu_item_get_filename                      (FrapMenuItem *item);
void          frap_menu_item_set_filename                      (FrapMenuItem *item,
                                                                const gchar  *filename);
const gchar  *frap_menu_item_get_command                       (FrapMenuItem *item);
void          frap_menu_item_set_command                       (FrapMenuItem *item,
                                                                const gchar  *command);
const gchar  *frap_menu_item_get_name                          (FrapMenuItem *item);
void          frap_menu_item_set_name                          (FrapMenuItem *item,
                                                                const gchar  *name);
const gchar  *frap_menu_item_get_icon_name                     (FrapMenuItem *item);
void          frap_menu_item_set_icon_name                     (FrapMenuItem *item,
                                                                const gchar  *icon_name);
gboolean      frap_menu_item_requires_terminal                 (FrapMenuItem *item);
void          frap_menu_item_set_requires_terminal             (FrapMenuItem *item,
                                                                gboolean      requires_terminal);
gboolean      frap_menu_item_get_no_display                    (FrapMenuItem *item);
void          frap_menu_item_set_no_display                    (FrapMenuItem *item,
                                                                gboolean      no_display);
gboolean      frap_menu_item_supports_startup_notification     (FrapMenuItem *item);
void          frap_menu_item_set_supports_startup_notification (FrapMenuItem *item,
                                                                gboolean      supports_startup_notification);
GList        *frap_menu_item_get_categories                    (FrapMenuItem *item);
void          frap_menu_item_set_categories                    (FrapMenuItem *item,
                                                                GList        *categories);
gboolean      frap_menu_item_show_in_environment               (FrapMenuItem *item);
void          frap_menu_item_ref                               (FrapMenuItem *item);
void          frap_menu_item_unref                             (FrapMenuItem *item);
gint          frap_menu_item_get_allocated                     (FrapMenuItem *item);
void          frap_menu_item_increment_allocated               (FrapMenuItem *item);
void          frap_menu_item_decrement_allocated               (FrapMenuItem *item);

G_END_DECLS;

#endif /* !__FRAP_MENU_ITEM_H__ */
