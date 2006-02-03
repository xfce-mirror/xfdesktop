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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>
#include <gobject/gmarshal.h>

#include "xfdesktop-icon.h"

enum {
    SIG_PIXBUF_CHANGED = 0,
    SIG_LABEL_CHANGED,
    SIG_N_SIGNALS,
};

static void xfdesktop_icon_base_init(gpointer g_class);

static guint __signals[SIG_N_SIGNALS] = { 0, };

GType
xfdesktop_icon_get_type()
{
    static GType icon_type = 0;
    
    if(!icon_type) {
        static const GTypeInfo icon_info = {
            sizeof(XfdesktopIconIface),
            xfdesktop_icon_base_init,
            NULL,
            NULL,
            NULL,
            NULL,
            0,
            0,
            NULL,
        };
        
        icon_type = g_type_register_static(G_TYPE_INTERFACE, "XfdesktopIcon",
                                           &icon_info, 0);
        g_type_interface_add_prerequisite(icon_type, G_TYPE_OBJECT);
    }
    
    return icon_type;
}

static void
xfdesktop_icon_base_init(gpointer g_class)
{
    static gboolean __inited = FALSE;
    
    if(G_UNLIKELY(!__inited)) {
        __signals[SIG_PIXBUF_CHANGED] = g_signal_new("pixbuf-changed",
                                                     XFDESKTOP_TYPE_ICON,
                                                     G_SIGNAL_RUN_LAST,
                                                     G_STRUCT_OFFSET(XfdesktopIconIface,
                                                                     pixbuf_changed),
                                                     NULL, NULL,
                                                     g_cclosure_marshal_VOID__VOID,
                                                     G_TYPE_NONE, 0);
        
        __signals[SIG_LABEL_CHANGED] = g_signal_new("label-changed",
                                                    XFDESKTOP_TYPE_ICON,
                                                    G_SIGNAL_RUN_LAST,
                                                    G_STRUCT_OFFSET(XfdesktopIconIface,
                                                                    label_changed),
                                                    NULL, NULL,
                                                    g_cclosure_marshal_VOID__VOID,
                                                    G_TYPE_NONE, 0);
        
        __inited = TRUE;
    }
}

GdkPixbuf *
xfdesktop_icon_get_pixbuf(XfdesktopIcon *icon,
                          gint size)
{
    XfdesktopIconIface *iface;
    
    g_return_val_if_fail(XFDESKTOP_IS_ICON(icon), NULL);
    
    iface = XFDESKTOP_ICON_GET_IFACE(icon);
    g_return_val_if_fail(iface->peek_pixbuf, NULL);
    
    return iface->peek_pixbuf(icon, size);
}

G_CONST_RETURN gchar *
xfdesktop_icon_peek_label(XfdesktopIcon *icon)
{
    XfdesktopIconIface *iface;
    
    g_return_val_if_fail(XFDESKTOP_IS_ICON(icon), NULL);
    
    iface = XFDESKTOP_ICON_GET_IFACE(icon);
    g_return_val_if_fail(iface->peek_label, NULL);
    
    return iface->peek_label(icon);
}

void
xfdesktop_icon_selected(XfdesktopIcon *icon)
{
    XfdesktopIconIface *iface;
    
    g_return_if_fail(XFDESKTOP_IS_ICON(icon));
    
    iface = XFDESKTOP_ICON_GET_IFACE(icon);
    g_return_if_fail(iface->selected);
    
    iface->selected(icon);
}

void
xfdesktop_icon_activated(XfdesktopIcon *icon)
{
    XfdesktopIconIface *iface;
    
    g_return_if_fail(XFDESKTOP_IS_ICON(icon));
    
    iface = XFDESKTOP_ICON_GET_IFACE(icon);
    g_return_if_fail(iface->activated);
    
    iface->activated(icon);
}

void
xfdesktop_icon_menu_popup(XfdesktopIcon *icon)
{
    XfdesktopIconIface *iface;
    
    g_return_if_fail(XFDESKTOP_IS_ICON(icon));
    
    iface = XFDESKTOP_ICON_GET_IFACE(icon);
    g_return_if_fail(iface->menu_popup);
    
    iface->menu_popup(icon);
}

void
xfdesktop_icon_position_changed(XfdesktopIcon *icon,
                                gint new_row,
                                gint new_col)
{
    XfdesktopIconIface *iface;
    
    g_return_if_fail(XFDESKTOP_IS_ICON(icon));
    
    iface = XFDESKTOP_ICON_GET_IFACE(icon);
    g_return_if_fail(iface->position_changed);
    
    iface->position_changed(icon, new_row, new_col);
}

void
xfdesktop_icon_pixbuf_changed(XfdesktopIcon *icon)
{
    g_return_if_fail(XFDESKTOP_IS_ICON(icon));
    
    g_signal_emit(icon, __signals[SIG_PIXBUF_CHANGED], 0);
}

void
xfdesktop_icon_label_changed(XfdesktopIcon *icon)
{
    g_return_if_fail(XFDESKTOP_IS_ICON(icon));
    
    g_signal_emit(icon, __signals[SIG_LABEL_CHANGED], 0);
}
