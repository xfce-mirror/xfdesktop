/* $Id: frap-menu-item-cache.h 24976 2007-02-14 11:04:52Z jannis $ */
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

#ifndef __FRAP_MENU_ITEM_CACHE_H__
#define __FRAP_MENU_ITEM_CACHE_H__

#include <glib-object.h>

G_BEGIN_DECLS;

typedef struct _FrapMenuItemCachePrivate FrapMenuItemCachePrivate;
typedef struct _FrapMenuItemCacheClass   FrapMenuItemCacheClass;
typedef struct _FrapMenuItemCache        FrapMenuItemCache;

#define FRAP_TYPE_MENU_ITEM_CACHE            (frap_menu_item_cache_get_type ())
#define FRAP_MENU_ITEM_CACHE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FRAP_TYPE_MENU_ITEM_CACHE, FrapMenuItemCache))
#define FRAP_MENU_ITEM_CACHE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FRAP_TYPE_MENU_ITEM_CACHE, FrapMenuItemCacheClass))
#define FRAP_IS_MENU_ITEM_CACHE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FRAP_TYPE_MENU_ITEM_CACHE))
#define FRAP_IS_MENU_ITEM_CACHE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FRAP_TYPE_MENU_ITEM_CACHE))
#define FRAP_MENU_ITEM_CACHE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FRAP_TYPE_MENU_ITEM_CACHE, FrapMenuItemCacheClass))

GType              frap_menu_item_cache_get_type    (void) G_GNUC_CONST;

FrapMenuItemCache *frap_menu_item_cache_get_default (void);

FrapMenuItem      *frap_menu_item_cache_lookup      (FrapMenuItemCache *cache,
                                                     const gchar       *filename,
                                                     const gchar       *desktop_id);
void               frap_menu_item_cache_foreach     (FrapMenuItemCache *cache,
                                                     GHFunc             func, 
                                                     gpointer           user_data);

#if defined(LIBFRAPMENU_COMPILATION)
void               _frap_menu_item_cache_init       (void) G_GNUC_INTERNAL;
void               _frap_menu_item_cache_shutdown   (void) G_GNUC_INTERNAL;
#endif

G_END_DECLS;

#endif /* !__FRAP_MENU_ITEM_CACHE_H__ */
