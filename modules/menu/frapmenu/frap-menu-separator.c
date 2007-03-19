/* $Id: frap-menu-separator.c 25185 2007-03-18 02:23:12Z jannis $ */
/* vim:set et ai sw=2 sts=2: */
/*-
 * Copyright (c) 2007 Jannis Pohlmann <jannis@xfce.org>
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

#include <frap-menu-separator.h>



static void               frap_menu_separator_class_init (FrapMenuSeparatorClass *klass);
static void               frap_menu_separator_init       (FrapMenuSeparator      *separator);
static void               frap_menu_separator_finalize   (GObject                *object);



static FrapMenuSeparator *_frap_menu_separator = NULL;



void
_frap_menu_separator_init (void)
{
  if (G_LIKELY (_frap_menu_separator == NULL))
    _frap_menu_separator = g_object_new (FRAP_TYPE_MENU_SEPARATOR, NULL);
}



void
_frap_menu_separator_shutdown (void)
{
  if (G_LIKELY (_frap_menu_separator != NULL))
    g_object_unref (G_OBJECT (_frap_menu_separator));
}



struct _FrapMenuSeparatorClass
{
  GObjectClass __parent__;
};

struct _FrapMenuSeparator
{
  GObject __parent__;
};



static GObjectClass *frap_menu_separator_parent_class = NULL;



GType
frap_menu_separator_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (FrapMenuSeparatorClass),
        NULL,
        NULL,
        (GClassInitFunc) frap_menu_separator_class_init,
        NULL,
        NULL,
        sizeof (FrapMenuSeparator),
        0,
        (GInstanceInitFunc) frap_menu_separator_init,
        NULL,
      };

      type = g_type_register_static (G_TYPE_OBJECT, "FrapMenuSeparator", &info, 0);
    }

  return type;
}



static void
frap_menu_separator_class_init (FrapMenuSeparatorClass *klass)
{
  GObjectClass *gobject_class;

  /* Determine parent type class */
  frap_menu_separator_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = frap_menu_separator_finalize;
}



static void
frap_menu_separator_init (FrapMenuSeparator *separator)
{
}



static void
frap_menu_separator_finalize (GObject *object)
{
  (*G_OBJECT_CLASS (frap_menu_separator_parent_class)->finalize) (object);
}



FrapMenuSeparator*
frap_menu_separator_get_default (void)
{
  return _frap_menu_separator;
}
