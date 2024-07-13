/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2004-2007,2022-2024 Brian Tarricone, <brian@tarricone.org>
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
 *  Random portions taken from or inspired by the original xfdesktop for xfce4:
 *     Copyright (C) 2002-2003 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *     Copyright (C) 2003 Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include <libxfce4util/libxfce4util.h>

#include "xfdesktop-common.h"
#include "xfdesktop-backdrop-cycler.h"
#include "xfdesktop-backdrop-manager.h"
#include "xfdesktop-backdrop-renderer.h"
#include "xfdesktop-marshal.h"

#define MONITOR_QUARK (monitor_quark())

typedef struct {
    XfwMonitor *xfwmonitor;
    gchar *identifier;
    gint scale;
    GdkRectangle device_geometry;

    gatomicrefcount ref_count;
} Monitor;

static GQuark monitor_quark(void) G_GNUC_CONST;
G_DEFINE_QUARK("monitor", monitor)

static const gchar *
monitor_get_identifier(Monitor *monitor) {
    g_return_val_if_fail(monitor != NULL, NULL);
    return monitor->identifier;
}

static GdkRectangle *
monitor_get_device_geometry(Monitor *monitor) {
    g_return_val_if_fail(monitor != NULL, NULL);
    return &monitor->device_geometry;
}

static Monitor *
monitor_ref(Monitor *monitor) {
    g_return_val_if_fail(monitor != NULL, NULL);
    g_atomic_ref_count_inc(&monitor->ref_count);
    return monitor;
}

static void
monitor_unref(Monitor *monitor) {
    g_return_if_fail(monitor != NULL);
    if (g_atomic_ref_count_dec(&monitor->ref_count)) {
        g_object_unref(monitor->xfwmonitor);
        g_free(monitor->identifier);
        g_free(monitor);
    }
}

static Monitor *
monitor_get(GPtrArray *monitors, guint index) {
    g_return_val_if_fail(monitors != NULL, NULL);
    g_return_val_if_fail(index < monitors->len, NULL);
    return g_ptr_array_index(monitors, index);
}

static Monitor *
monitor_get_from_xfw(GPtrArray *monitors, XfwMonitor *xfwmonitor) {
    g_return_val_if_fail(monitors != NULL, NULL);
    g_return_val_if_fail(XFW_IS_MONITOR(xfwmonitor), NULL);

    Monitor *monitor = g_object_get_qdata(G_OBJECT(xfwmonitor), MONITOR_QUARK);
    if (monitor == NULL) {
        g_warning("Can't find match for monitor '%s'; this is probably a bug in " PACKAGE,
                  xfw_monitor_get_description(xfwmonitor));
    }

    return monitor;
}

static Monitor *
monitor_get_from_identifier(GPtrArray *monitors, const gchar *identifier) {
    g_return_val_if_fail(monitors != NULL, NULL);
    g_return_val_if_fail(identifier != NULL, NULL);

    for (guint i = 0; i < monitors->len; ++i) {
        Monitor *monitor = monitor_get(monitors, i);
        if (g_strcmp0(identifier, monitor->identifier) == 0) {
            return monitor;
        }
    }

    return NULL;
}


typedef struct {
    cairo_surface_t *surface;
    gint width;
    gint height;
    gchar *image_filename;
    XfdesktopBackdropCycler *cycler;
    gboolean is_spanning;
} Backdrop;

static void
backdrop_free(Backdrop *backdrop) {
    cairo_surface_destroy(backdrop->surface);
    g_free(backdrop->image_filename);
    g_object_unref(backdrop->cycler);
    g_free(backdrop);
}


typedef struct {
    GCancellable *cancellable;
    gulong cancellation_forward_id;
    Monitor *monitor;
    GetImageSurfaceCallback callback;
    gpointer callback_user_data;
} RenderInstanceData;

