/* $Id: frap-menu-directory.c 24974 2007-02-14 10:49:23Z jannis $ */
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



#include <locale.h>
#include <glib.h>
#include <libxfce4util/libxfce4util.h>

#include <frap-menu-environment.h>
#include <frap-menu-directory.h>



void
_frap_menu_directory_init (void)
{
}



void
_frap_menu_directory_shutdown (void)
{
}



#define FRAP_MENU_DIRECTORY_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), FRAP_TYPE_MENU_DIRECTORY, FrapMenuDirectoryPrivate))



/* Desktop entry keys */
static const gchar *desktop_entry_keys[] = 
{
  "Name",
  "Comment",
  "Icon",
  "Categories",
  "OnlyShowIn",
  "NotShowIn",
  "NoDisplay",
  "Hidden",
  NULL
};



/* Property identifiers */
enum
{
  PROP_0,
  PROP_FILENAME,
  PROP_NAME,
  PROP_COMMENT,
  PROP_NO_DISPLAY,
  PROP_ICON,
};



static void       frap_menu_directory_class_init       (FrapMenuDirectoryClass       *klass);
static void       frap_menu_directory_init             (FrapMenuDirectory            *directory);
static void       frap_menu_directory_finalize         (GObject                      *object);
static void       frap_menu_directory_get_property     (GObject                      *object,
                                                        guint                         prop_id,
                                                        GValue                       *value,
                                                        GParamSpec                   *pspec);
static void       frap_menu_directory_set_property     (GObject                      *object,
                                                        guint                         prop_id,
                                                        const GValue                 *value,
                                                        GParamSpec                   *pspec);
static void       frap_menu_directory_free_private     (FrapMenuDirectory            *directory);
static void       frap_menu_directory_load             (FrapMenuDirectory            *directory);



struct _FrapMenuDirectoryPrivate
{
  /* Directory filename */
  gchar             *filename;

  /* Directory name */
  gchar             *name;

  /* Directory description (comment) */
  gchar             *comment;

  /* Icon */
  gchar             *icon;

  /* Environments in which the menu should be displayed only */
  gchar            **only_show_in;

  /* Environments in which the menu should be hidden */
  gchar            **not_show_in;

  /* Whether the menu should be ignored completely */
  guint              hidden : 1;

  /* Whether the menu should be hidden */
  guint              no_display : 1;
};

struct _FrapMenuDirectoryClass
{
  GObjectClass __parent__;
};

struct _FrapMenuDirectory
{
  GObject          __parent__;

  /* < private > */
  FrapMenuDirectoryPrivate *priv;
};



static GObjectClass *frap_menu_directory_parent_class = NULL;



GType
frap_menu_directory_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (FrapMenuDirectoryClass),
        NULL,
        NULL,
        (GClassInitFunc) frap_menu_directory_class_init,
        NULL,
        NULL,
        sizeof (FrapMenuDirectory),
        0,
        (GInstanceInitFunc) frap_menu_directory_init,
        NULL,
      };

      type = g_type_register_static (G_TYPE_OBJECT, "FrapMenuDirectory", &info, 0);
    }

  return type;
}



