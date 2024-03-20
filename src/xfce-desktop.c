/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2004-2007 Brian Tarricone, <brian@tarricone.org>
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

#include <ctype.h>
#include <errno.h>

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include <glib.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

#ifdef ENABLE_X11
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <gdk/gdkx.h>
#include <cairo-xlib.h>
#endif  /* ENABLE_X11 */

#ifdef ENABLE_WAYLAND
#include <gtk-layer-shell.h>
#endif

#ifdef ENABLE_DESKTOP_ICONS
#include "xfdesktop-icon-view.h"
#include "xfdesktop-window-icon-manager.h"
# ifdef ENABLE_FILE_ICONS
# include "xfdesktop-file-icon-manager.h"
# include "xfdesktop-special-file-icon.h"
# endif
#endif

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4windowing/libxfce4windowing.h>

#include <xfconf/xfconf.h>

#include "menu.h"
#include "windowlist.h"
#include "xfdesktop-backdrop-manager.h"
#include "xfdesktop-common.h"
#include "xfce-desktop.h"

#ifdef ENABLE_X11
#include "xfdesktop-x11.h"
#endif

/* disable setting the x background for bug 7442 */
//#define DISABLE_FOR_BUG7442

struct _XfceDesktopPrivate
{
    GdkScreen *gscreen;
    XfwScreen *xfw_screen;
    GdkMonitor *monitor;
    XfdesktopBackdropManager *backdrop_manager;
    XfwWorkspaceManager *workspace_manager;
    gboolean updates_frozen;

    XfconfChannel *channel;
    gchar *property_prefix;

    GCancellable *backdrop_load_cancellable;
    cairo_surface_t *bg_surface;
    GdkRectangle bg_surface_region;

    GList *workspaces;  // XfwWorkspace
    XfwWorkspace *active_workspace;

    gboolean single_workspace_mode;
    gint single_workspace_num;
    XfwWorkspace *single_workspace;

    gboolean has_pointer;

#ifdef ENABLE_DESKTOP_ICONS
    gint style_refresh_timer;
#endif

    gchar *last_filename;
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
static void xfce_desktop_style_updated(GtkWidget *w);

static void xfce_desktop_set_single_workspace_mode(XfceDesktop *desktop,
                                                   gboolean single_workspace);
static void xfce_desktop_set_single_workspace_number(XfceDesktop *desktop,
                                                     gint workspace_num);

static gboolean xfce_desktop_get_single_workspace_mode(XfceDesktop *desktop);
static XfwWorkspace *xfce_desktop_get_current_workspace(XfceDesktop *desktop);


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
    gdk_monitor_get_geometry(desktop->priv->monitor, &geom);

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

    g_object_unref (bus);
}

static void
backdrop_loaded(cairo_surface_t *surface, GdkRectangle *region, const gchar *image_filename, GError *error, gpointer user_data) {
    XfceDesktop *desktop = XFCE_DESKTOP(user_data);

    DBG("entering");
    if (error != NULL) {
        if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            DBG("backdrop loading cancelled");
        } else {
            g_clear_object(&desktop->priv->backdrop_load_cancellable);
            g_message("Failed to load backdrop for monitor %s: %s",
                      gdk_monitor_get_model(desktop->priv->monitor),
                      error->message);
        }
    } else if (surface != NULL) {
        g_clear_object(&desktop->priv->backdrop_load_cancellable);

        if (desktop->priv->bg_surface != surface) {
            if (desktop->priv->bg_surface != NULL) {
                cairo_surface_destroy(desktop->priv->bg_surface);
            }
            desktop->priv->bg_surface = cairo_surface_reference(surface);
        }
        desktop->priv->bg_surface_region = *region;

#ifdef ENABLE_X11
        if (xfw_windowing_get() == XFW_WINDOWING_X11) {
            xfdesktop_x11_set_root_image_file_property(desktop->priv->gscreen,
                                                       desktop->priv->monitor,
                                                       image_filename);

            /* do this again so apps watching the root win notice the update */
            xfdesktop_x11_set_root_image_surface(desktop->priv->gscreen, surface);
        }
#endif  /* ENABLE_X11 */

        GdkDisplay *display = gdk_monitor_get_display(desktop->priv->monitor);
        GdkMonitor *primary = gdk_display_get_primary_monitor(display);
        if ((primary != NULL && primary == desktop->priv->monitor) ||
            (primary == NULL && gdk_display_get_monitor(display, 0) == desktop->priv->monitor))
        {
            set_accountsservice_user_bg(image_filename);
        }

        gtk_widget_queue_draw(GTK_WIDGET(desktop));
    }
}

