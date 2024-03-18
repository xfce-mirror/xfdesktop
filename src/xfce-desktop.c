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
#include "xfdesktop-common.h"
#include "xfce-desktop.h"
#include "xfce-desktop-enum-types.h"
#include "xfce-workspace.h"

/* disable setting the x background for bug 7442 */
//#define DISABLE_FOR_BUG7442

typedef GtkMenu *(*PopulateMenuFunc)(GtkMenu *, gint);

struct _XfceDesktopPrivate
{
    GdkScreen *gscreen;
    XfwScreen *xfw_screen;
    XfwWorkspaceManager *workspace_manager;
    gboolean updates_frozen;

    XfconfChannel *channel;
    gchar *property_prefix;

    cairo_surface_t *bg_surface;

    GHashTable *workspaces;  // XfwWorkspace -> XfceWorkspace
    XfceWorkspace *active_workspace;

    gboolean single_workspace_mode;
    gint single_workspace_num;
    XfceWorkspace *single_workspace;

    SessionLogoutFunc session_logout_func;

    guint32 grab_time;

    GtkMenu *active_root_menu;

#ifdef ENABLE_DESKTOP_ICONS
    XfceDesktopIconStyle icons_style;
    XfdesktopIconViewManager *icon_view_manager;
    gint style_refresh_timer;
#endif

    gchar *last_filename;
};

