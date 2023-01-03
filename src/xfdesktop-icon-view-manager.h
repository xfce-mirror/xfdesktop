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

#ifndef __XFDESKTOP_ICON_VIEW_MANAGER_H__
#define __XFDESKTOP_ICON_VIEW_MANAGER_H__

#include <gtk/gtk.h>

#include <xfconf/xfconf.h>

#include "xfdesktop-icon.h"
#include "xfdesktop-icon-view.h"

G_BEGIN_DECLS

#define XFDESKTOP_TYPE_ICON_VIEW_MANAGER            (xfdesktop_icon_view_manager_get_type())
#define XFDESKTOP_ICON_VIEW_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), XFDESKTOP_TYPE_ICON_VIEW_MANAGER, XfdesktopIconViewManager))
#define XFDESKTOP_IS_ICON_VIEW_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), XFDESKTOP_TYPE_ICON_VIEW_MANAGER))
#define XFDESKTOP_ICON_VIEW_MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), XFDESKTOP_TYPE_ICON_VIEW_MANAGER, XfdesktopIconViewManagerClass))
#define XFDESKTOP_ICON_VIEW_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), XFDESKTOP_TYPE_ICON_VIEW_MANAGER, XfdesktopIconViewManagerClass))

typedef struct _XfdesktopIconViewManager XfdesktopIconViewManager;
typedef struct _XfdesktopIconViewManagerPrivate XfdesktopIconViewManagerPrivate;
typedef struct _XfdesktopIconViewManagerClass XfdesktopIconViewManagerClass;

struct _XfdesktopIconViewManager
{
    GObject parent;

    /*< private >*/
    XfdesktopIconViewManagerPrivate *priv;
};

struct _XfdesktopIconViewManagerClass
{
    GObjectClass parent_class;

    /* Virtual Functions */

    void (*populate_context_menu)(XfdesktopIconViewManager *manager,
                                  GtkMenuShell *menu);

    void (*sort_icons)(XfdesktopIconViewManager *manager,
                       GtkSortType sort_type);
};

GType xfdesktop_icon_view_manager_get_type(void) G_GNUC_CONST;

GtkWidget *xfdesktop_icon_view_manager_get_parent(XfdesktopIconViewManager *manager);
GtkFixed *xfdesktop_icon_view_manager_get_container(XfdesktopIconViewManager *manager);
XfconfChannel *xfdesktop_icon_view_manager_get_channel(XfdesktopIconViewManager *manager);

gboolean xfdesktop_icon_view_manager_get_show_icons_on_primary(XfdesktopIconViewManager *manager);

void xfdesktop_icon_view_manager_get_workarea(XfdesktopIconViewManager *manager,
                                              GdkRectangle *workarea);

/* virtual function accessors */

void xfdesktop_icon_view_manager_populate_context_menu(XfdesktopIconViewManager *manager,
                                                       GtkMenuShell *menu);
void xfdesktop_icon_view_manager_sort_icons(XfdesktopIconViewManager *manager,
                                            GtkSortType sort_type);

G_END_DECLS

#endif  /* __XFDESKTOP_ICON_VIEW_MANAGER_H__ */
