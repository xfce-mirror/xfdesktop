/* $Id: frap-menu-item-cache.c 24869 2007-02-06 19:50:55Z jannis $ */
/* vi:set expandtab sw=2 sts=2 et: */
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

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <libxfce4util/libxfce4util.h>

#include <tdb/tdb.h>

#include <frap-menu-item.h>
#include <frap-menu-item-cache.h>



/* TODO Use a proper value here */
#define FRAP_MENU_ITEM_CACHE_MAX_PATH_LENGTH 4096



#define FRAP_MENU_ITEM_CACHE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), FRAP_TYPE_MENU_ITEM_CACHE, FrapMenuItemCachePrivate))



static void          frap_menu_item_cache_class_init (FrapMenuItemCacheClass *klass);
static void          frap_menu_item_cache_init       (FrapMenuItemCache      *cache);
static void          frap_menu_item_cache_finalize   (GObject                *object);
static FrapMenuItem *frap_menu_item_cache_fetch_item (FrapMenuItemCache *cache,
                                                      const gchar       *filename);
static void          frap_menu_item_cache_store_item (FrapMenuItemCache      *cache,
                                                      const gchar            *filename,
                                                      FrapMenuItem           *item);




static FrapMenuItemCache *_frap_menu_item_cache = NULL;



void
_frap_menu_item_cache_init (void)
{
  if (G_LIKELY (_frap_menu_item_cache == NULL))
    _frap_menu_item_cache = g_object_new (FRAP_TYPE_MENU_ITEM_CACHE, NULL);
}



void
_frap_menu_item_cache_shutdown (void)
{
  if (G_LIKELY (_frap_menu_item_cache != NULL))
    g_object_unref (G_OBJECT (_frap_menu_item_cache));
}



struct _FrapMenuItemCacheClass
{
  GObjectClass __parent__;
};

struct _FrapMenuItemCachePrivate
{
  /* Hash table for mapping absolute filenames to FrapMenuItem's */
  GHashTable  *items;

  /* TDB context */
  TDB_CONTEXT *context;
  TDB_DATA     data;

  /* Mutex lock */
  GMutex      *lock;
};

struct _FrapMenuItemCache
{
  GObject __parent__;

  /* Private data */
  FrapMenuItemCachePrivate *priv;
};



static GObjectClass *frap_menu_item_cache_parent_class = NULL;



GType
frap_menu_item_cache_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info = 
      {
        sizeof (FrapMenuItemCacheClass),
        NULL,
        NULL,
        (GClassInitFunc) frap_menu_item_cache_class_init,
        NULL,
        NULL,
        sizeof (FrapMenuItemCache),
        0,
        (GInstanceInitFunc) frap_menu_item_cache_init,
        NULL,
      };

      type = g_type_register_static (G_TYPE_OBJECT, "FrapMenuItemCache", &info, 0);
    }
  
  return type;
}



static void
frap_menu_item_cache_class_init (FrapMenuItemCacheClass *klass)
{
  GObjectClass *gobject_class;

  g_type_class_add_private (klass, sizeof (FrapMenuItemCachePrivate));

  /* Determine the parent type class */
  frap_menu_item_cache_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = frap_menu_item_cache_finalize;
}



static void
frap_menu_item_cache_init (FrapMenuItemCache *cache)
{
  gchar *path;

  cache->priv = FRAP_MENU_ITEM_CACHE_GET_PRIVATE (cache);

  /* Initialize the mutex lock */
  cache->priv->lock = g_mutex_new ();

  /* Create empty hash table */
  cache->priv->items = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) frap_menu_item_unref);

  /* Determine path to the item cache file */
  path = xfce_resource_save_location (XFCE_RESOURCE_CACHE, "libfrapmenu/items.tdb", TRUE);

  /* Print a warning of the cache file could not be created */
  if (G_UNLIKELY (path == NULL))
    {
      path = xfce_resource_save_location (XFCE_RESOURCE_CACHE, "libfrapmenu/items.tdb", FALSE);
      g_warning ("Failed to create the libfrapmenu item cache in %s.", path);
      g_free (path);
      return;
    }

  /* Try to open the item cache file */
  cache->priv->context = tdb_open (path, 0, TDB_DEFAULT, O_CREAT | O_RDWR, 0600);

  /* Print warning if it could not be opened */
  if (G_UNLIKELY (cache->priv->context == NULL))
    g_warning ("Failed to open libfrapmenu item cache in %s: %s.", path, g_strerror (errno));

  /* Release the path */
  g_free (path);
}



FrapMenuItemCache*
frap_menu_item_cache_get_default (void)
{
  return g_object_ref (G_OBJECT (_frap_menu_item_cache));
}



static void
frap_menu_item_cache_finalize (GObject *object)
{
  FrapMenuItemCache *cache = FRAP_MENU_ITEM_CACHE (object);

  /* Free hash table */
#if GLIB_CHECK_VERSION(2,10,0)
  g_hash_table_unref (cache->priv->items);
#else
  g_hash_table_destroy (cache->priv->items);
#endif

  /* Close TDB database */
  if (G_LIKELY (cache->priv->context != NULL))
    tdb_close (cache->priv->context);

  /* Release mutex lock */
  g_mutex_free (cache->priv->lock);

  (*G_OBJECT_CLASS (frap_menu_item_cache_parent_class)->finalize) (object);
}



