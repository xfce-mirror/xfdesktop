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

#ifndef __XFDESKTOP_ICON_H__
#define __XFDESKTOP_ICON_H__

#include <glib-object.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS

#define XFDESKTOP_TYPE_ICON            (xfdesktop_icon_get_type())
#define XFDESKTOP_ICON(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), XFDESKTOP_TYPE_ICON, XfdesktopIcon))
#define XFDESKTOP_IS_ICON(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), XFDESKTOP_TYPE_ICON))
#define XFDESKTOP_ICON_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE((obj), XFDESKTOP_TYPE_ICON, XfdesktopIconIface))

typedef enum
{
    XFDESKTOP_ICON_DRAG_FAILED = 0,
    XFDESKTOP_ICON_DRAG_SUCCEEDED_MOVE_ICON,
    XFDESKTOP_ICON_DRAG_SUCCEEDED_NO_ACTION,
} XfdesktopIconDragResult;

typedef struct _XfdesktopIconIface XfdesktopIconIface;
typedef struct _XfdesktopIcon      XfdesktopIcon;  /* dummy */

struct _XfdesktopIconIface
{
    GTypeInterface g_iface;
    
    /*< signals >*/
    void (*pixbuf_changed)(XfdesktopIcon *icon);
    void (*label_changed)(XfdesktopIcon *icon);
    
    void (*selected)(XfdesktopIcon *icon);
    void (*activated)(XfdesktopIcon *icon);
    void (*menu_popup)(XfdesktopIcon *icon);
    
    /*< virtual functions >*/
    GdkPixbuf *(*peek_pixbuf)(XfdesktopIcon *icon, gint size);
    G_CONST_RETURN gchar *(*peek_label)(XfdesktopIcon *icon);
    
    void (*set_position)(XfdesktopIcon *icon, gint16 row, gint16 col);
    gboolean (*get_position)(XfdesktopIcon *icon, gint16 *row, gint16 *col);
    
    void (*set_extents)(XfdesktopIcon *icon, const GdkRectangle *extents);
    gboolean (*get_extents)(XfdesktopIcon *icon, GdkRectangle *extents);
    
    gboolean (*is_drop_dest)(XfdesktopIcon *icon);
    XfdesktopIconDragResult (*do_drop_dest)(XfdesktopIcon *icon, XfdesktopIcon *src_icon, GdkDragAction action);
};

GType xfdesktop_icon_get_type() G_GNUC_CONST;

/* virtual function accessors */

GdkPixbuf *xfdesktop_icon_peek_pixbuf(XfdesktopIcon *icon,
                                     gint size);
G_CONST_RETURN gchar *xfdesktop_icon_peek_label(XfdesktopIcon *icon);

void xfdesktop_icon_set_position(XfdesktopIcon *icon,
                                 gint16 row,
                                 gint16 col);
gboolean xfdesktop_icon_get_position(XfdesktopIcon *icon,
                                     guint16 *row,
                                     guint16 *col);

void xfdesktop_icon_set_extents(XfdesktopIcon *icon,
                                const GdkRectangle *extents);
gboolean xfdesktop_icon_get_extents(XfdesktopIcon *icon,
                                    GdkRectangle *extents);

gboolean xfdesktop_icon_is_drop_dest(XfdesktopIcon *icon);
XfdesktopIconDragResult xfdesktop_icon_do_drop_dest(XfdesktopIcon *icon,
                                                    XfdesktopIcon *src_icon,
                                                    GdkDragAction action);

/*< signal triggers >*/

void xfdesktop_icon_pixbuf_changed(XfdesktopIcon *icon);
void xfdesktop_icon_label_changed(XfdesktopIcon *icon);

void xfdesktop_icon_selected(XfdesktopIcon *icon);
void xfdesktop_icon_activated(XfdesktopIcon *icon);
void xfdesktop_icon_menu_popup(XfdesktopIcon *icon);

G_END_DECLS

#endif  /* __XFDESKTOP_ICON_H__ */
