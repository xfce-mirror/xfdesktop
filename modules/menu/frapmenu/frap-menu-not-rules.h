/* $Id: frap-menu-not-rules.h 24502 2007-01-16 10:08:36Z jannis $ */
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

#ifndef __FRAP_MENU_NOT_RULES_H__
#define __FRAP_MENU_NOT_RULES_H__

#include <glib-object.h>

typedef struct _FrapMenuNotRules        FrapMenuNotRules;
typedef struct _FrapMenuNotRulesClass   FrapMenuNotRulesClass;

#define FRAP_TYPE_MENU_NOT_RULES             (frap_menu_not_rules_get_type ())
#define FRAP_MENU_NOT_RULES(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), FRAP_TYPE_MENU_NOT_RULES, FrapMenuNotRules))
#define FRAP_MENU_NOT_RULES_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), FRAP_TYPE_MENU_NOT_RULES, FrapMenuNotRulesClass))
#define FRAP_IS_MENU_NOT_RULES(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FRAP_TYPE_MENU_NOT_RULES))
#define FRAP_IS_MENU_NOT_RULES_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((obj), FRAP_TYPE_MENU_NOT_RULES))
#define FRAP_MENU_NOT_RULES_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), FRAP_TYPE_MENU_NOT_RULES, FrapMenuNotRulesClass))

GType             frap_menu_not_rules_get_type (void) G_GNUC_CONST;

FrapMenuNotRules *frap_menu_not_rules_new      (void);

G_END_DECLS;

#endif /* !__FRAP_MENU_NOT_RULES_H__ */
