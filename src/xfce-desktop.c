/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2004-2007,2024 Brian Tarricone, <brian@tarricone.org>
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

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include <gio/gio.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4windowing/libxfce4windowing.h>
#include <xfconf/xfconf.h>

#ifdef ENABLE_X11
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <gdk/gdkx.h>
#include <cairo-xlib.h>
#endif  /* ENABLE_X11 */

#ifdef ENABLE_WAYLAND
#include <gtk-layer-shell.h>
#endif

#include "xfdesktop-backdrop-manager.h"
#include "xfdesktop-common.h"
#include "xfce-desktop.h"

#ifdef ENABLE_X11
#include "xfdesktop-x11.h"
#endif

/* disable setting the x background for bug 7442 */
//#define DISABLE_FOR_BUG7442

struct _XfceDesktop {
    GtkWindow parent_instance;

    GdkScreen *gscreen;
    XfwScreen *xfw_screen;
    XfwMonitor *monitor;
    XfdesktopBackdropManager *backdrop_manager;
    XfwWorkspaceManager *workspace_manager;
    gboolean updates_frozen;

    XfconfChannel *channel;
    gchar *property_prefix;

    XfwWorkspaceGroup *workspace_group;
    GList *workspaces;  // XfwWorkspace
    XfwWorkspace *active_workspace;

    gboolean single_workspace_mode;
    gint single_workspace_num;
    XfwWorkspace *single_workspace;

    XfwWorkspace *backdrop_workspace;
    GCancellable *backdrop_load_cancellable;
    cairo_surface_t *bg_surface;
    GdkRectangle bg_surface_region;

    gboolean is_active;
    gboolean has_pointer;

#ifdef ENABLE_DESKTOP_ICONS
    gint style_refresh_timer;
#endif
};

enum
{
    PROP_0 = 0,
    PROP_SCREEN,
    PROP_MONITOR,
    PROP_CHANNEL,
    PROP_PROPERTY_PREFIX,
    PROP_BACKDROP_MANAGER,
    PROP_SINGLE_WORKSPACE_MODE,
    PROP_SINGLE_WORKSPACE_NUMBER,
    PROP_ACTIVE,
};


static void xfce_desktop_constructed(GObject *object);
static void xfce_desktop_finalize(GObject *object);
static void xfce_desktop_set_property(GObject *object,
                                      guint property_id,
                                      const GValue *value,
                                      GParamSpec *pspec);
static void xfce_desktop_get_property(GObject *object,
                                      guint property_id,
                                      GValue *value,
                                      GParamSpec *pspec);

static void xfce_desktop_realize(GtkWidget *widget);
static void xfce_desktop_unrealize(GtkWidget *widget);

static gboolean xfce_desktop_draw(GtkWidget *w,
                                  cairo_t *cr);
static gboolean xfce_desktop_enter_leave_event(GtkWidget *w,
                                               GdkEventCrossing *event);
static gboolean xfce_desktop_focus_in_out_event(GtkWidget *w,
                                                GdkEventFocus *event);
static void xfce_desktop_style_updated(GtkWidget *w);

static void xfce_desktop_set_single_workspace_mode(XfceDesktop *desktop,
                                                   gboolean single_workspace);
static void xfce_desktop_set_single_workspace_number(XfceDesktop *desktop,
                                                     gint workspace_num);

static gboolean update_backdrop_workspace(XfceDesktop *desktop);


static struct
{
    const gchar *setting;
    GType setting_type;
    const gchar *property;
} setting_bindings[] = {
    { SINGLE_WORKSPACE_MODE, G_TYPE_BOOLEAN, "single-workspace-mode" },
    { SINGLE_WORKSPACE_NUMBER, G_TYPE_INT, "single-workspace-number" },
};

/* private functions */

static void
xfce_desktop_place_on_monitor(XfceDesktop *desktop) {
    GdkRectangle geom;
    xfw_monitor_get_logical_geometry(desktop->monitor, &geom);

    DBG("Moving desktop for geometry %dx%d+%d+%d for %s",
        geom.width, geom.height,
        geom.x, geom.y,
        xfw_monitor_get_description(desktop->monitor));

    gtk_widget_set_size_request(GTK_WIDGET(desktop), geom.width, geom.height);

#ifdef ENABLE_X11
    if (xfw_windowing_get() == XFW_WINDOWING_X11) {
        gtk_window_move(GTK_WINDOW(desktop), geom.x, geom.y);
    }
#endif

    // On wayland, layer-shell should already have anchored us to the top-left
    // corner of the monitor, so no need to change the position.
}

