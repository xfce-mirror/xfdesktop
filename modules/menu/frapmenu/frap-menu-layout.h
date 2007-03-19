/* $Id: frap-menu-layout.h 25194 2007-03-18 15:16:39Z jannis $ */
/* vi:set expandtab sw=2 sts=2: */
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

#if !defined(LIBFRAPMENU_INSIDE_LIBFRAPMENU_H) && !defined(LIBFRAPMENU_COMPILATION)
#error "Only <libfrapmenu/libfrapmenu.h> can be included directly. This file may disappear or change contents."
#endif

#ifndef __FRAP_MENU_LAYOUT_H__
#define __FRAP_MENU_LAYOUT_H__

#include <glib-object.h>

G_BEGIN_DECLS;

typedef enum
{
  FRAP_MENU_LAYOUT_MERGE_MENUS,
  FRAP_MENU_LAYOUT_MERGE_FILES,
  FRAP_MENU_LAYOUT_MERGE_ALL,
} FrapMenuLayoutMergeType;

typedef enum
{
  FRAP_MENU_LAYOUT_NODE_INVALID,
  FRAP_MENU_LAYOUT_NODE_FILENAME,
  FRAP_MENU_LAYOUT_NODE_MENUNAME,
  FRAP_MENU_LAYOUT_NODE_SEPARATOR,
  FRAP_MENU_LAYOUT_NODE_MERGE,
} FrapMenuLayoutNodeType;

typedef struct _FrapMenuLayoutNode    FrapMenuLayoutNode;

typedef struct _FrapMenuLayoutPrivate FrapMenuLayoutPrivate;
typedef struct _FrapMenuLayoutClass   FrapMenuLayoutClass;
typedef struct _FrapMenuLayout        FrapMenuLayout;

#define FRAP_TYPE_MENU_LAYOUT            (frap_menu_layout_get_type())
#define FRAP_MENU_LAYOUT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FRAP_TYPE_MENU_LAYOUT, FrapMenuLayout))
#define FRAP_MENU_LAYOUT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FRAP_TYPE_MENU_LAYOUT, FrapMenuLayoutClass))
#define FRAP_IS_MENU_LAYOUT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FRAP_TYPE_MENU_LAYOUT))
#define FRAP_IS_MENU_LAYOUT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FRAP_TYPE_MENU_LAYOUT))
#define FRAP_MENU_LAYOUT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FRAP_TYPE_MENU_LAYOUT, FrapMenuLayoutClass))

GType                   frap_menu_layout_get_type          (void) G_GNUC_CONST;

FrapMenuLayout         *frap_menu_layout_new               (void) G_GNUC_MALLOC;
void                    frap_menu_layout_add_filename      (FrapMenuLayout          *layout,
                                                            const gchar             *filename);
void                    frap_menu_layout_add_menuname      (FrapMenuLayout          *layout,
                                                            const gchar             *menuname);
void                    frap_menu_layout_add_separator     (FrapMenuLayout          *layout);
void                    frap_menu_layout_add_merge         (FrapMenuLayout          *layout,
                                                            FrapMenuLayoutMergeType  type);
GSList                 *frap_menu_layout_get_nodes         (FrapMenuLayout          *layout);
gboolean                frap_menu_layout_get_filename_used (FrapMenuLayout          *layout,
                                                            const gchar             *filename);
gboolean                frap_menu_layout_get_menuname_used (FrapMenuLayout          *layout,
                                                            const gchar             *menuname);

FrapMenuLayoutNodeType  frap_menu_layout_node_get_type     (FrapMenuLayoutNode       *node);
const gchar            *frap_menu_layout_node_get_filename (FrapMenuLayoutNode       *node);
const gchar            *frap_menu_layout_node_get_menuname (FrapMenuLayoutNode       *node);
FrapMenuLayoutMergeType frap_menu_layout_get_merge_type    (FrapMenuLayoutNode       *node);

G_END_DECLS;

#endif /* !__FRAP_MENU_LAYOUT_H__ */

