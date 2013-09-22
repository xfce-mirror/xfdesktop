/*
 *  xfworkspace - xfce4's desktop manager
 *
 *  Copyright (c) 2004-2007 Brian Tarricone, <bjt23@cornell.edu>
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
 *
 *  Random portions taken from or inspired by the original xfworkspace for xfce4:
 *     Copyright (C) 2002-2003 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *     Copyright (C) 2003 Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include <ctype.h>
#include <errno.h>

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <glib.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include <xfconf/xfconf.h>

#include "xfdesktop-common.h"
#include "xfce-workspace.h"
#include "xfce-desktop-enum-types.h"

struct _XfceWorkspacePriv
{
    GdkScreen *gscreen;

    XfconfChannel *channel;
    gchar *property_prefix;

    guint workspace_num;
    guint nbackdrops;
    gboolean xinerama_stretch;
    XfceBackdrop **backdrops;

    gulong *first_color_id;
    gulong *second_color_id;
};

enum
{
    WORKSPACE_BACKDROP_CHANGED,
    N_SIGNALS,
};

static guint signals[N_SIGNALS] = { 0, };

static void xfce_workspace_finalize(GObject *object);
static void xfce_workspace_set_property(GObject *object,
                                      guint property_id,
                                      const GValue *value,
                                      GParamSpec *pspec);
static void xfce_workspace_get_property(GObject *object,
                                      guint property_id,
                                      GValue *value,
                                      GParamSpec *pspec);

static void xfce_workspace_connect_backdrop_settings(XfceWorkspace *workspace,
                                                   XfceBackdrop *backdrop,
                                                   guint monitor);
static void xfce_workspace_disconnect_backdrop_settings(XfceWorkspace *workspace,
                                                        XfceBackdrop *backdrop,
                                                        guint monitor);

static void xfce_workspace_remove_backdrops(XfceWorkspace *workspace);

G_DEFINE_TYPE(XfceWorkspace, xfce_workspace, G_TYPE_OBJECT)

gboolean
xfce_workspace_get_xinerama_stretch(XfceWorkspace *workspace)
{
    g_return_val_if_fail(XFCE_IS_WORKSPACE(workspace), FALSE);
    g_return_val_if_fail(workspace->priv->backdrops != NULL, FALSE);
    g_return_val_if_fail(XFCE_IS_BACKDROP(workspace->priv->backdrops[0]), FALSE);

    return xfce_backdrop_get_image_style(workspace->priv->backdrops[0]) == XFCE_BACKDROP_IMAGE_SPANNING_SCREENS;
}

static void
backdrop_cycle_cb(XfceBackdrop *backdrop, gpointer user_data)
{
    const gchar* backdrop_file;
    gchar *new_backdrop = NULL;
    XfceWorkspace *workspace = XFCE_WORKSPACE(user_data);

    TRACE("entering");

    g_return_if_fail(XFCE_IS_BACKDROP(backdrop));

    backdrop_file = xfce_backdrop_get_image_filename(backdrop);

    if(backdrop_file == NULL)
        return;

    if(xfce_backdrop_get_random_order(backdrop)) {
        DBG("Random! current file: %s", backdrop_file);
        new_backdrop = xfdesktop_backdrop_choose_random(backdrop_file);
    } else {
        DBG("Next! current file: %s", backdrop_file);
        new_backdrop = xfdesktop_backdrop_choose_next(backdrop_file);
    }
    DBG("new file: %s for Workspace %d", new_backdrop,
        xfce_workspace_get_workspace_num(workspace));

    if(g_strcmp0(backdrop_file, new_backdrop) != 0) {
        xfce_backdrop_set_image_filename(backdrop, new_backdrop);
        g_free(new_backdrop);
    }
}

static void
backdrop_changed_cb(XfceBackdrop *backdrop, gpointer user_data)
{
    XfceWorkspace *workspace = XFCE_WORKSPACE(user_data);
    TRACE("entering");

    /* if we were spanning all the screens and we're not doing it anymore
     * we need to update all the backdrops for this workspace */
    if(workspace->priv->xinerama_stretch == TRUE &&
       xfce_workspace_get_xinerama_stretch(workspace) == FALSE) {
        guint i;

        for(i = 0; i < workspace->priv->nbackdrops; ++i) {
            /* skip the current backdrop, we'll get it last */
            if(workspace->priv->backdrops[i] != backdrop) {
                g_signal_emit(G_OBJECT(user_data),
                              signals[WORKSPACE_BACKDROP_CHANGED],
                              0,
                              workspace->priv->backdrops[i]);
            }
        }
    }

    workspace->priv->xinerama_stretch = xfce_workspace_get_xinerama_stretch(workspace);

    /* Propagate it up */
    g_signal_emit(G_OBJECT(user_data), signals[WORKSPACE_BACKDROP_CHANGED], 0, backdrop);
}

