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
#include <libxfce4windowing/libxfce4windowing.h>

#include "xfce-desktop.h"

G_BEGIN_DECLS

G_DECLARE_DERIVABLE_TYPE(XfdesktopIconViewManager, xfdesktop_icon_view_manager, XFDESKTOP, ICON_VIEW_MANAGER, GObject)
#define XFDESKTOP_TYPE_ICON_VIEW_MANAGER (xfdesktop_icon_view_manager_get_type())

struct _XfdesktopIconViewManagerClass
{
    GObjectClass parent_class;

    /* Virtual Functions */

    void (*desktop_added)(XfdesktopIconViewManager *manager,
                          XfceDesktop *desktop);
    void (*desktop_removed)(XfdesktopIconViewManager *manager,
                            XfceDesktop *desktop);

    GtkMenu *(*get_context_menu)(XfdesktopIconViewManager *manager,
                                 GtkWidget *widget);

    void (*sort_icons)(XfdesktopIconViewManager *manager,
                       GtkSortType sort_type);

    void (*reload)(XfdesktopIconViewManager *manager);
};

XfwScreen *xfdesktop_icon_view_manager_get_screen(XfdesktopIconViewManager *manager);
GList *xfdesktop_icon_view_manager_get_desktops(XfdesktopIconViewManager *manager);
XfconfChannel *xfdesktop_icon_view_manager_get_channel(XfdesktopIconViewManager *manager);

gboolean xfdesktop_icon_view_manager_get_show_icons_on_primary(XfdesktopIconViewManager *manager);

void xfdesktop_icon_view_manager_desktop_added(XfdesktopIconViewManager *manager,
                                               XfceDesktop *desktop);
void xfdesktop_icon_view_manager_desktop_removed(XfdesktopIconViewManager *manager,
                                                 XfceDesktop *desktop);

/* virtual function accessors */

GtkMenu *xfdesktop_icon_view_manager_get_context_menu(XfdesktopIconViewManager *manager,
                                                      GtkWidget *widget);
void xfdesktop_icon_view_manager_sort_icons(XfdesktopIconViewManager *manager,
                                            GtkSortType sort_type);

void xfdesktop_icon_view_manager_reload(XfdesktopIconViewManager *manager);

G_END_DECLS

#endif  /* __XFDESKTOP_ICON_VIEW_MANAGER_H__ */
