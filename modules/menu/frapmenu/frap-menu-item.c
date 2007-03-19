/* $Id: frap-menu-item.c 25197 2007-03-18 17:54:52Z jannis $ */
/* vi:set expandtab sw=2 sts=2: */
/*-
 * Copyright (c) 2006-2007 Jannis Pohlmann <jannis@xfce.org>
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#include <libxfce4util/libxfce4util.h>

#include <frap-menu-environment.h>
#include <frap-menu-item.h>



#define FRAP_MENU_ITEM_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), FRAP_TYPE_MENU_ITEM, FrapMenuItemPrivate))



/* Property identifiers */
enum
{
  PROP_0,
  PROP_DESKTOP_ID,
  PROP_FILENAME,
  PROP_REQUIRES_TERMINAL,
  PROP_NO_DISPLAY,
  PROP_STARTUP_NOTIFICATION,
  PROP_NAME,
  PROP_ICON_NAME,
  PROP_COMMAND,
};



static void     frap_menu_item_class_init             (FrapMenuItemClass *klass);
static void     frap_menu_item_init                   (FrapMenuItem      *item);
static void     frap_menu_item_finalize               (GObject           *object);
static void     frap_menu_item_get_property           (GObject           *object,
                                                       guint              prop_id,
                                                       GValue            *value,
                                                       GParamSpec        *pspec);
static void     frap_menu_item_set_property           (GObject           *object,
                                                       guint              prop_id,
                                                       const GValue      *value,
                                                       GParamSpec        *pspec);
static void     frap_menu_item_load                   (FrapMenuItem      *item);




struct _FrapMenuItemClass
{
  GObjectClass __parent__;
};

struct _FrapMenuItemPrivate
{
  /* Desktop file id */
  gchar    *desktop_id;

  /* Absolute filename */
  gchar    *filename;

  /* List of categories */
  GList    *categories;

  /* Whether this application requires a terminal to be started in */
  guint     requires_terminal : 1;

  /* Whether this menu item should be hidden */
  guint     no_display : 1;

  /* Whether this application supports startup notification */
  guint     supports_startup_notification : 1;

  /* Name to be displayed for the menu item */
  gchar    *name;

  /* Command to be executed when the menu item is clicked */
  gchar    *command;

  /* Menu item icon name */
  gchar    *icon_name;

  /* Environments in which the menu item should be displayed only */
  gchar   **only_show_in;

  /* Environments in which the menu item should be hidden */
  gchar   **not_show_in;

  /* Counter keeping the number of menus which use this item. This works
   * like a reference counter and should be increased / decreased by FrapMenu
   * items whenever the item is added to or removed from the menu. */
  guint     num_allocated;
};

struct _FrapMenuItem
{
  GObject __parent__;

  /* < private > */
  FrapMenuItemPrivate *priv;
};



static GObjectClass *frap_menu_item_parent_class = NULL;



GType
frap_menu_item_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info = 
      {
        sizeof (FrapMenuItemClass),
        NULL,
        NULL,
        (GClassInitFunc) frap_menu_item_class_init,
        NULL,
        NULL,
        sizeof (FrapMenuItem),
        0,
        (GInstanceInitFunc) frap_menu_item_init,
        NULL,
      };

      type = g_type_register_static (G_TYPE_OBJECT, "FrapMenuItem", &info, 0);
    }
  
  return type;
}