static void
frap_menu_directory_class_init (FrapMenuDirectoryClass *klass)
{
  GObjectClass *gobject_class;

  g_type_class_add_private (klass, sizeof(FrapMenuDirectoryPrivate));

  /* Determine the parent type class */
  frap_menu_directory_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = frap_menu_directory_finalize; 
  gobject_class->get_property = frap_menu_directory_get_property;
  gobject_class->set_property = frap_menu_directory_set_property;

  /**
   * FrapMenuDirectory:filename:
   *
   * The filename of an %FrapMenuDirectory object. Whenever this is redefined, the
   * directory entry is parsed again.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILENAME,
                                   g_param_spec_string ("filename",
                                                        _("Filename"),
                                                        _("Directory filename"),
                                                        NULL,
                                                        G_PARAM_READWRITE));

  /**
   * FrapMenuDirectory:name:
   *
   * Name of the directory.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILENAME,
                                   g_param_spec_string ("name",
                                                        _("Name"),
                                                        _("Directory name"),
                                                        NULL,
                                                        G_PARAM_READWRITE));

  /**
   * FrapMenuDirectory:comment:
   *
   * Directory description (comment).
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILENAME,
                                   g_param_spec_string ("comment",
                                                        _("Description"),
                                                        _("Directory description"),
                                                        NULL,
                                                        G_PARAM_READWRITE));

  /**
   * FrapMenuDirectory:icon:
   *
   * Icon associated with this directory.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILENAME,
                                   g_param_spec_string ("icon",
                                                        _("Icon"),
                                                        _("Directory icon"),
                                                        NULL,
                                                        G_PARAM_READWRITE));

  /**
   * FrapMenuDirectory:no-display:
   *
   * Whether this menu item is hidden in menus.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_NO_DISPLAY,
                                   g_param_spec_boolean ("no-display",
                                                         _("No Display"),
                                                         _("Visibility state of the related menu"),
                                                         FALSE,
                                                         G_PARAM_READWRITE));

}



static void
frap_menu_directory_init (FrapMenuDirectory *directory)
{
  directory->priv = FRAP_MENU_DIRECTORY_GET_PRIVATE (directory);
  directory->priv->filename = NULL;
  directory->priv->name = NULL;
  directory->priv->icon = NULL;
  directory->priv->only_show_in = NULL;
  directory->priv->not_show_in = NULL;
  directory->priv->hidden = FALSE;
  directory->priv->no_display = FALSE;
}



static void
frap_menu_directory_finalize (GObject *object)
{
  FrapMenuDirectory *directory = FRAP_MENU_DIRECTORY (object);

  /* Free private data */
  frap_menu_directory_free_private (directory);

  /* Free filename */
  if (G_LIKELY (directory->priv->filename != NULL))
    g_free (directory->priv->filename);

  (*G_OBJECT_CLASS (frap_menu_directory_parent_class)->finalize) (object);
}



