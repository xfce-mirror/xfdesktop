/*
 *  xfdesktop - xfce4's desktop manager
 *
 *  Copyright (c) 2024 Brian Tarricone, <brian@tarricone.org>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4windowing/libxfce4windowing.h>

#ifdef ENABLE_WAYLAND
#include <gtk-layer-shell.h>
#endif

#include "xfce-desktop.h"
#include "xfdesktop-icon-view-holder.h"
#include "xfdesktop-icon-view.h"

struct _XfdesktopIconViewHolder {
    GObject praent;

    XfwScreen *screen;
    XfceDesktop *desktop;
    XfdesktopIconView *icon_view;

    GtkWidget *container;
};

static void xfdesktop_icon_view_holder_finalize(GObject *object);


G_DEFINE_TYPE(XfdesktopIconViewHolder, xfdesktop_icon_view_holder, G_TYPE_OBJECT)


static void
xfdesktop_icon_view_holder_class_init(XfdesktopIconViewHolderClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = xfdesktop_icon_view_holder_finalize;
}

static void
xfdesktop_icon_view_holder_init(XfdesktopIconViewHolder *holder) {}

static void
xfdesktop_icon_view_holder_finalize(GObject *object) {
    XfdesktopIconViewHolder *holder = XFDESKTOP_ICON_VIEW_HOLDER(object);

#ifdef ENABLE_X11
    if (xfw_windowing_get() == XFW_WINDOWING_X11) {
        g_signal_handlers_disconnect_by_data(holder->screen, holder);
        XfwMonitor *monitor = xfce_desktop_get_monitor(holder->desktop);
        g_signal_handlers_disconnect_by_data(monitor, holder);
    }
#endif

    gtk_widget_destroy(holder->container);

    G_OBJECT_CLASS(xfdesktop_icon_view_holder_parent_class)->finalize(object);
}

#ifdef ENABLE_X11

static void
update_icon_view_geometry(XfdesktopIconViewHolder *holder) {
    XfwMonitor *monitor = xfce_desktop_get_monitor(holder->desktop);
    GdkRectangle new_workarea = { 0, };
    xfw_monitor_get_workarea(monitor, &new_workarea);

    DBG("new monitor %s workarea: %dx%d+%d+%d",
        xfw_monitor_get_connector(monitor),
        new_workarea.width, new_workarea.height,
        new_workarea.x, new_workarea.y);

    if (new_workarea.width > 0 && new_workarea.height > 0) {
        gtk_widget_set_size_request(GTK_WIDGET(holder->icon_view), new_workarea.width, new_workarea.height);
        gtk_fixed_move(GTK_FIXED(holder->container), GTK_WIDGET(holder->icon_view), new_workarea.x, new_workarea.y);
    }
}

static void
init_for_x11(XfdesktopIconViewHolder *holder) {
    holder->container = gtk_fixed_new();
    gtk_container_add(GTK_CONTAINER(holder->desktop), holder->container);
    gtk_widget_show(holder->container);

    gtk_container_add(GTK_CONTAINER(holder->container), GTK_WIDGET(holder->icon_view));
    update_icon_view_geometry(holder);
    gtk_widget_show(GTK_WIDGET(holder->icon_view));

    g_signal_connect_object(holder->screen, "monitors-changed",
                            G_CALLBACK(update_icon_view_geometry), holder,
                            G_CONNECT_AFTER | G_CONNECT_SWAPPED);

    XfwMonitor *monitor = xfce_desktop_get_monitor(holder->desktop);
    g_signal_connect_object(monitor, "notify::workarea",
                            G_CALLBACK(update_icon_view_geometry), holder,
                            G_CONNECT_AFTER | G_CONNECT_SWAPPED);
}

#endif  /* ENABLE_X11 */

#ifdef ENABLE_WAYLAND
static void
init_for_wayland(XfdesktopIconViewHolder *holder) {
    holder->container = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_app_paintable(holder->container, TRUE);
    gtk_layer_init_for_window(GTK_WINDOW(holder->container));
    gtk_layer_set_layer(GTK_WINDOW(holder->container), GTK_LAYER_SHELL_LAYER_BOTTOM);
    XfwMonitor *monitor = xfce_desktop_get_monitor(holder->desktop);
    gtk_layer_set_monitor(GTK_WINDOW(holder->container), xfw_monitor_get_gdk_monitor(monitor));
    gtk_layer_set_anchor(GTK_WINDOW(holder->container), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(holder->container), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(holder->container), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(holder->container), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(holder->container), GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
    gtk_layer_set_namespace(GTK_WINDOW(holder->container), "desktop-icons");

    gtk_container_add(GTK_CONTAINER(holder->container), GTK_WIDGET(holder->icon_view));
    gtk_widget_show(GTK_WIDGET(holder->icon_view));

    gtk_window_present(GTK_WINDOW(holder->container));
}
#endif  /* ENABLE_WAYLAND */

XfdesktopIconViewHolder *
xfdesktop_icon_view_holder_new(XfwScreen *screen, XfceDesktop *desktop, XfdesktopIconView *icon_view) {
    g_return_val_if_fail(XFW_IS_SCREEN(screen), NULL);
    g_return_val_if_fail(XFCE_IS_DESKTOP(desktop), NULL);
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW(icon_view), NULL);

    XfdesktopIconViewHolder *holder = g_object_new(XFDESKTOP_TYPE_ICON_VIEW_HOLDER, NULL);
    holder->screen = screen;
    holder->desktop = desktop;
    holder->icon_view = icon_view;

    if (xfw_windowing_get() == XFW_WINDOWING_X11) {
#ifdef ENABLE_X11
        init_for_x11(holder);
#else
        g_error("X11 unsupported");
#endif
    } else if (xfw_windowing_get() == XFW_WINDOWING_WAYLAND) {
#ifdef ENABLE_WAYLAND
        init_for_wayland(holder);
#else
        g_error("Wayland unsupported");
#endif
    } else {
        g_error("Unknown or unsupported windowing system");
    }

    return holder;
}

XfceDesktop *
xfdesktop_icon_view_holder_get_desktop(XfdesktopIconViewHolder *holder) {
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW_HOLDER(holder), NULL);
    return holder->desktop;
}

XfdesktopIconView *
xfdesktop_icon_view_holder_get_icon_view(XfdesktopIconViewHolder *holder) {
    g_return_val_if_fail(XFDESKTOP_IS_ICON_VIEW_HOLDER(holder), NULL);
    return holder->icon_view;
}
