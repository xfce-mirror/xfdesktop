/* $Id: frap-menu.c 24974 2007-02-14 10:49:23Z jannis $ */
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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <libxfce4util/libxfce4util.h>

#include <frap-menu-environment.h>
#include <frap-menu-item.h>
#include <frap-menu-rules.h>
#include <frap-menu-standard-rules.h>
#include <frap-menu-or-rules.h>
#include <frap-menu-and-rules.h>
#include <frap-menu-not-rules.h>
#include <frap-menu-directory.h>
#include <frap-menu-item-pool.h>
#include <frap-menu-item-cache.h>
#include <frap-menu-move.h>
#include <frap-menu.h>



/* Use g_access() on win32 */
#if defined(G_OS_WIN32)
#include <glib/gstdio.h>
#else
#define g_access(filename, mode) (access ((filename), (mode)))
#endif



/* Potential root menu files */
static const gchar FRAP_MENU_ROOT_SPECS[][30] = 
{
  "menus/applications.menu",
  "menus/gnome-applications.menu",
  "menus/kde-applications.menu",
};



static gint frap_menu_ref_count = 0;



/**
 * frap_menu_init:
 * @env : Name of the desktop environment (e.g. XFCE, GNOME, KDE) or %NULL.
 *
 * Initializes the libfrapmenu library and optionally defines the desktop 
 * environment for which menus will be generated. This means items belonging
 * only to other desktop environments will be ignored.
 **/
void
frap_menu_init (const gchar *env)
{
  if (g_atomic_int_exchange_and_add (&frap_menu_ref_count, 1) == 0)
    {
      /* Initialize the GObject type system */
      g_type_init ();

      /* Initialize the GThread system */
      if (!g_thread_supported ())
        g_thread_init (NULL);

      /* Set desktop environment */
      frap_menu_set_environment (env);

      /* Initialize the menu item cache */
      _frap_menu_item_cache_init ();

      /* Initialize the directory module */
      _frap_menu_directory_init ();
    }
}



/**
 * frap_menu_shutdown
 *
 * Shuts down the libfrapmenu library.
 **/
void
frap_menu_shutdown (void)
{
  if (g_atomic_int_dec_and_test (&frap_menu_ref_count))
    {
      /* Unset desktop environment */
      frap_menu_set_environment (NULL);

      /* Shutdown the directory module */
      _frap_menu_directory_shutdown ();

      /* Shutdown the menu item cache */
      _frap_menu_item_cache_shutdown ();
    }
}



#define FRAP_MENU_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), FRAP_TYPE_MENU, FrapMenuPrivate))



/* Menu file parser states */
typedef enum 
{
  FRAP_MENU_PARSE_STATE_START,
  FRAP_MENU_PARSE_STATE_ROOT,
  FRAP_MENU_PARSE_STATE_MENU,
  FRAP_MENU_PARSE_STATE_RULE,
  FRAP_MENU_PARSE_STATE_END,
  FRAP_MENU_PARSE_STATE_MOVE,

} FrapMenuParseState;

/* Menu file node types */
typedef enum
{
  FRAP_MENU_PARSE_NODE_TYPE_NONE,
  FRAP_MENU_PARSE_NODE_TYPE_NAME,
  FRAP_MENU_PARSE_NODE_TYPE_DIRECTORY,
  FRAP_MENU_PARSE_NODE_TYPE_APP_DIR,
  FRAP_MENU_PARSE_NODE_TYPE_LEGACY_DIR,
  FRAP_MENU_PARSE_NODE_TYPE_DIRECTORY_DIR,
  FRAP_MENU_PARSE_NODE_TYPE_FILENAME,
  FRAP_MENU_PARSE_NODE_TYPE_CATEGORY,
  FRAP_MENU_PARSE_NODE_TYPE_OLD,
  FRAP_MENU_PARSE_NODE_TYPE_NEW,

} FrapMenuParseNodeType;

/* Menu file parse context */
typedef struct _FrapMenuParseContext
{
  /* Menu to be parsed */
  FrapMenu             *root_menu;

  /* Parser state (position in XML tree */
  FrapMenuParseState    state;

  /* Menu hierarchy "stack" */
  GList                *menu_stack;

  /* Include/exclude rules stack */
  GList                *rule_stack;

  /* Current move instruction */
  FrapMenuMove         *move;

  /* Current node type (for text handler) */
  FrapMenuParseNodeType node_type;

} FrapMenuParseContext;

typedef struct _FrapMenuPair
{
  gpointer first;
  gpointer second;
} FrapMenuPair;

typedef struct _FrapMenuParseInfo
{
  /* Directory names */
  GSList     *directory_names;

  /* Desktop entry files items (desktop-file id => absolute filename) used for
   * resolving the menu items */
  GHashTable *files;

} FrapMenuParseInfo;



/* Property identifiers */
enum
{
  PROP_0,
  PROP_ENVIRONMENT,
  PROP_FILENAME,
  PROP_NAME,
  PROP_DIRECTORY,
  PROP_DIRECTORY_DIRS, /* TODO */
  PROP_LEGACY_DIRS, /* TODO */
  PROP_APP_DIRS, /* TODO Implement methods for this! */
  PROP_PARENT, /* TODO */
  PROP_ONLY_UNALLOCATED,
  PROP_DELETED,
};



static void               frap_menu_class_init                             (FrapMenuClass         *klass);
static void               frap_menu_instance_init                          (FrapMenu              *menu);
static void               frap_menu_finalize                               (GObject               *object);
static void               frap_menu_get_property                           (GObject               *object,
                                                                            guint                  prop_id,
                                                                            GValue                *value,
                                                                            GParamSpec            *pspec);
static void               frap_menu_set_property                           (GObject               *object,
                                                                            guint                  prop_id,
                                                                            const GValue          *value,
                                                                            GParamSpec            *pspec);

static gboolean           frap_menu_load                                   (FrapMenu              *menu,
                                                                            GError               **error);
static void               frap_menu_start_element                          (GMarkupParseContext   *context,
                                                                            const gchar           *element_name,
                                                                            const gchar          **attribute_names,
                                                                            const gchar          **attribute_values,
                                                                            gpointer               user_data,
                                                                            GError               **error);
static void               frap_menu_end_element                            (GMarkupParseContext   *context,
                                                                            const gchar           *element_name,
                                                                            gpointer               user_data,
                                                                            GError               **error);
static void               frap_menu_characters                             (GMarkupParseContext   *context,
                                                                            const gchar           *text,
                                                                            gsize                  text_len,
                                                                            gpointer               user_data,
                                                                            GError               **error);
static void               frap_menu_parse_info_add_directory_name          (FrapMenuParseInfo     *parse_info,
                                                                            const gchar           *name);
static void               frap_menu_parse_info_free                        (FrapMenuParseInfo     *menu);
static void               frap_menu_parse_info_consolidate_directory_names (FrapMenuParseInfo *parse_info);

static void               frap_menu_add_directory_dir                      (FrapMenu              *menu,
                                                                            const gchar           *dir);
static void               frap_menu_add_default_directory_dirs             (FrapMenu              *menu);
static void               frap_menu_add_app_dir                            (FrapMenu              *menu,
                                                                            const gchar           *dir);
static void               frap_menu_add_legacy_dir                         (FrapMenu              *menu,
                                                                            const gchar           *dir);
static void               frap_menu_add_kde_legacy_dirs                    (FrapMenu              *menu);
static void               frap_menu_add_default_app_dirs                   (FrapMenu              *menu);

static void               frap_menu_resolve_legacy_menus                   (FrapMenu              *menu);
static void               frap_menu_resolve_legacy_menu                    (FrapMenu              *menu,
                                                                            const gchar           *path);
static void               frap_menu_remove_duplicates                      (FrapMenu              *menu);
static void               frap_menu_consolidate_child_menus                (FrapMenu              *menu);
static void               frap_menu_merge_directory_name                   (const gchar           *name,
                                                                            FrapMenu              *menu);
static void               frap_menu_merge_directory_dir                    (const gchar           *dir,
                                                                            FrapMenu              *menu);
static void               frap_menu_merge_app_dir                          (const gchar           *dir,
                                                                            FrapMenu              *menu);
static void               frap_menu_merge_rule                             (FrapMenuRules         *rule,
                                                                            FrapMenu              *menu);
static void               frap_menu_consolidate_directory_dirs             (FrapMenu              *menu);
static void               frap_menu_consolidate_app_dirs                   (FrapMenu              *menu);
static void               frap_menu_resolve_directory                      (FrapMenu              *menu);
static FrapMenuDirectory *frap_menu_lookup_directory                       (FrapMenu              *menu,
                                                                            const gchar           *filename);
static GSList            *frap_menu_get_rules                              (FrapMenu              *menu);
static void               frap_menu_add_rule                               (FrapMenu              *menu,
                                                                            FrapMenuRules         *rules);
static void               frap_menu_add_move                               (FrapMenu              *menu,
                                                                            FrapMenuMove          *move);
static void               frap_menu_collect_files                          (FrapMenu              *menu);
static void               frap_menu_collect_files_from_path                (FrapMenu              *menu,
                                                                            const gchar           *path,
                                                                            const gchar           *id_prefix);
static void               frap_menu_resolve_items                          (FrapMenu              *menu,
                                                                            gboolean               only_unallocated);
static void               frap_menu_resolve_items_by_rule                  (FrapMenu              *menu,
                                                                            FrapMenuStandardRules *rule);
static void               frap_menu_resolve_item_by_rule                   (const gchar           *desktop_id,
                                                                            const gchar           *filename,
                                                                            FrapMenuPair          *data);
static void               frap_menu_resolve_deleted                        (FrapMenu              *menu);
static void               frap_menu_resolve_moves                          (FrapMenu              *menu);



struct _FrapMenuPrivate
{
  /* Menu filename */
  gchar             *filename;

  /* Menu name */
  gchar             *name;

  /* Directory */
  FrapMenuDirectory *directory;

  /* Submenus */
  GSList            *submenus;

  /* Parent menu */
  FrapMenu          *parent;

  /* Directory dirs */
  GSList            *directory_dirs;