enum
{
    PROP_0 = 0,
    PROP_SCREEN,
    PROP_CHANNEL,
    PROP_PROPERTY_PREFIX,
#ifdef ENABLE_DESKTOP_ICONS
    PROP_ICON_STYLE,
#endif
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
static gboolean xfce_desktop_button_press_event(GtkWidget *widget,
                                                GdkEventButton *evt);
static gboolean xfce_desktop_button_release_event(GtkWidget *widget,
                                                  GdkEventButton *evt);
static gboolean xfce_desktop_popup_menu(GtkWidget *widget);

static gboolean xfce_desktop_draw(GtkWidget *w,
                                  cairo_t *cr);
static gboolean xfce_desktop_delete_event(GtkWidget *w,
                                          GdkEventAny *evt);
static void xfce_desktop_style_updated(GtkWidget *w);

static void xfce_desktop_set_single_workspace_mode(XfceDesktop *desktop,
                                                   gboolean single_workspace);
static void xfce_desktop_set_single_workspace_number(XfceDesktop *desktop,
                                                     gint workspace_num);

static gboolean xfce_desktop_get_single_workspace_mode(XfceDesktop *desktop);
static XfceWorkspace *xfce_desktop_get_current_workspace(XfceDesktop *desktop);


static struct
{
    const gchar *setting;
    GType setting_type;
    const gchar *property;
} setting_bindings[] = {
    { SINGLE_WORKSPACE_MODE, G_TYPE_BOOLEAN, "single-workspace-mode" },
    { SINGLE_WORKSPACE_NUMBER, G_TYPE_INT, "single-workspace-number" },
#ifdef ENABLE_DESKTOP_ICONS
    { DESKTOP_ICONS_STYLE_PROP, 0 /* to be filled in later */, "icon-style" },
#endif
};

static void
xfce_desktop_settings_bindings_init(void)
{
    // Required because XFCE_TYPE_DESKTOP_ICON_STYLE is not a compile-time
    // constant, and therefore cannot be stored as static data.

#ifdef ENABLE_DESKTOP_ICONS
    for (gsize i = 0; i < G_N_ELEMENTS(setting_bindings); ++i) {
        if (g_strcmp0(setting_bindings[i].setting, DESKTOP_ICONS_STYLE_PROP) == 0) {
            setting_bindings[i].setting_type = XFCE_TYPE_DESKTOP_ICON_STYLE;
        }
    }
#endif
}


/* private functions */

#ifdef ENABLE_X11
static void
set_imgfile_root_property(XfceDesktop *desktop, const gchar *filename,
                          gint monitor)
{
    if (xfw_windowing_get() == XFW_WINDOWING_X11) {
        GdkDisplay *display;
        gchar property_name[128];

        display = gdk_screen_get_display(desktop->priv->gscreen);
        xfw_windowing_error_trap_push(display);

        g_snprintf(property_name, 128, XFDESKTOP_IMAGE_FILE_FMT, monitor);
        if(filename) {
            gdk_property_change(gdk_screen_get_root_window(desktop->priv->gscreen),
                                gdk_atom_intern(property_name, FALSE),
                                gdk_x11_xatom_to_atom(XA_STRING), 8,
                                GDK_PROP_MODE_REPLACE,
                                (guchar *)filename, strlen(filename)+1);
        } else {
            gdk_property_delete(gdk_screen_get_root_window(desktop->priv->gscreen),
                                gdk_atom_intern(property_name, FALSE));
        }

        xfw_windowing_error_trap_pop_ignored(display);
    }
}

static void
set_real_root_window_surface(GdkScreen *gscreen,
                             cairo_surface_t *surface)
{
#ifndef DISABLE_FOR_BUG7442
    Pixmap pixmap_id;
    GdkDisplay *display;
    GdkWindow *groot;
    cairo_pattern_t *pattern;

    groot = gdk_screen_get_root_window(gscreen);
    pixmap_id = cairo_xlib_surface_get_drawable (surface);

    display = gdk_screen_get_display(gscreen);
    xfw_windowing_error_trap_push(display);

    /* set root property for transparent Eterms */
    gdk_property_change(groot,
            gdk_atom_intern("_XROOTPMAP_ID", FALSE),
            gdk_atom_intern("PIXMAP", FALSE), 32,
            GDK_PROP_MODE_REPLACE, (guchar *)&pixmap_id, 1);
    /* and set the root window's BG surface, because aterm is somewhat lame. */
    pattern = cairo_pattern_create_for_surface(surface);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gdk_window_set_background_pattern(groot, pattern);
G_GNUC_END_IGNORE_DEPRECATIONS
    cairo_pattern_destroy(pattern);
    /* there really should be a standard for this crap... */

    xfw_windowing_error_trap_pop_ignored(display);
#endif
}
#endif  /* ENABLE_X11 */

static cairo_surface_t *
create_bg_surface(GdkScreen *gscreen, gpointer user_data)
{
    XfceDesktop *desktop = user_data;
    gint w, h;

    TRACE("entering");

    g_return_val_if_fail(XFCE_IS_DESKTOP(desktop), NULL);

    xfdesktop_get_screen_dimensions (gscreen, &w, &h);
    gtk_widget_set_size_request(GTK_WIDGET(desktop), w, h);
    gtk_window_resize(GTK_WINDOW(desktop), w, h);

    if(desktop->priv->bg_surface)
        cairo_surface_destroy(desktop->priv->bg_surface);
    desktop->priv->bg_surface = gdk_window_create_similar_surface(
                                    gtk_widget_get_window(GTK_WIDGET(desktop)),
                                                          CAIRO_CONTENT_COLOR_ALPHA, w, h);

    return desktop->priv->bg_surface;
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
backdrop_changed_cb(XfceBackdrop *backdrop, gpointer user_data)
{
    XfceDesktop *desktop = XFCE_DESKTOP(user_data);
    cairo_surface_t *surface = desktop->priv->bg_surface;
    cairo_surface_t *pix_surface;
    GdkScreen *gscreen = desktop->priv->gscreen;
    GdkDisplay *display;
    gchar *new_filename = NULL;
    GdkRectangle rect;
    cairo_region_t *clip_region = NULL;
    XfceWorkspace *current_workspace;
    gint i, monitor = -1;
    gint scale_factor;

    TRACE("entering");

    g_return_if_fail(XFCE_IS_DESKTOP(desktop));

    if(!XFCE_IS_BACKDROP(backdrop))
        return;

    if(desktop->priv->updates_frozen || !gtk_widget_get_realized(GTK_WIDGET(desktop)))
        return;

    TRACE("really entering");

    display = gdk_display_get_default();
    current_workspace = xfce_desktop_get_current_workspace(desktop);
    if (current_workspace == NULL) {
        return;
    }

    /* Find out which monitor the backdrop is on */
    for(i = 0; i < xfce_desktop_get_n_monitors(desktop); i++) {
        if(backdrop == xfce_workspace_get_backdrop(current_workspace, i)) {
            monitor = i;
            break;
        }
    }
    if(monitor == -1)
        return;
    /* notify Accountsservice of the new bg (only for monitor0) */
    if(monitor == 0)
    {
        if (xfce_desktop_get_current_workspace(desktop) == 0)
        {
            new_filename = g_strdup(xfce_backdrop_get_image_filename(backdrop));
            if (g_strcmp0(desktop->priv->last_filename, new_filename) != 0)
            {
                desktop->priv->last_filename = g_strdup(new_filename);
                set_accountsservice_user_bg(xfce_backdrop_get_image_filename(backdrop));
            }
            g_free(new_filename);
        }
    }

#ifdef G_ENABLE_DEBUG
    XF_DEBUG("backdrop changed for workspace %d, monitor %d (%s)", current_workspace, monitor,
             gdk_monitor_get_model(gdk_display_get_monitor(display, monitor)));
#endif

    if(xfce_desktop_get_n_monitors(desktop) > 1 && xfce_workspace_get_xinerama_stretch(current_workspace)) {
        /* Spanning screens */
        GdkRectangle monitor_rect;

        gdk_monitor_get_geometry(gdk_display_get_monitor(display, 0),
                                 &rect);
        /* Get the lowest x and y value for all the monitors in
         * case none of them start at 0,0 for whatever reason.
         */
        for(i = 1; i < xfce_desktop_get_n_monitors(desktop); i++) {
            gdk_monitor_get_geometry(gdk_display_get_monitor(display, i),
                                     &monitor_rect);

            if(monitor_rect.x < rect.x)
                rect.x = monitor_rect.x;
            if(monitor_rect.y < rect.y)
                rect.y = monitor_rect.y;
        }

        xfdesktop_get_screen_dimensions (gscreen, &rect.width, &rect.height);
        XF_DEBUG("xinerama_stretch x %d, y %d, width %d, height %d",
                 rect.x, rect.y, rect.width, rect.height);
    } else {
        gdk_monitor_get_geometry(gdk_display_get_monitor(display, monitor),
                                 &rect);
        XF_DEBUG("monitor x %d, y %d, width %d, height %d",
                 rect.x, rect.y, rect.width, rect.height);
    }

    scale_factor = gtk_widget_get_scale_factor(GTK_WIDGET(desktop));
    xfce_backdrop_set_size(backdrop, rect.width * scale_factor, rect.height * scale_factor);

    if(monitor > 0 && !xfce_workspace_get_xinerama_stretch(current_workspace)) {
        clip_region = cairo_region_create_rectangle(&rect);

        XF_DEBUG("clip_region: x: %d, y: %d, w: %d, h: %d",
                 rect.x, rect.y, rect.width, rect.height);

        /* If we are not monitor 0 on a multi-monitor setup we need to subtract
         * all the previous monitor regions so we don't draw over them. This
         * should prevent the overlap and double backdrop drawing bugs.
         */
        for(i = 0; i < monitor; i++) {
            GdkRectangle previous_monitor;
            cairo_region_t *previous_region;

            gdk_monitor_get_geometry(gdk_display_get_monitor(display, i),
                                     &previous_monitor);

            XF_DEBUG("previous_monitor: x: %d, y: %d, w: %d, h: %d",
                     previous_monitor.x, previous_monitor.y,
                     previous_monitor.width, previous_monitor.height);

            previous_region = cairo_region_create_rectangle(&previous_monitor);

            cairo_region_subtract(clip_region, previous_region);

            cairo_region_destroy(previous_region);
        }
    }

    if(clip_region != NULL) {
        /* Update the area to redraw to limit the icons/area painted */
        cairo_region_get_extents(clip_region, &rect);
        XF_DEBUG("area to update: x: %d, y: %d, w: %d, h: %d",
                 rect.x, rect.y, rect.width, rect.height);
    }

    if(rect.width != 0 && rect.height != 0) {
        /* get the composited backdrop pixbuf */
        GdkPixbuf *pix = xfce_backdrop_get_pixbuf(backdrop);
        cairo_t *cr;

        /* create the backdrop if needed */
        if(!pix) {
            xfce_backdrop_generate_async(backdrop);

            if(clip_region != NULL)
                cairo_region_destroy(clip_region);

            return;
        }

        /* Create the background surface if it isn't already */
        if(!desktop->priv->bg_surface) {
            surface = create_bg_surface(gscreen, desktop);

            if(!surface) {
                g_object_unref(pix);

                if(clip_region != NULL)
                    cairo_region_destroy(clip_region);

                return;
            }
        }

        cr = cairo_create(surface);
        pix_surface = gdk_cairo_surface_create_from_pixbuf(pix,
                                                           scale_factor,
                                                           gtk_widget_get_window(GTK_WIDGET(desktop)));
        cairo_set_source_surface(cr, pix_surface, rect.x, rect.y);
        cairo_surface_destroy(pix_surface);

        /* clip the area so we don't draw over a previous wallpaper */
        if(clip_region != NULL) {
            gdk_cairo_region(cr, clip_region);
            cairo_clip(cr);
        }

        cairo_paint(cr);

        /* tell gtk to redraw the repainted area */
        gtk_widget_queue_draw_area(GTK_WIDGET(desktop), rect.x, rect.y,
                                   rect.width, rect.height);

#ifdef ENABLE_X11
        set_imgfile_root_property(desktop,
                                  xfce_backdrop_get_image_filename(backdrop),
                                  monitor);

        /* do this again so apps watching the root win notice the update */
        set_real_root_window_surface(gscreen, surface);
#endif  /* ENABLE_X11 */

        g_object_unref(G_OBJECT(pix));
        cairo_destroy(cr);
        gtk_widget_show(GTK_WIDGET(desktop));
    }

    if(clip_region != NULL)
        cairo_region_destroy(clip_region);
}

static void
screen_size_changed_cb(GdkScreen *gscreen, gpointer user_data)
{
    XfceDesktop *desktop = user_data;
    XfceWorkspace *current_workspace;

    TRACE("entering");

    current_workspace = xfce_desktop_get_current_workspace(desktop);
    if (current_workspace == NULL) {
        return;
    }

    /* release the bg_surface since the dimensions may have changed */
    if(desktop->priv->bg_surface) {
        cairo_surface_destroy(desktop->priv->bg_surface);
        desktop->priv->bg_surface = NULL;
    }

    /* special case for 1 backdrop to handle xinerama stretching */
    if(xfce_workspace_get_xinerama_stretch(current_workspace)) {
       backdrop_changed_cb(xfce_workspace_get_backdrop(current_workspace, 0), desktop);
    } else {
        gint i;

        for(i = 0; i < xfce_desktop_get_n_monitors(desktop); i++) {
            XfceBackdrop *current_backdrop;
            current_backdrop = xfce_workspace_get_backdrop(current_workspace, i);
            backdrop_changed_cb(current_backdrop, desktop);
        }
    }
}

static void
screen_composited_changed_cb(GdkScreen *gscreen,
                             XfceDesktop *desktop)
{
    TRACE("entering");

    if (gtk_widget_get_realized(GTK_WIDGET(desktop))) {
        /* fake a screen size changed, so the background is properly set */
        screen_size_changed_cb(gscreen, desktop);
    }
}

static void
xfce_desktop_monitors_changed(GdkScreen *gscreen,
                              gpointer user_data)
{
    TRACE("entering");

    if (gdk_display_get_n_monitors(gdk_screen_get_display(gscreen)) > 0) {
        XfceDesktop *desktop = XFCE_DESKTOP(user_data);
        XfceWorkspace *workspace = NULL;
        GHashTableIter iter;

        g_hash_table_iter_init(&iter, desktop->priv->workspaces);
        while (g_hash_table_iter_next(&iter, NULL, (gpointer)&workspace)) {
            xfce_workspace_monitors_changed(workspace, gscreen);
        }

        /* fake a screen size changed, so the background is properly set */
        screen_size_changed_cb(gscreen, user_data);
    }
}

static void
workspace_backdrop_changed_cb(XfceWorkspace *workspace,
                              XfceBackdrop  *backdrop,
                              gpointer user_data)
{
    XfceDesktop *desktop = XFCE_DESKTOP(user_data);
    XfceWorkspace *current_workspace;
    gint monitor = 0, i;

    TRACE("entering");

    g_return_if_fail(XFCE_IS_WORKSPACE(workspace) && XFCE_IS_BACKDROP(backdrop));

    if (!gtk_widget_get_realized(GTK_WIDGET(desktop))) {
        return;
    }

    current_workspace = xfce_desktop_get_current_workspace(desktop);
    DBG("workspace=%p, current_workspace=%p is_current=%d", workspace, current_workspace, current_workspace == workspace);

    if (current_workspace == workspace) {
        /* Find out which monitor the backdrop is on */
        for(i = 0; i < xfce_desktop_get_n_monitors(desktop); i++) {
            if(backdrop == xfce_workspace_get_backdrop(current_workspace, i)) {
                monitor = i;
                break;
            }
        }

        /* Update the backdrop!
         * In spanning mode, ignore updates to monitors other than the primary
         */
        if (!xfce_workspace_get_xinerama_stretch(workspace) || monitor == 0) {
            backdrop_changed_cb(backdrop, user_data);
        }
    }
}

static void
workspace_changed_cb(XfwWorkspaceGroup *group,
                     XfwWorkspace *previously_active_space,
                     gpointer user_data)
{
    XfceDesktop *desktop = XFCE_DESKTOP(user_data);
    XfwWorkspace *active_xfw_workspace;
    XfceWorkspace *active_workspace;
    XfceWorkspace *current_workspace;

    TRACE("entering");

    active_xfw_workspace = xfw_workspace_group_get_active_workspace(group);
    active_workspace = g_hash_table_lookup(desktop->priv->workspaces, active_xfw_workspace);
    if (active_workspace == NULL || active_workspace == desktop->priv->active_workspace) {
        return;
    }

    desktop->priv->active_workspace = active_workspace;
    current_workspace = xfce_desktop_get_current_workspace(desktop);

    XF_DEBUG("new_active_workspace %d, new_current_workspace %d",
             xfce_workspace_get_workspace_num(active_workspace),
             current_workspace != NULL ? xfce_workspace_get_workspace_num(current_workspace) : -1);

    if (current_workspace == active_workspace && gtk_widget_get_realized(GTK_WIDGET(desktop))) {
        /* When we're spanning screens we only care about the first monitor */
        guint end = xfce_workspace_get_xinerama_stretch(current_workspace) ? 1 : xfce_desktop_get_n_monitors(desktop);

        for (guint i = 0; i < end; i++) {
            XfceBackdrop *backdrop = xfce_workspace_get_backdrop(current_workspace, i);
            backdrop_changed_cb(backdrop, desktop);
        }
    }
}

static void
workspace_created_cb(XfwWorkspaceManager *manager,
                     XfwWorkspace *new_workspace,
                     gpointer user_data)
{
    XfceDesktop *desktop = XFCE_DESKTOP(user_data);

    TRACE("entering");

    XfceWorkspace *workspace = xfce_workspace_new(desktop->priv->gscreen,
                                                  desktop->priv->channel,
                                                  new_workspace,
                                                  desktop->priv->property_prefix,
                                                  xfw_workspace_get_number(new_workspace));
    g_hash_table_insert(desktop->priv->workspaces, g_object_ref(new_workspace), workspace);

    if (gtk_widget_get_realized(GTK_WIDGET(desktop))) {
        xfce_workspace_monitors_changed(workspace, desktop->priv->gscreen);
    }
    g_signal_connect(workspace, "workspace-backdrop-changed",
                     G_CALLBACK(workspace_backdrop_changed_cb), desktop);
}

static void
workspace_destroyed_cb(XfwWorkspaceManager *manager,
                       XfwWorkspace *old_xfw_workspace,
                       gpointer user_data)
{
    XfceDesktop *desktop = XFCE_DESKTOP(user_data);
    XfceWorkspace *workspace = NULL;

    TRACE("entering");

    g_return_if_fail(XFCE_IS_DESKTOP(desktop));

    workspace = g_hash_table_lookup(desktop->priv->workspaces, old_xfw_workspace);
    if (workspace != NULL) {
        g_signal_handlers_disconnect_by_data(workspace, desktop);
        g_hash_table_remove(desktop->priv->workspaces, old_xfw_workspace);

        if (desktop->priv->active_workspace == workspace) {
            desktop->priv->active_workspace = NULL;
        }
    }
}

static void
workspace_group_created_cb(XfwWorkspaceManager* manager,
                           XfwWorkspaceGroup *group,
                           gpointer user_data)
{
    XfceDesktop *desktop = XFCE_DESKTOP(user_data);

    TRACE("entering");

    g_signal_connect(group, "active-workspace-changed",
                     G_CALLBACK(workspace_changed_cb), desktop);
}

static void
workspace_group_destroyed_cb(XfwWorkspaceManager *manager,
                             XfwWorkspaceGroup *group,
                             gpointer user_data)
{
    XfceDesktop *desktop = XFCE_DESKTOP(user_data);

    TRACE("entering");

    g_signal_handlers_disconnect_by_data(group, desktop);
}


#ifdef ENABLE_X11
static void
screen_set_x11_selection(XfceDesktop *desktop)
{
    Window xwin;
    gint xscreen;
    gchar selection_name[100], common_selection_name[32];
    Atom selection_atom, common_selection_atom, manager_atom;

    xwin = GDK_WINDOW_XID(gtk_widget_get_window(GTK_WIDGET(desktop)));
    xscreen = gdk_x11_screen_get_screen_number(desktop->priv->gscreen);

    g_snprintf(selection_name, 100, XFDESKTOP_SELECTION_FMT, xscreen);
    selection_atom = XInternAtom(gdk_x11_get_default_xdisplay(), selection_name, False);
    manager_atom = XInternAtom(gdk_x11_get_default_xdisplay(), "MANAGER", False);

    g_snprintf(common_selection_name, 32, "_NET_DESKTOP_MANAGER_S%d", xscreen);
    common_selection_atom = XInternAtom(gdk_x11_get_default_xdisplay(), common_selection_name, False);

    /* the previous check in src/main.c occurs too early, so workaround by
     * adding this one. */
   if(XGetSelectionOwner(gdk_x11_get_default_xdisplay(), selection_atom) != None) {
       g_critical("%s: already running, quitting.", PACKAGE);
       exit(0);
   }

    /* Check that _NET_DESKTOP_MANAGER_S%d isn't set, as it means another
     * desktop manager is running, e.g. nautilus */
    if(XGetSelectionOwner (gdk_x11_get_default_xdisplay(), common_selection_atom) != None) {
        g_critical("%s: another desktop manager is running.", PACKAGE);
        exit(1);
    }

    XSelectInput(gdk_x11_get_default_xdisplay(), xwin, PropertyChangeMask | ButtonPressMask);
    XSetSelectionOwner(gdk_x11_get_default_xdisplay(), selection_atom, xwin, GDK_CURRENT_TIME);
    XSetSelectionOwner(gdk_x11_get_default_xdisplay(), common_selection_atom, xwin, GDK_CURRENT_TIME);

    /* Check to see if we managed to claim the selection. If not,
     * we treat it as if we got it then immediately lost it */
    if(XGetSelectionOwner(gdk_x11_get_default_xdisplay(), selection_atom) == xwin) {
        XClientMessageEvent xev;
        Window xroot = GDK_WINDOW_XID(gdk_screen_get_root_window(desktop->priv->gscreen));

        xev.type = ClientMessage;
        xev.window = xroot;
        xev.message_type = manager_atom;
        xev.format = 32;
        xev.data.l[0] = GDK_CURRENT_TIME;
        xev.data.l[1] = selection_atom;
        xev.data.l[2] = xwin;
        xev.data.l[3] = 0;    /* manager specific data */
        xev.data.l[4] = 0;    /* manager specific data */

        XSendEvent(gdk_x11_get_default_xdisplay(), xroot, False, StructureNotifyMask, (XEvent *)&xev);
    } else {
        g_error("%s: could not set selection ownership", PACKAGE);
        exit(1);
    }
}
#endif  /* ENABLE_X11 */



/* gobject-related functions */


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
    widget_class->button_press_event = xfce_desktop_button_press_event;
    widget_class->button_release_event = xfce_desktop_button_release_event;
    widget_class->draw = xfce_desktop_draw;
    widget_class->delete_event = xfce_desktop_delete_event;
    widget_class->popup_menu = xfce_desktop_popup_menu;
    widget_class->style_updated = xfce_desktop_style_updated;

#define XFDESKTOP_PARAM_FLAGS  (G_PARAM_READWRITE \
                                | G_PARAM_STATIC_NAME \
                                | G_PARAM_STATIC_NICK \
                                | G_PARAM_STATIC_BLURB)

