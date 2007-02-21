/* $Id: frap-menu-and-rules.c 24502 2007-01-16 10:08:36Z jannis $ */
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
#include <frap-menu-rules.h>
#include <frap-menu-standard-rules.h>
#include <frap-menu-and-rules.h>



static void     frap_menu_and_rules_class_init     (FrapMenuAndRulesClass *klass);
static void     frap_menu_and_rules_init           (FrapMenuAndRules      *rules);
static void     frap_menu_and_rules_finalize       (GObject               *object);
static gboolean frap_menu_and_rules_match          (FrapMenuStandardRules *rules,
                                                    FrapMenuItem          *item);



struct _FrapMenuAndRulesClass
{
  FrapMenuStandardRulesClass __parent__;
};

struct _FrapMenuAndRules
{
  FrapMenuStandardRules __parent__;
};



static GObjectClass *frap_menu_and_rules_parent_class = NULL;



GType
frap_menu_and_rules_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (FrapMenuAndRulesClass),
        NULL,
        NULL,
        (GClassInitFunc) frap_menu_and_rules_class_init,
        NULL,
        NULL,
        sizeof (FrapMenuAndRules),
        0,
        (GInstanceInitFunc) frap_menu_and_rules_init,
        NULL,
      };

      type = g_type_register_static (FRAP_TYPE_MENU_STANDARD_RULES, "FrapMenuAndRules", &info, 0);
    }

  return type;
}



static void
frap_menu_and_rules_class_init (FrapMenuAndRulesClass *klass)
{
  GObjectClass *gobject_class;
  FrapMenuStandardRulesClass *rules_class;

  /* Determine the parent type class */
  frap_menu_and_rules_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = frap_menu_and_rules_finalize;

  rules_class = FRAP_MENU_STANDARD_RULES_CLASS (klass);
  rules_class->match_item = frap_menu_and_rules_match;
}



static void
frap_menu_and_rules_init (FrapMenuAndRules *rules)
{
}



static void
frap_menu_and_rules_finalize (GObject *object)
{
  (*G_OBJECT_CLASS (frap_menu_and_rules_parent_class)->finalize) (object); 
}



FrapMenuAndRules*
frap_menu_and_rules_new (void)
{
  return g_object_new (FRAP_TYPE_MENU_AND_RULES, NULL);
}



static gboolean
frap_menu_and_rules_match (FrapMenuStandardRules *rules,
                           FrapMenuItem          *item)
{
  GList                 *iter;

  g_return_val_if_fail (FRAP_IS_MENU_STANDARD_RULES (rules), FALSE);
  g_return_val_if_fail (FRAP_IS_MENU_ITEM (item), FALSE);

  /* Check if all elements should be included */
  if (rules->all)
    return TRUE;

  /* Compare desktop id's */
  for (iter = rules->filenames; iter != NULL; iter = g_list_next (iter))
    {
      if (g_utf8_collate (frap_menu_item_get_desktop_id (item), iter->data) != 0)
        return FALSE;
    }

  for (iter = rules->categories; iter != NULL; iter = g_list_next (iter))
    {
      if (!g_list_find_custom (frap_menu_item_get_categories (item), iter->data, (GCompareFunc) g_utf8_collate))
        return FALSE;
    }

  /* Match item against nested rules */
  for (iter = g_list_first (rules->rules); iter != NULL; iter = g_list_next (iter))
    {
      if (!frap_menu_rules_match (FRAP_MENU_RULES (iter->data), item))
        return FALSE;
    }

  return TRUE;
}