void
xfce_workspace_monitors_changed(XfceWorkspace *workspace,
                                GdkScreen *gscreen)
{
    guint i;
    guint n_monitors;
    GdkVisual *vis = NULL;

    TRACE("entering");

    vis = gdk_screen_get_rgba_visual(gscreen);
    if(vis == NULL)
        vis = gdk_screen_get_system_visual(gscreen);

    if(workspace->priv->nbackdrops > 0 &&
       xfce_workspace_get_xinerama_stretch(workspace)) {
        /* When spanning screens we only need one backdrop */
        n_monitors = 1;
    } else {
        n_monitors = gdk_screen_get_n_monitors(gscreen);
    }

    /* Remove all backdrops so that the correct monitor is added/removed and
     * things stay in the correct order */
    xfce_workspace_remove_backdrops(workspace);

    /* Allocate space for the backdrops and their color properties so they
     * can correctly be removed */
    workspace->priv->backdrops = g_realloc(workspace->priv->backdrops,
                                           sizeof(XfceBackdrop *) * n_monitors);
    workspace->priv->first_color_id = g_realloc(workspace->priv->first_color_id,
                                                sizeof(gulong) * n_monitors);
    workspace->priv->second_color_id = g_realloc(workspace->priv->second_color_id,
                                                 sizeof(gulong) * n_monitors);

    for(i = 0; i < n_monitors; ++i) {
        DBG("Adding workspace %d backdrop %d", workspace->priv->workspace_num, i);

        workspace->priv->backdrops[i] = xfce_backdrop_new(vis);
        xfce_workspace_connect_backdrop_settings(workspace,
                                               workspace->priv->backdrops[i],
                                               i);
        g_signal_connect(G_OBJECT(workspace->priv->backdrops[i]),
                         "changed",
                         G_CALLBACK(backdrop_changed_cb), workspace);
        g_signal_connect(G_OBJECT(workspace->priv->backdrops[i]),
                         "cycle",
                         G_CALLBACK(backdrop_cycle_cb),
                         workspace);
        g_signal_connect(G_OBJECT(workspace->priv->backdrops[i]),
                         "ready",
                         G_CALLBACK(backdrop_changed_cb), workspace);
    }
    workspace->priv->nbackdrops = n_monitors;
}

static void
xfce_workspace_class_init(XfceWorkspaceClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;

    g_type_class_add_private(klass, sizeof(XfceWorkspacePriv));

    gobject_class->finalize = xfce_workspace_finalize;
    gobject_class->set_property = xfce_workspace_set_property;
    gobject_class->get_property = xfce_workspace_get_property;

    signals[WORKSPACE_BACKDROP_CHANGED] = g_signal_new("workspace-backdrop-changed",
                                                   XFCE_TYPE_WORKSPACE,
                                                   G_SIGNAL_RUN_LAST,
                                                   G_STRUCT_OFFSET(XfceWorkspaceClass,
                                                                   changed),
                                                   NULL, NULL,
                                                   g_cclosure_marshal_VOID__POINTER,
                                                   G_TYPE_NONE, 1,
                                                   G_TYPE_POINTER);
}

static void
xfce_workspace_init(XfceWorkspace *workspace)
{
    workspace->priv = G_TYPE_INSTANCE_GET_PRIVATE(workspace, XFCE_TYPE_WORKSPACE,
                                                XfceWorkspacePriv);
}

static void
xfce_workspace_finalize(GObject *object)
{
    XfceWorkspace *workspace = XFCE_WORKSPACE(object);

    xfce_workspace_remove_backdrops(workspace);

    g_object_unref(G_OBJECT(workspace->priv->channel));
    g_free(workspace->priv->property_prefix);
    g_free(workspace->priv->backdrops);
    g_free(workspace->priv->first_color_id);
    g_free(workspace->priv->second_color_id);
}