static void
frap_menu_item_class_init (FrapMenuItemClass *klass)
{
  GObjectClass *gobject_class;

  g_type_class_add_private (klass, sizeof (FrapMenuItemPrivate));

  /* Determine the parent type class */
  frap_menu_item_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = frap_menu_item_finalize;
  gobject_class->get_property = frap_menu_item_get_property;
  gobject_class->set_property = frap_menu_item_set_property;

  /**
   * FrapMenuItem:desktop-id:
   *
   * The desktop-file id of this application.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_DESKTOP_ID,
                                   g_param_spec_string ("desktop-id",
                                                        _("Desktop-File Id"),
                                                        _("Desktop-File Id of the application"),
                                                        NULL,
                                                        G_PARAM_READWRITE));

  /**
   * FrapMenuItem:filename:
   *
   * The (absolute) filename of the %FrapMenuItem. Whenever this changes, the
   * complete file is reloaded. 
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILENAME,
                                   g_param_spec_string ("filename",
                                                        _("Filename"),
                                                        _("Absolute filename"),
                                                        NULL,
                                                        G_PARAM_READWRITE));

  /**
   * FrapMenuItem:requires-terminal:
   *
   * Whether this application requires a terinal to be started in.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_REQUIRES_TERMINAL,
                                   g_param_spec_boolean ("requires-terminal",
                                                         _("Requires a terminal"),
                                                         _("Whether this application requires a terminal"),
                                                         FALSE,
                                                         G_PARAM_READWRITE));

  /**
   * FrapMenuItem:no-display:
   *
   * Whether this menu item is hidden in menus.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_NO_DISPLAY,
                                   g_param_spec_boolean ("no-display",
                                                         _("No Display"),
                                                         _("Visibility state of the menu item"),
                                                         FALSE,
                                                         G_PARAM_READWRITE));

  /**
   * FrapMenuItem:startup-notification:
   *
   * Whether this application supports startup notification.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_STARTUP_NOTIFICATION,
                                   g_param_spec_boolean ("supports-startup-notification",
                                                         _("Startup notification"),
                                                         _("Startup notification support"),
                                                         FALSE,
                                                         G_PARAM_READWRITE));

  /**
  /**
   * FrapMenuItem:name:
   *
   * Name of the application (will be displayed in menus etc.).
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        _("Name"),
                                                        _("Name of the application"),
                                                        NULL,
                                                        G_PARAM_READWRITE));

  /**
   * FrapMenuItem:command:
   *
   * Command to be executed when the menu item is clicked.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_COMMAND,
                                   g_param_spec_string ("command",
                                                        _("Command"),
                                                        _("Application command"),
                                                        NULL,
                                                        G_PARAM_READWRITE));

  /**
   * FrapMenuItem:icon-name:
   *
   * Name of the icon to be displayed for this menu item.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ICON_NAME,
                                   g_param_spec_string ("icon-name",
                                                        _("Icon name"),
                                                        _("Name of the application icon"),
                                                        NULL,
                                                        G_PARAM_READWRITE));
}



static void
frap_menu_item_init (FrapMenuItem *item)
{
  item->priv = FRAP_MENU_ITEM_GET_PRIVATE (item);
  item->priv->desktop_id = NULL;
  item->priv->name = NULL;
  item->priv->filename = NULL;
  item->priv->command = NULL;
  item->priv->categories = NULL;
  item->priv->icon_name = NULL;
  item->priv->only_show_in = NULL;
  item->priv->not_show_in = NULL;
  item->priv->num_allocated = 0;
}



static void
frap_menu_item_finalize (GObject *object)
{
  FrapMenuItem *item = FRAP_MENU_ITEM (object);

  g_free (item->priv->desktop_id);
  g_free (item->priv->name);
  g_free (item->priv->filename);
  g_free (item->priv->command);
  g_free (item->priv->icon_name);

  g_strfreev (item->priv->only_show_in);
  g_strfreev (item->priv->not_show_in);

  g_list_foreach (item->priv->categories, (GFunc) g_free, NULL);
  g_list_free (item->priv->categories);

  (*G_OBJECT_CLASS (frap_menu_item_parent_class)->finalize) (object);
}



static void
frap_menu_item_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  FrapMenuItem *item = FRAP_MENU_ITEM (object);

  switch (prop_id)
    {
    case PROP_DESKTOP_ID:
      g_value_set_string (value, frap_menu_item_get_desktop_id (item));
      break;

    case PROP_FILENAME:
      g_value_set_string (value, frap_menu_item_get_filename (item));
      break;

    case PROP_REQUIRES_TERMINAL:
    case PROP_NO_DISPLAY:
    case PROP_STARTUP_NOTIFICATION:
    case PROP_NAME:
    case PROP_COMMAND:
    case PROP_ICON_NAME:
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
frap_menu_item_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  FrapMenuItem *item = FRAP_MENU_ITEM (object);

  switch (prop_id)
    {
    case PROP_DESKTOP_ID:
      frap_menu_item_set_desktop_id (item, g_value_get_string (value));
      break;

    case PROP_FILENAME:
      frap_menu_item_set_filename (item, g_value_get_string (value));
      break;

    case PROP_REQUIRES_TERMINAL:
      frap_menu_item_set_requires_terminal (item, g_value_get_boolean (value));
      break;

    case PROP_NO_DISPLAY:
      frap_menu_item_set_no_display (item, g_value_get_boolean (value));
      break;

    case PROP_STARTUP_NOTIFICATION:
      frap_menu_item_set_supports_startup_notification (item, g_value_get_boolean (value));
      break;

    case PROP_NAME:
      frap_menu_item_set_name (item, g_value_get_string (value));
      break;

    case PROP_COMMAND:
      frap_menu_item_set_command (item, g_value_get_string (value));
      break;

    case PROP_ICON_NAME:
      frap_menu_item_set_icon_name (item, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



FrapMenuItem*
frap_menu_item_new (const gchar *filename)
{
  FrapMenuItem *item = NULL;
  XfceRc       *rc;
  const gchar  *name;
  const gchar  *exec;
  const gchar  *icon;
  const gchar  *env;
  gchar        *command;
  gchar       **mt;
  gchar       **str_list;
  GList        *categories = NULL;
  gboolean      terminal;
  gboolean      no_display;
  gboolean      startup_notify;
  gboolean      show;

  /* Return NULL if the filename is not an absolute path or if the file does not exists */
  if (G_UNLIKELY (!g_path_is_absolute (filename) || !g_file_test (filename, G_FILE_TEST_EXISTS)))
    return NULL;

  /* Try to open the .desktop file */
  rc = xfce_rc_simple_open (filename, TRUE);
  if (G_UNLIKELY (rc == NULL))
    return NULL;

  /* Use the Desktop Entry section of the desktop file */
  xfce_rc_set_group (rc, "Desktop Entry");

  /* Abort if the file has been marked as "deleted"/hidden */
  if (G_UNLIKELY (xfce_rc_read_bool_entry (rc, "Hidden", FALSE)))
    {
      xfce_rc_close (rc);
      return NULL;
    }

  /* Parse name, exec command and icon name */
  name = xfce_rc_read_entry (rc, "Name", NULL);
  exec = xfce_rc_read_entry (rc, "Exec", NULL);
  icon = xfce_rc_read_entry (rc, "Icon", NULL);

  /* Validate Name and Exec fields */
  if (G_LIKELY (exec != NULL && name != NULL && g_utf8_validate (name, -1, NULL)))
    {
      /* Determine other application properties */
      terminal = xfce_rc_read_bool_entry (rc, "Terminal", FALSE);
      no_display = xfce_rc_read_bool_entry (rc, "NoDisplay", FALSE);
      startup_notify = xfce_rc_read_bool_entry (rc, "StartupNotify", FALSE) || xfce_rc_read_bool_entry (rc, "X-KDE-StartupNotify", FALSE);

      /* Allocate a new menu item instance */
      item = g_object_new (FRAP_TYPE_MENU_ITEM, 
                           "filename", filename,
                           "command", exec, 
                           "name", name, 
                           "icon-name", icon, 
                           "requires-terminal", terminal, 
                           "no-display", no_display, 
                           "supports-startup-notification", startup_notify, 
                           NULL);

      /* Determine the categories this application should be shown in */
      str_list = xfce_rc_read_list_entry (rc, "Categories", ";");
      if (G_LIKELY (str_list != NULL))
        {
          for (mt = str_list; *mt != NULL; ++mt)
            {
              if (**mt != '\0')
                categories = g_list_prepend (categories, g_strdup (*mt));
            }

          /* Free list */          
          g_strfreev (str_list);

          /* Assign categories list to the menu item */
          frap_menu_item_set_categories (item, categories);
        }

      /* Set the rest of the private data directly */
      item->priv->only_show_in = xfce_rc_read_list_entry (rc, "OnlyShowIn", ";");
      item->priv->not_show_in = xfce_rc_read_list_entry (rc, "NotShowIn", ";");
    }

  /* Close file handle */
  xfce_rc_close (rc);

  return item;
}