static void
frap_menu_directory_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  FrapMenuDirectory *directory = FRAP_MENU_DIRECTORY (object);

  switch (prop_id)
    {
    case PROP_FILENAME:
      g_value_set_string (value, frap_menu_directory_get_filename (directory));
      break;

    case PROP_NAME:
      g_value_set_string (value, frap_menu_directory_get_name (directory));
      break;

    case PROP_COMMENT:
      g_value_set_string (value, frap_menu_directory_get_comment (directory));
      break;

    case PROP_ICON:
      g_value_set_string (value, frap_menu_directory_get_icon (directory));
      break;

    case PROP_NO_DISPLAY:
      g_value_set_boolean (value, frap_menu_directory_get_no_display (directory));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
frap_menu_directory_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  FrapMenuDirectory *directory = FRAP_MENU_DIRECTORY (object);

  switch (prop_id)
    {
    case PROP_FILENAME:
      frap_menu_directory_set_filename (directory, g_value_get_string (value));
      break;

    case PROP_NAME:
      frap_menu_directory_set_name (directory, g_value_get_string (value));
      break;

    case PROP_COMMENT:
      frap_menu_directory_set_comment (directory, g_value_get_string (value));
      break;

    case PROP_ICON:
      frap_menu_directory_set_icon (directory, g_value_get_string (value));
      break;

    case PROP_NO_DISPLAY:
      frap_menu_directory_set_no_display (directory, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



const gchar*
frap_menu_directory_get_filename (FrapMenuDirectory *directory)
{
  g_return_val_if_fail (FRAP_IS_MENU_DIRECTORY (directory), NULL);
  return directory->priv->filename;
}



void
frap_menu_directory_set_filename (FrapMenuDirectory *directory, const gchar *filename)
{
  g_return_if_fail (FRAP_IS_MENU_DIRECTORY (directory));
  g_return_if_fail (filename != NULL);

  /* Check if there is an old filename */
  if (G_UNLIKELY (directory->priv->filename != NULL))
    {
      if (G_UNLIKELY (filename != NULL && g_utf8_collate (directory->priv->filename, filename) == 0))
        return;

      /* Free old filename */
      g_free (directory->priv->filename);
    }

  /* Set the new filename */
  directory->priv->filename = g_strdup (filename);

  /* Free private data before reloading the directory */
  frap_menu_directory_free_private (directory);

  /* Reload the menu */
  frap_menu_directory_load (directory);

  /* Notify listeners */
  g_object_notify (G_OBJECT (directory), "filename");
}



const gchar*
frap_menu_directory_get_name (FrapMenuDirectory *directory)
{
  g_return_val_if_fail (FRAP_IS_MENU_DIRECTORY (directory), NULL);
  return directory->priv->name;
}



void
frap_menu_directory_set_name (FrapMenuDirectory *directory, const gchar *name)
{
  g_return_if_fail (FRAP_IS_MENU_DIRECTORY (directory));
  g_return_if_fail (name != NULL);

  /* Free old name */
  if (G_UNLIKELY (directory->priv->name != NULL))
    g_free (directory->priv->name);

  /* Set the new filename */
  directory->priv->name = g_strdup (name);

  /* Notify listeners */
  g_object_notify (G_OBJECT (directory), "name");
}



const gchar*
frap_menu_directory_get_comment (FrapMenuDirectory *directory)
{
  g_return_val_if_fail (FRAP_IS_MENU_DIRECTORY (directory), NULL);
  return directory->priv->comment;
}



void
frap_menu_directory_set_comment (FrapMenuDirectory *directory, const gchar *comment)
{
  g_return_if_fail (FRAP_IS_MENU_DIRECTORY (directory));

  /* Free old name */
  if (G_UNLIKELY (directory->priv->comment != NULL))
    g_free (directory->priv->comment);

  /* Set the new filename */
  directory->priv->comment = g_strdup (comment);

  /* Notify listeners */
  g_object_notify (G_OBJECT (directory), "comment");
}


const gchar*
frap_menu_directory_get_icon (FrapMenuDirectory *directory)
{
  g_return_val_if_fail (FRAP_IS_MENU_DIRECTORY (directory), NULL);
  return directory->priv->icon;
}



void
frap_menu_directory_set_icon (FrapMenuDirectory *directory, const gchar *icon)
{
  g_return_if_fail (FRAP_IS_MENU_DIRECTORY (directory));
  g_return_if_fail (icon != NULL);

  /* Free old name */
  if (G_UNLIKELY (directory->priv->icon != NULL))
    g_free (directory->priv->icon);

  /* Set the new filename */
  directory->priv->icon = g_strdup (icon);

  /* Notify listeners */
  g_object_notify (G_OBJECT (directory), "icon");
}



gboolean
frap_menu_directory_get_no_display (FrapMenuDirectory *directory)
{
  g_return_val_if_fail (FRAP_IS_MENU_DIRECTORY (directory), FALSE);
  return directory->priv->no_display;
}



void        
frap_menu_directory_set_no_display (FrapMenuDirectory *directory,
                                    gboolean           no_display)
{
  g_return_if_fail (FRAP_IS_MENU_DIRECTORY (directory));
  
  /* Abort if old and new value are equal */
  if (directory->priv->no_display == no_display)
    return;

  /* Assign new value */
  directory->priv->no_display = no_display;

  /* Notify listeners */
  g_object_notify (G_OBJECT (directory), "no-display");
}



static void
frap_menu_directory_free_private (FrapMenuDirectory *directory)
{
  g_return_if_fail (FRAP_IS_MENU_DIRECTORY (directory));

  /* Free name */
  g_free (directory->priv->name);

  /* Free comment */
  g_free (directory->priv->comment);

  /* Free icon */
  g_free (directory->priv->icon);

  /* Free environment lists */
  g_strfreev (directory->priv->only_show_in);
  g_strfreev (directory->priv->not_show_in);
}



static void
frap_menu_directory_load (FrapMenuDirectory *directory)
{
  XfceRc      *entry;
  gchar      **values;

  const gchar  *name;
  const gchar  *comment;
  const gchar  *icon;

  g_return_if_fail (FRAP_IS_MENU_DIRECTORY (directory));
  g_return_if_fail (directory->priv->filename != NULL);

  entry = xfce_rc_simple_open (directory->priv->filename, TRUE);

  if (G_UNLIKELY (entry == NULL))
    {
      g_critical ("Could not load directory desktop entry %s", directory->priv->filename);
      return;
    }

  /* Treat the file as a desktop entry */
  xfce_rc_set_group (entry, "Desktop Entry");

  /* Read directory information */
  name = xfce_rc_read_entry (entry, "Name", NULL);
  comment = xfce_rc_read_entry (entry, "Comment", NULL);
  icon = xfce_rc_read_entry (entry, "Icon", NULL);

  /* Pass data to the directory */
  frap_menu_directory_set_name (directory, name);
  frap_menu_directory_set_comment (directory, comment);
  frap_menu_directory_set_icon (directory, icon);
  frap_menu_directory_set_no_display (directory, xfce_rc_read_bool_entry (entry, "NoDisplay", FALSE));

  /* Set rest of the private data directly */
  directory->priv->only_show_in = xfce_rc_read_list_entry (entry, "OnlyShowIn", ";");
  directory->priv->not_show_in = xfce_rc_read_list_entry (entry, "NotShowIn", ";");
  directory->priv->hidden = xfce_rc_read_bool_entry (entry, "Hidden", FALSE);

  xfce_rc_close (entry);
}



gboolean
frap_menu_directory_get_hidden (FrapMenuDirectory *directory)
{
  g_return_val_if_fail (FRAP_IS_MENU_DIRECTORY (directory), FALSE);
  return directory->priv->hidden;
}



gboolean
frap_menu_directory_show_in_environment (FrapMenuDirectory *directory)
{
  const gchar *env;
  gboolean     show = TRUE;
  gboolean     included;
  int          i;

  g_return_val_if_fail (FRAP_IS_MENU_DIRECTORY (directory), FALSE);
  
  /* Determine current environment */
  env = frap_menu_get_environment ();

  /* If no environment has been set, the menu is displayed no matter what
   * OnlyShowIn or NotShowIn contain */
  if (G_UNLIKELY (env == NULL))
    return TRUE;

  /* Check if we have a OnlyShowIn OR a NotShowIn list (only one of them will be
   * there, according to the desktop entry specification) */
  if (G_UNLIKELY (directory->priv->only_show_in != NULL))
    {
      /* Determine whether our environment is included in this list */
      included = FALSE;
      for (i = 0; i < g_strv_length (directory->priv->only_show_in); ++i) 
        {
          if (G_UNLIKELY (g_utf8_collate (directory->priv->only_show_in[i], env) == 0))
            included = TRUE;
        }

      /* If it's not, don't show the menu */
      if (G_LIKELY (!included))
        show = FALSE;
    }
  else if (G_UNLIKELY (directory->priv->not_show_in != NULL))
    {
      /* Determine whether our environment is included in this list */
      included = FALSE;
      for (i = 0; i < g_strv_length (directory->priv->not_show_in); ++i)
        {
          if (G_UNLIKELY (g_utf8_collate (directory->priv->not_show_in[i], env) == 0))
            included = TRUE;
        }

      /* If it is, hide the menu */
      if (G_UNLIKELY (included))
        show = FALSE;
    }

  return show;
}