static void
render_instance_data_free(RenderInstanceData *ridata) {
    if (ridata->cancellation_forward_id != 0) {
        g_signal_handler_disconnect(ridata->cancellable, ridata->cancellation_forward_id);
    }
    g_object_unref(ridata->cancellable);
    monitor_unref(ridata->monitor);
    g_free(ridata);
}


typedef struct {
    XfdesktopBackdropManager *manager;

    GCancellable *main_cancellable;
    gchar *property_prefix;
    gboolean is_spanning;
    gchar *image_filename;

    GList *instances; // RenderInstanceData
} RenderData;

static void
render_data_free(RenderData *rdata) {
    if (rdata->manager != NULL) {
        g_object_remove_weak_pointer(G_OBJECT(rdata->manager), (gpointer)&rdata->manager);
    }
    g_list_free_full(rdata->instances, (GDestroyNotify)render_instance_data_free);
    g_cancellable_cancel(rdata->main_cancellable);
    g_object_unref(rdata->main_cancellable);
    g_free(rdata->property_prefix);
    g_free(rdata->image_filename);
    g_free(rdata);
}


struct _XfdesktopBackdropManager {
    GObject parent;

    XfconfChannel *channel;
    XfwScreen *xfw_screen;
    XfwWorkspaceManager *workspace_manager;

    GdkRectangle screen_geometry;
    GPtrArray *monitors;  // Monitor
    GHashTable *backdrops;  // property name string -> Backdrop

    GHashTable *in_progress_rendering;  // property name string -> RenderData;
};

enum {
    SIG_BACKDROP_CHANGED,

    N_SIGNALS,
};

static void xfdesktop_backdrop_manager_constructed(GObject *obj);
static void xfdesktop_backdrop_manager_finalize(GObject *obj);

static void channel_property_changed(XfdesktopBackdropManager *manager,
                                     const gchar *property_name,
                                     const GValue *value);

G_DEFINE_TYPE(XfdesktopBackdropManager, xfdesktop_backdrop_manager, G_TYPE_OBJECT)


static guint signals[N_SIGNALS] = { 0, };


static void
xfdesktop_backdrop_manager_class_init(XfdesktopBackdropManagerClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->constructed = xfdesktop_backdrop_manager_constructed;
    gobject_class->finalize = xfdesktop_backdrop_manager_finalize;

    signals[SIG_BACKDROP_CHANGED] = g_signal_new("backdrop-changed",
                                                 XFDESKTOP_TYPE_BACKDROP_MANAGER,
                                                 G_SIGNAL_RUN_LAST,
                                                 0,
                                                 NULL,
                                                 NULL,
                                                 xfdesktop_marshal_VOID__OBJECT_OBJECT,
                                                 G_TYPE_NONE, 2,
                                                 XFW_TYPE_MONITOR,
                                                 XFW_TYPE_WORKSPACE);
}

static void
xfdesktop_backdrop_manager_init(XfdesktopBackdropManager *manager) {
    manager->channel = xfconf_channel_get("xfce4-desktop");
    manager->backdrops = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)backdrop_free);
    manager->xfw_screen = xfw_screen_get_default();
    manager->workspace_manager = xfw_screen_get_workspace_manager(manager->xfw_screen);
    manager->in_progress_rendering = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    g_signal_connect_swapped(manager->channel, "property-changed",
                             G_CALLBACK(channel_property_changed), manager);
}

static void
xfdesktop_backdrop_manager_constructed(GObject *obj) {
    G_OBJECT_CLASS(xfdesktop_backdrop_manager_parent_class)->constructed(obj);
    xfdesktop_backdrop_manager_monitors_changed(XFDESKTOP_BACKDROP_MANAGER(obj));
}