static void
set_accountsservice_user_bg(const gchar *background)
{
    GDBusConnection *bus;
    GVariant *variant;
    GError *error = NULL;
    gchar *object_path = NULL;

    bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
    if (bus == NULL) {
        g_warning ("Failed to get system bus: %s", error->message);
        g_error_free (error);
        return;
    }

    variant = g_dbus_connection_call_sync (bus,
                                           "org.freedesktop.Accounts",
                                           "/org/freedesktop/Accounts",
                                           "org.freedesktop.Accounts",
                                           "FindUserByName",
                                           g_variant_new ("(s)", g_get_user_name ()),
                                           G_VARIANT_TYPE ("(o)"),
                                           G_DBUS_CALL_FLAGS_NONE,
                                           -1,
                                           NULL,
                                           &error);

    if (variant == NULL) {
        DBG("Could not contact accounts service to look up '%s': %s",
            g_get_user_name (), error->message);
        g_error_free(error);
        g_object_unref (bus);
        return;
    }

    g_variant_get(variant, "(o)", &object_path);
    g_variant_unref(variant);

    variant = g_dbus_connection_call_sync (bus,
                                           "org.freedesktop.Accounts",
                                           object_path,
                                           "org.freedesktop.DBus.Properties",
                                           "Set",
                                           g_variant_new ("(ssv)",
                                                          "org.freedesktop.DisplayManager.AccountsService",
                                                          "BackgroundFile",
                                                          g_variant_new_string (background ? background : "")),
                                           G_VARIANT_TYPE ("()"),
                                           G_DBUS_CALL_FLAGS_NONE,
                                           -1,
                                           NULL,
                                           &error);
    if (variant != NULL) {
        g_variant_unref (variant);
    } else {
        g_warning ("Failed to register the newly set background with AccountsService '%s': %s", background, error->message);
        g_clear_error (&error);
    }

    g_free (object_path);
    g_object_unref (bus);
}

static void
backdrop_loaded(cairo_surface_t *surface, GdkRectangle *region, GFile *image_file, GError *error, gpointer user_data) {
    XfceDesktop *desktop = XFCE_DESKTOP(user_data);

    DBG("entering, surface=%p, dims=%dx%d+%d+%d",
        surface,
        region != NULL ? region->width : 0,
        region != NULL ? region->height : 0,
        region != NULL ? region->x : 0,
        region != NULL ? region->y : 0);

    if (error != NULL) {
        if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            DBG("backdrop loading cancelled");
        } else {
            g_clear_object(&desktop->backdrop_load_cancellable);
            g_message("Failed to load backdrop for monitor %s: %s",
                      xfw_monitor_get_connector(desktop->monitor),
                      error->message);
        }
    } else if (surface != NULL) {
        g_clear_object(&desktop->backdrop_load_cancellable);

        if (desktop->bg_surface != surface) {
            if (desktop->bg_surface != NULL) {
                cairo_surface_destroy(desktop->bg_surface);
            }
            desktop->bg_surface = cairo_surface_reference(surface);
        }
        desktop->bg_surface_region = *region;

#ifdef ENABLE_X11
        if (xfw_monitor_is_primary(desktop->monitor) && xfw_windowing_get() == XFW_WINDOWING_X11) {
            gint monitor_idx = -1;
            for (GList *l = xfw_screen_get_monitors(desktop->xfw_screen); l != NULL; l = l->next) {
                if (XFW_MONITOR(l->data) == desktop->monitor) {
                    xfdesktop_x11_set_root_image_file_property(desktop->gscreen,
                                                               monitor_idx,
                                                               image_file != NULL
                                                               ? g_file_peek_path(image_file)
                                                               : NULL);
                    break;
                }
                monitor_idx++;
            }

            /* do this again so apps watching the root win notice the update */
            xfdesktop_x11_set_root_image_surface(desktop->gscreen, surface);
            xfdesktop_x11_set_compat_properties(GTK_WIDGET(desktop));
        }
#endif  /* ENABLE_X11 */

        if (xfw_monitor_is_primary(desktop->monitor)) {
            set_accountsservice_user_bg(image_file != NULL ? g_file_peek_path(image_file) : NULL);
        }

        gtk_widget_queue_draw(GTK_WIDGET(desktop));
    }
}

static void
fetch_backdrop(XfceDesktop *desktop) {
    TRACE("entering");
    if (gtk_widget_get_realized(GTK_WIDGET(desktop)) && desktop->backdrop_workspace != NULL) {
        if (desktop->backdrop_load_cancellable != NULL) {
            g_cancellable_cancel(desktop->backdrop_load_cancellable);
            g_object_unref(desktop->backdrop_load_cancellable);
        }
        desktop->backdrop_load_cancellable = g_cancellable_new();

        xfdesktop_backdrop_manager_get_image_surface(desktop->backdrop_manager,
                                                     desktop->backdrop_load_cancellable,
                                                     desktop->monitor,
                                                     desktop->backdrop_workspace,
                                                     backdrop_loaded,
                                                     desktop);
    }
}

