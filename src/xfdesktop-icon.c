/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2006-2007 Brian Tarricone, <brian@tarricone.org>
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

#include "libxfce4windowing/libxfce4windowing.h"
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <glib-object.h>
#include <gobject/gmarshal.h>

#include "xfdesktop-icon.h"

struct _XfdesktopIconPrivate
{
    XfwMonitor *monitor;
    gint16 row;
    gint16 col;
};

enum {
    SIG_PIXBUF_CHANGED = 0,
    SIG_LABEL_CHANGED,
    SIG_POS_CHANGED,
    SIG_N_SIGNALS,
};


static guint __signals[SIG_N_SIGNALS] = { 0, };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(XfdesktopIcon, xfdesktop_icon, G_TYPE_OBJECT)


static void
xfdesktop_icon_class_init(XfdesktopIconClass *klass)
{
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
}

static void
xfdesktop_icon_init(XfdesktopIcon *icon)
{
    icon->priv = xfdesktop_icon_get_instance_private(icon);
    icon->priv->row = -1;
    icon->priv->col = -1;
}

gboolean
xfdesktop_icon_set_monitor(XfdesktopIcon *icon, XfwMonitor *monitor) {
    g_return_val_if_fail(XFDESKTOP_IS_ICON(icon), FALSE);
    g_return_val_if_fail(monitor == NULL || XFW_IS_MONITOR(monitor), FALSE);

    if (icon->priv->monitor != monitor) {
        icon->priv->monitor = monitor;
        return TRUE;
    } else {
        return FALSE;
    }
}

XfwMonitor *
xfdesktop_icon_get_monitor(XfdesktopIcon *icon) {
    g_return_val_if_fail(XFDESKTOP_IS_ICON(icon), NULL);
    return icon->priv->monitor;
}

gboolean
xfdesktop_icon_set_position(XfdesktopIcon *icon,
                            gint16 row,
                            gint16 col)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON(icon), FALSE);
    g_return_val_if_fail((row >= 0 && col >= 0) || (row == -1 && col == -1), FALSE);

    if (row != icon->priv->row || col != icon->priv->col) {
        icon->priv->row = row;
        icon->priv->col = col;
        g_signal_emit(G_OBJECT(icon), __signals[SIG_POS_CHANGED], 0, NULL);
        return TRUE;
    } else {
        return FALSE;
    }
}

gboolean
xfdesktop_icon_get_position(XfdesktopIcon *icon,
                            gint16 *row,
                            gint16 *col)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON(icon) && row && col, FALSE);

    if (icon->priv->row != -1 && icon->priv->col != -1) {
        *row = icon->priv->row;
        *col = icon->priv->col;
        return TRUE;
    } else {
        return FALSE;
    }
}

/*< required >*/
const gchar *
xfdesktop_icon_peek_label(XfdesktopIcon *icon)
{
    XfdesktopIconClass *klass;

    g_return_val_if_fail(XFDESKTOP_IS_ICON(icon), NULL);
    klass = XFDESKTOP_ICON_GET_CLASS(icon);
    g_return_val_if_fail(klass->peek_label, NULL);

    return klass->peek_label(icon);
}

/*< required >*/
gchar *
xfdesktop_icon_get_identifier(XfdesktopIcon *icon)
{
    XfdesktopIconClass *klass;

    g_return_val_if_fail(XFDESKTOP_IS_ICON(icon), NULL);

    klass = XFDESKTOP_ICON_GET_CLASS(icon);

    if(!klass->get_identifier)
        return NULL;

    return klass->get_identifier(icon);
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
xfdesktop_icon_get_allowed_drop_actions(XfdesktopIcon *icon,
                                        GdkDragAction *suggested_action)
{
    XfdesktopIconClass *klass;

    g_return_val_if_fail(XFDESKTOP_IS_ICON(icon), FALSE);

    klass = XFDESKTOP_ICON_GET_CLASS(icon);

    if(!klass->get_allowed_drop_actions) {
        if(suggested_action)
            *suggested_action = 0;
        return 0;
    }

    return klass->get_allowed_drop_actions(icon, suggested_action);
}

/*< optional; required if get_allowed_drop_actions() can return nonzero >*/
gboolean
xfdesktop_icon_do_drop_dest(XfdesktopIcon *icon,
                            GList *src_icons,
                            GdkDragAction action)
{
    XfdesktopIconClass *klass;

    g_return_val_if_fail(XFDESKTOP_IS_ICON(icon), FALSE);
    klass = XFDESKTOP_ICON_GET_CLASS(icon);
    g_return_val_if_fail(klass->do_drop_dest, FALSE);

    return klass->do_drop_dest(icon, src_icons, action);
}

/*< optional >*/
const gchar *
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

    if (klass->set_thumbnail_file == NULL) {
        g_object_unref(file);
    } else {
        klass->set_thumbnail_file(icon, file);
    }
}

gboolean
xfdesktop_icon_activate(XfdesktopIcon *icon,
                        GtkWindow *window)
{
    XfdesktopIconClass *klass;

    g_return_val_if_fail(XFDESKTOP_IS_ICON(icon), FALSE);
    g_return_val_if_fail(GTK_IS_WINDOW(window), FALSE);

    klass = XFDESKTOP_ICON_GET_CLASS(icon);

    if (klass->activate != NULL) {
        return klass->activate(icon, window);
    } else {
        return FALSE;
    }
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
