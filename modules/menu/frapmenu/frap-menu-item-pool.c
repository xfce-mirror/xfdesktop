/* $Id: frap-menu-item-pool.c 24874 2007-02-07 23:29:34Z jannis $ */
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <frap-menu-item.h>
#include <frap-menu-item-pool.h>
#include <frap-menu-rules.h>
#include <frap-menu-standard-rules.h>



#define FRAP_MENU_ITEM_POOL_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), FRAP_TYPE_MENU_ITEM_POOL, FrapMenuItemPoolPrivate))



static void     frap_menu_item_pool_class_init       (FrapMenuItemPoolClass *klass);
static void     frap_menu_item_pool_init             (FrapMenuItemPool      *pool);
static void     frap_menu_item_pool_finalize         (GObject               *object);
static gboolean frap_menu_item_pool_filter_exclude   (const gchar           *desktop_id,
                                                      FrapMenuItem          *item,
                                                      FrapMenuStandardRules *rules);
static gboolean frap_menu_item_pool_remove           (const gchar           *desktop_id,
                                                      FrapMenuItem          *item);



struct _FrapMenuItemPoolClass
{
  GObjectClass __parent__;
};

struct _FrapMenuItemPoolPrivate
{
  /* Hash table for mapping desktop-file id's to FrapMenuItem's */
  GHashTable *items;
};

struct _FrapMenuItemPool
{
  GObject __parent__;

  /* < private > */
  FrapMenuItemPoolPrivate *priv;
};



static GObjectClass *frap_menu_item_pool_parent_class = NULL;



GType
frap_menu_item_pool_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info = 
      {
        sizeof (FrapMenuItemPoolClass),
        NULL,
        NULL,
        (GClassInitFunc) frap_menu_item_pool_class_init,
        NULL,
        NULL,
        sizeof (FrapMenuItemPool),
        0,
        (GInstanceInitFunc) frap_menu_item_pool_init,
        NULL,
      };

      type = g_type_register_static (G_TYPE_OBJECT, "FrapMenuItemPool", &info, 0);
    }
  
  return type;
}



static void
frap_menu_item_pool_class_init (FrapMenuItemPoolClass *klass)
{
  GObjectClass *gobject_class;

  g_type_class_add_private (klass, sizeof (FrapMenuItemPoolPrivate));

  /* Determine the parent type class */
  frap_menu_item_pool_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = frap_menu_item_pool_finalize;
}



static void
frap_menu_item_pool_init (FrapMenuItemPool *pool)
{
  pool->priv = FRAP_MENU_ITEM_POOL_GET_PRIVATE (pool);
  pool->priv->items = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) frap_menu_item_unref);
}



static void
frap_menu_item_pool_finalize (GObject *object)
{
  FrapMenuItemPool *pool = FRAP_MENU_ITEM_POOL (object);

#if GLIB_CHECK_VERSION(2,10,0)
  g_hash_table_unref (pool->priv->items);
#else
  g_hash_table_destroy (pool->priv->items);
#endif

  (*G_OBJECT_CLASS (frap_menu_item_pool_parent_class)->finalize) (object);
}



FrapMenuItemPool*
frap_menu_item_pool_new (void)
{
  return g_object_new (FRAP_TYPE_MENU_ITEM_POOL, NULL);
}



void
frap_menu_item_pool_insert (FrapMenuItemPool *pool,
                            FrapMenuItem     *item)
{
  g_return_if_fail (FRAP_IS_MENU_ITEM_POOL (pool));
  g_return_if_fail (FRAP_IS_MENU_ITEM (item));

  /* Insert into the hash table and remove old item (if any) */
  g_hash_table_replace (pool->priv->items, g_strdup (frap_menu_item_get_desktop_id (item)), item);

  /* Grab a reference on the item */
  frap_menu_item_ref (item);
}



FrapMenuItem*
frap_menu_item_pool_lookup (FrapMenuItemPool *pool,
                            const gchar      *desktop_id)
{
  g_return_val_if_fail (FRAP_IS_MENU_ITEM_POOL (pool), NULL);
  g_return_val_if_fail (desktop_id != NULL, NULL);

  return g_hash_table_lookup (pool->priv->items, desktop_id);
}



void 
frap_menu_item_pool_foreach (FrapMenuItemPool *pool,
                             GHFunc            func,
                             gpointer          user_data)
{
  g_return_if_fail (FRAP_IS_MENU_ITEM_POOL (pool));

  g_hash_table_foreach (pool->priv->items, func, user_data);
}



void
frap_menu_item_pool_apply_exclude_rule (FrapMenuItemPool      *pool,
                                        FrapMenuStandardRules *rule)
{
  g_return_if_fail (FRAP_IS_MENU_ITEM_POOL (pool));
  g_return_if_fail (FRAP_IS_MENU_STANDARD_RULES (rule));

  /* Remove all items which match this exclude rule */
  g_hash_table_foreach_remove (pool->priv->items, (GHRFunc) frap_menu_item_pool_filter_exclude, rule);
}



static gboolean
frap_menu_item_pool_filter_exclude (const gchar           *desktop_id,
                                    FrapMenuItem          *item,
                                    FrapMenuStandardRules *rule)
{
  g_return_val_if_fail (FRAP_IS_MENU_STANDARD_RULES (rule), FALSE);
  g_return_val_if_fail (FRAP_IS_MENU_ITEM (item), FALSE);

  return frap_menu_rules_match (FRAP_MENU_RULES (rule), item);
}



gboolean
frap_menu_item_pool_get_empty (FrapMenuItemPool *pool)
{
  g_return_val_if_fail (FRAP_IS_MENU_ITEM_POOL (pool), TRUE);
  return (g_hash_table_size (pool->priv->items) == 0);
}
