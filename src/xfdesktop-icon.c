/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2006-2007 Brian Tarricone, <bjt23@cornell.edu>
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <glib-object.h>
#include <gobject/gmarshal.h>

#include "xfdesktop-icon.h"
#include "xfdesktop-marshal.h"

struct _XfdesktopIconPrivate
{
    gint16 row;
    gint16 col;

    GdkRectangle pixbuf_extents;
    GdkRectangle text_extents;
    GdkRectangle total_extents;
};

enum {
    SIG_PIXBUF_CHANGED = 0,
    SIG_LABEL_CHANGED,
    SIG_POS_CHANGED,
    SIG_SELECTED,
    SIG_ACTIVATED,
    SIG_N_SIGNALS,
};


static guint __signals[SIG_N_SIGNALS] = { 0, };


G_DEFINE_ABSTRACT_TYPE(XfdesktopIcon, xfdesktop_icon, G_TYPE_OBJECT)


static void
xfdesktop_icon_class_init(XfdesktopIconClass *klass)
{
    g_type_class_add_private(klass, sizeof(XfdesktopIconPrivate));
    
    __signals[SIG_PIXBUF_CHANGED] = g_signal_new("pixbuf-changed",
                                                 XFDESKTOP_TYPE_ICON,
                                                 G_SIGNAL_RUN_LAST,
                                                 G_STRUCT_OFFSET(XfdesktopIconClass,
                                                                 pixbuf_changed),
                                                 NULL, NULL,
                                                 g_cclosure_marshal_VOID__VOID,
                                                 G_TYPE_NONE, 0);
    
    __signals[SIG_LABEL_CHANGED] = g_signal_new("label-changed",
                                                XFDESKTOP_TYPE_ICON,
                                                G_SIGNAL_RUN_LAST,
                                                G_STRUCT_OFFSET(XfdesktopIconClass,
                                                                label_changed),
                                                NULL, NULL,
                                                g_cclosure_marshal_VOID__VOID,
                                                G_TYPE_NONE, 0);
    
    __signals[SIG_POS_CHANGED] = g_signal_new("position-changed",
                                              XFDESKTOP_TYPE_ICON,
                                              G_SIGNAL_RUN_LAST,
                                              G_STRUCT_OFFSET(XfdesktopIconClass,
                                                              position_changed),
                                              NULL, NULL,
                                              g_cclosure_marshal_VOID__VOID,
                                              G_TYPE_NONE, 0);
    
    __signals[SIG_SELECTED] = g_signal_new("selected",
                                           XFDESKTOP_TYPE_ICON,
                                           G_SIGNAL_RUN_LAST,
                                           G_STRUCT_OFFSET(XfdesktopIconClass,
                                                           selected),
                                           NULL, NULL,
                                           g_cclosure_marshal_VOID__VOID,
                                           G_TYPE_NONE, 0);
    
    __signals[SIG_ACTIVATED] = g_signal_new("activated",
                                            XFDESKTOP_TYPE_ICON,
                                            G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                            G_STRUCT_OFFSET(XfdesktopIconClass,
                                                            activated),
                                            g_signal_accumulator_true_handled,
                                            NULL,
                                            xfdesktop_marshal_BOOLEAN__VOID,
                                            G_TYPE_BOOLEAN, 0);
}

static void
xfdesktop_icon_init(XfdesktopIcon *icon)
{
    icon->priv = G_TYPE_INSTANCE_GET_PRIVATE(icon, XFDESKTOP_TYPE_ICON,
                                             XfdesktopIconPrivate);
}


void
xfdesktop_icon_set_position(XfdesktopIcon *icon,
                            gint16 row,
                            gint16 col)
{
    g_return_if_fail(XFDESKTOP_IS_ICON(icon));
    
    icon->priv->row = row;
    icon->priv->col = col;
    
    g_signal_emit(G_OBJECT(icon), __signals[SIG_POS_CHANGED], 0, NULL);
}

gboolean
xfdesktop_icon_get_position(XfdesktopIcon *icon,
                            guint16 *row,
                            guint16 *col)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON(icon) && row && col, FALSE);
    
    *row = icon->priv->row;
    *col = icon->priv->col;
    
    return TRUE;
}

void
xfdesktop_icon_set_extents(XfdesktopIcon *icon,
                           const GdkRectangle *pixbuf_extents,
                           const GdkRectangle *text_extents,
                           const GdkRectangle *total_extents)
{
    g_return_if_fail(XFDESKTOP_IS_ICON(icon) && pixbuf_extents
                     && text_extents && total_extents);

    icon->priv->pixbuf_extents = *pixbuf_extents;
    icon->priv->text_extents = *text_extents;
    icon->priv->total_extents = *total_extents;
}

gboolean
xfdesktop_icon_get_extents(XfdesktopIcon *icon,
                           GdkRectangle *pixbuf_extents,
                           GdkRectangle *text_extents,
                           GdkRectangle *total_extents)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON(icon), FALSE);

    if(pixbuf_extents)
        *pixbuf_extents = icon->priv->pixbuf_extents;
    if(text_extents)
        *text_extents = icon->priv->text_extents;
    if(total_extents)
        *total_extents = icon->priv->total_extents;

    return TRUE;
}

