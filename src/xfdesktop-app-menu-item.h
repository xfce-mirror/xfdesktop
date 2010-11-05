/*
 * A GtkImageMenuItem subclass that handles menu items that are
 * intended to represent launchable applications.
 *
 * Copyright (c) 2004,2009 Brian Tarricone <bjt23@cornell.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __XFDESKTOP_APP_MENU_ITEM_H__
#define __XFDESKTOP_APP_MENU_ITEM_H__

#include <glib-object.h>
#include <garcon/garcon.h>

G_BEGIN_DECLS

#define XFDESKTOP_TYPE_APP_MENU_ITEM        (xfdesktop_app_menu_item_get_type())
#define XFDESKTOP_APP_MENU_ITEM(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), XFDESKTOP_TYPE_APP_MENU_ITEM, XfdesktopAppMenuItem))
#define XFDESKTOP_APP_MENU_ITEM_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), XFDESKTOP_TYPE_APP_MENU_ITEM, XfdesktopAppMenuItemClass))
#define XFCE_IS_APP_MENU_ITEM(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), XFDESKTOP_TYPE_APP_MENU_ITEM))
#define XFCE_IS_APP_MENU_ITEM_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), XFDESKTOP_TYPE_APP_MENU_ITEM))
#define XFDESKTOP_APP_MENU_ITEM_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), XFDESKTOP_TYPE_APP_MENU_ITEM, XfdesktopAppMenuItemClass))

typedef struct _XfdesktopAppMenuItem       XfdesktopAppMenuItem;

GType      xfdesktop_app_menu_item_get_type      (void) G_GNUC_CONST;

GtkWidget *xfdesktop_app_menu_item_new           (GarconMenuItem *item);

void       xfdesktop_app_menu_item_set_show_icon (gboolean        show_icon);

G_END_DECLS

#endif /* !def __XFDESKTOP_APP_MENU_ITEM_H__ */