static void
xfdesktop_backdrop_manager_finalize(GObject *obj) {
    XfdesktopBackdropManager *manager = XFDESKTOP_BACKDROP_MANAGER(obj);

    g_signal_handlers_disconnect_by_data(manager->channel, manager);

    g_ptr_array_free(manager->monitors, TRUE);
    g_hash_table_destroy(manager->backdrops);

    g_hash_table_destroy(manager->in_progress_rendering);

    g_object_unref(manager->xfw_screen);

    G_OBJECT_CLASS(xfdesktop_backdrop_manager_parent_class)->finalize(obj);
}

static gchar *
build_property_prefix(XfdesktopBackdropManager *manager,
                      XfwMonitor *xfwmonitor,
                      XfwWorkspace *workspace,
                      Monitor **monitor,
                      gboolean *is_spanning)
{
    g_return_val_if_fail(XFW_IS_MONITOR(xfwmonitor), NULL);
    g_return_val_if_fail(XFW_IS_WORKSPACE(workspace), NULL);
    g_return_val_if_fail(monitor == NULL || *monitor == NULL, NULL);

    DBG("entering(mon=%s, ws=%s)", xfw_monitor_get_connector(xfwmonitor), xfw_workspace_get_name(workspace));

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gint screen_num = gdk_screen_get_number(gdk_screen_get_default());
G_GNUC_END_IGNORE_DEPRECATIONS

    gint workspace_num;
    if (xfconf_channel_get_bool(manager->channel, SINGLE_WORKSPACE_MODE, FALSE)) {
        workspace_num = xfconf_channel_get_int(manager->channel, SINGLE_WORKSPACE_NUMBER, 0);
    } else {
        workspace_num = xfw_workspace_get_number(workspace);
    }

    Monitor *the_monitor = monitor_get_from_xfw(manager->monitors, xfwmonitor);
    g_return_val_if_fail(the_monitor != NULL, NULL);

    Monitor *first_monitor = monitor_get(manager->monitors, 0);
    g_return_val_if_fail(first_monitor != NULL, NULL);

    gchar *span_monitor_property_prefix = g_strdup_printf("/backdrop/screen%d/monitor%s/workspace%d",
                                                          screen_num,
                                                          monitor_get_identifier(first_monitor),
                                                          workspace_num);
    gchar *first_image_style_prop = g_strconcat(span_monitor_property_prefix, "/image-style", NULL);
    XfceBackdropImageStyle first_image_style = xfconf_channel_get_int(manager->channel,
                                                                      first_image_style_prop,
                                                                      XFCE_BACKDROP_IMAGE_NONE);
    g_free(first_image_style_prop);

    if (monitor != NULL) {
        *monitor = the_monitor;
    }

    if (first_image_style == XFCE_BACKDROP_IMAGE_SPANNING_SCREENS) {
        if (is_spanning != NULL) {
            *is_spanning = TRUE;
        }
        return span_monitor_property_prefix;
    } else {
        g_free(span_monitor_property_prefix);
        if (is_spanning != NULL) {
            *is_spanning = FALSE;
        }
        return g_strdup_printf("/backdrop/screen%d/monitor%s/workspace%d",
                               screen_num,
                               monitor_get_identifier(the_monitor),
                               workspace_num);
    }
}

static GdkRGBA
fetch_color(XfconfChannel *channel, const gchar *property_name) {
    GdkRGBA color;

    if (!xfconf_channel_get_array(channel,
                                  property_name,
                                  G_TYPE_DOUBLE, &color.red,
                                  G_TYPE_DOUBLE, &color.green,
                                  G_TYPE_DOUBLE, &color.blue,
                                  G_TYPE_DOUBLE, &color.alpha,
                                  G_TYPE_INVALID))
    {
        color.red = color.green = color.blue = color.alpha = 1.0;
    }

    return color;
}

static XfwWorkspace *
find_workspace_by_number(XfwWorkspaceManager *manager, gint number) {
    g_return_val_if_fail(XFW_IS_WORKSPACE_MANAGER(manager), NULL);
    g_return_val_if_fail(number >= 0, NULL);

    for (GList *l = xfw_workspace_manager_list_workspaces(manager);
         l != NULL;
         l = l->next)
    {
        XfwWorkspace *workspace = XFW_WORKSPACE(l->data);
        if (number == (gint)xfw_workspace_get_number(workspace)) {
            return workspace;
        }
    }

    return NULL;
}