static void
fetch_backdrop(XfceDesktop *desktop) {
    if (!gtk_widget_get_realized(GTK_WIDGET(desktop))) {
        return;
    }

    XfwWorkspace *current_workspace = xfce_desktop_get_current_workspace(desktop);
    if (current_workspace == NULL) {
        return;
    }

    if (desktop->priv->backdrop_load_cancellable != NULL) {
        g_cancellable_cancel(desktop->priv->backdrop_load_cancellable);
        g_object_unref(desktop->priv->backdrop_load_cancellable);
    }
    desktop->priv->backdrop_load_cancellable = g_cancellable_new();

    xfdesktop_backdrop_manager_get_image_surface(desktop->priv->backdrop_manager,
                                                 desktop->priv->backdrop_load_cancellable,
                                                 desktop->priv->monitor,
                                                 current_workspace,
                                                 backdrop_loaded,
                                                 desktop);
}

static void
screen_composited_changed_cb(GdkScreen *gscreen, XfceDesktop *desktop) {
    fetch_backdrop(desktop);
}

static void
monitor_prop_changed(GdkMonitor *monitor, GParamSpec *pspec, XfceDesktop *desktop) {
    xfce_desktop_place_on_monitor(desktop);
    fetch_backdrop(desktop);
}

static void
workspace_changed_cb(XfwWorkspaceGroup *group, XfwWorkspace *previously_active_space, XfceDesktop *desktop) {
    TRACE("entering");

    XfwWorkspace *old_current_workspace = xfce_desktop_get_current_workspace(desktop);
    desktop->priv->active_workspace = xfw_workspace_group_get_active_workspace(group);
    XfwWorkspace *current_workspace = xfce_desktop_get_current_workspace(desktop);

    XF_DEBUG("new_active_workspace %d, new_current_workspace %d",
             desktop->priv->active_workspace != NULL ? (gint)xfw_workspace_get_number(desktop->priv->active_workspace) : -1,
             current_workspace != NULL ? (gint)xfw_workspace_get_number(current_workspace) : -1);

    if (old_current_workspace != current_workspace) {
        fetch_backdrop(desktop);
    }
}

static void
group_workspace_added(XfwWorkspaceGroup *group, XfwWorkspace *workspace, XfceDesktop *desktop) {
    DBG("entering");
    desktop->priv->workspaces = g_list_prepend(desktop->priv->workspaces, workspace);
    // Run this again; if ->single_workspace is NULL, it will try to populate it
    xfce_desktop_set_single_workspace_number(desktop, desktop->priv->single_workspace_num);
}

static void
group_workspace_removed(XfwWorkspaceGroup *group, XfwWorkspace *workspace, XfceDesktop *desktop) {
    desktop->priv->workspaces = g_list_remove(desktop->priv->workspaces, workspace);
    if (desktop->priv->active_workspace == workspace) {
        desktop->priv->active_workspace = NULL;
    }
    if (desktop->priv->single_workspace == workspace) {
        desktop->priv->single_workspace = NULL;
    }
}

static void
group_monitor_added(XfwWorkspaceGroup *group, GdkMonitor *monitor, XfceDesktop *desktop) {
    if (monitor == desktop->priv->monitor) {
        g_signal_connect(group, "workspace-added",
                         G_CALLBACK(group_workspace_added), desktop);
        g_signal_connect(group, "workspace-removed",
                         G_CALLBACK(group_workspace_removed), desktop);
        g_signal_connect(group, "active-workspace-changed",
                         G_CALLBACK(workspace_changed_cb), desktop);
        workspace_changed_cb(group, NULL, desktop);
    }
}

