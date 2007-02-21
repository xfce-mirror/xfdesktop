/* $Id: frap-menu-move.h 24502 2007-01-16 10:08:36Z jannis $ */
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

#ifndef __FRAP_MENU_MOVE_H__
#define __FRAP_MENU_MOVE_H__

#include <glib-object.h>

G_BEGIN_DECLS;

typedef struct _FrapMenuMoveClass FrapMenuMoveClass;
typedef struct _FrapMenuMove      FrapMenuMove;

#define FRAP_TYPE_MENU_MOVE            (frap_menu_move_get_type())
#define FRAP_MENU_MOVE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FRAP_TYPE_MENU_MOVE, FrapMenuMove))
#define FRAP_MENU_MOVE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FRAP_TYPE_MENU_MOVE, FrapMenuMoveClass))
#define FRAP_IS_MENU_MOVE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FRAP_TYPE_MENU_MOVE))
#define FRAP_IS_MENU_MOVE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FRAP_TYPE_MENU_MOVE))
#define FRAP_MENU_MOVE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FRAP_TYPE_MENU_MOVE, FrapMenuMoveClass))

GType         frap_menu_move_get_type    (void) G_GNUC_CONST;

FrapMenuMove *frap_menu_move_new         (void);

const gchar  *frap_menu_move_get_old     (FrapMenuMove *move);
void          frap_menu_move_set_old     (FrapMenuMove *move,
                                          const gchar  *old);

const gchar  *frap_menu_move_get_new     (FrapMenuMove *move);
void          frap_menu_move_set_new     (FrapMenuMove *move,
                                          const gchar  *new);

G_END_DECLS;

#endif /* !__FRAP_MENU_MOVE_H__ */