static gboolean
parse_property_prefix(XfdesktopBackdropManager *manager,
                      const gchar *property_prefix,
                      gint *out_screen_num,
                      Monitor **out_monitor,
                      XfwWorkspace **out_workspace)
{
    gchar *name = property_prefix[0] == '/' ? (gchar *)property_prefix + 1 : (gchar *)property_prefix;
    gchar **parts = g_strsplit(name, "/", -1);

    if (parts != NULL &&
        g_strcmp0(parts[0], "backdrop") == 0 &&
        parts[1] != NULL && g_str_has_prefix(parts[1], "screen") &&
        parts[2] != NULL && g_str_has_prefix(parts[2], "monitor") &&
        parts[3] != NULL && g_str_has_prefix(parts[3], "workspace"))
    {
        gint screen_num = strtol(parts[1] + 6, NULL, 10);
        const gchar *monitor_identifier = parts[2] + 7;
        gint workspace_num = strtol(parts[3] + 9, NULL, 10);
        DBG("got screen_num %d, monitor %s, workspace %d", screen_num, monitor_identifier, workspace_num);

        Monitor *monitor = monitor_get_from_identifier(manager->monitors, monitor_identifier);
        XfwWorkspace *workspace = find_workspace_by_number(manager->workspace_manager, workspace_num);
        DBG("monitor=%p, workspace=%p", monitor, workspace);

        if (monitor != NULL && workspace != NULL) {
            if (out_screen_num != NULL) {
                *out_screen_num = screen_num;
            }

            if (out_monitor != NULL) {
                *out_monitor = monitor;
            }

            if (out_workspace != NULL) {
                *out_workspace = workspace;
            }

            g_strfreev(parts);
            return TRUE;
        }
    }

    g_strfreev(parts);

    return FALSE;
}

static Monitor *
steal_existing_monitor(GPtrArray *monitors, XfwMonitor *xfwmonitor) {
    if (monitors != NULL) {
        for (guint i = 0; i < monitors->len; ++i) {
            Monitor *monitor = g_ptr_array_index(monitors, i);
            // XXX: is this a good metric for equality, or should we look at the
            // connector, position, etc.?
            if (monitor->xfwmonitor == xfwmonitor) {
                g_ptr_array_steal_index(monitors, i);
                return monitor;
            }
        }
    }

    return NULL;
}

// This can be called with xfwmonitor and workspace null, in which case it will
// try to figure it out on its own.
static void
xfdesktop_backdrop_manager_invalidate_internal(XfdesktopBackdropManager *manager,
                                               const gchar *property_prefix,
                                               XfwMonitor *xfwmonitor,
                                               XfwWorkspace *workspace)
{
    Backdrop *backdrop = g_hash_table_lookup(manager->backdrops, property_prefix);
    if (backdrop != NULL) {
        if (backdrop->surface != NULL) {
            cairo_surface_destroy(backdrop->surface);
            backdrop->surface = NULL;

            gint screen_num = 0;
            Monitor *monitor = NULL;

            if (xfwmonitor == NULL || workspace == NULL) {
                if (parse_property_prefix(manager, property_prefix, &screen_num, &monitor, &workspace)) {
                    xfwmonitor = monitor->xfwmonitor;
                } else {
                    g_message("Can't parse property prefix to figure out monitor and workspace");
                }
            }

            if (xfwmonitor != NULL && workspace != NULL) {
                GPtrArray *xfwmonitors_to_signal = NULL;

                if (backdrop->is_spanning) {
                    GList *xfw_monitors = xfw_screen_get_monitors(manager->xfw_screen);
                    xfwmonitors_to_signal = g_ptr_array_sized_new(g_list_length(xfw_monitors));
                    for (GList *l = xfw_monitors; l != NULL; l = l->next) {
                        g_ptr_array_add(xfwmonitors_to_signal, XFW_MONITOR(l->data));
                    }
                } else {
                    xfwmonitors_to_signal = g_ptr_array_sized_new(1);
                    g_ptr_array_add(xfwmonitors_to_signal, xfwmonitor);
                }

                for (guint i = 0; i < xfwmonitors_to_signal->len; ++i) {
                    g_signal_emit(manager,
                                  signals[SIG_BACKDROP_CHANGED],
                                  0,
                                  g_ptr_array_index(xfwmonitors_to_signal, i),
                                  workspace);
                }

                g_ptr_array_free(xfwmonitors_to_signal, TRUE);
            }
        }
    }
}