static void
xfce_workspace_set_property(GObject *object,
                          guint property_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
    switch(property_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
xfce_workspace_get_property(GObject *object,
                          guint property_id,
                          GValue *value,
                          GParamSpec *pspec)
{
    switch(property_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
xfce_workspace_connect_backdrop_settings(XfceWorkspace *workspace,
                                         XfceBackdrop *backdrop,
                                         guint monitor)
{
    XfconfChannel *channel = workspace->priv->channel;
    char buf[1024];
    gint pp_len;
    gchar *monitor_name = NULL;

    TRACE("entering");

    monitor_name = gdk_screen_get_monitor_plug_name(workspace->priv->gscreen, monitor);

    if(monitor_name == NULL) {
        g_snprintf(buf, sizeof(buf), "%smonitor%d/workspace%d/",
                   workspace->priv->property_prefix, monitor, workspace->priv->workspace_num);
    } else {
        g_snprintf(buf, sizeof(buf), "%smonitor%s/workspace%d/",
                   workspace->priv->property_prefix, monitor_name, workspace->priv->workspace_num);
    }
    pp_len = strlen(buf);

    DBG("prefix string: %s", buf);

    g_strlcat(buf, "color-style", sizeof(buf));
    xfconf_g_property_bind(channel, buf, XFCE_TYPE_BACKDROP_COLOR_STYLE,
                           G_OBJECT(backdrop), "color-style");

    buf[pp_len] = 0;
    g_strlcat(buf, "color1", sizeof(buf));
    workspace->priv->first_color_id[monitor] = xfconf_g_property_bind_gdkcolor(channel, buf,
                                                            G_OBJECT(backdrop), "first-color");

    buf[pp_len] = 0;
    g_strlcat(buf, "color2", sizeof(buf));
    workspace->priv->second_color_id[monitor] = xfconf_g_property_bind_gdkcolor(channel, buf,
                                                            G_OBJECT(backdrop), "second-color");

    buf[pp_len] = 0;
    g_strlcat(buf, "image-style", sizeof(buf));
    xfconf_g_property_bind(channel, buf, XFCE_TYPE_BACKDROP_IMAGE_STYLE,
                           G_OBJECT(backdrop), "image-style");

    buf[pp_len] = 0;
    g_strlcat(buf, "backdrop-cycle-enable", sizeof(buf));
    xfconf_g_property_bind(channel, buf, G_TYPE_BOOLEAN,
                           G_OBJECT(backdrop), "backdrop-cycle-enable");

    buf[pp_len] = 0;
    g_strlcat(buf, "backdrop-cycle-timer", sizeof(buf));
    xfconf_g_property_bind(channel, buf, G_TYPE_UINT,
                           G_OBJECT(backdrop), "backdrop-cycle-timer");

    buf[pp_len] = 0;
    g_strlcat(buf, "backdrop-cycle-random-order", sizeof(buf));
    xfconf_g_property_bind(channel, buf, G_TYPE_BOOLEAN,
                           G_OBJECT(backdrop), "backdrop-cycle-random-order");

    buf[pp_len] = 0;
    g_strlcat(buf, "last-image", sizeof(buf));
    xfconf_g_property_bind(channel, buf, G_TYPE_STRING,
                           G_OBJECT(backdrop), "image-filename");

    g_free(monitor_name);
}

static void
xfce_workspace_disconnect_backdrop_settings(XfceWorkspace *workspace,
                                            XfceBackdrop *backdrop,
                                            guint monitor)
{
    TRACE("entering");

    g_return_if_fail(XFCE_IS_BACKDROP(backdrop));

    xfconf_g_property_unbind_all(G_OBJECT(backdrop));
}

static void
xfce_workspace_remove_backdrops(XfceWorkspace *workspace)
{
    guint i;
    guint n_monitors;

    g_return_if_fail(XFCE_IS_WORKSPACE(workspace));

    n_monitors = gdk_screen_get_n_monitors(workspace->priv->gscreen);

    for(i = 0; i < n_monitors && i < workspace->priv->nbackdrops; ++i) {
        xfce_workspace_disconnect_backdrop_settings(workspace,
                                                    workspace->priv->backdrops[i],
                                                    i);
        g_object_unref(G_OBJECT(workspace->priv->backdrops[i]));
        workspace->priv->backdrops[i] = NULL;
    }
    workspace->priv->nbackdrops = 0;
}

/* public api */

/**
 * xfce_workspace_new:
 * @gscreen: The current #GdkScreen.
 * @channel: An #XfconfChannel to use for settings.
 * @property_prefix: String prefix for per-screen properties.
 * @number: The workspace number to represent
 *
 * Creates a new #XfceWorkspace for the specified #GdkScreen.  If @gscreen is
 * %NULL, the default screen will be used.
 *
 * Return value: A new #XfceWorkspace.
 **/
XfceWorkspace *
xfce_workspace_new(GdkScreen *gscreen,
                   XfconfChannel *channel,
                   const gchar *property_prefix,
                   gint number)
{
    XfceWorkspace *workspace;

    g_return_val_if_fail(channel && property_prefix, NULL);

    workspace = g_object_new(XFCE_TYPE_WORKSPACE, NULL);

    if(!gscreen)
        gscreen = gdk_display_get_default_screen(gdk_display_get_default());

    workspace->priv->gscreen = gscreen;
    workspace->priv->workspace_num = number;
    workspace->priv->channel = g_object_ref(G_OBJECT(channel));
    workspace->priv->property_prefix = g_strdup(property_prefix);

    return workspace;
}

gint
xfce_workspace_get_workspace_num(XfceWorkspace *workspace)
{
    g_return_val_if_fail(XFCE_IS_WORKSPACE(workspace), -1);

    return workspace->priv->workspace_num;
}

void
xfce_workspace_set_workspace_num(XfceWorkspace *workspace, gint number)
{
    g_return_if_fail(XFCE_IS_WORKSPACE(workspace));

    workspace->priv->workspace_num = number;
}

XfceBackdrop *xfce_workspace_get_backdrop(XfceWorkspace *workspace,
                                          guint monitor)
{
    g_return_val_if_fail(XFCE_IS_WORKSPACE(workspace)
                         && monitor < workspace->priv->nbackdrops, NULL);
    return workspace->priv->backdrops[monitor];
}