FrapMenuItem*
frap_menu_item_cache_lookup (FrapMenuItemCache *cache,
                             const gchar       *filename,
                             const gchar       *desktop_id)
{
  FrapMenuItem        *item = NULL;
  FrapMenuItemPrivate *priv = NULL;
  TDB_DATA             key_data;
  TDB_DATA             data;
  gssize               key_size;
  gchar                key_path[4096];

  g_return_val_if_fail (FRAP_IS_MENU_ITEM_CACHE (cache), NULL);
  g_return_val_if_fail (g_path_is_absolute (filename), NULL);
  g_return_val_if_fail (desktop_id != NULL, NULL);

  /* Acquire lock on the item cache as it's likely that we need to load 
   * items from the hard drive and store it in the hash table of the 
   * item cache */
  g_mutex_lock (cache->priv->lock);

  /* Search filename in the hash table */
  item = g_hash_table_lookup (cache->priv->items, filename);

  /* Return the item if we we found one */
  if (item != NULL)
    {
      /* Update desktop id, if necessary */
      frap_menu_item_set_desktop_id (item, desktop_id);

      /* TODO Check OnlyShowIn / NotShowIn values */

      /* Store updated item in cache */
      frap_menu_item_cache_store_item (cache, filename, item);

      /* Release item cache lock */
      g_mutex_unlock (cache->priv->lock);

      return item;
    }

  /* Otherwise, search in the item cache */
  if (G_LIKELY (cache->priv->context != NULL))
    {
      /* Fetch item from cache */
      item = frap_menu_item_cache_fetch_item (cache, filename);

      if (G_LIKELY (item != NULL))
        {
          /* Update desktop id */
          frap_menu_item_set_desktop_id (item, desktop_id);

          /* TODO Check OnlyShowIn / NotShowIn */

          /* Store updated item in the cache */
          frap_menu_item_cache_store_item (cache, filename, item);

          /* Add item to the hash table */
          g_hash_table_replace (cache->priv->items, g_strdup (filename), item);

          /* Grab a reference on the item, but don't increase the allocation
           * counter */
          g_object_ref (G_OBJECT (item));

          /* Release item cache lock */
          g_mutex_unlock (cache->priv->lock);

          return item;
        }
    }

  /* Last chance is to load it directly from the file */
  item = frap_menu_item_new (filename);

  if (G_LIKELY (item != NULL))
    {
      /* Update desktop id */
      frap_menu_item_set_desktop_id (item, desktop_id);

      /* Store updated item in cache */
      frap_menu_item_cache_store_item (cache, filename, item);

      /* The file has been loaded, add the item to the hash table */
      g_hash_table_replace (cache->priv->items, g_strdup (filename), item);

      /* Grab a reference on it but don't increase the allocation counter */
      g_object_ref (G_OBJECT (item));
    }

  /* Release item cache lock */
  g_mutex_unlock (cache->priv->lock);

  return item;
}



void 
frap_menu_item_cache_foreach (FrapMenuItemCache *cache,
                              GHFunc             func,
                              gpointer           user_data)
{
  g_return_if_fail (FRAP_IS_MENU_ITEM_CACHE (cache));

  /* Acquire lock on the item cache */
  g_mutex_lock (cache->priv->lock);

  g_hash_table_foreach (cache->priv->items, func, user_data);

  /* Release item cache lock */
  g_mutex_lock (cache->priv->lock);
}



typedef struct _FooItem
{
  gchar *name;
} FooItem; 



static FrapMenuItem*
frap_menu_item_cache_fetch_item (FrapMenuItemCache *cache,
                                 const gchar       *filename)
{
  TDB_DATA      key;
  gssize        key_length;
  gchar         key_path[FRAP_MENU_ITEM_CACHE_MAX_PATH_LENGTH];
  gchar         c;
  TDB_DATA      data;
  FooItem      *foo;
  FrapMenuItem *item = NULL;
  
  g_return_val_if_fail (FRAP_IS_MENU_ITEM_CACHE (cache), NULL);
  g_return_val_if_fail (cache->priv->context != NULL, NULL);
  g_return_val_if_fail (g_path_is_absolute (filename), NULL);

  /* Generate key path */
  key_length = g_snprintf (key_path, FRAP_MENU_ITEM_CACHE_MAX_PATH_LENGTH, filename, NULL);

  /* Generate key */
  key.dsize = key_length;
  key.dptr = key_path;

  /* Try to fetch the data */
  data = tdb_fetch (cache->priv->context, key);

  if (G_LIKELY (data.dptr != NULL))
    {
      /* TODO Read item information from the data buffer into a newly created 
       * FrapMenuItem object. */
    }

  return item;
}



static void
frap_menu_item_cache_store_item (FrapMenuItemCache *cache,
                                 const gchar       *filename,
                                 FrapMenuItem      *item)
{
  TDB_DATA     key;
  TDB_DATA     data;
  gssize       key_length;
  gchar        key_path[FRAP_MENU_ITEM_CACHE_MAX_PATH_LENGTH];
  FooItem      foo;

  g_return_if_fail (FRAP_IS_MENU_ITEM_CACHE (cache));
  g_return_if_fail (cache->priv->context != NULL);
  g_return_if_fail (g_path_is_absolute (filename));
  g_return_if_fail (FRAP_IS_MENU_ITEM (item));

  /* Generate key path */
  key_length = g_snprintf (key_path, FRAP_MENU_ITEM_CACHE_MAX_PATH_LENGTH, filename, NULL);

  /* Generate key */
  key.dsize = key_length;
  key.dptr = key_path;

  /* TODO Allocate a suitable buffer for item information and copy this
   * information into the data buffer. Afterwards, store it in the database: 
   *   tdb_store (cache->priv->context, key, data, TDB_REPLACE);
   */
}