static void
channel_property_changed(XfdesktopBackdropManager *manager, const gchar *property_name, const GValue *value) {
    DBG("entering");

    const gchar *last_slash = g_strrstr(property_name, "/");
    if (last_slash != NULL) {
        gsize len = (gsize)(last_slash - property_name);
        if (len > 0) {
            gchar *property_prefix = g_strndup(property_name, len);
            xfdesktop_backdrop_manager_invalidate_internal(manager, property_prefix, NULL, NULL);
            g_free(property_prefix);
        }
    }
}

static void
notify_complete(cairo_surface_t *surface,
                Monitor *monitor,
                gboolean is_spanning,
                const gchar *image_filename,
                GetImageSurfaceCallback callback,
                gpointer callback_user_data)
{
    g_return_if_fail(surface != NULL);
    g_return_if_fail(monitor != NULL);
    g_return_if_fail(callback != NULL);

    GdkRectangle region = *monitor_get_device_geometry(monitor);
    if (!is_spanning) {
        region.x = region.y = 0;
    }

    callback(surface, &region, image_filename, NULL, callback_user_data);
}

static void
render_finished(cairo_surface_t *surface, gint width, gint height, GError *error, gpointer user_data) {
    RenderData *rdata = user_data;
    const gchar *property_prefix = rdata->property_prefix;

    if (error != NULL && !g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
        g_message("Failed to load image file '%s': %s", rdata->image_filename, error->message);
    }

    if (surface != NULL) {
        if (rdata->manager != NULL) {
            Backdrop *backdrop = g_hash_table_lookup(rdata->manager->backdrops, rdata->property_prefix);
            if (backdrop == NULL) {
                backdrop = g_new0(Backdrop, 1);
                backdrop->cycler = xfdesktop_backdrop_cycler_new(rdata->manager->channel, rdata->property_prefix);
                g_hash_table_insert(rdata->manager->backdrops, rdata->property_prefix, backdrop);
                rdata->property_prefix = NULL;
            } else {
                cairo_surface_destroy(backdrop->surface);
                g_free(backdrop->image_filename);
            }

            // XXX: maybe we shouldn't cache if error is non-null
            backdrop->surface = surface;
            backdrop->width = width;
            backdrop->height = height;
            backdrop->image_filename = rdata->image_filename;
            backdrop->is_spanning = rdata->is_spanning;

            rdata->image_filename = NULL;
        }

        for (GList *l = rdata->instances; l != NULL; l = l->next) {
            RenderInstanceData *ridata = l->data;
            notify_complete(surface,
                            ridata->monitor,
                            rdata->is_spanning,
                            error == NULL ? rdata->image_filename : NULL,
                            ridata->callback,
                            ridata->callback_user_data);
        }
    } else {
        for (GList *l = rdata->instances; l != NULL; l = l->next) {
            RenderInstanceData *ridata = l->data;
            ridata->callback(NULL, NULL, NULL, error, ridata->callback_user_data);
        }
    }

    if (rdata->manager != NULL) {
        g_hash_table_remove(rdata->manager->in_progress_rendering, property_prefix);
    }
    render_data_free(rdata);
}

