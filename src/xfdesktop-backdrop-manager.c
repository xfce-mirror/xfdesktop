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

#include <glib.h>
#include <gio/gio.h>
#include <libxfce4util/libxfce4util.h>
#include <stdio.h>
#include <stdlib.h>

#include "xfdesktop-common.h"
#include "xfdesktop-backdrop-cycler.h"
#include "xfdesktop-backdrop-manager.h"
#include "xfdesktop-backdrop-renderer.h"
#include "xfdesktop-marshal.h"

#define MONITOR_QUARK (monitor_quark())

struct _XfdesktopBackdropManager {
    GObject parent;

    XfconfChannel *channel;
    XfwScreen *xfw_screen;
    XfwWorkspaceManager *workspace_manager;

    GdkRectangle screen_geometry;
    GPtrArray *monitors;  // Monitor
    GHashTable *backdrops;  // property prefix string -> Backdrop

    GHashTable *in_progress_rendering;  // property prefix string -> RenderData;
};

enum {
    PROP0,
    PROP_SCREEN,
    PROP_CHANNEL,
};

enum {
    SIG_BACKDROP_CHANGED,

    N_SIGNALS,
};

typedef struct {
    XfwMonitor *xfwmonitor;
    gchar *identifier;
    gint scale;
    GdkRectangle device_geometry;
    GdkRectangle logical_geometry;

    GArray *signal_ids;

    gatomicrefcount ref_count;
} Monitor;

typedef struct {
    XfdesktopBackdropManager *manager;
    cairo_surface_t *surface;
    gint width;
    gint height;
    GFile *image_file;
    GFileMonitor *image_file_monitor;
    XfdesktopBackdropCycler *cycler;
    gboolean is_spanning;
} Backdrop;

typedef struct {
    GCancellable *cancellable;
    gulong cancellation_forward_id;
    Monitor *monitor;
    GetImageSurfaceCallback callback;
    gpointer callback_user_data;
} RenderInstanceData;

typedef struct {
    XfdesktopBackdropManager *manager;

    GCancellable *main_cancellable;
    gchar *property_prefix;
    gboolean is_spanning;
    GFile *image_file;

    GList *instances; // RenderInstanceData
} RenderData;

static GQuark monitor_quark(void) G_GNUC_CONST;
G_DEFINE_QUARK("monitor", monitor)

static void xfdesktop_backdrop_manager_constructed(GObject *obj);
static void xfdesktop_backdrop_manager_set_property(GObject *obj,
                                                    guint property_id,
                                                    const GValue *value,
                                                    GParamSpec *pspec);
static void xfdesktop_backdrop_manager_get_property(GObject *obj,
                                                    guint property_id,
                                                    GValue *value,
                                                    GParamSpec *pspec);
static void xfdesktop_backdrop_manager_finalize(GObject *obj);

static void xfwmonitor_changed(XfwMonitor *xfwmonitor,
                               GParamSpec *pspec,
                               XfdesktopBackdropManager *manager);

static void channel_property_changed(XfdesktopBackdropManager *manager,
                                     const gchar *property_name,
                                     const GValue *value);
static void screen_monitors_changed(XfwScreen *screen,
                                   XfdesktopBackdropManager *manager);
static void screen_monitor_removed(XfwScreen *screen,
                                   XfwMonitor *monitor,
                                   XfdesktopBackdropManager *manager);

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
        g_object_set_qdata(G_OBJECT(monitor->xfwmonitor), MONITOR_QUARK, NULL);
        for (gsize i = 0; i < monitor->signal_ids->len; ++i) {
            g_signal_handler_disconnect(monitor->xfwmonitor, g_array_index(monitor->signal_ids, gulong, i));
        }
        g_array_free(monitor->signal_ids, TRUE);
        g_object_unref(monitor->xfwmonitor);
        g_free(monitor->identifier);
        g_free(monitor);
    }
}

