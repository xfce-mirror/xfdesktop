/* $Id: frap-menu-item-pool.h 24874 2007-02-07 23:29:34Z jannis $ */
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

#ifndef __FRAP_MENU_ITEM_POOL_H__
#define __FRAP_MENU_ITEM_POOL_H__

#include <glib-object.h>
#include <frap-menu-standard-rules.h>

G_BEGIN_DECLS;

typedef struct _FrapMenuItemPoolPrivate FrapMenuItemPoolPrivate;
typedef struct _FrapMenuItemPoolClass   FrapMenuItemPoolClass;
typedef struct _FrapMenuItemPool        FrapMenuItemPool;

#define FRAP_TYPE_MENU_ITEM_POOL            (frap_menu_item_pool_get_type ())
#define FRAP_MENU_ITEM_POOL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FRAP_TYPE_MENU_ITEM_POOL, FrapMenuItemPool))
#define FRAP_MENU_ITEM_POOL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FRAP_TYPE_MENU_ITEM_POOL, FrapMenuItemPoolClass))
#define FRAP_IS_MENU_ITEM_POOL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FRAP_TYPE_MENU_ITEM_POOL))
#define FRAP_IS_MENU_ITEM_POOL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FRAP_TYPE_MENU_ITEM_POOL))
#define FRAP_MENU_ITEM_POOL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FRAP_TYPE_MENU_ITEM_POOL, FrapMenuItemPoolClass))

GType             frap_menu_item_pool_get_type           (void) G_GNUC_CONST;

FrapMenuItemPool *frap_menu_item_pool_new                (void);

void              frap_menu_item_pool_insert             (FrapMenuItemPool      *pool,
                                                          FrapMenuItem          *item);
FrapMenuItem     *frap_menu_item_pool_lookup             (FrapMenuItemPool      *pool,
                                                          const gchar           *desktop_id);
void              frap_menu_item_pool_foreach            (FrapMenuItemPool      *pool,
                                                          GHFunc                 func, 
                                                          gpointer               user_data);
void              frap_menu_item_pool_apply_exclude_rule (FrapMenuItemPool      *pool,
                                                          FrapMenuStandardRules *rule);
gboolean          frap_menu_item_pool_get_empty          (FrapMenuItemPool      *pool);

G_END_DECLS;

#endif /* !__FRAP_MENU_ITEM_POOL_H__ */