static void
group_monitor_removed(XfwWorkspaceGroup *group, GdkMonitor *monitor, XfceDesktop *desktop) {
    if (monitor == desktop->priv->monitor) {
        g_signal_handlers_disconnect_by_func(group, group_workspace_added, desktop);
        g_signal_handlers_disconnect_by_func(group, group_workspace_removed, desktop);
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
    if (g_list_find(monitors, desktop->priv->monitor) != NULL) {
        group_monitor_added(group, desktop->priv->monitor, desktop);
    }
}

static void
workspace_group_destroyed_cb(XfwWorkspaceManager *manager,
                             XfwWorkspaceGroup *group,
                             gpointer user_data)
{
    XfceDesktop *desktop = XFCE_DESKTOP(user_data);

    TRACE("entering");

    group_monitor_removed(group, desktop->priv->monitor, desktop);
    g_signal_handlers_disconnect_by_data(group, desktop);
}

static void
manager_backdrop_changed(XfdesktopBackdropManager *manager,
                         GdkMonitor *monitor,
                         XfwWorkspace *workspace,
                         XfceDesktop *desktop)
{
    DBG("entering: monitor=%p, our monitor=%p, workspace=%d, our workspace=%d",
        monitor, desktop->priv->monitor, xfw_workspace_get_number(workspace), xfw_workspace_get_number(xfce_desktop_get_current_workspace(desktop)));
    if (monitor == desktop->priv->monitor && workspace == xfce_desktop_get_current_workspace(desktop)) {
        fetch_backdrop(desktop);
    }
}


G_DEFINE_TYPE_WITH_PRIVATE(XfceDesktop, xfce_desktop, GTK_TYPE_WINDOW)


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
    widget_class->style_updated = xfce_desktop_style_updated;

    g_object_class_install_property(gobject_class, PROP_SCREEN,
                                    g_param_spec_object("screen",
                                                        "gdk screen",
                                                        "gdk screen",
                                                        GDK_TYPE_SCREEN,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_MONITOR,
                                    g_param_spec_object("monitor",
                                                        "gdk monitor",
                                                        "gdk monitor",
                                                        GDK_TYPE_MONITOR,
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
}

static void
xfce_desktop_init(XfceDesktop *desktop)
{
    desktop->priv = xfce_desktop_get_instance_private(desktop);
    desktop->priv->single_workspace_num = -1;
    desktop->priv->last_filename = g_strdup("");
}

static void
xfce_desktop_constructed(GObject *obj)
{
    XfceDesktop *desktop = XFCE_DESKTOP(obj);
    XfwWorkspaceManager *workspace_manager;

    G_OBJECT_CLASS(xfce_desktop_parent_class)->constructed(obj);

    gtk_window_set_screen(GTK_WINDOW(desktop), desktop->priv->gscreen);
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

    if (desktop->priv->channel != NULL) {
        for (gsize i = 0; i < G_N_ELEMENTS(setting_bindings); ++i) {
            g_assert(setting_bindings[i].setting_type != 0);
            xfconf_g_property_bind(desktop->priv->channel,
                                   setting_bindings[i].setting, setting_bindings[i].setting_type,
                                   G_OBJECT(desktop), setting_bindings[i].property);
        }
    }

    desktop->priv->xfw_screen = xfw_screen_get_default();
    workspace_manager = xfw_screen_get_workspace_manager(desktop->priv->xfw_screen);
    desktop->priv->workspace_manager = workspace_manager;

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

    g_signal_connect(desktop->priv->backdrop_manager, "backdrop-changed",
                     G_CALLBACK(manager_backdrop_changed), desktop);
}

static void
xfce_desktop_finalize(GObject *object)
{
    XfceDesktop *desktop = XFCE_DESKTOP(object);

    if (desktop->priv->backdrop_load_cancellable != NULL) {
        g_cancellable_cancel(desktop->priv->backdrop_load_cancellable);
        g_object_unref(desktop->priv->backdrop_load_cancellable);
    }

    g_list_free(desktop->priv->workspaces);
    g_object_unref(desktop->priv->xfw_screen);

    if (desktop->priv->channel != NULL) {
        g_object_unref(G_OBJECT(desktop->priv->channel));
    }
    g_free(desktop->priv->property_prefix);

#ifdef ENABLE_DESKTOP_ICONS
    if(desktop->priv->style_refresh_timer != 0)
        g_source_remove(desktop->priv->style_refresh_timer);
#endif

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
            desktop->priv->gscreen = g_value_get_object(value);
            break;

        case PROP_MONITOR:
            xfce_desktop_update_monitor(desktop, g_value_get_object(value));
            break;

        case PROP_CHANNEL:
            desktop->priv->channel = g_value_dup_object(value);
            break;

        case PROP_PROPERTY_PREFIX:
            desktop->priv->property_prefix = g_value_dup_string(value);
            break;

        case PROP_BACKDROP_MANAGER:
            desktop->priv->backdrop_manager = g_value_get_object(value);
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
            g_value_set_object(value, desktop->priv->gscreen);
            break;

        case PROP_MONITOR:
            g_value_set_object(value, desktop->priv->monitor);
            break;

        case PROP_CHANNEL:
            g_value_set_object(value, desktop->priv->channel);
            break;

        case PROP_PROPERTY_PREFIX:
            g_value_set_string(value, desktop->priv->property_prefix);
            break;

        case PROP_BACKDROP_MANAGER:
            g_value_set_object(value, desktop->priv->backdrop_manager);
            break;

        case PROP_SINGLE_WORKSPACE_MODE:
            g_value_set_boolean(value, desktop->priv->single_workspace_mode);
            break;

        case PROP_SINGLE_WORKSPACE_NUMBER:
            g_value_set_int(value, desktop->priv->single_workspace_num);
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
        gtk_layer_set_monitor(window, desktop->priv->monitor);
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

    g_signal_connect(G_OBJECT(desktop->priv->gscreen), "composited-changed",
                     G_CALLBACK(screen_composited_changed_cb), desktop);

    gtk_widget_add_events(GTK_WIDGET(desktop), GDK_EXPOSURE_MASK);

    xfce_desktop_refresh(desktop, FALSE);

    TRACE("exiting");
}

static void
xfce_desktop_unrealize(GtkWidget *widget)
{
    XfceDesktop *desktop = XFCE_DESKTOP(widget);
    GdkDisplay  *display;
    GdkWindow *groot;

    g_return_if_fail(XFCE_IS_DESKTOP(desktop));

    if(gtk_widget_get_mapped(widget))
        gtk_widget_unmap(widget);
    gtk_widget_set_mapped(widget, FALSE);

    gtk_container_forall(GTK_CONTAINER(widget),
                         xfdesktop_widget_unrealize,
                         NULL);

    g_signal_handlers_disconnect_by_func(G_OBJECT(desktop->priv->gscreen),
                                         G_CALLBACK(screen_composited_changed_cb), desktop);

    display = gdk_screen_get_display(desktop->priv->gscreen);
    xfw_windowing_error_trap_push(display);

    groot = gdk_screen_get_root_window(desktop->priv->gscreen);
    gdk_property_delete(groot, gdk_atom_intern("XFCE_DESKTOP_WINDOW", FALSE));
    gdk_property_delete(groot, gdk_atom_intern("NAUTILUS_DESKTOP_WINDOW_ID", FALSE));

#ifndef DISABLE_FOR_BUG7442
    gdk_property_delete(groot, gdk_atom_intern("_XROOTPMAP_ID", FALSE));
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gdk_window_set_background_pattern(groot, NULL);
G_GNUC_END_IGNORE_DEPRECATIONS
#endif

#ifdef ENABLE_X11
    if (xfw_windowing_get() == XFW_WINDOWING_X11) {
        xfdesktop_x11_set_root_image_file_property(desktop->priv->gscreen, desktop->priv->monitor, NULL);
     }
#endif

    gdk_display_flush(display);
    xfw_windowing_error_trap_pop_ignored(display);

    if(desktop->priv->bg_surface) {
        cairo_surface_destroy(desktop->priv->bg_surface);
        desktop->priv->bg_surface = NULL;
    }

    g_object_unref(G_OBJECT(gtk_widget_get_window(widget)));
    gtk_widget_set_window(widget, NULL);

    gtk_selection_remove_all(widget);

    gtk_widget_set_realized(widget, FALSE);
}

static gboolean
xfce_desktop_draw(GtkWidget *w,
                  cairo_t *cr)
{
    XfceDesktop *desktop = XFCE_DESKTOP(w);

    cairo_save(cr);

    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);

    if (desktop->priv->bg_surface != NULL) {
        gint scale_factor = gtk_widget_get_scale_factor(w);
        cairo_scale(cr, 1.0 / scale_factor, 1.0 / scale_factor);
        cairo_set_source_surface(cr,
                                 desktop->priv->bg_surface,
                                 0 - desktop->priv->bg_surface_region.x,
                                 0 - desktop->priv->bg_surface_region.y);
        cairo_rectangle(cr,
                        0,
                        0,
                        desktop->priv->bg_surface_region.width,
                        desktop->priv->bg_surface_region.height);
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
    desktop->priv->has_pointer = event->type == GDK_ENTER_NOTIFY;
    return FALSE;
}

#ifdef ENABLE_DESKTOP_ICONS
static gboolean
style_refresh_cb(gpointer user_data)
{
    XfceDesktop *desktop = user_data;
    GList *children;

    TRACE("entering");

    desktop->priv->style_refresh_timer = 0;

    g_return_val_if_fail(XFCE_IS_DESKTOP(desktop), FALSE);

    if(!gtk_widget_get_realized(GTK_WIDGET(desktop)))
        return FALSE;

    if(desktop->priv->workspaces == NULL)
        return FALSE;

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

    if(desktop->priv->style_refresh_timer != 0)
        g_source_remove(desktop->priv->style_refresh_timer);

    desktop->priv->style_refresh_timer = g_idle_add_full(G_PRIORITY_LOW,
                                                         style_refresh_cb,
                                                         desktop,
                                                         NULL);
#endif

    GTK_WIDGET_CLASS(xfce_desktop_parent_class)->style_updated(w);
}

static gboolean
xfce_desktop_get_single_workspace_mode(XfceDesktop *desktop)
{
    g_return_val_if_fail(XFCE_IS_DESKTOP(desktop), TRUE);
    return desktop->priv->single_workspace_mode;
}

static XfwWorkspace *
xfce_desktop_get_current_workspace(XfceDesktop *desktop)
{
    g_return_val_if_fail(XFCE_IS_DESKTOP(desktop), NULL);

    /* If we're in single_workspace mode we need to return the workspace that
     * it was set to, if possible, otherwise return the current workspace, if
     * we have one, or just the lowest-numbered workspace */
    if (xfce_desktop_get_single_workspace_mode(desktop) && desktop->priv->single_workspace != NULL) {
        DBG("returning single_workspace");
        return desktop->priv->single_workspace;
    } else if (desktop->priv->active_workspace != NULL) {
        DBG("returning current_workspace");
        return desktop->priv->active_workspace;
    } else {
        XfwWorkspace *lowest_workspace = NULL;

        for (GList *l = desktop->priv->workspaces; l != NULL; l = l->next) {
            XfwWorkspace *workspace = XFW_WORKSPACE(l->data);
            if (lowest_workspace == NULL ||
                xfw_workspace_get_number(workspace) < xfw_workspace_get_number(lowest_workspace))
            {
                lowest_workspace = workspace;
            }
        }
        DBG("returning lowest_workspace");
        return lowest_workspace;
    }
}

/* public api */

/**
 * xfce_desktop_new:
 * @gscreen: The current #GdkScreen.
 * @monitor: #GdkMonitor to display the widget on.
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
                 GdkMonitor *monitor,
                 XfconfChannel *channel,
                 const gchar *property_prefix,
                 XfdesktopBackdropManager *backdrop_manager)
{
    g_return_val_if_fail(GDK_IS_SCREEN(gscreen), NULL);
    g_return_val_if_fail(GDK_IS_MONITOR(monitor), NULL);
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

GdkMonitor *
xfce_desktop_get_monitor(XfceDesktop *desktop) {
    g_return_val_if_fail(XFCE_IS_DESKTOP(desktop), NULL);
    return desktop->priv->monitor;
}

void
xfce_desktop_update_monitor(XfceDesktop *desktop, GdkMonitor *monitor) {
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));
    g_return_if_fail(GDK_IS_MONITOR(monitor));

    if (desktop->priv->monitor != monitor) {
        if (desktop->priv->monitor != NULL) {
            g_signal_handlers_disconnect_by_data(desktop->priv->monitor, desktop);
        }
        g_signal_connect(monitor, "notify::logical-geometry",
                         G_CALLBACK(monitor_prop_changed), desktop);
        g_signal_connect(monitor, "notify::scale",
                         G_CALLBACK(monitor_prop_changed), desktop);
    }

    desktop->priv->monitor = monitor;
    if (gtk_widget_get_realized(GTK_WIDGET(desktop))) {
        xfce_desktop_place_on_monitor(desktop);
        fetch_backdrop(desktop);
    }
}

static void
xfce_desktop_set_single_workspace_mode(XfceDesktop *desktop,
                                       gboolean single_workspace)
{
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));

    if(single_workspace == desktop->priv->single_workspace_mode)
        return;

    desktop->priv->single_workspace_mode = single_workspace;

    XF_DEBUG("single_workspace_mode now %s", single_workspace ? "TRUE" : "FALSE");

    /* If the desktop has been realized then fake a screen size change to
     * update the backdrop. There's no reason to if there's no desktop yet */
    if (gtk_widget_get_realized(GTK_WIDGET(desktop))) {
        fetch_backdrop(desktop);
    }
}

static void
xfce_desktop_set_single_workspace_number(XfceDesktop *desktop,
                                         gint workspace_num)
{
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));

    if (workspace_num < 0 || (workspace_num == desktop->priv->single_workspace_num &&
                              desktop->priv->single_workspace != NULL))
    {
        return;
    }

    if (workspace_num != desktop->priv->single_workspace_num) {
        XF_DEBUG("single_workspace_num now %d", workspace_num);
    }

    desktop->priv->single_workspace_num = workspace_num;

    desktop->priv->single_workspace = NULL;
    for (GList *l = desktop->priv->workspaces; l != NULL; l = l->next) {
        XfwWorkspace *workspace = XFW_WORKSPACE(l->data);
        if ((gint)xfw_workspace_get_number(workspace) == workspace_num) {
            desktop->priv->single_workspace = workspace;
            break;
        }
    }

    if (desktop->priv->single_workspace_mode &&
        desktop->priv->single_workspace != NULL &&
        gtk_widget_get_realized(GTK_WIDGET(desktop)))
    {
        fetch_backdrop(desktop);
    }
}