static Monitor *
monitor_new(XfdesktopBackdropManager *manager, XfwMonitor *xfwmonitor) {
    g_return_val_if_fail(XFW_IS_MONITOR(xfwmonitor), NULL);

    Monitor *monitor = g_new0(Monitor, 1);
    g_atomic_ref_count_init(&monitor->ref_count);
    monitor->xfwmonitor = g_object_ref(xfwmonitor);
    // TODO: this is what we use now, but it would be better to use _get_identifier()
    // after migrating config.
    monitor->identifier = g_strdup(xfw_monitor_get_connector(xfwmonitor));
    monitor->scale = xfw_monitor_get_scale(xfwmonitor);
    xfw_monitor_get_physical_geometry(xfwmonitor, &monitor->device_geometry);
    xfw_monitor_get_logical_geometry(xfwmonitor, &monitor->logical_geometry);
    monitor->signal_ids = g_array_sized_new(FALSE, TRUE, sizeof(gulong), 3);

    g_object_set_qdata(G_OBJECT(xfwmonitor), MONITOR_QUARK, monitor);

    gulong id;
    id = g_signal_connect(xfwmonitor, "notify::scale",
                          G_CALLBACK(xfwmonitor_changed), manager);
    g_array_append_val(monitor->signal_ids, id);
    id = g_signal_connect(xfwmonitor, "notify::physical-geometry",
                          G_CALLBACK(xfwmonitor_changed), manager);
    g_array_append_val(monitor->signal_ids, id);
    id =g_signal_connect(xfwmonitor, "notify::logical-geometry",
                         G_CALLBACK(xfwmonitor_changed), manager);
    g_array_append_val(monitor->signal_ids, id);

    g_ptr_array_add(manager->monitors, monitor);

    return monitor;
}

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
monitor_get(GPtrArray *monitors, guint index) {
    g_return_val_if_fail(monitors != NULL, NULL);
    g_return_val_if_fail(index < monitors->len, NULL);
    return g_ptr_array_index(monitors, index);
}