const gchar*
frap_menu_item_get_desktop_id (FrapMenuItem *item)
{
  g_return_val_if_fail (FRAP_IS_MENU_ITEM (item), NULL);
  return item->priv->desktop_id;
}



void
frap_menu_item_set_desktop_id (FrapMenuItem *item,
                               const gchar  *desktop_id)
{
  g_return_if_fail (FRAP_IS_MENU_ITEM (item));
  g_return_if_fail (desktop_id != NULL);

  /* Free old desktop_id */
  if (G_UNLIKELY (item->priv->desktop_id != NULL))
    {
      /* Abort if old and new desktop_id are equal */
      if (G_UNLIKELY (g_utf8_collate (item->priv->desktop_id, desktop_id) == 0))
        return;

      /* Otherwise free the old desktop_id */
      g_free (item->priv->desktop_id);
    }

  /* Assign the new desktop_id */
  item->priv->desktop_id = g_strdup (desktop_id);

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "desktop_id");
}



const gchar*
frap_menu_item_get_filename (FrapMenuItem *item)
{
  g_return_val_if_fail (FRAP_IS_MENU_ITEM (item), NULL);
  return item->priv->filename;
}



void
frap_menu_item_set_filename (FrapMenuItem *item,
                             const gchar  *filename)
{
  g_return_if_fail (FRAP_IS_MENU_ITEM (item));
  g_return_if_fail (filename != NULL);
  g_return_if_fail (g_path_is_absolute (filename));

  /* Check if there is an old filename */
  if (G_UNLIKELY (item->priv->filename != NULL))
    {
      /* Abort if old and new filename are equal */
      if (G_UNLIKELY (g_utf8_collate (item->priv->filename, filename) == 0))
        return;

      /* Otherwise free the old filename */
      g_free (item->priv->filename);
    }

  /* Assign the new filename */
  item->priv->filename = g_strdup (filename);

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "filename");
}