  /* Legacy dirs */
  GSList            *legacy_dirs;

  /* App dirs */
  GSList            *app_dirs;

  /* Only include desktop entries not used in other menus */
  guint              only_unallocated : 1;

  /* Whether this menu should be ignored or not */
  guint              deleted : 1;

  /* Include/exclude rules */
  GSList            *rules;

  /* Move instructions */
  GSList            *moves;

  /* Menu item pool */
  FrapMenuItemPool  *pool;

  /* Shared menu item cache */
  FrapMenuItemCache *cache;

  /* Parse information (used for resolving) */
  FrapMenuParseInfo *parse_info;
};

struct _FrapMenuClass
{
  GObjectClass __parent__;
};

struct _FrapMenu
{
  GObject          __parent__;

  /* < private > */
  FrapMenuPrivate *priv;
};



static GObjectClass *frap_menu_parent_class = NULL;



GType
frap_menu_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (FrapMenuClass),
        NULL,
        NULL,
        (GClassInitFunc) frap_menu_class_init,
        NULL,
        NULL,
        sizeof (FrapMenu),
        0,
        (GInstanceInitFunc) frap_menu_instance_init,
        NULL,
      };

      type = g_type_register_static (G_TYPE_OBJECT, "FrapMenu", &info, 0);
    }

  return type;
}



static void
frap_menu_class_init (FrapMenuClass *klass)
{
  GObjectClass *gobject_class;

  g_type_class_add_private (klass, sizeof (FrapMenuPrivate));

  /* Determine the parent type class */
  frap_menu_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = frap_menu_finalize; 
  gobject_class->get_property = frap_menu_get_property;
  gobject_class->set_property = frap_menu_set_property;

  /**
   * FrapMenu:filename:
   *
   * The filename of an %FrapMenu object. Whenever this is redefined, the
   * menu is reloaded.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILENAME,
                                   g_param_spec_string ("filename",
                                                        _("Filename"),
                                                        _("XML menu filename"),
                                                        NULL,
                                                        G_PARAM_READWRITE));

  /**
   * FrapMenu:name:
   *
   * The name of the menu.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        _("Name"),
                                                        _("Menu name"),
                                                        NULL,
                                                        G_PARAM_READWRITE));

  /**
   * FrapMenu:directory:
   *
   * The directory entry associated with this menu. 
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_DIRECTORY,
                                   g_param_spec_object ("directory",
                                                        _("Directory"),
                                                        _("Directory entry associated with this menu"),
                                                        FRAP_TYPE_MENU_DIRECTORY,
                                                        G_PARAM_READWRITE));

  /**
   * FrapMenu:only-unallocated:
   *
   * Whether this menu should only contain desktop entries not used by other
   * menus.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ONLY_UNALLOCATED,
                                   g_param_spec_boolean ("only-unallocated",
                                                        _("Only unallocated"),
                                                        _("Whether this menu only contains unallocated entries"),
                                                        FALSE,
                                                        G_PARAM_READWRITE));

  /**
   * FrapMenu:deleted:
   *
   * Whether this menu should be ignored.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ONLY_UNALLOCATED,
                                   g_param_spec_boolean ("deleted",
                                                        _("Deleted"),
                                                        _("Whether this menu should be ignored"),
                                                        FALSE,
                                                        G_PARAM_READWRITE));
}



static void
frap_menu_instance_init (FrapMenu *menu)
{
  menu->priv = FRAP_MENU_GET_PRIVATE (menu);
  menu->priv->filename = NULL;
  menu->priv->name = NULL;
  menu->priv->directory = NULL;
  menu->priv->submenus = NULL;
  menu->priv->parent = NULL;
  menu->priv->directory_dirs = NULL;
  menu->priv->legacy_dirs = NULL;
  menu->priv->app_dirs = NULL;
  menu->priv->only_unallocated = FALSE;
  menu->priv->rules = NULL;
  menu->priv->moves = NULL;
  menu->priv->pool = frap_menu_item_pool_new ();

  /* Take reference on the menu item cache */
  menu->priv->cache = frap_menu_item_cache_get_default ();

  menu->priv->parse_info = g_new (FrapMenuParseInfo, 1);
  menu->priv->parse_info->directory_names = NULL;
  menu->priv->parse_info->files = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
}



static void
frap_menu_finalize (GObject *object)
{
  FrapMenu *menu = FRAP_MENU (object);

  /* Free filename */
  g_free (menu->priv->filename);

  /* Free name */
  g_free (menu->priv->name);

  /* Free directory */
  if (G_LIKELY (menu->priv->directory != NULL))
    g_object_unref (menu->priv->directory);

  /* Free directory dirs */
  g_slist_foreach (menu->priv->directory_dirs, (GFunc) g_free, NULL);
  g_slist_free (menu->priv->directory_dirs);

  /* Free legacy dirs (check if this is the best way to free the list) */
  g_slist_foreach (menu->priv->legacy_dirs, (GFunc) g_free, NULL);
  g_slist_free (menu->priv->legacy_dirs);

  /* Free app dirs */
  g_slist_foreach (menu->priv->app_dirs, (GFunc) g_free, NULL);
  g_slist_free (menu->priv->app_dirs);

  /* TODO Free submenus etc. */
  g_slist_foreach (menu->priv->submenus, (GFunc) g_object_unref, NULL);
  g_slist_free (menu->priv->submenus);

  /* Free rules */
  g_slist_foreach (menu->priv->rules, (GFunc) g_object_unref, NULL);
  g_slist_free (menu->priv->rules);

  /* Free move instructions */
  g_slist_foreach (menu->priv->moves, (GFunc) g_object_unref, NULL);
  g_slist_free (menu->priv->moves);

  /* Free item pool */
  g_object_unref (G_OBJECT (menu->priv->pool));

  /* Release item cache reference */
  g_object_unref (G_OBJECT (menu->priv->cache));

  /* Free parse information */
  frap_menu_parse_info_free (menu->priv->parse_info);

  (*G_OBJECT_CLASS (frap_menu_parent_class)->finalize) (object);
}



