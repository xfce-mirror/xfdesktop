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

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define XFDESKTOP_TYPE_ICON            (xfdesktop_icon_get_type())
#define XFDESKTOP_ICON(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), XFDESKTOP_TYPE_ICON, XfdesktopIcon))
#define XFDESKTOP_ICON_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), XFDESKTOP_TYPE_ICON, XfdesktopIconClass))
#define XFDESKTOP_IS_ICON(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), XFDESKTOP_TYPE_ICON))
#define XFDESKTOP_ICON_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), XFDESKTOP_TYPE_ICON, XfdesktopIconClass))

typedef enum
{
    XFDESKTOP_ICON_DRAG_FAILED = 0,
    XFDESKTOP_ICON_DRAG_SUCCEEDED_MOVE_ICON,
    XFDESKTOP_ICON_DRAG_SUCCEEDED_NO_ACTION,
} XfdesktopIconDragResult;

typedef struct _XfdesktopIcon        XfdesktopIcon;
typedef struct _XfdesktopIconClass   XfdesktopIconClass;
typedef struct _XfdesktopIconPrivate XfdesktopIconPrivate;

struct _XfdesktopIcon
{
    GObject parent;
    
    /*< private >*/
    XfdesktopIconPrivate *priv;
};

struct _XfdesktopIconClass
{
    GObjectClass parent;
    
    /*< signals >*/
    void (*pixbuf_changed)(XfdesktopIcon *icon);
    void (*label_changed)(XfdesktopIcon *icon);
    
    void (*position_changed)(XfdesktopIcon *icon);
    
    void (*selected)(XfdesktopIcon *icon);
    /* XfdektopIcon::activated has weird semantics: you should NEVER connect to
     * this signal normally: always use g_signal_connect_after(), as the default
     * signal handler may do some special setup for the icon.  this is lame;
     * you should be able to use normal g_signal_connect(), but signal handlers
     * with return values are (for some unknown reason) not allowed to be
     * G_SIGNAL_RUN_FIRST.  go figure. */
    gboolean (*activated)(XfdesktopIcon *icon);
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
    
    G_CONST_RETURN gchar *(*peek_tooltip)(XfdesktopIcon *icon);
    
    GtkWidget *(*get_popup_menu)(XfdesktopIcon *icon);
};

GType xfdesktop_icon_get_type() G_GNUC_CONST;

/* virtual function accessors */

GdkPixbuf *xfdesktop_icon_peek_pixbuf(XfdesktopIcon *icon,
                                     gint size);
G_CONST_RETURN gchar *xfdesktop_icon_peek_label(XfdesktopIcon *icon);
G_CONST_RETURN gchar *xfdesktop_icon_peek_tooltip(XfdesktopIcon *icon);

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

GtkWidget *xfdesktop_icon_get_popup_menu(XfdesktopIcon *icon);

GtkWidget *xfdesktop_icon_peek_icon_view(XfdesktopIcon *icon);

/*< signal triggers >*/

void xfdesktop_icon_pixbuf_changed(XfdesktopIcon *icon);
void xfdesktop_icon_label_changed(XfdesktopIcon *icon);
void xfdesktop_icon_position_changed(XfdesktopIcon *icon);

void xfdesktop_icon_selected(XfdesktopIcon *icon);
gboolean xfdesktop_icon_activated(XfdesktopIcon *icon);
void xfdesktop_icon_menu_popup(XfdesktopIcon *icon);

G_END_DECLS

#endif  /* __XFDESKTOP_ICON_H__ */