static void
forward_cancellation(GCancellable *cancellable, GCancellable *main_cancellable) {
    g_cancellable_cancel(main_cancellable);
}

static void
create_backdrop(XfdesktopBackdropManager *manager,
                GCancellable *cancellable,
                gchar *property_prefix,
                Monitor *monitor,
                gboolean is_spanning,
                GetImageSurfaceCallback callback,
                gpointer callback_user_data)
{
    XfconfChannel *channel = manager->channel;
    gchar *prop_name;

    DBG("Creating backdrop from setting prefix %s", property_prefix);

    prop_name = g_strconcat(property_prefix, "/color-style", NULL);
    XfceBackdropColorStyle color_style = xfconf_channel_get_int(channel, prop_name, XFCE_BACKDROP_COLOR_TRANSPARENT);
    g_free(prop_name);

    prop_name = g_strconcat(property_prefix, "/rgba1", NULL);
    GdkRGBA color1 = fetch_color(channel, prop_name);
    g_free(prop_name);

    prop_name = g_strconcat(property_prefix, "/rgba2", NULL);
    GdkRGBA color2 = fetch_color(channel, prop_name);
    g_free(prop_name);

    prop_name = g_strconcat(property_prefix, "/image-style", NULL);
    XfceBackdropImageStyle image_style = xfconf_channel_get_int(channel, prop_name, XFCE_BACKDROP_IMAGE_ZOOMED);
    g_free(prop_name);

    prop_name = g_strconcat(property_prefix, "/last-image", NULL);
    gchar *image_filename = xfconf_channel_get_string(channel, prop_name, NULL);
    g_free(prop_name);

    GdkRectangle *geom;
    if (is_spanning) {
        geom = &manager->screen_geometry;
    } else {
        geom = monitor_get_device_geometry(monitor);
    }

    RenderData *rdata = g_new0(RenderData, 1);
    rdata->manager = manager;
    g_object_add_weak_pointer(G_OBJECT(manager), (gpointer)&rdata->manager);
    rdata->main_cancellable = g_cancellable_new();
    rdata->property_prefix = property_prefix;
    rdata->is_spanning = is_spanning;
    rdata->image_filename = image_filename;

    RenderInstanceData *ridata = g_new0(RenderInstanceData, 1);
    ridata->cancellable = g_object_ref(cancellable);
    ridata->cancellation_forward_id = g_cancellable_connect(cancellable,
                                                            G_CALLBACK(forward_cancellation),
                                                            rdata->main_cancellable,
                                                            NULL);
    ridata->monitor = monitor_ref(monitor);
    ridata->callback = callback;
    ridata->callback_user_data = callback_user_data;
    rdata->instances = g_list_append(rdata->instances, ridata);

    g_hash_table_insert(manager->in_progress_rendering, g_strdup(property_prefix), rdata);

    xfdesktop_backdrop_render(rdata->main_cancellable,
                              color_style,
                              &color1,
                              &color2,
                              image_style,
                              image_filename,
                              geom->width,
                              geom->height,
                              render_finished,
                              rdata);
}

XfdesktopBackdropManager *
xfdesktop_backdrop_manager_get(void) {
    static XfdesktopBackdropManager *singleton = NULL;

    if (singleton == NULL) {
        singleton = g_object_new(XFDESKTOP_TYPE_BACKDROP_MANAGER, NULL);
        g_object_add_weak_pointer(G_OBJECT(singleton), (gpointer)&singleton);
    } else {
        g_object_ref(singleton);
    }

    return singleton;
}

