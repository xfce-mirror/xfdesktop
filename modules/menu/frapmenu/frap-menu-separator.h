/* $Id: frap-menu-separator.h 25185 2007-03-18 02:23:12Z jannis $ */
/* vi:set et ai sw=2 sts=2: */
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

#ifndef __FRAP_MENU_SEPARATOR_H__
#define __FRAP_MENU_SEPARATOR_H__

#include <glib-object.h>

G_BEGIN_DECLS;

typedef struct _FrapMenuSeparatorClass FrapMenuSeparatorClass;
typedef struct _FrapMenuSeparator      FrapMenuSeparator;

#define FRAP_TYPE_MENU_SEPARATOR            (frap_menu_separator_get_type())
#define FRAP_MENU_SEPARATOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FRAP_TYPE_MENU_SEPARATOR, FrapMenuSeparator))
#define FRAP_MENU_SEPARATOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FRAP_TYPE_MENU_SEPARATOR, FrapMenuSeparatorClass))
#define FRAP_IS_MENU_SEPARATOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FRAP_TYPE_MENU_SEPARATOR))
#define FRAP_IS_MENU_SEPARATOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FRAP_TYPE_MENU_SEPARATOR))
#define FRAP_MENU_SEPARATOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FRAP_TYPE_MENU_SEPARATOR, FrapMenuSeparatorClass))


GType              frap_menu_separator_get_type    (void) G_GNUC_CONST;

FrapMenuSeparator *frap_menu_separator_get_default (void);

#if defined(LIBFRAPMENU_COMPILATION)
void               _frap_menu_separator_init       (void) G_GNUC_INTERNAL;
void               _frap_menu_separator_shutdown   (void) G_GNUC_INTERNAL;
#endif

G_END_DECLS;

#endif /* !__FRAP_MENU_SEPARATOR_H__ */
