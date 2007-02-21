/* $Id: frap-menu-standard-rules.h 24502 2007-01-16 10:08:36Z jannis $ */
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
 * MERCHANTABILITY or FITNESS FSTANDARD A PARTICULAR PURPOSE.  See the GNU
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

#ifndef __FRAP_MENU_STANDARD_RULES_H__
#define __FRAP_MENU_STANDARD_RULES_H__

#include <glib-object.h>

typedef struct _FrapMenuStandardRules        FrapMenuStandardRules;
typedef struct _FrapMenuStandardRulesClass   FrapMenuStandardRulesClass;

#define FRAP_TYPE_MENU_STANDARD_RULES             (frap_menu_standard_rules_get_type ())
#define FRAP_MENU_STANDARD_RULES(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), FRAP_TYPE_MENU_STANDARD_RULES, FrapMenuStandardRules))
#define FRAP_MENU_STANDARD_RULES_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), FRAP_TYPE_MENU_STANDARD_RULES, FrapMenuStandardRulesClass))
#define FRAP_IS_MENU_STANDARD_RULES(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FRAP_TYPE_MENU_STANDARD_RULES))
#define FRAP_IS_MENU_STANDARD_RULES_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((obj), FRAP_TYPE_MENU_STANDARD_RULES))
#define FRAP_MENU_STANDARD_RULES_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), FRAP_TYPE_MENU_STANDARD_RULES, FrapMenuStandardRulesClass))

struct _FrapMenuStandardRulesClass
{
  GObjectClass __parent__;

  gboolean (*match_item) (FrapMenuStandardRules *rules, 
                          FrapMenuItem          *item);
};

struct _FrapMenuStandardRules
{
  GObject __parent__;

  /* Nested rules */
  GList   *rules;

  /* Filename rules */
  GList   *filenames;

  /* Category rules */
  GList   *categories;

  /* All rule */
  guint    all : 1;

  /* Whether this rules object is treated as an include or exclude element */
  gboolean include;
};

GType    frap_menu_standard_rules_get_type      (void) G_GNUC_CONST;

gboolean frap_menu_standard_rules_get_include   (FrapMenuStandardRules *rules);
void     frap_menu_standard_rules_set_include   (FrapMenuStandardRules *rules,
                                                 gboolean               include);

G_END_DECLS;

#endif /* !__FRAP_MENU_STANDARD_RULES_H__ */