void
xfdesktop_backdrop_manager_get_image_surface(XfdesktopBackdropManager *manager,
                                             GCancellable *cancellable,
                                             XfwMonitor *xfwmonitor,
                                             XfwWorkspace *workspace,
                                             GetImageSurfaceCallback callback,
                                             gpointer callback_user_data)
{
    g_return_if_fail(XFDESKTOP_IS_BACKDROP_MANAGER(manager));
    g_return_if_fail(G_IS_CANCELLABLE(cancellable));
    g_return_if_fail(XFW_IS_MONITOR(xfwmonitor));
    g_return_if_fail(XFW_IS_WORKSPACE(workspace));
    g_return_if_fail(callback != NULL);

    Monitor *monitor = NULL;
    gboolean is_spanning = FALSE;

    gchar *property_prefix = build_property_prefix(manager, xfwmonitor, workspace, &monitor, &is_spanning);
    Backdrop *backdrop = g_hash_table_lookup(manager->backdrops, property_prefix);
    if (backdrop != NULL && backdrop->surface != NULL) {
        g_free(property_prefix);
        notify_complete(backdrop->surface,
                        monitor,
                        is_spanning,
                        backdrop->image_filename,
                        callback,
                        callback_user_data);
    } else {
        RenderData *rdata = g_hash_table_lookup(manager->in_progress_rendering, property_prefix);
        if (rdata != NULL) {
            g_free(property_prefix);
            RenderInstanceData *ridata = g_new0(RenderInstanceData, 1);
            ridata->cancellable = g_object_ref(cancellable);
            ridata->cancellation_forward_id = g_cancellable_connect(cancellable,
                                                                    G_CALLBACK(forward_cancellation),
                                                                    rdata->main_cancellable,
                                                                    NULL);
            ridata->monitor = monitor_get_from_xfw(manager->monitors, xfwmonitor);
            ridata->callback = callback;
            ridata->callback_user_data = callback_user_data;
            rdata->instances = g_list_append(rdata->instances, ridata);
        } else {
            create_backdrop(manager, cancellable, property_prefix, monitor, is_spanning, callback, callback_user_data);
        }
    }
}

static XfdesktopBackdropCycler *
get_backdrop_cycler(XfdesktopBackdropManager *manager, XfwMonitor *xfwmonitor, XfwWorkspace *workspace) {
    gchar *property_prefix = build_property_prefix(manager, xfwmonitor, workspace, NULL, NULL);
    Backdrop *backdrop = g_hash_table_lookup(manager->backdrops, property_prefix);
    g_free(property_prefix);

    return backdrop != NULL ? backdrop->cycler : NULL;
}

gboolean
xfdesktop_backdrop_manager_can_cycle_backdrop(XfdesktopBackdropManager *manager,
                                              XfwMonitor *xfwmonitor,
                                              XfwWorkspace *workspace)
{
    g_return_val_if_fail(XFDESKTOP_IS_BACKDROP_MANAGER(manager), FALSE);
    g_return_val_if_fail(XFW_IS_MONITOR(xfwmonitor), FALSE);
    g_return_val_if_fail(XFW_IS_WORKSPACE(workspace), FALSE);

    XfdesktopBackdropCycler *cycler = get_backdrop_cycler(manager, xfwmonitor, workspace);
    return cycler != NULL ? xfdesktop_backdrop_cycler_is_enabled(cycler) : FALSE;
}

void
xfdesktop_backdrop_manager_cycle_backdrop(XfdesktopBackdropManager *manager,
                                          XfwMonitor *xfwmonitor,
                                          XfwWorkspace *workspace)
{
    g_return_if_fail(XFDESKTOP_IS_BACKDROP_MANAGER(manager));
    g_return_if_fail(XFW_IS_MONITOR(xfwmonitor));
    g_return_if_fail(XFW_IS_WORKSPACE(workspace));

    XfdesktopBackdropCycler *cycler = get_backdrop_cycler(manager, xfwmonitor, workspace);
    if (cycler != NULL) {
        xfdesktop_backdrop_cycler_cycle_backdrop(cycler);
    }
}

