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

#ifndef __XFDESKTOP_FILE_ICON_MANAGER_H__
#define __XFDESKTOP_FILE_ICON_MANAGER_H__

#include <gdk/gdk.h>
#include <xfconf/xfconf.h>
#include <libxfce4windowing/libxfce4windowing.h>

#include "xfdesktop-icon-view-manager.h"

G_BEGIN_DECLS

#define XFDESKTOP_TYPE_FILE_ICON_MANAGER     (xfdesktop_file_icon_manager_get_type())
#define XFDESKTOP_FILE_ICON_MANAGER(obj)     (G_TYPE_CHECK_INSTANCE_CAST((obj), XFDESKTOP_TYPE_FILE_ICON_MANAGER, XfdesktopFileIconManager))
#define XFDESKTOP_IS_FILE_ICON_MANAGER(obj)  (G_TYPE_CHECK_INSTANCE_TYPE((obj), XFDESKTOP_TYPE_FILE_ICON_MANAGER))

typedef struct _XfdesktopFileIconManager         XfdesktopFileIconManager;
typedef struct _XfdesktopFileIconManagerClass    XfdesktopFileIconManagerClass;
typedef struct _XfdesktopFileIconManagerPrivate  XfdesktopFileIconManagerPrivate;

struct _XfdesktopFileIconManager
{
    XfdesktopIconViewManager parent;

    /*< private >*/
    XfdesktopFileIconManagerPrivate *priv;
};

struct _XfdesktopFileIconManagerClass
{
    XfdesktopIconViewManagerClass parent;
};

GType xfdesktop_file_icon_manager_get_type(void) G_GNUC_CONST;

XfdesktopIconViewManager *xfdesktop_file_icon_manager_new(XfwScreen *screen,
                                                          GdkScreen *gdkscreen,
                                                          XfconfChannel *channel,
                                                          GList *desktops,
                                                          GFile *folder);

G_END_DECLS

#endif  /* __XFDESKTOP_FILE_ICON_MANAGER_H__ */
