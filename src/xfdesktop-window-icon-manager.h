/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2006 Brian Tarricone, <brian@tarricone.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#ifndef __XFDESKTOP_WINDOW_ICON_MANAGER_H__
#define __XFDESKTOP_WINDOW_ICON_MANAGER_H__

#include <gtk/gtk.h>
#include <xfconf/xfconf.h>

#include "xfdesktop-icon-view-manager.h"

G_BEGIN_DECLS

#define XFDESKTOP_TYPE_WINDOW_ICON_MANAGER     (xfdesktop_window_icon_manager_get_type())
#define XFDESKTOP_WINDOW_ICON_MANAGER(obj)     (G_TYPE_CHECK_INSTANCE_CAST((obj), XFDESKTOP_TYPE_WINDOW_ICON_MANAGER, XfdesktopWindowIconManager))
#define XFDESKTOP_IS_WINDOW_ICON_MANAGER(obj)  (G_TYPE_CHECK_INSTANCE_TYPE((obj), XFDESKTOP_TYPE_WINDOW_ICON_MANAGER))

typedef struct _XfdesktopWindowIconManager         XfdesktopWindowIconManager;
typedef struct _XfdesktopWindowIconManagerClass    XfdesktopWindowIconManagerClass;
typedef struct _XfdesktopWindowIconManagerPrivate  XfdesktopWindowIconManagerPrivate;

struct _XfdesktopWindowIconManager
{
    XfdesktopIconViewManager parent;

    /*< private >*/
    XfdesktopWindowIconManagerPrivate *priv;
};

struct _XfdesktopWindowIconManagerClass
{
    XfdesktopIconViewManagerClass parent_class;
};

GType xfdesktop_window_icon_manager_get_type(void) G_GNUC_CONST;

XfdesktopIconViewManager *xfdesktop_window_icon_manager_new(XfconfChannel *channel,
                                                            GtkWidget *parent);

G_END_DECLS

#endif  /* __XFDESKTOP_WINDOW_ICON_MANAGER_H__ */