static void
frap_menu_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  FrapMenu *menu = FRAP_MENU (object);

  switch (prop_id)
    {
    case PROP_FILENAME:
      g_value_set_string (value, frap_menu_get_filename (menu));
      break;

    case PROP_NAME:
      g_value_set_string (value, frap_menu_get_name (menu));
      break;

    case PROP_DIRECTORY:
      g_value_set_object (value, frap_menu_get_directory (menu));
      break;

    case PROP_ONLY_UNALLOCATED:
      g_value_set_boolean (value, frap_menu_get_only_unallocated (menu));
      break;

    case PROP_DELETED:
      g_value_set_boolean (value, frap_menu_get_deleted (menu));
      break;
    
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
frap_menu_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  FrapMenu *menu = FRAP_MENU (object);

  switch (prop_id)
    {
    case PROP_FILENAME:
      frap_menu_set_filename (menu, g_value_get_string (value));
      break;

    case PROP_NAME:
      frap_menu_set_name (menu, g_value_get_string (value));
      break;

    case PROP_DIRECTORY:
      frap_menu_set_directory (menu, g_value_get_object (value));
      break;

    case PROP_ONLY_UNALLOCATED:
      frap_menu_set_only_unallocated (menu, g_value_get_boolean (value));
      break;

    case PROP_DELETED:
      frap_menu_set_deleted (menu, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


/**
 * frap_menu_get_root:
 * @error : Return location for errors or %NULL.
 *
 * Returns a pointer to the system root menu. If the root menu is not in memory
 * already, it is loaded from disk, which may take some time as it involves
 * parsing and merging a lot of files. So if you call this function from a GUI
 * program it should be done in a way that won't block the user interface (e.g.
 * by using a worker thread).
 * The returned pointer needs to be unref'd when it is not used anymore.
 *
 * Return value: Pointer to the system root menu. This pointer needs to be
 *               unref'd later.
 **/
FrapMenu*
frap_menu_get_root (GError **error)
{
  static FrapMenu *root_menu = NULL;
  gchar           *filename;
  guint            n;

  if (G_UNLIKELY (root_menu == NULL))
    {
      /* Search for a usable root menu file */
      for (n = 0; n < G_N_ELEMENTS (FRAP_MENU_ROOT_SPECS) && root_menu == NULL; ++n)
        {
          /* Search for the root menu file */
          filename = xfce_resource_lookup (XFCE_RESOURCE_CONFIG, FRAP_MENU_ROOT_SPECS[n]);
          if (G_UNLIKELY (filename == NULL))
            continue;

          /* Try to load the root menu from this file */
          root_menu = frap_menu_new (filename, NULL);
          if (G_LIKELY (root_menu != NULL))
            {
              /* Add weak pointer on the menu */
              g_object_add_weak_pointer (G_OBJECT (root_menu), (gpointer) &root_menu);
            }

          /* Free filename string */
          g_free (filename);
        }

      /* Check if we failed to load the root menu */
      if (G_UNLIKELY (root_menu == NULL))
        {
          /* Let the caller know there was no suitable file */
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, _("Failed to locate the application root menu"));
        }
    }
  else
    g_object_ref (G_OBJECT (root_menu));
  
  return root_menu;
}



/**
 * frap_menu_new:
 * @filename : filename containing the menu structure you want to load.
 * @error    : return location for errors or %NULL.
 *
 * Parses a file and returns the menu structure found in this file. This
 * may involve parsing and merging of a lot of other files. So if you call this
 * function from a GUI program it should be done in a way that won't block the
 * user interface (e.g. by using a worker thread).
 * The returned pointer needs to be unref'd when it is not used anymore.
 *
 * Return value: Menu structure found in @filename.
 **/
FrapMenu*
frap_menu_new (const gchar *filename, 
               GError     **error)
{
  FrapMenu *menu;

  g_return_val_if_fail (g_path_is_absolute (filename), NULL);

  /* Create new menu */
  menu = g_object_new (FRAP_TYPE_MENU, "filename", filename, NULL);

  /* Try to load the menu structure */
  if (!frap_menu_load (menu, error))
    {
      g_object_unref (G_OBJECT (menu));
      return NULL;
    }

  return menu;
}



const gchar*
frap_menu_get_filename (FrapMenu *menu)
{
  g_return_val_if_fail (FRAP_IS_MENU (menu), NULL);
  return menu->priv->filename;
}



void
frap_menu_set_filename (FrapMenu *menu, const gchar *filename)
{
  g_return_if_fail (FRAP_IS_MENU (menu));

  /* Abort if filenames are equal */
  if (G_UNLIKELY (menu->priv->filename != NULL))
    {
      if (G_UNLIKELY (filename != NULL && g_utf8_collate (filename, menu->priv->filename) == 0))
        return;

      /* Free old filename */
      g_free (menu->priv->filename);
    }

  /* Set the new filename */
  menu->priv->filename = g_strdup (filename);

  /* Notify listeners */
  g_object_notify (G_OBJECT (menu), "filename");
}



const gchar*
frap_menu_get_name (FrapMenu *menu)
{
  g_return_val_if_fail (FRAP_IS_MENU (menu), NULL);
  return menu->priv->name;
}



void
frap_menu_set_name (FrapMenu    *menu, 
                    const gchar *name)
{
  g_return_if_fail (FRAP_IS_MENU (menu));
  g_return_if_fail (name != NULL);

  /* Abort if names are equal */
  if (G_UNLIKELY (menu->priv->name != NULL))
    {
      if (G_UNLIKELY (g_utf8_collate (name, menu->priv->name) == 0))
        return;

      /* Free old name */
      g_free (menu->priv->name);
    }

  /* Set the new filename */
  menu->priv->name = g_strdup (name);

  /* Notify listeners */
  g_object_notify (G_OBJECT (menu), "name");
}



FrapMenuDirectory*
frap_menu_get_directory (FrapMenu *menu)
{
  g_return_val_if_fail (FRAP_IS_MENU (menu), NULL);
  return menu->priv->directory;
}



void
frap_menu_set_directory (FrapMenu          *menu,
                         FrapMenuDirectory *directory)
{
  g_return_if_fail (FRAP_IS_MENU (menu));
  g_return_if_fail (FRAP_IS_MENU_DIRECTORY (directory));

  /* Abort if directories (TODO: and their locations) are equal */
  if (G_UNLIKELY (directory == menu->priv->directory))
    return;
  
  /* Destroy old directory */
  if (G_UNLIKELY (menu->priv->directory != NULL))
    g_object_unref (menu->priv->directory);

  /* Remove the floating reference and acquire a normal one */
#if GLIB_CHECK_VERSION(2,10,0)
  g_object_ref_sink (G_OBJECT (directory));
#else
  g_object_ref (G_OBJECT (directory));
#endif

  /* Set the new directory */
  menu->priv->directory = directory;

  /* Notify listeners */
  g_object_notify (G_OBJECT (menu), "directory");
}



gboolean
frap_menu_get_only_unallocated (FrapMenu *menu)
{
  g_return_val_if_fail (FRAP_IS_MENU (menu), FALSE);
  return menu->priv->only_unallocated;
}



void
frap_menu_set_only_unallocated (FrapMenu *menu,
                                gboolean  only_unallocated)
{
  g_return_if_fail (FRAP_IS_MENU (menu));

  /* Abort if values are equal */
  if (menu->priv->only_unallocated == only_unallocated)
    return;

  /* Set new value */
  menu->priv->only_unallocated = only_unallocated;

  /* Notify listeners */
  g_object_notify (G_OBJECT (menu), "only-unallocated");
}



gboolean
frap_menu_get_deleted (FrapMenu *menu)
{
  g_return_val_if_fail (FRAP_IS_MENU (menu), FALSE);
  return menu->priv->deleted;
}



void
frap_menu_set_deleted (FrapMenu *menu,
                       gboolean  deleted)
{
  g_return_if_fail (FRAP_IS_MENU (menu));

  /* Abort if values are equal */
  if (menu->priv->deleted == deleted)
    return;

  /* Set new value */
  menu->priv->deleted = deleted;

  /* Notify listeners */
  g_object_notify (G_OBJECT (menu), "deleted");
}



GSList*
frap_menu_get_directory_dirs (FrapMenu *menu)
{
  FrapMenu *current_menu;
  GSList   *directories = NULL;
  GList    *menus = NULL;
  GList    *iter;
  GSList    *iter2;
  
  g_return_val_if_fail (FRAP_IS_MENU (menu), NULL);

  /* Collect all menus from menu -> parent -> ... -> root */
  for (current_menu = menu; current_menu != NULL; current_menu = current_menu->priv->parent)
    menus = g_list_prepend (menus, current_menu);

  /* Iterate over all menus from root -> parent -> ... -> menu */
  for (iter = menus; iter != NULL; iter = g_list_next (iter))
    {
      /* Fetch current menu */
      current_menu = FRAP_MENU (iter->data);

      /* Iterate over all directory dirs */
      for (iter2 = current_menu->priv->directory_dirs; iter2 != NULL; iter2 = g_slist_next (iter2))
        {
          /* Append directory dir to the list */
          directories = g_slist_append (directories, iter2->data);
        }

      /* Free the directory dir list */
      g_slist_free (iter2);
    }

  /* Free menu list */
  g_list_free (menus);

  return directories;
}



GSList*
frap_menu_get_legacy_dirs (FrapMenu *menu)
{
  /* FIXME: Collecting legacy dirs from the bottom up might be wrong. Perhaps
   *        only <Menu> items with <LegacyDir> elements are allowed to parse
   *        legacy menu hierarchies - verify this!
   */

  FrapMenu *current_menu;
  GSList   *directories = NULL;
  GList    *menus = NULL;
  GList    *iter;
  GSList    *iter2;
  
  g_return_val_if_fail (FRAP_IS_MENU (menu), NULL);

  /* Collect all menus from menu -> parent -> ... -> root */
  for (current_menu = menu; current_menu != NULL; current_menu = current_menu->priv->parent)
    menus = g_list_prepend (menus, current_menu);

  /* Iterate over all menus from root -> parent -> ... -> menu */
  for (iter = menus; iter != NULL; iter = g_list_next (iter))
    {
      /* Fetch current menu */
      current_menu = FRAP_MENU (iter->data);

      /* Iterate over all legacy directories */
      for (iter2 = current_menu->priv->legacy_dirs; iter2 != NULL; iter2 = g_slist_next (iter2))
        {
          /* Append legacy dir to the list */
          directories = g_slist_append (directories, iter2->data);
        }

      /* Free the legacy dir list */
      g_slist_free (iter2);
    }

  /* Free menu list */
  g_list_free (menus);

  return directories;
}



GSList*
frap_menu_get_app_dirs (FrapMenu *menu)
{
  FrapMenu *current_menu;
  GSList   *directories = NULL;
  GList    *menus = NULL;
  GList    *iter;
  GSList    *iter2;
  
  g_return_val_if_fail (FRAP_IS_MENU (menu), NULL);

  /* Collect all menus from menu -> parent -> ... -> root */
  for (current_menu = menu; current_menu != NULL; current_menu = current_menu->priv->parent)
    menus = g_list_prepend (menus, current_menu);

  /* Iterate over all menus from root -> parent -> ... -> menu */
  for (iter = menus; iter != NULL; iter = g_list_next (iter))
    {
      /* Fetch current menu */
      current_menu = FRAP_MENU (iter->data);

      /* Iterate over all application directories */
      for (iter2 = current_menu->priv->app_dirs; iter2 != NULL; iter2 = g_slist_next (iter2))
        {
          /* Append app dir to the list */
          directories = g_slist_append (directories, iter2->data);
        }

      /* Free the app dir list */
      g_slist_free (iter2);
    }

  /* Free menu list */
  g_list_free (menus);

  return directories;
}



static gboolean
frap_menu_load (FrapMenu *menu, GError **error)
{
  /* Parser structure (connect handlers etc.) */
  GMarkupParseContext *context;
  GMarkupParser parser = {
      frap_menu_start_element,
      frap_menu_end_element,
      frap_menu_characters,
      NULL,
      NULL
  };
  FrapMenuParseContext menu_context;

  /* File content information */
  gchar *contents;
  gsize contents_length;
  GIOStatus status;
  GIOChannel *stream;

  g_return_val_if_fail (FRAP_IS_MENU (menu), FALSE);
  g_return_val_if_fail (menu->priv->filename != NULL, FALSE);

  /* Try to open the menu file */
  stream = g_io_channel_new_file (menu->priv->filename, "r", error);

  if (G_UNLIKELY (stream == NULL))
    return FALSE;

  /* Try to read the menu file */
  status = g_io_channel_read_to_end (stream, &contents, &contents_length, error);
  
  /* Free IO handle */
  g_io_channel_unref (stream);

  if (G_UNLIKELY (status != G_IO_STATUS_NORMAL))
    return FALSE;

  /* Define menu parse context */
  menu_context.root_menu = menu;
  menu_context.state = FRAP_MENU_PARSE_STATE_START;
  menu_context.state = FRAP_MENU_PARSE_NODE_TYPE_NONE;
  menu_context.menu_stack = NULL;
  menu_context.rule_stack = NULL;
  menu_context.move = NULL;

  /* Allocate parse context */
  context = g_markup_parse_context_new (&parser, 0, &menu_context, NULL);

  /* Try to parse the menu file */
  if (!g_markup_parse_context_parse (context, contents, contents_length, error) || !g_markup_parse_context_end_parse (context, error))
    {
      g_markup_parse_context_free (context);
      return FALSE;
    }
  
  /* Free file contents */
  g_free (contents);

  /* Free parse context */
  g_markup_parse_context_free (context);

  /* Free menu parse context */
  g_list_free (menu_context.menu_stack);
  g_list_free (menu_context.rule_stack);

#if 0
  frap_menu_resolve_legacy_menus (menu);
#endif
  frap_menu_remove_duplicates (menu);
  frap_menu_resolve_directory (menu);
  frap_menu_resolve_moves (menu);

  /* Collect all potential menu item filenames */
  frap_menu_collect_files (menu);

  /* Resolve menu items in two steps to handle <OnlyUnallocated/> properly */
  frap_menu_resolve_items (menu, FALSE);
  frap_menu_resolve_items (menu, TRUE);
  
  frap_menu_resolve_deleted (menu);

  return TRUE;
}



static void
frap_menu_start_element (GMarkupParseContext *context,
                         const gchar         *element_name,
                         const gchar        **attribute_names,
                         const gchar        **attribute_values,
                         gpointer             user_data,
                         GError             **error)
{
  FrapMenuParseContext *menu_context = (FrapMenuParseContext *)user_data;
  FrapMenu             *current_menu;
  FrapMenuRules        *current_rule;

  switch (menu_context->state) 
    {
    case FRAP_MENU_PARSE_STATE_START:
      if (g_utf8_collate (element_name, "Menu") == 0)
        {
          menu_context->state = FRAP_MENU_PARSE_STATE_ROOT;
          menu_context->menu_stack = g_list_prepend (menu_context->menu_stack, menu_context->root_menu);
        }
      break;

    case FRAP_MENU_PARSE_STATE_ROOT:
    case FRAP_MENU_PARSE_STATE_MENU:
      /* Fetch current menu from stack */
      current_menu = g_list_first (menu_context->menu_stack)->data;

      if (g_utf8_collate (element_name, "Name") == 0)
        menu_context->node_type = FRAP_MENU_PARSE_NODE_TYPE_NAME;

      else if (g_utf8_collate (element_name, "Directory") == 0)
        menu_context->node_type = FRAP_MENU_PARSE_NODE_TYPE_DIRECTORY;
      else if (g_utf8_collate (element_name, "DirectoryDir") == 0)
        menu_context->node_type = FRAP_MENU_PARSE_NODE_TYPE_DIRECTORY_DIR;
      else if (g_utf8_collate (element_name, "DefaultDirectoryDirs") == 0)
        frap_menu_add_default_directory_dirs (current_menu);

      else if (g_utf8_collate (element_name, "DefaultAppDirs") == 0)
        frap_menu_add_default_app_dirs (current_menu);
      else if (g_utf8_collate (element_name, "AppDir") == 0)
        menu_context->node_type = FRAP_MENU_PARSE_NODE_TYPE_APP_DIR;

      else if (g_utf8_collate (element_name, "KDELegacyDirs") == 0)
        frap_menu_add_kde_legacy_dirs (current_menu);
      else if (g_utf8_collate (element_name, "LegacyDir") == 0)
        menu_context->node_type = FRAP_MENU_PARSE_NODE_TYPE_LEGACY_DIR;

      else if (g_utf8_collate (element_name, "OnlyUnallocated") == 0)
        frap_menu_set_only_unallocated (current_menu, TRUE);
      else if (g_utf8_collate (element_name, "NotOnlyUnallocated") == 0)
        frap_menu_set_only_unallocated (current_menu, FALSE);

      else if (g_utf8_collate (element_name, "Deleted") == 0)
        frap_menu_set_deleted (current_menu, TRUE);
      else if (g_utf8_collate (element_name, "NotDeleted") == 0)
        frap_menu_set_deleted (current_menu, FALSE);

      else if (g_utf8_collate (element_name, "Include") == 0)
        {
          /* Create include rule */
          FrapMenuOrRules *rule = frap_menu_or_rules_new ();

          /* Set include property */
          frap_menu_standard_rules_set_include (FRAP_MENU_STANDARD_RULES (rule), TRUE);

          /* Add rule to the menu */
          frap_menu_add_rule (current_menu, FRAP_MENU_RULES (rule));

          /* Put rule to the stack */
          menu_context->rule_stack = g_list_prepend (menu_context->rule_stack, rule);

          /* Set parse state */
          menu_context->state = FRAP_MENU_PARSE_STATE_RULE;
        }
      else if (g_utf8_collate (element_name, "Exclude") == 0)
        {
          /* Create exclude rule */
          FrapMenuOrRules *rule = frap_menu_or_rules_new ();

          /* Set include property */
          frap_menu_standard_rules_set_include (FRAP_MENU_STANDARD_RULES (rule), FALSE);

          /* Add rule to the menu */
          frap_menu_add_rule (current_menu, FRAP_MENU_RULES (rule));

          /* Put rule to the stack */
          menu_context->rule_stack = g_list_prepend (menu_context->rule_stack, rule);

          /* Set parse state */
          menu_context->state = FRAP_MENU_PARSE_STATE_RULE;
        }

      else if (g_utf8_collate (element_name, "Menu") == 0)
        {
          /* Create new menu */
          FrapMenu *menu = g_object_new (FRAP_TYPE_MENU, "filename", current_menu->priv->filename, NULL);

          /* Add menu as submenu to the current menu */
          frap_menu_add_menu (current_menu, menu); 

          /* Set parse state */
          menu_context->state = FRAP_MENU_PARSE_STATE_MENU;

          /* Push new menu to the stack */
          menu_context->menu_stack = g_list_prepend (menu_context->menu_stack, menu);
        }

      else if (g_utf8_collate (element_name, "Move") == 0)
        {
          /* Set parse state */
          menu_context->state = FRAP_MENU_PARSE_STATE_MOVE;
        }
      
      break;

    case FRAP_MENU_PARSE_STATE_RULE:
      /* Fetch current rule from stack */
      current_rule = FRAP_MENU_RULES (g_list_first (menu_context->rule_stack)->data);

      if (g_utf8_collate (element_name, "All") == 0)
        {
          frap_menu_rules_add_all (current_rule);
        }
      else if (g_utf8_collate (element_name, "Filename") == 0)
        {
          menu_context->node_type = FRAP_MENU_PARSE_NODE_TYPE_FILENAME;
        }
      else if (g_utf8_collate (element_name, "Category") == 0)
        {
          menu_context->node_type = FRAP_MENU_PARSE_NODE_TYPE_CATEGORY;
        }
      else if (g_utf8_collate (element_name, "And") == 0)
        {
          /* Create include rule */
          FrapMenuAndRules *rule = frap_menu_and_rules_new ();

          /* Add rule to the current rule */
          frap_menu_rules_add_rules (current_rule, FRAP_MENU_RULES (rule));

          /* Put rule to the stack */
          menu_context->rule_stack = g_list_prepend (menu_context->rule_stack, rule);
        }
      else if (g_utf8_collate (element_name, "Or") == 0)
        {
          /* Create include rule */
          FrapMenuOrRules *rule = frap_menu_or_rules_new ();

          /* Add rule to the current rule */
          frap_menu_rules_add_rules (current_rule, FRAP_MENU_RULES (rule));

          /* Put rule to the stack */
          menu_context->rule_stack = g_list_prepend (menu_context->rule_stack, rule);
        }
      else if (g_utf8_collate (element_name, "Not") == 0)
        {
          /* Create include rule */
          FrapMenuNotRules *rule = frap_menu_not_rules_new ();

          /* Add rule to the current rule */
          frap_menu_rules_add_rules (current_rule, FRAP_MENU_RULES (rule));

          /* Put rule to the stack */
          menu_context->rule_stack = g_list_prepend (menu_context->rule_stack, rule);
        }

      break;

    case FRAP_MENU_PARSE_STATE_MOVE:
      /* Fetch current menu from stack */
      current_menu = g_list_first (menu_context->menu_stack)->data;

      if (g_utf8_collate (element_name, "Old") == 0)
        {
          /* Create a new move instruction in the parse context */
          menu_context->move = frap_menu_move_new ();

          /* Add move instruction to the menu */
          frap_menu_add_move (current_menu, menu_context->move);

          menu_context->node_type = FRAP_MENU_PARSE_NODE_TYPE_OLD;
        }
      else if (g_utf8_collate (element_name, "New") == 0)
        menu_context->node_type = FRAP_MENU_PARSE_NODE_TYPE_NEW;

      break;
    }
}



static void 
frap_menu_end_element (GMarkupParseContext *context,
                       const gchar         *element_name,
                       gpointer             user_data,
                       GError             **error)
{
  FrapMenuParseContext *menu_context = (FrapMenuParseContext *)user_data;

  switch (menu_context->state)
    {
    case FRAP_MENU_PARSE_STATE_ROOT:
      if (g_utf8_collate (element_name, "Menu") == 0)
        {
          /* Remove root menu from stack */
          menu_context->menu_stack = g_list_delete_link (menu_context->menu_stack, g_list_first (menu_context->menu_stack));

          /* Set parser state */
          menu_context->state = FRAP_MENU_PARSE_STATE_END;
        }
      break;

    case FRAP_MENU_PARSE_STATE_MENU:
      if (g_utf8_collate (element_name, "Menu") == 0)
        {
          /* Remove current menu from stack */
          menu_context->menu_stack = g_list_delete_link (menu_context->menu_stack, g_list_first (menu_context->menu_stack));

          /* Set parse state to _STATE_ROOT only if there are no other menus
           * left on the stack. Otherwise, we're still inside a <Menu> element. */
          if (G_LIKELY (g_list_length (menu_context->menu_stack) == 1))
            menu_context->state = FRAP_MENU_PARSE_STATE_ROOT;
        }
      break;

    case FRAP_MENU_PARSE_STATE_RULE:
      if (g_utf8_collate (element_name, "Include") == 0 || g_utf8_collate (element_name, "Exclude") == 0 || g_utf8_collate (element_name, "Or") == 0 || g_utf8_collate (element_name, "And") == 0 || g_utf8_collate (element_name, "Not") == 0)
        {
          /* Remove current rule from stack */
          menu_context->rule_stack = g_list_delete_link (menu_context->rule_stack, g_list_first (menu_context->rule_stack));

          /* Set parse state */
          if (g_list_length (menu_context->rule_stack) == 0)
            {
              if (g_list_length (menu_context->menu_stack) > 1) 
                menu_context->state = FRAP_MENU_PARSE_STATE_MENU;
              else
                menu_context->state = FRAP_MENU_PARSE_STATE_ROOT;
            }
        }
      break;

    case FRAP_MENU_PARSE_STATE_MOVE:
      if (g_utf8_collate (element_name, "Move") == 0)
        {
          /* Set menu parse state */
          menu_context->state = FRAP_MENU_PARSE_STATE_MENU;

          /* Handle incomplete move commands (those missing a <New> element) */
          if (G_UNLIKELY (menu_context->move != NULL && frap_menu_move_get_new (menu_context->move) == NULL))
            {
              /* Determine current menu */
              FrapMenu *current_menu = FRAP_MENU (g_list_first (menu_context->menu_stack)->data);

              /* Print warning */
              g_warning ("Ignoring <Old>%s</Old>", frap_menu_move_get_old (menu_context->move));

              /* Remove move command from the menu */
              current_menu->priv->moves = g_slist_remove (current_menu->priv->moves, menu_context->move);

              /* Free the move command */
              g_object_unref (menu_context->move);
            }
        }
      else if (g_utf8_collate (element_name, "New") == 0)
        menu_context->move = NULL;
      break;
    }
}



static void
frap_menu_characters (GMarkupParseContext *context,
                      const gchar         *text,
                      gsize                text_len,
                      gpointer             user_data,
                      GError             **error)
{
  FrapMenuParseContext *menu_context = (FrapMenuParseContext *)user_data;
  FrapMenu             *current_menu = g_list_first (menu_context->menu_stack)->data;
  FrapMenuRules        *current_rule = NULL;

  /* Generate NULL-terminated string */
  gchar *content = g_strndup (text, text_len);

  /* Fetch current rule from stack (if possible) */
  if (g_list_length (menu_context->rule_stack) > 0)
    current_rule = g_list_first (menu_context->rule_stack)->data;

  switch (menu_context->node_type)
    {
    case FRAP_MENU_PARSE_NODE_TYPE_NAME:
      frap_menu_set_name (current_menu, content);
      break;

    case FRAP_MENU_PARSE_NODE_TYPE_DIRECTORY:
      frap_menu_parse_info_add_directory_name (current_menu->priv->parse_info, content);
      break;

    case FRAP_MENU_PARSE_NODE_TYPE_DIRECTORY_DIR:
      frap_menu_add_directory_dir (current_menu, content);
      break;

    case FRAP_MENU_PARSE_NODE_TYPE_APP_DIR:
      frap_menu_add_app_dir (current_menu, content);
      break;

    case FRAP_MENU_PARSE_NODE_TYPE_LEGACY_DIR:
      frap_menu_add_legacy_dir (current_menu, content);
      break;

    case FRAP_MENU_PARSE_NODE_TYPE_FILENAME:
      if (G_LIKELY (current_rule != NULL))
        frap_menu_rules_add_filename (current_rule, content);
      break;

    case FRAP_MENU_PARSE_NODE_TYPE_CATEGORY:
      if (G_LIKELY (current_rule != NULL))
        frap_menu_rules_add_category (current_rule, content);
      break;

    case FRAP_MENU_PARSE_NODE_TYPE_OLD:
      frap_menu_move_set_old (menu_context->move, content);
      break;

    case FRAP_MENU_PARSE_NODE_TYPE_NEW:
      if (G_LIKELY (menu_context->move != NULL))
        frap_menu_move_set_new (menu_context->move, content);
      else
        g_warning ("Ignoring <New>%s</New>", content);
      break;
    }

  /* Free string */
  g_free (content);

  /* Invalidate node type information */
  menu_context->node_type = FRAP_MENU_PARSE_NODE_TYPE_NONE;
}



static void
frap_menu_parse_info_add_directory_name (FrapMenuParseInfo *parse_info,
                                         const gchar       *name)
{
  g_return_if_fail (name != NULL);
  parse_info->directory_names = g_slist_append (parse_info->directory_names, g_strdup (name));
}



static void
frap_menu_parse_info_consolidate_directory_names (FrapMenuParseInfo *parse_info)
{
  GSList *names = NULL;
  GSList *iter;

  g_return_if_fail (parse_info != NULL);

  /* Iterate over directory names in reverse order */
  for (iter = g_slist_reverse (parse_info->directory_names); iter != NULL; iter = g_slist_next (iter))
    {
      /* Prepend name to the new list unless it already exists */
      if (G_LIKELY (g_slist_find_custom (names, iter->data, (GCompareFunc) g_utf8_collate) == NULL))
        names = g_slist_prepend (names, g_strdup (iter->data));
    }

  /* Free old list */
  g_slist_foreach (parse_info->directory_names, (GFunc) g_free, NULL);
  g_slist_free (parse_info->directory_names);

  parse_info->directory_names = names;
}



static void
frap_menu_parse_info_free (FrapMenuParseInfo *parse_info)
{
  g_return_if_fail (parse_info != NULL);

  /* Free directory names */
  g_slist_foreach (parse_info->directory_names, (GFunc) g_free, NULL);
  g_slist_free (parse_info->directory_names);

#if GLIB_CHECK_VERSION(2,12,0)
  g_hash_table_unref (parse_info->files);
#else
  g_hash_table_destroy (parse_info->files);
#endif

  /* Free parse info */
  g_free (parse_info);
}



static void
frap_menu_add_directory_dir (FrapMenu    *menu,
                             const gchar *dir)
{
  /* Absolute path of the directory (free'd by the menu instance later) */
  gchar *path;

  g_return_if_fail (FRAP_IS_MENU (menu));
  g_return_if_fail (dir != NULL);

  if (!g_path_is_absolute (dir))
    {
      /* Determine the absolute path (directory) of the menu file */
      gchar *dirname = g_path_get_dirname (menu->priv->filename);

      /* Construct absolute path */
      path = g_build_path (G_DIR_SEPARATOR_S, dirname, dir, NULL);

      /* Free absolute menu file directory path */
      g_free (dirname);
    }
  else
    path = g_strdup (dir);

  /* Append path */
  menu->priv->directory_dirs = g_slist_append (menu->priv->directory_dirs, path);
}



static void
frap_menu_add_default_directory_dirs (FrapMenu *menu)
{
  int          i;
  gchar       *path;
  gchar       *kde_data_dir;
  const gchar *kde_dir;

  const gchar * const *dirs;

  g_return_if_fail (FRAP_IS_MENU (menu));

  /* Append $KDEDIR/share/desktop-directories as a workaround for distributions 
   * not installing KDE menu files properly into $XDG_DATA_DIR */

  /* Get KDEDIR environment variable */
  kde_dir = g_getenv ("KDEDIR");

  /* Check if this variable is set */
  if (G_UNLIKELY (kde_dir != NULL))
    {
      /* Build KDE data dir */
      kde_data_dir = g_build_filename (kde_dir, "share", "desktop-directories", NULL);

      /* Add it as a directory dir if it exists */
      if (G_LIKELY (g_file_test (kde_data_dir, G_FILE_TEST_IS_DIR)))
        frap_menu_add_directory_dir (menu, kde_data_dir);

      /* Free the KDE data dir */
      g_free (kde_data_dir);
    }

  /* The $KDEDIR workaround ends here */

  /* Append system-wide data dirs */
  dirs = g_get_system_data_dirs ();
  for (i = 0; dirs[i] != NULL; i++)
    {
      path = g_build_path (G_DIR_SEPARATOR_S, dirs[i], "desktop-directories", NULL);
      frap_menu_add_directory_dir (menu, path);
      g_free (path);
    }

  /* Append user data dir */
  path = g_build_path (G_DIR_SEPARATOR_S, g_get_user_data_dir (), "desktop-directories", NULL);
  frap_menu_add_directory_dir (menu, path);
  g_free (path);
}



static void
frap_menu_add_legacy_dir (FrapMenu    *menu,
                          const gchar *dir)
{
  /* Absolute path of the directory (free'd by the menu instance later) */
  gchar *path;

  g_return_if_fail (FRAP_IS_MENU (menu));
  g_return_if_fail (menu->priv->filename != NULL);
  g_return_if_fail (dir != NULL);

  if (!g_path_is_absolute (dir))
    {
      /* Determine the absolute path (directory) of the menu file */
      gchar *dirname = g_path_get_dirname (menu->priv->filename);

      /* Construct absolute path */
      path = g_build_path (G_DIR_SEPARATOR_S, dirname, dir, NULL);

      /* Free absolute menu file directory path */
      g_free (dirname);
    }
  else
    path = g_strdup (dir);

  /* Check if there already are legacy dirs */
  if (G_LIKELY (menu->priv->legacy_dirs != NULL))
    {
      /* Remove all previous occurences of the directory from the list */
      /* TODO: This probably is rather dirty and should be replaced with a more
       * clean algorithm. */
      GSList *iter = menu->priv->legacy_dirs;
      while (iter != NULL) 
        {
          gchar *data = (gchar *)iter->data;
          if (g_utf8_collate (data, dir) == 0)
            {
              GSList *tmp = g_slist_next (iter);
              menu->priv->app_dirs = g_slist_remove_link (menu->priv->legacy_dirs, iter);
              iter = tmp;
            }
          else
            iter = iter->next;
        }
      
      /* Append directory */
      menu->priv->legacy_dirs = g_slist_append (menu->priv->legacy_dirs, path);
    }
  else
    {
      /* Create new GSList and append the absolute path of the directory */
      menu->priv->legacy_dirs = g_slist_append (menu->priv->legacy_dirs, path);
    }
}



static void
frap_menu_add_kde_legacy_dirs (FrapMenu *menu)
{
  static gchar **kde_legacy_dirs = NULL;

  g_return_if_fail (FRAP_IS_MENU (menu));

  if (G_UNLIKELY (kde_legacy_dirs == NULL))
    {
      gchar       *std_out;
      gchar       *std_err;
      gint         status;
      GError      *error = NULL;
      const gchar *kde_dir = g_getenv ("KDEDIR");
      const gchar *path = g_getenv ("PATH");
      gchar       *kde_path;

      /* Determine value of KDEDIR */
      if (G_UNLIKELY (kde_dir != NULL))
        {
          /* Build KDEDIR/bin path */
          gchar *kde_bin_dir = g_build_path (G_DIR_SEPARATOR_S, kde_dir, "bin", NULL);
          
          /* Expand PATH to include KDEDIR/bin - if necessary */
          const gchar *occurence = g_strrstr (path, kde_bin_dir);
          if (G_LIKELY (occurence == NULL))
            {
              /* PATH = $PATH:$KDEDIR/bin */
              kde_path = g_strjoin (G_SEARCHPATH_SEPARATOR_S, path, kde_bin_dir);

              /* Set new $PATH value */
              g_setenv ("PATH", kde_path, TRUE);

              /* Free expanded PATH value */
              g_free (kde_path);
            }
              
          /* Free KDEDIR/bin */
          g_free (kde_bin_dir);
        }

      /* Parse output of kde-config */
      if (g_spawn_command_line_sync ("kde-config --path apps", &std_out, &std_err, &status, &error))
        kde_legacy_dirs = g_strsplit (g_strchomp (std_out), G_SEARCHPATH_SEPARATOR_S, 0);
      else
        g_error_free (error);

      /* Free output buffers */
      g_free (std_err);
      g_free (std_out);
    }

  if (kde_legacy_dirs != NULL) /* This is neither likely nor unlikely */
    {
      int i;

      /* Add all KDE legacy dirs to the list */
      for (i = 0; i < g_strv_length (kde_legacy_dirs); i++) 
        frap_menu_add_legacy_dir (menu, kde_legacy_dirs[i]);
    }
}



static void
frap_menu_add_app_dir (FrapMenu    *menu,
                       const gchar *dir)
{
  /* Absolute path of the directory (free'd by the menu instance later) */
  gchar *path;

  g_return_if_fail (FRAP_IS_MENU (menu));
  g_return_if_fail (menu->priv->filename != NULL);
  g_return_if_fail (dir != NULL);

  if (!g_path_is_absolute (dir))
    {
      /* Determine the absolute path (directory) of the menu file */
      gchar *dirname = g_path_get_dirname (menu->priv->filename);

      /* Construct absolute path */
      path = g_build_path (G_DIR_SEPARATOR_S, dirname, dir, NULL);

      /* Free absolute menu file directory path */
      g_free (dirname);
    }
  else
    path = g_strdup (dir);

  /* Append path */
  menu->priv->app_dirs = g_slist_append (menu->priv->app_dirs, path);
}



static void 
frap_menu_add_default_app_dirs (FrapMenu *menu)
{
  int    i;
  gchar *path;
  gchar *kde_data_dir;
  const gchar *kde_dir;

  const gchar * const *dirs;

  g_return_if_fail (FRAP_IS_MENU (menu));

  /* Append $KDEDIR/share/applications as a workaround for distributions 
   * not installing KDE menu files properly into $XDG_DATA_DIR */

  /* Get KDEDIR environment variable */
  kde_dir = g_getenv ("KDEDIR");

  /* Check if this variable is set */
  if (G_UNLIKELY (kde_dir != NULL))
    {
      /* Build KDE data dir */
      kde_data_dir = g_build_filename (kde_dir, "share", "applications", NULL);

      /* Add it as an app dir if it exists */
      if (G_LIKELY (g_file_test (kde_data_dir, G_FILE_TEST_IS_DIR)))
        frap_menu_add_app_dir (menu, kde_data_dir);

      /* Free the KDE data dir */
      g_free (kde_data_dir);
    }

  /* The $KDEDIR workaround ends here */

  /* Append system-wide data dirs */
  dirs = g_get_system_data_dirs ();
  for (i = 0; dirs[i] != NULL; i++)
    {
      path = g_build_path (G_DIR_SEPARATOR_S, dirs[i], "applications", NULL);
      frap_menu_add_app_dir (menu, path);
      g_free (path);
    }

  /* Append user data dir */
  path = g_build_path (G_DIR_SEPARATOR_S, g_get_user_data_dir (), "applications", NULL);
  frap_menu_add_app_dir (menu, path);
  g_free (path);
}



GSList*
frap_menu_get_menus (FrapMenu *menu)
{
  g_return_val_if_fail (FRAP_IS_MENU (menu), NULL);
  return menu->priv->submenus;
}



void
frap_menu_add_menu (FrapMenu *menu,
                    FrapMenu *submenu)
{
  g_return_if_fail (FRAP_IS_MENU (menu));
  g_return_if_fail (FRAP_IS_MENU (submenu));

  /* Remove floating reference and acquire a 'real' one */
#if GLIB_CHECK_VERSION (2,10,0)
  g_object_ref_sink (G_OBJECT (submenu));
#else
  g_object_ref (G_OBJECT (submenu));
#endif

  /* Append menu to the list */
  menu->priv->submenus = g_slist_append (menu->priv->submenus, submenu);

  /* TODO: Use property method here */
  submenu->priv->parent = menu;
}



FrapMenu*
frap_menu_get_menu_with_name (FrapMenu    *menu,
                              const gchar *name)
{
  FrapMenu *result = NULL;
  FrapMenu *submenu;
  GSList   *iter;

  g_return_val_if_fail (FRAP_IS_MENU (menu), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  /* Iterate over the submenu list */
  for (iter = menu->priv->submenus; iter != NULL; iter = g_slist_next (iter))
    {
      submenu = FRAP_MENU (iter->data);

      /* End loop when a matching submenu is found */
      if (G_UNLIKELY (g_utf8_collate (frap_menu_get_name (submenu), name) == 0))
        {
          result = submenu;
          break;
        }
    }

  return result;
}



FrapMenu *
frap_menu_get_parent (FrapMenu *menu)
{
  g_return_val_if_fail (FRAP_IS_MENU (menu), NULL);
  return menu->priv->parent;
}



static void
frap_menu_resolve_legacy_menus (FrapMenu *menu)
{
  GSList      *iter;

  g_return_if_fail (FRAP_IS_MENU (menu));

  /* Iterate over list of legacy directories */
  for (iter = menu->priv->legacy_dirs; iter != NULL; iter = g_slist_next (iter))
    {
      /* Check if the directory exists */
      if (g_file_test (iter->data, G_FILE_TEST_IS_DIR))
        {
          /* Resolve legacy menu hierarchy found in this directory */
          frap_menu_resolve_legacy_menu (menu, iter->data);
        }
    }

  /* Resolve legacy menus of all child menus */
  for (iter = menu->priv->submenus; iter != NULL; iter = g_slist_next (iter))
    {
      frap_menu_resolve_legacy_menus (FRAP_MENU (iter->data));
    }
}



static void
frap_menu_resolve_legacy_menu (FrapMenu    *menu,
                               const gchar *path)
{
  FrapMenu          *legacy_menu;
  FrapMenuDirectory *directory = NULL;
  GDir              *dir;
  const gchar       *filename;
  gchar             *absolute_filename;

  g_return_if_fail (FRAP_IS_MENU (menu));
  g_return_if_fail (path != NULL && g_file_test (path, G_FILE_TEST_IS_DIR));

  /* Open directory for reading */
  dir = g_dir_open (path, 0, NULL);

  /* Abort if directory could not be opened */
  if (G_UNLIKELY (dir == NULL))
    return;

  /* Create the legacy menu */
  legacy_menu = g_object_new (FRAP_TYPE_MENU, "filename", menu->priv->filename, NULL);

  /* Set legacy menu name to the directory path */
  frap_menu_set_name (legacy_menu, path);

  /* Iterate over directory entries */
  while ((filename = g_dir_read_name (dir)) != NULL)
    {
      /* Build absolute filename for this entry */
      absolute_filename = g_build_filename (path, filename, NULL);

      if (g_file_test (absolute_filename, G_FILE_TEST_IS_DIR))
        {
          /* We have a subdir -> create another legacy menu for this subdirectory */
          frap_menu_resolve_legacy_menu (legacy_menu, absolute_filename);
        }
      else if (g_utf8_collate (".directory", filename) == 0) 
        {
          /* We have a .directory file -> create the directory object for the legacy menu */
          directory = g_object_new (FRAP_TYPE_MENU_DIRECTORY, "filename", absolute_filename, NULL);
        }
    }

  /* Check if there was a .directory file in the directory. Otherwise, don't add
   * this legacy menu to its parent (-> it is ignored). */
  if (G_LIKELY (directory != NULL))
    {
      /* Set the legacy menu directory */
      frap_menu_set_directory (legacy_menu, directory);

      /* Add legacy menu to its new parent */
      frap_menu_add_menu (menu, legacy_menu);
    }
  else
    {
      /* Destroy the legacy menu again - no .directory file found */
      g_object_unref (legacy_menu);
    }

  /* Close directory handle */
  g_dir_close (dir);
}



static void
frap_menu_remove_duplicates (FrapMenu *menu)
{
  GSList *iter;

  g_return_if_fail (FRAP_IS_MENU (menu));

  frap_menu_consolidate_child_menus (menu);
  frap_menu_consolidate_app_dirs (menu);
  frap_menu_consolidate_directory_dirs (menu);
  frap_menu_parse_info_consolidate_directory_names (menu->priv->parse_info);

  for (iter = menu->priv->submenus; iter != NULL; iter = g_slist_next (iter))
    {
      FrapMenu *submenu = FRAP_MENU (iter->data);
      frap_menu_remove_duplicates (submenu);
    }
}



static void
frap_menu_consolidate_child_menus (FrapMenu *menu)
{
  GSList      *iter;
  GSList      *merged_submenus = NULL;
  GHashTable  *groups;
  const gchar *name;
  FrapMenu    *submenu;
  FrapMenu    *merged_menu;

  g_return_if_fail (FRAP_IS_MENU (menu));

  /* Setup the hash table */
  groups = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  /* Iterate over all submenus */
  for (iter = menu->priv->submenus; iter != NULL; iter = g_slist_next (iter))
    {
      submenu = FRAP_MENU (iter->data);

      /* Get menu this submenu should be appended to */
      merged_menu = FRAP_MENU (g_hash_table_lookup (groups, frap_menu_get_name (submenu)));

      if (G_LIKELY (merged_menu == NULL))
        {
          /* Create empty menu */
          merged_menu = g_object_new (FRAP_TYPE_MENU, NULL);

          /* Add new menu to the hash table */
          g_hash_table_insert (groups, (gpointer) frap_menu_get_name (submenu), merged_menu);
        }

      /* Copy menu information */
      /* FIXME This introduces possible bugs. E.g. if merged_menu has one <Deleted> 
       * element and submenu has none, the lines below would set it to <NotDeleted> 
       * (which is the default value). This does not follow the spec! Same goes
       * for <OnlyUnallocated>. */
      frap_menu_set_name (merged_menu, frap_menu_get_name (submenu));
      frap_menu_set_only_unallocated (merged_menu, frap_menu_get_only_unallocated (submenu));
      frap_menu_set_deleted (merged_menu, frap_menu_get_deleted (submenu));
      frap_menu_set_filename (merged_menu, frap_menu_get_filename (submenu));

      /* Set parent menu */
      merged_menu->priv->parent = menu;

      /* Append directory names, directory and app dirs as well as rules to the merged menu */      
      g_slist_foreach (submenu->priv->parse_info->directory_names, (GFunc) frap_menu_merge_directory_name, merged_menu);
      g_slist_foreach (submenu->priv->directory_dirs, (GFunc) frap_menu_merge_directory_dir, merged_menu);
      g_slist_foreach (submenu->priv->app_dirs, (GFunc) frap_menu_merge_app_dir, merged_menu);
      g_slist_foreach (submenu->priv->rules, (GFunc) frap_menu_merge_rule, merged_menu);

      /* TODO Merge submenus of submenu and merged_menu! */

      /* Add merged menu to the new list of submenus if not included already */
      if (g_slist_find (merged_submenus, merged_menu) == NULL)
        merged_submenus = g_slist_append (merged_submenus, merged_menu);
    }

  /* Free old submenu list (and the submenus) */
  g_slist_foreach (menu->priv->submenus, (GFunc) g_object_unref, NULL);
  g_slist_free (menu->priv->submenus);

  /* Use list of merged submenus as new submenu list */
  menu->priv->submenus = merged_submenus;

  /* Free hash table */
#if GLIB_CHECK_VERSION(2,10,0)  
  g_hash_table_unref (groups);
#else
  g_hash_table_destroy (groups);
#endif
}



static void
frap_menu_merge_directory_name (const gchar *name,
                                FrapMenu    *menu)
{
  g_return_if_fail (FRAP_IS_MENU (menu));
  menu->priv->parse_info->directory_names = g_slist_append (menu->priv->parse_info->directory_names, (gpointer) name);
}



static void
frap_menu_merge_directory_dir (const gchar *dir, 
                               FrapMenu    *menu)
{
  g_return_if_fail (FRAP_IS_MENU (menu));
  frap_menu_add_directory_dir (menu, dir);
}



static void
frap_menu_merge_app_dir (const gchar *dir,
                         FrapMenu    *menu)
{
  g_return_if_fail (FRAP_IS_MENU (menu));
  frap_menu_add_app_dir (menu, dir);
}



static void
frap_menu_merge_rule (FrapMenuRules *rules,
                      FrapMenu      *menu)
{
  g_return_if_fail (FRAP_IS_MENU (menu));
  g_return_if_fail (FRAP_IS_MENU_RULES (rules));
  frap_menu_add_rule (menu, rules);
}



static void
frap_menu_consolidate_directory_dirs (FrapMenu *menu)
{
  GSList *iter;
  GSList *dirs = NULL;

  g_return_if_fail (FRAP_IS_MENU (menu));

  /* Iterate over directory dirs in reverse order */
  for (iter = g_slist_reverse (menu->priv->directory_dirs); iter != NULL; iter = g_slist_next (iter))
    {
      /* Prepend directory dir to the new list unless it already exists */
      if (G_LIKELY (g_slist_find_custom (dirs, iter->data, (GCompareFunc) g_utf8_collate) == NULL))
        dirs = g_slist_prepend (dirs, g_strdup (iter->data));
    }

  /* Free old list */
  g_slist_foreach (menu->priv->directory_dirs, (GFunc) g_free, NULL);
  g_slist_free (menu->priv->directory_dirs);

  menu->priv->directory_dirs = dirs;
}



static void
frap_menu_consolidate_app_dirs (FrapMenu *menu)
{
  GSList *iter;
  GSList *dirs = NULL;

  g_return_if_fail (FRAP_IS_MENU (menu));

  /* Iterate over app dirs in reverse order */
  for (iter = menu->priv->app_dirs; iter != NULL; iter = g_slist_next (iter))
    {
      /* Append app dir to the new list unless it already exists */
      if (G_LIKELY (g_slist_find_custom (dirs, iter->data, (GCompareFunc) g_utf8_collate) == NULL))
        dirs = g_slist_append (dirs, g_strdup (iter->data));
    }

  /* Free old list */
  g_slist_foreach (menu->priv->app_dirs, (GFunc) g_free, NULL);
  g_slist_free (menu->priv->app_dirs);

  menu->priv->app_dirs = dirs;
}



static void
frap_menu_resolve_directory (FrapMenu *menu)
{
  GSList            *directory_names;
  GSList            *iter;
  FrapMenuDirectory *directory = NULL;

  g_return_if_fail (FRAP_IS_MENU (menu));

  /* Get reverse copy of all directory names */
  directory_names = g_slist_reverse (g_slist_copy (menu->priv->parse_info->directory_names));

  /* Try to load one directory name after another */
  for (iter = directory_names; iter != NULL; iter = g_slist_next (iter))
    {
      /* Try to load the directory with this name */
      directory = frap_menu_lookup_directory (menu, iter->data);

      /* Abort search if the directory was loaded successfully */
      if (G_LIKELY (directory != NULL))
        break;
    }

  if (G_LIKELY (directory != NULL)) 
    {
      /* Set the directory (assuming that we found at least one valid name) */
      frap_menu_set_directory (menu, directory);
    }

  /* Free reverse list copy */
  g_slist_free (directory_names);

  /* ... and all submenus (recursively) */
  for (iter = frap_menu_get_menus (menu); iter != NULL; iter = g_slist_next (iter))
    frap_menu_resolve_directory (iter->data);
}



static FrapMenuDirectory*
frap_menu_lookup_directory (FrapMenu    *menu,
                            const gchar *filename)
{
  FrapMenuDirectory *directory = NULL;
  FrapMenu          *current;
  GSList            *dirs;
  gchar             *dirname;
  gchar             *absolute_path;
  GSList            *iter;
  gboolean           found = FALSE;
  
  g_return_val_if_fail (FRAP_IS_MENU (menu), NULL);
  g_return_val_if_fail (filename != NULL, NULL);

  /* Iterate through all (including parent) menus from the bottom up */
  for (current = menu; current != NULL; current = frap_menu_get_parent (current))
    {
      /* Allocate a reverse copy of the menu's directory dirs */
      dirs = g_slist_reverse (g_slist_copy (frap_menu_get_directory_dirs (current)));

      /* Iterate through all directories */
      for (iter = dirs; iter != NULL; iter = g_slist_next (iter))
        {
          /* Check if the path is absolute */
          if (G_UNLIKELY (!g_path_is_absolute (iter->data)))
            {
              /* Determine directory of the menu file */
              dirname = g_path_get_dirname (frap_menu_get_filename (menu));
              
              /* Build absolute path */
              absolute_path = g_build_filename (dirname, iter->data, filename, NULL);

              /* Free directory name */
              g_free (dirname); 
            }
          else
            absolute_path = g_build_filename (iter->data, filename, NULL);

          /* Check if the file exists and is readable */
          if (G_UNLIKELY (g_file_test (absolute_path, G_FILE_TEST_EXISTS)))
            {
              if (G_LIKELY (g_access (absolute_path, R_OK) == 0))
                {
                  /* Load menu directory */
                  directory = g_object_new (FRAP_TYPE_MENU_DIRECTORY, "filename", absolute_path, NULL);

                  /* Update search status */
                  found = TRUE;
                }
            }
          
          /* Free the absolute path */
         g_free (absolute_path);

          /* Cancel search if we found the menu directory file */
          if (G_UNLIKELY (found))
            break;
        }

      /* Free reverse copy */
      g_slist_free (dirs);
    }

  return directory;
}



static void
frap_menu_collect_files (FrapMenu *menu)
{
  GSList *iter;

  g_return_if_fail (FRAP_IS_MENU (menu));

  /* Collect desktop entry filenames */
  for (iter = g_slist_reverse (frap_menu_get_app_dirs (menu)); iter != NULL; iter = g_slist_next (iter))
    frap_menu_collect_files_from_path (menu, iter->data, NULL);

  /* Collect filenames for submenus */
  for (iter = menu->priv->submenus; iter != NULL; iter = g_slist_next (iter))
    frap_menu_collect_files (FRAP_MENU (iter->data));
}



static void
frap_menu_collect_files_from_path (FrapMenu    *menu,
                                   const gchar *path,
                                   const gchar *id_prefix)
{
  GDir        *dir;
  const gchar *filename;
  gchar       *absolute_path;
  gchar       *new_id_prefix;
  gchar       *desktop_id;

  g_return_if_fail (FRAP_IS_MENU (menu));
  g_return_if_fail (path != NULL && g_path_is_absolute (path));

  /* Skip directory if it doesn't exist */
  if (G_UNLIKELY (!g_file_test (path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)))
    return;

  /* Open directory for reading */
  dir = g_dir_open (path, 0, NULL);

  /* Abort if directory cannot be opened */
  if (G_UNLIKELY (dir == NULL))
    return;

  /* Read file by file */
  while ((filename = g_dir_read_name (dir)) != NULL)
    {
      /* Build absolute path */
      absolute_path = g_build_filename (path, filename, NULL);

      /* Treat files and directories differently */
      if (g_file_test (absolute_path, G_FILE_TEST_IS_DIR))
        {
          /* Create new desktop-file id prefix */
          if (G_LIKELY (id_prefix == NULL))
            new_id_prefix = g_strdup (filename);
          else
            new_id_prefix = g_strjoin ("-", id_prefix, filename, NULL);

          /* Collect files in the directory */
          frap_menu_collect_files_from_path (menu, absolute_path, new_id_prefix);

          /* Free id prefix */
          g_free (new_id_prefix);
        }
      else
        {
          /* Skip all filenames which do not end with .desktop */
          if (G_LIKELY (g_str_has_suffix (filename, ".desktop")))
            {
              /* Create desktop-file id */
              if (G_LIKELY (id_prefix == NULL))
                desktop_id = g_strdup (filename);
              else
                desktop_id = g_strjoin ("-", id_prefix, filename, NULL);

              /* Insert into the files hash table if the desktop-file id does not exist in there yet */
              if (G_LIKELY (g_hash_table_lookup (menu->priv->parse_info->files, desktop_id) == NULL))
                g_hash_table_insert (menu->priv->parse_info->files, g_strdup (desktop_id), g_strdup (absolute_path));

              /* Free desktop-file id */
              g_free (desktop_id);
            }
        }

      /* Free absolute path */
      g_free (absolute_path);
    }
}



static void
frap_menu_resolve_items (FrapMenu *menu,
                         gboolean  only_unallocated)
{
  FrapMenuStandardRules *rule;
  GSList                *iter;
  GDir                  *dir;
  const gchar           *app_dir;

  g_return_if_fail (menu != NULL && FRAP_IS_MENU (menu));

  /* Resolve items in this menu (if it matches the only_unallocated argument.
   * This means that in the first pass, all items of menus without 
   * <OnlyUnallocated /> are resolved and in the second pass, only items of 
   * menus with <OnlyUnallocated /> are resolved */
  if (menu->priv->only_unallocated == only_unallocated)
    {
      /* Iterate over all rules */
      for (iter = menu->priv->rules; iter != NULL; iter = g_slist_next (iter))
        {
          rule = FRAP_MENU_STANDARD_RULES (iter->data);

          if (G_LIKELY (frap_menu_standard_rules_get_include (rule)))
            {
              /* Resolve available items and match them against this rule */
              frap_menu_resolve_items_by_rule (menu, rule);
            }
          else
            {
              /* Remove all items matching this exclude rule from the item pool */
              frap_menu_item_pool_apply_exclude_rule (menu->priv->pool, rule);
            }
        }
    }

  /* Iterate over all submenus */
  for (iter = menu->priv->submenus; iter != NULL; iter = g_slist_next (iter))
    {
      /* Resolve items of the submenu */
      frap_menu_resolve_items (FRAP_MENU (iter->data), only_unallocated);
    }
}



static void
frap_menu_resolve_items_by_rule (FrapMenu              *menu,
                                 FrapMenuStandardRules *rule)
{
  FrapMenuPair pair;
  GSList      *iter;

  g_return_if_fail (FRAP_IS_MENU (menu));
  g_return_if_fail (FRAP_IS_MENU_STANDARD_RULES (rule));

  /* Store menu and rule pointer in the pair */
  pair.first = menu;
  pair.second = rule;

  /* Try to insert each of the collected desktop entry filenames into the menu */
  g_hash_table_foreach (menu->priv->parse_info->files, (GHFunc) frap_menu_resolve_item_by_rule, &pair);
}



static void
frap_menu_resolve_item_by_rule (const gchar  *desktop_id,
                                const gchar  *filename,
                                FrapMenuPair *data)
{
  FrapMenu              *menu;
  FrapMenuStandardRules *rule;
  FrapMenuItem          *item;

  g_return_if_fail (FRAP_IS_MENU (data->first));
  g_return_if_fail (FRAP_IS_MENU_STANDARD_RULES (data->second));

  /* Restore menu and rule from the data pair */
  menu = FRAP_MENU (data->first);
  rule = FRAP_MENU_STANDARD_RULES (data->second);

  /* Try to load the menu item from the cache */
  item = frap_menu_item_cache_lookup (menu->priv->cache, filename, desktop_id);

  if (G_LIKELY (item != NULL))
    {
      /* Only include item if menu not only includes unallocated items
       * or if the item is not allocated yet */
      if (!menu->priv->only_unallocated || (frap_menu_item_get_allocated (item) < 1))
        {
          /* Add item to the pool if it matches the include rule */
          if (G_LIKELY (frap_menu_standard_rules_get_include (rule) && frap_menu_rules_match (FRAP_MENU_RULES (rule), item)))
            frap_menu_item_pool_insert (menu->priv->pool, item);
        }
    }
}



static void
frap_menu_resolve_deleted (FrapMenu *menu)
{
  GSList  *iter;
  gboolean deleted;

  g_return_if_fail (FRAP_IS_MENU (menu));

  /* Note: There's a limitation: if the root menu has a <Deleted /> we
   * can't just free the pointer here. Therefor we only check child menus. */

  for (iter = menu->priv->submenus; iter != NULL; iter = g_slist_next (iter))
    {
      FrapMenu *submenu = iter->data;

      /* Determine whether this submenu was deleted */
      deleted = submenu->priv->deleted;
      if (G_LIKELY (submenu->priv->directory != NULL))
        deleted = deleted || frap_menu_directory_get_hidden (submenu->priv->directory);

      /* Remove submenu if it is deleted, otherwise check submenus of the submenu */
      if (G_UNLIKELY (deleted))
        {
          /* Remove submenu from the list ... */
          menu->priv->submenus = g_slist_remove_link (menu->priv->submenus, iter);

          /* ... and destroy it */
          g_object_unref (G_OBJECT (submenu));
        }
      else
        frap_menu_resolve_deleted (submenu);
    }
}



static void
frap_menu_resolve_moves (FrapMenu *menu)
{
  FrapMenu     *submenu;
  FrapMenu     *target_submenu;
  FrapMenuMove *move;
  GSList       *iter;
  GSList       *submenu_iter;
  GSList       *removed_menus;

  g_return_if_fail (FRAP_IS_MENU (menu));

  /* Recurse into the submenus which need to perform move actions first */
  for (submenu_iter = menu->priv->submenus; submenu_iter != NULL; submenu_iter = g_slist_next (submenu_iter))
    {
      submenu = FRAP_MENU (submenu_iter->data);

      /* Resolve moves of the child menu */
      frap_menu_resolve_moves (submenu);
    }

  /* Iterate over the move instructions */
  for (iter = menu->priv->moves; iter != NULL; iter = g_slist_next (iter))
    {
      move = FRAP_MENU_MOVE (iter->data);

      /* Fetch submenu with the old name */
      submenu = frap_menu_get_menu_with_name (menu, frap_menu_move_get_old (move));

      /* Only go into details if there actually is a submenu with this name */
      if (submenu != NULL)
        {
          /* Fetch the target submenu */
          target_submenu = frap_menu_get_menu_with_name (menu, frap_menu_move_get_new (move));

          /* If there is no target, just rename the submenu */
          if (target_submenu == NULL)
            frap_menu_set_name (submenu, frap_menu_move_get_new (move));
          else
            {
              /* TODO Set <Deleted>, <OnlyUnallocated>, etc. See FIXME in this file for what kind 
               * of bugs this may introduce. */

              /* Append directory names, directory and app dirs as well as rules to the merged menu */      
              g_slist_foreach (submenu->priv->parse_info->directory_names, (GFunc) frap_menu_merge_directory_name, target_submenu);
              g_slist_foreach (submenu->priv->directory_dirs, (GFunc) frap_menu_merge_directory_dir, target_submenu);
              g_slist_foreach (submenu->priv->app_dirs, (GFunc) frap_menu_merge_app_dir, target_submenu);
              g_slist_foreach (submenu->priv->rules, (GFunc) frap_menu_merge_rule, target_submenu);
              
              /* Remove submenu from the submenu list */
              menu->priv->submenus = g_slist_remove (menu->priv->submenus, submenu);

              /* TODO Merge submenus of submenu and target_submenu. Perhaps even
               * find a better way for merging menus as the current way is a)
               * duplicated (see consolidate_child_menus) and b) buggy */

              /* TODO Free the submenu - this introduces a strange item pool
               * error ... */
              /* g_object_unref (submenu); */
            }
        }
    }
}



static GSList*
frap_menu_get_rules (FrapMenu *menu)
{
  g_return_val_if_fail (FRAP_IS_MENU (menu), NULL);
  return menu->priv->rules;
}



static void
frap_menu_add_rule (FrapMenu      *menu,
                    FrapMenuRules *rules)
{
  g_return_if_fail (FRAP_IS_MENU (menu));
  g_return_if_fail (FRAP_IS_MENU_RULES (rules));

  menu->priv->rules = g_slist_append (menu->priv->rules, rules);
}



static void
frap_menu_add_move (FrapMenu     *menu,
                    FrapMenuMove *move)
{
  g_return_if_fail (FRAP_IS_MENU (menu));
  g_return_if_fail (FRAP_IS_MENU_MOVE (move));

  menu->priv->moves = g_slist_append (menu->priv->moves, move);
}



FrapMenuItemPool*
frap_menu_get_item_pool (FrapMenu *menu)
{
  g_return_val_if_fail (FRAP_IS_MENU (menu), NULL);

  return menu->priv->pool;
}



static void
items_collect (const gchar  *desktop_id,
               FrapMenuItem *item,
               GSList      **listp)
{
  *listp = g_slist_prepend (*listp, item);
}



/**
 * frap_menu_get_items:
 * @menu : a #FrapMenu.
 *
 * Convenience wrapper around frap_menu_get_item_pool(), which simply returns the
 * #FrapMenuItem<!---->s contained within the associated item pool as singly linked
 * list.
 *
 * The caller is responsible to free the returned list using
 * <informalexample><programlisting>
 * g_slist_free (list);
 * </programlisting></informalexample>
 * when no longer needed.
 * 
 * Return value: the list of #FrapMenuItem<!---->s within this menu.
 **/
GSList*
frap_menu_get_items (FrapMenu *menu)
{
  GSList *items = NULL;

  g_return_val_if_fail (FRAP_IS_MENU (menu), NULL);

  /* collect the items in the pool */
  frap_menu_item_pool_foreach (menu->priv->pool, (GHFunc) items_collect, &items);

  return items;
}