    g_object_class_install_property(gobject_class, PROP_SCREEN,
                                    g_param_spec_object("screen",
                                                        "gdk screen",
                                                        "gdk screen",
                                                        GDK_TYPE_SCREEN,
                                                        XFDESKTOP_PARAM_FLAGS | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property(gobject_class, PROP_CHANNEL,
                                    g_param_spec_object("channel",
                                                        "xfconf channel",
                                                        "xfconf channel",
                                                        XFCONF_TYPE_CHANNEL,
                                                        XFDESKTOP_PARAM_FLAGS | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property(gobject_class, PROP_PROPERTY_PREFIX,
                                    g_param_spec_string("property-prefix",
                                                        "xfconf property prefix",
                                                        "xfconf property prefix",
                                                        "",
                                                        XFDESKTOP_PARAM_FLAGS | G_PARAM_CONSTRUCT_ONLY));

#ifdef ENABLE_DESKTOP_ICONS
    g_object_class_install_property(gobject_class, PROP_ICON_STYLE,
                                    g_param_spec_enum("icon-style",
                                                      "icon style",
                                                      "icon style",
                                                      XFCE_TYPE_DESKTOP_ICON_STYLE,
#ifdef ENABLE_FILE_ICONS
                                                      XFCE_DESKTOP_ICON_STYLE_FILES,
#else
                                                      XFCE_DESKTOP_ICON_STYLE_WINDOWS,
#endif /* ENABLE_FILE_ICONS */
                                                      XFDESKTOP_PARAM_FLAGS));
#endif /* ENABLE_DESKTOP_ICONS */

    g_object_class_install_property(gobject_class, PROP_SINGLE_WORKSPACE_MODE,
                                    g_param_spec_boolean("single-workspace-mode",
                                                         "single-workspace-mode",
                                                         "single-workspace-mode",
                                                         TRUE,
                                                         XFDESKTOP_PARAM_FLAGS));

    g_object_class_install_property(gobject_class, PROP_SINGLE_WORKSPACE_NUMBER,
                                    g_param_spec_int("single-workspace-number",
                                                     "single-workspace-number",
                                                     "single-workspace-number",
                                                     0, G_MAXINT16, 0,
                                                     XFDESKTOP_PARAM_FLAGS));

#undef XFDESKTOP_PARAM_FLAGS

    xfce_desktop_settings_bindings_init();
}

static void
xfce_desktop_init(XfceDesktop *desktop)
{
    desktop->priv = xfce_desktop_get_instance_private(desktop);

    desktop->priv->workspaces = g_hash_table_new_full(g_direct_hash, g_direct_equal, g_object_unref, g_object_unref);
    desktop->priv->last_filename = g_strdup("");
}

static void
xfce_desktop_constructed(GObject *obj)
{
    XfceDesktop *desktop = XFCE_DESKTOP(obj);
    XfwWorkspaceManager *workspace_manager;

    G_OBJECT_CLASS(xfce_desktop_parent_class)->constructed(obj);

    gtk_window_set_type_hint(GTK_WINDOW(desktop), GDK_WINDOW_TYPE_HINT_DESKTOP);
    /* Accept focus is needed for the menu pop up either by the menu key on
     * the keyboard or Shift+F10. */
    gtk_window_set_accept_focus(GTK_WINDOW(desktop), TRUE);
    /* Can focus is needed for the gtk_grab_add/remove commands */
    gtk_widget_set_can_focus(GTK_WIDGET(desktop), TRUE);
    gtk_window_set_resizable(GTK_WINDOW(desktop), FALSE);

    for (gsize i = 0; i < G_N_ELEMENTS(setting_bindings); ++i) {
        g_assert(setting_bindings[i].setting_type != 0);
        xfconf_g_property_bind(desktop->priv->channel,
                               setting_bindings[i].setting, setting_bindings[i].setting_type,
                               G_OBJECT(desktop), setting_bindings[i].property);
    }

    desktop->priv->xfw_screen = xfw_screen_get_default();
    workspace_manager = xfw_screen_get_workspace_manager(desktop->priv->xfw_screen);
    desktop->priv->workspace_manager = workspace_manager;

    /* watch for workspace changes */
    for (GList *gl = xfw_workspace_manager_list_workspace_groups(workspace_manager);
         gl != NULL;
         gl = gl->next)
    {
        XfwWorkspaceGroup *group = XFW_WORKSPACE_GROUP(gl->data);
        workspace_group_created_cb(workspace_manager, group, desktop);
    }
    g_signal_connect(workspace_manager, "workspace-group-created",
                     G_CALLBACK(workspace_group_created_cb), desktop);
    g_signal_connect(workspace_manager, "workspace-group-destroyed",
                     G_CALLBACK(workspace_group_destroyed_cb), desktop);

    for (GList *wl = xfw_workspace_manager_list_workspaces(workspace_manager);
         wl != NULL;
         wl = wl->next)
    {
        workspace_created_cb(workspace_manager, XFW_WORKSPACE(wl->data), desktop);
    }
    g_signal_connect(workspace_manager, "workspace-created",
                     G_CALLBACK(workspace_created_cb), desktop);
    g_signal_connect(workspace_manager, "workspace-destroyed",
                     G_CALLBACK(workspace_destroyed_cb), desktop);
}

static void
xfce_desktop_finalize(GObject *object)
{
    XfceDesktop *desktop = XFCE_DESKTOP(object);

    if (desktop->priv->active_root_menu != NULL) {
        gtk_menu_shell_deactivate(GTK_MENU_SHELL(desktop->priv->active_root_menu));
    }

    g_hash_table_destroy(desktop->priv->workspaces);
    g_object_unref(desktop->priv->xfw_screen);

    g_object_unref(G_OBJECT(desktop->priv->channel));
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

        case PROP_CHANNEL:
            desktop->priv->channel = g_value_dup_object(value);
            break;

        case PROP_PROPERTY_PREFIX:
            desktop->priv->property_prefix = g_value_dup_string(value);
            break;

#ifdef ENABLE_DESKTOP_ICONS
        case PROP_ICON_STYLE:
            DBG("about to set icon style: %d", g_value_get_enum(value));
            xfce_desktop_set_icon_style(desktop,
                                        g_value_get_enum(value));
            DBG("finished setting icon style");
            break;
#endif
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

        case PROP_CHANNEL:
            g_value_set_object(value, desktop->priv->channel);
            break;

        case PROP_PROPERTY_PREFIX:
            g_value_set_string(value, desktop->priv->property_prefix);
            break;

#ifdef ENABLE_DESKTOP_ICONS
        case PROP_ICON_STYLE:
            g_value_set_enum(value, desktop->priv->icons_style);
            break;
#endif
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
    GdkAtom atom;
    gint sw, sh;

    TRACE("entering");

    gtk_window_set_screen(GTK_WINDOW(desktop), desktop->priv->gscreen);
    xfdesktop_get_screen_dimensions (desktop->priv->gscreen, &sw, &sh);

    /* chain up */
    GTK_WIDGET_CLASS(xfce_desktop_parent_class)->realize(widget);

    gtk_window_set_title(GTK_WINDOW(desktop), _("Desktop"));
    gtk_window_set_decorated(GTK_WINDOW(desktop), FALSE);
    gtk_widget_set_size_request(GTK_WIDGET(desktop), sw, sh);
    gtk_window_move(GTK_WINDOW(desktop), 0, 0);

    atom = gdk_atom_intern("_NET_WM_WINDOW_TYPE_DESKTOP", FALSE);
    gdk_property_change(gtk_widget_get_window(GTK_WIDGET(desktop)),
            gdk_atom_intern("_NET_WM_WINDOW_TYPE", FALSE),
            gdk_atom_intern("ATOM", FALSE), 32,
            GDK_PROP_MODE_REPLACE, (guchar *)&atom, 1);

#ifdef ENABLE_X11
    if (xfw_windowing_get() == XFW_WINDOWING_X11) {
        Window xid = GDK_WINDOW_XID(gtk_widget_get_window(GTK_WIDGET(desktop)));
        GdkWindow *groot = gdk_screen_get_root_window(desktop->priv->gscreen);

        gdk_property_change(groot,
                gdk_atom_intern("XFCE_DESKTOP_WINDOW", FALSE),
                gdk_atom_intern("WINDOW", FALSE), 32,
                GDK_PROP_MODE_REPLACE, (guchar *)&xid, 1);

        gdk_property_change(groot,
                gdk_atom_intern("NAUTILUS_DESKTOP_WINDOW_ID", FALSE),
                gdk_atom_intern("WINDOW", FALSE), 32,
                GDK_PROP_MODE_REPLACE, (guchar *)&xid, 1);

        screen_set_x11_selection(desktop);
    }
#endif  /* ENABLE_X11 */

    /* watch for screen changes */
    g_signal_connect(G_OBJECT(desktop->priv->gscreen), "monitors-changed",
                     G_CALLBACK(xfce_desktop_monitors_changed), desktop);
    g_signal_connect(G_OBJECT(desktop->priv->gscreen), "size-changed",
            G_CALLBACK(screen_size_changed_cb), desktop);
    g_signal_connect(G_OBJECT(desktop->priv->gscreen), "composited-changed",
            G_CALLBACK(screen_composited_changed_cb), desktop);

    gtk_widget_add_events(GTK_WIDGET(desktop), GDK_EXPOSURE_MASK);

    xfce_desktop_refresh(desktop, FALSE, TRUE);

    TRACE("exiting");
}

static void
xfce_desktop_unrealize(GtkWidget *widget)
{
    XfceDesktop *desktop = XFCE_DESKTOP(widget);
    GdkDisplay  *display;
    GHashTableIter iter;
    XfceWorkspace *workspace;
    GdkWindow *groot;
    gchar property_name[128];

    g_return_if_fail(XFCE_IS_DESKTOP(desktop));

    g_signal_handlers_disconnect_by_func(G_OBJECT(desktop->priv->gscreen),
                                         G_CALLBACK(xfce_desktop_monitors_changed),
                                         desktop);

    if(gtk_widget_get_mapped(widget))
        gtk_widget_unmap(widget);
    gtk_widget_set_mapped(widget, FALSE);

    gtk_container_forall(GTK_CONTAINER(widget),
                         xfdesktop_widget_unrealize,
                         NULL);

    g_signal_handlers_disconnect_by_func(G_OBJECT(desktop->priv->gscreen),
            G_CALLBACK(screen_size_changed_cb), desktop);
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

    g_hash_table_iter_init(&iter, desktop->priv->workspaces);
    while (g_hash_table_iter_next(&iter, NULL, (gpointer)&workspace)) {
        g_snprintf(property_name, 128, XFDESKTOP_IMAGE_FILE_FMT, xfce_workspace_get_workspace_num(workspace));
        gdk_property_delete(groot, gdk_atom_intern(property_name, FALSE));
    }

    gdk_display_flush(display);
    xfw_windowing_error_trap_pop_ignored(display);

    if(desktop->priv->bg_surface) {
        cairo_surface_destroy(desktop->priv->bg_surface);
        desktop->priv->bg_surface = NULL;
    }

    gtk_window_set_icon(GTK_WINDOW(widget), NULL);

    g_object_unref(G_OBJECT(gtk_widget_get_window(widget)));
    gtk_widget_set_window(widget, NULL);

    gtk_selection_remove_all(widget);

    gtk_widget_set_realized(widget, FALSE);
}

static gboolean
xfce_desktop_button_press_event(GtkWidget *w,
                                GdkEventButton *evt)
{
    guint button = evt->button;
    guint state = evt->state;
    XfceDesktop *desktop = XFCE_DESKTOP(w);

    DBG("entering");

    g_return_val_if_fail(XFCE_IS_DESKTOP(w), FALSE);

    if(evt->type == GDK_BUTTON_PRESS) {
        if(button == 3 || (button == 1 && (state & GDK_SHIFT_MASK))) {
            /* no icons on the desktop, grab the focus and pop up the menu */
            if(!gtk_widget_has_grab(w))
                gtk_grab_add(w);

            xfce_desktop_popup_root_menu(desktop, button, evt->time);
            return TRUE;
        } else if(button == 2 || (button == 1 && (state & GDK_SHIFT_MASK)
                                  && (state & GDK_CONTROL_MASK)))
        {
            /* always grab the focus and pop up the menu */
            if(!gtk_widget_has_grab(w))
                gtk_grab_add(w);

            xfce_desktop_popup_secondary_root_menu(desktop, button, evt->time);
            return TRUE;
        }
    }

    return FALSE;
}

static gboolean
xfce_desktop_button_release_event(GtkWidget *w,
                                  GdkEventButton *evt)
{
    DBG("entering");

    gtk_grab_remove(w);

    return FALSE;
}

/* This function gets called when the user presses the menu key on the keyboard.
 * Or Shift+F10 or whatever key binding the user has chosen. */
static gboolean
xfce_desktop_popup_menu(GtkWidget *w)
{
    GdkEvent *evt;
    guint button, etime;

    DBG("entering");

    evt = gtk_get_current_event();
    if(evt != NULL && (GDK_BUTTON_PRESS == evt->type || GDK_BUTTON_RELEASE == evt->type)) {
        button = evt->button.button;
        etime = evt->button.time;
    } else {
        button = 0;
        etime = gtk_get_current_event_time();
    }

    xfce_desktop_popup_root_menu(XFCE_DESKTOP(w), button, etime);

    gdk_event_free((GdkEvent*)evt);
    return TRUE;
}

static gboolean
xfce_desktop_draw(GtkWidget *w,
                  cairo_t *cr)
{
    XfceDesktop *desktop = XFCE_DESKTOP(w);
    GList *children, *l;

    /*TRACE("entering");*/

    if (desktop->priv->bg_surface == NULL) {
        create_bg_surface(gtk_widget_get_screen(w), desktop);
    }

    cairo_save(cr);

    cairo_set_source_surface(cr, desktop->priv->bg_surface, 0, 0);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr);

    cairo_restore(cr);

    children = gtk_container_get_children(GTK_CONTAINER(w));
    for(l = children; l; l = l->next) {
        gtk_container_propagate_draw(GTK_CONTAINER(w),
                                     GTK_WIDGET(l->data),
                                     cr);
    }
    g_list_free(children);

    return FALSE;
}

static gboolean
xfce_desktop_delete_event(GtkWidget *w,
                          GdkEventAny *evt)
{
    if(XFCE_DESKTOP(w)->priv->session_logout_func)
        XFCE_DESKTOP(w)->priv->session_logout_func();

    return TRUE;
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

static XfceWorkspace *
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
        XfceWorkspace *lowest_workspace = NULL;
        XfceWorkspace *workspace = NULL;
        GHashTableIter iter;

        g_hash_table_iter_init(&iter, desktop->priv->workspaces);
        while (g_hash_table_iter_next(&iter, NULL, (gpointer)&workspace)) {
            if (lowest_workspace == NULL ||
                xfce_workspace_get_workspace_num(workspace) < xfce_workspace_get_workspace_num(lowest_workspace))
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
 * @channel: An #XfconfChannel to use for settings.
 * @property_prefix: String prefix for per-screen properties.
 *
 * Creates a new #XfceDesktop for the specified #GdkScreen.  Settings
 * will be fetched using @channel.  Per-screen/monitor settings will
 * have @property_prefix prepended to Xfconf property names.
 *
 * Return value: A new #XfceDesktop.
 **/
GtkWidget *
xfce_desktop_new(GdkScreen *gscreen,
                 XfconfChannel *channel,
                 const gchar *property_prefix)
{
    g_return_val_if_fail(GDK_IS_SCREEN(gscreen), NULL);
    g_return_val_if_fail(XFCONF_IS_CHANNEL(channel), NULL);
    g_return_val_if_fail(property_prefix != NULL, NULL);

    return g_object_new(XFCE_TYPE_DESKTOP,
                        "screen", gscreen,
                        "channel", channel,
                        "property-prefix", property_prefix,
                        NULL);
}

gint
xfce_desktop_get_n_monitors(XfceDesktop *desktop)
{
    g_return_val_if_fail(XFCE_IS_DESKTOP(desktop), 0);

    return gdk_display_get_n_monitors(gdk_screen_get_display(desktop->priv->gscreen));
}

void
xfce_desktop_set_icon_style(XfceDesktop *desktop,
                            XfceDesktopIconStyle style)
{
    g_return_if_fail(XFCE_IS_DESKTOP(desktop)
                     && style <= XFCE_DESKTOP_ICON_STYLE_FILES);

#ifdef ENABLE_DESKTOP_ICONS
    if(style == desktop->priv->icons_style)
        return;

    desktop->priv->icons_style = style;

    // FIXME: probably should ensure manager actually got freed and any icon view
    // instances are no longer present as children
    g_clear_object(&desktop->priv->icon_view_manager);

    switch (desktop->priv->icons_style) {
        case XFCE_DESKTOP_ICON_STYLE_NONE:
            /* nada */
            break;

        case XFCE_DESKTOP_ICON_STYLE_WINDOWS:
            desktop->priv->icon_view_manager = xfdesktop_window_icon_manager_new(desktop->priv->channel,
                                                                                 GTK_WIDGET(desktop));
            break;

#ifdef ENABLE_FILE_ICONS
        case XFCE_DESKTOP_ICON_STYLE_FILES:
            {
                GFile *file;
                const gchar *desktop_path;

                desktop_path = g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP);
                file = g_file_new_for_path(desktop_path);
                desktop->priv->icon_view_manager = xfdesktop_file_icon_manager_new(desktop->priv->channel,
                                                                                   GTK_WIDGET(desktop),
                                                                                   file);
                g_object_unref(file);
            }
            break;
#endif

        default:
            g_critical("Unusable XfceDesktopIconStyle: %d.  Unable to " \
                       "display desktop icons.",
                       desktop->priv->icons_style);
            break;
    }

    gtk_widget_queue_draw(GTK_WIDGET(desktop));
#endif
}

XfceDesktopIconStyle
xfce_desktop_get_icon_style(XfceDesktop *desktop)
{
    g_return_val_if_fail(XFCE_IS_DESKTOP(desktop), XFCE_DESKTOP_ICON_STYLE_NONE);

#ifdef ENABLE_DESKTOP_ICONS
    return desktop->priv->icons_style;
#else
    return XFCE_DESKTOP_ICON_STYLE_NONE;
#endif
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
    if(gtk_widget_get_realized(GTK_WIDGET(desktop)))
        screen_size_changed_cb(desktop->priv->gscreen, desktop);
}

static void
xfce_desktop_set_single_workspace_number(XfceDesktop *desktop,
                                         gint workspace_num)
{
    GHashTableIter iter;
    XfceWorkspace *workspace;

    g_return_if_fail(XFCE_IS_DESKTOP(desktop));

    if(workspace_num == desktop->priv->single_workspace_num)
        return;

    XF_DEBUG("single_workspace_num now %d", workspace_num);

    desktop->priv->single_workspace_num = workspace_num;

    g_hash_table_iter_init(&iter, desktop->priv->workspaces);
    while (g_hash_table_iter_next(&iter, NULL, (gpointer)&workspace)) {
        if (xfce_workspace_get_workspace_num(workspace) == workspace_num) {
            desktop->priv->single_workspace = workspace;
            break;
        }
    }

    if (xfce_desktop_get_single_workspace_mode(desktop) && gtk_widget_get_realized(GTK_WIDGET(desktop))) {
        /* Fake a screen size changed to update the backdrop */
        screen_size_changed_cb(desktop->priv->gscreen, desktop);
    }
}

void
xfce_desktop_set_session_logout_func(XfceDesktop *desktop,
                                     SessionLogoutFunc logout_func)
{
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));
    desktop->priv->session_logout_func = logout_func;
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
    if(gtk_widget_get_realized(GTK_WIDGET(desktop)))
        xfce_desktop_monitors_changed(desktop->priv->gscreen, desktop);
}

static gboolean
xfce_desktop_menu_destroy_idled(gpointer data)
{
    gtk_widget_destroy(GTK_WIDGET(data));
    return FALSE;
}

static void
xfce_desktop_menu_deactivated(GtkWidget *menu,
                              XfceDesktop *desktop)
{
    if (desktop->priv->active_root_menu == GTK_MENU(menu)) {
        desktop->priv->active_root_menu = NULL;
    }
    g_idle_add(xfce_desktop_menu_destroy_idled, menu);
}

static void
xfce_desktop_do_menu_popup(XfceDesktop *desktop,
                           guint button,
                           guint activate_time,
                           gboolean populate_from_icon_view,
                           PopulateMenuFunc populate_func)
{
    GdkScreen *screen;
    GtkMenu *menu = NULL;

    DBG("entering");

    if (desktop->priv->active_root_menu != NULL) {
        gtk_menu_shell_deactivate(GTK_MENU_SHELL(desktop->priv->active_root_menu));
        desktop->priv->active_root_menu = NULL;
    }

    if(gtk_widget_has_screen(GTK_WIDGET(desktop)))
        screen = gtk_widget_get_screen(GTK_WIDGET(desktop));
    else
        screen = gdk_display_get_default_screen(gdk_display_get_default());


#ifdef ENABLE_DESKTOP_ICONS
    if (populate_from_icon_view && desktop->priv->icon_view_manager != NULL) {
        menu = xfdesktop_icon_view_manager_get_context_menu(desktop->priv->icon_view_manager);
    }
#endif

    menu = (*populate_func)(menu, gtk_widget_get_scale_factor(GTK_WIDGET(desktop)));

    if (menu != NULL) {
        gtk_menu_set_screen(menu, screen);
        gtk_menu_attach_to_widget(menu, GTK_WIDGET(desktop), NULL);
        /* if the toplevel is the garcon menu, it loads items asynchronously; calling _show() forces
         * loading to complete.  otherwise, the _get_children() call would return NULL */
        gtk_widget_show(GTK_WIDGET(menu));
        g_signal_connect(menu, "deactivate",
                         G_CALLBACK(xfce_desktop_menu_deactivated), desktop);

        /* Per gtk_menu_popup's documentation "for conflict-resolve initiation of
         * concurrent requests for mouse/keyboard grab requests." */
        if(activate_time == 0)
            activate_time = gtk_get_current_event_time();

        desktop->priv->active_root_menu = menu;
        xfce_gtk_menu_popup_until_mapped(menu, NULL, NULL, NULL, NULL, button, activate_time);
    }
}


void
xfce_desktop_popup_root_menu(XfceDesktop *desktop,
                             guint button,
                             guint activate_time)
{
    DBG("entering");

    gchar *cmd = xfconf_channel_get_string(desktop->priv->channel , DESKTOP_MENU_COMMAND, NULL);

    if (cmd != NULL) {
        DBG("calling root menu command: %s", cmd);
        GAppInfo *appinfo = g_app_info_create_from_commandline(cmd,
                                                               NULL,
                                                               G_APP_INFO_CREATE_NONE,
                                                               NULL);
        g_free(cmd);
        g_return_if_fail(appinfo != NULL);
        g_app_info_launch(appinfo, NULL, NULL, NULL);
        g_object_unref(appinfo);
        return;
    } else {
        xfce_desktop_do_menu_popup(desktop, button, activate_time, TRUE, menu_populate);
    }
}

void
xfce_desktop_popup_secondary_root_menu(XfceDesktop *desktop,
                                       guint button,
                                       guint activate_time)
{
    DBG("entering");

    xfce_desktop_do_menu_popup(desktop, button, activate_time, FALSE, windowlist_populate);
}

void
xfce_desktop_refresh(XfceDesktop *desktop,
                     gboolean advance_wallpaper,
                     gboolean all_monitors)
{
    XfceWorkspace *current_workspace;
    gint current_monitor_num = -1;

    TRACE("entering");

    g_return_if_fail(XFCE_IS_DESKTOP(desktop));

    if(!gtk_widget_get_realized(GTK_WIDGET(desktop)))
        return;

    if(desktop->priv->workspaces == NULL) {
        return;
    }

    current_workspace = xfce_desktop_get_current_workspace(desktop);
    if (current_workspace == NULL) {
        return;
    }

    if(!all_monitors) {
        GdkDisplay *display = gdk_screen_get_display(desktop->priv->gscreen);
        current_monitor_num = xfdesktop_get_current_monitor_num(display);
    }

    /* reload backgrounds */
    for(gint i = 0; i < xfce_desktop_get_n_monitors(desktop); i++) {
        XfceBackdrop *backdrop;

        if(!all_monitors && current_monitor_num != i) {
            continue;
        }

        backdrop = xfce_workspace_get_backdrop(current_workspace, i);
        if (G_LIKELY(backdrop != NULL)) {
            if(advance_wallpaper) {
                /* We need to trigger a new wallpaper event */
                xfce_backdrop_force_cycle(backdrop);
            } else {
                /* Reinitialize wallpaper */
                xfce_backdrop_clear_cached_image(backdrop);
                /* Fake a changed event so we redraw the wallpaper */
                backdrop_changed_cb(backdrop, desktop);
            }
        }
    }
}

void
xfce_desktop_arrange_icons(XfceDesktop *desktop)
{
    g_return_if_fail(XFCE_IS_DESKTOP(desktop));

#ifdef ENABLE_DESKTOP_ICONS
    if (desktop->priv->icon_view_manager != NULL) {
        xfdesktop_icon_view_manager_sort_icons(desktop->priv->icon_view_manager,
                                               GTK_SORT_ASCENDING);
    }
#endif
}

gboolean
xfce_desktop_get_cycle_backdrop(XfceDesktop *desktop)
{
    gint           monitor_num;
    GdkDisplay    *display;
    XfceWorkspace *workspace;

    g_return_val_if_fail(XFCE_IS_DESKTOP(desktop), FALSE);

    display = gdk_screen_get_display(desktop->priv->gscreen);
    monitor_num = xfdesktop_get_current_monitor_num(display);

    workspace = xfce_desktop_get_current_workspace(desktop);
    if (workspace != NULL) {
        XfceBackdrop *backdrop = xfce_workspace_get_backdrop(workspace, monitor_num);
        return xfce_backdrop_get_cycle_backdrop(backdrop);
    } else {
        return FALSE;
    }
}