/*< required >*/
GdkPixbuf *
xfdesktop_icon_peek_pixbuf(XfdesktopIcon *icon,
                           gint size)
{
    XfdesktopIconClass *klass;
    
    g_return_val_if_fail(XFDESKTOP_IS_ICON(icon), NULL);
    klass = XFDESKTOP_ICON_GET_CLASS(icon);
    g_return_val_if_fail(klass->peek_pixbuf, NULL);
    
    return klass->peek_pixbuf(icon, size);
}

/*< required >*/
G_CONST_RETURN gchar *
xfdesktop_icon_peek_label(XfdesktopIcon *icon)
{
    XfdesktopIconClass *klass;
    
    g_return_val_if_fail(XFDESKTOP_IS_ICON(icon), NULL);
    klass = XFDESKTOP_ICON_GET_CLASS(icon);
    g_return_val_if_fail(klass->peek_label, NULL);
    
    return klass->peek_label(icon);
}

/*< optional; drags aren't allowed if not provided >*/
GdkDragAction
xfdesktop_icon_get_allowed_drag_actions(XfdesktopIcon *icon)
{
    XfdesktopIconClass *klass;
    
    g_return_val_if_fail(XFDESKTOP_IS_ICON(icon), FALSE);
    
    klass = XFDESKTOP_ICON_GET_CLASS(icon);
    
    if(!klass->get_allowed_drag_actions)
        return 0;
    
    return klass->get_allowed_drag_actions(icon);
}

/*< optional; drops aren't allowed if not provided >*/
GdkDragAction
xfdesktop_icon_get_allowed_drop_actions(XfdesktopIcon *icon)
{
    XfdesktopIconClass *klass;
    
    g_return_val_if_fail(XFDESKTOP_IS_ICON(icon), FALSE);
    
    klass = XFDESKTOP_ICON_GET_CLASS(icon);
    
    if(!klass->get_allowed_drop_actions)
        return 0;
    
    return klass->get_allowed_drop_actions(icon);
}

/*< optional; required if get_allowed_drop_actions() can return nonzero >*/
gboolean
xfdesktop_icon_do_drop_dest(XfdesktopIcon *icon,
                            XfdesktopIcon *src_icon,
                            GdkDragAction action)
{
    XfdesktopIconClass *klass;
    
    g_return_val_if_fail(XFDESKTOP_IS_ICON(icon), FALSE);
    klass = XFDESKTOP_ICON_GET_CLASS(icon);    
    g_return_val_if_fail(klass->do_drop_dest, FALSE);
    
    return klass->do_drop_dest(icon, src_icon, action);
}

/*< optional >*/
G_CONST_RETURN gchar *
xfdesktop_icon_peek_tooltip(XfdesktopIcon *icon)
{
    XfdesktopIconClass *klass;
    
    g_return_val_if_fail(XFDESKTOP_IS_ICON(icon), NULL);
    
    klass = XFDESKTOP_ICON_GET_CLASS(icon);
    
    if(!klass->peek_tooltip)
        return NULL;
    
    return klass->peek_tooltip(icon);
}

/*< optional >*/
void xfdesktop_icon_delete_thumbnail(XfdesktopIcon *icon)
{
    XfdesktopIconClass *klass;

    g_return_if_fail(XFDESKTOP_IS_ICON(icon));

    klass = XFDESKTOP_ICON_GET_CLASS(icon);

    if(!klass->delete_thumbnail_file)
        return;

    klass->delete_thumbnail_file(icon);
}

/*< optional >*/
void
xfdesktop_icon_set_thumbnail_file(XfdesktopIcon *icon, GFile *file)
{
    XfdesktopIconClass *klass;

    g_return_if_fail(XFDESKTOP_IS_ICON(icon));

    klass = XFDESKTOP_ICON_GET_CLASS(icon);

    if(!klass->set_thumbnail_file)
        return;

    klass->set_thumbnail_file(icon, file);
}

/*< optional >*/
gboolean
xfdesktop_icon_populate_context_menu(XfdesktopIcon *icon,
                                     GtkWidget *menu)
{
    XfdesktopIconClass *klass;
    
    g_return_val_if_fail(XFDESKTOP_IS_ICON(icon), FALSE);
    
    klass = XFDESKTOP_ICON_GET_CLASS(icon);
    
    if(!klass->populate_context_menu)
        return FALSE;
    
    return klass->populate_context_menu(icon, menu);
}

GtkWidget *
xfdesktop_icon_peek_icon_view(XfdesktopIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON(icon), NULL);
    return g_object_get_data(G_OBJECT(icon), "--xfdesktop-icon-view");
}

/*< signal triggers >*/

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

void
xfdesktop_icon_position_changed(XfdesktopIcon *icon)
{
    g_return_if_fail(XFDESKTOP_IS_ICON(icon));
    g_signal_emit(icon, __signals[SIG_POS_CHANGED], 0);
}


void
xfdesktop_icon_selected(XfdesktopIcon *icon)
{
    g_return_if_fail(XFDESKTOP_IS_ICON(icon));
    g_signal_emit(G_OBJECT(icon), __signals[SIG_SELECTED], 0, NULL);
}

gboolean
xfdesktop_icon_activated(XfdesktopIcon *icon)
{
    gboolean ret = FALSE;
    
    g_return_val_if_fail(XFDESKTOP_IS_ICON(icon), FALSE);
    
    g_signal_emit(G_OBJECT(icon), __signals[SIG_ACTIVATED], 0, &ret);
    
    return ret;
}