void
xfce_desktop_freeze_updates(XfceDesktop *desktop)
{
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));
    desktop->priv->updates_frozen = TRUE;
}

void
xfce_desktop_thaw_updates(XfceDesktop *desktop)
{
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));

    desktop->priv->updates_frozen = FALSE;
    if (gtk_widget_get_realized(GTK_WIDGET(desktop))) {
        fetch_backdrop(desktop);
    }
}

gboolean
xfce_desktop_has_pointer(XfceDesktop *desktop) {
    g_return_val_if_fail(XFCE_IS_DESKTOP(desktop), FALSE);
    return desktop->priv->has_pointer;
}

void
xfce_desktop_refresh(XfceDesktop *desktop, gboolean advance_wallpaper) {
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));
    if (advance_wallpaper) {
        XfwWorkspace *current_workspace = xfce_desktop_get_current_workspace(desktop);

        // Block because we're going to unconditionally request the new backdrop below
        g_signal_handlers_block_by_func(desktop->priv->backdrop_manager, manager_backdrop_changed, desktop);
        xfdesktop_backdrop_manager_cycle_backdrop(desktop->priv->backdrop_manager,
                                                  desktop->priv->monitor,
                                                  current_workspace);
        g_signal_handlers_unblock_by_func(desktop->priv->backdrop_manager, manager_backdrop_changed, desktop);
    }

    fetch_backdrop(desktop);
}