GList*
frap_menu_item_get_categories (FrapMenuItem *item)
{
  g_return_val_if_fail (FRAP_IS_MENU_ITEM (item), NULL);
  return item->priv->categories;
}



void
frap_menu_item_set_categories (FrapMenuItem *item,
                               GList        *categories)
{
  g_return_if_fail (FRAP_IS_MENU_ITEM (item));

  if (G_UNLIKELY (item->priv->categories != NULL))
    {
      /* Abort if lists are equal */
      if (G_UNLIKELY (item->priv->categories == categories))
        return;

      /* Free old list */
      g_list_foreach (item->priv->categories, (GFunc) g_free, NULL);
      g_list_free (item->priv->categories);
    }

  /* Assign new list */
  item->priv->categories = categories;
}



const gchar*
frap_menu_item_get_command (FrapMenuItem *item)
{
  g_return_val_if_fail (FRAP_IS_MENU_ITEM (item), NULL);
  return item->priv->command;
}



void
frap_menu_item_set_command (FrapMenuItem *item,
                            const gchar  *command)
{
  g_return_if_fail (FRAP_IS_MENU_ITEM (item));
  g_return_if_fail (command != NULL);

  if (G_UNLIKELY (item->priv->command != NULL))
    {
      /* Abort if old and new command are equal */
      if (G_UNLIKELY (g_utf8_collate (item->priv->command, command) == 0))
        return;

      /* Otherwise free old command */
      g_free (item->priv->command);
    }

  /* Assign new command */
  item->priv->command = g_strdup (command);

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "command");
}



const gchar*
frap_menu_item_get_name (FrapMenuItem *item)
{
  g_return_val_if_fail (FRAP_IS_MENU_ITEM (item), NULL);
  return item->priv->name;
}



void
frap_menu_item_set_name (FrapMenuItem *item,
                         const gchar  *name)
{
  g_return_if_fail (FRAP_IS_MENU_ITEM (item));

  if (G_UNLIKELY (item->priv->name != NULL))
    {
      /* Abort if old and new name are equal */
      if (G_UNLIKELY (g_utf8_collate (item->priv->name, name) == 0))
        return;

      /* Otherwise free old name */
      g_free (item->priv->name);
    }

  /* Assign new name */
  item->priv->name = g_strdup (name);

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "name");
}



const gchar*
frap_menu_item_get_icon_name (FrapMenuItem *item)
{
  g_return_val_if_fail (FRAP_IS_MENU_ITEM (item), NULL);
  return item->priv->icon_name;
}



void        
frap_menu_item_set_icon_name (FrapMenuItem *item,
                              const gchar  *icon_name)
{
  g_return_if_fail (FRAP_IS_MENU_ITEM (item));

  if (G_UNLIKELY (item->priv->icon_name != NULL))
    {
      /* Abort if old and new icon name are equal */
      if (G_UNLIKELY (g_utf8_collate (item->priv->icon_name, icon_name) == 0))
        return;

      /* Otherwise free old icon name */
      g_free (item->priv->icon_name);
    }

  /* Assign new icon name */
  item->priv->icon_name = g_strdup (icon_name);

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "icon_name");
}



gboolean
frap_menu_item_requires_terminal (FrapMenuItem *item)
{
  g_return_val_if_fail (FRAP_IS_MENU_ITEM (item), FALSE);
  return item->priv->requires_terminal;
}