static void
screen_composited_changed_cb(GdkScreen *gscreen, XfceDesktop *desktop) {
    fetch_backdrop(desktop);
}

static void
monitor_prop_changed(XfwMonitor *monitor, GParamSpec *pspec, XfceDesktop *desktop) {
    xfce_desktop_place_on_monitor(desktop);
    fetch_backdrop(desktop);
}

static void
workspace_changed_cb(XfwWorkspaceGroup *group, XfwWorkspace *previously_active_space, XfceDesktop *desktop) {
    TRACE("entering");
    update_backdrop_workspace(desktop);
}

static void
group_workspace_added(XfwWorkspaceGroup *group, XfwWorkspace *workspace, XfceDesktop *desktop) {
    DBG("entering");
    if (g_list_find(desktop->workspaces, workspace) == NULL) {
        desktop->workspaces = g_list_prepend(desktop->workspaces, workspace);
    }
    // Run this again; if ->single_workspace is NULL, it will try to populate it
    xfce_desktop_set_single_workspace_number(desktop, desktop->single_workspace_num);
}

static void
group_workspace_removed(XfwWorkspaceGroup *group, XfwWorkspace *workspace, XfceDesktop *desktop) {
    desktop->workspaces = g_list_remove(desktop->workspaces, workspace);
    if (desktop->active_workspace == workspace) {
        desktop->active_workspace = NULL;
    }
    if (desktop->single_workspace == workspace) {
        desktop->single_workspace = NULL;
    }
    if (desktop->backdrop_workspace == workspace) {
        desktop->backdrop_workspace = NULL;
        update_backdrop_workspace(desktop);
    }
}

static void
group_monitor_added(XfwWorkspaceGroup *group, XfwMonitor *monitor, XfceDesktop *desktop) {
    if (monitor == desktop->monitor) {
        desktop->workspace_group = group;
        g_signal_connect(group, "workspace-added",
                         G_CALLBACK(group_workspace_added), desktop);
        g_signal_connect(group, "workspace-removed",
                         G_CALLBACK(group_workspace_removed), desktop);
        g_signal_connect(group, "active-workspace-changed",
                         G_CALLBACK(workspace_changed_cb), desktop);
        for (GList *l = xfw_workspace_group_list_workspaces(group); l; l = l->next) {
            group_workspace_added(desktop->workspace_group, XFW_WORKSPACE(l->data), desktop);
        }
        update_backdrop_workspace(desktop);
    }
}

static void
group_monitor_removed(XfwWorkspaceGroup *group, XfwMonitor *monitor, XfceDesktop *desktop) {
    if (monitor == desktop->monitor) {
        desktop->workspace_group = NULL;
        g_signal_handlers_disconnect_by_func(group, group_workspace_added, desktop);
        g_signal_handlers_disconnect_by_func(group, group_workspace_removed, desktop);
        g_signal_handlers_disconnect_by_func(group, workspace_changed_cb, desktop);
        update_backdrop_workspace(desktop);
    }
}

static void
workspace_group_created_cb(XfwWorkspaceManager* manager,
                           XfwWorkspaceGroup *group,
                           gpointer user_data)
{
    XfceDesktop *desktop = XFCE_DESKTOP(user_data);

    TRACE("entering");

    g_signal_connect(group, "monitor-added",
                     G_CALLBACK(group_monitor_added), desktop);
    g_signal_connect(group, "monitor-removed",
                     G_CALLBACK(group_monitor_removed), desktop);

    GList *monitors = xfw_workspace_group_get_monitors(group);
    if (g_list_find(monitors, desktop->monitor) != NULL) {
        group_monitor_added(group, desktop->monitor, desktop);
    }
}

static void
workspace_group_destroyed_cb(XfwWorkspaceManager *manager,
                             XfwWorkspaceGroup *group,
                             gpointer user_data)
{
    XfceDesktop *desktop = XFCE_DESKTOP(user_data);

    TRACE("entering");

    group_monitor_removed(group, desktop->monitor, desktop);
    g_signal_handlers_disconnect_by_data(group, desktop);
}

static void
manager_backdrop_changed(XfdesktopBackdropManager *manager,
                         XfwMonitor *monitor,
                         XfwWorkspace *workspace,
                         XfceDesktop *desktop)
{
    DBG("entering: monitor=%p, our monitor=%p, workspace=%d, our workspace=%d",
        monitor, desktop->monitor, xfw_workspace_get_number(workspace),
        xfw_workspace_get_number(desktop->backdrop_workspace));
    if (monitor == desktop->monitor && workspace == desktop->backdrop_workspace) {
        fetch_backdrop(desktop);
    }
}


G_DEFINE_TYPE(XfceDesktop, xfce_desktop, GTK_TYPE_WINDOW)


