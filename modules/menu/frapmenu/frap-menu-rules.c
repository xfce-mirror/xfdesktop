/* $Id: frap-menu-rules.c 24502 2007-01-16 10:08:36Z jannis $ */
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



static void frap_menu_rules_class_init (gpointer klass);



GType
frap_menu_rules_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (FrapMenuRulesIface),
        NULL,
        NULL,
        (GClassInitFunc) frap_menu_rules_class_init,
        NULL,
        NULL,
        0,
        0,
        NULL,
      };

      type = g_type_register_static (G_TYPE_INTERFACE, "FrapMenuRules", &info, 0);
      g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
    }
  
  return type;
}



static void
frap_menu_rules_class_init (gpointer klass)
{
}



gboolean
frap_menu_rules_match (FrapMenuRules *rules,
                       FrapMenuItem  *item)
{
  g_return_val_if_fail (FRAP_IS_MENU_RULES (rules), FALSE);
  return (*FRAP_MENU_RULES_GET_IFACE (rules)->match) (rules, item);
}



void
frap_menu_rules_add_rules (FrapMenuRules *rules,
                           FrapMenuRules *additional_rules)
{
  g_return_if_fail (FRAP_IS_MENU_RULES (rules));
  g_return_if_fail (FRAP_IS_MENU_RULES (additional_rules));
  (*FRAP_MENU_RULES_GET_IFACE (rules)->add_rules) (rules, additional_rules);
}



void frap_menu_rules_add_all (FrapMenuRules *rules)
{
  g_return_if_fail (FRAP_IS_MENU_RULES (rules));
  (*FRAP_MENU_RULES_GET_IFACE (rules)->add_all) (rules);
}



void
frap_menu_rules_add_filename (FrapMenuRules *rules,
                              const gchar   *filename)
{
  g_return_if_fail (FRAP_IS_MENU_RULES (rules));
  (*FRAP_MENU_RULES_GET_IFACE (rules)->add_filename) (rules, filename);
}


void
frap_menu_rules_add_category (FrapMenuRules *rules,
                              const gchar   *category)
{
  g_return_if_fail (FRAP_IS_MENU_RULES (rules));
  (*FRAP_MENU_RULES_GET_IFACE (rules)->add_category) (rules, category);
}