void        
frap_menu_item_set_requires_terminal (FrapMenuItem *item,
                                      gboolean      requires_terminal)
{
  g_return_if_fail (FRAP_IS_MENU_ITEM (item));
  
  /* Abort if old and new value are equal */
  if (item->priv->requires_terminal == requires_terminal)
    return;

  /* Assign new value */
  item->priv->requires_terminal = requires_terminal;

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "requires-terminal");
}



gboolean    
frap_menu_item_get_no_display (FrapMenuItem *item)
{
  g_return_val_if_fail (FRAP_IS_MENU_ITEM (item), FALSE);
  return item->priv->no_display;
}



void        
frap_menu_item_set_no_display (FrapMenuItem *item,
                               gboolean      no_display)
{
  g_return_if_fail (FRAP_IS_MENU_ITEM (item));
  
  /* Abort if old and new value are equal */
  if (item->priv->no_display == no_display)
    return;

  /* Assign new value */
  item->priv->no_display = no_display;

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "no-display");
}



gboolean    
frap_menu_item_supports_startup_notification (FrapMenuItem *item)
{
  g_return_val_if_fail (FRAP_IS_MENU_ITEM (item), FALSE);
  return item->priv->supports_startup_notification;
}



void        
frap_menu_item_set_supports_startup_notification (FrapMenuItem *item,
                                                  gboolean      supports_startup_notification)
{
  g_return_if_fail (FRAP_IS_MENU_ITEM (item));
  
  /* Abort if old and new value are equal */
  if (item->priv->supports_startup_notification == supports_startup_notification)
    return;

  /* Assign new value */
  item->priv->supports_startup_notification = supports_startup_notification;

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "supports-startup-notification");
}



gboolean
frap_menu_item_show_in_environment (FrapMenuItem *item)
{
  const gchar *env;
  gboolean     show = TRUE;
  gboolean     included;
  int          i;

  g_return_val_if_fail (FRAP_IS_MENU_ITEM (item), FALSE);

  /* Determine current environment */
  env = frap_menu_get_environment ();

  /* If no environment has been set, the menu item is displayed no matter what
   * OnlyShowIn or NotShowIn contain */
  if (G_UNLIKELY (env == NULL))
    return TRUE;

  /* Check if we have a OnlyShowIn OR a NotShowIn list (only one of them will be
   * there, according to the desktop entry specification) */
  if (G_UNLIKELY (item->priv->only_show_in != NULL))
    {
      /* Determine whether our environment is included in this list */
      included = FALSE;
      for (i = 0; i < g_strv_length (item->priv->only_show_in); ++i) 
        {
          if (G_UNLIKELY (g_utf8_collate (item->priv->only_show_in[i], env) == 0))
            included = TRUE;
        }

      /* If it's not, don't show the menu item */
      if (G_LIKELY (!included))
        show = FALSE;
    }
  else if (G_UNLIKELY (item->priv->not_show_in != NULL))
    {
      /* Determine whether our environment is included in this list */
      included = FALSE;
      for (i = 0; i < g_strv_length (item->priv->not_show_in); ++i)
        {
          if (G_UNLIKELY (g_utf8_collate (item->priv->not_show_in[i], env) == 0))
            included = TRUE;
        }

      /* If it is, hide the menu item */
      if (G_UNLIKELY (included))
        show = FALSE;
    }

  return show;
}



void
frap_menu_item_ref (FrapMenuItem *item)
{
  g_return_if_fail (FRAP_IS_MENU_ITEM (item));

  /* Increment the allocation counter */
  frap_menu_item_increment_allocated (item);

  /* Grab a reference on the object */
  g_object_ref (G_OBJECT (item));
}



void
frap_menu_item_unref (FrapMenuItem *item)
{
  g_return_if_fail (FRAP_IS_MENU_ITEM (item));

  /* Decrement the reference counter */
  g_object_unref (G_OBJECT (item));
}



gint
frap_menu_item_get_allocated (FrapMenuItem *item)
{
  g_return_val_if_fail (FRAP_IS_MENU_ITEM (item), FALSE);
  return item->priv->num_allocated;
}



void
frap_menu_item_increment_allocated (FrapMenuItem *item)
{
  g_return_if_fail (FRAP_IS_MENU_ITEM (item));
  item->priv->num_allocated++;
}



void
frap_menu_item_decrement_allocated (FrapMenuItem *item)
{
  g_return_if_fail (FRAP_IS_MENU_ITEM (item));

  if (item->priv->num_allocated > 0)
    item->priv->num_allocated--;
}