static void
xfce_desktop_class_init(XfceDesktopClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;
    GtkWidgetClass *widget_class = (GtkWidgetClass *)klass;

    gobject_class->constructed = xfce_desktop_constructed;
    gobject_class->finalize = xfce_desktop_finalize;
    gobject_class->set_property = xfce_desktop_set_property;
    gobject_class->get_property = xfce_desktop_get_property;

    widget_class->realize = xfce_desktop_realize;
    widget_class->unrealize = xfce_desktop_unrealize;
    widget_class->draw = xfce_desktop_draw;
    widget_class->enter_notify_event = xfce_desktop_enter_leave_event;
    widget_class->leave_notify_event = xfce_desktop_enter_leave_event;
    widget_class->focus_in_event = xfce_desktop_focus_in_out_event;
    widget_class->focus_out_event = xfce_desktop_focus_in_out_event;
    widget_class->style_updated = xfce_desktop_style_updated;

    g_object_class_install_property(gobject_class, PROP_SCREEN,
                                    g_param_spec_object("screen",
                                                        "gdk screen",
                                                        "gdk screen",
                                                        GDK_TYPE_SCREEN,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_MONITOR,
                                    g_param_spec_object("monitor",
                                                        "xfw monitor",
                                                        "xfw monitor",
                                                        XFW_TYPE_MONITOR,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_CHANNEL,
                                    g_param_spec_object("channel",
                                                        "xfconf channel",
                                                        "xfconf channel",
                                                        XFCONF_TYPE_CHANNEL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_PROPERTY_PREFIX,
                                    g_param_spec_string("property-prefix",
                                                        "xfconf property prefix",
                                                        "xfconf property prefix",
                                                        "",
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_BACKDROP_MANAGER,
                                    g_param_spec_object("backdrop-manager",
                                                        "backdrop manager",
                                                        "backdrop manager",
                                                        XFDESKTOP_TYPE_BACKDROP_MANAGER,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_SINGLE_WORKSPACE_MODE,
                                    g_param_spec_boolean("single-workspace-mode",
                                                         "single-workspace-mode",
                                                         "single-workspace-mode",
                                                         TRUE,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_SINGLE_WORKSPACE_NUMBER,
                                    g_param_spec_int("single-workspace-number",
                                                     "single-workspace-number",
                                                     "single-workspace-number",
                                                     -1, G_MAXINT16, -1,
                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class,
                                    PROP_ACTIVE,
                                    g_param_spec_boolean("active",
                                                         "active",
                                                         "active",
                                                         FALSE,
                                                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

static void
xfce_desktop_init(XfceDesktop *desktop)
{
    desktop->single_workspace_num = -1;
}

static void
xfce_desktop_constructed(GObject *obj)
{
    XfceDesktop *desktop = XFCE_DESKTOP(obj);
    XfwWorkspaceManager *workspace_manager;

    G_OBJECT_CLASS(xfce_desktop_parent_class)->constructed(obj);

    gtk_window_set_screen(GTK_WINDOW(desktop), desktop->gscreen);
    gtk_window_set_type_hint(GTK_WINDOW(desktop), GDK_WINDOW_TYPE_HINT_DESKTOP);
    /* Accept focus is needed for the menu pop up either by the menu key on
     * the keyboard or Shift+F10. */
    gtk_window_set_accept_focus(GTK_WINDOW(desktop), TRUE);
    /* Can focus is needed for the gtk_grab_add/remove commands */
    gtk_widget_set_can_focus(GTK_WIDGET(desktop), TRUE);
    gtk_window_set_resizable(GTK_WINDOW(desktop), FALSE);
    gtk_window_set_title(GTK_WINDOW(desktop), _("Desktop"));
    gtk_window_set_decorated(GTK_WINDOW(desktop), FALSE);

#ifdef ENABLE_WAYLAND
    if (xfw_windowing_get() == XFW_WINDOWING_WAYLAND) {
        gtk_layer_init_for_window(GTK_WINDOW(desktop));
    }
#endif

    if (desktop->channel != NULL) {
        for (gsize i = 0; i < G_N_ELEMENTS(setting_bindings); ++i) {
            g_assert(setting_bindings[i].setting_type != 0);
            xfconf_g_property_bind(desktop->channel,
                                   setting_bindings[i].setting, setting_bindings[i].setting_type,
                                   G_OBJECT(desktop), setting_bindings[i].property);
        }
    }

    desktop->xfw_screen = xfw_screen_get_default();
    workspace_manager = xfw_screen_get_workspace_manager(desktop->xfw_screen);
    desktop->workspace_manager = workspace_manager;

    /* watch for workspace changes */
    for (GList *gl = xfw_workspace_manager_list_workspace_groups(workspace_manager);
         gl != NULL;
         gl = gl->next)
    {
        workspace_group_created_cb(workspace_manager, XFW_WORKSPACE_GROUP(gl->data), desktop);
    }
    g_signal_connect(workspace_manager, "workspace-group-created",
                     G_CALLBACK(workspace_group_created_cb), desktop);
    g_signal_connect(workspace_manager, "workspace-group-destroyed",
                     G_CALLBACK(workspace_group_destroyed_cb), desktop);

    g_signal_connect(desktop->backdrop_manager, "backdrop-changed",
                     G_CALLBACK(manager_backdrop_changed), desktop);
}

static void
xfce_desktop_finalize(GObject *object)
{
    XfceDesktop *desktop = XFCE_DESKTOP(object);

    g_signal_handlers_disconnect_by_data(desktop->backdrop_manager, desktop);
    g_signal_handlers_disconnect_by_data(desktop->workspace_manager, desktop);
    for (GList *l = xfw_workspace_manager_list_workspace_groups(desktop->workspace_manager);
         l != NULL;
         l = l->next)
    {
        g_signal_handlers_disconnect_by_data(l->data, desktop);
    }

    if (desktop->backdrop_load_cancellable != NULL) {
        g_cancellable_cancel(desktop->backdrop_load_cancellable);
        g_object_unref(desktop->backdrop_load_cancellable);
    }

    g_list_free(desktop->workspaces);
    g_object_unref(desktop->xfw_screen);

    if (desktop->channel != NULL) {
        g_object_unref(G_OBJECT(desktop->channel));
    }
    g_free(desktop->property_prefix);

#ifdef ENABLE_DESKTOP_ICONS
    if(desktop->style_refresh_timer != 0)
        g_source_remove(desktop->style_refresh_timer);
#endif

    g_signal_handlers_disconnect_by_data(desktop->monitor, desktop);
    g_object_unref(desktop->monitor);

    G_OBJECT_CLASS(xfce_desktop_parent_class)->finalize(object);
}

static void
xfce_desktop_set_property(GObject *object,
                          guint property_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
    XfceDesktop *desktop = XFCE_DESKTOP(object);

    switch(property_id) {
        case PROP_SCREEN:
            desktop->gscreen = g_value_get_object(value);
            break;

        case PROP_MONITOR:
            xfce_desktop_update_monitor(desktop, g_value_get_object(value));
            break;

        case PROP_CHANNEL:
            desktop->channel = g_value_dup_object(value);
            break;

        case PROP_PROPERTY_PREFIX:
            desktop->property_prefix = g_value_dup_string(value);
            break;

        case PROP_BACKDROP_MANAGER:
            desktop->backdrop_manager = g_value_get_object(value);
            break;

        case PROP_SINGLE_WORKSPACE_MODE:
            xfce_desktop_set_single_workspace_mode(desktop,
                                                   g_value_get_boolean(value));
            break;

        case PROP_SINGLE_WORKSPACE_NUMBER:
            xfce_desktop_set_single_workspace_number(desktop,
                                                     g_value_get_int(value));
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
xfce_desktop_get_property(GObject *object,
                          guint property_id,
                          GValue *value,
                          GParamSpec *pspec)
{
    XfceDesktop *desktop = XFCE_DESKTOP(object);

    switch(property_id) {
        case PROP_SCREEN:
            g_value_set_object(value, desktop->gscreen);
            break;

        case PROP_MONITOR:
            g_value_set_object(value, desktop->monitor);
            break;

        case PROP_CHANNEL:
            g_value_set_object(value, desktop->channel);
            break;

        case PROP_PROPERTY_PREFIX:
            g_value_set_string(value, desktop->property_prefix);
            break;

        case PROP_BACKDROP_MANAGER:
            g_value_set_object(value, desktop->backdrop_manager);
            break;

        case PROP_SINGLE_WORKSPACE_MODE:
            g_value_set_boolean(value, desktop->single_workspace_mode);
            break;

        case PROP_SINGLE_WORKSPACE_NUMBER:
            g_value_set_int(value, desktop->single_workspace_num);
            break;

        case PROP_ACTIVE:
            g_value_set_boolean(value, xfce_desktop_is_active(desktop));
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
xfce_desktop_realize(GtkWidget *widget)
{
    XfceDesktop *desktop = XFCE_DESKTOP(widget);

    TRACE("entering");

#ifdef ENABLE_WAYLAND
    if (xfw_windowing_get() == XFW_WINDOWING_WAYLAND) {
        GtkWindow *window = GTK_WINDOW(desktop);
        gtk_layer_set_layer(window, GTK_LAYER_SHELL_LAYER_BACKGROUND);
        gtk_layer_set_monitor(window, xfw_monitor_get_gdk_monitor(desktop->monitor));
        gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
        gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
        gtk_layer_set_margin(window, GTK_LAYER_SHELL_EDGE_TOP, 0);
        gtk_layer_set_margin(window, GTK_LAYER_SHELL_EDGE_LEFT, 0);
        gtk_layer_set_margin(window, GTK_LAYER_SHELL_EDGE_BOTTOM, 0);
        gtk_layer_set_margin(window, GTK_LAYER_SHELL_EDGE_RIGHT, 0);
        gtk_layer_set_exclusive_zone(window, -1);
        gtk_layer_set_namespace(window, "desktop");
    }
#endif

    /* chain up */
    GTK_WIDGET_CLASS(xfce_desktop_parent_class)->realize(widget);

    xfce_desktop_place_on_monitor(desktop);
    gdk_window_lower(gtk_widget_get_window(widget));

    g_signal_connect(G_OBJECT(desktop->gscreen), "composited-changed",
                     G_CALLBACK(screen_composited_changed_cb), desktop);

    gtk_widget_add_events(GTK_WIDGET(desktop), GDK_EXPOSURE_MASK);

    xfce_desktop_refresh(desktop);

    TRACE("exiting");
}

static void
xfce_desktop_unrealize(GtkWidget *widget)
{
    XfceDesktop *desktop = XFCE_DESKTOP(widget);

    g_return_if_fail(XFCE_IS_DESKTOP(desktop));

    g_signal_handlers_disconnect_by_func(G_OBJECT(desktop->gscreen),
                                         G_CALLBACK(screen_composited_changed_cb), desktop);

#ifdef ENABLE_X11
    if (xfw_monitor_is_primary(desktop->monitor) && xfw_windowing_get() == XFW_WINDOWING_X11) {
        xfdesktop_x11_set_root_image_surface(desktop->gscreen, NULL);
        xfdesktop_x11_set_compat_properties(NULL);

        gint monitor_idx = -1;
        for (GList *l = xfw_screen_get_monitors(desktop->xfw_screen); l != NULL; l = l->next) {
            if (XFW_MONITOR(l->data) == desktop->monitor) {
                xfdesktop_x11_set_root_image_file_property(desktop->gscreen,
                                                           monitor_idx,
                                                           NULL);
                break;
            }
            monitor_idx++;
        }
     }
#endif

    if (desktop->bg_surface != NULL) {
        cairo_surface_destroy(desktop->bg_surface);
        desktop->bg_surface = NULL;
    }

    GTK_WIDGET_CLASS(xfce_desktop_parent_class)->unrealize(widget);
}

static gboolean
xfce_desktop_draw(GtkWidget *w,
                  cairo_t *cr)
{
    XfceDesktop *desktop = XFCE_DESKTOP(w);

    cairo_save(cr);

    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);

    if (desktop->bg_surface != NULL) {
        gdouble scale = xfw_monitor_get_fractional_scale(desktop->monitor);
        cairo_scale(cr, 1.0 / scale, 1.0 / scale);
        cairo_set_source_surface(cr,
                                 desktop->bg_surface,
                                 0 - desktop->bg_surface_region.x,
                                 0 - desktop->bg_surface_region.y);
        cairo_rectangle(cr,
                        0,
                        0,
                        desktop->bg_surface_region.width,
                        desktop->bg_surface_region.height);
        cairo_fill(cr);
    } else {
        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);
        cairo_paint(cr);
    }

    cairo_restore(cr);

    GList *children = gtk_container_get_children(GTK_CONTAINER(w));
    for (GList *l = children; l; l = l->next) {
        gtk_container_propagate_draw(GTK_CONTAINER(w),
                                     GTK_WIDGET(l->data),
                                     cr);
    }
    g_list_free(children);

    return FALSE;
}

static gboolean
xfce_desktop_enter_leave_event(GtkWidget *w, GdkEventCrossing *event) {
    XfceDesktop *desktop = XFCE_DESKTOP(w);
    gboolean old_is_active = xfce_desktop_is_active(desktop);

    desktop->has_pointer = event->type == GDK_ENTER_NOTIFY;

    gboolean (*callback)(GtkWidget *, GdkEventCrossing *) = desktop->has_pointer
        ? GTK_WIDGET_CLASS(xfce_desktop_parent_class)->enter_notify_event
        : GTK_WIDGET_CLASS(xfce_desktop_parent_class)->leave_notify_event;
    gboolean ret = callback != NULL ? callback(w, event) : FALSE;

    if (old_is_active != xfce_desktop_is_active(desktop)) {
        g_object_notify(G_OBJECT(w), "active");
    }

    return ret;
}

static gboolean
xfce_desktop_focus_in_out_event(GtkWidget *w, GdkEventFocus *event) {
    gboolean has_focus = event->in;

    gboolean (*callback)(GtkWidget *, GdkEventFocus *) = has_focus
        ? GTK_WIDGET_CLASS(xfce_desktop_parent_class)->focus_in_event
        : GTK_WIDGET_CLASS(xfce_desktop_parent_class)->focus_out_event;
    gboolean ret = callback != NULL ? callback(w, event) : FALSE;

    XfceDesktop *desktop = XFCE_DESKTOP(w);
    if ((has_focus && !desktop->is_active) || (!has_focus && desktop->is_active)) {
        g_object_notify(G_OBJECT(w), "active");
    }

    return ret;
}

#ifdef ENABLE_DESKTOP_ICONS
static gboolean
style_refresh_cb(gpointer user_data)
{
    XfceDesktop *desktop = user_data;
    GList *children;

    TRACE("entering");

    desktop->style_refresh_timer = 0;

    g_return_val_if_fail(XFCE_IS_DESKTOP(desktop), FALSE);

    if(!gtk_widget_get_realized(GTK_WIDGET(desktop)))
        return FALSE;

    if (desktop->workspaces == NULL) {
        return FALSE;
    }

    gtk_widget_queue_draw(GTK_WIDGET(desktop));

    children = gtk_container_get_children(GTK_CONTAINER(desktop));
    for (GList *l = children; l != NULL; l = l->next) {
        if (GTK_IS_WIDGET(l->data)) {
            gtk_widget_reset_style(GTK_WIDGET(l->data));
        }
    }
    g_list_free(children);

    return FALSE;
}
#endif

static void
xfce_desktop_style_updated(GtkWidget *w)
{
#ifdef ENABLE_DESKTOP_ICONS
    XfceDesktop *desktop = XFCE_DESKTOP(w);

    TRACE("entering");

    if (desktop->style_refresh_timer != 0) {
        g_source_remove(desktop->style_refresh_timer);
    }

    desktop->style_refresh_timer = g_idle_add_full(G_PRIORITY_LOW,
                                                         style_refresh_cb,
                                                         desktop,
                                                         NULL);
#endif

    GTK_WIDGET_CLASS(xfce_desktop_parent_class)->style_updated(w);
}

static gboolean
update_backdrop_workspace(XfceDesktop *desktop) {
    TRACE("entering");

    if (desktop->workspace_group != NULL) {
        desktop->active_workspace = xfw_workspace_group_get_active_workspace(desktop->workspace_group);
    } else {
        desktop->active_workspace = NULL;
    }

    XfwWorkspace *old_backdrop_workspace = desktop->backdrop_workspace;

    if (desktop->single_workspace_mode && desktop->single_workspace != NULL) {
        DBG("using single_workspace");
        desktop->backdrop_workspace = desktop->single_workspace;
    } else if (desktop->active_workspace != NULL) {
        DBG("using active_workspace");
        desktop->backdrop_workspace = desktop->active_workspace;
    } else {
        XfwWorkspace *lowest_workspace = NULL;

        for (GList *l = desktop->workspaces; l != NULL; l = l->next) {
            XfwWorkspace *workspace = XFW_WORKSPACE(l->data);
            if (lowest_workspace == NULL ||
                xfw_workspace_get_number(workspace) < xfw_workspace_get_number(lowest_workspace))
            {
                lowest_workspace = workspace;
            }
        }
        DBG("using lowest_workspace");
        desktop->backdrop_workspace = lowest_workspace;
    }

    XF_DEBUG("new_active_workspace %d, new_backdrop_workspace %d",
             desktop->active_workspace != NULL ? (gint)xfw_workspace_get_number(desktop->active_workspace) : -1,
             desktop->backdrop_workspace != NULL ? (gint)xfw_workspace_get_number(desktop->backdrop_workspace) : -1);

    if (desktop->backdrop_workspace != old_backdrop_workspace || desktop->bg_surface == NULL) {
        fetch_backdrop(desktop);
        return TRUE;
    } else {
        return FALSE;
    }
}

/* public api */

/**
 * xfce_desktop_new:
 * @gscreen: The current #GdkScreen.
 * @monitor: #XfwMonitor to display the widget on.
 * @channel: An #XfconfChannel to use for settings.
 * @property_prefix: String prefix for per-screen properties.
 * @backdrop_manager: An #XfdesktopBackdropManager.
 *
 * Creates a new #XfceDesktop for the specified #GdkScreen.  Settings
 * will be fetched using @channel.  Per-screen/monitor settings will
 * have @property_prefix prepended to Xfconf property names.
 *
 * Return value: A new #XfceDesktop.
 **/
GtkWidget *
xfce_desktop_new(GdkScreen *gscreen,
                 XfwMonitor *monitor,
                 XfconfChannel *channel,
                 const gchar *property_prefix,
                 XfdesktopBackdropManager *backdrop_manager)
{
    g_return_val_if_fail(GDK_IS_SCREEN(gscreen), NULL);
    g_return_val_if_fail(XFW_IS_MONITOR(monitor), NULL);
    g_return_val_if_fail(channel == NULL || XFCONF_IS_CHANNEL(channel), NULL);
    g_return_val_if_fail(property_prefix != NULL, NULL);
    g_return_val_if_fail(XFDESKTOP_IS_BACKDROP_MANAGER(backdrop_manager), NULL);

    return g_object_new(XFCE_TYPE_DESKTOP,
                        "screen", gscreen,
                        "monitor", monitor,
                        "channel", channel,
                        "property-prefix", property_prefix,
                        "backdrop-manager", backdrop_manager,
                        NULL);
}

XfwMonitor *
xfce_desktop_get_monitor(XfceDesktop *desktop) {
    g_return_val_if_fail(XFCE_IS_DESKTOP(desktop), NULL);
    return desktop->monitor;
}

void
xfce_desktop_update_monitor(XfceDesktop *desktop, XfwMonitor *monitor) {
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));
    g_return_if_fail(XFW_IS_MONITOR(monitor));

    if (desktop->monitor != monitor) {
        if (desktop->monitor != NULL) {
            g_signal_handlers_disconnect_by_data(desktop->monitor, desktop);
            g_object_unref(desktop->monitor);
        }

        desktop->monitor = g_object_ref(monitor);

        g_signal_connect(monitor, "notify::logical-geometry",
                         G_CALLBACK(monitor_prop_changed), desktop);
        g_signal_connect(monitor, "notify::scale",
                         G_CALLBACK(monitor_prop_changed), desktop);

        if (gtk_widget_get_realized(GTK_WIDGET(desktop))) {
            xfce_desktop_place_on_monitor(desktop);
            fetch_backdrop(desktop);
        }

        g_object_notify(G_OBJECT(desktop), "monitor");
    }

}

static void
xfce_desktop_set_single_workspace_mode(XfceDesktop *desktop,
                                       gboolean single_workspace)
{
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));

    if (single_workspace != desktop->single_workspace_mode) {
        desktop->single_workspace_mode = single_workspace;
        XF_DEBUG("single_workspace_mode now %s", single_workspace ? "TRUE" : "FALSE");
        update_backdrop_workspace(desktop);
    }
}

static void
xfce_desktop_set_single_workspace_number(XfceDesktop *desktop,
                                         gint workspace_num)
{
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));

    if (workspace_num >= 0
        && (workspace_num != desktop->single_workspace_num
            || desktop->single_workspace == NULL))
    {
        if (workspace_num != desktop->single_workspace_num) {
            XF_DEBUG("single_workspace_num now %d", workspace_num);
        }
        desktop->single_workspace_num = workspace_num;

        desktop->single_workspace = NULL;
        for (GList *l = desktop->workspaces; l != NULL; l = l->next) {
            XfwWorkspace *workspace = XFW_WORKSPACE(l->data);
            if ((gint)xfw_workspace_get_number(workspace) == workspace_num) {
                desktop->single_workspace = workspace;
                break;
            }
        }

        update_backdrop_workspace(desktop);
    }
}

void
xfce_desktop_freeze_updates(XfceDesktop *desktop)
{
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));
    desktop->updates_frozen = TRUE;
}

void
xfce_desktop_thaw_updates(XfceDesktop *desktop)
{
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));

    if (desktop->updates_frozen) {
        desktop->updates_frozen = FALSE;
        fetch_backdrop(desktop);
    }
}

void
xfce_desktop_set_is_active(XfceDesktop *desktop, gboolean active) {
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));

    if (active != desktop->is_active) {
        desktop->is_active = active;

        if (!gtk_window_has_toplevel_focus(GTK_WINDOW(desktop))) {
            g_object_notify(G_OBJECT(desktop), "active");
        }
    }
}

gboolean
xfce_desktop_is_active(XfceDesktop *desktop) {
    g_return_val_if_fail(XFCE_IS_DESKTOP(desktop), FALSE);
    return desktop->is_active
        || desktop->has_pointer
        || gtk_window_has_toplevel_focus(GTK_WINDOW(desktop));
}

void
xfce_desktop_refresh(XfceDesktop *desktop) {
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));

    if (desktop->backdrop_workspace != NULL) {
        fetch_backdrop(desktop);
    }
}

void
xfce_desktop_cycle_backdrop(XfceDesktop *desktop) {
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));

    if (desktop->backdrop_workspace != NULL) {
        xfdesktop_backdrop_manager_cycle_backdrop(desktop->backdrop_manager,
                                                  desktop->monitor,
                                                  desktop->backdrop_workspace);
    }
}