static Monitor *
get_or_create_monitor(XfdesktopBackdropManager *manager, XfwMonitor *xfwmonitor) {
    g_return_val_if_fail(XFDESKTOP_IS_BACKDROP_MANAGER(manager), NULL);
    g_return_val_if_fail(XFW_IS_MONITOR(xfwmonitor), NULL);

    Monitor *monitor = g_object_get_qdata(G_OBJECT(xfwmonitor), MONITOR_QUARK);
    if (monitor == NULL) {
        monitor = monitor_new(manager, xfwmonitor);
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


static void
backdrop_free(Backdrop *backdrop) {
    cairo_surface_destroy(backdrop->surface);
    if (backdrop->image_file_monitor != NULL) {
        g_file_monitor_cancel(backdrop->image_file_monitor);
        g_object_unref(backdrop->image_file_monitor);
    }
    if (backdrop->image_file != NULL) {
        g_object_unref(backdrop->image_file);
    }
    g_object_unref(backdrop->cycler);
    g_free(backdrop);
}


static void
render_instance_data_free(RenderInstanceData *ridata) {
    if (ridata->cancellation_forward_id != 0) {
        g_signal_handler_disconnect(ridata->cancellable, ridata->cancellation_forward_id);
    }
    g_object_unref(ridata->cancellable);
    monitor_unref(ridata->monitor);
    g_free(ridata);
}


static void
render_data_free(RenderData *rdata) {
    if (rdata->manager != NULL) {
        g_object_remove_weak_pointer(G_OBJECT(rdata->manager), (gpointer)&rdata->manager);
    }
    g_list_free_full(rdata->instances, (GDestroyNotify)render_instance_data_free);
    g_cancellable_cancel(rdata->main_cancellable);
    g_object_unref(rdata->main_cancellable);
    g_free(rdata->property_prefix);
    if (rdata->image_file != NULL) {
        g_object_unref(rdata->image_file);
    }
    g_free(rdata);
}


G_DEFINE_TYPE(XfdesktopBackdropManager, xfdesktop_backdrop_manager, G_TYPE_OBJECT)


static guint signals[N_SIGNALS] = { 0, };


static void
xfdesktop_backdrop_manager_class_init(XfdesktopBackdropManagerClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->constructed = xfdesktop_backdrop_manager_constructed;
    gobject_class->set_property = xfdesktop_backdrop_manager_set_property;
    gobject_class->get_property = xfdesktop_backdrop_manager_get_property;
    gobject_class->finalize = xfdesktop_backdrop_manager_finalize;

    g_object_class_install_property(gobject_class,
                                    PROP_SCREEN,
                                    g_param_spec_object("screen",
                                                        "screen",
                                                        "XfwScreen",
                                                        XFW_TYPE_SCREEN,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class,
                                    PROP_CHANNEL,
                                    g_param_spec_object("channel",
                                                        "channel",
                                                        "xfconf channel",
                                                        XFCONF_TYPE_CHANNEL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

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
    manager->monitors = g_ptr_array_new_with_free_func((GDestroyNotify)monitor_unref);
    manager->backdrops = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)backdrop_free);
    manager->in_progress_rendering = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
}

static void
xfdesktop_backdrop_manager_constructed(GObject *obj) {
    G_OBJECT_CLASS(xfdesktop_backdrop_manager_parent_class)->constructed(obj);

    XfdesktopBackdropManager *manager = XFDESKTOP_BACKDROP_MANAGER(obj);
    manager->workspace_manager = xfw_screen_get_workspace_manager(manager->xfw_screen);

    screen_monitors_changed(manager->xfw_screen, manager);
    g_signal_connect(manager->xfw_screen, "monitors-changed",
                     G_CALLBACK(screen_monitors_changed), manager);
    g_signal_connect(manager->xfw_screen, "monitor-removed",
                     G_CALLBACK(screen_monitor_removed), manager);

    g_signal_connect_swapped(manager->channel, "property-changed",
                             G_CALLBACK(channel_property_changed), manager);
}

static void
xfdesktop_backdrop_manager_set_property(GObject *obj, guint property_id, const GValue *value, GParamSpec *pspec) {
    XfdesktopBackdropManager *manager = XFDESKTOP_BACKDROP_MANAGER(obj);

    switch (property_id) {
        case PROP_SCREEN:
            manager->xfw_screen = g_value_get_object(value);
            break;

        case PROP_CHANNEL:
            manager->channel = g_value_get_object(value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, property_id, pspec);
            break;
    }
}

static void
xfdesktop_backdrop_manager_get_property(GObject *obj, guint property_id, GValue *value, GParamSpec *pspec) {
    XfdesktopBackdropManager *manager = XFDESKTOP_BACKDROP_MANAGER(obj);

    switch (property_id) {
        case PROP_SCREEN:
            g_value_set_object(value, manager->xfw_screen);
            break;

        case PROP_CHANNEL:
            g_value_set_object(value, manager->channel);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, property_id, pspec);
            break;
    }
}

static void
xfdesktop_backdrop_manager_finalize(GObject *obj) {
    XfdesktopBackdropManager *manager = XFDESKTOP_BACKDROP_MANAGER(obj);

    g_signal_handlers_disconnect_by_data(manager->xfw_screen, manager);
    g_signal_handlers_disconnect_by_data(manager->channel, manager);

    g_hash_table_destroy(manager->in_progress_rendering);

    g_ptr_array_free(manager->monitors, TRUE);
    g_hash_table_destroy(manager->backdrops);

    G_OBJECT_CLASS(xfdesktop_backdrop_manager_parent_class)->finalize(obj);
}

static gchar *
build_property_prefix_prefix(XfdesktopBackdropManager *manager, Monitor *monitor) {
    GdkScreen *gscreen = gdk_screen_get_default();
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gint screen_num = gdk_screen_get_number(gscreen);
G_GNUC_END_IGNORE_DEPRECATIONS
    return g_strdup_printf("/backdrop/screen%d/monitor%s/",
                           screen_num,
                           monitor_get_identifier(monitor));
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

    GList *xfwmonitors = xfw_screen_get_monitors(manager->xfw_screen);
    XfwMonitor *first_xfwmonitor = xfwmonitors != NULL ? XFW_MONITOR(xfwmonitors->data) : NULL;
    g_return_val_if_fail(first_xfwmonitor != NULL, NULL);
    Monitor *first_monitor = get_or_create_monitor(manager, first_xfwmonitor);

    gchar *span_monitor_property_prefix = g_strdup_printf("/backdrop/screen%d/monitor%s/workspace%d",
                                                          screen_num,
                                                          monitor_get_identifier(first_monitor),
                                                          workspace_num);
    gchar *first_image_style_prop = g_strconcat(span_monitor_property_prefix, "/image-style", NULL);
    XfceBackdropImageStyle first_image_style = xfconf_channel_get_int(manager->channel,
                                                                      first_image_style_prop,
                                                                      XFCE_BACKDROP_IMAGE_NONE);
    g_free(first_image_style_prop);

    Monitor *the_monitor = get_or_create_monitor(manager, xfwmonitor);
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

static void
emit_backdrop_changed(XfdesktopBackdropManager *manager, const gchar *property_prefix, Backdrop *backdrop) {
    Monitor *monitor = NULL;
    XfwWorkspace *workspace = NULL;
    if (parse_property_prefix(manager, property_prefix, NULL, &monitor, &workspace)) {
        RenderData *rdata = g_hash_table_lookup(manager->in_progress_rendering, property_prefix);
        if (rdata != NULL) {
            g_cancellable_cancel(rdata->main_cancellable);
            g_hash_table_remove(manager->in_progress_rendering, property_prefix);
        }

        if (backdrop->is_spanning) {
            for (guint i = 0; i < manager->monitors->len; ++i) {
                Monitor *a_monitor = g_ptr_array_index(manager->monitors, i);
                g_signal_emit(manager,
                              signals[SIG_BACKDROP_CHANGED],
                              0,
                              a_monitor->xfwmonitor,
                              workspace);
            }
        } else {
            g_signal_emit(manager, signals[SIG_BACKDROP_CHANGED], 0, monitor->xfwmonitor, workspace);
        }
    } else {
        g_message("Can't parse property prefix to figure out monitor and workspace");
    }
}

static void
invalidate_backdrops_for_property_prefix(XfdesktopBackdropManager *manager, const gchar *property_prefix) {
    Backdrop *backdrop = g_hash_table_lookup(manager->backdrops, property_prefix);
    if (backdrop != NULL) {
        if (backdrop->surface != NULL) {
            cairo_surface_destroy(backdrop->surface);
            backdrop->surface = NULL;
        }
        emit_backdrop_changed(manager, property_prefix, backdrop);
    }
}

static void
channel_property_changed(XfdesktopBackdropManager *manager, const gchar *property_name, const GValue *value) {
    DBG("entering(%s)", property_name);

    const gchar *last_slash = g_strrstr(property_name, "/");
    if (last_slash != NULL) {
        gsize len = (gsize)(last_slash - property_name);
        if (len > 0) {
            gchar *property_prefix = g_strndup(property_name, len);
            invalidate_backdrops_for_property_prefix(manager, property_prefix);
            g_free(property_prefix);
        }
    }
}

typedef struct {
    XfdesktopBackdropManager *manager;
    gchar *property_prefix_prefix;
} BackdropInvalidateForeachData;

static void
backdrops_ht_invalidate(gpointer key, gpointer value, gpointer data) {
    const gchar *property_prefix = key;
    BackdropInvalidateForeachData *bifd = data;

    if (g_str_has_prefix(property_prefix, bifd->property_prefix_prefix)) {
        Backdrop *backdrop = value;
        if (backdrop->surface != NULL) {
            cairo_surface_destroy(backdrop->surface);
            backdrop->surface = NULL;
        }
        emit_backdrop_changed(bifd->manager, property_prefix, backdrop);
    }
}

static void
invalidate_backdrops_for_monitor(XfdesktopBackdropManager *manager, Monitor *monitor) {
    BackdropInvalidateForeachData bifd = {
        .manager = manager,
        .property_prefix_prefix = build_property_prefix_prefix(manager, monitor),
    };
    g_hash_table_foreach(manager->backdrops, backdrops_ht_invalidate, &bifd);
    g_free(bifd.property_prefix_prefix);
}


static void
xfwmonitor_changed(XfwMonitor *xfwmonitor, GParamSpec *pspec, XfdesktopBackdropManager *manager) {
    gboolean changed = FALSE;

    for (guint i = 0; i < manager->monitors->len; ++i) {
        Monitor *monitor = g_ptr_array_index(manager->monitors, i);
        if (monitor->xfwmonitor == xfwmonitor) {
            gint scale = xfw_monitor_get_scale(xfwmonitor);
            if (scale != monitor->scale) {
                monitor->scale = scale;
                changed = TRUE;
            }

            GdkRectangle physical_geometry;
            xfw_monitor_get_physical_geometry(xfwmonitor, &physical_geometry);
            if (!gdk_rectangle_equal(&physical_geometry, &monitor->device_geometry)) {
                monitor->device_geometry = physical_geometry;
                changed = TRUE;
            }

            GdkRectangle logical_geometry;
            xfw_monitor_get_logical_geometry(xfwmonitor, &logical_geometry);
            if (!gdk_rectangle_equal(&logical_geometry, &monitor->logical_geometry)) {
                monitor->logical_geometry = logical_geometry;
                changed = TRUE;
            }

            if (changed) {
                invalidate_backdrops_for_monitor(manager, monitor);
            }

            break;
        }
    }
}

static gboolean
backdrops_ht_monitor_removed(gpointer key, gpointer value, gpointer data) {
    const gchar *property_prefix = key;
    const gchar *property_prefix_prefix = data;
    return g_str_has_prefix(property_prefix, property_prefix_prefix);
}

static void
screen_monitors_changed(XfwScreen *screen, XfdesktopBackdropManager *manager) {
    GList *monitors = xfw_screen_get_monitors(screen);
    if (monitors == NULL) {
        manager->screen_geometry.x = 0;
        manager->screen_geometry.y = 0;
        manager->screen_geometry.width = 0;
        manager->screen_geometry.height = 0;
    } else {
        XfwMonitor *monitor = XFW_MONITOR(monitors->data);
        xfw_monitor_get_physical_geometry(monitor, &manager->screen_geometry);
        for (GList *l = monitors->next; l != NULL; l = l->next) {
            monitor = XFW_MONITOR(l->data);
            GdkRectangle geom;
            xfw_monitor_get_physical_geometry(monitor, &geom);
            gdk_rectangle_union(&geom, &manager->screen_geometry, &manager->screen_geometry);
        }
    }
}

static void
screen_monitor_removed(XfwScreen *screen, XfwMonitor *xfwmonitor, XfdesktopBackdropManager *manager) {
    for (guint i = 0; i < manager->monitors->len; ++i) {
        Monitor *monitor = g_ptr_array_index(manager->monitors, i);
        if (monitor->xfwmonitor == xfwmonitor) {
            gchar *property_prefix_prefix = build_property_prefix_prefix(manager, monitor);
            g_hash_table_foreach_remove(manager->backdrops, backdrops_ht_monitor_removed, property_prefix_prefix);
            g_free(property_prefix_prefix);

            g_ptr_array_remove_index(manager->monitors, i);
            break;
        }
    }
}

static void
notify_complete(cairo_surface_t *surface,
                Monitor *monitor,
                gboolean is_spanning,
                GFile *image_file,
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

    callback(surface, &region, image_file, NULL, callback_user_data);
}

static void
backdrop_image_file_changed(GFileMonitor *fmonitor,
                            GFile *file,
                            GFile *other_file,
                            GFileMonitorEvent event,
                            Backdrop *backdrop)
{
    switch (event) {
        case G_FILE_MONITOR_EVENT_DELETED: {
            DBG("backdrop file '%s' deleted", g_file_peek_path(file));

            g_file_monitor_cancel(backdrop->image_file_monitor);
            g_clear_object(&backdrop->image_file_monitor);
            g_clear_object(&backdrop->image_file);

            // FIXME: this is super inefficient
            GHashTableIter iter;
            g_hash_table_iter_init(&iter, backdrop->manager->backdrops);

            const gchar *property_prefix;
            Backdrop *a_backdrop;
            while (g_hash_table_iter_next(&iter, (gpointer)&property_prefix, (gpointer)&a_backdrop)) {
                if (a_backdrop == backdrop) {
                    invalidate_backdrops_for_property_prefix(backdrop->manager, property_prefix);
                }
            }
            break;
        }

        case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT: {
            DBG("backdrop file '%s' changed on disk", g_file_peek_path(file));

            // FIXME: this is super inefficient
            GHashTableIter iter;
            g_hash_table_iter_init(&iter, backdrop->manager->backdrops);

            const gchar *property_prefix;
            Backdrop *a_backdrop;
            while (g_hash_table_iter_next(&iter, (gpointer)&property_prefix, (gpointer)&a_backdrop)) {
                if (a_backdrop == backdrop) {
                    invalidate_backdrops_for_property_prefix(backdrop->manager, property_prefix);
                }
            }
            break;
        }

        default:
            break;
    }
}

static void
render_finished(cairo_surface_t *surface, gint width, gint height, GError *error, gpointer user_data) {
    RenderData *rdata = user_data;
    const gchar *property_prefix = rdata->property_prefix;

    if (error != NULL && !g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
        g_message("Failed to load image file '%s': %s", g_file_peek_path(rdata->image_file), error->message);
    }

    if (surface != NULL) {
        if (rdata->manager != NULL) {
            Backdrop *backdrop = g_hash_table_lookup(rdata->manager->backdrops, rdata->property_prefix);
            if (backdrop == NULL) {
                backdrop = g_new0(Backdrop, 1);
                backdrop->manager = rdata->manager;
                backdrop->cycler = xfdesktop_backdrop_cycler_new(rdata->manager->channel, rdata->property_prefix,
                                                                 rdata->image_file);
                g_hash_table_insert(rdata->manager->backdrops, rdata->property_prefix, backdrop);
                rdata->property_prefix = NULL;
            } else {
                g_clear_pointer(&backdrop->surface, cairo_surface_destroy);
                g_clear_object(&backdrop->image_file);
            }

            // XXX: maybe we shouldn't cache if error is non-null
            backdrop->surface = surface;
            backdrop->width = width;
            backdrop->height = height;
            if (rdata->image_file != NULL) {
                backdrop->image_file = g_object_ref(rdata->image_file);
            }
            backdrop->is_spanning = rdata->is_spanning;

            if (backdrop->image_file_monitor != NULL) {
                g_file_monitor_cancel(backdrop->image_file_monitor);
                g_clear_object(&backdrop->image_file_monitor);
            }
            if (backdrop->image_file != NULL) {
                backdrop->image_file_monitor = g_file_monitor_file(backdrop->image_file,
                                                                   G_FILE_MONITOR_NONE,
                                                                   NULL,
                                                                   NULL);
                if (backdrop->image_file_monitor == NULL) {
                    g_signal_connect(backdrop->image_file_monitor, "changed",
                                     G_CALLBACK(backdrop_image_file_changed), backdrop);
                }
            }
        }

        for (GList *l = rdata->instances; l != NULL; l = l->next) {
            RenderInstanceData *ridata = l->data;
            notify_complete(surface,
                            ridata->monitor,
                            rdata->is_spanning,
                            error == NULL ? rdata->image_file : NULL,
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
    GFile *image_file = image_filename != NULL ? g_file_new_for_path(image_filename) : NULL;
    g_free(image_filename);

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
    rdata->image_file = image_file;

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
                              image_file,
                              geom->width,
                              geom->height,
                              render_finished,
                              rdata);
}

XfdesktopBackdropManager *
xfdesktop_backdrop_manager_new(XfwScreen *screen, XfconfChannel *channel) {
    return g_object_new(XFDESKTOP_TYPE_BACKDROP_MANAGER,
                        "screen", screen,
                        "channel", channel,
                        NULL);
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
                        backdrop->image_file,
                        callback,
                        callback_user_data);
    } else {
        RenderData *rdata = g_hash_table_lookup(manager->in_progress_rendering, property_prefix);
        if (rdata != NULL && g_cancellable_is_cancelled(rdata->main_cancellable)) {
            g_hash_table_remove(manager->in_progress_rendering, property_prefix);
            rdata = NULL;  // Will get freed when callback is called.
        }

        if (rdata != NULL) {
            g_free(property_prefix);
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