void
xfdesktop_backdrop_manager_invalidate(XfdesktopBackdropManager *manager,
                                      XfwMonitor *xfwmonitor,
                                      XfwWorkspace *workspace)
{
    g_return_if_fail(XFDESKTOP_IS_BACKDROP_MANAGER(manager));
    g_return_if_fail(XFW_IS_MONITOR(xfwmonitor));
    g_return_if_fail(XFW_IS_WORKSPACE(workspace));

    gchar *property_prefix = build_property_prefix(manager, xfwmonitor, workspace, NULL, NULL);
    xfdesktop_backdrop_manager_invalidate_internal(manager, property_prefix, xfwmonitor, workspace);
    g_free(property_prefix);
}

void
xfdesktop_backdrop_manager_monitors_changed(XfdesktopBackdropManager *manager) {
    g_return_if_fail(XFDESKTOP_IS_BACKDROP_MANAGER(manager));

    GList *xfw_monitors = xfw_screen_get_monitors(manager->xfw_screen);
    gint nmonitors = g_list_length(xfw_monitors);
    if (nmonitors > 0) {
        GdkRectangle screen_geometry = { 0, };
        GPtrArray *monitors = g_ptr_array_new_full(nmonitors, (GDestroyNotify)monitor_unref);

        for (GList *l = xfw_monitors; l != NULL; l = l->next) {
            XfwMonitor *xfwmonitor = XFW_MONITOR(l->data);

            Monitor *monitor = steal_existing_monitor(manager->monitors, xfwmonitor);

            if (monitor == NULL) {
                monitor = g_new0(Monitor, 1);
                g_atomic_ref_count_init(&monitor->ref_count);
                monitor->xfwmonitor = g_object_ref(xfwmonitor);

                g_object_set_qdata_full(G_OBJECT(xfwmonitor),
                                        MONITOR_QUARK,
                                        monitor_ref(monitor),
                                        (GDestroyNotify)monitor_unref);
            } else {
                g_free(monitor->identifier);
            }

            // TODO: this is what we use now, but it would be better to use _get_identifier()
            // after migrating config.
            monitor->identifier = g_strdup(xfw_monitor_get_connector(xfwmonitor));
            monitor->scale = xfw_monitor_get_scale(xfwmonitor);
            xfw_monitor_get_physical_geometry(xfwmonitor, &monitor->device_geometry);

            gdk_rectangle_union(&screen_geometry, &monitor->device_geometry, &screen_geometry);

            g_ptr_array_add(monitors, monitor);
        }

        GPtrArray *old_monitors = manager->monitors;
        manager->monitors = monitors;
        manager->screen_geometry = screen_geometry;

        // Remove any Backdrop instances that belonged to monitors that don't exist anymore.
        if (old_monitors != NULL) {
            for (guint i = 0; i < old_monitors->len; ++i) {
                Monitor *old_monitor = g_ptr_array_index(old_monitors, i);
                GdkScreen *screen = gdk_screen_get_default();
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                gint screen_num = gdk_screen_get_number(screen);
G_GNUC_END_IGNORE_DEPRECATIONS
                gchar *property_prefix = g_strdup_printf("/backdrop/screen%d/monitor%s/",
                                                         screen_num,
                                                         old_monitor->identifier);

                GHashTableIter iter;
                g_hash_table_iter_init(&iter, manager->backdrops);
                const gchar *key;
                while (g_hash_table_iter_next(&iter, (gpointer)&key, NULL)) {
                    if (g_str_has_prefix(key, property_prefix)) {
                        g_hash_table_iter_remove(&iter);
                    }
                }

                g_free(property_prefix);
            }

            g_ptr_array_free(old_monitors, TRUE);
        }

        // Invalidate all backdrops that remain.
        GHashTableIter iter;
        g_hash_table_iter_init(&iter, manager->backdrops);
        const gchar *property_prefix;
        while (g_hash_table_iter_next(&iter, (gpointer)&property_prefix, NULL)) {
            xfdesktop_backdrop_manager_invalidate_internal(manager, property_prefix, NULL, NULL);
        }
    }
}
