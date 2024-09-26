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

#define GET_PRIVATE(icon) ((XfdesktopIconPrivate *)xfdesktop_icon_get_instance_private(XFDESKTOP_ICON(icon)))

typedef struct _XfdesktopIconPrivate
{
    gchar *identifier;
    XfwMonitor *monitor;
    gint16 row;
    gint16 col;
} XfdesktopIconPrivate;

enum {
    SIG_PIXBUF_CHANGED = 0,
    SIG_LABEL_CHANGED,
    SIG_POS_CHANGED,
    SIG_N_SIGNALS,
};

static void xfdesktop_icon_finalize(GObject *object);


static guint __signals[SIG_N_SIGNALS] = { 0, };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(XfdesktopIcon, xfdesktop_icon, G_TYPE_OBJECT)


static void
xfdesktop_icon_class_init(XfdesktopIconClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = xfdesktop_icon_finalize;

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
    XfdesktopIconPrivate *priv = GET_PRIVATE(icon);
    priv->row = -1;
    priv->col = -1;
}

static void
xfdesktop_icon_finalize(GObject *object) {
    XfdesktopIconPrivate *priv = GET_PRIVATE(object);
    g_free(priv->identifier);

    G_OBJECT_CLASS(xfdesktop_icon_parent_class)->finalize(object);
}

gboolean
xfdesktop_icon_set_monitor(XfdesktopIcon *icon, XfwMonitor *monitor) {
    g_return_val_if_fail(XFDESKTOP_IS_ICON(icon), FALSE);
    g_return_val_if_fail(monitor == NULL || XFW_IS_MONITOR(monitor), FALSE);

    XfdesktopIconPrivate *priv = GET_PRIVATE(icon);
    if (priv->monitor != monitor) {
        priv->monitor = monitor;
        return TRUE;
    } else {
        return FALSE;
    }
}

XfwMonitor *
xfdesktop_icon_get_monitor(XfdesktopIcon *icon) {
    g_return_val_if_fail(XFDESKTOP_IS_ICON(icon), NULL);
    return GET_PRIVATE(icon)->monitor;
}

gboolean
xfdesktop_icon_set_position(XfdesktopIcon *icon,
                            gint16 row,
                            gint16 col)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON(icon), FALSE);
    g_return_val_if_fail((row >= 0 && col >= 0) || (row == -1 && col == -1), FALSE);

    XfdesktopIconPrivate *priv = GET_PRIVATE(icon);
    if (row != priv->row || col != priv->col) {
        priv->row = row;
        priv->col = col;
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

    XfdesktopIconPrivate *priv = GET_PRIVATE(icon);
    if (priv->row != -1 && priv->col != -1) {
        *row = priv->row;
        *col = priv->col;
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
const gchar *
xfdesktop_icon_peek_identifier(XfdesktopIcon *icon)
{
    g_return_val_if_fail(XFDESKTOP_IS_ICON(icon), NULL);

    XfdesktopIconPrivate *priv = GET_PRIVATE(icon);
    if (priv->identifier == NULL) {
        XfdesktopIconClass *klass = XFDESKTOP_ICON_GET_CLASS(icon);
        g_return_val_if_fail(klass->get_identifier != NULL, NULL);
        priv->identifier = klass->get_identifier(icon);
    }

    return priv->identifier;
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
