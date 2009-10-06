/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2006 Brian Tarricone, <bjt23@cornell.edu>
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __XFDESKTOP_ICON_VIEW_MANAGER_H__
#define __XFDESKTOP_ICON_VIEW_MANAGER_H__

#include <glib-object.h>
#include <gtk/gtk.h>

#include "xfdesktop-icon.h"

G_BEGIN_DECLS

#define XFDESKTOP_TYPE_ICON_VIEW_MANAGER            (xfdesktop_icon_view_manager_get_type())
#define XFDESKTOP_ICON_VIEW_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), XFDESKTOP_TYPE_ICON_VIEW_MANAGER, XfdesktopIconViewManager))
#define XFDESKTOP_IS_ICON_VIEW_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), XFDESKTOP_TYPE_ICON_VIEW_MANAGER))
#define XFDESKTOP_ICON_VIEW_MANAGER_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE((obj), XFDESKTOP_TYPE_ICON_VIEW_MANAGER, XfdesktopIconViewManagerIface))

typedef struct _XfdesktopIconViewManagerIface XfdesktopIconViewManagerIface;
typedef struct _XfdesktopIconViewManager XfdesktopIconViewManager;  /* dummy */

/* fwd decl - meh */
struct _XfdesktopIconView;

struct _XfdesktopIconViewManagerIface
{
    GTypeInterface g_iface;
    
    /*< virtual functions >*/
    gboolean (*manager_init)(XfdesktopIconViewManager *manager,
                             struct _XfdesktopIconView *icon_view);
    void (*manager_fini)(XfdesktopIconViewManager *manager);
    
    gboolean (*drag_drop)(XfdesktopIconViewManager *manager,
                          XfdesktopIcon *drop_icon,
                          GdkDragContext *context,
                          guint16 row,
                          guint16 col,
                          guint time_);
    void (*drag_data_received)(XfdesktopIconViewManager *manager,
                               XfdesktopIcon *drop_icon,
                               GdkDragContext *context,
                               guint16 row,
                               guint16 col,
                               GtkSelectionData *data,
                               guint info,
                               guint time_);
    void (*drag_data_get)(XfdesktopIconViewManager *manager,
                          GList *drag_icons,
                          GdkDragContext *context,
                          GtkSelectionData *data,
                          guint info,
                          guint time_);
};

GType xfdesktop_icon_view_manager_get_type(void) G_GNUC_CONST;

/* virtual function accessors */

gboolean xfdesktop_icon_view_manager_init(XfdesktopIconViewManager *manager,
                                          struct _XfdesktopIconView *icon_view);
void xfdesktop_icon_view_manager_fini(XfdesktopIconViewManager *manager);

gboolean xfdesktop_icon_view_manager_drag_drop(XfdesktopIconViewManager *manager,
                                               XfdesktopIcon *drop_icon,
                                               GdkDragContext *context,
                                               guint16 row,
                                               guint16 col,
                                               guint time_);
void xfdesktop_icon_view_manager_drag_data_received(XfdesktopIconViewManager *manager,
                                                    XfdesktopIcon *drop_icon,
                                                    GdkDragContext *context,
                                                    guint16 row,
                                                    guint16 col,
                                                    GtkSelectionData *data,
                                                    guint info,
                                                    guint time_);
void xfdesktop_icon_view_manager_drag_data_get(XfdesktopIconViewManager *manager,
                                               GList *drag_icons,
                                               GdkDragContext *context,
                                               GtkSelectionData *data,
                                               guint info,
                                               guint time_);



G_END_DECLS

#endif  /* __XFDESKTOP_ICON_VIEW_MANAGER_H__ */
